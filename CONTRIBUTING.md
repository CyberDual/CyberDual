# Contributing to CyberDual

CyberDual is an arena: every new attacker, reviewer, or task makes the leaderboard more useful. This guide explains
how to contribute results, how we check them, and how to propose new tasks. Send contributions and private questions to
**cy54@illinois.edu** and **hwiwonl2@illinois.edu**; public questions can also go to [GitHub issues](https://github.com/CyberDual/CyberDual/issues).

## What you can contribute

An entry is a full attacker or reviewer setup, and any part of it can be the contribution:

| Contribution | Example |
|---|---|
| **Model** | a frontier, unreleased, or open-weights model in an existing harness (Claude Code, Codex) |
| **Prompt** | a new attacker or reviewer prompt for an existing model and harness, such as a stealthier attack strategy or a stricter review checklist |
| **Harness** | your own agent scaffold: its tools, its loop, and how it reads the repository, tests code, and edits it |
| **Agent swarm** | several agents working together, such as planners, coders, and critics that plant a bug as a team, or a panel of reviewers that cross-check a change |

**What the CLI runs.** `./cyberdual attack` and `./cyberdual review` run models in Claude Code or Codex, chosen by the
model name: `gpt-*`, `o<N>` and `codex*` run in Codex; `claude-*`, `kimi-*`, `glm-*` and `grok-*` run in Claude Code
(Kimi, GLM and Grok through their providers' endpoints, [RUNNING.md](RUNNING.md)); any other name runs in Claude Code
against the Anthropic API. A reviewer of any kind, including your own harness or swarm, plugs in through
`--reviewer custom --cmd ...` (README, *Your own reviewer*). An attacker harness or swarm has no plug-in point yet, and
neither do hand-built attacks or a model that cannot leave your infrastructure: email us before you start, and we will
agree on how to run, grade and report them.

## Play both roles

CyberDual is a duel, so an entry plays **both roles** with the same model, harness, prompt, and swarm setup:

| Role | You run | You send |
|---|---|---|
| **Attacker** | `./cyberdual attack` on the 70 single-commit tasks and the 14 windows, then `./cyberdual bank export` | the exported bank (attacks with PoCs) |
| **Reviewer** | `./cyberdual review` on all 1,088 frozen attacks, then `./cyberdual score` | `outcomes.csv` |

We review your attacks with the three reference reviewers, so your attack success rate is comparable with the
README table, and your catch rate places you on the reviewer leaderboard. The entry then appears on both leaderboards
and in the attacker-versus-reviewer chart. An entry that can only play one role (for example a review pipeline) may
contribute that role alone; it is listed for that role only.

**New tasks** are a separate contribution: see [Proposing new tasks](#proposing-new-tasks).

## Rules for leaderboard entries

- **Use the released protocol.** Run the shipped commands with their defaults: the 250k output-token cap per agent
  call, one review per attack (pass@1), and no network for reviewers beyond their model endpoint (model reviewers reach
  only it; custom reviewers get none) unless your entry is tagged *Modified setup*. Use the released prompts (`roles/`) unless your entry contributes its own. A new prompt, harness, or agent
  swarm is a normal entry as long as its agents get only the inputs the harness gives them; a custom reviewer runs with
  `--reviewer custom --cmd ...` and gets only the handoff.
- **Agent swarms.** The token cap applies to each agent call. Report the total tokens and cost across all agents, and
  keep every agent's trajectory.
- **No answer material.** Agents must never see `attack-bank/` files, the PoCs, `results/`, upstream fix commits,
  or this repository's history. Do not tune on the attacks you report: a prompt or harness developed on the bank must
  say so in its metadata.
- **Cover everything.** The attacker run covers all 70 tasks and 14 windows; the reviewer run covers all 1,088 attacks
  (a `--subset` review is welcome as a *Partial* entry). Refusals, errors, and timeouts stay in the results; they are
  scored as the protocol defines.
- **Keep the trajectories.** Every agent call's transcript must be kept as `./cyberdual` writes it, and linked from
  the contribution. We do not rank entries without trajectories.
- **One configuration, one entry.** Contribute the configuration you stand behind; do not send several variants of
  one model, prompt, harness, or swarm and keep the best.

## Contributing a result

A contribution has these parts:

| Part | What to include |
|---|---|
| **Model** | every model used: name and version, provider or weights, reasoning effort, open or closed weights |
| **Harness** | the agent harness and version (Claude Code, Codex, or your own), its tools, and network access; your own harness's code or a link to it |
| **Prompt** | every system prompt you used, verbatim (or "default" if you used `roles/` unchanged) |
| **Agent swarm** | for a multi-agent system: each agent, its role, its model and prompt, and how the agents coordinate; the code or a link to it |
| **Trajectories** | the transcript of every agent call, for every agent in the system, as `./cyberdual` writes it in the run folder |

Plus the results of both roles: the exported bank from the attacker run and `outcomes.csv` from the reviewer run.

1. **Run both roles.** For example, with Claude Code:
   ```bash
   # attacker: the 70 tasks and 14 windows, then export the valid attacks
   for s in scratch golden split; do ./cyberdual attack --setup $s --attacker <model-id> --out runs/attack_<name>_$s; done
   ./cyberdual bank export --run runs/attack_<name>_scratch runs/attack_<name>_golden runs/attack_<name>_split \
     --name <name> --out banks/<name>
   # reviewer: the 1,088 frozen attacks (or --reviewer custom --cmd "<your reviewer>")
   ./cyberdual review --reviewer claude-code --model <model-id> --out runs/review_<name>
   ./cyberdual score runs/review_<name>          # writes runs/review_<name>/outcomes.csv and prints the metrics
   ```
2. **Package.** Copy [`submissions/TEMPLATE/`](submissions/TEMPLATE) to a folder named `<YYYY-MM-DD>_<name>`, fill
   in `metadata.yaml` (contribution type, model, harness, prompt, swarm, cost), add `outcomes.csv` and the exported bank,
   the prompt files, the harness or swarm code (or a link), and the trajectories (or a download link if they are large), and write a short `README.md` on anything that differs
   from the defaults.
3. **Email** the folder (or a download link) to **cy54@illinois.edu** and **hwiwonl2@illinois.edu** with the subject
   `[CyberDual contribution] <name>`. Say whether it may be published now or must stay private until a release date
   (at most 90 days out). We publish a pre-release result only once the model or system is publicly available, and
   not if the released model differs from the one evaluated. You may withdraw before publication.

## How we check a contribution

1. **Recompute.** `./cyberdual score` on your `outcomes.csv` must reproduce the numbers you claim.
2. **Regrade a sample.** On a random sample of at least 50 attacks or 10%, whichever is larger, we regrade your
   attacks' validity with `./cyberdual verify --bank <your bank>` (attacker track), and rebuild your shipped trees
   from the review folder you send and rerun each attack's PoC (reviewer track). Any mismatch blocks the entry until
   it is explained.
3. **Read the trajectories.** We audit a sample for answer leakage (reading bank files, fetching upstream fixes),
   tampering with the grader or sanitizers, and protocol changes.
4. **Attacker entries** are reviewed by the three reference reviewers, so their ASR is comparable with the table in
   the README.

Entries that pass steps 1–3 are listed as **Self-reported**. Entries that we rerun ourselves (or rerun on a sample
with matching results) get the **Verified** badge.

## Leaderboard tags

Each row shows: Verified or Self-reported; open or closed weights; harness (and whether it is open source); reasoning
effort; date; benchmark version (attack bank and window quarter); cost per attack; and *Modified setup* or *Partial*
when the run deviates from the protocol or covers a subset (such rows are listed but not ranked).

## Proposing new tasks

Email a short proposal to **cy54@illinois.edu** and **hwiwonl2@illinois.edu** before building anything (project, why it is a good target, the window or fix you have in mind). A good task:

- comes from an actively maintained open-source C/C++ project with a public API that parses untrusted input;
- builds under AddressSanitizer in the grader image, pinned to a commit, with its test suite passing;
- has a recent maintenance window (50–199 non-merge commits ending at the benchmark's cutoff date) or, for a
  single-commit task, a recent upstream fix with a short defect report;
- passes the **oracle check** (a reference injected defect with a PoC that crashes the attacked tree and not the
  clean tree) and the **no-op check** (the unchanged tree never crashes under the PoC harness);
- has a license that allows redistributing the snapshot.

Accepted tasks enter the next quarterly window refresh as a new benchmark version; earlier versions stay frozen and
scoreable.

## Harness and prompt contributions

Bug fixes and portability improvements to `harness/` and `./cyberdual` are welcome as ordinary pull requests. Changes
that alter what an agent sees or how a run is graded (prompts, caps, the grader, the crash channel) change the
benchmark; propose them in an issue first, since they need a new benchmark version.

## Responsible disclosure

CyberDual's attacks exist only in sandboxed copies of upstream projects. Never propose a planted defect upstream.

If a run reveals a memory-safety bug that is **not** planted by a CyberDual attack (it reproduces on the clean
upstream tree), do not publish it, include it in a public pull request, or leave it in public trajectories. Report it
privately to the project's security contact (its `SECURITY.md`, a GitHub private advisory, or its security list) and
copy cy54@illinois.edu and hwiwonl2@illinois.edu. We follow a 90-day disclosure window, as OSS-Fuzz does: details become public after 90 days, or
earlier once a fix ships. Affected trajectories stay private until then.

## License of contributions

By contributing, you agree that your outcomes, metadata, and trajectories are released under CC BY 4.0 (see
[LICENSE-DATA](LICENSE-DATA)) and your code under the MIT license (see [LICENSE](LICENSE)), and you confirm that you may
publish the model outputs you contribute.
