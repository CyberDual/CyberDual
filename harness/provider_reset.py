#!/usr/bin/env python3
"""provider_reset.py CELL -> prints the epoch (UTC) at which a provider usage quota resets, if the cell's attacker call
ended on a quota message that names its reset time; exit 1 otherwise.

Known forms: Claude Code's rejected rate_limit_event (resetsAt) or its "resets Oct 26, 6pm (UTC)" result text
(parsed by _provider_quota.py); z.ai GLM Coding Plan, "API Error: Request rejected (429) ... [1308][Usage limit reached for 5 hour. Your limit
will reset at 2026-10-15 08:57:29]" (Beijing time, UTC+8). The top-up drivers sleep until this time instead of
counting the call toward the infra cap, so a plan quota pauses a run rather than ending it.
"""
import datetime
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _provider_quota import read_events, reset_time  # noqa: E402

ZAI = re.compile(r'\[1308\]\[Usage limit reached[^\]]*?reset at (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]')


def reset_epoch(cell):
    p = os.path.join(cell, 'attacker_transcript.jsonl')
    if not os.path.isfile(p):
        return None
    with open(p, 'rb') as fh:
        fh.seek(max(0, os.path.getsize(p) - 20000))
        tail = fh.read().decode(errors='ignore')
    m = ZAI.findall(tail)
    if not m:
        try:
            r, resets_at = read_events(p)
        except OSError:
            return None
        t = reset_time(str((r or {}).get('result') or ''), datetime.datetime.now(datetime.timezone.utc), resets_at) \
            if r and r.get('is_error') else None
        return int(t.timestamp()) if t else None
    t = datetime.datetime.strptime(m[-1], '%Y-%m-%d %H:%M:%S').replace(tzinfo=datetime.timezone(datetime.timedelta(hours=8)))
    return int(t.timestamp())


if __name__ == '__main__':
    e = reset_epoch(sys.argv[1])
    if e is None:
        sys.exit(1)
    print(e)
