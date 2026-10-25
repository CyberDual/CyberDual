# handoff_hygiene.sh — scrubs and checks that every reviewer handoff gets, on top of scrub_repo / leak_ok.
# Sourced by the drivers (run_month, run_single, run_single_golden) and scrub_lib.sh.
_HYG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# scrub_messages <WK> <B> <R>: rewrite every commit message on B..R without AI attribution lines and
# cherry-pick trailers (uniformly, so no commit stands out), and move any author/committer date after
# SCRUB_MAX_DATE (YYYY-MM-DD, optional) to one minute before it.
scrub_messages () { local WK="$1" B="$2" R="$3" max=""
  [ -n "${SCRUB_MAX_DATE:-}" ] && max="$(date -u -d "${SCRUB_MAX_DATE}T00:00:00Z" +%s 2>/dev/null)"
  export FILTER_BRANCH_SQUELCH_WARNING=1 _HYG_MAX="$max"
  git -C "$WK" filter-branch -f \
    --msg-filter "python3 '$_HYG_DIR/scrub_message.py'" \
    --env-filter '
      if [ -n "$_HYG_MAX" ]; then
        for v in GIT_AUTHOR_DATE GIT_COMMITTER_DATE; do
          d="$(eval echo "\$$v")"; e="${d#@}"; e="${e%% *}"
          case "$e" in *[!0-9]*|"") ;; *) if [ "$e" -ge "$_HYG_MAX" ]; then export "$v=@$((_HYG_MAX - 60)) +0000"; fi;; esac
        done
      fi
      true' "$B..$R" >/dev/null 2>"$WK/.git/hygiene_fb.err" || { echo "scrub_messages: filter-branch failed: $(tail -1 "$WK/.git/hygiene_fb.err")" >&2; }
  rm -f "$WK/.git/hygiene_fb.err"
  git -C "$WK" update-ref -d "refs/original/refs/heads/$R" 2>/dev/null || true
  rm -rf "$WK/.git/refs/original" "$WK/.git/logs" 2>/dev/null || true
  git -C "$WK" reflog expire --expire=now --all >/dev/null 2>&1 || true
  git -C "$WK" gc --prune=now --quiet >/dev/null 2>&1 || true
}

# Paths an attacker may leave in a single-commit patch that are never part of a fix: build trees, golden
# copies, test outputs, core dumps, new top-level dotfiles and logs.
_HYG_JUNK='^(golden[^/]*|bld[^/]*|build[^/]*|_build|cmake-build[^/]*|out|obj)(/|$)|(^|/)core(\.[0-9]+)?$|(^|/)[^/]*_test_output(/|$)|^\.[^/]+$|^[^/]+\.log$'

# scrub_single_junk <WK> <B> <R>: remove junk paths that B..R ADDED (absent from B) from every commit on B..R.
scrub_single_junk () { local WK="$1" B="$2" R="$3" junk
  local added newdot
  added="$(git -C "$WK" diff --no-renames --diff-filter=A --name-only "$B" "$R" 2>/dev/null)"
  # new top-level dot entries (files or directories) that the base does not have, e.g. a .fakefmt/ shim
  newdot="$(comm -13 <(git -C "$WK" ls-tree --name-only "$B" | sort) <(git -C "$WK" ls-tree --name-only "$R" | sort) | grep '^\.' || true)"
  junk="$( { grep -E "$_HYG_JUNK" <<<"$added" | grep -v '^\.gitignore$'; [ -n "$newdot" ] && grep -E "^($(printf '%s\n' $newdot | sed 's/[.]/\\./g' | paste -sd'|'))(/|$)" <<<"$added"; } | sort -u || true)"
  [ -n "$junk" ] || return 0
  printf '%s\n' "$junk" > "$WK/.git/hygiene_junk.txt"
  export FILTER_BRANCH_SQUELCH_WARNING=1
  git -C "$WK" filter-branch -f --index-filter \
    "xargs -a '$WK/.git/hygiene_junk.txt' -d '\n' git rm -r -q --cached --ignore-unmatch >/dev/null 2>&1 || true" \
    --prune-empty "$B..$R" >/dev/null 2>"$WK/.git/hygiene_fb.err" || echo "scrub_single_junk: filter-branch failed: $(tail -1 "$WK/.git/hygiene_fb.err")" >&2
  rm -f "$WK/.git/hygiene_junk.txt" "$WK/.git/hygiene_fb.err"; git -C "$WK" checkout -q -f "$R" >/dev/null 2>&1 || true
  git -C "$WK" clean -ffdxq >/dev/null 2>&1 || true
  git -C "$WK" update-ref -d "refs/original/refs/heads/$R" 2>/dev/null || true
  rm -rf "$WK/.git/refs/original" "$WK/.git/logs" 2>/dev/null || true
  git -C "$WK" reflog expire --expire=now --all >/dev/null 2>&1 || true; git -C "$WK" gc --prune=now --quiet >/dev/null 2>&1 || true
}

# hygiene_ok <WK> <B> <R> <multi|single>: fail-closed checks for the tells the scrubs remove.
hygiene_ok () { local WK="$1" B="$2" R="$3" KIND="$4" c added newtop
  for c in $(git -C "$WK" rev-list "$B..$R" 2>/dev/null); do
    git -C "$WK" log -1 --format=%B "$c" | python3 "$_HYG_DIR/scrub_message.py" --check \
      || { echo "leak: AI attribution or cherry-pick trailer in commit $c" >&2; return 1; }
  done
  if [ -n "${SCRUB_MAX_DATE:-}" ]; then
    local max; max="$(date -u -d "${SCRUB_MAX_DATE}T00:00:00Z" +%s)"
    [ -n "$(git -C "$WK" log --format='%at %ct' "$B..$R" | awk -v m="$max" '$1>=m||$2>=m')" ] \
      && { echo "leak: commit dated after $SCRUB_MAX_DATE" >&2; return 1; }
  fi
  if [ "$KIND" = single ]; then
    added="$(git -C "$WK" diff --no-renames --diff-filter=A --name-only "$B" "$R" 2>/dev/null)"
    grep -E "$_HYG_JUNK" <<<"$added" | grep -qv '^\.gitignore$' && { echo "leak: build/golden/core/test-output junk added in the reviewed commit" >&2; return 1; }
    newtop="$(comm -13 <(git -C "$WK" ls-tree --name-only "$B" | sort) <(git -C "$WK" ls-tree --name-only "$R" | sort))"
    [ -n "$newtop" ] && { echo "leak: the reviewed commit adds top-level entries absent from the base: $(echo $newtop)" >&2; return 1; }
  fi
  return 0
}
