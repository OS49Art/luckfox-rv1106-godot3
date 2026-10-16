# Evidence boundaries

Historical source evidence was reviewed on2026-10-16. These are sanitized prior
observations from the maintained private360 production application, **not this
new neutral demo's board acceptance**, and no raw private logs/art are published.

- P6 production fix removed only desktop test_width/test_height720; original
  runtime/launcher unchanged.8 slots/3tabs/input boundaries checked through
  normal GUI path using88 programmatically injected evdev backend press logs,
  expected actions0 differences. This is **not88 human finger checks**.
- Same-state framebuffer vs independent desktop-nearest oracle:720×720,
  BGRx32,stride2880, exact2× blocks; core and attribute overlay518400 pixels,
 0 differences. Framebuffer precedes physical optics: not LCD sharpness proof.
- LVGL open/Back restoration and paired old-runtime/PCK/launcher rollback then
  new-pack reactivation were verified historically. Physical open/return was
  separately confirmed by user; do not merge that with synthetic touch evidence.
- No optical latency, panel FPS, vsync, zero-tearing or current360 ten-minute
  stability acceptance. Prior different demo long-run evidence is not transferable.

## Final P6 performance reference (not a fair speed comparison)

These are **historical final-P6 production-pack observations**, not measurements
of this public neutral demo, nor the earlier P5 diagnostic variant. Three
interleaved rounds used the same original runtime, hot-function O3 CPU/full,
RGA disabled,180 deterministic UI changes and181 render submissions per run.

| Historical application | Round mean render times (ms) | Median of round means (ms) | Median of round mean presentation times (ms) |
|---|---|---:|---:|
| Old270×480 panel |22.930 /22.060 /22.954|22.930|2.850|
| Final360×360 panel |8.050 /8.246 /8.422|8.246|3.374|

**Not strict same UI/workload:** layout, assets, fonts and update APIs differ
(old set_attribute versus new set_enhancement); matching update counts does not
make the work equivalent. No speedup ratio/percentage or pure-layout claim is
justified. These are cumulative render/presentation means, **not per-frame p95,
displayed FPS, input latency or touch-to-photon latency**. Do not invert them to
claim panel refresh rate. Presentation is part of render timing, not an extra
cost to add to it.

The new presentation mean increases even while total render mean decreases:
the square pack writes the full720×720 output (2073600 bytes/submit), whereas
the old height-fit logical content occupies405×720 with cached margins. The
output work is therefore different, not evidence of a general presenter regression.
Memory samples are omitted from this compact table because collection windows
and sample counts differ; no equal-window/instantaneous-peak comparison is implied.
No current360 dedicated ten-minute stability or physical optics acceptance is
inferred from this short benchmark.

## This public checkout

Host/static/demo/patch tests are recorded in VALIDATION.md after execution.
Cross-compilation and hardware tests must be run by a builder with matching
SDK/board. New portable adapter and neutral demo require fresh device validation;
no inherited PASS is claimed. CI covers offline checks only.
