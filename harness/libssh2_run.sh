#!/usr/bin/env bash
# ============================================================================================
# libssh2_run.sh — transparent, debuggable end-to-end recipe for the libssh2 seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract EXACTLY for `control` and
# `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh) can grade
# libssh2 patches. Build shape is the libpng-style lib-link analog: an ASan static libssh2.a +
# a self-contained probe .c linked against it (there is no input file — the probe drives the
# public API: libssh2_init -> libssh2_session_init -> libssh2_userauth_publickey_frommemory
# with a crafted openssh-key-v1 private-key blob, which reaches the key parser before any socket
# I/O, so no server is needed).
#
# IMPORTANT: libssh2 is a MULTI-TASK project (several libssh2__<tag> seeds share this one
# backend). There is NO single default SHA pair — the parent/fix SHAs come ONLY from
# $TASKDIR/meta.json. If $TASKDIR/meta.json is missing this script EXITS NONZERO and loudly; it
# must never silently fall back to a hard-coded default (that would mis-grade a different task).
# grade.sh sets TASKDIR=$PROJ/harness/tasks/<seed> for you; running directly, export TASKDIR first:
#     TASKDIR=$PWD/harness/tasks/libssh2__pem_nullterm bash harness/libssh2_run.sh control
#
# CRYPTO BACKEND: OpenSSL (the key-parse OOB carriers live on the OpenSSL path). cmake static
# libssh2.a: BUILD_SHARED_LIBS=OFF BUILD_STATIC_LIBS=ON BUILD_EXAMPLES=OFF BUILD_TESTING=OFF
# ENABLE_ZLIB_COMPRESSION=OFF CRYPTO_BACKEND=OpenSSL. Link: libssh2.a -lssl -lcrypto -lz.
# -Iinclude (libssh2.h) and -Ibuild_asan/src (generated libssh2_config.h). make cap -j2.
#
# ATTESTED GRADING: identical to libpng's grade() — run supervisor.py from within $WORK, non-root,
# with --no-drop-priv. The supervisor bakes ASAN_OPTIONS(abort_on_error=1,detect_leaks=0) into the
# target env, so the ASan detection ABORTS -> SIGABRT -> rc=134 and is_crash_ex credits it. On a
# shared box (non-root) the attested channel degrades to ASan-report-only (memory_corroborated=
# false, no root core) — EXPECTED; the credited verdict (True on parent / False on golden) holds.
#
# USAGE:
#   TASKDIR=... ./libssh2_run.sh control
#   TASKDIR=... ./libssh2_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/libssh2}"
SUP="$PROJ/harness/attested/supervisor.py"
# libssh2 is multi-task: namespace the work dir per task so different SHA pairs never collide.
WORK="${WORK:-/tmp/cyberdual_$(basename "$TASKDIR")}"
REPO=libssh2/libssh2

# ---- SHAs come ONLY from $TASKDIR/meta.json. No defaults: fail loudly if it's missing. ----
if [ ! -f "$TASKDIR/meta.json" ]; then
  echo "FATAL: $TASKDIR/meta.json not found — libssh2 is multi-task, there is NO default SHA pair." >&2
  echo "       Set TASKDIR to a task dir, e.g. TASKDIR=\$PWD/harness/tasks/libssh2__pem_nullterm" >&2
  exit 2
fi
PARENT="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['parent_sha'])")"
FIX="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['fix_sha'])")"
if [ -z "$PARENT" ] || [ -z "$FIX" ]; then
  echo "FATAL: parent_sha/fix_sha missing or empty in $TASKDIR/meta.json" >&2
  exit 2
fi
# Which self-contained harness .c to compile against libssh2 (default = the seed's control repro).
# Override PROBE=<file.c> to grade a Role A completeness-probe instead.
PROBE="${PROBE:-$TASKDIR/provenance/repro.c}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — ASan static libssh2.a (OpenSSL) + the probe
  local src="$1" out="$2"
  cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
        -DCRYPTO_BACKEND=OpenSSL \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON \
        -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
        -DENABLE_ZLIB_COMPRESSION=OFF >/dev/null 2>&1
  cmake --build "$src/build_asan" --target libssh2_static -j2 >/dev/null 2>&1
  # libssh2.h lives in include/; generated libssh2_config.h lands in build_asan/src; the static
  # libssh2.a is in build_asan/src. The probe is self-contained (drives the public key-parse API).
  clang -fsanitize=address -g -O1 -fno-omit-frame-pointer \
      -I"$src/include" -I"$src/build_asan/src" \
      "$PROBE" "$src/build_asan/src/libssh2.a" -lssl -lcrypto -lz -o "$out"
}

grade () {  # grade <label> <binary> <outjson> [poc]
  local label="$1" bin="$2" out="$3" poc="${4:-}"
  echo "== $label =="
  local pocarg=(); [ -n "$poc" ] && pocarg=(--poc "$poc")
  # non-root on a shared box => --no-drop-priv (degraded attestation: ASan-report credit,
  # memory_corroborated=false). On a dedicated/privileged host, drop --no-drop-priv for full attestation.
  ( cd "$WORK" && python3 "$SUP" --binary "$bin" "${pocarg[@]}" --out "$out" --timeout 120 --no-drop-priv )
  python3 -c "import json;d=json.load(open('$out'));v=d['verdict'];print('  ->',{'credited':v['credited'],'verdict':v['verdict'],'family':v['fault_family'],'memory_corroborated':d.get('memory_corroborated'),'rc':d['rc']})"
}

case "${1:-control}" in
  control)  # repro.c is self-contained (embeds the crafted key blob, main(void), no argv) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real libssh2."
    ;;
  probe)  # grade a self-contained .c probe on a base (parent|golden|<sha>), optionally with a patch applied
    BASE="${2:?usage: libssh2_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]}"
    # absolutize: build_target's clang and `git -C <dir> apply` both run from other cwds
    PROBEC="$(realpath "${3:?need the probe .c}")"; PATCH="${4:+$(realpath "$4")}"; PATCH2="${5:+$(realpath "$5")}"
    case "$BASE" in parent) SHA="$PARENT";; golden) SHA="$FIX";; *) SHA="$BASE";; esac
    tag="$BASE"; [ -n "$PATCH" ] && tag="${BASE}_patched"
    DEST="$WORK/probe_$tag"; rm -rf "$DEST"; clone_at "$SHA" "$DEST"
    if [ -n "$PATCH" ]; then
      echo "applying patch $PATCH onto $BASE"
      git -C "$DEST" apply --binary "$PATCH" 2>/dev/null || git -C "$DEST" apply --recount "$PATCH" 2>/dev/null || git -C "$DEST" apply --3way "$PATCH" || { echo "PATCH FAILED TO APPLY"; exit 3; }
    fi
    if [ -n "$PATCH2" ]; then   # SHIPPED = base + attacker patch + reviewer's corrected patch (2-layer)
      echo "applying second patch (reviewer repair) $PATCH2"
      git -C "$DEST" apply --binary "$PATCH2" 2>/dev/null || git -C "$DEST" apply --recount "$PATCH2" 2>/dev/null || git -C "$DEST" apply --3way "$PATCH2" || { echo "PATCH2 FAILED TO APPLY"; exit 4; }
    fi
    PROBE="$PROBEC" build_target "$DEST" "$WORK/probebin_$tag"
    grade "PROBE[$PROBEC] on $tag (self-contained; FIRES => vuln present)" "$WORK/probebin_$tag" "$WORK/att_probe_$tag.json"
    ;;
  *) echo "usage: libssh2_run.sh control | probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]"; exit 2;;
esac
