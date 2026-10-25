#!/usr/bin/env python3
"""harness/attested/core_helper.py — the ROOT-owned kernel core-dump analyzer.

This is leg (2) of the unforgeable channel described in harness/attested/crash_detector.py's docstring:
a ROOT-analyzed CORE DUMP backtrace. It is installed as the target of
`/proc/sys/kernel/core_pattern`:

    |/opt/attested/core_helper.py %E %s %p %t

When a supervised target faults, the KERNEL (not the target) invokes this program AS
ROOT and feeds the raw core dump on **stdin**. The target uid (4242) can neither:

  * write `/proc/sys/kernel/core_pattern`  (root-only sysctl) — so it cannot redirect,
  * intercept or replace this helper        (root-owned, mode 0755, runs in the initial
                                              namespace as pid-1's child, not the target's),
  * read / rewind / unlink the output       (written to CORES_DIR, a root-owned dir mode
                                              0700 that uid 4242 cannot enter).

Therefore the backtrace this helper extracts — the REAL faulting frame inside the target
binary — is a signal the target UID cannot forge, rewind, or delete. It is symbolized by
`gdb` against the on-disk target binary (whose symbol table the ATTACKER-authored source
cannot change post-compile in a way that renames a root-observed runtime frame).

OUTPUT (all under CORES_DIR, root:root 0700):
  * <key>.core   — the raw core (kept for audit; root-only)
  * <key>.bt     — the canonicalized backtrace block, in the EXACT shape
                   harness/attested/crash_detector.py::_FRAME / _ATTESTED_HEADER consumers expect:
                       Program terminated with signal SIGSEGV, Segmentation fault.
                           #0 0x<addr> in <func> <file>:<line>
                           #1 0x<addr> in <func> <file>:<line>
                   Frames are rendered VERBATIM including any `raise`/`abort`/`kill`
                   self-signal frames — we never editorialize the core, so the detector's
                   `_has_self_signal_frame` can route a deliberate self-signal OUT of the
                   memory-safety headline (see the self_signal note in the supervisor).
  * <key>.json   — {pid, signal, signame, exe, bt_path} machine-readable meta.

`<key>` = "<pid>.<time>" so the supervisor can correlate by the pid it forked, with a
newest-file-since-start fallback for the pid-namespace case (documented in the supervisor).

IMPORT SAFETY: importing this module has NO side effects (the kernel entrypoint is under
`if __name__ == "__main__"`). `symbolize_core()` / `canonicalize_backtrace()` are reused by
the supervisor and the offline selftest, so they must stay pure/subprocess-only.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time

# Root-owned, mode 0700 directory the target uid cannot enter/read/write/unlink.
# Overridable for the offline selftest (which has no root and no core_pattern).
CORES_DIR = os.environ.get("ATTESTED_CORES_DIR", "/var/attested/cores")

# signal number -> (name, human description) for the "Program terminated ..." line.
_SIGNAMES = {
    4: ("SIGILL", "Illegal instruction"),
    5: ("SIGTRAP", "Trace/breakpoint trap"),
    6: ("SIGABRT", "Aborted"),
    7: ("SIGBUS", "Bus error"),
    8: ("SIGFPE", "Floating point exception"),
    11: ("SIGSEGV", "Segmentation fault"),
}

# A gdb `bt` frame line. Two shapes:
#   "#0  0x0000555... in decode_chunk (p=0x1, n=5) at src/decode.c:77"
#   "#1  decode_chunk (p=0x1) at src/decode.c:77"           (inlined: no address)
#   "#2  0x00007f... in ?? ()"                              (unsymbolized)
_GDB_FRAME = re.compile(
    r"^#(?P<idx>\d+)\s+"
    r"(?:(?P<addr>0x[0-9a-fA-F]+)\s+in\s+)?"     # optional "0xADDR in "
    r"(?P<func>[^\s(]+)"                          # function symbol (up to '(' or space)
    r"[^\n]*?"                                    # args / junk
    r"(?:\bat\s+(?P<loc>[^\s:]+:\d+(?::\d+)?))?"  # optional "at file:line[:col]"
    r"\s*$"
)


def _signame(signum: int):
    return _SIGNAMES.get(int(signum), ("SIG%d" % int(signum), "Fatal signal"))


def canonicalize_backtrace(gdb_text: str, signum: int) -> str:
    """Render raw `gdb bt` output into the canonical block harness/attested/crash_detector.py expects.

    Pure string transform (no I/O). Preserves EVERY frame — including libc raise/abort/kill
    frames — so the detector can flag a self-signal; it never drops or reorders frames.
    Synthesizes `0x0` for inlined frames that gdb printed without an address, so the
    detector's `#N 0x.. in <sym>` regex still matches and the binary-own symbol is counted.
    """
    name, desc = _signame(signum)
    lines = ["Program terminated with signal %s, %s." % (name, desc)]
    for raw in (gdb_text or "").splitlines():
        m = _GDB_FRAME.match(raw.strip())
        if not m:
            continue
        idx = m.group("idx")
        addr = m.group("addr") or "0x0"
        func = m.group("func")
        loc = m.group("loc") or ""
        # skip pure-noise frames gdb emits with no resolvable function name
        if func in ("", "()"):
            continue
        frame = "    #%s %s in %s" % (idx, addr, func)
        if loc:
            frame += " " + loc
        lines.append(frame)
    return "\n".join(lines) + "\n"


def symbolize_core(exe: str, core_path: str, signum: int, timeout: int = 60) -> str:
    """Run gdb against (exe, core) and return the canonicalized backtrace block.

    Uses `-nx` (ignore the invoking user's ~/.gdbinit — the target must not influence gdb
    via a planted init file) and `-batch`. Returns "" if gdb is unavailable or fails; the
    caller then falls back to the ASan-pipe text / kernel signal alone (honest degrade).
    """
    if not (exe and os.path.exists(exe) and os.path.exists(core_path)):
        return ""
    try:
        proc = subprocess.run(
            ["gdb", "-q", "-nx", "-batch",
             "-ex", "set pagination off",
             "-ex", "bt",
             exe, core_path],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=timeout, check=False,
        )
        gdb_out = proc.stdout.decode("utf-8", "replace")
    except (OSError, subprocess.SubprocessError):
        return ""
    return canonicalize_backtrace(gdb_out, signum)


def _kernel_entry(argv) -> int:
    """Invoked by the kernel as root via `|core_helper.py %E %s %p %t`. Core on stdin."""
    # %E is the executable path with '/' replaced by '!' (and leading '!'); restore it.
    exe = (argv[1] if len(argv) > 1 else "").replace("!", "/")
    signum = int(argv[2]) if len(argv) > 2 and argv[2].isdigit() else 0
    pid = argv[3] if len(argv) > 3 else "0"
    tstamp = argv[4] if len(argv) > 4 else str(int(time.time()))

    os.makedirs(CORES_DIR, exist_ok=True)
    try:
        os.chmod(CORES_DIR, 0o700)  # target uid cannot enter/read/unlink
    except OSError:
        pass

    key = "%s.%s" % (pid, tstamp)
    core_path = os.path.join(CORES_DIR, key + ".core")

    # Drain the raw core from stdin to a root-only file.
    with open(core_path, "wb") as f:
        data = sys.stdin.buffer.read()
        f.write(data)
    try:
        os.chmod(core_path, 0o600)
    except OSError:
        pass

    bt = symbolize_core(exe, core_path, signum)
    name, _ = _signame(signum)

    bt_path = os.path.join(CORES_DIR, key + ".bt")
    with open(bt_path, "w") as f:
        f.write(bt)
    with open(os.path.join(CORES_DIR, key + ".json"), "w") as f:
        json.dump({"pid": pid, "signal": signum, "signame": name,
                   "exe": exe, "time": tstamp, "bt_path": bt_path,
                   "core_path": core_path}, f)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(_kernel_entry(sys.argv))
    except Exception:  # a crashing core handler must never wedge the kernel pipe
        sys.exit(0)
