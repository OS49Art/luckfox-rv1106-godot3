"""Replayable ARM/uClibc build fixes, pinned to official Godot 3.6.3.

Only edits the explicitly supplied source checkout, never the SDK.
"""
import pathlib
import sys

sys.dont_write_bytecode = True
from check_source import check
root = check(sys.argv[1])
path = root / "platform/server/detect.py"
text = path.read_text()
old = '    if host_is_64_bit and (env["bits"] == "32" or env["arch"] == "x86"):'
new = '    if env["arch"] not in ("arm", "arm64") and host_is_64_bit and (env["bits"] == "32" or env["arch"] == "x86"):'
old2 = '    elif not host_is_64_bit and (env["bits"] == "64" or env["arch"] == "x86_64"):'
new2 = '    elif env["arch"] not in ("arm", "arm64") and not host_is_64_bit and (env["bits"] == "64" or env["arch"] == "x86_64"):'
for before, after in ((old, new), (old2, new2)):
    if after not in text:
        if text.count(before) != 1: raise SystemExit("Unexpected upstream source; refusing patch")
        text = text.replace(before, after)
path.write_text(text)
print("Patched ARM cross-architecture flags (no compiler-version bypass).")
