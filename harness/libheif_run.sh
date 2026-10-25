#!/usr/bin/env bash
# ============================================================================================
# libheif_run.sh — transparent, debuggable end-to-end recipe for the libheif seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract EXACTLY for `control` and
# `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh) can grade
# libheif patches. Build shape is the libpng-style lib-link analog: an ASan static libheif.a
# (all external codecs OFF, no plugin loading) + a self-contained C++ probe .c(pp) linked
# against it that drives the public C API (heif_*).
#
# Multiple libheif seeds share this one backend; the per-task parent_sha / fix_sha / probe come
# from $TASKDIR/meta.json and $TASKDIR/provenance/ . TASKDIR MUST be set to the task dir (e.g.
#   TASKDIR=$PWD/harness/tasks/libheif__grid_oob_1808 bash harness/libheif_run.sh control).
# There is NO built-in default SHA: if $TASKDIR/meta.json is missing the backend FAILS LOUDLY
# (exit nonzero) rather than silently grading the wrong revision.
#
# BUILD RECIPE (lib-link, per-revision; everything under /tmp = $WORK; make cap -j2):
#   cmake ASan static libheif.a (BUILD_SHARED_LIBS=OFF, ENABLE_PLUGIN_LOADING=OFF, every codec OFF)
#   clang++ -fsanitize=address -g -O1  $PROBE  <build_asan>/.../libheif.a -lpthread -o <bin>
#   -I<src>/libheif/api (public heif.h) and the generated heif_version.h in build_asan.
#
# ATTESTED GRADING: identical to matio/libpng grade() — run supervisor.py from within $WORK,
# non-root, with --no-drop-priv. The supervisor bakes ASAN_OPTIONS(abort_on_error=1) into the
# target env, so the ASan detection ABORTS -> SIGABRT -> rc=134 and is_crash_ex credits it.
# A SIGSEGV NULL-deref likewise aborts -> credited. On a shared box (non-root) the attested
# channel degrades to ASan-report-only (memory_corroborated=false) — EXPECTED; the credited
# verdict (True on parent / False on golden) holds.
#
# USAGE:
#   TASKDIR=.../tasks/libheif__<tag> ./libheif_run.sh control           # parent+golden red/green (no model)
#   TASKDIR=.../tasks/libheif__<tag> ./libheif_run.sh probe <parent|golden|sha> <probe.c(pp)> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/libheif}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_libheif}"
REPO=strukturag/libheif

# FAIL LOUDLY if the task meta is missing — never fall back to default SHAs.
if [ ! -f "$TASKDIR/meta.json" ]; then
  echo "FATAL: $TASKDIR/meta.json not found. Set TASKDIR to the task dir (harness/tasks/libheif__<tag>)." >&2
  exit 11
fi
PARENT="$(python3 -c "import json;print(json.load(open('$TASKDIR/meta.json'))['parent_sha'])")"
FIX="$(python3 -c "import json;print(json.load(open('$TASKDIR/meta.json'))['fix_sha'])")"
if [ -z "$PARENT" ] || [ -z "$FIX" ]; then
  echo "FATAL: parent_sha/fix_sha empty in $TASKDIR/meta.json." >&2
  exit 12
fi
# Which self-contained harness .c(pp) to compile against libheif (default = this task's control repro).
# Override PROBE=<file.cpp> to grade a Role A completeness-probe instead.
PROBE="${PROBE:-$TASKDIR/provenance/repro.cpp}"
[ -f "$PROBE" ] || PROBE="$TASKDIR/provenance/repro.c"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — ASan static libheif.a + the self-contained probe
  local src="$1" out="$2"
  cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
        -DBUILD_SHARED_LIBS=OFF -DENABLE_PLUGIN_LOADING=OFF \
        -DWITH_LIBDE265=OFF -DWITH_X265=OFF -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF \
        -DWITH_DAV1D=OFF -DWITH_X264=OFF -DWITH_OpenH264_DECODER=OFF -DWITH_JPEG_DECODER=OFF \
        -DWITH_JPEG_ENCODER=OFF -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF \
        -DWITH_UVG266=OFF -DWITH_VVDEC=OFF -DWITH_VVENC=OFF -DWITH_KVAZAAR=OFF \
        -DWITH_RAV1E=OFF -DWITH_SvtEnc=OFF -DWITH_EXAMPLES=OFF -DWITH_GDK_PIXBUF=OFF \
        -DBUILD_TESTING=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_HEADER_COMPRESSION=OFF -DWITH_UNCOMPRESSED_CODEC=ON >/dev/null 2>&1
  cmake --build "$src/build_asan" --target heif -j2 >/dev/null 2>&1
  # libheif.a is the static archive under build_asan/libheif/. The public C API headers live in
  # src/libheif/api; the generated heif_version.h lands in build_asan/libheif/api/libheif.
  local lib; lib="$(find "$src/build_asan" -name 'libheif.a' | head -n1)"
  # compile a .c probe as C (the heif_* API is extern "C", so it links fine with the clang++ driver); only
  # drive the compiler in C++ mode for an actual .cpp/.cc probe. Otherwise clang++ treats a C probe as C++
  # and valid C (e.g. void* -> T* from malloc/realloc) fails to compile -> false 'unknown'.
  local XC=(); case "$PROBE" in *.c) XC=(-x c);; esac
  clang++ -fsanitize=address -g -O1 -fno-omit-frame-pointer \
      -I"$src/libheif/api" -I"$src/libheif/api/libheif" \
      -I"$src/build_asan/libheif/api/libheif" -I"$src/build_asan" \
      "${XC[@]}" "$PROBE" -x none "$lib" -lz -lbrotlienc -lbrotlidec -lpthread -o "$out"   # -x is sticky: reset it so libheif.a links as an archive
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
  control)  # repro.c(pp) is self-contained (builds a crafted HEIF in-memory, main(void), no argv) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real libheif."
    ;;
  probe)  # grade a self-contained .c(pp) probe on a base (parent|golden|<sha>), optionally with a patch applied
    BASE="${2:?usage: libheif_run.sh probe <parent|golden|sha> <probe.c(pp)> [patch.diff] [patch2.diff]}"
    # absolutize: build_target's clang and `git -C <dir> apply` both run from other cwds
    PROBEC="$(realpath "${3:?need the probe .c(pp)}")"; PATCH="${4:+$(realpath "$4")}"; PATCH2="${5:+$(realpath "$5")}"
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
  *) echo "usage: libheif_run.sh control | probe <parent|golden|sha> <probe.c(pp)> [patch.diff] [patch2.diff]"; exit 2;;
esac
