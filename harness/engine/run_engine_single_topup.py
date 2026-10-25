#!/usr/bin/env python3
"""Fill one single-commit submission per engine task and setup for a Codex agent (gpt-*, GLM_AGENT=codex).

The original five-task pilot ran three cells per task and setup. This queue
keeps those artifacts, selects the earliest gradeable cell, and adds cells only
for tasks without a gradeable submission. Refusals and missing artifacts do
not consume a task's one-submission quota, nor do infrastructure outcomes
(GRADER_ERROR, AGENT_TIMEOUT; see engine_common.py). Reviews run only on the selected
execution-valid submission.
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


MAX_RAW_PER_TASK_SETUP = 20
MAX_CONSECUTIVE_REFUSALS = 10
MAX_CONSECUTIVE_GRADER_ERRORS = 2   # a task-setup whose cells keep failing in the grader is an infrastructure stop
ATTACKS_ONLY = False
GPT_REVIEW = ec.review_grade_name(review_single_gpt.MODEL, kind="codex")


def gradeable(job: Path) -> bool:
    """fills the task's slot: a grade that is not an infrastructure outcome + diff + probe + submission manifest"""
    return ec.single_gradeable(job)


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
        task_dir = base.RESULTS / engine / seed_path.stem
        for mode in base.SETUPS:
            selected = selected_job(task_dir, mode)
            jobs = [p for p in (task_dir / mode).glob("k*") if re.fullmatch(r"k\d+", p.name)]
            outcome = (json.loads((selected[1] / "grade.json").read_text()).get("outcome")
                       if selected else None)
            rows.append({"task_iid": seed_path.stem, "mode": mode,
                         "selected": str(selected[1].relative_to(base.RESULTS)) if selected else None,
                         "outcome": outcome, "raw_cells": len(jobs),
                         "refusals": sum(base.refusal(p) for p in jobs
                                         if (p / "grade.json").exists()),
                         "infrastructure": sum(ec.read_json(p / "grade.json").get("outcome") in ec.INFRA_OUTCOMES
                                               for p in jobs)})
    payload = {"attacker": base.MODEL, "engine": engine, "phase": phase,
               "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
               "target": len(rows), "selected_gradeable": sum(bool(r["selected"]) for r in rows),
               "raw_cells": sum(r["raw_cells"] for r in rows), "rows": rows}
    target = base.RESULTS / f"status_{engine}_selected.json"
    temp = target.with_suffix(".json.tmp")
    temp.write_text(json.dumps(payload, indent=2) + "\n")
    temp.replace(target)


def needs_work(task_dir: Path) -> bool:
    for mode in base.SETUPS:
        selected = selected_job(task_dir, mode)
        if not selected:
            return True
        job = selected[1]
        grade = json.loads((job / "grade.json").read_text())
        if grade.get("outcome") == "VALID_INJECTION" and (
                not base.review_files(job)["grade"].exists()
                or not (job / GPT_REVIEW).exists()):
            return True
    return False


