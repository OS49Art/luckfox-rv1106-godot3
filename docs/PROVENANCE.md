# Provenance / licenses

CPU rasterizer, shared mapping/test, CPU installer and ARM cross patch were
allowlist-curated from an existing independent RV1106 Godot3 adaptation. The
user explicitly authorized open-source release; original port/scripts/demo are
MIT under root LICENSE. Godot-derived API and patch context retain attribution
in LICENSES/Godot-MIT.txt. No parent repository/history was copied.

Public changes: pinned source checks, explicit portable build arguments, removal
of hardcoded optional RGA loader, required-device failure exit and new native pidfd lifecycle supervisor,
descriptor-isolated single-threaded LVGL hooks/reaper, upload-only
staging, original neutral demo,360-window regression checks and documentation.
The public launcher is adapted from the verified handoff contract, not claimed
byte-identical or freshly board-validated. CPU raster/math remain unchanged; explicitly requested device failures now exit70.

Godot3.6.3 upstream: https://github.com/godotengine/godot at
 a3378686f7196db78d7b3cc7947f72a13bc4864c . Clone it separately. Keep upstream
AUTHORS.md, LICENSE.txt and COPYRIGHT.txt; root MIT does not relicense upstream
third-party components. SDK/toolchain come separately from Luckfox and retain
their licenses. Python3/SCons4.8.1 are external host dependencies, not vendored.

No AI character art source, crops, icons, backgrounds, product lore or third-party
font binaries are packaged. Documentation-only UI screenshots are the narrowly
scoped exception described below. Demo rectangles are original source;
labels use Godot built-in font governed by upstream notices. No standalone
font/CC0 artwork licensing claim is needed. No external figure framework bundled.

When distributing binaries, ship all applicable engine/third-party/SDK library
notices and satisfy relevant source/offer obligations. This source repository
contains no runtime binary or complete upstream/SDK tree. Built runtime licensing
is broader than this repository's original-code MIT license.

## Documentation figures

Four original architecture/deployment SVG+PNG diagrams are MIT, Copyright2026
OS49Art, covered by the root LICENSE. Editable SVG and captions are included.
These are original vector illustrations, not private device/application captures,
not model/inference assets. Method guidance credit and evidence boundaries are
in FIGURES.md; no third-party guidance/code or figure-generation services bundled.

## Documentation-only UI screenshots

[Gallery and capture matrix](SCREENSHOTS.md) includes historical RV framebuffer
readback from 2026-10-15 and new Godot3 desktop offscreen targets from 2026-10-16.
These depict the separately maintained character prototype, **not the public
neutral demo** and not new supervisor/board acceptance. The user explicitly
authorized public screenshot display. Only rendered PNGs are included; no
underlying art/font/application sources or packs were copied into this release.
The MIT code license does not relicense depicted artwork or third-party material;
see [screenshot scope](../LICENSES/SCREENSHOTS.md). Documentation display is not
a grant of reusable artwork or a commercial-use guarantee.
