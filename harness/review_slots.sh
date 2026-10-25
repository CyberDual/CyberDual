# Shared limit on concurrent Claude REVIEWER calls across every run and session on this host (the review credential
# has a session limit, and parallel reviews raise the spend rate). review_slot_run CMD... takes one of REVIEW_SLOTS
# (default 8) lock files under REVIEW_SLOT_DIR, runs CMD, and releases the slot when CMD ends (also if the process
# dies: the lock goes with its file descriptor). Calls wait while all slots are taken. Codex reviewers are not limited.
review_slot_run () {
  local n="${REVIEW_SLOTS:-8}" d="${REVIEW_SLOT_DIR:-/tmp/cyberdual_review_slots}" i fd rc waited=0
  mkdir -p "$d"
  while :; do
    for i in $(seq 1 "$n"); do
      exec {fd}>>"$d/slot.$i"
      if flock -n "$fd"; then
        [ "$waited" = 1 ] && echo "[review slots] got slot $i of $n" >&2
        "$@"; rc=$?
        flock -u "$fd"; exec {fd}>&-
        return "$rc"
      fi
      exec {fd}>&-
    done
    [ "$waited" = 0 ] && echo "[review slots] all $n reviewer slots busy; waiting" >&2; waited=1
    sleep 15
  done
}
