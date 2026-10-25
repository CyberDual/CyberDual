#!/usr/bin/env python3
"""engine_window.py — the frozen V8 / SpiderMonkey maintenance windows (attack-bank/engines/windows/{v8,sm}/window.json).

An engine window lives inside a prebuilt substrate image (revbench-engine-maint:<engine>_<window tag>); window.json
records what that image must contain, so every engine attack starts from the same window and a substrate is never
rebuilt from upstream silently (run_engine_window.sh needs FORCE_SUBSTRATE=1 and then verifies the result).

  engine_window.py generate --engine v8 --image IMG [--out FILE]   read the window from IMG (docker run --network none)
  engine_window.py verify   --engine v8 --image IMG [--window FILE] exit 0 iff IMG matches window.json (count, base,
                                                                    tip, untampered tip), else print why and exit 1
  engine_window.py show     --engine v8 [--window FILE]

window.json: kind=engine, engine, image (tag), image_id (at generation), workdir, since, until, def_branch, subtree,
base (main), tip (maint-batch), orig_month (the untampered tip kept for the capture diff; == tip in a clean image),
tree, n_commits (main..maint-batch), commits (rewritten maint-batch SHAs, oldest first), candidates (the ordered
upstream candidate list the substrate build replayed, /tmp/window.txt), upstream (the upstream SHA of every replayed
commit, in order), skips (candidates that did not apply), mapping (how upstream SHAs were matched), list_sha1.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
WORKDIR = {"v8": "/src/v8", "sm": "/src/gecko-dev"}
WINDOW = {"v8": dict(since="2026-09-24", until="2026-10-01", def_branch="origin/main", subtree=None),
          "sm": dict(since="2026-09-17", until="2026-10-01", def_branch="firefox/main", subtree="js/src")}


def window_file(engine: str) -> Path:
    base = Path(os.environ.get("ENGINE_WINDOW_DIR") or ROOT / "attack-bank" / "engines" / "windows")
    return base / engine / "window.json"


def in_image(image: str, workdir: str, script: str, timeout: int = 900) -> str:
    run = subprocess.run(["docker", "run", "--rm", "--network", "none", "--entrypoint", "sh", "-w", workdir, image,
                          "-c", script], capture_output=True, text=True, timeout=timeout)
    if run.returncode:
        raise RuntimeError(f"docker run on {image} failed ({run.returncode}): {run.stderr[-400:]}")
    return run.stdout


def facts(image: str, engine: str) -> dict:
    """the window as the image holds it (refs, count, commit list)"""
    out = in_image(image, WORKDIR[engine], 'set -e; git rev-parse main maint-batch "maint-batch^{tree}"; '
                   'git rev-parse --verify -q orig_month || echo none; git rev-list --count main..maint-batch; '
                   'git symbolic-ref -q HEAD || echo detached').split()
    base, tip, tree, orig, count, head = out[:6]
    return dict(base=base, tip=tip, tree=tree, orig_month=None if orig == "none" else orig, n_commits=int(count),
                head=head)


def generate(engine: str, image: str) -> dict:
    wd = WORKDIR[engine]
    f = facts(image, engine)
    commits = in_image(image, wd, "git rev-list --reverse main..maint-batch").split()
    candidates = in_image(image, wd, "cat /tmp/window.txt 2>/dev/null || true").split()
    applied_file = in_image(image, wd, "cat /tmp/window.applied.txt 2>/dev/null || true").split()
    upstream, skips, mapping = [], [], ""
    trailers = in_image(image, wd, 'for s in $(git rev-list --reverse main..maint-batch); do '
                        'git show -s --format=%B "$s" | sed -n "s/^(cherry picked from commit \\([0-9a-f]\\{40\\}\\))$/\\1/p" | tail -1 | grep . || echo none; done').split()
    if applied_file:
        upstream, mapping = applied_file, "/tmp/window.applied.txt (recorded by the substrate build)"
        skips = [c for c in candidates if c not in set(applied_file)]
    elif len(trailers) == len(commits) and "none" not in trailers:
        upstream, mapping = trailers, "the '(cherry picked from commit <sha>)' trailer of every replayed commit"
        skips = [c for c in candidates if c not in set(trailers)]
    elif len(candidates) == len(commits):
        upstream, mapping = candidates, ("positional: every candidate in /tmp/window.txt applied (counts equal); "
                                         "upstream objects are not in the image")
    else:
        raise RuntimeError(f"{engine}: {len(candidates)} candidates, {len(commits)} commits, no way to map them")
    image_id = subprocess.run(["docker", "image", "inspect", image, "--format", "{{.Id}}"],
                              capture_output=True, text=True).stdout.strip()
    return dict(kind="engine", engine=engine, image=image, image_id=image_id, workdir=wd, **WINDOW[engine],
                base=f["base"], tip=f["tip"], orig_month=f["orig_month"], tree=f["tree"], n_commits=f["n_commits"],
                commits=commits, candidates=candidates, upstream=upstream, skips=skips, mapping=mapping,
                list_sha1=hashlib.sha1(("\n".join(commits) + "\n").encode()).hexdigest(),
                generated_utc=datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                generated_by="harness/engine/engine_window.py generate (docker run --network none, git)")


def check(meta: dict, got: dict, *, clean: bool = True) -> list[str]:
    problems = []
    for key in ("base", "tip", "n_commits"):
        if got[key] != meta[key]:
            problems.append(f"{key}: image has {got[key]}, window.json has {meta[key]}")
    if clean and got.get("orig_month") != meta["tip"]:
        problems.append(f"orig_month (untampered tip) {got.get('orig_month')} != window tip {meta['tip']}")
    if len(meta.get("commits", [])) != meta["n_commits"] or len(meta.get("upstream", [])) != meta["n_commits"]:
        problems.append("window.json is inconsistent (commits / upstream lists vs n_commits)")
    return problems


def verify(engine: str, image: str, window: Path) -> list[str]:
    if not window.is_file():
        return [f"no frozen window at {window} (generate it with engine_window.py generate)"]
    meta = json.loads(window.read_text())
    try:
        got = facts(image, engine)
    except (RuntimeError, subprocess.TimeoutExpired, ValueError) as exc:
        return [f"cannot read the window from {image}: {exc}"]
    return check(meta, got)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=("generate", "verify", "show"))
    ap.add_argument("--engine", required=True, choices=sorted(WORKDIR))
    ap.add_argument("--image")
    ap.add_argument("--window", type=Path, help="window.json (default attack-bank/engines/windows/<engine>/window.json)")
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    window = args.window or window_file(args.engine)
    if args.cmd == "show":
        print(window.read_text())
        return 0
    if not args.image:
        ap.error("--image required")
    if args.cmd == "generate":
        meta = generate(args.engine, args.image)
        out = args.out or window
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(meta, indent=1) + "\n")
        print(f"{out}: {meta['n_commits']} commits base={meta['base'][:8]} tip={meta['tip'][:8]} skips={len(meta['skips'])}")
        return 0
    problems = verify(args.engine, args.image, window)
    if problems:
        print(f"WINDOW MISMATCH {args.image} vs {window}:", *problems, sep="\n  ", file=sys.stderr)
        return 1
    print(f"window ok: {args.image} matches {window}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