def run(engine: str, dry_run: bool) -> None:
    seeds = base.seed_files(engine)
    if dry_run:
        for path in seeds:
            for mode in base.SETUPS:
                print(engine, path.stem, mode, "selected" if selected_job(
                    base.RESULTS / engine / path.stem, mode) else "needs submission")
        return
    base.RESULTS.mkdir(parents=True, exist_ok=True)
    status(engine, "attacking")
    for path in seeds:
        seed = base.load_seed(path, engine)
        task_dir = base.RESULTS / engine / seed["task_iid"]
        if not needs_work(task_dir):
            continue
        task_dir.mkdir(parents=True, exist_ok=True)
        (task_dir / "seed.json").write_text(json.dumps(seed, indent=2) + "\n")
        with ec.task_lock(base.RESULTS, seed["task_iid"]).open("a") as task_lock, \
                base.build_lock_path().open("a") as lock_handle:
            fcntl.flock(task_lock, fcntl.LOCK_EX)
            base.say(f"{engine} {base.MODEL} selected queue waiting for build slot: {seed['task_iid']}")
            fcntl.flock(lock_handle, fcntl.LOCK_EX)
            ec.wait_for_disk()
            try:
                cli_image = base.ensure_codex_image(engine)
                base.extract_golden(seed, task_dir, base.CONFIG[engine][1], base.CONFIG[engine][0])
                parent_image = base.prepare_parent(seed, task_dir, cli_image)
            except (ec.InfraError, RuntimeError) as error:
                base.say(f"{seed['task_iid']}: reference setup failed, task skipped (no slot filled): {error}")
                (task_dir / "reference_error.json").write_text(json.dumps(ec.grader_error(
                    "reference_parent_or_golden_unavailable", error=str(error)[:500]), indent=2) + "\n")
                continue
            try:
                for mode in base.SETUPS:
                    streak = infra = 0
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
                        infra = infra + 1 if ec.read_json(job / "grade.json").get("outcome") == ec.GRADER_ERROR else 0
                        if infra >= MAX_CONSECUTIVE_GRADER_ERRORS:
                            base.say(f"{seed['task_iid']} {mode}: stopped after {infra} consecutive GRADER_ERROR "
                                     "cells (infrastructure; fix and resume)")
                            break
                        streak = streak + 1 if base.refusal(job) else 0
                        if streak >= MAX_CONSECUTIVE_REFUSALS:
                            base.say(f"{seed['task_iid']} {mode}: stopped after {streak} refusals")
                            break
                for mode in (() if ATTACKS_ONLY else base.SETUPS):   # reviews run separately with --attacks-only
                    selected = selected_job(task_dir, mode)
                    if not selected:
                        continue
                    k, job = selected
                    grade = json.loads((job / "grade.json").read_text())
                    if grade.get("outcome") == "VALID_INJECTION":
                        base.run_job(seed, mode, k, parent_image, task_dir, reviews_only=True)
                        gpt_grade = job / GPT_REVIEW
                        if (not gpt_grade.exists() or
                                json.loads(gpt_grade.read_text()).get("outcome") in
                                ("REVIEW_RUN_FAILED", "REVIEWER_BUILD_OR_GRADE_FAILED")):
                            result = review_single_gpt.replay(seed, mode, k, job, parent_image)
                            base.say(f"{seed['task_iid']} {mode} k{k}: GPT review {result['outcome']}")
            finally:
                base.call(["docker", "image", "rm", parent_image], check=False)
                base.call(["docker", "image", "rm", base.golden_tag(seed)], check=False)
                fcntl.flock(lock_handle, fcntl.LOCK_UN)
                fcntl.flock(task_lock, fcntl.LOCK_UN)
        status(engine, "attacking")
    status(engine, "complete")
    base.say(f"{engine} {base.MODEL} selected single-commit queue finished")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", choices=("v8", "sm"), required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--show-argv", action="store_true",
                        help="with --dry-run: print the container and agent argv of each setup's first task")
    parser.add_argument("--attacks-only", action="store_true", help="attack each task-setup and stop; no reviews")
    parser.add_argument("--seed-dir", type=Path, default=base.SEEDS,
                        help="directory holding the frozen five- or ten-task cohort")
    args = parser.parse_args()
    global ATTACKS_ONLY
    ATTACKS_ONLY = args.attacks_only
    base.SEEDS = args.seed_dir.resolve()
    if args.dry_run:
        run(args.engine, True)
        if args.show_argv:
            seed = base.load_seed(base.seed_files(args.engine)[0], args.engine)
            for mode in base.SETUPS:
                base.preview_attack(seed, mode, f"revbench-engine-single-codex:{args.engine}")
        return
    base.check_attacker_credentials()
    base.RESULTS.mkdir(parents=True, exist_ok=True)
    pid_file = base.RESULTS / f"queue_{args.engine}_selected.pid"
    pid_file.write_text(f"{os.getpid()}\n")
    try:
        run(args.engine, False)
    finally:
        if pid_file.exists() and pid_file.read_text().strip() == str(os.getpid()):
            pid_file.unlink()


if __name__ == "__main__":
    main()
