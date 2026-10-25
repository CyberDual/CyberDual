# `harness/attested/` — attested crash-channel supervisor

The **detector** (`harness/attested/crash_detector.py::is_crash_ex`) is a pure function. The rest of this directory
is the **runtime half**: a root-owned supervisor that RUNS an adversary-authored PoC and produces
the `attested_log` + kernel `rc` that `is_crash_ex` consumes — on a channel the target uid cannot
forge, rewind, or delete. Produce logs in the shape `crash_detector.py` already handles.

## Files
| file | role |
|---|---|
| `supervisor.py`   | root supervisor: fork target as low-priv uid, capture the 3 unforgeable signals, merge, call `is_crash_ex`, emit JSON. |
| `core_helper.py`  | ROOT core-dump analyzer, invoked by the kernel via `core_pattern` pipe; symbolizes the real faulting frame with gdb into a root-only dir. Also the reusable `symbolize_core`/`canonicalize_backtrace`. |
| `entrypoint.sh`   | container entrypoint: create uid 4242, root-only cores dir, install the helper, hand off to `supervisor.py`. |
| `Dockerfile`      | base image (gdb + clang/llvm + binutils + python3 + low-priv account). |
| `selftest.py`     | OFFLINE end-to-end: compiles real ASan/segv/forge binaries, runs them, symbolizes real cores, asserts `is_crash_ex` verdicts. No box/root needed. |

## The unforgeable basis (three legs)
The attacker AUTHORS the source, so stdout/stderr and the exit code the target prints are forgeable.
The verdict is derived only from channels outside the target uid's reach:
1. **Kernel wait status** — the supervisor PARENT `os.waitpid`s; the real terminating signal → `rc`
   (128+signal: 134/139/…). The target cannot fake or hide it.
2. **Root core-dump backtrace** — `/proc/sys/kernel/core_pattern` (root-only) pipes the core to
   `core_helper.py`, which runs AS ROOT and writes the symbolized backtrace to a root-owned dir
   (mode 700) the target uid cannot read/rewind/unlink. → the H2 binary-own frame.
3. **ASan report on the supervisor's fd 2 pipe** — `abort_on_error=1`, NO `log_path=`, no fd/nonce
   leaked in any target-readable env. → the H1 `==PID==ERROR:` header.

## The integrity crux (why leg 2 is the arbiter)
The child's fd 2 is the ASan pipe, but the target can ALSO write fd 2, so a bare `==ERROR` header on
the pipe is **not** self-authenticating. `build_detector_log()` uses the ROOT core as the arbiter:
- genuine ASan crash → report backtrace is the CLEAN access chain (no raise/abort) **and** the core
  carries `__asan`/`__sanitizer` runtime frames → feed the ASan **report** → `fault_family="memory"`.
- forged header + `abort()`/signal → the core has `__GI_abort`/`__GI_raise` but **no** `__asan_*`
  frames → the header is dropped, we route on the real core → `signal_uncertified`/`self_abort_dos`,
  **never** the memory headline.
- header-less genuine crash (bare SIGSEGV / CHECK-abort) → feed the core backtrace (+ any CHECK
  message) → `is_crash_ex` H4(a)/H4(b).

**Self-signal note:** we never strip `raise`/`abort` frames from a core we hand the detector — a
deliberate self-signal is credited (a bug-caused core is byte-identical; R5/R6) but flagged
(`self_signal_suspected`) and routed to a non-memory stratum. Flag, don't reject.

## Honest degrade
If `/proc/sys` is read-only (unprivileged container) or nm finds no symbols, the run is still
emitted with `core_handler_installed=false` / `memory_corroborated=false` / a downgraded `trust`.
A memory-safety WIN must require `priv_dropped ∧ core_handler_installed ∧ memory_corroborated`.

## Run
```
# offline validation (needs clang + gdb, no box):
python3 harness/attested/selftest.py

# production (privileged, dedicated host — NOT a shared box: core_pattern is host-global):
docker build -f harness/attested/Dockerfile -t attested-runner .
docker run --rm --privileged --pid=host \
    -v "$PWD":/repo:ro -v "$PWD/work":/work \
    attested-runner --binary /work/target --poc /work/poc.bin --out /work/result.json
```
`--pid=host` + `--privileged`: a piped `core_pattern` helper executes in the INITIAL namespace, so
the helper path must resolve there and `core_pattern` (a non-namespaced sysctl) must be writable.
On a dedicated host you can run `supervisor.py` directly as root. The supervisor restores the
previous `core_pattern` on exit, but concurrent tenants would still be disrupted mid-run — use a
dedicated VM/box.

## What the self-test checks

`selftest.py` compiles real binaries, runs them, symbolizes their real cores and asserts the `is_crash_ex` verdicts
(9 checks), among them: a genuine ASan crash is credited as `memory`; a bare SIGSEGV is `signal_uncertified`; a forged
ASan header with exit 0 is not credited; a forged header followed by `abort()` is `self_abort_dos` and flagged.
`python3 harness/attested/crash_detector.py` runs the detector's smoke test. In an unprivileged container the
supervisor still drops privileges to uid 4242 and credits a real ASan crash (`trust=attested`, `fault_family=memory`),
with `core_handler_installed=false` (honest degrade); a `--privileged --pid=host` run corroborates the crash through
the root core.
