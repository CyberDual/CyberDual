# Prompts (attacker = role A, reviewer = role B)

One prompt per role and setup.
`./cyberdual attack` and `./cyberdual review` select them; the drivers also accept `ATT_SYS` / `REV_SYS`
(repo-relative or absolute paths) to run a different prompt on purpose.

| setup | attacker | reviewer | driver |
|---|---|---|---|
| single commit, task only (`--setup scratch`, `single_from_scratch`) | `attacker.task-only.md` (new runs); `attacker.task-only.earlier.md` (see below) | `reviewer.single.md` | `harness/run_single.sh` |
| single commit, fix in hand (`--setup golden`, `single_golden_given`) | `attacker.fix-in-hand.md` | `reviewer.single.md` | `harness/run_single_golden.sh` |
| window, defect over at least two commits (`--setup split`, `multi_required`) | `attacker.window.md` | `reviewer.window.md` | `harness/run_month.sh` |

- `vuln_type.md` (the in-scope vulnerability types) is not a prompt of its own: `harness/run_month.sh` appends it
  to every window attacker task, and also the project's `harness/tasks/<project>/input_format.md` when present.
- The task message each agent receives (repository layout, required outputs) is written by the driver. Each cell
  keeps a copy of the system prompt it ran with (`attacker.system.md`, `role_a.month.system.md`,
  `reviewer.system.md`) and its file name (`attacker_prompt.txt`, `reviewer_prompt.txt`).
- `cyberdual review` on a bank attack uses the reviewer prompt stored in its attack record
  (`attack_record/reviewer.system.md`) when the record has one, else the reviewer prompt above for its kind.

V8 and SpiderMonkey (`./cyberdual engine attack`, `harness/engine/`):

| setup | attacker | reviewer | driver |
|---|---|---|---|
| task only (`scratch`) | `attacker.engine-task-only.md` | `reviewer.engine.md` | `harness/engine/run_engine_single*.py` |
| fix in hand (`golden`) | `attacker.engine-fix-in-hand.md` | `reviewer.engine.md` | `harness/engine/run_engine_single*.py` |
| window (`multi_required`) | `attacker.engine-window.md` | `reviewer.engine.md` | `harness/engine/run_multi_quota.py` |

## Prompts of the attacks in the bank

| setup | attacks | attacker prompt |
|---|---|---|
| window (`multi_required`) | 427 | `attacker.window.md` |
| fix in hand (`single_golden_given`) | 312 | `attacker.fix-in-hand.md` |
| task only (`single_from_scratch`) | 349 | `attacker.task-only.earlier.md` for the 119 Opus 4.8, Sonnet 5 and GPT-6-Sol attacks; `attacker.task-only.md` for the 230 Grok 4.7, GLM 5.3 (cx and cc) and Kimi K3 attacks |

`./cyberdual attack --setup scratch` uses `attacker.task-only.md`. The two task-only prompts differ in their Regime A
instructions: `attacker.task-only.earlier.md` gives no reference PoC and allows only a new defect (`new_vuln`), while
`attacker.task-only.md` also describes an incomplete fix of a reference PoC.

Reviewer prompts of the 944 attacks with an attack record: 592 records include `reviewer.window.md` or
`reviewer.single.md`, 226 include no prompt (they are reviewed with the prompt above for their kind), and 126 records
(Opus 4.8, Sonnet 5 and GPT-6-Sol attackers) include a shorter reviewer prompt, which `cyberdual review` uses.
