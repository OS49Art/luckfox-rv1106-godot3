# Experimental desktop integration and recovery

This public adapter is **new source, not board-validated**. Review and run host
CI first, then use a recovery console for first hardware tests. No firmware,
DT/timing/partition/swap/network/boot-service changes are authorized by this guide.
Requires Linux pidfd_open/pidfd_send_signal (normally5.3+, firmware must enable
these), /proc identity and framebuffer profile720×720 BGRx32/stride2880/zero offset.
Unsupported pidfd/profile fails before stopping LVGL; no numeric-PID signal fallback.
Goodix event0 single-touch axes must be valid; historically0..719/rotation0.

Kernel minimum for both required pidfd operations is Linux5.3 (or a confirmed
vendor backport); header availability alone does not prove kernel support.
Matching ARM/uClibc GCC8.3 compilation remains a separate NOT_RUN gate here.
An older SDK may lack SYS_pidfd/__NR_pidfd macros. The supervisor uses narrowly
scoped Linux syscall-number compatibility definitions for ARM32/x86-64/AArch64;
these are syscall identifiers, **not physical addresses or memory probes**.
Do not infer ARM ABI/link correctness from Ubuntu x86-64 compile. Build with the
actual matching SDK headers/sysroot and verify target syscall support before
adopting this new supervisor. Unknown architecture is a compile-time refusal.

## Inspect and back up first

Verify board model/SoC/DDR (Ultra/UltraW G3/256MB, not B/BW128MB), uname/libc,
matching uClibc ELF loader/shared libraries, fb format/stride and evdev name/ABS
ranges, current fb/input owners and storage/RAM. ADB product name and Linux
MemTotal alone are not board/DDR proof. fb backup uses about2MiB plus rendering
memory. Never inspect or publish unrelated application/model data.

With Godot inactive, back up the known-good runtime/PCK/launcher together into
a new timestamped directory and verify SHA256, also off-device. Never overwrite
the only backup. A new supervisor deployment adds desktop-supervisor: record
that executable too. Keep all device identities, addresses, paths, logs and
recovery dumps private. No automatic activation or in-place replacement supplied.

## Build and stage

Build runtime via README, then matching native supervisor:

~~~sh
export SUPERVISOR_OUT=/absolute/output/desktop-supervisor
sh scripts/build_supervisor.sh
# Inspect ARM ABI/loader/dependencies for BOTH runtime and supervisor.
sh scripts/stage_device.sh "$BOARD_SSH_TARGET" "$NEW_REMOTE_RELEASE_DIR" "$RUNTIME" "$PCK" "$SUPERVISOR_OUT"
sha256sum "$RUNTIME" "$PCK" "$SUPERVISOR_OUT" scripts/launch_desktop.sh
~~~

Staging uses a new non-existing directory; compare local/remote hashes. It never
activates it. Partial uploads remain inactive. SSH host-key checks stay enabled.
Release needs runtime, panel.pck, desktop-supervisor and launch.sh, all trusted.
Do not pass UI input as paths/executable arguments. No passwords in source/argv.

## Required LVGL lifecycle integration

**SIGSTOP does not release EVIOCGRAB.** Integrate real desktop input release and
reacquire hooks. release_input must drain/release the launching touch, ungrab and
close/disable LVGL input before forking; acquire_input must reopen/regrab and
re-enable it after child exit, including failed startup. The helper closes all
inherited descriptors>=3 and redirects0/1/2 to /dev/null, so child does not retain
LVGL framebuffer, evdev, sockets or pipes. Runtime output uses private runtime.log.
Do not integrate if you cannot implement these hooks. Check competing owners.

port/lvgl_entry_adapter.h is **single-threaded-host only** (fork+setenv); do not
call it inside a multithreaded desktop. Such a host needs a separately reviewed
posix_spawn/prepared-environment/file-actions integration, not this helper.
Example shape, implement the hooks using your actual LVGL/input driver API:

