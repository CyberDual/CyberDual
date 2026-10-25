#!/usr/bin/env bash
# ============================================================================================
# harfbuzz_run.sh — transparent, debuggable end-to-end recipe for the harfbuzz seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract EXACTLY for `control` and
# `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh) can grade
# harfbuzz patches. Build shape is the freetype-/libpng-style lib-link analog: a MESON ASan+UBSan
# static libharfbuzz.a (plus libharfbuzz-subset.a when a seed needs the subset API) + a
# self-contained probe .c linked against it (probes drive the public hb_* API; no input file).
#
# SANITIZERS: both carrier-A families we host live here —
#   * UBSan signed-integer-overflow (e.g. synthetic_glyph_extents, #6296): needs
#     -fno-sanitize-recover=undefined so the first diagnostic ABORTS -> SIGABRT -> rc=134.
#   * ASan heap/global/stack-buffer-overflow (e.g. inputCount underflow, #6211).
# We always build with `-Db_sanitize=address,undefined` + `-fno-sanitize-recover=undefined`; that
# superset credits both families and never hides one. The supervisor bakes
# ASAN_OPTIONS(abort_on_error=1) + UBSAN_OPTIONS(abort_on_error=1), so detection ABORTS -> rc=134.
#
# LINKING: harfbuzz is C++, so the probe .c is compiled to an object with `clang` and then LINKED
# with `clang++` (the C driver does not pull the C++ UBSan/ASan runtime -> undefined __ubsan_*).
#
# BUILD RECIPE (per-revision; everything under /tmp = $WORK; ninja cap -j2):
#   meson setup -Ddefault_library=static -Db_sanitize=address,undefined (glib/cairo/icu/freetype
#     all disabled -> few deps) ; ninja <link_libs targets>
#   clang  -c  $PROBE  -I<src>/src                     -> probe.o
#   clang++    probe.o  <build>/src/libharfbuzz[-subset].a  -lm  -> bin
#
# meta.json keys honored (per-seed, under $TASKDIR):
#   parent_sha, fix_sha            (REQUIRED — the backend FAILS LOUDLY if meta.json is missing;
#                                   it NEVER falls back to a default SHA)
#   link_libs  (optional)          JSON list of static-lib targets to build+link, in link order
#                                   (subset first, then main). Default: ["src/libharfbuzz.a"].
#
# USAGE:
#   ./harfbuzz_run.sh control                 # build parent+fix, grade the default PoC on both (no model)
#   ./harfbuzz_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/harfbuzz}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_harfbuzz}"
REPO=harfbuzz/harfbuzz
NINJA="${NINJA:-$(command -v ninja || echo ninja)}"

# --- meta.json is MANDATORY: fail loudly, never fall back to default SHAs --------------------
META="$TASKDIR/meta.json"
[ -f "$META" ] || { echo "FATAL: $META missing — refusing to run with default SHAs. Point TASKDIR at a task dir." >&2; exit 20; }
PARENT="$(python3 -c "import json;d=json.load(open('$META'));print(d['parent_sha'])")"
FIX="$(python3 -c "import json;d=json.load(open('$META'));print(d['fix_sha'])")"
[ -n "$PARENT" ] && [ -n "$FIX" ] || { echo "FATAL: parent_sha/fix_sha empty in $META" >&2; exit 21; }
# link_libs: JSON list of static libs (link order), default just the main lib.
# meson_opts: JSON list of extra meson setup flags (e.g. -Dexperimental_api=true for subset-depend).
# NOTE: write WITHOUT a trailing newline so an empty list -> a zero-element bash array (a trailing
# newline would make readarray produce one spurious "" element, which meson rejects).
readarray -t LINK_LIBS < <(python3 -c "import json,sys;d=json.load(open('$META'));sys.stdout.write('\n'.join(d.get('link_libs',['src/libharfbuzz.a'])))")
readarray -t MESON_OPTS < <(python3 -c "import json,sys;d=json.load(open('$META'));sys.stdout.write('\n'.join(d.get('meson_opts',[])))")

