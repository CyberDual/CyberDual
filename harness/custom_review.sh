# custom_review.sh — run a user-supplied reviewer command in place of the model reviewer (sourced by rereview_any.sh).
#
# Contract (README.md, "Your own reviewer"):
#   - CYBERDUAL_REVIEW_CMD runs with the review checkout as its cwd, in the agent image (CYBERDUAL_CUSTOM_IMAGE,
#     default IMAGE) with --network none; CYBERDUAL_CUSTOM_NETWORK=1 (`--allow-network`) uses Docker's default bridge
#     network instead (no egress allow-list). CYBERDUAL_CUSTOM_MOUNT=<dir> is mounted read-only at /opt/custom;
#     CYBERDUAL_CUSTOM_ENV="VAR ..." passes those variables through by name (values never appear in argv or logs).
#   - env CYBERDUAL_REPO = the checkout (/work), CYBERDUAL_PROMPT = the review prompt (system prompt + task) the
#     model reviewers get. The checkout is the same scrubbed handoff: no PoC, no answer key.
#   - the command may edit files in place. If its stdout holds a unified diff, the diff is applied to the checkout.
#     An optional review.json (verdict, locations) is kept beside the grade for analysis only; verdict "refused"
#     marks a refusal (REVIEWER_REFUSED after REV_ATTEMPTS, like a model refusal).
#   - the tree it leaves is committed and graded by rereview_any.sh exactly like a model reviewer's.
# Files beside the grade (review_<suf>_<arm>_transcript.*): .jsonl (a synthetic result event), .rc, .stdout, .stderr,
# .stdout_diff (applied | apply_failed), .review.json; review_<suf>_prompt.md is the prompt it was given.
# A non-zero exit (other than the wall timeout) is an infrastructure failure (REVIEWER_INFRA_FAILURE, re-run).

# run_custom_review WK PROMPT_FILE TRANSCRIPT -> sets CYBERDUAL_CUSTOM_REFUSED (0|1)
run_custom_review () {
  local wk="$1" prompt="$2" tr="$3" base="${3%.jsonl}" rc=0 net=(--network none) mnt=() envs=() v
  [ "${CYBERDUAL_CUSTOM_NETWORK:-0}" = 1 ] && net=()
  [ -n "${CYBERDUAL_CUSTOM_MOUNT:-}" ] && mnt=(-v "$(cd "$CYBERDUAL_CUSTOM_MOUNT" && pwd)":/opt/custom:ro)
  for v in ${CYBERDUAL_CUSTOM_ENV:-}; do envs+=(-e "$v"); done
  declare -p WALLPFX >/dev/null 2>&1 || WALLPFX=(timeout --signal=TERM "${WALL:-3600}")
  "${WALLPFX[@]}" docker run --rm -i "${net[@]}" --user "$(id -u):$(id -g)" -e HOME=/tmp "${envs[@]}" "${mnt[@]}" \
    -e CYBERDUAL_REPO=/work -e CYBERDUAL_PROMPT=/prompt.md -v "$wk":/work -v "$prompt":/prompt.md:ro -w /work \
    "${CYBERDUAL_CUSTOM_IMAGE:-$IMAGE}" bash -c "$CYBERDUAL_REVIEW_CMD" < /dev/null > "$base.stdout" 2> "$base.stderr" || rc=$?
  printf '%s\n' "$rc" > "$base.rc"
  # a unified diff on stdout is applied to the checkout (index and tree), as if the command had edited the files
  if grep -qE '^(diff --git |--- [^ ]|\+\+\+ [^ ])' "$base.stdout" 2>/dev/null; then
    if git -C "$wk" apply --whitespace=nowarn "$base.stdout" 2>>"$base.stderr"; then echo "applied" > "$base.stdout_diff"
    else echo "apply_failed" > "$base.stdout_diff"; echo "   [custom reviewer] stdout diff did not apply (kept the tree as left)" >&2; fi
  fi
  CYBERDUAL_CUSTOM_REFUSED=0
  if [ -f "$wk/review.json" ]; then
    cp "$wk/review.json" "$base.review.json"
    CYBERDUAL_CUSTOM_REFUSED="$(python3 -c "import json,sys
try: v=str(json.load(open(sys.argv[1])).get('verdict','')).lower()
except Exception: v=''
print(1 if v in ('refused','refusal') else 0)" "$wk/review.json")"
  fi
  # the transcript _budget_exhausted.py reads: a finished session unless the command failed (wall stops: .rc 124/137)
  if [ "$rc" = 0 ]; then printf '{"type":"result","subtype":"success","total_cost_usd":0,"custom_reviewer":true}\n' > "$tr"
  else printf '{"type":"system","subtype":"custom_reviewer_exit","rc":%s}\n' "$rc" > "$tr"; fi
}
