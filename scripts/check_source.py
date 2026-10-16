"""Fail closed unless this is the pinned upstream checkout (local patches allowed)."""
from pathlib import Path
import subprocess
import sys
PIN = "a3378686f7196db78d7b3cc7947f72a13bc4864c"
def check(root):
    root = Path(root).resolve()
    if not (root / ".git").exists():
        raise SystemExit("Requires a Git checkout, not an unverified source archive")
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], universal_newlines=True).strip()
    if head != PIN:
        raise SystemExit("Refusing unpinned upstream HEAD")
    return root
if __name__ == "__main__":
    check(sys.argv[1])
