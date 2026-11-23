"""Publish a single-task grade with its original completion time."""
import fcntl
import json
import os
import tempfile
import time
from pathlib import Path


def write_single_grade(cell, grade):
    cell = Path(cell)
    result = cell / 'series_grade_v3.json'
    # Serialize regrades of the same cell. Readers see one complete JSON document.
    directory = os.open(cell, os.O_RDONLY | os.O_DIRECTORY)
    try:
        fcntl.flock(directory, fcntl.LOCK_EX)
        if result.exists():
            # A legacy observation has no recoverable first completion time.
            first = json.loads(result.read_text()).get('first_grade_ns')
        else:
            first = time.time_ns()
        if first is not None:
            grade = dict(grade, first_grade_ns=first)
        with tempfile.TemporaryDirectory(dir=cell) as tmp:
            staged = Path(tmp) / result.name
            staged.write_text(json.dumps(grade, indent=2))
            os.replace(staged, result)
    finally:
        os.close(directory)
