#!/bin/sh
# Upload only into a new release directory; never activate/overwrite a live app.
set -eu
if test "$#" != 5; then echo 'Usage: sh stage_device.sh SSH_TARGET NEW_REMOTE_ABSOLUTE_DIR RUNTIME PCK SUPERVISOR' >&2; exit 64; fi
target=$1; remote=$2; runtime=$3; pack=$4; supervisor=$5
case "$target" in ''|-*|*[!a-zA-Z0-9@._-]*) exit 64;; esac
case "$remote" in /*) ;; *) exit 64;; esac
case "$remote" in /|*/../*|*/..|*[!a-zA-Z0-9/._-]*) exit 64;; esac
test -f "$runtime" && test -f "$pack" && test -f "$supervisor"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# mkdir without -p refuses an existing destination. SSH host-key verification stays enabled.
ssh "$target" "umask 077; mkdir '$remote'"
scp "$runtime" "$target:$remote/runtime"
scp "$pack" "$target:$remote/panel.pck"
scp "$supervisor" "$target:$remote/desktop-supervisor"
scp "$repo/scripts/launch_desktop.sh" "$target:$remote/launch.sh"
ssh "$target" "chmod 755 '$remote/runtime' '$remote/launch.sh' '$remote/desktop-supervisor'; cd '$remote'; sha256sum runtime panel.pck launch.sh desktop-supervisor"
echo 'STAGED ONLY: compare local hashes, inspect ABI/device/backup, then activate manually.'
