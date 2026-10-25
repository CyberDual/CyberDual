#!/usr/bin/env python3
"""codex_output_cap.py CODEX_HOME [--cap N] [--json OUT] — output tokens of a Codex call, from its session rollouts.

Codex writes a `token_count` event after every turn with the session's running `total_token_usage` (provider counts;
output_tokens includes reasoning). The call's output is the last such total, summed over its rollout files (one per
session). With --cap: exit 0 when the total has reached the cap (the caller stops the container), else exit 1.
With --json: write the same record shape token_meter.py keeps for Claude Code calls.
"""
import argparse
import glob
import json
import os


def usage(home):
    out = inp = reasoning = 0; files = 0
    for p in glob.glob(os.path.join(home, 'sessions', '**', 'rollout-*.jsonl'), recursive=True):
        last = None
        with open(p, errors='ignore') as fh:
            for line in fh:
                if '"token_count"' not in line or 'total_token_usage' not in line:
                    continue
                try:
                    ev = json.loads(line)
                except ValueError:
                    continue
                info = (ev.get('payload') or ev).get('info') or {}
                if info.get('total_token_usage'):
                    last = info['total_token_usage']
        if last:
            files += 1
            out += last.get('output_tokens') or 0
            inp += last.get('input_tokens') or 0
            reasoning += last.get('reasoning_output_tokens') or 0
    return dict(output_tokens=out, input_tokens=inp, reasoning_output_tokens=reasoning, sessions=files)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('home'); ap.add_argument('--cap', type=int); ap.add_argument('--json')
    a = ap.parse_args()
    u = usage(a.home)
    if a.json:
        rec = dict(u, cap=a.cap, capped=bool(a.cap and u['output_tokens'] >= a.cap), source='codex rollout total_token_usage')
        with open(a.json, 'w') as fh:
            json.dump(rec, fh)
    raise SystemExit(0 if a.cap and u['output_tokens'] >= a.cap else 1)
