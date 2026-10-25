#!/usr/bin/env bash
# harness/attested/entrypoint.sh — container entrypoint for the attested crash channel.
#
# Runs AS ROOT inside the container, prepares the unforgeable substrate, then hands off to
# the root supervisor. Forwards all args to supervisor.py (see its --help).
#
#   docker run --rm --privileged --pid=host \
#       -v /opt/attested:/opt/attested:ro -v "$PWD/work":/work \
#       attested-runner --binary /work/target --poc /work/poc.bin --out /work/result.json
#
# WHY --privileged --pid=host: a piped /proc/sys/kernel/core_pattern helper is executed by the
# kernel in the INITIAL namespace, so core_helper.py must be reachable at that namespace's path
# and core_pattern (a non-namespaced sysctl) must be writable. On a dedicated host you can run
# supervisor.py directly as root instead of in a container.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_UID="${ATTESTED_TARGET_UID:-4242}"
TARGET_GID="${ATTESTED_TARGET_GID:-$TARGET_UID}"
CORES_DIR="${ATTESTED_CORES_DIR:-/var/attested/cores}"

if [ "$(id -u)" -ne 0 ]; then
    echo "entrypoint: must run as root (needs core_pattern + setuid)" >&2
    exit 2
fi

# 1) low-priv target account the source-authoring adversary runs as (no shell, no home write).
if ! getent group "$TARGET_GID" >/dev/null 2>&1; then
    groupadd -g "$TARGET_GID" attested_target 2>/dev/null || true
fi
if ! getent passwd "$TARGET_UID" >/dev/null 2>&1; then
    useradd -u "$TARGET_UID" -g "$TARGET_GID" -M -N -s /usr/sbin/nologin attested_target 2>/dev/null || true
fi

# 2) root-only cores dir the target uid cannot enter/read/rewind/unlink.
mkdir -p "$CORES_DIR"
chown 0:0 "$CORES_DIR"
chmod 700 "$CORES_DIR"

# 3) the root core-dump helper must be executable at the initial-namespace path.
CORE_HELPER="${ATTESTED_CORE_HELPER:-/opt/attested/core_helper.py}"
if [ ! -x "$CORE_HELPER" ]; then
    # fall back to the in-image copy alongside this script
    if [ -f "$HERE/core_helper.py" ]; then
        CORE_HELPER="$HERE/core_helper.py"
        chmod 0755 "$CORE_HELPER" || true
    else
        echo "entrypoint: core helper not found at $CORE_HELPER" >&2
        exit 2
    fi
fi
export ATTESTED_CORE_HELPER="$CORE_HELPER"
export ATTESTED_CORES_DIR="$CORES_DIR"
export ATTESTED_TARGET_UID="$TARGET_UID"
export ATTESTED_TARGET_GID="$TARGET_GID"

# 4) allow cores to be produced at all (belt-and-suspenders; supervisor also sets RLIMIT_CORE).
ulimit -c unlimited || true

exec python3 "$HERE/supervisor.py" "$@"
