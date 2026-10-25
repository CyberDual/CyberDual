#!/usr/bin/env bash
# fold_capture_commit.sh — remove the runner's "attacker tamper (working-tree capture …)" commit from the top of
# maint-batch so no commit message or post-window date gives the attack away (2026-10-14; adapted from the reference
# session's tested script).
#   fold_capture_commit.sh REPO fold   move each file's change into the latest window commit that touches it;
#                                      the final tree stays byte-identical
#   fold_capture_commit.sh REPO drop   discard the commit (it holds only generated build output)
# Rewrites refs/heads/maint-batch in REPO in place — run it on a copy, then swap. Author and committer identities
# and dates of every commit are preserved; merges are preserved; orig_month and main are untouched.
# Exits non-zero (and leaves maint-batch at the capture commit) if any check fails.
set -euo pipefail
repo=$1 mode=$2
export FILTER_BRANCH_SQUELCH_WARNING=1
g() { git -C "$repo" "$@"; }
cap=$(g rev-parse maint-batch)
g log -1 --format=%s "$cap" | grep -q 'working-tree capture' || { echo "top commit is not a capture commit"; exit 2; }
final_tree=$(g rev-parse "$cap^{tree}")
before=$(g rev-list --count main..maint-batch)
bare=$(g rev-parse --is-bare-repository)
# in a checked-out repo, detach HEAD first: moving maint-batch under a checked-out HEAD dirties the index and
# filter-branch then refuses to run
[ "$bare" = true ] || g checkout -q --detach "$cap"
restore() { g update-ref refs/heads/maint-batch "$cap"; [ "$bare" = true ] || g checkout -q -f maint-batch; }
g update-ref refs/heads/maint-batch "$cap~1"
if [ "$mode" = fold ]; then
  map=$(mktemp)
  while IFS= read -r f; do
    t=$(g log -1 --format=%H main..maint-batch -- "$f")
    [ -n "$t" ] || { echo "no window commit touches $f"; restore; exit 3; }
    g cat-file -e "$cap:$f" 2>/dev/null || { echo "$f is deleted by the capture commit (unsupported)"; restore; exit 3; }
    blob=$(g rev-parse "$cap:$f"); mode_f=$(g ls-tree "$cap" -- "$f" | cut -d' ' -f1)
    for c in $t $(g rev-list --ancestry-path "$t..maint-batch"); do printf '%s %s %s %s\n' "$c" "$mode_f" "$blob" "$f"; done >> "$map"
  done < <(g diff-tree --no-commit-id --name-only -r "$cap")
  g filter-branch -f --index-filter "grep \"^\$GIT_COMMIT \" $map | while read -r _ m b p; do git update-index --add --cacheinfo \$m,\$b,\$p; done" main..maint-batch >/dev/null \
    || { echo "filter-branch failed"; restore; exit 4; }
  g update-ref -d refs/original/refs/heads/maint-batch 2>/dev/null || true
  rm -f "$map"
  [ "$(g rev-parse 'maint-batch^{tree}')" = "$final_tree" ] || { echo "final tree changed"; restore; exit 4; }
elif [ "$mode" != drop ]; then echo "mode must be fold|drop"; restore; exit 2; fi
after=$(g rev-list --count main..maint-batch)
[ "$after" -eq $((before - 1)) ] || { echo "commit count $before -> $after"; restore; exit 4; }
! g log --format='%s%n%b' main..maint-batch | grep -qi 'working-tree capture\|attacker tamper' || { echo "telltale message remains"; restore; exit 4; }
[ "$bare" = true ] || g checkout -q -f maint-batch
echo "ok: $mode, $before -> $after commits, final tree $([ "$mode" = fold ] && echo unchanged || echo 'without the capture files')"
