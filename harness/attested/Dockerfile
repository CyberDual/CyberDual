# harness/attested/Dockerfile — base image for the attested crash-channel supervisor.
#
# Provides gdb (root core symbolization), clang/llvm-symbolizer (ASan builds + frame
# resolution), nm/binutils (host-side symbol harvest), python3 (supervisor + helper), and a
# low-priv target account. The supervisor + helper are the runtime; the DETECTOR
# (harness/attested/crash_detector.py) is pure and mounted read-only with the repo.
#
# BUILD:
#   docker build -f harness/attested/Dockerfile -t attested-runner .
#
# RUN (see entrypoint.sh header for why these flags):
#   docker run --rm --privileged --pid=host \
#       -v "$PWD":/repo:ro -v "$PWD/work":/work \
#       -e PYTHONPATH=/repo \
#       attested-runner --binary /work/target --poc /work/poc.bin --out /work/result.json
#
# BASE-IMAGE NOTE: any glibc distro with gdb + a sanitizer toolchain works (ubuntu:22.04 here).
# core_pattern is a NON-namespaced, host-global sysctl and a piped helper runs in the INITIAL
# namespace, so the container needs --privileged (writable /proc/sys) and the helper must be
# reachable at the initial-namespace path — either bake it at /opt/attested and run --pid=host,
# or run supervisor.py directly on a dedicated host as root. On a SHARED host, do NOT mutate
# core_pattern globally; use a dedicated VM/box (the supervisor restores the previous value on
# exit, but concurrent tenants would still be disrupted mid-run).
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 gdb clang llvm binutils libc6-dbg ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# low-priv target account (uid/gid 4242) — the adversary-authored PoC runs here, never as root.
RUN groupadd -g 4242 attested_target \
    && useradd -u 4242 -g 4242 -M -N -s /usr/sbin/nologin attested_target

# root-only cores dir the target uid cannot enter/read/rewind/unlink.
RUN mkdir -p /var/attested/cores && chown 0:0 /var/attested/cores && chmod 700 /var/attested/cores

# runtime: supervisor + root core helper, at the initial-namespace path.
RUN mkdir -p /opt/attested
COPY harness/attested/core_helper.py /opt/attested/core_helper.py
COPY harness/attested/supervisor.py  /opt/attested/supervisor.py
COPY harness/attested/entrypoint.sh  /opt/attested/entrypoint.sh
RUN chmod 0755 /opt/attested/core_helper.py /opt/attested/supervisor.py /opt/attested/entrypoint.sh

# the pure detector lives in the mounted repo; make it importable.
ENV PYTHONPATH=/repo
ENV ATTESTED_CORE_HELPER=/opt/attested/core_helper.py
ENV ATTESTED_CORES_DIR=/var/attested/cores

ENTRYPOINT ["/opt/attested/entrypoint.sh"]
