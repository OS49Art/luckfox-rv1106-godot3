# Documentation figure captions

## Runtime architecture — public integration design
Godot 3.6.3 with a restricted CPU Canvas backend is cross-built for ARM32/uClibc (1), while GDScript and assets are exported as a matching Godot 3 PCK (2). The LVGL entry is wired to a new external native Linux pidfd supervisor (3); it is a generic lifecycle helper, not another UI application and not the historical byte-identical launcher. Real desktop hooks must drain/release the initiating touch, ungrab and close/disable desktop input before forking. **SIGSTOP does not release EVIOCGRAB.** Only a reviewed single-threaded LVGL host is supported by this callback adapter; multithreaded hosts need separately reviewed integration. The supervisor requires pidfd support, has no numeric-PID signal fallback, and enforces singleton ownership.

Supported Canvas instructions render at 360 x 360 and are presented to the 720 x 720 framebuffer at exact nearest-neighbor 2x scale (4). evdev coordinates follow raw ABS → calibrated framebuffer → logical mapping, then the normal Godot GUI path. Cleanup confirms runtime termination/reaping before framebuffer restoration and desktop resume. After resume, the nonblocking LVGL timer reaps its helper child and explicitly reacquires input. The dashed return summarizes this coordinated lifecycle, not a single unconditional operation. Solid arrows describe build, data or control relationships; crossings without junctions are not connected.

**Public integration design; see validation limits below.** This is a limited CPU 2D port, not full GPU/3D/shader, Godot 4 or C# support; no RGA execution is supported by the public integration.

## Safe deployment and rollback — public integration design
Verify executable identity, pidfd/platform support, framebuffer/touch profiles and ownership, and implement reviewed input release/reacquire hooks before changes (1). Preserve an untouched timestamped, hash-checked matched **four-file** release: runtime, PCK, launcher and native supervisor, including an independent backup (2). Stage all four trusted files into a new inactive release directory, without activation or in-place replacement (3); verify local/remote hashes plus runtime and supervisor ABI/loader compatibility (4).

Use the integrated LVGL entry/helper only with working input hooks (5), then test evdev-to-GUI behavior, boundaries and same-state exact-2x framebuffer output (6). Check confirmed runtime termination/reaping, framebuffer restoration, desktop resume, helper reaping, input reacquisition and appropriate lock cleanup (7). Successful process creation is not UI acceptance. A required framebuffer or touch acquisition failure now exits the public runtime with status 70; this behavior intentionally differs from historical binaries.

Preflight failure means **STOP before changing files**. The red recovery branch represents **any** post-change release-check failure, not only step 6. Quiesce only a verified Godot identity, confirm ownership and child termination, then switch to an untouched known-good matched four-file release or restore its full backup, verify every hash, and test entry/Back/release again (8). Never mix unrelated runtime, PCK, launcher or supervisor versions. A historical known-good release without this supervisor is a separate complete historical set, not permission to mix it with the current four-file design.

Restore, identity, resume or unconfirmed-child failures are not automatic recovery success: the native helper returns recovery-required status 74 and preserves owned lock, framebuffer snapshot and owner/recovery metadata. Preserve the release backup too; stop and investigate using an independent recovery console. Do not delete the only dump, blindly clear a lock or resume LVGL over a live runtime. Snapshot failure also preserves recovery state and prevents runtime startup. Input reacquisition errors must be surfaced; never assume touch is usable merely because the desktop resumed. SIGKILL, power loss and kernel failure cannot be made recoverable by this helper. This diagram is a decision guide, not an automatic installer.

## Validation limits and comparison boundary
The CPU backend core and the historical 360-to-720 presentation/input path have historical board evidence. **The new public pidfd supervisor, LVGL adapter/hooks and modified required-device failure behavior are not board-validated; a fresh ARM build and public integration hardware acceptance remain NOT_RUN in the reviewed source.** Prepared Linux native CI must not be described as executed until actual results are collected. Host/model/source checks do not establish Linux driver or board correctness. These figures assert the proposed public integration structure, not successful execution of that adapter on hardware.

Historical current-package input acceptance used programmatic evdev injection through the real backend and normal GUI; it is not a new human-finger or optical latency test. Framebuffer readback precedes physical LCD scanning. The 360 package has no dedicated ten-minute stability result. Historical and current workloads differ: any separate performance reference table must retain that caveat, avoid controlled-speedup claims, and not convert render-time reciprocals into displayed FPS. These structural diagrams contain no performance data.

## Artwork and editing
Original artwork: Copyright (c) 2026 OS49Art, MIT License; covered by the root LICENSE. No upstream logos, screenshots, artwork, embedded fonts or image-generation services were used. Fonts: Arial, Helvetica, sans-serif system fallback; live SVG text remains editable. Stable n-* and e-* IDs map components and relationships; edit the primary SVG sources directly. Only the SVG sources, matching PNG exports and this caption document are distributed; no generator, private brief or QA artifacts are bundled. No Nature journal compliance, endorsement or certification is claimed.


## Figure files

- Architecture: [editable SVG](images/architecture.svg), [PNG](images/architecture.png).
- Deployment/recovery: [editable SVG](images/deployment.svg), [PNG](images/deployment.png).

Method guidance consulted: [Nature Figure in nature-skills](https://github.com/Yuan1z0825/nature-skills), Apache-2.0 guidance. No third-party guidance/code is bundled, and no Nature journal compliance, endorsement or certification is claimed.
