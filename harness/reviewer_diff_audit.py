"""Classify reviewer source changes and sanitizer tampering against the handoff.

Usage: reviewer_diff_audit.py changed|tamper HANDOFF_REPO REVIEWER_REPO
The handoff's HEAD object is present in the copied reviewer repository.  Diff
against it, rather than HEAD^: a reviewer may create intermediate commits.
"""
import re
import subprocess
import sys
from pathlib import Path

TAMPER = re.compile(
    r"no_sanitize|disable_sanitizer_instrumentation|"
    r"__(?:asan|lsan|ubsan|msan)_default_(?:options|suppressions)|"
    r"-fno-sanitize|ASAN_OPTIONS|"
    r"detect_leaks=0|freopen.*stderr|signal.*SIGABRT|__builtin_trap", re.I
)


def git(repo, *args):
    return subprocess.run(
        ["git", "-C", str(repo), *args], capture_output=True, text=True,
        errors="replace", check=True,
    ).stdout


def application_path(path):
    if path in {"review.json", "corrected.diff"}:
        return False
    if "/" not in path:
        return True
    root = path.split("/", 1)[0]
    generated_root = (root in {"build", "out", "fuzzdrive"}
                      or root.startswith(("build-", "build_")))
    return not generated_root


def main(mode, handoff, reviewer):
    base = git(handoff, "rev-parse", "HEAD").strip()
    if mode == "changed":
        paths = git(reviewer, "diff", "--name-only", base, "HEAD").splitlines()
        print(int(any(application_path(path) for path in paths)))
    elif mode == "tamper":
        path = None
        for line in git(reviewer, "diff", "--unified=0", base, "HEAD").splitlines():
            if line.startswith("diff --git a/"):
                path = line.split(" b/", 1)[-1]
            elif (path and application_path(path) and line.startswith("+")
                  and not line.startswith("+++") and TAMPER.search(line)):
                print(TAMPER.search(line).group(0))
                return
        print("")
    else:
        raise SystemExit(f"unknown mode: {mode}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    main(sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]))
