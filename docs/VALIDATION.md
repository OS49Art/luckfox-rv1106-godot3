# Public-curation validation — 2026-10-16

Local results and an observed GitHub-hosted Ubuntu Linux run are recorded below.

| Check | Result | Scope |
|---|---|---|
| Python3.8 tests/test_repository.py | PASS8 tests | window, syntax, CPU policy, allowlist/index/history/all-blob/privacy scans, image audits/negative controls and relative-link checks |
| tests/test_install.py disposable pinned upstream | PASS | both installers twice, idempotent byte hashes; dummy SCsub collects .cpp |
| Host mapping test, MSVC19.16 x64 | PASS |518400 square2× +291600 height-fit mappings, bars, full coverage, integer regression |
| sh -n scripts/*.sh (Git Bash) | PASS | syntax only |
| missing-argument launch/build/stage | PASS | fail closed without device mutation |
| Godot3.6.3 source demo --smoke | PASS | DEMO_READY / DEMO_SMOKE_PASS / exit0 |
| Godot3.6.3 tests/demo_smoke.gd | PASS | slot7,tab2,overlay,action,return; DEMO_BEHAVIOR_PASS / exit0 |
| Original diagrams SVG/PNG | PASS | exact four-file image allowlist, static SVG/no active/external content; PNG CRC/chunk/no metadata/trailing/size validation; visual review, no product/device screenshots |
| Export RV1106 CPU PCK | PASS |4 pack entries, no private assets; output outside source tree |
| Exported PCK re-open --smoke | PASS | explicit project working directory; DEMO_SMOKE_PASS / exit0 |
| ARM32/uClibc compile of public checkout | NOT_RUN | matching SDK not in this curation environment |
| Neutral demo/new portable launcher on board | NOT_RUN | no hardware operations authorized for curation |
| Native Linux supervisor/header/FD/reaping/pidfd/queued-signal tests | PASS on GitHub Ubuntu | actual production-supervisor compile and native syscall tests; not ARM/board acceptance |
| Shared lifecycle failure model, MSVC19.16 | PASS | deterministic TERM at STOP/spawn/wait boundaries, short backup/write/CONT/identity/reap failure; not native POSIX |

Editor available locally was official3.6.3 **Mono**, used only for GDScript
host smoke/export, not C# or Godot4. Some runs emitted Mono debugger-port
warnings. A PCK launch without explicit project working directory produced a
native Mono error and no smoke marker despite exit0: treated as FAIL, not success.
A second attempt with a differently scoped editor and ambiguous pack path timed
out without smoke marker, also treated as FAIL.
Rerun from demo directory with --path . and explicit relative pack passed.
Use the standard non-Mono3.6.3 editor as documented for cleaner reproduction.
All generated .mono/.import/cache/binaries/PCK stayed outside the final tree.

Reproduction commands (repo root; adapt executable names):

~~~sh
python3 tests/test_repository.py
python3 -B tests/test_install.py /explicit/disposable/pinned-godot-checkout
c++ -std=c++11 -Iport/godot3 port/godot3/test_viewport_mapping.cpp -o /tmp/mapping-test
/tmp/mapping-test
for f in scripts/*.sh; do sh -n "$f"; done
godot3 --no-window --path demo -- --smoke
godot3 --no-window --path demo -s ../tests/demo_smoke.gd
mkdir -p out
godot3 --no-window --path demo --export-pack "RV1106 CPU PCK" ../out/panel.pck
(cd demo && godot3 --no-window --path . --main-pack ../out/panel.pck -- --smoke)
~~~

No framebuffer output, optics, displayed FPS or physical touch performance is
claimed from these host tests. Historical evidence has separate scope in EVIDENCE.md.

## Safety revision after independent review

Replaced shell traps with a native pidfd supervisor and signal-blocked state
machine; added explicit LVGL ungrab/regrab hooks, descriptor isolation, asynchronous
waitpid reaper, retained recovery snapshots on failure, and required-device exit70.
No new hardware/ARM acceptance is claimed. Native tests exercise actual Linux
syscalls plus ordinary-file/mock fixtures; the local MSVC lifecycle test covers
only the dependency-free state model, **not** Linux/POSIX compilation.

Ubuntu CI now compiles production supervisor, runs shared lifecycle mock and
native FD-sentinel/exec/reaper/pidfd/queued TERM/restore/device-failure tests, scans
full fetched repository history, verifies manifest and pinned installer replay.
Observed GitHub run [38842940954](https://github.com/OS49Art/luckfox-rv1106-godot3/actions/runs/38842940954) completed successfully for commit 3b6e480d8e0ab1ca36e8a63a3c1f750c52644bd8 on 2026-10-16. All steps passed: privacy/history/image gates, 42 manifest hashes, shell syntax, mapping, production Linux supervisor compile, lifecycle model, native POSIX tests and pinned installer replay. The job completed in 20 seconds. Runner/action deprecation notices were informational, not test failures. These are actual Linux host results, not ARM ABI, board, optical or manual-touch acceptance.

## Original figure integration

Only four reviewed original figure artifacts (architecture/deployment SVG+PNG)
and adapted public captions were added. SVG is primary editable source; PNG
exports have no text/EXIF/private metadata chunks. Static SVG copyright metadata
is plaintext and audited. Relative README/caption image/source links are checked.
No generator, private guidance/brief, QA JSON, logs, HTML or alternate render
artifacts are distributed. Figures describe new experimental pidfd supervision
and four-file matched recovery; no fresh board/native-CI acceptance is inferred.

## Documentation screenshot integration — 2026-10-16

[Gallery](SCREENSHOTS.md): three metadata-only sanitized historical actual RV
framebuffer PNGs (2026-10-15), seven newly captured Godot3.6.3/GDScript GLES2
viewport PNGs (360 source plus six desktop offscreen target sizes). Actual capture
process exit0 and GALLERY_CAPTURE_PASS; each engine target had expected dimensions.
All six targets matched independent same-state nearest sampling at every content
pixel; all retained bars were black. A fresh capture run removed redundant
Image.flip_y calls with render_target_v_flip=true; the invariant top header
matches the independent upright historical RV header at every compared pixel
for the 360 source and all six targets. Top-header/bottom-action geometry was
also asserted independently of the scaling source. Rejected inverted captures
remain private only; new raw/public visible hashes replace their old values.
768×1024 has fractional 32/15 scale, not
uniform integer pixels. Native hidden windows were measured separately: tall
requests clamp to height1203; they are not the published full-size target captures.
No mobile, optical, board, new neutral-demo or supervisor acceptance is inferred.

Python repository gates now PASS9 (the existing eight plus screenshot scope),
including exact ten-file screenshot allowlist/dimensions, PNG CRC/chunk/bounded
pixel validation, metadata and corrupt/decompression negative controls, relative
links and unchanged index/history/all-blob privacy scans. Metadata removal kept
all original visible RGBA pixel hashes; only IHDR/IDAT/IEND remain. The maintained
prototype source tree hashes before/after the controlled capture run matched.
Private helpers/logs/art/font sources and packs remain outside this public tree.
[Artwork scope](../LICENSES/SCREENSHOTS.md) excludes depicted material from any
implied MIT asset reuse grant. This documentation-only addition does not change
runtime, launcher, backend, supervisor, neutral demo or CI workflow code.
