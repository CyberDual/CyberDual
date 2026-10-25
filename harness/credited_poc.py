#!/usr/bin/env python3
"""Print the credited PoC of a run cell, chosen exactly as rereview_any.sh chooses it.

  credited_poc.py CELL

Order: a C/C++ source named by selection.json's pocs[0].input; then the canonical names (residual, newbug,
probe; .c then .cpp) under CELL/pocs and CELL/substrate/work/pocs; then any C/C++ file there, skipping
fix-verification and feature-check programs. Exits 1 if there is none.
"""
import json
import re
import sys
from pathlib import Path

SRC = ('.c', '.cc', '.cpp', '.cxx', '.C')
CHECK = re.compile(r'check_fix|feature_check|verify|_fix\.|/check[_.]|fixcheck', re.I)


def credited_poc(cell):
    cell = Path(cell)
    roots = [cell / 'substrate' / 'work', cell]
    sel = cell / 'selection.json'
    if sel.is_file():
        try:
            named = ((json.loads(sel.read_text()).get('pocs') or [{}])[0] or {}).get('input') or ''
        except (ValueError, AttributeError, IndexError):
            named = ''
        if named.endswith(SRC):
            for root in roots:
                if (root / named).is_file():
                    return root / named
    order = [cell / 'pocs/residual.c', cell / 'substrate/work/pocs/residual.c',
             cell / 'substrate/work/pocs/newbug.c', cell / 'substrate/work/pocs/newbug.cpp',
             cell / 'pocs/newbug.c', cell / 'pocs/newbug.cpp',
             cell / 'substrate/work/pocs/probe.c', cell / 'substrate/work/pocs/probe.cpp',
             cell / 'pocs/probe.c', cell / 'pocs/probe.cpp']
    for p in order:
        if p.is_file():
            return p
    found = sorted(p for d in (cell / 'pocs', cell / 'substrate/work/pocs') if d.is_dir()
                   for p in d.iterdir() if p.is_file() and p.name.endswith(SRC))
    keep = [p for p in found if not CHECK.search(str(p))]
    return (keep or found or [None])[0]


if __name__ == '__main__':
    p = credited_poc(sys.argv[1])
    if p is None:
        sys.exit(1)
    print(p)
