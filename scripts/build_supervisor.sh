#!/bin/sh
# Same ARM/uClibc toolchain as runtime. Output never committed.
set -eu
: "${TOOLCHAIN_PREFIX:?Set matching ARM32 uClibc GCC8.3 prefix}"
: "${SUPERVISOR_OUT:?Set absolute output executable path outside source tree}"
case "$SUPERVISOR_OUT" in /*) ;; *) exit 64;; esac
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"${TOOLCHAIN_PREFIX}gcc" -dumpmachine | grep -q '^arm.*uclibc' || exit 65
"${TOOLCHAIN_PREFIX}gcc" -dumpfullversion | grep -q '^8[.]3' || exit 65
SYSROOT=${SYSROOT:-$("${TOOLCHAIN_PREFIX}gcc" -print-sysroot)}
test -d "$SYSROOT"
exec "${TOOLCHAIN_PREFIX}g++" -std=c++11 -O2 -Wall -Wextra --sysroot="$SYSROOT" \
 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -static-libstdc++ -static-libgcc \
 "$repo/port/desktop_supervisor.cpp" -o "$SUPERVISOR_OUT"
