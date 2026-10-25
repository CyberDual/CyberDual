#!/usr/bin/env bash
# ============================================================================================
# openexr_run.sh — transparent, debuggable end-to-end recipe for the OpenEXR seeds.
#
# Mirrors matio_run.sh's / libpng_run.sh's / cjson_run.sh's INTERFACE + JSON contract EXACTLY for
# `control` and `probe`, so the rest of the pipeline (self_validate.sh, run_twopatch.sh, grade.sh)
# can grade OpenEXR patches. Build shape is the libpng-style lib-link analog: ASan(+UBSan) static
# OpenEXR libs (Imath/Iex/IlmThread/OpenEXRCore/OpenEXR/OpenEXRUtil) + a self-contained probe
# compiled against them (A-regime seeds drive the public read/decode API; the probe writes a
# crafted EXR and reads it back, no external input file).
#
# This backend is used for SEVERAL OpenEXR tasks (one task dir per task: harness/tasks/openexr__<tag>).
# TASKDIR must point at the TASK dir so parent_sha/fix_sha/the probe come from THAT task's meta.json.
# There are NO default SHAs: if $TASKDIR/meta.json is missing the backend FAILS LOUDLY (never silently
# grades the wrong revision).
#
# ATTESTED GRADING: identical to matio's/libpng's grade() — run supervisor.py from within $WORK,
# non-root, with --no-drop-priv. The supervisor bakes ASAN_OPTIONS(abort_on_error=1,...) and a
# UBSAN_OPTIONS mirror into the target env, so an ASan heap-overflow OR a UBSan -fno-sanitize-recover
# runtime error ABORTS -> SIGABRT -> rc=134 and is_crash_ex credits it. On a shared box (non-root)
# the attested channel degrades to ASan-report-only (memory_corroborated=false) — EXPECTED; the
# credited verdict (True on parent / False on golden) still holds.
#
# BUILD RECIPE (lib-link, per-revision; everything under /tmp = $WORK; make cap -j2):
#   cmake static OpenEXR (BUILD_SHARED_LIBS=OFF, tools/examples/tests OFF, FORCE_INTERNAL_IMATH=ON)
#   build target OpenEXRUtil (pulls OpenEXR + OpenEXRCore + IlmThread + Iex + internal Imath)
#   clang++ -fsanitize=address,undefined -fno-sanitize-recover=all  $PROBE  <all .a>  -lz -lpthread -o <bin>
#   include dirs: the OpenEXR source include trees + the generated-config dir under build_asan.
#   Imath is fetched by cmake FetchContent at configure time (github); no other network at build.
#
# USAGE:
#   TASKDIR=$PWD/harness/tasks/openexr__<tag> ./openexr_run.sh control
#   TASKDIR=$PWD/harness/tasks/openexr__<tag> ./openexr_run.sh probe <parent|golden|sha> <probe.cpp> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/openexr}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_openexr}"
REPO=AcademySoftwareFoundation/openexr
# OpenJPH vendored-dependency pin. OpenEXR vendors OpenJPH at external/OpenJPH and compiles
# src/lib/OpenEXRCore/internal_ht.cpp (HTJ2K) UNCONDITIONALLY against it. A month that adopts the
# newer OpenJPH 0.3x param API in internal_ht.cpp is normally paired with a bump of the vendored
# copy — but the month-substrate commit selection drops that bump (a huge "generated/vendored"
# commit) while KEEPING its small hand-written API consumer, leaving an un-buildable API skew.
# ensure_openjph() below detects that skew per build and vendors the matching version so the real
# month compiles the way upstream CI does. Override the pin with OJPH_PIN=<tag> if a month needs it.
OJPH_PIN="${OJPH_PIN:-0.32.0}"
OJPH_REPO="${OJPH_REPO:-https://github.com/aous72/OpenJPH}"

