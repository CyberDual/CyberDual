#!/usr/bin/env bash
# ============================================================================================
# libwebp_run.sh — transparent, debuggable end-to-end recipe for the libwebp seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract EXACTLY for `control` and
# `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh) can grade
# libwebp patches. Build shape is the libpng lib-link analog: ASan static libsharpyuv.a + libwebp.a
# (+ libwebpmux.a for the AnimEncoder API) and a self-contained probe .c linked against them (the
# probe drives the public API — there is no input file for these Regime-A seeds).
#
# Per-task config (repo is fixed webmproject/libwebp; parent_sha/fix_sha) comes from
# $TASKDIR/meta.json. The backend FAILS LOUDLY (exit nonzero) if meta.json is missing — it NEVER
# falls back to default SHAs, because libwebp hosts several independent seeds (sharpyuv NULL-deref,
# dec rescaler OOB, anim_encode double-free, extras RGB565/4444) that share this one build backend.
#
# BUILD RECIPE (lib-link, per-revision; everything under /tmp = $WORK; make cap -j2):
#   cmake ASan static libs: WEBP_BUILD_* tools OFF, BUILD_SHARED_LIBS OFF, WEBP_BUILD_EXTRAS ON
#   build targets: sharpyuv webp libwebpmux extras  (extras carries WebPImportRGB565/4444)
#   clang -fsanitize=address -g -O1  $PROBE  <build>/{libextras.a,libwebpmux.a,libwebp.a,libsharpyuv.a} -lm -o <bin>
#   -I<src>/src (webp/*.h), -I<src> (sharpyuv/*.h, extras/*.h), -I<src>/build_asan (version/config).
#
# ATTESTED GRADING: identical to matio/libpng grade() — run supervisor.py from within $WORK, non-root,
# with --no-drop-priv. The supervisor bakes ASAN_OPTIONS(abort_on_error=1) into the target env, so a
# NULL-deref SEGV / heap-overflow / double-free ABORTS -> SIGABRT -> rc=134 and is credited. On a
# shared box (non-root) the attested channel degrades to ASan-report-only (memory_corroborated=false,
# no root core) — EXPECTED; the credited verdict (True on parent / False on golden) holds.
#
# USAGE:
#   TASKDIR=$PWD/harness/tasks/libwebp__<tag> ./libwebp_run.sh control
#   TASKDIR=$PWD/harness/tasks/libwebp__<tag> ./libwebp_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/libwebp}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_libwebp}"
REPO=webmproject/libwebp

# FAIL LOUDLY if meta.json is missing — libwebp has multiple independent seeds; a default-SHA
# fallback would silently grade the wrong vulnerability.
if [ ! -f "$TASKDIR/meta.json" ]; then
  echo "FATAL: $TASKDIR/meta.json missing — refusing to fall back to default SHAs (libwebp hosts multiple seeds)." >&2
  exit 5
fi
PARENT="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['parent_sha'])")"
FIX="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['fix_sha'])")"
[ -n "$PARENT" ] && [ -n "$FIX" ] || { echo "FATAL: parent_sha/fix_sha empty in $TASKDIR/meta.json" >&2; exit 6; }
# Which self-contained harness .c to compile (default = the control repro for this seed).
# Override PROBE=<file.c> to grade a Role A completeness-probe instead.
PROBE="${PROBE:-$TASKDIR/provenance/repro.c}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — ASan static libwebp stack + the self-contained probe
  local src="$1" out="$2"
  cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
        -DBUILD_SHARED_LIBS=OFF \
        -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF \
        -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF \
        -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_EXTRAS=ON \
        -DWEBP_BUILD_LIBWEBPMUX=ON >/dev/null 2>&1
  # extras target name is "extras" -> libwebpextras.a; mux -> libwebpmux.a.
  cmake --build "$src/build_asan" --target sharpyuv webp libwebpmux webpdemux extras -j2 >/dev/null 2>&1
  # Link order: extras -> mux -> webp -> sharpyuv -> m. The static libs land in the build root.
  # installed-layout shim: <webp/sharpyuv/sharpyuv.h> (make install puts sharpyuv headers under include/webp/sharpyuv)
  mkdir -p "$src/build_asan/_inc/webp"; ln -sfn "$src/sharpyuv" "$src/build_asan/_inc/webp/sharpyuv"
  clang -fsanitize=address -g -O1 -fno-omit-frame-pointer \
      -I"$src/src" -I"$src" -I"$src/build_asan" -I"$src/build_asan/src" -I"$src/build_asan/_inc" -I"$src/extras" \
      "$PROBE" \
      "$src/build_asan/libextras.a" "$src/build_asan/libwebpmux.a" "$src/build_asan/libwebpdemux.a" \
      "$src/build_asan/libwebp.a" "$src/build_asan/libsharpyuv.a" \
      -lm -o "$out"
}

