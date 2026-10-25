#!/usr/bin/env bash
# ============================================================================================
# libarchive_run.sh — transparent, debuggable end-to-end recipe for the libarchive seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract EXACTLY for `control` and
# `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh) can grade
# libarchive patches. Build shape is the freetype/libpng-style lib-link analog: an ASan static
# libarchive.a + a self-contained probe .c linked against it (the probe drives the public API;
# carrier-A repros embed their own fixture and write temp inputs themselves).
#
# This backend serves BOTH carrier types in the libarchive seed family:
#   A (security/crash-fix): provenance/repro.c is a self-contained public-API PoC that crashes
#     (ASan abort, rc=134) on the parent and is clean (rc=0) on the fix. `control` grades it on both.
#   B (merged feature PR): provenance/repro.c is a trivial no-crash probe (links against the lib at
#     the parent so `probe` mode is well-formed for grading planted new_vuln PoCs later). The
#     held-out feature test lives at provenance/heldout_verify.c and is run out-of-band (see the
#     seed meta.json `test_cmd`), not by this backend's control/probe modes.
#
# Per-seed SHAs come from $TASKDIR/meta.json (parent_sha / fix_sha). FAIL LOUDLY if it is missing —
# NEVER fall back to a default SHA (a wrong default would silently mis-grade the wrong revision).
#
# BUILD RECIPE (lib-link, per-revision; everything under /tmp = $WORK; make cap -j2):
#   CC=clang cmake ASan static libarchive.a (BUILD_SHARED_LIBS=OFF, ENABLE_WERROR=OFF, ENABLE_TEST=OFF;
#     ZLIB/LZMA/BZip2 ON so the zip/7zip formats work; CLI tools off). Target: archive_static.
#   clang -fsanitize=address -g -O1  $PROBE  <build_asan>/libarchive/libarchive.a -llzma -lbz2 -lz -lmd -o <bin>
#     (-lmd: libarchive's cmake auto-detects libmd on this host -> ARCHIVE_CRYPTO_*_LIBMD=TRUE, so
#      archive_digest.c.o references MD5Init/SHA1Init/RMD160*/SHA256*/SHA512* from libmd; a probe that
#      pulls in a digest-using reader/writer (e.g. gnutar writer) needs -lmd or the link fails.)
#   -I<src>/libarchive (archive.h / archive_entry.h).
#
# ATTESTED GRADING: identical to matio/libpng — run supervisor.py from within $WORK, non-root, with
# --no-drop-priv. The supervisor bakes ASAN_OPTIONS(abort_on_error=1,detect_leaks=0) into the target
# env, so an ASan error ABORTS -> SIGABRT -> rc=134 and is credited. On a shared box (non-root) the
# attested channel degrades to ASan-report-only (memory_corroborated=false) — EXPECTED; the credited
# verdict (True on parent / False on golden) holds.
#
# USAGE:
#   TASKDIR=$PWD/harness/tasks/libarchive__<tag> ./libarchive_run.sh control
#   TASKDIR=$PWD/harness/tasks/libarchive__<tag> ./libarchive_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/libarchive}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_libarchive}"
REPO=libarchive/libarchive

# FAIL LOUDLY if the per-seed meta.json is absent — never fall back to a default SHA.
if [ ! -f "$TASKDIR/meta.json" ]; then
  echo "FATAL: $TASKDIR/meta.json missing — refusing to run with default SHAs. Set TASKDIR to a seed dir." >&2
  exit 2
fi
PARENT="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['parent_sha'])")"
FIX="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['fix_sha'])")"
if [ -z "$PARENT" ] || [ -z "$FIX" ]; then
  echo "FATAL: parent_sha/fix_sha empty in $TASKDIR/meta.json" >&2
  exit 2
fi
# Which self-contained harness .c to compile against libarchive (default = the seed's control repro).
# Override PROBE=<file.c> to grade a Role A completeness-probe / planted new_vuln PoC instead.
PROBE="${PROBE:-$TASKDIR/provenance/repro.c}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — ASan static libarchive.a + the self-contained probe
  local src="$1" out="$2"
  CC=clang cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=clang \
        -DENABLE_WERROR=OFF \
        -DCMAKE_C_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
        -DBUILD_SHARED_LIBS=OFF -DENABLE_TEST=OFF \
        -DENABLE_OPENSSL=OFF -DENABLE_LIBB2=OFF -DENABLE_LZ4=OFF -DENABLE_LZO=OFF \
        -DENABLE_LZMA=ON -DENABLE_ZSTD=OFF -DENABLE_ZLIB=ON -DENABLE_BZip2=ON \
        -DENABLE_LIBXML2=OFF -DENABLE_EXPAT=OFF -DENABLE_PCREPOSIX=OFF -DENABLE_NETTLE=OFF \
        -DENABLE_ICONV=OFF -DENABLE_ACL=OFF -DENABLE_XATTR=OFF \
        -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_CAT=OFF -DENABLE_UNZIP=OFF >/dev/null 2>&1
  cmake --build "$src/build_asan" --target archive_static -j2 >/dev/null 2>&1
  # archive.h / archive_entry.h live under src/libarchive; the static lib lands at
  # build_asan/libarchive/libarchive.a. repro.c is self-contained (drives the public read API).
  clang -fsanitize=address -g -O1 -fno-omit-frame-pointer \
      -I"$src/libarchive" \
      "$PROBE" "$src/build_asan/libarchive/libarchive.a" -llzma -lbz2 -lz -lmd -o "$out"
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
  control)  # repro.c is self-contained (embeds its fixture, main(void), no argv) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real libarchive."
    ;;
  probe)  # grade a self-contained .c probe on a base (parent|golden|<sha>), optionally with a patch applied
    BASE="${2:?usage: libarchive_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]}"
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
  *) echo "usage: libarchive_run.sh control | probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]"; exit 2;;
esac