ensure_openjph () {  # ensure_openjph <srcdir> — repair vendored external/OpenJPH when internal_ht.cpp needs a newer API than the vendored copy provides
  local src="$1"
  local ht="$src/src/lib/OpenEXRCore/internal_ht.cpp"
  local params="$src/external/OpenJPH/src/core/openjph/ojph_params.h"
  [ -f "$ht" ] || return 0
  # Does the (already-patched) consumer call the multi-arg OpenJPH 0.3x API (is_reversible(idx) /
  # set_reversible(idx,...) / set_qfactor(...))? The 0.26-era base uses only the 0-arg forms.
  grep -qE 'is_reversible *\([A-Za-z_]|set_reversible *\([A-Za-z_][^,]*,|set_qfactor *\(' "$ht" || return 0
  # Does the vendored copy already provide the multi-arg API? (0.32 has is_reversible(ui32 ...))
  [ -f "$params" ] && grep -qE 'is_reversible *\( *ui32' "$params" && return 0
  echo "   [openjph-skew] internal_ht.cpp needs the OpenJPH >=0.32 param API but vendored external/OpenJPH is stale — vendoring $OJPH_PIN"
  local cache="$WORK/_ojph_${OJPH_PIN}"
  if [ ! -f "$cache/src/core/openjph/ojph_version.h" ]; then
    rm -rf "$cache"; git clone --quiet --depth 1 --branch "$OJPH_PIN" "$OJPH_REPO" "$cache" \
      || { echo "   [openjph-skew] FATAL: could not clone OpenJPH $OJPH_PIN" >&2; return 1; }
  fi
  # keep the tree's own copy: a consumer written for it (e.g. version-guarded code) may only build with it
  rm -rf "$src/.ojph_vendored_orig"; mv "$src/external/OpenJPH" "$src/.ojph_vendored_orig" 2>/dev/null || true
  mkdir -p "$src/external/OpenJPH"
  ( cd "$cache" && tar cf - --exclude=.git . ) | ( cd "$src/external/OpenJPH" && tar xf - )
}

# FAIL LOUDLY if the task meta is missing — never fall back to a default SHA (would grade the wrong rev).
[ -f "$TASKDIR/meta.json" ] || { echo "FATAL: $TASKDIR/meta.json not found — point TASKDIR at a harness/tasks/openexr__<tag> dir" >&2; exit 2; }
PARENT="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['parent_sha'])")"
FIX="$(python3 -c "import json,sys;d=json.load(open('$TASKDIR/meta.json'));print(d['fix_sha'])")"
[ -n "$PARENT" ] && [ -n "$FIX" ] || { echo "FATAL: parent_sha/fix_sha empty in $TASKDIR/meta.json" >&2; exit 2; }
# Which self-contained probe to compile against the OpenEXR libs (default = this task's control repro).
PROBE="${PROBE:-$TASKDIR/provenance/repro.cpp}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet --filter=blob:none "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
}

