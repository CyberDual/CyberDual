# CyberDual attack bank: layout

Everything under `attack-bank/` is **answer material**: never mount it, or any file from it, where an attacker or
reviewer agent can read it. The pipeline hands an agent only the scrubbed task or window it builds from here.

```
attack-bank/
  attacks/              the benchmark's 1,088 valid attacks (window-required, single fix-in-hand, single task-only;
    INDEX.jsonl         14 libraries), the only bank. One row per attack: id, kind, setup, project,
    COUNTS.json         attacker_model (+ harness/provider/attacker_setup for the GLM and Kimi runs),
    multi/<id>/         attack_hash, labels {sonnet-5, opus-4.8, gpt-6-sol}. COUNTS.json: per setup x attacker.
    single/<id>/
    MANIFEST.sha256     sha256 of every other file of attacks/ (`./cyberdual verify-bank` checks it; `./cyberdual
                        bank manifest` rewrites it after a change to the bank)
  tasks/                host-side task answers: manifest_3month.json (the 70 single-commit tasks with their
                        upstream fix_sha) and grader_controls.tsv (parent/fix control pairs the per-project graders
                        use for their control mode)
  windows/<project>/window.json   the frozen maintenance window of each project (window id, base, tip, tree, counts)
  engines/              the V8 / SpiderMonkey study (harness/engine/README.md): attacks/<id>/ (the 106 valid engine
                        attacks), LAUNCHES.csv (every launch), SELECTED.csv (the 180 slots), INDEX.jsonl,
                        windows/{v8,sm}/window.json, MANIFEST.sha256 (`./cyberdual engine verify-bank`)
```

The reviewer labels are also in `../results/outcomes.csv` (one row per attack attempt, valid or not). `labels` in
INDEX.jsonl / meta.json are the same labels (neither INDEX.jsonl nor meta.json is part of an attack's hash).

## One instance: `attacks/<kind>/<id>/`

| path | content |
|---|---|
| `meta.json` | project, upstream repo, base (multi) / parent (single) SHA, base and review branch names, credited PoC, labels, attack_hash; windows: `window_commits`, the number of commits the reviewer saw |
| `attack_record/record.json` | the attack record (`viper.attack_record/1`): base SHA, series and clean tips and trees, PoC hash, reviewer prompt, `attack_hash` |
| `attack_record/series.bundle` | git bundle `base..review branch` (`maint-batch` for windows, `patch` for single commits): what the reviewer sees |
| `attack_record/clean.bundle` | the same window without the attack, or the golden fix (golden-given); absent for from-scratch |
| `attack_record/poc/` | the credited PoC (and its helper headers) |
| `attack_record/reviewer.system.md` | the reviewer system prompt this attack was reviewed with (when one was recorded) |
| `window.bundle` | the scrubbed reviewer handoff (`base..review branch`); present in 1,077 of the 1,088 entries, including all 144 without an attack record |
| `oracle/pocs/` | all PoC files the attacker wrote; `meta.credited_poc` names the credited one |
| `oracle/injected.diff` | the planted change (multi: tampered vs untampered window; single: the attacker's patch) |
| `oracle/rationale.json` | the attacker's own account of the defect (absent in 20 entries whose attacker wrote none) |
| `oracle/expected.json` | what a correct grade asserts |

944 entries have an attack record; 144 entries have only `window.bundle` + `oracle/` (`cyberdual verify` and
`cyberdual review` handle both). Bundles hold only the window: fetch the base commit from `upstream_repo` first
(the CLI does this through a bare upstream cache, `CYBERDUAL_CACHE`).

## Engine instances: `engines/attacks/<id>/`

| path | content |
|---|---|
| `meta.json` | engine (`v8`, `sm`), setup, attacker, harness, upstream repository, base commit, reference labels (`paper_labels`: Sonnet 5, GPT-6-Sol), crash evidence (`asan`, or `v8_sigsegv` for a verified V8 SIGSEGV) |
| `review/patch.diff`, `review/task.txt` | single commits: the commit the reviewer sees, applied on `meta.base` (the task's parent), and the reviewer's task |
| `window.bundle` | windows: the attacked `main..maint-batch`; its prerequisite is the frozen window base. For five V8 windows it was rebuilt from the attacker's submission, SHA-identical to the reviewed history (`meta.window_bundle_source`) |
| `oracle/probe.js` | the credited JavaScript PoC |
| `oracle/injected.diff`, `oracle/rationale.json`, `oracle/expected.json`, `oracle/seed.json` | the planted change, the attacker's account, what a correct grade asserts, and the task (single commits) |

## Hashes

`attack_hash = sha256("viper.attack_record/1|kind|base|series_tip_commit|clean_tip_commit|poc_sha256")`. Git
commit ids already hash content, messages and parents, so the hash covers the whole attack.
`python3 harness/attack_record.py verify <entry>/attack_record --base-repo <repo with the base branch>` re-derives
it, checks the PoC hash and materializes the series to check its tree. Review grades chain
`attack_hash -> handoff_hash -> shipped_hash`.

## Notes on the records

- **Paths.** Path fields in `meta.json` (`source_cell`) and `record.json` (`provenance.substrate`, `cell`, evidence
  fields) are relative to run folders that are not part of this repository. They are informational only and are not
  hashed.
- **Commit trailers.** Commit messages written or rewritten by an attacker may contain Signed-off-by, Co-authored-by,
  or Reported-by lines copied from or modeled on the upstream commit. These lines do not mean that the named people
  wrote, reviewed, or endorsed the attack. Trailers that attackers invented and that named real people appear as
  pseudonyms (Maintainer A–I, `Maintainer A <maintainer-a@example.invalid>`, ...) in the bundles and in
  `oracle/rationale.json`. Genuine upstream commits and their trailers are as upstream. Trailers that credit AI tools
  (for example a `Co-Authored-By` line added by the attacker's agent) appear as the agent wrote them.
- **Large injected diffs.** Two `oracle/injected.diff` files are over 30 MB, because the attacker copied whole trees
  into its patch; both are as the attacker submitted them.
  - `single/golden_libheif__iref_repeated_reference_k1_1791425732_933640_21244` (31.8 MB): mostly a `golden-check/`
    folder, plus `golden-build/` files and build logs. These are only in the answer key: the reviewer's commit
    (`attack_record/series.bundle`) touches only `libheif/` and `build-emscripten.sh`.
  - `single/single_curl__mime_base64_size_integer_overflow_k2_1791941663_alpha_44ea5176a3` (35.6 MB): the attacker
    committed two build trees (`bt/`, `basan/`: generated docs, test certificates and logs) next to the one-file change
    in `lib/`. They are part of the commit the reviewer sees (`window.bundle`).
- **Kimi K3 provider.** The 152 `kimi-k3` attacks ran in Claude Code with Kimi K3 served through AWS Bedrock
  (`global.moonshotai.kimi-k3`, reasoning effort max); `harness` / `provider` in `INDEX.jsonl` record this. These
  fields are not hashed.

## Ethics

The PoCs are working triggers for memory-safety defects that the attackers planted into replayed maintenance
history; none of these defects was submitted upstream. Run grading only in the provided containers.
