"""Offline reviewer resume checks, using two synthetic rows and a stub reviewer."""
from argparse import Namespace
from contextlib import ExitStack
from pathlib import Path
import json
import os
import subprocess
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from harness import review_provenance as provenance

cli = runpy.run_path(str(ROOT / 'cyberdual'))['cmd_review'].__globals__


class ResumeTest(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.bank = self.root / 'bank'
        self.bank.mkdir()
        self.out = self.root / 'out'
        self.mount = self.root / 'tool'
        self.mount.mkdir()
        (self.mount / 'main').write_text('version one')
        (self.bank / 'INDEX.jsonl').write_text(''.join(
            json.dumps(dict(kind='single', id=name)) + '\n' for name in ['a', 'b']))
        self.calls = []
        self.image_id = 'v1'
        self.stack.enter_context(patch.dict(os.environ, {'EXAMPLE_VALUE': 'private-fixture'}, clear=True))
        self.stack.enter_context(patch.dict(cli, freeze_code=lambda out: None, review_code_identity=lambda: 'code',
                                            image_info=lambda image: self.image_id, materialize=self.cell))
        self.stack.enter_context(patch('subprocess.call', side_effect=self.grade))

    def cell(self, row, out):
        cell = out / row['id']
        cell.mkdir(exist_ok=True)
        return cell

    def grade(self, argv, **kwargs):
        cell = Path(argv[-1])
        self.calls.append(cell.name)
        (cell / 'review_grade_custom-x_shell.json').write_text('{"outcome":"REVIEWER_CATCH"}')
        return 0

    def review(self, **changes):
        options = dict(run=None, bank=str(self.bank), out=self.out, reviewer='custom', model='x',
                       custom_cmd='private-command-fixture', custom_env=['EXAMPLE_VALUE'], custom_mount=str(self.mount),
                       custom_image=None, allow_network=False, set=[], limit=None, only=None,
                       subset='all', kinds=['single'], dry_run=False, par=1)
        return cli['cmd_review'](Namespace(**dict(options, **changes)))

    def test_resume_subset_and_retry(self):
        self.assertEqual(self.review(only=['a']), 0)
        self.assertEqual(self.review(), 0)
        self.assertEqual(self.calls, ['a', 'b'])
        (self.out / 'a/review_grade_custom-x_shell.json').write_text('{"outcome":"PROVIDER_QUOTA"}')
        self.assertEqual(self.review(), 0)
        self.assertEqual(self.calls, ['a', 'b', 'a'])
        for path in [self.out / 'cyberdual_runs.jsonl', self.out / '.review-provenance.json']:
            self.assertNotIn('private-fixture', path.read_text())
            self.assertNotIn('private-command-fixture', path.read_text())

    def test_rotating_a_custom_credential_preserves_results(self):
        with patch.dict(os.environ, {'MY_API_KEY': 'first'}):
            self.review(custom_env=['MY_API_KEY'])
        with patch.dict(os.environ, {'MY_API_KEY': 'rotated'}):
            self.review(custom_env=['MY_API_KEY'])
        self.assertEqual(self.calls, ['a', 'b'])

    def test_run_source_can_grow_and_detects_changed_reviewed_input(self):
        run = self.root / 'run'
        cell = run / 'a'
        cell.mkdir(parents=True)
        (cell / 'roleA.diff').write_text('old fixture')
        selected = [cell]
        with patch.dict(cli, selected_valid=lambda run: selected):
            self.review(bank=None, run=run)
            second = run / 'b'
            second.mkdir()
            selected.append(second)
            self.review(bank=None, run=run)
            (cell / 'roleA.diff').write_text('changed fixture')
            with self.assertRaisesRegex(SystemExit, 'previously selected run inputs changed'):
                self.review(bank=None, run=run, only=['b'])
        self.assertEqual(self.calls, ['a', 'b'])

    def test_handoff_drift_when_substrate_unchanged(self):
        run = self.root / 'run'
        cell = run / 'a'
        for name in ('substrate/work', 'handoff'):
            path = cell / name
            path.mkdir(parents=True)
            self.git(path, 'init', '-q', '-b', 'patch')
            (path / 'README').write_text('fixture')
            self.git(path, 'add', 'README')
            self.git(path, 'commit', '-qm', 'initial')
        with patch.dict(cli, selected_valid=lambda run: [cell]):
            self.review(bank=None, run=run)
            (cell / 'handoff/untracked').write_text('cleaned before review')
            self.review(bank=None, run=run)
            (cell / 'handoff/README').write_text('tracked edit')
            with self.assertRaisesRegex(SystemExit, 'changed'):
                self.review(bank=None, run=run)
            self.git(cell / 'handoff', 'commit', '-qam', 'changed handoff')
            with self.assertRaisesRegex(SystemExit, 'changed'):
                self.review(bank=None, run=run)
        self.assertEqual(self.calls, ['a'])

    def git(self, path, *args):
        subprocess.run(['git', '-C', str(path), '-c', 'user.name=Test', '-c', 'user.email=test@example.org',
                        *args], check=True, capture_output=True)

    def test_configuration_mismatch(self):
        self.review()
        for options in [dict(custom_cmd='v2'), dict(allow_network=True),
                        dict(set=['CYBERDUAL_CODEX_EFFORT=low'])]:
            with self.subTest(options=options), self.assertRaisesRegex(SystemExit, 'configuration changed'):
                self.review(**options)
        with patch.dict(os.environ, {'EXAMPLE_VALUE': 'new-value'}), self.assertRaisesRegex(SystemExit, 'configuration changed'):
            self.review()
        self.image_id = 'v2'
        with self.assertRaisesRegex(SystemExit, 'configuration changed'):
            self.review()
        self.image_id = 'v1'
        (self.mount / 'main').write_text('version two')
        with self.assertRaisesRegex(SystemExit, 'configuration changed'):
            self.review()
        self.assertEqual(self.calls, ['a', 'b'])

    def test_different_bank_with_same_ids(self):
        self.review(only=['a'])
        entry = self.bank / 'single/a'
        entry.mkdir(parents=True)
        (entry / 'metadata.txt').write_text('changed input')
        with self.assertRaisesRegex(SystemExit, 'input source changed'):
            self.review()
        self.assertEqual(self.calls, ['a'])

    def test_legacy_grade_and_concurrent_job(self):
        (self.out / 'a').mkdir(parents=True)
        (self.out / 'a/review_grade_custom-x_shell.json').write_text('{"outcome":"REVIEWER_CATCH"}')
        with provenance.output_lock(self.out), self.assertRaisesRegex(SystemExit, 'another review'):
            self.review()
        with self.assertRaisesRegex(SystemExit, 'old results'):
            self.review(only=['b'])
        self.assertEqual(self.calls, [])


if __name__ == '__main__':
    unittest.main()
