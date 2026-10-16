// SPDX-License-Identifier: MIT
#include "desktop_lifecycle.h"
#include <cassert>
#include <cstdio>
#include <string>
struct Mock : DesktopOps {
 std::string inject,fail;
 bool term=false,stopped=false,live=false,dead=false,restored=false,resumed=false,kept=false,removed=false;
 bool pending() override {bool value=term;term=false;return value;}
 bool stop() override {stopped=true;if(inject=="inside-stop")term=true;return fail!="stop";}
 bool snapshot() override {return fail!="short-backup";}
 bool spawn() override {live=true;if(inject=="inside-spawn")term=true;return true;}
 int await_child() override {live=false;dead=true;if(inject=="inside-wait")term=true;return fail=="touch"?70:0;}
 bool terminate_and_reap() override {if(fail=="reap")return false;live=false;dead=true;return true;}
 bool restore() override {assert(dead&&!live);restored=true;if(inject=="inside-restore")term=true;return fail!="write";}
 bool resume() override {assert(!live);resumed=true;if(inject=="inside-resume")term=true;return fail!="continue"&&fail!="identity-gone"&&fail!="same-exe-new-start";}
 void checkpoint(const char *name) override {if(inject==name)term=true;}
 void recovery(const char*) override {kept=true;}
 bool remove_owned_lock() override {if(kept)return false;removed=true;return true;}
};
int main(){
 Mock normal;assert(desktop_session(normal)==0&&normal.restored&&normal.resumed&&normal.removed);
 for(const char *point:{"inside-stop","stopped","snapshot","inside-spawn","spawned","inside-wait","waited","inside-restore","inside-resume"}){
  Mock m;m.inject=point;assert(desktop_session(m)!=0);assert(m.resumed&&!m.live&&m.removed);
 }
 for(const char *failure:{"write","continue","identity-gone","same-exe-new-start","short-backup","reap"}){
  Mock m;m.fail=failure;assert(desktop_session(m)!=0);assert(m.kept&&!m.removed);
 }
 Mock touch;touch.fail="touch";assert(desktop_session(touch)==70&&touch.restored&&touch.resumed&&touch.removed);
 puts("PASS lifecycle transition TERM, short snapshot, restore write/CONT/identity, reap, required-touch-exit mocks");
}
