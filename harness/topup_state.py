#!/usr/bin/env python3
"""Rebuild a top-up unit's stop-rule state from the attempts already on disk.

  topup_state.py single OUTBASE PREFIX TASK     ->  "<tries> <consec> <infra>"
  topup_state.py window OUTBASE PROJECT         ->  "<tries> <consec> <infra>"

Replays the unit's attempts in launch order with the drivers' own rules: a gradeable attempt resets the refusal
streak and counts as a model attempt; a provider rate-limit / usage-limit failure counts only toward the infra cap;
any other attempt adds to the refusal streak and counts as a model attempt. So a re-run resumes the stop rule
exactly where it was instead of restarting the streak at zero.
"""
import glob, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def is_429(cell):
    p = os.path.join(cell, 'attacker_transcript.jsonl')
    if not os.path.isfile(p):
        return False
    with open(p, 'rb') as fh:
        fh.seek(max(0, os.path.getsize(p) - 6000))
        t = fh.read().decode(errors='ignore')
    return ('"is_error":true' in t and re.search(r'429|usage limit|rate.?limit', t, re.I) is not None
            and re.search(r'cyber|safeguard', t, re.I) is None)


def series(cell):
    try:
        return json.load(open(os.path.join(cell, 'series_grade_v3.json')))
    except (OSError, ValueError):
        return None


def genuine_single(cell):
    g = series(cell)
    if not g or g.get('outcome') not in ('VALID_SERIES', 'INVALID_SERIES', 'GRADER_ERROR'):   # GRADER_ERROR holds the slot (regraded later)
        return False
    d, pc = os.path.join(cell, 'roleA.diff'), os.path.join(cell, 'pocs')
    return (os.path.isfile(d) and os.path.getsize(d) > 0 and os.path.isdir(pc)
            and any(os.path.isfile(os.path.join(pc, n)) and os.path.getsize(os.path.join(pc, n)) > 0 for n in os.listdir(pc)))


def genuine_window(cell):
    from multi_artifacts import genuine
    g = series(cell)
    return bool(g and g.get('outcome') in ('VALID_SERIES', 'INVALID_SERIES', 'GRADER_ERROR') and g.get('terminal_state') != 'GRADER_STARVED'
                and genuine(cell))


def main():
    kind = sys.argv[1]
    if kind == 'single':
        base, prefix, task = sys.argv[2:5]
        cells = glob.glob(f'{base}/{prefix}_{task}_k*'); ok = genuine_single
    else:
        base, project = sys.argv[2:4]
        cells = glob.glob(f'{base}/month_{project}_vB_k*'); ok = genuine_window
    cells = [c for c in cells if os.path.isdir(c) and series(c) is not None]      # finished attempts only

    def epoch(c):
        m = re.search(r'_k\d+_(\d+)_', os.path.basename(c))
        return int(m.group(1)) if m else 0
    tries = consec = infra = 0
    for c in sorted(cells, key=epoch):
        if ok(c):
            consec = 0; tries += 1
        elif is_429(c):
            infra += 1
        else:
            consec += 1; tries += 1
    print(tries, consec, infra)


if __name__ == '__main__':
    main()
