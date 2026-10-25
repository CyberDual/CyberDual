"""Regression checks for reviewer source-change and tamper classification."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("reviewer_diff_audit.py")


def git(repo, *args):
    subprocess.run(["git", "-C", str(repo), *args], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def classify(mode, handoff, review):
    return subprocess.check_output(
        [sys.executable, str(SCRIPT), mode, str(handoff), str(review)],
        text=True,
    ).strip()


class ReviewerDiffAuditTest(unittest.TestCase):
    def test_report_and_build_output_are_not_source_edits_or_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            handoff = Path(tmp) / "handoff"
            handoff.mkdir()
            git(handoff, "init", "-q")
            git(handoff, "config", "user.name", "Test")
            git(handoff, "config", "user.email", "test@example.org")
            (handoff / "CMakeLists.txt").write_text("project(example)\n")
            git(handoff, "add", "-A")
            git(handoff, "commit", "-qm", "handoff")
            review = Path(tmp) / "review"
            # Git may create transient maintenance locks while writing objects;
            # clone through Git instead of copying its live .git directory.
            subprocess.run(["git", "clone", "-q", "--no-hardlinks", str(handoff), str(review)],
                           check=True)
            git(review, "config", "user.name", "Test")
            git(review, "config", "user.email", "test@example.org")
            (review / "review.json").write_text('{"verdict":"APPROVE"}\n')
            (review / "build-asan").mkdir()
            (review / "build-asan/CMakeCache.txt").write_text(
                "CMAKE_C_FLAGS:STRING=-fno-sanitize=alignment\n"
            )
            git(review, "add", "-A")
            git(review, "commit", "-qm", "report and build output")
            self.assertEqual(classify("changed", handoff, review), "0")
            self.assertEqual(classify("tamper", handoff, review), "")

            (review / "CMakeLists.txt").write_text(
                "project(example)\nset(CMAKE_C_FLAGS -fno-sanitize=address)\n"
            )
            git(review, "add", "-A")
            git(review, "commit", "-qm", "source tamper")
            self.assertEqual(classify("changed", handoff, review), "1")
            self.assertEqual(classify("tamper", handoff, review), "-fno-sanitize")

            (review / "review.json").write_text('{"verdict":"REJECT"}\n')
            git(review, "add", "-A")
            git(review, "commit", "-qm", "intermediate reviewer report")
            self.assertEqual(classify("changed", handoff, review), "1")
            self.assertEqual(classify("tamper", handoff, review), "-fno-sanitize")


if __name__ == "__main__":
    unittest.main()
