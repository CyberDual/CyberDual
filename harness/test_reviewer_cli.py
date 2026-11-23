"""Offline checks for reviewer sample sizes and the tool versions reported by doctor."""
import argparse
import contextlib
import importlib.machinery
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


CLI = Path(__file__).resolve().parents[1] / "cyberdual"
loader = importlib.machinery.SourceFileLoader("reviewer_cli", str(CLI))
cli = importlib.util.module_from_spec(importlib.util.spec_from_loader(loader.name, loader))
loader.exec_module(cli)


class ReviewerLimitTest(unittest.TestCase):
    def test_bank_review_limits(self):
        with tempfile.TemporaryDirectory() as tmp:
            bank = Path(tmp)
            rows = [dict(id=str(i), kind="single", setup="single_from_scratch", project="fixture") for i in range(3)]
            (bank / "INDEX.jsonl").write_text("".join(json.dumps(row) + "\n" for row in rows))
            for limit, expected in (([], 3), (["--limit", "1"], 1), (["--limit", "0"], 0)):
                with self.subTest(limit=limit):
                    result = subprocess.run([sys.executable, str(CLI), "review", "--reviewer", "custom",
                                             "--cmd", "false", "--dry-run", "--bank", tmp, *limit],
                                            capture_output=True, text=True,
                                            env=dict(PATH=os.environ["PATH"], HOME=tmp))
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(f"{expected} valid attacks", result.stdout)

    def test_run_review_rejects_bank_limit(self):
        for limit in ("0", "1"):
            with self.subTest(limit=limit):
                result = subprocess.run([sys.executable, str(CLI), "review", "--reviewer", "custom",
                                         "--run", "fixture", "--limit", limit], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn("--limit applies to bank reviews", result.stderr)

    def test_negative_limit_rejected_before_bank_lookup(self):
        result = subprocess.run([sys.executable, str(CLI), "review", "--reviewer", "custom",
                                 "--limit", "-1"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--limit must be nonnegative", result.stderr)


class DoctorTest(unittest.TestCase):
    def test_each_pin_and_version_command_exit_status(self):
        for tool, code, override in (
                (None, 0, None), ("claude", 0, "0.0.0"), ("codex", 0, "0.0.0"),
                ("claude", 1, None), ("codex", 1, None)):
            with self.subTest(tool=tool, code=code, override=override):
                checked = []

                def docker(argv, **kwargs):
                    binary = argv[-2]
                    versions = {"claude": cli.PROTOCOL["PINNED_CLAUDE_CODE"], "codex": cli.PROTOCOL["PINNED_CODEX"]}
                    version = override if binary == tool and override is not None else versions[binary]
                    stdout = f"codex-cli {version}\n" if binary == "codex" else f"{version} (Claude Code)\n"
                    self.assertEqual(argv[-1], "--version")
                    checked.append(binary)
                    return subprocess.CompletedProcess(argv, code if binary == tool else 0, stdout=stdout, stderr="")

                with patch.object(cli, "base_env", return_value={}), patch.object(cli, "image_info", return_value="id"), \
                     patch.object(cli.shutil, "which", return_value="/mock/bin"), patch.object(cli.subprocess, "run", docker), \
                     contextlib.redirect_stdout(io.StringIO()):
                    rc = cli.cmd_doctor(argparse.Namespace(models=[], selftest=False))
                self.assertEqual(checked, ["claude", "codex"])
                self.assertEqual(rc, 0 if tool is None else 1)

    def test_selftest_discovers_both_suites(self):
        discovered = []

        def run(argv, **kwargs):
            if "discover" in argv:
                self.assertEqual(argv[1:5], ["-m", "unittest", "discover", "-s"])
                discovered.append(Path(argv[5]))
                return subprocess.CompletedProcess(argv, 0, stdout="", stderr="OK")
            if argv[-1] == "--version":
                key = "PINNED_CLAUDE_CODE" if argv[-2] == "claude" else "PINNED_CODEX"
                return subprocess.CompletedProcess(argv, 0, stdout=cli.PROTOCOL[key], stderr="")
            self.assertTrue(argv[-1].endswith("selftest.py"))
            return subprocess.CompletedProcess(argv, 0, stdout="passed", stderr="")

        with patch.object(cli, "base_env", return_value={}), patch.object(cli, "image_info", return_value="id"), \
             patch.object(cli.shutil, "which", return_value="/mock/bin"), patch.object(cli.subprocess, "run", run), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cli.cmd_doctor(argparse.Namespace(models=[], selftest=True)), 0)
        self.assertEqual(discovered, [cli.H, cli.H / "engine"])


if __name__ == "__main__":
    unittest.main()
