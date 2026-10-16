// SPDX-License-Identifier: MIT
#ifndef RV_DESKTOP_LIFECYCLE_H
#define RV_DESKTOP_LIFECYCLE_H
// Signals MUST remain blocked in the implementation until cleanup completes.
// pending() consumes deferred termination only at consistent state boundaries.
struct DesktopOps {
 virtual ~DesktopOps() {}
 virtual bool pending()=0;
 virtual bool stop()=0;
 virtual bool snapshot()=0;
 virtual bool spawn()=0;
 virtual int await_child()=0;
 virtual bool terminate_and_reap()=0;
 virtual bool restore()=0;
 virtual bool resume()=0;
 virtual void checkpoint(const char*)=0;
 virtual void recovery(const char*)=0;
 virtual bool remove_owned_lock()=0;
};
inline int desktop_session(DesktopOps &o) {
 bool resume_needed=false,snapshot=false,child=false,may_have_drawn=false;
 int result=70;
 if(o.pending())goto cleanup;
 // Resume intent precedes STOP; no asynchronous cleanup runs in this transition.
 resume_needed=true;
 if(!o.stop())goto cleanup;
 o.checkpoint("stopped");if(o.pending())goto cleanup;
 if(!o.snapshot()){o.recovery("snapshot-failed");goto cleanup;}
 snapshot=true;
 o.checkpoint("snapshot");if(o.pending())goto cleanup;
 if(!o.spawn())goto cleanup;
 child=true;may_have_drawn=true;
 o.checkpoint("spawned");if(o.pending())goto cleanup;
 result=o.await_child();
 // await_child guarantees either reaped or still owned; reconcile in one cleanup.
 o.checkpoint("waited");if(o.pending())result=70;
cleanup:
 if(child&&!o.terminate_and_reap()) {o.recovery("child-not-confirmed-dead");return 74;}
 bool okay=true;
 if(may_have_drawn&&(!snapshot||!o.restore()))okay=false;
 // Attempt verified resume even after FB restore failure, but retain recovery data.
 if(resume_needed&&!o.resume())okay=false;
 if(!okay){o.recovery("restore-or-resume-failed");return 74;}
 if(!o.remove_owned_lock()){o.recovery("lock-cleanup-failed");return 74;}
 if(o.pending())result=70; // Replay termination received during noninterruptible cleanup.
 return result;
}
#endif
