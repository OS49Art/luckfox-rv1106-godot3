// SPDX-License-Identifier: MIT
#define RV_SUPERVISOR_NO_MAIN
#include "desktop_supervisor.cpp"
#include "godot3/cpu_required_device.h"
#include <cassert>
static int released=0,acquired=0;
static int release_hook(void*){released++;return 0;}
static int acquire_hook(void*){acquired++;return 0;}
struct SignalMock : DesktopOps {
 LinuxDesktop collector;
 std::string inject;bool live=false,continued=false,restored=false;
 explicit SignalMock(const char *point):collector(getpid(),"/unused"),inject(point){sigemptyset(&collector.signals);sigaddset(&collector.signals,SIGTERM);}
 bool pending() override{return collector.pending();}
 bool stop() override{if(inject=="inside-stop")raise(SIGTERM);return true;}
 bool snapshot() override{return true;}
 bool spawn() override{live=true;if(inject=="inside-spawn")raise(SIGTERM);return true;}
 int await_child() override{live=false;if(inject=="inside-wait")raise(SIGTERM);return 0;}
 bool terminate_and_reap() override{live=false;return true;}
 bool restore() override{assert(!live);restored=true;if(inject=="inside-restore")raise(SIGTERM);return true;}
 bool resume() override{assert(!live);continued=true;if(inject=="inside-resume")raise(SIGTERM);return true;}
 void checkpoint(const char *point) override{if(inject==point)raise(SIGTERM);}
 void recovery(const char*) override{assert(false);}
 bool remove_owned_lock() override{return true;}
};
int main(int argc,char **argv){
 if(argc==3&&std::string(argv[1])=="--fdcheck"){
  int sentinel=atoi(argv[2]);if(fcntl(sentinel,F_GETFD)!=-1||errno!=EBADF)return 10;
  for(int fd=0;fd<3;fd++){char p[64],target[128];snprintf(p,sizeof(p),"/proc/self/fd/%d",fd);ssize_t n=readlink(p,target,sizeof(target)-1);if(n<0)return 11;target[n]=0;if(strcmp(target,"/dev/null"))return 12;}return 0;
 }
 char directory[]="/tmp/rv-native-XXXXXX";assert(mkdtemp(directory));
 std::string base=directory;
 Identity me;assert(identity(getpid(),me));Identity changed=me;changed.start+="1";assert(!same(me,changed));
 Identity held;int pin=pin_process(getpid(),me.exe,held);assert(pin>=0&&same(me,held));assert(signal_fd(pin,0));close(pin);
 // A pidfd to an exited process cannot signal a later process with recycled numeric PID.
 pid_t exited=fork();assert(exited>=0);if(exited==0)_exit(0);
 pin=(int)syscall(__NR_pidfd_open,exited,0);assert(pin>=0);int status;assert(waitpid(exited,&status,0)==exited);
 assert(!signal_fd(pin,0)&&errno==ESRCH);close(pin);
 // FD without CLOEXEC must not cross header's fork/exec. Reaper reacquires once.
 int fd=open("/dev/null",O_RDONLY);assert(fd>=0&&dup2(fd,200)==200);close(fd);assert(fcntl(200,F_GETFD)==0);
 std::ofstream launch(base+"/launch.sh");launch<<"#!/bin/sh\nexec '"<<me.exe<<"' --fdcheck 200\n";launch.close();
 rv1106_entry entry={0,nullptr,nullptr};assert(rv1106_open_godot(&entry,base.c_str(),me.exe.c_str(),release_hook,acquire_hook,nullptr)==0);
 assert(released==1&&entry.child>0);int done=0;
 for(int i=0;i<500&&done==0;i++){done=rv1106_poll_godot(&entry,&status);usleep(10000);}
 assert(done==1&&WIFEXITED(status)&&WEXITSTATUS(status)==0&&acquired==1&&entry.child==0);close(200);
 // Actual blocked TERM queued inside each transition: no asynchronous handler cleanup.
 sigset_t terms,previous;sigemptyset(&terms);sigaddset(&terms,SIGTERM);assert(sigprocmask(SIG_BLOCK,&terms,&previous)==0);
 for(const char *point:{"inside-stop","stopped","snapshot","inside-spawn","spawned","inside-wait","waited","inside-restore","inside-resume"}){
  SignalMock m(point);assert(desktop_session(m)!=0&&m.continued&&!m.live);
 }
 assert(sigprocmask(SIG_SETMASK,&previous,nullptr)==0);
 // Real supervisor gated spawn/wait/reap without display devices.
 std::ofstream runtime(base+"/runtime");runtime<<"#!/bin/sh\nexit 70\n";runtime.close();assert(chmod((base+"/runtime").c_str(),0700)==0);
 char cwd[4096];assert(getcwd(cwd,sizeof(cwd)));assert(chdir(base.c_str())==0);
 {
  LinuxDesktop childOwner(getpid(),base);childOwner.saved=me;sigemptyset(&childOwner.signals);
  childOwner.dir=open(base.c_str(),O_RDONLY|O_DIRECTORY);assert(childOwner.dir>=0);
  assert(childOwner.spawn());assert(childOwner.child_fd>=0);
  assert(childOwner.await_child()==70&&childOwner.reaped);assert(childOwner.terminate_and_reap());
 }
 assert(chdir(cwd)==0);
 // Real restore routines, ordinary file target; no device access.
 LinuxDesktop restore(getpid(),base);restore.lock=base;restore.dir=open(base.c_str(),O_RDONLY|O_DIRECTORY);assert(restore.dir>=0);
 restore.saved=me;restore.desktop_fd=pin_process(getpid(),me.exe,held);assert(restore.desktop_fd>=0);
 restore.fb=open((base+"/fb").c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);assert(restore.fb>=0);
 restore.image.assign(LinuxDesktop::bytes,42);
 int snap=open((base+"/before.raw").c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);assert(snap>=0);assert(write_all(snap,restore.image.data(),LinuxDesktop::bytes));close(snap);
 assert(restore.restore());
 assert(truncate((base+"/before.raw").c_str(),3)==0);assert(!restore.restore());
 restore.recovery("short-snapshot-test");assert(!restore.remove_owned_lock()&&access((base+"/before.raw").c_str(),F_OK)==0);
 snap=open((base+"/before.raw").c_str(),O_WRONLY|O_TRUNC);assert(snap>=0&&write_all(snap,restore.image.data(),LinuxDesktop::bytes));close(snap);
 close(restore.fb);restore.fb=open("/dev/full",O_WRONLY);assert(restore.fb>=0&&!restore.restore());
 restore.saved.start+="1";assert(!restore.resume());restore.saved=me;
 close(restore.desktop_fd);restore.desktop_fd=-1;assert(!restore.resume());
 // Exact runtime failure helper used by requested touch/fb acquisition errors.
 for(const char *failure:{"touch open","touch name","touch axes","touch grab","fb open","fb mmap"}){
  pid_t child=fork();assert(child>=0);if(child==0){cpu_require_device(false,failure);_exit(99);}assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==70);
 }
 cpu_require_device(true,"optional no-device memory render");
 for(const char *name:{"launch.sh","before.raw","recovery.txt","fb","runtime","runtime.log","runtime.pid"})unlink((base+"/"+name).c_str());
 assert(rmdir(base.c_str())==0);
 puts("PASS native Linux fd isolation/async reaping/pidfd identity/queued TERM/restore failures/required-device failure helper");
}
