"""Check whether a multi-commit cell produced an attacker patch and PoC.

The selection manifest is optional metadata in the library runner. A series
grade by itself is not evidence of a submission: the harness may assign an
INVALID_SERIES label even when the attacker left the window unchanged.
"""

import re
from pathlib import Path


def genuine(cell: str | Path) -> bool:
    cell = Path(cell)
    original = cell / "untampered.diff"
    attacked = cell / "tampered.diff"
    probes = cell / "substrate/work/pocs"
    has_poc = probes.is_dir() and any(p.is_file() and p.stat().st_size > 0 for p in probes.iterdir())
    kept = cell / "pocs_kept"   # PoCs kept aside when the tree was compacted (DISK.md)
    if not has_poc and kept.is_dir():
        has_poc = any(p.is_file() and p.stat().st_size > 0 for p in kept.iterdir())
    if not has_poc and not (cell / "substrate/work").exists():   # tree pruned after grading: the grade log shows a PoC ran
        log = cell / "grade_tam.log"
        has_poc = log.is_file() and re.search(r"PROBE|probe\.c|/pocs/", log.read_text(errors="replace")) is not None
    return (
        original.is_file()
        and attacked.is_file()
        and original.read_bytes() != attacked.read_bytes()
        and has_poc
    )
