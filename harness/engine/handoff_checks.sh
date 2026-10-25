# handoff_checks.sh — answer-key checks of an engine reviewer handoff (POSIX sh; run_engine_window.sh prepends this
# file to the scripts it runs inside the handoff / verification containers, and tests source it on the host).
# The reviewer must never see the attacker's probe or manifests: not in the tree, and not anywhere in the history
# it receives (git log main..maint-batch), where an attacker commit may have added and later deleted them.

# history_leaks [RANGE]: every path named pocs/..., submission.json or selection.json, at any depth, that a commit in
# RANGE (default main..maint-batch) adds, modifies or deletes; prints nothing when the history is clean
history_leaks () {
  git log --format= --name-only --no-renames "${1:-main..maint-batch}" -- 2>/dev/null \
    | grep -E '(^|/)(pocs/|submission\.json$|selection\.json$)' | sort -u
}

# scrub_history_leaks [RANGE]: rewrite RANGE without those paths (index-filter; commit identities, messages and every
# other path are kept, commits that become empty are kept so the window length does not change); returns 1 if a
# leaked path survives
scrub_history_leaks () {
  _range="${1:-main..maint-batch}"
  [ -n "$(history_leaks "$_range")" ] || return 0
  FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch -f --index-filter \
    "git rm -r -q --cached --ignore-unmatch -- ':(glob)**/pocs/**' ':(glob)**/submission.json' ':(glob)**/selection.json'" \
    "$_range" >/dev/null 2>&1 || return 1
  git for-each-ref --format="%(refname)" refs/original 2>/dev/null | while read -r _r; do git update-ref -d "$_r"; done
  [ -z "$(history_leaks "$_range")" ]
}
