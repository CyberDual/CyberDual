"""Regression checks for provider-quota stops: wait for a short limit, stop on a long one, never treat a quota stop as a review."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from datetime import datetime, timezone

from _provider_quota import wait_seconds

HERE = Path(__file__).resolve().parent
NOW = datetime(2026, 10, 23, 12, 0, tzinfo=timezone.utc)


def transcript(directory, text, status=429, resets_at=None):
    path = Path(directory) / "transcript.jsonl"
    events = []
    if resets_at:
        events.append({"type": "rate_limit_event", "rate_limit_info": {"status": "rejected", "resetsAt": resets_at}})
    events.append({"type": "result", "is_error": True, "result": text, "api_error_status": status})
    path.write_text("".join(json.dumps(e) + "\n" for e in events))
    return path


def run(path, **env):
    r = subprocess.run([sys.executable, str(HERE / "_provider_quota.py"), str(path)], capture_output=True, text=True,
                       env={**os.environ, **env})
    return r.returncode, r.stdout.strip()


class ProviderQuotaTest(unittest.TestCase):
    def test_session_limit_waits_to_reset(self):
        self.assertEqual(wait_seconds("session limit · resets 2pm (UTC)", 429, NOW), 2 * 3600 + 300)

    def test_dated_weekly_limit(self):
        self.assertEqual(wait_seconds("You've hit your weekly limit · resets Oct 26, 6pm (UTC)", 429, NOW),
                         (3 * 24 + 6) * 3600 + 300)

    def test_dated_limit_across_new_year(self):
        dec = datetime(2026, 12, 30, tzinfo=timezone.utc)
        self.assertEqual(wait_seconds("resets Jan 2, 1am (UTC)", 429, dec), (3 * 24 + 1) * 3600 + 300)

    def test_no_stated_reset(self):
        self.assertEqual(wait_seconds("Too many requests", 429, NOW), 600)
        self.assertEqual(wait_seconds("overloaded", 529, NOW), 120)

    def test_long_limit_stops(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out = run(transcript(d, "You've hit your weekly limit", resets_at=int(time.time()) + 3 * 86400))
            self.assertEqual(rc, 3)
            self.assertRegex(out, r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")

    def test_short_limit_waits(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out = run(transcript(d, "session limit", resets_at=int(time.time()) + 3600))
            self.assertEqual(rc, 0)
            self.assertTrue(3800 <= int(out) <= 3901)

    def test_max_wait_is_configurable(self):
        with tempfile.TemporaryDirectory() as d:
            rc, _ = run(transcript(d, "session limit", resets_at=int(time.time()) + 3600), QUOTA_MAX_WAIT="600")
            self.assertEqual(rc, 3)

    def test_finished_review_is_not_quota(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "transcript.jsonl"
            path.write_text(json.dumps({"type": "result", "is_error": False, "result": "VERDICT: APPROVE"}) + "\n")
            self.assertEqual(run(path)[0], 1)


if __name__ == "__main__":
    unittest.main()
