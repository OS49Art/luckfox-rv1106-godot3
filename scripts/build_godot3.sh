#!/bin/sh
# ARM32/uClibc CPU Canvas; engine -Os, hot raster functions O3, no fast-math.
set -eu
: "${GODOT_SRC:?Set GODOT_SRC}"
: "${TOOLCHAIN_PREFIX:?Set full ARM32 uClibc GCC8.3 toolchain prefix ending in -}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo/scripts/check_source.py" "$GODOT_SRC"
"${TOOLCHAIN_PREFIX}gcc" -dumpmachine | grep -q '^arm.*uclibc' || exit 65
"${TOOLCHAIN_PREFIX}gcc" -dumpfullversion | grep -q '^8[.]3' || exit 65
SYSROOT=${SYSROOT:-$("${TOOLCHAIN_PREFIX}gcc" -print-sysroot)}
test -d "$SYSROOT" || exit 65
python3 "$repo/scripts/patch_godot3_cross.py" "$GODOT_SRC"
python3 "$repo/scripts/install_godot3_cpu.py" "$GODOT_SRC"
cd "$GODOT_SRC"
exec python3 -m SCons -j "${JOBS:-2}" \
 platform=server tools=no target=release bits=32 arch=arm \
 CC="${TOOLCHAIN_PREFIX}gcc" CXX="${TOOLCHAIN_PREFIX}g++" \
 AR="${TOOLCHAIN_PREFIX}ar" RANLIB="${TOOLCHAIN_PREFIX}ranlib" \
 modules_enabled_by_default=no module_gdscript_enabled=yes module_webp_enabled=yes \
 disable_3d=yes debug_symbols=no optimize=size lto=none \
 CCFLAGS="--sysroot=$SYSROOT -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard" \
 LINKFLAGS="--sysroot=$SYSROOT"
