# Architecture / compatibility

SceneTree / GDScript / Controls / resources → VisualServer Canvas commands →
CpuStorage + CpuCanvas → logical32-bit pixel buffer → nearest CPU presenter → fb0.
Goodix evdev → calibrated framebuffer coordinates → shared inverse mapping →
InputDefault / SceneTree / Viewport → normal Godot GUI signals.
LVGL remains the desktop and pauses during exclusive framebuffer/touch ownership.

The rasterizer subclasses upstream dummy storage/Canvas. Installer selects CPU
only when GODOT_CPU_RENDER exists, allows drawing and real resource loading,
pumps input even on static frames and returns actual mouse position. Dummy
SCsub picks up the copied .cpp. ARM patch avoids host x86 -m32/-m64 flags; it
does not bypass compiler-version checks. Upstream remains pinned3.6.3.

Supported controlled subset: rectangles, multirect, nearest textures, source-over
alpha, clipping, affine translation/scale/rotation and solid polygon subset.
Single viewport; render target maximum1024×1024. Flat StyleBox controls/text are
used instead of assuming full default themes work.

Not a complete engine port: no GL/Vulkan,3D, shaders, lighting, particles,
complex mesh/skeletons, general NinePatch/default themes, ViewportTexture or
complex nested viewport support. Single-touch press/release only; dragging,
multi-touch, audio and arbitrary projects are not verified. Inherited dummy
commands may silently omit output: inspect new Canvas features visually.
No vsync/double-buffer guarantee; tearing is possible.

## Production window/input regression

Production width=height=360; omit test_width/test_height. Backend already maps
720×720 touch to360 logical. A desktop test window/viewport attachment at720 can
make Viewport apply a second half-scale. Fix project window settings, not manual
coordinate multiplication or a parallel GUI hit-test system. Desktop fixtures
must stay separate from production PCK. 360→720 is exact2×, touch floor(x/2),floor(y/2).
270×480 height-fit historically gives405×720,left157; this is not responsive layout.

### Synthetic evdev injection pitfall

Linux input may filter unchanged ABS values. Reopening an injector and sending
the same coordinates can yield tracking/BTN/SYN events without X/Y, while a fresh
runtime still has initial coordinates0,0. Historical P6 diagnostics first sent
an adjacent ABS value then the target before SYN to ensure target X/Y arrived.
This is an **injector issue**, not a physical-touch calibration fix or permission
to multiply production coordinates. Inspect actual received events and wait for
complete/flushable logs before diagnosing a stalled event pump. This repository
does not ship a privileged event injector; hardware acceptance remains separate.

## Required device failure and native handoff

cpu_required_device.h is copied with the rasterizer. Explicit FB/touch request
path/open/format/mmap/name/axes/grab failure terminates runtime with exit70;
no requested devices retains optional memory rendering. This public runtime thus
has an intentional acquisition-failure behavior change from historical binaries.
The native desktop_supervisor.cpp and shared desktop_lifecycle.h replace shell
ownership traps; LVGL callback closes inherited descriptors and supplies explicit
input release/reacquire hooks plus a nonblocking waitpid timer reaper. See DEVICE.md.
This new pidfd-based integration is experimental and not yet board-validated.

CPU/full presentation remains default in the launcher. Public curation disables
the private optional RGA dlopen loader, retaining inert presenter members to
minimize unrelated renderer drift. No RGA source/header/library is required,
no address probe is distributed, no RGA execution is supported here.

Existing-solutions preflight uses official pinned Godot server/dummy and Linux
export workflow plus Luckfox SDK, not a new scene framework. Stock ARM desktop
templates require matching libc and graphics stack; they do not by themselves
solve uClibc/no-GLES framebuffer. Live search was unavailable during curation;
this is not a claim that no alternative open-source ports exist.
