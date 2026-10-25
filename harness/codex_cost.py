#!/usr/bin/env python3
"""Price a Codex session from its rollout logs (the CODEX_HOME sessions/ tree).

Usage:
  codex_cost.py CODEX_HOME --model M --budget USD --stop-file F    # exit 0 iff spend >= budget (writes F)
  codex_cost.py CODEX_HOME --model M --budget USD --json [--stop-file F]   # print a JSON cost record

Each rollout's last token_count event holds cumulative totals; uncached input =
input - cached input, and reasoning tokens are already inside output tokens.
Prices are USD per 1M tokens from OpenAI's API pricing page (October 2026);
override with CODEX_PRICE_IN / CODEX_PRICE_CACHED / CODEX_PRICE_OUT. Under
ChatGPT sign-in this is the API-equivalent cost, not a charge.
"""
import argparse
import glob
import json
import os
import sys

PRICES = {'gpt-6-sol': (2.00, 0.20, 10.00), 'gpt-6.1-sol': (2.00, 0.10, 10.00)}


def usage(home):
    total = dict(input_tokens=0, cached_input_tokens=0, output_tokens=0, reasoning_output_tokens=0)
    for path in glob.glob(os.path.join(home, 'sessions', '**', 'rollout-*.jsonl'), recursive=True):
        last = None
        with open(path, errors='ignore') as fh:
            for line in fh:
                if '"token_count"' not in line:
                    continue
                try:
                    info = json.loads(line)['payload'].get('info')
                except (ValueError, KeyError, AttributeError):
                    continue
                if info:
                    last = info['total_token_usage']
        for key in total:
            total[key] += (last or {}).get(key, 0)
    return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('home')
    ap.add_argument('--model', default='gpt-6-sol')
    ap.add_argument('--budget', type=float, default=25.0)
    ap.add_argument('--stop-file')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    p_in, p_cached, p_out = PRICES.get(a.model, PRICES['gpt-6-sol'])
    p_in = float(os.environ.get('CODEX_PRICE_IN', p_in))
    p_cached = float(os.environ.get('CODEX_PRICE_CACHED', p_cached))
    p_out = float(os.environ.get('CODEX_PRICE_OUT', p_out))
    u = usage(a.home)
    uncached = u['input_tokens'] - u['cached_input_tokens']
    usd = (uncached * p_in + u['cached_input_tokens'] * p_cached + u['output_tokens'] * p_out) / 1e6
    if a.json:
        hit = bool(a.stop_file and os.path.exists(a.stop_file))
        print(json.dumps(dict(model=a.model, usage=u, uncached_input_tokens=uncached,
                              price_per_million=dict(input=p_in, cached_input=p_cached, output=p_out),
                              usd=round(usd, 4), budget_usd=a.budget, budget_hit=hit), indent=1))
        return 0
    if usd >= a.budget:
        if a.stop_file:
            with open(a.stop_file, 'w') as fh:
                fh.write(f'{usd:.2f}')
        return 0
    return 1


if __name__ == '__main__':
    sys.exit(main())
