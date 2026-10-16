// SPDX-License-Identifier: MIT
// Linux/POSIX; single-threaded LVGL hosts ONLY (fork+setenv restriction).
#ifndef RV1106_LVGL_ENTRY_ADAPTER_H
#define RV1106_LVGL_ENTRY_ADAPTER_H
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

// Prepare the descriptor bound BEFORE fork. Refuse an unbounded/unknown limit.
static inline long rv1106_fd_bound(void) {
 struct rlimit r;
 if(getrlimit(RLIMIT_NOFILE,&r) || r.rlim_max==RLIM_INFINITY || r.rlim_max>2147483647UL) { errno=EINVAL;return -1; }
 long bound=(long)r.rlim_max;
 // A host may have lowered RLIMIT after opening a higher fd. Include actual
 // descriptors from /proc; single-threaded contract prevents concurrent opens.
 DIR *directory=opendir("/proc/self/fd");if(!directory)return -1;
 struct dirent *item;
 errno=0;
 while((item=readdir(directory))!=0){
  if(item->d_name[0]<'0'||item->d_name[0]>'9')continue;
  char *end=0;long fd=strtol(item->d_name,&end,10);
  if(end&&*end==0&&fd>=bound&&fd<2147483647L)bound=fd+1;
 }
 int scan_error=errno;
 if(closedir(directory)||scan_error)return -1;
 return bound;
}
// No inherited framebuffer, evdev open-file-description/grab, sockets or pipes.
static inline int rv1106_isolate_fds(long bound) {
 int fd=open("/dev/null",O_RDWR);
 if(fd<0)return -1;
 for(int i=0;i<3;i++)if(dup2(fd,i)<0)return -1;
 for(long i=3;i<bound;i++)close((int)i);
 return 0;
}
// release_input MUST relinquish LVGL EVIOCGRAB/close input before child launch.
// acquire_input MUST reopen/regrab after child exit (including failed startup).
// Do not merely SIGSTOP a desktop which still holds EVIOCGRAB.
typedef int (*rv1106_input_hook)(void *);
struct rv1106_entry {
 pid_t child;
 rv1106_input_hook acquire_input;
 void *context;
};
static inline int rv1106_open_godot(struct rv1106_entry *entry,const char *app_dir,
 const char *desktop_exe,rv1106_input_hook release_input,rv1106_input_hook acquire_input,void *context) {
 if(!entry||entry->child>0||!release_input||!acquire_input||!app_dir||app_dir[0]!='/'||!desktop_exe||desktop_exe[0]!='/') { errno=EINVAL;return -1; }
 long bound=rv1106_fd_bound();if(bound<0)return -1;
 char pid_text[32],launcher[4096];
 int n=snprintf(launcher,sizeof(launcher),"%s/launch.sh",app_dir);
 if(n<0||(size_t)n>=sizeof(launcher)){errno=ENAMETOOLONG;return -1;}
 snprintf(pid_text,sizeof(pid_text),"%ld",(long)getpid());
 if(release_input(context)!=0)return -1;
 pid_t child=fork();
 if(child<0){int error=errno;acquire_input(context);errno=error;return -1;}
 if(child==0){
  if(rv1106_isolate_fds(bound)||setsid()<0||setenv("APP_DIR",app_dir,1)||setenv("DESKTOP_EXE",desktop_exe,1)||setenv("DESKTOP_PID",pid_text,1))_exit(126);
  execl("/bin/sh","sh",launcher,(char*)0);_exit(127);
 }
 entry->child=child;entry->acquire_input=acquire_input;entry->context=context;return 0;
}
// Call each LVGL timer tick. Never block the UI.1=completed,0=pending,-1=failure.
// Status is raw waitpid status. Integrator must surface nonzero/failed reacquire.
static inline int rv1106_poll_godot(struct rv1106_entry *entry,int *status) {
 if(!entry||entry->child<=0||!status){errno=EINVAL;return -1;}
 pid_t p=waitpid(entry->child,status,WNOHANG);
 if(p==0 || (p<0&&errno==EINTR))return 0;
 // On ECHILD another reaper interfered; report it, but reacquire input explicitly.
 entry->child=0;
 int recovered=entry->acquire_input(entry->context);
 return p<0||recovered!=0?-1:1;
}
#endif
