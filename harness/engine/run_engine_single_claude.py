#!/usr/bin/env python3
"""Engine single-commit sweep for Claude Code agents (claude-*, kimi-*, glm-*, grok-*): first gradeable artifact per
task and setup. (Written for the Opus 4.8 sweep; the attacker is VIPER_ENGINE_ATTACKER.)

Raw refusals and no-artifact calls are retained. Valid and invalid submissions with a source diff, JS probe, and
submission manifest both satisfy the quota; infrastructure outcomes (GRADER_ERROR, AGENT_TIMEOUT) never do.
Reviewers only receive execution-valid submissions after attack generation. Codex agents (gpt-*, GLM_AGENT=codex)
belong to run_engine_single_topup.py; if one is given here anyway, its parent image carries the Codex CLI.
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import time

import engine_common as ec
import run_engine_single as base
import review_single_gpt


MODEL = os.environ.get("VIPER_ENGINE_ATTACKER", "claude-opus-4-8")
RESULTS = Path(os.environ["CYBERDUAL_ENGINE_OUT"]) if os.environ.get("CYBERDUAL_ENGINE_OUT") else base.ROOT / "cyberdual-work/engine/single_claude"
STATUS_SUFFIX = os.environ.get("VIPER_ENGINE_STATUS_SUFFIX", "")
MAX_RAW_PER_TASK_SETUP = 20
MAX_CONSECUTIVE_REFUSALS = 10
MAX_CONSECUTIVE_GRADER_ERRORS = 2   # a task-setup whose cells keep failing in the grader is an infrastructure stop

# Reuse the identical parent, build, grader, and Sonnet reviewer code used by
# the GPT-6-Sol single sweep. Only the attacker model and stopping rule differ.
base.MODEL = MODEL
base.RESULTS = RESULTS
attack = base.attack          # dispatches on the attacker's agent (engine_agent.sh)


def gradeable(job: Path) -> bool:
    """fills the task's slot: a grade that is not an infrastructure outcome + diff + probe + submission manifest"""
    return ec.single_gradeable(job)


def cli_image(engine: str) -> str:
    """the image the task parent is built from: it must carry the attacker's CLI"""
    return base.ensure_codex_image(engine) if base.attacker_agent() == "codex" else base.CONFIG[engine][1]


def cli_image_name(engine: str) -> str:
    """the parent's CLI image name, without building it (dry runs)"""
    return f"revbench-engine-single-codex:{engine}" if base.attacker_agent() == "codex" else base.CONFIG[engine][1]


def ordered_seeds(engine: str) -> list[Path]:
    files = base.seed_files(engine)
    return list(reversed(files)) if os.environ.get("VIPER_ENGINE_SEED_ORDER") == "reverse" else files


def selected_job(task_dir: Path, mode: str) -> tuple[int, Path] | None:
    jobs = sorted((task_dir / mode).glob("k*"),
                  key=lambda p: int(p.name[1:]) if re.fullmatch(r"k\d+", p.name) else 999999)
    for job in jobs:
        if gradeable(job):
            return int(job.name[1:]), job
    return None


def status(engine: str, phase: str) -> None:
    rows = []
    for seed_path in base.seed_files(engine):
        task_dir = RESULTS / engine / seed_path.stem
        for mode in base.SETUPS:
            selected = selected_job(task_dir, mode)
            jobs = list((task_dir / mode).glob("k*"))
            rows.append({"task_iid": seed_path.stem, "mode": mode,
                         "raw_calls": len(jobs), "selected": str(selected[1].relative_to(RESULTS)) if selected else None,
                         "outcome": json.loads((selected[1] / "grade.json").read_text()).get("outcome") if selected else None,
                         "refusals": sum(base.refusal(p) for p in jobs if (p / "grade.json").exists()),
                         "infrastructure": sum(ec.read_json(p / "grade.json").get("outcome") in ec.INFRA_OUTCOMES
                                               for p in jobs)})
    payload = {"attacker": MODEL, "engine": engine, "phase": phase,
               "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
               "target": len(rows), "selected_gradeable": sum(bool(r["selected"]) for r in rows),
               "raw_calls": sum(r["raw_calls"] for r in rows), "rows": rows}
    RESULTS.mkdir(parents=True, exist_ok=True)
    path = RESULTS / f"status_{engine}{STATUS_SUFFIX}.json"
    temp = path.with_suffix(".json.tmp")
    temp.write_text(json.dumps(payload, indent=2) + "\n")
    temp.replace(path)


