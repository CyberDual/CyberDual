#!/usr/bin/env python3
"""Did a Claude reviewer session end on a provider quota or rate limit? If so, how long to wait before retrying.

  python3 _provider_quota.py <transcript.jsonl>
    exit 0: a quota stop that resets soon; prints the seconds to wait before retrying
    exit 3: a quota stop that resets later than QUOTA_MAX_WAIT seconds (default 6 h, e.g. a weekly limit);
            prints the reset time (UTC, ISO 8601). The caller stops and records the review as PROVIDER_QUOTA.
    exit 1: not a quota stop

A quota stop is a final `result` event with is_error=true whose status or text names a limit (HTTP 429/529,
"session limit", "usage limit", "rate limit", "overloaded"). Such a session never reviewed anything: the caller
must retry it (after the wait) rather than grade the untouched tree. The wait runs to the reset time plus 5 minutes:
the epoch in Claude Code's rejected `rate_limit_event` (resetsAt) when present, else the time the message states
("resets 9:20am (UTC)", "resets Oct 26, 6pm (UTC)"); without either, 10 min for a 429 and 2 min for overload.
"""
import json, os, re, sys
from datetime import datetime, timedelta, timezone

LIMIT = re.compile(r'session limit|usage limit|weekly limit|rate.?limit|overloaded|too many requests', re.I)
MONTHS = {m: i for i, m in enumerate(('jan', 'feb', 'mar', 'apr', 'may', 'jun', 'jul', 'aug', 'sep', 'oct', 'nov', 'dec'), 1)}
DATED = re.compile(r'resets\s+([A-Za-z]{3})[a-z]*\.?\s+(\d{1,2}),?\s+(?:at\s+)?(\d{1,2})(?::(\d{2}))?\s*(am|pm)?\s*(?:\(([^)]+)\))?', re.I)
RESET = re.compile(r'resets\s+(\d{1,2})(?::(\d{2}))?\s*(am|pm)?\s*(?:\(([^)]+)\))?', re.I)


def decisive_result(results):
    """the result event that says how the session ended: the last one that is an error or carries text. A session
    can emit several result events (it resumes when background subagents report); trailing empty ones from
    those resumptions do not undo an earlier provider error."""
    d = None
    for e in results:
        if e.get('is_error') or str(e.get('result') or '').strip():
            d = e
    return d or (results[-1] if results else None)


def read_events(path):
    """the result events and the latest rejected rate_limit_event's resetsAt (epoch seconds, or None)"""
    res, resets_at = [], None
    for line in open(path, errors='replace'):
        line = line.strip()
        if not line.startswith('{') or not ('"result"' in line or '"rate_limit_event"' in line):
            continue
        try:
            e = json.loads(line)
        except ValueError:
            continue
        if e.get('type') == 'result':
            res.append(e)
        elif e.get('type') == 'rate_limit_event':
            info = e.get('rate_limit_info') or {}
            if info.get('status') == 'rejected' and isinstance(info.get('resetsAt'), (int, float)):
                resets_at = int(info['resetsAt'])
    return decisive_result(res), resets_at


def final_result(path):
    return read_events(path)[0]


def _zone(tz):
    try:
        from zoneinfo import ZoneInfo
        return timezone.utc if tz.strip().upper() in ('UTC', 'GMT') else ZoneInfo(tz.strip())
    except Exception:
        return timezone.utc


def _hour(h, ap):
    return h % 12 + (12 if ap == 'pm' else 0) if ap else h


def reset_time(text, now, resets_at=None):
    """the stated reset (aware datetime), or None"""
    if resets_at:
        return datetime.fromtimestamp(resets_at, timezone.utc)
    m = DATED.search(text or '')
    if m and m.group(1).lower()[:3] in MONTHS:
        zone = _zone(m.group(6) or 'UTC')
        local = now.astimezone(zone)
        try:
            t = local.replace(month=MONTHS[m.group(1).lower()[:3]], day=int(m.group(2)),
                              hour=_hour(int(m.group(3)), (m.group(5) or '').lower()), minute=int(m.group(4) or 0),
                              second=0, microsecond=0)
        except ValueError:
            return None
        return t if t > local - timedelta(days=180) else t.replace(year=t.year + 1)   # "resets Jan 2" seen in December
    return None


def wait_seconds(text, status, now=None, resets_at=None):
    now = now or datetime.now(timezone.utc)
    t = reset_time(text, now, resets_at)
    if t is not None:
        return max(int((t - now).total_seconds()), 0) + 300
    m = RESET.search(text or '')
    if m:
        h, mi, ap, tz = int(m.group(1)), int(m.group(2) or 0), (m.group(3) or '').lower(), (m.group(4) or 'UTC')
        h = _hour(h, ap)
        local = now.astimezone(_zone(tz))
        reset = local.replace(hour=h, minute=mi, second=0, microsecond=0)
        if reset <= local:
            reset += timedelta(days=1)
        wait = (reset - local).total_seconds()
        # a session window is 5 h: a reset more than 6 h away means the stated time has just passed (clock skew)
        return int(wait) + 300 if wait <= 6 * 3600 else 300
    return 600 if str(status) == '429' else 120


def main():
    try:
        r, resets_at = read_events(sys.argv[1])
    except OSError:
        return 1
    if not r or not r.get('is_error'):
        return 1
    text, status = str(r.get('result') or ''), r.get('api_error_status')
    if not (str(status) in ('429', '529') or LIMIT.search(text)):
        return 1
    now = datetime.now(timezone.utc).replace(microsecond=0)
    wait = wait_seconds(text, status, now, resets_at)
    if wait > int(os.environ.get('QUOTA_MAX_WAIT', 6 * 3600)):
        print((now + timedelta(seconds=wait - 300)).strftime('%Y-%m-%dT%H:%M:%SZ'))
        return 3
    print(wait)
    return 0


if __name__ == '__main__':
    sys.exit(main())