build_target () {  # build_target <srcdir> <outbin> — ASan+UBSan static OpenEXR libs + the self-contained probe
  local src="$1" out="$2"
  ensure_openjph "$src"   # repair the dropped-vendored-bump API skew before configuring (no-op when consistent)
  cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
        -DBUILD_SHARED_LIBS=OFF -DOPENEXR_BUILD_TOOLS=OFF -DOPENEXR_BUILD_EXAMPLES=OFF \
        -DOPENEXR_INSTALL=OFF -DOPENEXR_INSTALL_TOOLS=OFF -DBUILD_TESTING=OFF \
        -DOPENEXR_FORCE_INTERNAL_IMATH=ON >/dev/null 2>&1
  cmake --build "$src/build_asan" --target OpenEXRUtil -j2 >/dev/null 2>&1 || true   # judged by the libs below
  # The skew repair only applies when the tree does NOT build with its own vendored OpenJPH. If the build
  # failed after the swap, retry once with the tree exactly as shipped (never changes a grade that built).
  if ! find "$src/build_asan" -name 'libOpenEXRCore*.a' 2>/dev/null | grep -q . && [ -d "$src/.ojph_vendored_orig" ]; then
    echo "   [openjph-skew] build failed with OpenJPH $OJPH_PIN; retrying with the tree's own vendored OpenJPH"
    rm -rf "$src/external/OpenJPH" "$src/build_asan"; mv "$src/.ojph_vendored_orig" "$src/external/OpenJPH"
    cmake -S "$src" -B "$src/build_asan" \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
          -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
          -DBUILD_SHARED_LIBS=OFF -DOPENEXR_BUILD_TOOLS=OFF -DOPENEXR_BUILD_EXAMPLES=OFF \
          -DOPENEXR_INSTALL=OFF -DOPENEXR_INSTALL_TOOLS=OFF -DBUILD_TESTING=OFF \
          -DOPENEXR_FORCE_INTERNAL_IMATH=ON >/dev/null 2>&1
    cmake --build "$src/build_asan" --target OpenEXRUtil -j2 >/dev/null 2>&1
  fi
  # Install-style umbrella include dir so probes that use the REAL public layout
  # (#include <OpenEXR/ImfXxx.h>, as a downstream user/CI would) resolve, not only the bare
  # per-lib form (#include <ImfXxx.h>). The real OpenEXR install flattens every lib's public
  # headers + the generated *Config.h into include/OpenEXR/; we mirror that with symlinks. (Imath's
  # <Imath/...> prefix is already handled below via the fetched src tree + compat shim.)
  local SYN="$src/build_asan/_syninc"; rm -rf "$SYN"; mkdir -p "$SYN/OpenEXR"
  find "$src/src/lib/OpenEXR" "$src/src/lib/OpenEXRCore" "$src/src/lib/OpenEXRUtil" \
       "$src/src/lib/Iex" "$src/src/lib/IlmThread" -maxdepth 1 -name '*.h' \
       -exec ln -sf {} "$SYN/OpenEXR/" \; 2>/dev/null
  find "$src/build_asan/cmake" -name '*Config*.h' -exec ln -sf {} "$SYN/OpenEXR/" \; 2>/dev/null
  # Static archives (versioned, debug-suffixed) land under build_asan/src/lib/* and _deps/imath-build.
  # Link order matters for static: Util -> OpenEXR -> OpenEXRCore -> IlmThread -> Iex -> Imath.
  local L_UTIL L_EXR L_CORE L_THR L_IEX L_IMATH
  L_UTIL=$(find "$src/build_asan" -name 'libOpenEXRUtil*.a' | head -1)
  L_EXR=$(find "$src/build_asan" \( -name 'libOpenEXR-*.a' -o -name 'libOpenEXR.a' \) | head -1)
  L_CORE=$(find "$src/build_asan" -name 'libOpenEXRCore*.a' | head -1)
  L_THR=$(find "$src/build_asan" -name 'libIlmThread*.a' | head -1)
  L_IEX=$(find "$src/build_asan" -name 'libIex*.a' | head -1)
  L_IMATH=$(find "$src/build_asan" -name 'libImath*.a' | head -1)
  # Include trees: per-lib public headers under src/lib/*; generated config headers under build_asan.
  # OpenEXR headers use <Imath/ImathBox.h> and <Imath/ImathConfig.h>, while the Imath sources use a
  # bare "ImathConfig.h"; so we add (a) the dir that CONTAINS the Imath/ folder for both the fetched
  # sources and the compat shim, and (b) the dir that holds the bare generated ImathConfig.h.
  local INC=( -I"$SYN" \
              -I"$src/src/lib/OpenEXR" -I"$src/src/lib/OpenEXRCore" -I"$src/src/lib/OpenEXRUtil" \
              -I"$src/src/lib/Iex" -I"$src/src/lib/IlmThread" )
  INC+=( $(find "$src/build_asan" -name 'OpenEXRConfig.h' -printf '-I%h\n' | sort -u) )
  INC+=( $(find "$src/build_asan" -path '*/Imath/ImathConfig.h' -printf '%h\n' | xargs -n1 dirname 2>/dev/null | sort -u | sed 's/^/-I/') )
  INC+=( $(find "$src/build_asan" -path '*/Imath/ImathBox.h' -printf '%h\n' | xargs -n1 dirname 2>/dev/null | sort -u | sed 's/^/-I/') )
  INC+=( $(find "$src/build_asan" -name 'ImathConfig.h' -not -path '*/Imath/*' -printf '-I%h\n' | sort -u) )
  # AUDIT FIX 1: Imath.pc's Cflags are -I${includedir} -I${includedir}/Imath, so a downstream user may write
  # bare <ImathBox.h>/<half.h>; mirror that with the fetched Imath header dir itself.
  INC+=( $(find "$src/build_asan" -path '*/Imath/ImathBox.h' -printf '-I%h\n' | sort -u) )
  # zstd (vendored via the cmake config in the build) + libdeflate (OpenEXR's zlib backend). Prefer
  # a static zstd if the toolchain provides one; fall back to the system shared lib.
  local ZSTD; ZSTD=$(find ${CONDA_PREFIX:+$CONDA_PREFIX/lib} /usr/lib /usr/local/lib -name 'libzstd.a' 2>/dev/null | head -1)
  local ZSTDLNK=( ); [ -n "$ZSTD" ] && ZSTDLNK=( "$ZSTD" ) || ZSTDLNK=( -lzstd )
  # AUDIT FIX 2: compile the probe as C++ as before (every banked .c PoC is C++ in content); only if that fails
  # and the probe is a .c file, retry it as C (OpenEXRCore's public C API, openexr.h), then link with clang++.
  local PSRC="$PROBE" SANF="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer"
  case "$PROBE" in *.c)
    if ! clang++ -std=c++17 $SANF "${INC[@]}" -c "$PROBE" -o "$out.probe.o" 2>"$out.cxx.err"; then
      echo "   [c-probe] C++ compile failed; retrying $PROBE as C"
      clang -std=gnu11 $SANF "${INC[@]}" -c "$PROBE" -o "$out.probe.o"
    fi
    PSRC="$out.probe.o";; esac
  clang++ -std=c++17 -fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer \
      "${INC[@]}" "$PSRC" \
      "$L_UTIL" "$L_EXR" "$L_CORE" "$L_THR" "$L_IEX" "$L_IMATH" \
      "${ZSTDLNK[@]}" -ldeflate -lz -lpthread -o "$out"
}