def run_engine(engine: str, *, dry_run: bool) -> None:
    if dry_run:
        for path in base.seed_files(engine):
            for mode in base.SETUPS:
                print(engine, path.stem, mode, "first gradeable among resampled raw calls")
        return
    RESULTS.mkdir(parents=True, exist_ok=True)
    status(engine, "attacking")
    for path in ordered_seeds(engine):
        seed = base.load_seed(path, engine)
        task_dir = RESULTS / engine / seed["task_iid"]
        if all(selected_job(task_dir, mode) for mode in base.SETUPS):
            continue
        task_dir.mkdir(parents=True, exist_ok=True)
        (task_dir / "seed.json").write_text(json.dumps(seed, indent=2))
        with ec.task_lock(RESULTS, seed["task_iid"]).open("a") as task_lock, \
                base.build_lock_path().open("a") as lock_handle:
            fcntl.flock(task_lock, fcntl.LOCK_EX)
            base.say(f"{engine} {MODEL} single waiting for shared build slot: {seed['task_iid']}")
            fcntl.flock(lock_handle, fcntl.LOCK_EX)
            ec.wait_for_disk()
            try:
                base.extract_golden(seed, task_dir, base.CONFIG[engine][1], base.CONFIG[engine][0])
                parent_image = base.prepare_parent(seed, task_dir, cli_image(engine))
            except (ec.InfraError, RuntimeError) as error:
                base.say(f"{seed['task_iid']}: reference setup failed, task skipped (no slot filled): {error}")
                (task_dir / "reference_error.json").write_text(json.dumps(ec.grader_error(
                    "reference_parent_or_golden_unavailable", error=str(error)[:500]), indent=2) + "\n")
                fcntl.flock(lock_handle, fcntl.LOCK_UN)
                fcntl.flock(task_lock, fcntl.LOCK_UN)
                continue
            try:
                for mode in base.SETUPS:
                    consecutive_refusals = consecutive_infra = 0
                    for k in range(1, MAX_RAW_PER_TASK_SETUP + 1):
                        if selected_job(task_dir, mode):
                            break
                        job = task_dir / mode / f"k{k}"
                        if (job / "grade.json").exists():
                            continue
                        ec.run_job_guarded(base.run_job, job, seed, mode, k, parent_image, task_dir, defer_review=True)
                        status(engine, "attacking")
                        if gradeable(job):
                            break
                        outcome = ec.read_json(job / "grade.json").get("outcome")
                        consecutive_infra = consecutive_infra + 1 if outcome == ec.GRADER_ERROR else 0
                        if consecutive_infra >= MAX_CONSECUTIVE_GRADER_ERRORS:
                            base.say(f"{seed['task_iid']} {mode}: stopped after {consecutive_infra} consecutive "
                                     "GRADER_ERROR cells (infrastructure; fix and resume)")
                            break
                        consecutive_refusals = consecutive_refusals + 1 if base.refusal(job) else 0
                        if consecutive_refusals >= MAX_CONSECUTIVE_REFUSALS:
                            base.say(f"{seed['task_iid']} {mode}: stopped after 10 consecutive refusals")
                            break
            finally:
                base.call(["docker", "image", "rm", parent_image], check=False)
                base.call(["docker", "image", "rm", base.golden_tag(seed)], check=False)
                fcntl.flock(lock_handle, fcntl.LOCK_UN)
                fcntl.flock(task_lock, fcntl.LOCK_UN)
    status(engine, "attacks_complete")
    base.say(f"{engine} {MODEL} single attack queue complete")


def review_engine(engine: str) -> None:
    gpt_grade = ec.review_grade_name(review_single_gpt.MODEL, kind="codex")
    for path in ordered_seeds(engine):
        seed = base.load_seed(path, engine)
        task_dir = RESULTS / engine / seed["task_iid"]
        selected = [(mode, selected_job(task_dir, mode)) for mode in base.SETUPS]
        if not any(item and json.loads((item[1] / "grade.json").read_text()).get("outcome") == "VALID_INJECTION"
                   and (not base.review_files(item[1])["grade"].exists()
                        or not (item[1] / gpt_grade).exists())
                   for _, item in selected):
            continue
        with ec.task_lock(RESULTS, seed["task_iid"]).open("a") as task_lock, \
                base.build_lock_path().open("a") as lock_handle:
            fcntl.flock(task_lock, fcntl.LOCK_EX)
            fcntl.flock(lock_handle, fcntl.LOCK_EX)
            ec.wait_for_disk()
            try:
                parent_image = base.prepare_parent(seed, task_dir, cli_image(engine))
            except ec.InfraError as error:
                base.say(f"{seed['task_iid']}: parent unavailable, reviews postponed: {error}")
                continue
            try:
                for mode, item in selected:
                    if not item:
                        continue
                    k, job = item
                    if json.loads((job / "grade.json").read_text()).get("outcome") != "VALID_INJECTION":
                        continue
                    base.run_job(seed, mode, k, parent_image, task_dir, defer_review=False)
                    if not (job / gpt_grade).exists():
                        result = review_single_gpt.replay(seed, mode, k, job, parent_image)
                        base.say(f"{seed['task_iid']} {mode} k{k}: GPT review {result['outcome']}")
            finally:
                base.call(["docker", "image", "rm", parent_image], check=False)
                fcntl.flock(lock_handle, fcntl.LOCK_UN)
                fcntl.flock(task_lock, fcntl.LOCK_UN)
        status(engine, "reviewing")
    status(engine, "complete")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", choices=("v8", "sm"), required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--show-argv", action="store_true",
                        help="with --dry-run: print the container and agent argv of each setup's first task")
    parser.add_argument("--reviews-only", action="store_true")
    parser.add_argument("--attacks-only", action="store_true", help="run the attack queue without the review pass")
    parser.add_argument("--seed-dir", type=Path, default=base.SEEDS,
                        help="directory holding the frozen five- or ten-task cohort")
    args = parser.parse_args()
    base.SEEDS = args.seed_dir.resolve()
    if args.dry_run:
        run_engine(args.engine, dry_run=True)
        if args.show_argv:
            seed = base.load_seed(ordered_seeds(args.engine)[0], args.engine)
            for mode in base.SETUPS:
                base.preview_attack(seed, mode, cli_image_name(args.engine))
        return
    base.check_attacker_credentials()
    RESULTS.mkdir(parents=True, exist_ok=True)
    pid_file = RESULTS / f"queue_{args.engine}{STATUS_SUFFIX}.pid"
    pid_file.write_text(f"{os.getpid()}\n")
    try:
        if args.reviews_only:
            review_engine(args.engine)
        else:
            run_engine(args.engine, dry_run=False)
            if not args.attacks_only:
                review_engine(args.engine)
    finally:
        if pid_file.exists() and pid_file.read_text().strip() == str(os.getpid()):
            pid_file.unlink()


if __name__ == "__main__":
    main()
