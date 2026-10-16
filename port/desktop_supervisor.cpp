// SPDX-License-Identifier: MIT
// Linux >=5.3 pidfd ownership. No numeric-PID signal fallback.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "desktop_lifecycle.h"
#include "lvgl_entry_adapter.h"
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <signal.h>
#include <time.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#ifndef __NR_pidfd_open
#if defined(__arm__) || defined(__x86_64__) || defined(__aarch64__)
#define __NR_pidfd_open 434
#define __NR_pidfd_send_signal 424
#else
#error Requires Linux syscall numbers for this architecture
#endif
#endif
struct Identity { std::string exe,start; };
static bool identity(pid_t pid,Identity &out) {
 std::string base="/proc/"+std::to_string(pid);char buf[4096];
 ssize_t n=readlink((base+"/exe").c_str(),buf,sizeof(buf)-1);if(n<=0)return false;
 out.exe=std::string(buf,(size_t)n);
 std::ifstream f(base+"/stat");std::string line;getline(f,line);
 size_t end=line.rfind(')');if(end==std::string::npos)return false;
 std::istringstream words(line.substr(end+2));std::string word;
 // First token is field3(state), starttime is field22.
 for(int field=3;field<=22;field++){if(!(words>>word))return false;if(field==22)out.start=word;}
 return true;
}
static bool same(const Identity &a,const Identity &b){return a.exe==b.exe&&a.start==b.start;}
static int pin_process(pid_t pid,const std::string &exe,Identity &saved) {
 if(!identity(pid,saved)||saved.exe!=exe)return -1;
 int fd=(int)syscall(__NR_pidfd_open,pid,0);if(fd<0)return -1;
 Identity after;if(!identity(pid,after)||!same(saved,after)){close(fd);return -1;}
 return fd;
}
static bool signal_fd(int fd,int sig){return syscall(__NR_pidfd_send_signal,fd,sig,nullptr,0)==0;}
static bool write_all(int fd,const void *data,size_t bytes) {
 const char *p=(const char*)data;
 while(bytes){ssize_t n=write(fd,p,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;bytes-=(size_t)n;}return true;
}
static bool read_all(int fd,void *data,size_t bytes){
 char *p=(char*)data;while(bytes){ssize_t n=read(fd,p,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;bytes-=(size_t)n;}return true;
}
class LinuxDesktop : public DesktopOps {
public:
 static const size_t bytes=2073600;
 pid_t desktop,child=-1;int desktop_fd=-1,child_fd=-1,fb=-1,dir=-1;
 Identity saved;sigset_t signals;std::string app,lock="/tmp/godot-panel.lock";
 bool reaped=false,keep=false;int child_status=0;long fd_bound;
 std::vector<unsigned char> image;
 LinuxDesktop(pid_t p,const std::string &a):desktop(p),app(a),fd_bound(rv1106_fd_bound()){}
 ~LinuxDesktop(){if(desktop_fd>=0)close(desktop_fd);if(child_fd>=0)close(child_fd);if(fb>=0)close(fb);if(dir>=0)close(dir);}
 bool current(){Identity now;return identity(desktop,now)&&same(saved,now);}
 bool metadata(const char *name,const std::string &text){
  int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
  if(fd<0)return false;bool ok=write_all(fd,text.data(),text.size());if(close(fd))ok=false;return ok;
 }
 bool setup(const std::string &exe){
  desktop_fd=pin_process(desktop,exe,saved);if(desktop_fd<0||fd_bound<0)return false;
  // Refuse already stopped/zombie desktop. Never take responsibility for another pause.
  std::ifstream state("/proc/"+std::to_string(desktop)+"/stat");std::string line;getline(state,line);
  size_t at=line.rfind(')');if(at==std::string::npos||at+2>=line.size()||strchr("TtZX",line[at+2]))return false;
  fb=open("/dev/fb0",O_RDWR|O_CLOEXEC);if(fb<0)return false;
  fb_var_screeninfo v={};fb_fix_screeninfo f={};
  if(ioctl(fb,FBIOGET_VSCREENINFO,&v)||ioctl(fb,FBIOGET_FSCREENINFO,&f)||v.xres!=720||v.yres!=720||v.xoffset||v.yoffset||v.bits_per_pixel!=32||f.line_length!=2880||f.smem_len<bytes||v.red.offset!=16||v.green.offset!=8||v.blue.offset!=0||v.red.length!=8||v.green.length!=8||v.blue.length!=8)return false;
  try {image.resize(bytes);}catch(...){return false;} // Allocate before STOP.
  if(mkdir(lock.c_str(),0700))return false;
  dir=open(lock.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)return false;
  Identity supervisor_identity;if(!identity(getpid(),supervisor_identity)){keep=true;return false;}
  if(!metadata("owner.txt","supervisor="+std::to_string(getpid())+"\nsupervisor_start="+supervisor_identity.start+"\ndesktop="+std::to_string(desktop)+"\nstart="+saved.start+"\nexe="+saved.exe+"\n")){keep=true;return false;}
  return true;
 }
 bool pending() override {
  timespec zero={0,0};bool found=false;int sig;
  while((sig=sigtimedwait(&signals,nullptr,&zero))>0)if(sig!=SIGCHLD)found=true;
  return found;
 }
 bool stop() override {
  if(!current()||!signal_fd(desktop_fd,SIGSTOP))return false;
  for(int i=0;i<100;i++){
   std::ifstream f("/proc/"+std::to_string(desktop)+"/stat");std::string line;getline(f,line);size_t at=line.rfind(')');
   if(!current())return false;
   if(at!=std::string::npos&&at+2<line.size()&&strchr("Tt",line[at+2]))return true;
   timespec t={0,10000000};nanosleep(&t,nullptr);
  }return false;
 }
 bool snapshot() override {
  image.resize(bytes);if(lseek(fb,0,SEEK_SET)<0||!read_all(fb,image.data(),bytes))return false;
  int fd=openat(dir,"before.raw",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
  bool ok=write_all(fd,image.data(),bytes);if(close(fd))ok=false;return ok;
 }
 bool spawn() override {
  int gate[2];if(pipe2(gate,O_CLOEXEC))return false;
  child=fork();if(child<0){close(gate[0]);close(gate[1]);return false;}
  if(child==0){
   close(gate[1]);char go=0;
   if(!read_all(gate[0],&go,1)||go!=1)_exit(126);close(gate[0]);
   if(rv1106_isolate_fds(fd_bound))_exit(126);
   sigset_t empty;sigemptyset(&empty);sigprocmask(SIG_SETMASK,&empty,nullptr);
   int log=open("runtime.log",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
   if(log<0||dup2(log,1)<0||dup2(log,2)<0)_exit(126);if(log>2)close(log);
   execl("./runtime","runtime","--main-pack","panel.pck",(char*)0);_exit(127);
  }
  child_fd=(int)syscall(__NR_pidfd_open,child,0);
  close(gate[0]);
  if(child_fd<0){
   // Child has not executed runtime. EOF cancels startup before it can draw.
   close(gate[1]);while(waitpid(child,&child_status,0)<0&&errno==EINTR){}reaped=true;return false;
  }
  char go=1;bool released=write_all(gate[1],&go,1);close(gate[1]);
  // Once released, report owned child even if metadata fails, so cleanup reaps it.
  if(!released||!metadata("runtime.pid",std::to_string(child)+"\n"))keep=true;
  return true;
 }
 int await_child() override {
  for(;;){
   pid_t p=waitpid(child,&child_status,WNOHANG);
   if(p==child){reaped=true;return WIFEXITED(child_status)?WEXITSTATUS(child_status):128+WTERMSIG(child_status);}
   if(p<0&&errno!=EINTR)return 70;
   if(!current()||pending())return 70;
   timespec t={0,20000000};nanosleep(&t,nullptr);
  }
 }
 bool terminate_and_reap() override {
  if(reaped)return true;
  if(child_fd<0)return false;
  if(!signal_fd(child_fd,SIGTERM)&&errno!=ESRCH)return false;
  for(int i=0;i<100;i++){
   pid_t p=waitpid(child,&child_status,WNOHANG);if(p==child){reaped=true;return true;}
   if(p<0&&errno!=EINTR)return false;timespec t={0,20000000};nanosleep(&t,nullptr);
  }
  if(!signal_fd(child_fd,SIGKILL)&&errno!=ESRCH)return false;
  for(int i=0;i<100;i++){
   pid_t p=waitpid(child,&child_status,WNOHANG);if(p==child){reaped=true;return true;}
   if(p<0&&errno!=EINTR)return false;timespec t={0,20000000};nanosleep(&t,nullptr);
  }return false;
 }
 bool restore() override {
  int snap=openat(dir,"before.raw",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat st={};
  bool ok=snap>=0&&fstat(snap,&st)==0&&st.st_size==(off_t)bytes;
  if(snap>=0)close(snap);
  return ok&&image.size()==bytes&&lseek(fb,0,SEEK_SET)>=0&&write_all(fb,image.data(),bytes);
 }
 bool resume() override {
  if(!current()||!signal_fd(desktop_fd,SIGCONT))return false;
  for(int i=0;i<100;i++){
   std::ifstream f("/proc/"+std::to_string(desktop)+"/stat");std::string line;getline(f,line);size_t at=line.rfind(')');
   if(!current())return false;
   if(at!=std::string::npos&&at+2<line.size()&&!strchr("TtZX",line[at+2]))return true;
   timespec t={0,10000000};nanosleep(&t,nullptr);
  }return false;
 }
 void checkpoint(const char*) override {} // No environment-triggered test hooks in production.
 void recovery(const char *why) override {keep=true;metadata("recovery.txt",std::string(why)+"\n");fprintf(stderr,"RECOVERY_REQUIRED %s %s\n",lock.c_str(),why);}
 bool remove_owned_lock() override {
  if(keep)return false;
  // Verify pathname still names our opened directory before scoped removal.
  struct stat a={},b={};if(fstat(dir,&a)||lstat(lock.c_str(),&b)||a.st_ino!=b.st_ino||a.st_dev!=b.st_dev)return false;
  const char *names[]={"runtime.pid","owner.txt","before.raw"};
  for(const char *n:names)if(unlinkat(dir,n,0)&&errno!=ENOENT)return false;
  return rmdir(lock.c_str())==0;
 }
};
#ifndef RV_SUPERVISOR_NO_MAIN
int main(int argc,char **argv){
 if(argc!=4||argv[1][0]!='/'||argv[3][0]!='/')return 64;
 char *end=nullptr;long p=strtol(argv[2],&end,10);if(!end||*end||p<=1||p>2147483647)return 64;
 // Reset inherited SIGCHLD disposition; no SA_NOCLDWAIT/automatic reaping.
 struct sigaction action={};action.sa_handler=SIG_DFL;sigemptyset(&action.sa_mask);
 for(int s:{SIGCHLD,SIGTERM,SIGINT,SIGHUP})if(sigaction(s,&action,nullptr))return 70;
 action.sa_handler=SIG_IGN;if(sigaction(SIGPIPE,&action,nullptr))return 70;
 LinuxDesktop o((pid_t)p,argv[1]);sigemptyset(&o.signals);
 for(int s:{SIGTERM,SIGINT,SIGHUP,SIGCHLD})sigaddset(&o.signals,s);
 if(sigprocmask(SIG_BLOCK,&o.signals,nullptr)||chdir(argv[1]))return 70;
 if(!o.setup(argv[3])){fprintf(stderr,"SUPERVISOR_PREFLIGHT_FAILED\n");return 70;}
 int result=desktop_session(o);
 if(result==74||o.keep)fprintf(stderr,"RECOVERY_REQUIRED\n");
 else fprintf(stderr,"GODOT_DESKTOP_HANDOFF_COMPLETE runtime_status=%d\n",result);
 return result;
}
#endif