~~~cpp
static rv1106_entry entry = {0, nullptr, nullptr};
// On LV_EVENT_CLICKED (after release_input drains the finger release):
if (rv1106_open_godot(&entry, app_dir, verified_desktop_exe,
                     release_input, acquire_input, driver_context) != 0) {
    show_launch_error();
}
// On an existing LVGL timer tick, NOT a blocking wait in the click callback:
if (entry.child > 0) {
    int status = 0;
    int done = rv1106_poll_godot(&entry, &status); // WNOHANG; reacquires on completion
    if (done < 0 || (done == 1 && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)))
        show_recovery_error();
}
~~~

Do not install a competing SIGCHLD reaper for this child. Surface reacquire/fork
errors and verify input is usable again. Host gets resumed by supervisor first;
timer then reaps child and reacquires input. Never synchronously wait inside UI.
For a console experiment, release any LVGL grab using its reviewed API first,
then set APP_DIR/DESKTOP_EXE/DESKTOP_PID explicitly and run launch.sh. Restore
its input API afterward. Merely supplying a PID is insufficient integration.

## Supervisor state and failure behavior

The shell only validates arguments/sets CPU-full environment and execs the native
supervisor; it has no ownership traps or background-PID races. Supervisor blocks
TERM/INT/HUP/CHLD, captures desktop exe plus /proc starttime around pidfd_open,
then mkdir-singleton /tmp/godot-panel.lock and saves owner metadata. It refuses
an already stopped desktop. All signals to desktop/runtime use stable pidfds.

Resume intent is recorded before STOP. STOP is confirmed before framebuffer
snapshot. Runtime child waits behind a pipe gate until its pidfd is captured;
termination stays pending until transition state is consistent. waitpid is owned
by supervisor, not a shell; after reap it never signals that completed PID.
Queued termination is consumed at checkpoints and normal wait polling. Cleanup
confirms runtime termination/reaping (TERM then bounded KILL) before touching
framebuffer/resuming LVGL. If child death is unconfirmed, it does neither.

Requested runtime FB or touch path/open/format/mmap/name/axes/grab failures exit70
promptly, so supervisor performs restore. Without requested devices, memory-only
render remains optional. Successful process launch is not full UI acceptance.

Snapshot uses exact-size reads/writes. Restore/identity/CONT failures return74,
print RECOVERY_REQUIRED and **preserve owned lock/snapshot/metadata**. Snapshot
failure also retains recovery state and does not start runtime. Only confirmed
restoration/resume permits normal scoped cleanup. Pending signals never interrupt
this cleanup. SIGKILL, power loss, kernel failure and filesystem exhaustion cannot
be made recoverable by a process; keep independent backups/recovery console.

On a recovery lock: inspect owner metadata + live process starttime/exe and actual
hardware owners before action. Never trust an old PID or remove the only dump.
If runtime is still alive, stop only its verified identity; do not resume desktop
over it. Use a reviewed manual procedure to restore fb and resume/reacquire LVGL,
copy recovery material off-device, then remove only inspected owned files. No
automatic stale lock removal, arbitrary addresses or broad cleanup is provided.

## Hardware acceptance / rollback gate

- Actual card entry, duplicate singleton rejection,360 logical→720 exact2× display.
- All8slots/3tabs/details/action and header Back even over popup; test boundaries.
- Separate actual finger checks from evdev injection and API state tests.
- Check runtime exit, framebuffer/touch release, supervisor status, desktop
  resumed, input hook reacquired, lock cleanup; repeat entry/Back/normal TERM.
- Capture same-state nearest oracle if needed, also inspect physical LCD.
  Test RSS/CMA/OOM and dedicated duration; no FPS/optical inference from capture.
- To roll back: with verified Godot stopped and reviewed desktop ownership,
  switch entry to untouched known-good release (or restore the whole paired
  backup triple, plus supervisor if that release uses one), hash-check and test
  old entry/Back/release. Never mix arbitrary runtime/PCK versions.

Native host CI covers modeled failure paths, not your driver/API/board. Fresh
ARM build and new public adapter hardware acceptance remain NOT_RUN here.
