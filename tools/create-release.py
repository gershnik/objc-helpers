#! /usr/bin/env -S python3 -u

import sys
import re
import subprocess

from pathlib import Path
from datetime import date

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent

NEW_VER = sys.argv[1]

unreleased_link_pattern = re.compile(r"^\[Unreleased\]: (.*)$", re.DOTALL)
lines = []
with open(ROOT / "CHANGELOG.md", "rt") as change_log:
    for line in change_log.readlines():
        # Move Unreleased section to new version
        if re.fullmatch(r"^## Unreleased.*$", line, re.DOTALL):
            lines.append(line)
            lines.append("\n")
            lines.append(
                f"## [{NEW_VER}] - {date.today().isoformat()}\n"
            )
        else:
            lines.append(line)
    lines.append(f'[{NEW_VER}]: https://github.com/gershnik/objc-helpers/releases/v{NEW_VER}\n')

with open(ROOT / "CHANGELOG.md", "wt") as change_log:
    change_log.writelines(lines)

subprocess.run(['git', 'add', ROOT / "CHANGELOG.md"], check=True)
subprocess.run(['git', 'commit', '-m', f'chore: creating version {NEW_VER}'], check=True)
subprocess.run(['git', 'tag', f'v{NEW_VER}'], check=True)