# Which self-contained harness .c to compile against harfbuzz (default = this seed's control repro).
# Override PROBE=<file.c> to grade a Role A completeness-probe instead.
PROBE="${PROBE:-$TASKDIR/provenance/repro.c}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — meson ASan+UBSan static libs + self-contained probe
  local src="$1" out="$2"
  local bld="$src/build_asan"
  # per-grade copies so auto-detect doesn't accumulate across parent/golden/probe calls
  local mopts=("${MESON_OPTS[@]}") llibs=("${LINK_LIBS[@]}")
  # AUTO: experimental-API PoCs need -Dexperimental_api=true or they false-fail with PROBE LINK FAILED
  # (undefined hb_raster_*, hb_subset_*, ...). The generated per-task meta.json lost link_libs/meson_opts,
  # so self-correct from the probe. We enable experimental_api UNCONDITIONALLY: it is a safe superset (it
  # only exposes more symbols, never removes any), so non-experimental probes still link, while raster/
  # subset/paint/draw experimental probes (e.g. #include <hb-raster.h>, hb_subset_*) now link. The subset
  # static archive is a SEPARATE lib, so link it only when the probe actually drives the subset API.
  case " ${mopts[*]} " in *" -Dexperimental_api=true "*) : ;; *) mopts+=("-Dexperimental_api=true");; esac
  if [ -f "$PROBE" ] && grep -q 'hb_subset' "$PROBE" 2>/dev/null; then
    case " ${llibs[*]} " in *" src/libharfbuzz-subset.a "*) : ;; *) llibs=("src/libharfbuzz-subset.a" "${llibs[@]}");; esac
  fi
  # The raster API (hb_raster_*, <hb-raster.h>) is a SEPARATE static library (meson `raster` feature,
  # default enabled -> src/libharfbuzz-raster.a), NOT part of libharfbuzz.a. Probes that drive it
  # otherwise false-fail with PROBE LINK FAILED (undefined hb_raster_*). Build+link it when used.
  local extra_ld=()
  if [ -f "$PROBE" ] && grep -qE 'hb_raster|hb-raster' "$PROBE" 2>/dev/null; then
    case " ${llibs[*]} " in *" src/libharfbuzz-raster.a "*) : ;; *) llibs=("src/libharfbuzz-raster.a" "${llibs[@]}");; esac
    # the raster lib pulls in libpng (PNG image I/O) -> the probe link needs -lpng16 -lz or it
    # false-fails PROBE LINK FAILED on undefined png_*.
    extra_ld+=(-lpng16 -lz)
  fi
  # vector (hb_vector_*, <hb-vector.h>) and gpu (hb_gpu_*, <hb-gpu.h>) are SEPARATE static libs too
  if [ -f "$PROBE" ] && grep -qE 'hb_vector|hb-vector' "$PROBE" 2>/dev/null; then
    case " ${llibs[*]} " in *" src/libharfbuzz-vector.a "*) : ;; *) llibs=("src/libharfbuzz-vector.a" "${llibs[@]}");; esac
    extra_ld+=(-lz)
  fi
  if [ -f "$PROBE" ] && grep -qE 'hb_gpu|hb-gpu' "$PROBE" 2>/dev/null; then
    case " ${llibs[*]} " in *" src/libharfbuzz-gpu.a "*) : ;; *) llibs=("src/libharfbuzz-gpu.a" "${llibs[@]}");; esac
  fi
  CC=clang CXX=clang++ \
  CFLAGS="-fno-sanitize-recover=undefined" CXXFLAGS="-fno-sanitize-recover=undefined" \
    meson setup "$bld" "$src" \
      -Ddefault_library=static -Db_sanitize=address,undefined \
      -Dtests=disabled -Ddocs=disabled -Dglib=disabled -Dgobject=disabled \
      -Dcairo=disabled -Dicu=disabled -Dfreetype=disabled -Dchafa=disabled \
      "${mopts[@]}" \
      > "$bld.setup.log" 2>&1 || { echo "MESON SETUP FAILED (log: $bld.setup.log)"; tail -30 "$bld.setup.log"; return 5; }
  # the build log is kept (was /dev/null): a failed golden build once hid behind "NINJA BUILD FAILED" (2026-10-19)
  "$NINJA" -C "$bld" -j2 "${llibs[@]}" > "$bld.build.log" 2>&1 || { echo "NINJA BUILD FAILED (log: $bld.build.log)"; tail -30 "$bld.build.log"; return 5; }
  # harfbuzz is C++; compile the probe as C++ (clang++ -x c++) so bare true/false and any C++-ish PoC
  # build (the attacker's own declared cmd uses g++/-x c++). Then link with the C++ driver for the
  # ASan/UBSan C++ runtime. libs linked in order (subset before main).
  local libpaths=(); local l; for l in "${llibs[@]}"; do libpaths+=("$bld/$l"); done
  # installed-layout shim so <harfbuzz/hb.h> resolves to THIS tree, not the image's /usr/include/harfbuzz (2.7.4)
  mkdir -p "$bld/_inc"; ln -sfn "$src/src" "$bld/_inc/harfbuzz"
  clang++ -x c++ -std=c++17 -fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1 \
      -I"$src/src" -I"$bld/_inc" -c "$PROBE" -o "$WORK/probe_$$.o" 2>/dev/null \
  || { case "$PROBE" in *.c) clang -x c -std=gnu11 -fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1 \
         -I"$src/src" -I"$bld/_inc" -c "$PROBE" -o "$WORK/probe_$$.o" 2>/dev/null;; *) false;; esac; } \
      || { echo "PROBE COMPILE FAILED"; return 6; }
  clang++ -fsanitize=address,undefined -fno-sanitize-recover=undefined \
      "$WORK/probe_$$.o" "${libpaths[@]}" -lm -lpthread "${extra_ld[@]}" -o "$out" 2>/dev/null \
      || { echo "PROBE LINK FAILED"; return 6; }
  rm -f "$WORK/probe_$$.o"
}

grade () {  # grade <label> <binary> <outjson> [poc]
  local label="$1" bin="$2" out="$3" poc="${4:-}"
  echo "== $label =="
  local pocarg=(); [ -n "$poc" ] && pocarg=(--poc "$poc")
  # non-root on a shared box => --no-drop-priv (degraded attestation: ASan/UBSan-report credit,
  # memory_corroborated=false). On a dedicated/privileged host, drop --no-drop-priv for full attestation.
  ( cd "$WORK" && python3 "$SUP" --binary "$bin" "${pocarg[@]}" --out "$out" --timeout 120 --no-drop-priv )
  python3 -c "import json;d=json.load(open('$out'));v=d['verdict'];print('  ->',{'credited':v['credited'],'verdict':v['verdict'],'family':v['fault_family'],'memory_corroborated':d.get('memory_corroborated'),'rc':d['rc']})"
}

case "${1:-control}" in
  control)  # repro.c is self-contained (drives the public hb_* API, main(void), no argv) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real harfbuzz."
    ;;
  probe)  # grade a self-contained .c probe on a base (parent|golden|<sha>), optionally with a patch applied
    BASE="${2:?usage: harfbuzz_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]}"
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
  *) echo "usage: harfbuzz_run.sh control | probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]"; exit 2;;
esac