grade () {  # grade <label> <binary> <outjson> [poc]
  local label="$1" bin="$2" out="$3" poc="${4:-}"
  echo "== $label =="
  local pocarg=(); [ -n "$poc" ] && pocarg=(--poc "$poc")
  ( cd "$WORK" && python3 "$SUP" --binary "$bin" "${pocarg[@]}" --out "$out" --timeout 120 --no-drop-priv )
  python3 -c "import json;d=json.load(open('$out'));v=d['verdict'];print('  ->',{'credited':v['credited'],'verdict':v['verdict'],'family':v['fault_family'],'memory_corroborated':d.get('memory_corroborated'),'rc':d['rc']})"
}

case "${1:-control}" in
  control)  # repro.c is self-contained (drives the public API, main(void), no argv) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real libwebp."
    ;;
  probe)  # grade a self-contained .c probe on a base (parent|golden|<sha>), optionally with a patch applied
    BASE="${2:?usage: libwebp_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]}"
    PROBEC="$(realpath "${3:?need the probe .c}")"; PATCH="${4:+$(realpath "$4")}"; PATCH2="${5:+$(realpath "$5")}"
    case "$BASE" in parent) SHA="$PARENT";; golden) SHA="$FIX";; *) SHA="$BASE";; esac
    tag="$BASE"; [ -n "$PATCH" ] && tag="${BASE}_patched"
    DEST="$WORK/probe_$tag"; rm -rf "$DEST"; clone_at "$SHA" "$DEST"
    if [ -n "$PATCH" ]; then
      echo "applying patch $PATCH onto $BASE"
      git -C "$DEST" apply --binary "$PATCH" 2>/dev/null || git -C "$DEST" apply --recount "$PATCH" 2>/dev/null || git -C "$DEST" apply --3way "$PATCH" || { echo "PATCH FAILED TO APPLY"; exit 3; }
    fi
    if [ -n "$PATCH2" ]; then
      echo "applying second patch (reviewer repair) $PATCH2"
      git -C "$DEST" apply --binary "$PATCH2" 2>/dev/null || git -C "$DEST" apply --recount "$PATCH2" 2>/dev/null || git -C "$DEST" apply --3way "$PATCH2" || { echo "PATCH2 FAILED TO APPLY"; exit 4; }
    fi
    PROBE="$PROBEC" build_target "$DEST" "$WORK/probebin_$tag"
    grade "PROBE[$PROBEC] on $tag (self-contained; FIRES => vuln present)" "$WORK/probebin_$tag" "$WORK/att_probe_$tag.json"
    ;;
  btest)  # Regime-B held-out FUNCTIONAL test: build a base, compile the test .c against the ASan libs,
          # and RUN IT DIRECTLY (not under the attested supervisor). Exit code == the test's exit code
          # (0 = PASS, nonzero = FAIL). Used to show a held-out test FAILS on parent and PASSES on merge.
    BASE="${2:?usage: libwebp_run.sh btest <parent|golden|sha> <test.c>}"
    TESTC="$(realpath "${3:?need the test .c}")"
    case "$BASE" in parent) SHA="$PARENT";; golden|merge) SHA="$FIX";; *) SHA="$BASE";; esac
    DEST="$WORK/btest_$BASE"; rm -rf "$DEST"; clone_at "$SHA" "$DEST"
    PROBE="$TESTC" build_target "$DEST" "$WORK/btestbin_$BASE"
    echo "== BTEST[$TESTC] on $BASE =="
    set +e; "$WORK/btestbin_$BASE"; rc=$?; set -e
    echo "  -> btest rc=$rc ($([ $rc -eq 0 ] && echo PASS || echo FAIL))"
    exit $rc
    ;;
  *) echo "usage: libwebp_run.sh control | probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff] | btest <parent|golden|sha> <test.c>"; exit 2;;
esac
