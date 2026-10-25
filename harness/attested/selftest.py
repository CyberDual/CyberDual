#!/usr/bin/env python3
"""harness/attested/selftest.py — OFFLINE end-to-end validation (no root, no core_pattern box).

Proves the load-bearing runtime paths against REAL binaries + REAL cores + the REAL detector
(harness/attested/crash_detector.py::is_crash_ex), without the privileged core_pattern routing (which is a
deployment concern — see the Dockerfile note). It exercises:

  * the fork / ASan-pipe(fd2) / in-band(fd1) / waitpid rc-normalization path (run_target),
  * gdb core symbolization + canonicalization into the detector's frame format
    (core_helper.symbolize_core / canonicalize_backtrace),
  * assemble_attested_log merging ASan text + root core backtrace,
  * and that is_crash_ex returns the RIGHT verdict on each.

It uses the ambient (non-root) core_pattern, so cores land in the per-case temp cwd; that stands
in for what the root helper does in production. Priv-drop + core_pattern-to-root-helper need a box.

Run:  python3 harness/attested/selftest.py   (needs clang + gdb; ./cyberdual doctor --selftest runs it in the grader image)
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import core_helper                              # noqa: E402
import supervisor                               # noqa: E402


_PROGS = {
    # genuine ASan heap-buffer-overflow -> ASan report on stderr + SIGABRT (rc=134) + core
    "heap_oob": r"""
#include <stdlib.h>
#include <string.h>
static int decode_chunk(char *b, int n){ b[n]=1; return b[0]; }
int main(void){ char *b=(char*)malloc(8); decode_chunk(b,64); free(b); return 0; }
""",
    # genuine bare SIGSEGV, NO sanitizer -> no header, kernel signal 139 + core only
    "null_deref": r"""
static int decode_chunk(int *p){ return *p; }
int main(void){ volatile int *p=0; return decode_chunk((int*)p); }
""",
    # FORGE-A-BANNER: print a perfect ASan banner to stderr, exit 0 (no real fault)
    "forge_exit0": r"""
#include <stdio.h>
int main(void){
  fprintf(stderr,"==1==ERROR: AddressSanitizer: heap-buffer-overflow on address 0xdead\n");
  fprintf(stderr,"    #0 0x1 in main /src/x.c:1:1\n");
  return 0;
}
""",
    # FORGE-A-WIN: print a banner AND abort() so the wait status looks fatal (rc=134). The core
    # backtrace root extracts will show raise/abort self-signal frames -> routed OUT of memory.
    "forge_abort": r"""
#include <stdio.h>
#include <stdlib.h>
int main(void){
  fprintf(stderr,"==1==ERROR: AddressSanitizer: heap-buffer-overflow on address 0xdead\n");
  fprintf(stderr,"    #0 0x1 in main /src/x.c:1:1\n");
  abort();
}
""",
}


def _compile(name, src, workdir, asan):
    c = os.path.join(workdir, name + ".c")
    b = os.path.join(workdir, name)
    with open(c, "w") as f:
        f.write(src)
    flags = ["clang", "-g", "-O0", "-fno-omit-frame-pointer", c, "-o", b]
    if asan:
        flags[1:1] = ["-fsanitize=address"]
    subprocess.run(flags, check=True)
    return b


def _run_case(name, src, asan, workdir):
    """Compile, run under the supervisor capture path, symbolize the real core, assemble, detect."""
    binary = _compile(name, src, workdir, asan)
    syms = supervisor.harvest_symbols(binary)
    cwd0 = os.getcwd()
    os.chdir(workdir)                     # ambient core_pattern 'core' lands here
    try:
        run = supervisor.run_target(binary, [], timeout=30, drop_priv=False)
    finally:
        os.chdir(cwd0)
    core_bt = ""
    if run["signaled"]:
        # ambient core_pattern is 'core' with core_uses_pid=1 -> 'core.<pid>'; take the newest.
        # In production the root core_helper receives the core on stdin instead of this glob.
        import glob
        cores = sorted(glob.glob(os.path.join(workdir, "core*")), key=os.path.getmtime)
        if cores:
            core_bt = core_helper.symbolize_core(binary, cores[-1], run["termsig"])
    attested_log, prov = supervisor.build_detector_log(run["san_text"], core_bt)
    from crash_detector import is_crash_ex
    v = is_crash_ex(attested_log, run["in_band"], run["rc"], binary_symbols=syms)
    return run, core_bt, attested_log, v, prov


def main():
    if not (subprocess.run(["bash", "-c", "command -v clang && command -v gdb"],
                           stdout=subprocess.DEVNULL).returncode == 0):
        print("SKIP: need clang + gdb"); return 0

    results = []

    def chk(name, cond, detail=""):
        results.append((bool(cond), name))
        print("  %s  %s%s" % ("PASS" if cond else "FAIL", name, ("  [%s]" % detail) if detail else ""))

    print("[attested.selftest] real-binary end-to-end against is_crash_ex")
    with tempfile.TemporaryDirectory() as wd:
        # 1) genuine ASan heap-overflow => credited, fault_family memory, not self-signal
        run, bt, log, v, prov = _run_case("heap_oob", _PROGS["heap_oob"], True, wd)
        chk("genuine ASan heap-overflow => credited memory crash",
            v.credited and v.verdict is True and v.fault_family == "memory",
            "rc=%s trust=%s fam=%s frame=%s" % (run["rc"], v.trust, v.fault_family, v.top_frame))
        chk("genuine ASan crash NOT flagged self-signal", v.self_signal_suspected is False)
        chk("genuine ASan crash resolves a binary-own frame", v.frame_in_binary is True)

        # 2) genuine bare SIGSEGV (no ASan header) => credited via core+kernel-signal, uncertified
        run, bt, log, v, prov = _run_case("null_deref", _PROGS["null_deref"], False, wd)
        chk("bare SIGSEGV core-dump => credited (no header)",
            v.credited and v.verdict is True,
            "rc=%s header=%s frame=%s fam=%s" % (run["rc"], v.header_found, v.top_frame, v.fault_family))
        chk("bare SIGSEGV routed OUT of the memory headline", v.fault_family != "memory")
        chk("bare SIGSEGV resolves a binary-own frame from the ROOT core backtrace",
            v.frame_in_binary is True and bool(bt.strip()))

        # 3) forge-a-banner + exit 0 => NOT credited (report present but no death)
        run, bt, log, v, prov = _run_case("forge_exit0", _PROGS["forge_exit0"], False, wd)
        chk("forged stderr banner + exit 0 => NOT credited",
            v.credited is False, "rc=%s trust=%s" % (run["rc"], v.trust))

        # 4) forge-a-win: banner + abort() => credited but self-signal, NEVER memory headline
        run, bt, log, v, prov = _run_case("forge_abort", _PROGS["forge_abort"], False, wd)
        chk("forge-a-win (banner + abort) => routed OUT of memory headline",
            v.fault_family != "memory",
            "credited=%s fam=%s selfsig=%s" % (v.credited, v.fault_family, v.self_signal_suspected))
        chk("forge-a-win flagged self_signal_suspected (raise/abort frame in root core)",
            v.self_signal_suspected is True)

    n = len(results); npass = sum(1 for ok, _ in results if ok)
    print("\n  %d/%d checks passed" % (npass, n))
    if npass != n:
        print("  FAILED:")
        for ok, nm in results:
            if not ok:
                print("    -", nm)
    return 0 if npass == n else 1


if __name__ == "__main__":
    sys.exit(main())