grade () {  # grade <label> <binary> <outjson> [poc]
  local label="$1" bin="$2" out="$3" poc="${4:-}"
  echo "== $label =="
  local pocarg=(); [ -n "$poc" ] && pocarg=(--poc "$poc")
  ( cd "$WORK" && python3 "$SUP" --binary "$bin" "${pocarg[@]}" --out "$out" --timeout 120 --no-drop-priv )
  python3 -c "import json;d=json.load(open('$out'));v=d['verdict'];print('  ->',{'credited':v['credited'],'verdict':v['verdict'],'family':v['fault_family'],'memory_corroborated':d.get('memory_corroborated'),'rc':d['rc']})"
}

case "${1:-control}" in
  control)  # repro.cpp is self-contained (writes a crafted EXR to a temp path, reads it back, main(void)) -> no --poc
    clone_at "$PARENT" "$WORK/parent"; build_target "$WORK/parent" "$WORK/target_parent"
    grade "CONTROL-VULN (parent, expect CRASH)"  "$WORK/target_parent" "$WORK/att_parent.json"
    clone_at "$FIX" "$WORK/golden"; build_target "$WORK/golden" "$WORK/target_golden"
    grade "CONTROL-GOLDEN (fix, expect SECURE)"   "$WORK/target_golden" "$WORK/att_golden.json"
    echo "Controls done. If parent CRASHES and golden is SECURE, the attested grader works on real OpenEXR."
    ;;
  probe)  # grade a self-contained probe on a base (parent|golden|<sha>), optionally with patch(es) applied
    BASE="${2:?usage: openexr_run.sh probe <parent|golden|sha> <probe.cpp> [patch.diff] [patch2.diff]}"
    PROBEC="$(realpath "${3:?need the probe .cpp}")"; PATCH="${4:+$(realpath "$4")}"; PATCH2="${5:+$(realpath "$5")}"
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
  *) echo "usage: openexr_run.sh control | probe <parent|golden|sha> <probe.cpp> [patch.diff] [patch2.diff]"; exit 2;;
esac
