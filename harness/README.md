# harness/: the pipeline

Use `./cyberdual` at the repo root to run experiments (see [`../RUNNING.md`](../RUNNING.md)). It calls the
scripts below with the benchmark protocol and records each run. This page documents what those scripts do,
for debugging or for running one step by hand. The method is in [`PIPELINE.md`](PIPELINE.md), reviewer
replay in [`REVIEWING.md`](REVIEWING.md) and the Codex adapter in [`CODEX.md`](CODEX.md).

> **Answers are held out.** Never give an agent anything from `attack-bank/attacks/*/*/oracle/`, a golden
> `fix_sha`, or `attack-bank/tasks/manifest_3month.json`.

## One attempt, by hand

```bash
# window: writes one cell directory
ATT_SYS=roles/attacker.window.md ATT_MODEL=claude-opus-4-8 REV=claude-sonnet-5 \
  REPO=https://github.com/tbeu/matio SINCE=2026-07-01 UNTIL=2026-10-01 \
  bash harness/run_month.sh matio runs/manual/month_matio_vB_k1_$(date +%s)_$$_0
# single commit, task only (run_single_golden.sh: fix in hand)
ATT_MODEL=claude-opus-4-8 bash harness/run_single.sh cyberdual-single/seeds/curl__doh_https_rr_min_length.json \
  runs/manual/single_curl__doh_https_rr_min_length_k1_$(date +%s)_$$_0
# one more reviewer on a stored valid attack, and an offline grade check
REV=gpt-6-sol bash harness/rereview_any.sh <cell>
bash harness/verify_attack.sh <cell>          # cells with an attack record: harness/verify_record.sh
```

The top-up drivers `run_single_topup.sh` and `run_month_topup.sh` repeat these per task or project until
the quota is met or the stop rule fires; `cyberdual attack` sets their inputs.

## Script index

| script | role |
|---|---|
| `run_single_topup.sh`, `run_month_topup.sh`, `topup_state.py`, `provider_reset.py`, `multi_artifacts.py` | quota and stop-rule drivers (one gradeable attack per task, five per window project) |
| `run_single.sh`, `run_single_golden.sh`, `run_month.sh` | one attempt: substrate, attack, validity, scrub, optional in-run review, grade |
| `rereview_any.sh`, `custom_review.sh`, `review_cred.sh`, `review_slots.sh` | a reviewer on a stored valid attack (model or custom command) |
| `verify_attack.sh`, `verify_record.sh`, `credited_poc.py` | grade a stored attack against its control with no model call |
| `attack_record.py`, `rebuild_handoff.py`, `scrub_message.py` | the attack record (hash-chained attack -> review -> grade contract); byte-identical to the builders that wrote the bank's records |
| `agent_run.sh`, `agent_provider.sh`, `attacker_cap.py`, `token_meter.py`, `codex_output_cap.py`, `codex_cost.py` | agent dispatch (Claude Code, Codex; Kimi, GLM, Grok through Claude Code) with the output-token cap and effort pins |
| `_refusal.py`, `_budget_exhausted.py`, `_provider_quota.py`, `extract_submission.py` | refusal detection, limit labelling, provider quota stops, submission capture |
| `scrub_lib.sh`, `handoff_hygiene.sh`, `fold_capture_commit.sh`, `reviewer_diff_audit.py` | handoff scrub and leak gate, leftover-edit folding, reviewer tamper and change audit |
| `p0_build_month.sh`, `window_snapshot.py`, `window_carriers.py`, `working_set_shas.sh`, `per_commit_sizes.sh`, `project_windows.tsv`, `window_pins.tsv`, `windows/`, `check_windows.sh` | window construction, frozen windows, editable commits, per-project spans and repository URLs |
| `grade.sh`, `grade_failure.sh`, `<project>_run.sh` (the 14 libraries), `tasks/<project>/` | per-project ASan build and PoC grading; `tasks/` holds project templates (`meta.json`, `input_format.md`) and the per-grade task folders the scripts create |
| `attested/` | attested crash channel: supervisor, core helper, crash detector, self-test |
| `egress_up.sh`, `egress_proxy.py` | egress firewall: agents reach only the model endpoints |
| `export_attack_bank.py`, `bank_manifest.py` | bank export and the bank's sha256 manifest |
| `Dockerfile.agent`, `Dockerfile.grader`, `grader-packages.txt`, `Dockerfile.runner` | agent, grader and runner images |
| `test_refusal.py`, `test_reviewer_diff_audit.py`, `test_agent_run_argv.py`, `test_cell_info.py`, `testdata/` | unit tests (`python3 -m unittest`) |
