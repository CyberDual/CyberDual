"""Regression checks for the slot rule's validity in cyberdual cell_info: an attack killed at the time limit is invalid,
one stopped at the output-token cap is graded on the tree it left (owner 2026-10-21)."""
import importlib.machinery
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

_path = Path(__file__).resolve().parent.parent / 'cyberdual'
_loader = importlib.machinery.SourceFileLoader('cyberdual_cli', str(_path))
_spec = importlib.util.spec_from_loader('cyberdual_cli', _loader)
cyberdual = importlib.util.module_from_spec(_spec)
_loader.exec_module(cyberdual)


class CellInfoTest(unittest.TestCase):
    def window_cell(self, root, rc=None, output_stop=False, outcome='VALID_SERIES'):
        cell = Path(root) / 'month_openexr_vB_k2_1792475456_1_1'
        (cell / 'substrate' / 'work' / 'pocs').mkdir(parents=True)
        (cell / 'substrate' / 'work' / 'pocs' / 'probe.c').write_text('int main(void) { return 0; }\n')
        (cell / 'untampered.diff').write_text('clean\n')
        (cell / 'tampered.diff').write_text('attacked\n')
        (cell / 'series_grade_v3.json').write_text(json.dumps({'outcome': outcome}))
        if rc is not None:
            (cell / 'attacker_transcript.rc').write_text(f'{rc}\n')
        if output_stop:
            (cell / 'attacker_transcript.kimi_output_stop').write_text('251781\n')
        return cyberdual.cell_info(cell)

    def test_finished_valid(self):
        with tempfile.TemporaryDirectory() as d:
            info = self.window_cell(d, rc=0)
            self.assertTrue(info['gradeable'])
            self.assertTrue(info['valid'])

    def test_time_limit_kill_is_invalid(self):
        for rc in (124, 137):
            with tempfile.TemporaryDirectory() as d:
                info = self.window_cell(d, rc=rc)
                self.assertTrue(info['gradeable'])
                self.assertFalse(info['valid'])

    def test_output_cap_stop_is_graded_on_tree_left(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertTrue(self.window_cell(d, rc=137, output_stop=True)['valid'])

    def test_output_cap_stop_invalid_series_stays_invalid(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertFalse(self.window_cell(d, rc=137, output_stop=True, outcome='INVALID_SERIES')['valid'])


if __name__ == '__main__':
    unittest.main()
