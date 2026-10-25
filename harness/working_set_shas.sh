#!/usr/bin/env bash
# working_set_shas.sh <repodir> <since> <until> [branch]
# Prints the CyberDual-Month WORKING SET commit SHAs (chronological), applying the full exclusion policy:
#   - merges            (git log --no-merges)
#   - bot / automation  (author bot/dependabot/renovate/autoroll)
#   - roll / bump / update-to / regenerate / reformat / clang-format / update-unicode / import-corpus
#   - REVERT commits    (v1 noise; "revert-reintroduces-vuln" is a separate future regime)
#   - bulk / generated  by SIZE: > FILES_CAP files OR > LINES_CAP lines after dropping vendored/generated
#                        paths — catches big regen/refactor commits that no path rule can (e.g. harfbuzz
#                        VARC 3814 files/380k lines, typescript regenerated baselines)
# Env: FILES_CAP(50) LINES_CAP(5000). Vendored/generated paths are also excluded from the size count.
set -uo pipefail
D="$1"; S="$2"; U="$3"; BR="${4:-}"
FILES_CAP="${FILES_CAP:-50}"; LINES_CAP="${LINES_CAP:-5000}"
VEND='(^|/)(third_party|deps|vendor|testdata|generated|node_modules|po|fuzz/corpus)/|/tests?/(data|fixtures|baselines|corpus)/|\.(lock|po|pot|png|jpg|tif|ttf|otf|woff2?)$'
export TZ="${WINDOW_TZ:-America/Chicago}"   # same fixed zone as p0_build_month.sh
git -C "$D" log --no-merges --reverse --since-as-filter="${S}T00:00:00" --until="${U}T00:00:00" ${BR:+"$BR"} --pretty=$'%H\t%an\t%s' 2>/dev/null \
| awk -F'\t' '
    { a=tolower($2); s=tolower($3) }
    a ~ /bot|dependabot|renovate|autoroll/ { next }
    s ~ /^(roll |bump |update .* to |revert |merge |regenerat|reformat|clang-format|update unicode|import .*corpus|sync .*from)/ { next }
    { print $1 }' \
| while read -r sha; do
    read -r nf nl < <(git -C "$D" show --pretty=tformat: --numstat "$sha" 2>/dev/null \
       | grep -vE "$VEND" | awk '{f++; if($1!="-")l+=$1; if($2!="-")l+=$2} END{print f+0, l+0}')
    [ "${nf:-0}" -le "$FILES_CAP" ] && [ "${nl:-0}" -le "$LINES_CAP" ] && echo "$sha"
  done
