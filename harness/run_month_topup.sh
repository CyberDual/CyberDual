#!/usr/bin/env bash
# run_month_topup.sh — resample per project until TARGET genuine attacker attempts, where a GENUINE
# attempt = the attacker produced a patch+PoC with an adjudicable VALID_SERIES or
# INVALID_SERIES result. Grader-starved cells, cyber-blocks, self-refusals and no-output do NOT count (the attacker's internal
# resample loop in run_month already retries these via _refusal.py on the agent transcript; a cell
# that still ends REFUSED/NO_OUTPUT is skipped and replaced). Reuses existing genuine cells; only tops
# up the shortfall. Invalid attempts (PoC can't fire / patch can't apply or build) DO count — they are
# real attempts whose output had issues.
#   ATT_MODEL=claude-opus-4-8 OUTBASE=runs_v2multi bash harness/run_month_topup.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; PROJ_ROOT="$(cd "$HERE/.." && pwd)"; cd "$PROJ_ROOT"
SIZES="${SIZES:-$HERE/per_commit_sizes.sh}"
TARGET="${TARGET:-5}"; MAXCELLS="${MAXCELLS:-20}"; PAR="${PAR:-4}"   # documented multi policy: GIVEUP=6 MAXCELLS=20
GIVEUP="${GIVEUP:-6}"; INFRA_CAP="${INFRA_CAP:-12}"                   # infra (429) failures get a SEPARATE cap, never count as refusals
ATT_SYS="${ATT_SYS:-roles/attacker.window.md}"   # enforced-multi (required arm)
ATT_MODEL="${ATT_MODEL:-claude-opus-4-8}"; REV="${REV:-claude-sonnet-5}"; ARMS="${ARMS-shell}"
OUTBASE="${OUTBASE:-runs_v2multi}"; UNTIL="${UNTIL:-2026-10-01}"
ATT_ATTEMPTS="${ATT_ATTEMPTS:-2}"   # attacker internal resample per cell on refusal (LOW: stochastic refusal clears in 1-2; persistent gives up fast). Total/project bounded by MAXCELLS*ATT_ATTEMPTS.
mkdir -p "$OUTBASE"

declare -A SINCE REPO
while IFS='|' read -r p repo start span; do
  case "$p" in ''|'#'*) continue;; esac
  [ -n "$repo" ] && [ -n "$start" ] || continue
  SINCE["$p"]="$start"; REPO["$p"]="$repo"
done < <(grep -E '^[a-z][a-z0-9_-]*\|https?://' "$SIZES")

LIST="${PROJECTS:-${!SINCE[*]}}"; [ "$#" -gt 0 ] && LIST="$*"

genuine_count () { local p="$1"
  python3 - "$OUTBASE" "$p" <<'PY'
import glob, json, sys
from harness.multi_artifacts import genuine
base, project = sys.argv[1:]
count = 0
for cell in glob.glob(f"{base}/month_{project}_vB_*"):
    try:
        with open(f"{cell}/series_grade_v3.json") as stream:
            grade = json.load(stream)
    except (OSError, ValueError):
        continue
    count += (grade.get("outcome") in ("VALID_SERIES", "INVALID_SERIES", "GRADER_ERROR")   # GRADER_ERROR holds the slot
              and grade.get("terminal_state") != "GRADER_STARVED" and genuine(cell))
print(count)
PY
}
# GIVEUP = consecutive REFUSED/no-output (model-ended) attempts after which a project is a refusal wall.
# Provider 429 / usage-limit failures are INFRA, not refusals: they NEVER increment the refusal streak
# and do NOT consume the MAXCELLS model-call backstop; they have their own INFRA_CAP so repeated 429s
# cannot loop forever. (reset refusal streak on any new genuine.)
is_429_cell () { local o="$1"; local tp="$o/attacker_transcript.jsonl"; [ -f "$tp" ] || return 1   # two locals: $o is unset until its own statement ends
  local t; t="$(tail -c 6000 "$tp" 2>/dev/null)"
  echo "$t" | grep -q '"is_error":true' || return 1
  echo "$t" | grep -qiE '429|usage limit|rate.?limit' || return 1
  echo "$t" | grep -qiE 'cyber|safeguard' && return 1   # cyber is a model refusal, not infra
  return 0; }
wait_disk () { local free; while free="$(df -Pk "$OUTBASE" | awk 'NR==2{print int($4/1048576)}')" && [ "$free" -lt "${MIN_FREE_GB:-20}" ]; do
  echo "   [disk] only ${free} GB free under $OUTBASE (< ${MIN_FREE_GB:-20}); waiting 5 min"; sleep 300; done; }
