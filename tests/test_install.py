"""Run only against an explicit disposable pinned upstream checkout."""
import hashlib
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
def install():
    for script in ('patch_godot3_cross.py','install_godot3_cpu.py'):
        subprocess.check_call([sys.executable, str(ROOT/'scripts'/script), str(source)])
def digest():
    files = ['platform/server/detect.py','platform/server/os_server.cpp']
    files += ['drivers/dummy/'+n for n in ('rasterizer_cpu.cpp','rasterizer_cpu.h','cpu_viewport_mapping.h','cpu_required_device.h')]
    return [hashlib.sha256((source/f).read_bytes()).hexdigest() for f in files]
install()
before = digest()
install()
if digest() != before:
    raise SystemExit('Installer is not idempotent')
scsub = (source/'drivers/dummy/SCsub').read_text()
if '*.cpp' not in scsub:
    raise SystemExit('Upstream no longer collects the CPU .cpp')
print('PASS pinned installer replay/idempotence/source collection')
