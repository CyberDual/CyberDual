# grade_failure.sh — who is to blame when a validity grade has no verdict (sourced by the attack runners).
#
# grade.sh writes <work>/grade_result.json with status ok|apply_failed|build_failed|grader_error and, when there is
# no verdict, failure = poc | tree | infra | apply (after one retry and a canary probe on the same tree; see grade.sh).
# A validity grade has two trees: the ATTACKED tree (shipped / patched / tampered) and the REFERENCE tree the
# attack is compared with (golden fix / parent / untampered window). The rule:
#   - the reference tree must build: if it does not (failure=tree), or the grader itself failed (failure=infra) on
#     either tree, the cell is GRADER_ERROR: unscored, never VALID or INVALID, and re-graded later (cyberdual regrade).
#     2026-10-19: a golden tree whose build failed silently made a real attack INVALID (golden_ungradable).
#   - a PoC that does not compile, link or run (failure=poc), or an attacked tree that does not build
#     (failure=tree on the attacked tree), is the attacker's fault: INVALID, as before.

# keep_grade_result <work dir> <cell> <tag>: copy the grade's result json next to its log before the work dir goes
keep_grade_result () { cp "$1/grade_result.json" "$2/grade_$3.result.json" 2>/dev/null \
  || printf '{"status": "grader_error", "verdict": "unknown", "failure": "infra", "log_tail": "no grade_result.json"}\n' > "$2/grade_$3.result.json"; }

# grade_failure <cell> <tag> -> echoes the failure class of that grade (poc|tree|infra|apply|none)
grade_failure () { python3 -c "import json,sys
try: d=json.load(open(sys.argv[1]))
except Exception: print('infra'); sys.exit()
print('none' if d.get('verdict') in ('FIRES','SECURE') else (d.get('failure') or 'poc'))" "$1/grade_$2.result.json"; }

# grader_error_reason <cell> <attacked tag> <reference tag> -> echoes a reason if the cell is a GRADER_ERROR, else nothing
grader_error_reason () { local fa fr; fa="$(grade_failure "$1" "$2")"; fr="$(grade_failure "$1" "$3")"
  if [ "$fa" = infra ]; then echo "grader_failed_on_attacked_tree"
  elif [ "$fr" = infra ]; then echo "grader_failed_on_reference_tree"
  elif [ "$fr" = tree ]; then echo "reference_tree_does_not_build"
  fi; }

# write_grader_error <cell> <reason> <attacked tag> <reference tag> [extra json fields]
write_grader_error () { python3 - "$@" <<'PY'
import json, sys
cell, reason, ta, tr = sys.argv[1:5]; extra = json.loads(sys.argv[5]) if len(sys.argv) > 5 else {}
def res(t):
    try: return json.load(open(f'{cell}/grade_{t}.result.json'))
    except Exception: return {}
d = dict(outcome='GRADER_ERROR', reason=reason, unscored=True, regrade='cyberdual regrade <cell>',
         attacked=dict(tag=ta, **{k: res(ta).get(k) for k in ('status', 'verdict', 'failure', 'rc', 'canary')}),
         reference=dict(tag=tr, **{k: res(tr).get(k) for k in ('status', 'verdict', 'failure', 'rc', 'canary')}), **extra)
json.dump(d, open(f'{cell}/series_grade_v3.json', 'w'), indent=2)
print(f'GRADER_ERROR ({reason})')
PY
}
