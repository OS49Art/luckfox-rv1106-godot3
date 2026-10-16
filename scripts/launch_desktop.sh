#!/bin/sh
# The native supervisor owns all signal/child/restore transitions, not shell traps.
set -eu
: "${APP_DIR:?Absolute release directory}"
: "${DESKTOP_EXE:?Freshly verified real LVGL executable}"
: "${DESKTOP_PID:?Freshly verified LVGL PID}"
case "$APP_DIR:$DESKTOP_EXE" in /*:/*) ;; *) exit 64;; esac
case "$DESKTOP_PID" in ''|*[!0-9]*) exit 64;; esac
cd "$APP_DIR"
test -x ./desktop-supervisor && test -x ./runtime && test -f ./panel.pck
export GODOT_CPU_RENDER=1 GODOT_CPU_FB=/dev/fb0 GODOT_CPU_TOUCH=/dev/input/event0
export GODOT_CPU_SCALE=height GODOT_PANEL_DESKTOP=1 GODOT_CPU_PRESENT_FULL=1
export XDG_DATA_HOME=/tmp/godot-panel-userdata
unset GODOT_CPU_RGA GODOT_CPU_VERIFY_FB GODOT_CPU_TEST_CLICK GODOT_CPU_DUMP GODOT_CPU_DUMP_FIRST GODOT_CPU_BACKGROUND_SOLID
exec ./desktop-supervisor "$APP_DIR" "$DESKTOP_PID" "$DESKTOP_EXE"
