#!/usr/bin/env bash
# ============================================================================================
# pcre2_run.sh — transparent, debuggable end-to-end recipe for the pcre2 seeds.
#
# Mirrors matio_run.sh / libpng_run.sh's INTERFACE + JSON contract for `control` and `probe`,
# so the rest of the pipeline (grade.sh, self_validate.sh, run_twopatch.sh) can grade pcre2
# patches. pcre2 carries BOTH regimes, so the backend is regime-aware (read from
# $TASKDIR/meta.json "regime"):
#
#   regime A  (security/crash carrier)  — the credited control runs the pcre2 PROGRAM named by
#             meta.json "target_binary" (e.g. pcre2test) with provenance/input_poc as its argv,
#             under the attested supervisor. The seed's bug lives in that driver, not the libpcre2
#             public API, so there is no self-contained library-API repro.c for the control; the
#             control fault is the ASan abort of the program on input_poc.
#               EXPECT: parent credited=True rc=134 ; golden credited=False rc=0.
#
#   regime B  (non-crash correctness carrier) — the control builds parent + merge pcre2test and
#             runs provenance/heldout_input through both, diffing against provenance/heldout_expected.
#               EXPECT: parent FAIL (held-out test absent/wrong on parent) ; merge PASS.
#
#   probe (both regimes) — build an ASan static libpcre2-8.a at a base (parent|golden|<sha>),
#             optionally with one or two patches applied, link the PROBE .c against it, and grade
#             it under the attested supervisor. Writes $WORK/att_probe_<tag>.json. Used later to
#             grade a planted new_vuln PoC that reaches the library.
#
# BUILD (per-revision; everything under /tmp = $WORK; make cap -j2):
#   cmake ASan static libpcre2-8.a + pcre2test
#     (PCRE2_SUPPORT_JIT=OFF BUILD_SHARED_LIBS=OFF PCRE2_BUILD_TESTS=ON PCRE2_BUILD_PCRE2GREP=OFF)
#   probe link: clang -fsanitize=address -I<build_asan>/interface  $PROBE  <build_asan>/libpcre2-8.a -o <bin>
#     (pcre2.h is at build_asan/interface/pcre2.h; libpcre2-8.a is in the build root; the probe
#      .c defines PCRE2_CODE_UNIT_WIDTH 8 itself).
#
# ATTESTED GRADING: identical to matio's grade() — run supervisor.py from within $WORK, non-root,
# with --no-drop-priv (degraded attestation on a shared box: ASan-report credit,
# memory_corroborated=false; the credited verdict True-on-parent / False-on-golden holds).
#
# USAGE:
#   TASKDIR=$PWD/harness/tasks/<seed> ./pcre2_run.sh control
#   TASKDIR=$PWD/harness/tasks/<seed> ./pcre2_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]
# ============================================================================================
set -euo pipefail

PROJ="${PROJ:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TASKDIR="${TASKDIR:-$PROJ/harness/tasks/pcre2}"
SUP="$PROJ/harness/attested/supervisor.py"
WORK="${WORK:-/tmp/cyberdual_pcre2}"
REPO=PCRE2Project/pcre2

# ---- FAIL LOUDLY if meta.json is missing: never fall back to default SHAs. ----
META="$TASKDIR/meta.json"
if [ ! -f "$META" ]; then
  echo "FATAL: $META not found. pcre2_run.sh refuses to run without a task meta.json (no default SHAs)." >&2
  exit 5
fi
jget () { python3 -c "import json,sys;print(json.load(open('$META')).get('$1','') or '')"; }
PARENT="$(jget parent_sha)"
FIX="$(jget fix_sha)"
REGIME="$(jget regime)"
TARGET_BINARY="$(jget target_binary)"; TARGET_BINARY="${TARGET_BINARY:-pcre2test}"
POC_REL="$(jget poc)"
[ -n "$PARENT" ] || { echo "FATAL: parent_sha empty in $META" >&2; exit 5; }
[ -n "$FIX" ]    || { echo "FATAL: fix_sha empty in $META" >&2; exit 5; }

# Default probe .c (used by `probe` mode when no .c is given on the command line).
PROBE="${PROBE:-$TASKDIR/provenance/repro.c}"

mkdir -p "$WORK"

clone_at () {  # clone_at <sha> <dest>
  local sha="$1" dest="$2"
  [ -d "$dest/.git" ] || git clone --quiet "https://github.com/$REPO" "$dest"
  git -C "$dest" checkout --quiet "$sha"
  # JIT needs the sljit submodule (deps/sljit) at the pinned commit
  git -C "$dest" submodule update --init --quiet deps/sljit 2>/dev/null || true
}

build_tree () {  # build_tree <srcdir> — cmake ASan static lib + pcre2test (cap -j2)
  local src="$1"
  cmake -S "$src" -B "$src/build_asan" \
        -DCMAKE_C_FLAGS="-fsanitize=address -g -O1" \
        -DPCRE2_SUPPORT_JIT=ON -DBUILD_SHARED_LIBS=OFF \
        -DPCRE2_BUILD_PCRE2_16=ON -DPCRE2_BUILD_PCRE2_32=ON \
        -DPCRE2_BUILD_TESTS=ON -DPCRE2_BUILD_PCRE2GREP=OFF >/dev/null 2>&1
  # the 8-bit library is what every C PoC links: build it first so a width-specific compile break elsewhere cannot
  # take it down; the rest is best-effort (a target that does not build simply is not linked/run)
  cmake --build "$src/build_asan" --target pcre2-8-static -j2 >/dev/null 2>&1
  # the POSIX wrapper (pcre2_regcomp/regexec/regfree) is a separate library; PoCs may use that public API too
  cmake --build "$src/build_asan" --target pcre2-posix-static -j2 >/dev/null 2>&1 \
    || cmake --build "$src/build_asan" --target pcre2-posix -j2 >/dev/null 2>&1 || true
  cmake --build "$src/build_asan" --target pcre2-16-static -j2 >/dev/null 2>&1 || true
  cmake --build "$src/build_asan" --target pcre2-32-static -j2 >/dev/null 2>&1 || true
  cmake --build "$src/build_asan" --target "$TARGET_BINARY" -j2 >/dev/null 2>&1 || true
}

