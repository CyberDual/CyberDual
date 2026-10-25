#!/usr/bin/env python3
"""Reconstruct an engine image from source diff and rebuild before probing.

result.json: rebuild OK|FAILED, failure apply|build|infra (when FAILED or the probe has no verdict), asan_instrumented,
probe (the verdict line, engine_common.grade_probe), infrastructure (true: the grader's fault, never a score). A source
image that was removed from Docker is loaded back from the engine image store first (image_store.py).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess

import engine_common as ec
from run_engine_single import grade_image, make_image_from_diff, has_asan_symbol


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", choices=("v8", "sm"), required=True)
    parser.add_argument("--source-image", required=True)
    parser.add_argument("--diff", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--base-ref", default="maint-batch")
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--result", type=Path)
    args = parser.parse_args()
    if not args.diff.is_file() or not args.diff.stat().st_size:
        parser.error("nonempty --diff required")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    output = {"engine": args.engine, "source_image": args.source_image,
              "diff": str(args.diff), "rebuilt_image": args.tag, "infrastructure": False}
    try:
        ec.ensure_image(args.source_image)
        make_image_from_diff(args.engine, args.source_image, args.diff,
                             args.tag, args.base_ref, args.log)
        output["rebuild"] = "OK"
        output["asan_instrumented"] = has_asan_symbol(args.engine, args.tag)
        if args.probe and output["asan_instrumented"]:
            output["probe"] = grade_image(args.engine, args.tag, args.probe,
                                           args.result.parent if args.result else args.log.parent,
                                           "source_rebuilt")
            if output["probe"].startswith("GRADER_ERROR"):
                output.update(infrastructure=True, failure="infra")
    except ec.BuildError as exc:
        output.update(rebuild="FAILED", failure=exc.stage, error=str(exc))
    except (ec.InfraError, subprocess.TimeoutExpired) as exc:
        output.update(rebuild=output.get("rebuild", "FAILED"), failure="infra", infrastructure=True, error=str(exc))
    except Exception as exc:
        output.update(rebuild="FAILED", failure="infra", infrastructure=True, error=str(exc))
    if args.result:
        args.result.write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps(output))
    return 0 if output["rebuild"] == "OK" and output.get("asan_instrumented") and not output["infrastructure"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
