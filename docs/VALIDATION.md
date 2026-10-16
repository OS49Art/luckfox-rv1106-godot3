# Public-curation validation — 2026-10-16

These tests were actually executed locally; CI has not yet run on GitHub.

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
| Native Linux supervisor/header/FD/reaper/pidfd/queued-signal tests | NOT_RUN locally | no native Linux environment; added Ubuntu CI compile/run gate, not yet observed |
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
CI result remains NOT_RUN until an actual GitHub run is collected.

## Original figure integration

Only four reviewed original figure artifacts (architecture/deployment SVG+PNG)
and adapted public captions were added. SVG is primary editable source; PNG
exports have no text/EXIF/private metadata chunks. Static SVG copyright metadata
is plaintext and audited. Relative README/caption image/source links are checked.
No generator, private guidance/brief, QA JSON, logs, HTML or alternate render
artifacts are distributed. Figures describe new experimental pidfd supervision
and four-file matched recovery; no fresh board/native-CI acceptance is inferred.