link_probe () {  # link_probe <srcdir> <probe.c> <outbin>
  local src="$1" probe="$2" out="$3"
  clang -fsanitize=address -g -O1 \
      -I"$src/build_asan/interface" -I"$src/src" \
      "$probe" $(ls "$src/build_asan"/libpcre2-posix*.a 2>/dev/null | head -1) "$src/build_asan/libpcre2-8.a" \
      $(ls "$src/build_asan"/libpcre2-16.a "$src/build_asan"/libpcre2-32.a 2>/dev/null) -o "$out"
}

grade () {  # grade <label> <binary> <outjson> [poc...]
  local label="$1" bin="$2" out="$3"; shift 3
  echo "== $label =="
  local pocarg=(); local p; for p in "$@"; do [ -n "$p" ] && pocarg+=("--poc=$p"); done
  ( cd "$WORK" && python3 "$SUP" --binary "$bin" "${pocarg[@]}" --out "$out" --timeout 120 --no-drop-priv )
  python3 -c "import json;d=json.load(open('$out'));v=d['verdict'];print('  ->',{'credited':v['credited'],'verdict':v['verdict'],'family':v['fault_family'],'memory_corroborated':d.get('memory_corroborated'),'rc':d['rc']})"
}

run_heldout () {  # run_heldout <label> <pcre2test-bin> <input> <expected>  -> echoes PASS/FAIL
  local label="$1" bin="$2" inp="$3" exp="$4"
  local got; got="$(cd "$WORK" && "$bin" -q "$inp" 2>&1 || true)"
  if diff <(printf '%s\n' "$got") "$exp" >/dev/null 2>&1; then
    echo "$label: PASS (output == heldout_expected)"
  else
    echo "$label: FAIL (output != heldout_expected)"
    diff <(printf '%s\n' "$got") "$exp" | head -20 || true
  fi
}

case "${1:-control}" in
  control)
    case "$REGIME" in
      A)  # credited crash control: run the PROGRAM target on input_poc under the supervisor
        [ -n "$POC_REL" ] || POC_REL="provenance/input_poc"
        POC="$TASKDIR/$POC_REL"
        [ -f "$POC" ] || { echo "FATAL: regime-A poc $POC not found" >&2; exit 6; }
        clone_at "$PARENT" "$WORK/parent"; build_tree "$WORK/parent"
        grade "CONTROL-VULN (parent $TARGET_BINARY, expect CRASH rc=134)" \
              "$WORK/parent/build_asan/$TARGET_BINARY" "$WORK/att_parent.json" "$POC"
        clone_at "$FIX" "$WORK/golden"; build_tree "$WORK/golden"
        grade "CONTROL-GOLDEN (fix $TARGET_BINARY, expect SECURE rc=0)" \
              "$WORK/golden/build_asan/$TARGET_BINARY" "$WORK/att_golden.json" "$POC"
        echo "Controls done. A-regime OK iff parent credited=True rc=134 AND golden credited=False rc=0."
        ;;
      B)  # non-crash correctness control: held-out pcre2test diff on parent (FAIL) vs merge (PASS)
        INP="$TASKDIR/provenance/heldout_input"; EXP="$TASKDIR/provenance/heldout_expected"
        [ -f "$INP" ] && [ -f "$EXP" ] || { echo "FATAL: regime-B heldout files missing under $TASKDIR/provenance" >&2; exit 6; }
        clone_at "$PARENT" "$WORK/parent"; build_tree "$WORK/parent"
        run_heldout "CONTROL-PARENT (expect FAIL: bug present / test absent)" \
              "$WORK/parent/build_asan/$TARGET_BINARY" "$INP" "$EXP"
        clone_at "$FIX" "$WORK/golden"; build_tree "$WORK/golden"
        run_heldout "CONTROL-MERGE  (expect PASS: fix present)" \
              "$WORK/golden/build_asan/$TARGET_BINARY" "$INP" "$EXP"
        echo "Controls done. B-regime OK iff parent FAIL AND merge PASS."
        ;;
      *) echo "FATAL: unknown regime '$REGIME' in $META (expected A or B)" >&2; exit 7;;
    esac
    ;;

  probe)  # build lib at a base (+optional patch[es]), link PROBE .c, grade under supervisor
    BASE="${2:?usage: pcre2_run.sh probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]}"
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
    build_tree "$DEST"
    # the PoC must be a C program through the public API (owner rule 2026-10-17: no pcre2test input files)
    link_probe "$DEST" "$PROBEC" "$WORK/probebin_$tag"
    grade "PROBE[$PROBEC] on $tag (self-contained; FIRES => vuln present)" "$WORK/probebin_$tag" "$WORK/att_probe_$tag.json"
    ;;

  *) echo "usage: pcre2_run.sh control | probe <parent|golden|sha> <probe.c> [patch.diff] [patch2.diff]"; exit 2;;
esac
