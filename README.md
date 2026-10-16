# Godot 3 CPU Canvas for Luckfox RV1106G3

Run a small Godot **3.6.3 GDScript** UI on GPU-less embedded Linux framebuffer, with safe entry/return from an existing LVGL desktop. This is an independent, limited **CPU Canvas backend**, not a GPU driver or full Godot port. **Godot 4 and C# are not supported.**

[中文](README.zh-CN.md) · [Architecture / limits](docs/ARCHITECTURE.md) · [Device / rollback](docs/DEVICE.md) · [Evidence](docs/EVIDENCE.md) · [Provenance](docs/PROVENANCE.md)

## Included

- Complete CPU rasterizer source and headers, replayable server-platform installer.
- Pinned Godot upstream and ARM32 hard-float uClibc build script.
- Shared framebuffer/touch mapping and host tests.
- Original neutral 360×360 demo: eight slots, three tabs, details, action, Back. Geometry and built-in text only; no private application artwork.
- Explicit singleton LVGL handoff. Optimized CPU/full baseline, **no RGA**.

**Experimental source release:** new supervisor/neutral demo need fresh target validation.
Native Linux CI is a separate gate, not inherited hardware acceptance.

## Integration diagrams

![Independent Godot3 CPU Canvas architecture](docs/images/architecture.png)

![Experimental native-supervisor deployment and matched four-file recovery](docs/images/deployment.png)

[Full captions, editable SVGs and figure provenance](docs/FIGURES.md).
These describe the **new experimental public integration**, not a board-validated
supervisor/hooks implementation; historical P6 entry results do not validate it.

## Quick start

Linux cross-build host: Git, Python3, SCons **4.8.1**, and matching Luckfox **ARM32 uClibc GCC8.3** toolchain/sysroot. Obtain SDK separately from [Luckfox](https://github.com/LuckfoxTECH/luckfox-pico). Do not substitute a glibc compiler. No firmware changes needed.

~~~sh
git clone --depth 1 --branch 3.6.3-stable https://github.com/godotengine/godot.git ../godot-3.6.3
export GODOT_SRC="$(cd ../godot-3.6.3 && pwd)"
git -C "$GODOT_SRC" rev-parse HEAD
# Must be a3378686f7196db78d7b3cc7947f72a13bc4864c
export TOOLCHAIN_PREFIX=/absolute/sdk/toolchain/bin/arm-rockchip830-linux-uclibcgnueabihf-
# SYSROOT optionally overrides compiler -print-sysroot; JOBS defaults to 2.
sh scripts/build_godot3.sh
file "$GODOT_SRC"/bin/godot_server*
"${TOOLCHAIN_PREFIX}readelf" -l "$GODOT_SRC"/bin/godot_server* | grep interpreter
~~~

Use a **fresh explicit upstream checkout**: installer modifies it, not SDK/board. Pinned HEAD and patch anchors are checked. Engine -Os plus hot raster functions O3, no fast-math. Usual result: bin/godot_server.x11.opt.arm; check actual SCons output.

Open demo/project.godot in the **standard Godot3.6.3 editor** and import. Install matching Linux/X11 export templates for CLI pack export:

~~~sh
mkdir -p out
godot3 --no-window --path demo --export-pack "RV1106 CPU PCK" ../out/panel.pck
godot3 --no-window --path demo -- --smoke
# On target ARM board, without framebuffer ownership (memory renderer only):
GODOT_CPU_RENDER=1 GODOT_CPU_PRESENT_FULL=1 ./runtime --main-pack panel.pck -- --smoke
~~~

For real LCD/touch **never start over running LVGL**. Build desktop-supervisor
with scripts/build_supervisor.sh and integrate explicit LVGL input release/reacquire
(the parent SIGSTOP does not release an evdev grab). Linux pidfd support is required. Follow [DEVICE.md](docs/DEVICE.md): verify ABI, display/input ownership, paired backups, then launch exclusively. Production project is360×360 without desktop test_width/test_height; framebuffer scaling is independent exact2× on720×720.

## Historical performance reference

A small [final-P6 reference table](docs/EVIDENCE.md#final-p6-performance-reference-not-a-fair-speed-comparison)
records old270 and final360 production-pack round mean render/presentation times.
Different UI/assets/fonts/update APIs mean **not a strict same-workload comparison**;
these are not this neutral demo's timings, speedup claims, FPS, p95 or latency.

## Host tests / CI

~~~sh
python3 tests/test_repository.py
c++ -std=c++11 -Iport/godot3 port/godot3/test_viewport_mapping.cpp -o /tmp/mapping-test
/tmp/mapping-test
for f in scripts/*.sh; do sh -n "$f"; done
# Optional standard Godot3.6.3 editor behavior test:
godot3 --no-window --path demo -s ../tests/demo_smoke.gd
~~~

CI also compiles the real Linux supervisor and runs descriptor-sentinel/async-reaper,
pidfd, queued-TERM, restoration-failure and required-device-failure tests.
Publication uses an explicit source allowlist and scans index/history for excluded
application/model data, not just .gitignore.
CI checks mapping, repository constraints and installers against pinned upstream. It does **not** fake hardware tests. Read [evidence boundaries](docs/EVIDENCE.md). No engine binaries, PCKs, SDKs or proprietary assets are shipped.

## License

Original port/scripts/demo MIT; upstream Godot notices retained in [LICENSES/Godot-MIT.txt](LICENSES/Godot-MIT.txt). Preserve upstream COPYRIGHT.txt and third-party notices when distributing built runtimes. Not an official Godot/Luckfox port.

## Actual UI screenshots

![Historical RV framebuffer character prototype](docs/images/screenshots/rv-fixed-core.png)

Historical RV1106 framebuffer readback (2026-10-15), separate character prototype;
not an LCD photo, not this neutral demo or new supervisor acceptance.

[Gallery, six desktop resolution targets and limitations](docs/SCREENSHOTS.md).
Documentation display only; [MIT does not relicense depicted artwork](LICENSES/SCREENSHOTS.md).
