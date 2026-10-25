#!/usr/bin/env bash
# per_commit_sizes.sh — per-commit size (files, +ins, -del) for every WORKING-SET commit of each
# chosen library at its window. Reuses working_set_shas.sh (merges/bot/roll/revert/generated/outlier
# already excluded). Output: benchmarks/cyberdual_month_commit_sizes.csv (one row per commit).
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; PROJ="$(cd "$HERE/.." && pwd)"
OUT="$PROJ/benchmarks/cyberdual_month_commit_sizes.csv"
TMP="$PROJ/_pcs_tmp"; mkdir -p "$TMP"; UNTIL=2026-10-01
echo "project,window,sha,files,insertions,deletions,subject" > "$OUT"
MAP=$(cat <<'M'
expat|https://github.com/libexpat/libexpat|2026-09-01|month
libarchive|https://github.com/libarchive/libarchive|2026-09-01|month
pcre2|https://github.com/PCRE2Project/pcre2|2026-09-01|month
harfbuzz|https://github.com/harfbuzz/harfbuzz|2026-09-01|month
libheif|https://github.com/strukturag/libheif|2026-09-01|month
libssh2|https://github.com/libssh2/libssh2|2026-09-01|month
curl|https://github.com/curl/curl|2026-09-17|biweek
exiv2|https://github.com/Exiv2/exiv2|2026-08-01|bimonth
openexr|https://github.com/AcademySoftwareFoundation/openexr|2026-08-01|bimonth
gpac|https://github.com/gpac/gpac|2026-08-01|bimonth
libwebp|https://github.com/webmproject/libwebp|2026-08-01|bimonth
matio|https://github.com/tbeu/matio|2026-07-01|trimonth
freetype|https://github.com/freetype/freetype|2026-07-01|trimonth
xz|https://github.com/tukaani-project/xz|2026-07-01|trimonth
M
)
while IFS='|' read -r proj url since label; do
  [ -z "$proj" ] && continue
  echo ">> $proj [$label $since..$UNTIL]" >&2
  d="$TMP/$proj"; rm -rf "$d"; buf=$(date -u -d "$since - 5 days" +%Y-%m-%d)
  timeout 900 nice -n 19 git clone --quiet --no-tags --single-branch --shallow-since="$buf" "$url" "$d" 2>>"$TMP/err" || { echo "$proj,$label,CLONE_FAIL,,,," >>"$OUT"; rm -rf "$d"; continue; }
  def=$(git -C "$d" symbolic-ref --short HEAD 2>/dev/null || echo master)
  bash "$HERE/working_set_shas.sh" "$d" "$since" "$UNTIL" "$def" | while read -r sha; do
    [ -n "$sha" ] || continue
    read -r f i del < <(git -C "$d" show --pretty=tformat: --numstat "$sha" 2>/dev/null | awk '{c++; if($1!="-")ins+=$1; if($2!="-")d+=$2} END{print c+0, ins+0, d+0}')
    subj=$(git -C "$d" log -1 --pretty=%s "$sha" 2>/dev/null | tr ',\n' ';  ' | cut -c1-70)
    echo "$proj,$label,${sha:0:10},$f,$i,$del,$subj" >> "$OUT"
  done
  rm -rf "$d"
done <<<"$MAP"
rm -rf "$TMP"; echo "DONE -> $OUT ($(($(wc -l <"$OUT")-1)) commits)" >&2
