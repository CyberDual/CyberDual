"""Offline regression tests for the published sample of observations."""
import contextlib
import importlib.machinery
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from single_grade import write_single_grade

loader = importlib.machinery.SourceFileLoader('cohort_cli', str(HERE.parent / 'cyberdual'))
spec = importlib.util.spec_from_loader(loader.name, loader)
cli = importlib.util.module_from_spec(spec)
loader.exec_module(cli)


class ReportCohortTests(unittest.TestCase):
    def test_grade_and_timestamp_are_published_together_and_survive_cap_relabel(self):
        cell = self.cell(1, finish=1000)
        result = cell / 'series_grade_v3.json'
        original = result.read_bytes()
        with mock.patch('single_grade.os.replace', side_effect=OSError('write failed')):
            with self.assertRaises(OSError):
                write_single_grade(cell, {'outcome': 'INVALID_SERIES'})
        self.assertEqual(result.read_bytes(), original)
        (cell / 'attacker_transcript.meter.json').write_text('{"capped":true}')
        subprocess.run([sys.executable, HERE / 'attacker_cap.py', cell], check=True)
        relabeled = json.loads(result.read_text())
        self.assertEqual((relabeled['first_grade_ns'], relabeled['outcome']), (1000, 'INVALID_SERIES'))

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.run = self.root / 'run'
        self.run.mkdir()

    def cell(self, launch, suffix='a', finish=None, kind='single'):
        cell = self.run / f'{kind}_task_k1_{launch}_{suffix}'
        cell.mkdir()
        # These plain text placeholders model presence and size only.
        if kind == 'month':
            (cell / 'untampered.diff').write_text('one')
            (cell / 'tampered.diff').write_text('two')
            evidence = cell / 'pocs_kept'
        else:
            (cell / 'roleA.diff').write_text('observation')
            evidence = cell / 'pocs'
        evidence.mkdir()
        (evidence / 'placeholder').write_text('present')
        result = {'outcome': 'VALID_SERIES'}
        if finish is None:
            (cell / 'series_grade_v3.json').write_text(json.dumps(result))
        else:
            with mock.patch('single_grade.time.time_ns', return_value=finish):
                write_single_grade(cell, result)
        return cell

    def selected(self, target=1):
        return cli.report_cohort(self.run, {'env': {'TARGET': str(target)}})[3]['cohort']['selected_cells']

    def test_first_completion_survives_regrade_copy_and_later_arrival(self):
        early_launch = self.cell(10, finish=3000)
        first = self.cell(11, finish=2000)
        with mock.patch('single_grade.time.time_ns', return_value=5000):
            write_single_grade(first, {'outcome': 'INVALID_SERIES'})
        self.assertEqual(2000, json.loads((first / 'series_grade_v3.json').read_text())['first_grade_ns'])
        self.cell(8, finish=4000)
        source = self.run
        self.run = self.root / 'copy'
        shutil.copytree(source, self.run, copy_function=shutil.copyfile)
        os.utime(self.run / first.name / 'series_grade_v3.json', (20, 20))
        os.utime(self.run / early_launch.name / 'series_grade_v3.json', (10, 10))
        self.assertEqual(self.selected(), [first.name])

    def test_quota_and_same_time_tie_apply_to_cli_report(self):
        last = self.cell(12, 'b', finish=20)
        tie2 = self.cell(11, 'b', finish=10)
        tie1 = self.cell(11, 'a', finish=10)
        (last / 'review_grade_example_shell.json').write_text('{"outcome":"REVIEWER_CATCH"}')
        cli.runs_file(self.run).write_text(json.dumps({'command': 'attack', 'env': {'TARGET': '2'}}))
        output = self.root / 'observed.json'
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
            cli.cmd_report(SimpleNamespace(dirs=[self.run], json=output))
        rec, = json.loads(output.read_text())
        self.assertEqual((rec['slots'], rec['gradeable'], rec['unfilled']), (2, 2, 0))
        self.assertEqual(rec['cohort']['selected_cells'], [tie1.name, tie2.name])
        self.assertEqual(rec['reviewed_outside_cohort'], [last.name])
        self.assertIn('outside the reported cohort', err.getvalue())

    def test_legacy_ambiguity_and_authoritative_snapshot(self):
        original = self.cell(10)
        write_single_grade(original, {'outcome': 'VALID_SERIES'})
        self.assertNotIn('first_grade_ns', json.loads((original / 'series_grade_v3.json').read_text()))
        _, _, _, report = cli.report_cohort(self.run, {})
        self.assertEqual(report['cohort']['selected_cells'], [original.name])
        self.cell(1, finish=5)
        with self.assertRaisesRegex(ValueError, 'historical first completion is unknown'):
            self.selected()
        (self.run / 'report_cohort.json').write_text(json.dumps(report['cohort']))
        self.assertEqual(self.selected(), [original.name])
        (original / 'series_grade_v3.json').unlink()
        with self.assertRaisesRegex(ValueError, 'missing or ungradeable'):
            self.selected()

    def test_review_and_report_plan_the_same_sample_after_copy(self):
        later = self.cell(1, finish=3000)
        first = self.cell(3, finish=1000)
        second = self.cell(2, finish=2000)
        cli.runs_file(self.run).write_text(json.dumps({'command': 'attack', 'env': {'TARGET': '2'}}))
        copied = self.root / 'copy'
        shutil.copytree(self.run, copied, copy_function=shutil.copyfile)
        os.utime(copied / later.name / 'series_grade_v3.json', (1, 1))
        output = self.root / 'report.json'
        with contextlib.redirect_stdout(io.StringIO()):
            cli.cmd_report(SimpleNamespace(dirs=[copied], json=output))
        rec, = json.loads(output.read_text())
        self.assertEqual(rec['cohort']['selected_cells'], [first.name, second.name])
        with mock.patch.object(sys, 'argv', ['cyberdual', 'review', '--run', str(copied), '--dry-run',
                                             '--reviewer', 'custom', '--cmd', 'true']), \
             mock.patch.object(cli, 'base_env', return_value={}), contextlib.redirect_stdout(io.StringIO()) as plan:
            self.assertEqual(cli.main(), 0)
        self.assertEqual([line.strip() for line in plan.getvalue().splitlines() if line.startswith('  single_')],
                         rec['cohort']['selected_cells'])

    def summary(self, *manifests):
        cli.runs_file(self.run).write_text(''.join(json.dumps(dict(command='attack', **m)) + '\n' for m in manifests))
        output = self.root / 'accounting.json'
        with contextlib.redirect_stdout(io.StringIO()):
            cli.cmd_report(SimpleNamespace(dirs=[self.run], json=output))
        return json.loads(output.read_text())[0]

    def test_missing_declared_unit_is_pending_until_a_model_stop(self):
        first = self.cell(10, finish=1)
        (first / 'review_grade_example_shell.json').write_text('{"outcome":"REVIEWER_MISS"}')
        m1 = dict(only=['task'], setup='scratch')
        m2 = dict(only=['other'], setup='scratch')
        report = self.summary(m1, m2)
        self.assertEqual((report['slots'], report['unfilled'], report['refused']), (2, 0, 0))
        self.assertEqual(report['pending_units'], {'other': 1})
        self.assertIsNone(report['reviewers']['example']['asr_denominator'])
        end = self.run / '.topup_end' / 'other'
        end.parent.mkdir()
        end.write_text('g=0 tries=0 consec=0 infra=12')
        self.assertEqual(self.summary(m1, m2)['pending_slots'], 1)
        end.write_text('g=0 tries=3 consec=3 infra=0')
        report = self.summary(m1, m2)
        self.assertEqual((report['pending_slots'], report['unfilled'], report['refused']), (0, 1, 1))
        self.assertEqual(report['reviewers']['example']['asr_denominator'], 2)
        end.write_text('g=0 tries=10 consec=0 infra=0')
        report = self.summary(m1, m2)
        self.assertEqual((report['pending_slots'], report['unfilled'], report['refused']), (0, 1, 0))

    def test_grader_error_holds_slot_without_contributing_a_denominator(self):
        a = self.cell(10, finish=1)
        b = self.cell(11, finish=2)
        write_single_grade(b, {'outcome': 'GRADER_ERROR'})
        (a / 'review_grade_example_shell.json').write_text('{"outcome":"REVIEWER_MISS"}')
        m = dict(only=['task'], setup='scratch', env={'TARGET': '2'})
        report = self.summary(m)
        self.assertEqual((report['pending_series'], report['gradeable'], report['pending_slots']), ([b.name], 2, 1))
        self.assertIsNone(report['reviewers']['example']['attack_success_per_slot'])
        write_single_grade(b, {'outcome': 'INVALID_SERIES'})
        self.assertEqual(self.summary(m)['reviewers']['example']['attack_success_per_slot'], 0.5)

    def test_default_registry_is_specific_to_recorded_run(self):
        first = self.cell(10, finish=1)
        (first / 'review_grade_example_shell.json').write_text('{"outcome":"REVIEWER_MISS"}')
        m = dict(only=None, setup='scratch', code={'tag': 'sample'})
        partial = self.summary(m)
        self.assertEqual(partial['unit_scope'], 'observed')
        self.assertFalse(partial['complete_unit_plan'])
        self.assertIsNone(partial['reviewers']['example']['asr_denominator'])
        seeds = self.run / '_code/sample/cyberdual-single/seeds'
        seeds.mkdir(parents=True)
        for name in ['task', 'missing']:
            (seeds / f'{name}.json').write_text('{}')
        report = self.summary(m)
        self.assertEqual(report['unit_scope'], 'frozen_registry')
        self.assertEqual(report['pending_units'], {'missing': 1})
        self.assertEqual(report['slots'], 2)

    def test_completely_unstarted_plan_and_invalid_manifest(self):
        report = self.summary(dict(only=['planned'], setup='scratch'))
        self.assertEqual(report['pending_units'], {'planned': 1})
        with self.assertRaisesRegex(SystemExit, 'invalid unit list'):
            self.summary(dict(only='planned', setup='scratch'))

    def test_window_uses_launch_then_id(self):
        self.cell(9, 'z', kind='month')
        second = self.cell(6, 'b', kind='month')
        first = self.cell(6, 'a', kind='month')
        self.assertEqual(self.selected(2), [first.name, second.name])


if __name__ == '__main__':
    unittest.main()
