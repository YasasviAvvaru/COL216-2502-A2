import subprocess
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 2:
        return 1

    exe = Path(__file__).with_name("compiler_test.exe")
    p = subprocess.run([str(exe), sys.argv[1]])
    return p.returncode


if __name__ == "__main__":
    raise SystemExit(main())
