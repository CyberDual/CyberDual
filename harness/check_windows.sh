#!/usr/bin/env bash
# check_windows.sh — fail LOUDLY if project maintenance windows are inconsistent or out of band.
# Guards against the libssh2-trimonth class of bug (a window set differently in two scripts, or a window
# whose working-set commit count is outside the design band). Run in CI / before any month sweep.
#
#   bash harness/check_windows.sh            # consistency only (fast, no clones)
#   CHECK_WS=1 bash harness/check_windows.sh # also verify each built window's working-set is in band
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$HERE/project_windows.tsv"
WS_LO="${WS_LO:-45}"; WS_HI="${WS_HI:-200}"   # design band ~50-200; floor 45 tolerates matio (48)
rc=0

# canonical since/until per project
declare -A C_SINCE C_UNTIL C_REPO
while IFS=$'\t' read -r p repo since until span; do
  case "$p" in ''|'#'*) continue;; esac
  C_SINCE["$p"]="$since"; C_UNTIL["$p"]="$until"; C_REPO["$p"]="$repo"
done < "$CANON"
echo ">> canonical windows: ${#C_SINCE[@]} projects (band ${WS_LO}-${WS_HI} WS)"

# 1) consistency: every window-setting source must agree with the canonical SINCE
check_source () { # <file> <regex to pull 'proj|url|since'>
  local f="$1"; [ -f "$f" ] || return 0
  grep -E '^[a-z][a-z0-9_-]*\|https?://' "$f" | while IFS='|' read -r p url since rest; do
    [ -n "${C_SINCE[$p]:-}" ] || continue
    if [ "$since" != "${C_SINCE[$p]}" ]; then
      echo "MISMATCH: $f sets $p since=$since but canonical=${C_SINCE[$p]}" >&2; echo X >>"$HERE/.winfail"
    fi
  done
}
: > "$HERE/.winfail"
for f in per_commit_sizes.sh run_month_topup.sh; do check_source "$HERE/$f"; done
if [ -s "$HERE/.winfail" ]; then rc=1; else echo "   consistency: all sources agree with project_windows.tsv"; fi
rm -f "$HERE/.winfail"

# 2) working-set band: build each window's WS and require WS_LO <= WS <= WS_HI
if [ "${CHECK_WS:-0}" = 1 ]; then
  echo ">> working-set band check (clones each repo; slow)"
  for p in "${!C_SINCE[@]}"; do
    d="$(mktemp -d /tmp/wincheck.XXXXXX)"
    if git clone --quiet --filter=blob:none "${C_REPO[$p]}" "$d" 2>/dev/null; then
      def="$(git -C "$d" symbolic-ref --short HEAD 2>/dev/null || echo master)"
      ws="$(bash "$HERE/working_set_shas.sh" "$d" "${C_SINCE[$p]}" "${C_UNTIL[$p]}" "$def" 2>/dev/null | wc -l)"
      if [ "$ws" -lt "$WS_LO" ] || [ "$ws" -gt "$WS_HI" ]; then
        echo "OUT-OF-BAND: $p WS=$ws (band ${WS_LO}-${WS_HI}) window ${C_SINCE[$p]}..${C_UNTIL[$p]}" >&2; rc=1
      else echo "   $p WS=$ws ok"; fi
    else echo "   $p clone failed (skipped)" >&2; fi
    rm -rf "$d"
  done
fi

[ "$rc" = 0 ] && echo ">> windows OK" || echo ">> WINDOW CHECK FAILED" >&2
exit $rc