topup_project () { local p="$1"; [ "$p" = typescript ] && return 0
  local repo="${REPO[$p]:-}" since="${SINCE[$p]:-}" tries=0 consec=0 infra=0 qstop="" g gprev out rc
  [ -n "$repo" ] && [ -n "$since" ] || { echo "[$p] no repo/since"; return 0; }
  g="$(genuine_count "$p")"
  # resume the stop rule from the attempts already on disk (a re-run continues the same streak and backstop)
  read -r tries consec infra < <(python3 "$HERE/topup_state.py" window "$OUTBASE" "$p")
  echo "[$p] start: $g/$TARGET genuine (resumed: model-cells $tries, refusal streak $consec, infra429 $infra)"
  while [ "$g" -lt "$TARGET" ] && [ "$tries" -lt "$MAXCELLS" ] && [ "$consec" -lt "$GIVEUP" ] && [ "$infra" -lt "$INFRA_CAP" ]; do
    wait_disk; gprev="$g"
    out="$OUTBASE/month_${p}_vB_k$((g+1))_$(date +%s)_$$_$RANDOM"
    SINCE="$since" UNTIL="$UNTIL" REPO="$repo" ATT_SYS="$ATT_SYS" ATT_MODEL="$ATT_MODEL" ARMS="$ARMS" \
      REV="$REV" ATT_ATTEMPTS="$ATT_ATTEMPTS" WALL="${WALL:-3600}" \
      bash "$HERE/run_month.sh" "$p" "$out" >"${out}.live.log" 2>&1; rc=$?
    g="$(genuine_count "$p")"
    if [ "$g" -gt "$gprev" ]; then consec=0; tries=$((tries+1))   # genuine attempt: reset refusal streak, counts vs MAXCELLS
    elif is_429_cell "$out" && reset="$(python3 "$HERE/provider_reset.py" "$out")"; then   # plan quota with a stated reset: wait it out
      mkdir -p "$OUTBASE/_infra429"; mv "$out" "${out}.live.log" "$OUTBASE/_infra429/" 2>/dev/null || true
      now=$(date +%s)
      if [ $(( reset - now )) -gt "${QUOTA_MAX_WAIT:-21600}" ]; then   # e.g. a weekly limit: stop this arm; rerun the same command after the reset to resume
        echo "   [quota] provider usage limit resets $(date -u -d @"$reset" +%FT%TZ); stopping (rerun after the reset to resume)"; qstop="$(date -u -d @"$reset" +%FT%TZ)"; infra="${INFRA_CAP:-12}"
      else
        echo "   [quota] provider usage limit; sleeping until $(date -u -d @"$reset" +%FT%TZ) + 5 min, then continuing"
        [ "$reset" -gt "$now" ] && sleep $(( reset - now + 300 ))
      fi
    elif [ ! -f "$out/attacker_transcript.jsonl" ] && grep -qE 'SUBSTRATE_FAIL|WINDOW MISMATCH|FROZEN WINDOW|PIN FETCH FAIL|CLONE FAIL|No space left on device' "${out}.live.log" 2>/dev/null; then
      # the window could not be built, so no attacker ran: infrastructure, NOT a refusal, NOT a model call
      infra=$((infra+1)); mkdir -p "$OUTBASE/_substrate_fail"; mv "$out" "${out}.live.log" "$OUTBASE/_substrate_fail/" 2>/dev/null || true
    elif is_429_cell "$out"; then infra=$((infra+1))              # provider 429: infra, NOT a refusal, NOT a model call
    else consec=$((consec+1)); tries=$((tries+1)); fi             # model refusal / empty: counts vs refusal streak + MAXCELLS
    echo "   [$p] cell rc=$rc -> genuine $g (refusal $consec/$GIVEUP, infra429 $infra/$INFRA_CAP, model-cells $tries/$MAXCELLS)"
  done
  mkdir -p "$OUTBASE/.topup_end"; echo "g=$g tries=$tries consec=$consec infra=$infra" > "$OUTBASE/.topup_end/$p"
  if [ "$g" -ge "$TARGET" ]; then echo "[$p] DONE: $g/$TARGET genuine after $tries model cells"
  elif [ "$consec" -ge "$GIVEUP" ]; then echo "[$p] GIVEUP: $g/$TARGET (refusal-wall — $GIVEUP consecutive refusals) after $tries model cells"
  elif [ -n "$qstop" ]; then echo "[$p] QUOTA-STOP: $g/$TARGET (provider usage limit resets $qstop; rerun after the reset) after $tries model cells"
  elif [ "$infra" -ge "$INFRA_CAP" ]; then echo "[$p] INFRA-CAP: $g/$TARGET ($INFRA_CAP provider-429s; not a refusal wall — retry later) after $tries model cells"
  else echo "[$p] MAXCELLS: $g/$TARGET (hit $MAXCELLS model-cell backstop) after $tries cells"; fi
}
echo ">> topup model=$ATT_MODEL out=$OUTBASE prompt=$(basename "$ATT_SYS") target=$TARGET maxcells=$MAXCELLS par=$PAR"
START=$(date +%s)
for p in $LIST; do
  while [ "$(jobs -rp | wc -l)" -ge "$PAR" ]; do wait -n 2>/dev/null || sleep 2; done
  topup_project "$p" &
done
wait
MISSING=""; for p in $LIST; do [ "$p" = typescript ] && continue
  m="$OUTBASE/.topup_end/$p"; { [ -f "$m" ] && [ "$(stat -c %Y "$m")" -ge "$START" ]; } || MISSING="$MISSING $p"; done
if [ -n "$MISSING" ]; then echo ">> topup INCOMPLETE ($OUTBASE): $(echo $MISSING | wc -w) project(s) never reached a stop rule:$MISSING"; exit 1; fi
echo ">> topup complete ($OUTBASE)"
