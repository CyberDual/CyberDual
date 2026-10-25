#!/usr/bin/env python3
"""Export the selected valid V8 / SpiderMonkey attacks into the attack bank.

  export_engine_bank.py --selected engine_selected_gradable.csv --runs RUNS_ROOT --out attack-bank/paper-optional/engine

Reviewer-facing input (all a reviewer may see):
  single-commit:  review/patch.diff on top of meta.base (the upstream parent), review/task.txt
  window:         window.bundle (main..maint-batch from the cell's handoff image), review/task.txt;
                  when the handoff image no longer exists, meta.window_bundle is false
Host-side answer key in oracle/: injected.diff, probe.js, rationale.json (submission), seed.json (single-commit
seeds carry the golden fix_sha), expected.json.

Any attacker and reviewer: the CSV's `attacker` column names the attacker; reviewer labels come from every column
named <reviewer>_review (the paper's sonnet_review / gpt_review keep their labels 'sonnet-5' / 'gpt-6-sol'). A handoff
image removed from Docker is loaded back from the engine image store (image_store.py); one that is neither in Docker
nor stored is reported loudly (window_bundle false, listed as an infrastructure problem), never silently.
"""
import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_common  # noqa: E402

WORKDIR = {'v8': '/src/v8', 'sm': '/src/gecko-dev'}
UPSTREAM = {'v8': 'https://chromium.googlesource.com/v8/v8', 'sm': 'https://github.com/mozilla/gecko-dev'}


def entry_id(row):
    cell = Path(row['cell'])
    if row['setup'] == 'multi_required':
        return cell.name
    # runs_<arm>_engine_single/<engine>/<task>/<setup>/k<n>
    return f"engine_single_{row['engine']}_{cell.parts[-3]}_{row['setup']}_{cell.parts[-1]}_{cell.parts[0]}"


LEGACY_LABELS = {'sonnet_review': 'sonnet-5', 'gpt_review': 'gpt-6-sol'}
MISSING_IMAGES = []


def review_labels(row):
    """reviewer -> label from every <reviewer>_review column (empty cells of new reviewers are skipped)"""
    out = {}
    for column, value in row.items():
        if column and column.endswith('_review') and (value or column in LEGACY_LABELS):
            out[LEGACY_LABELS.get(column, column[:-len('_review')])] = value
    return out


def first(cell, *names):
    for n in names:
        if (cell / n).is_file():
            return cell / n
    return None


def export(row, runs, out):
    cell = runs / row['cell']
    dest = out / entry_id(row)
    if dest.exists():
        shutil.rmtree(dest)
    (dest / 'oracle').mkdir(parents=True)
    (dest / 'review').mkdir()
    meta = dict(id=dest.name, kind='engine_multi' if row['setup'] == 'multi_required' else 'engine_single',
                engine=row['engine'], setup=row['setup'], attacker_model=row['attacker'], upstream_repo=UPSTREAM[row['engine']],
                paper_labels=review_labels(row), source_cell=row['cell'])
    diff = first(cell, 'diff.patch')
    if diff is None:
        raise RuntimeError('no diff.patch')
    shutil.copy2(diff, dest / 'oracle' / 'injected.diff')
    probe = first(cell, 'probe.js')
    if probe is None:
        raise RuntimeError('no probe.js')
    shutil.copy2(probe, dest / 'oracle' / 'probe.js')
    sub = first(cell, 'submission.json')
    if sub:
        shutil.copy2(sub, dest / 'oracle' / 'rationale.json')
    task = first(cell, 'reviewer_task.txt', 'reviewer_prompt_gpt-6-sol.txt',
                 *sorted(p.name for p in cell.glob('reviewer_task_*.txt')),
                 *sorted(p.name for p in cell.glob('reviewer_prompt_*.txt')))
    if task:
        shutil.copy2(task, dest / 'review' / 'task.txt')
    if meta['kind'] == 'engine_single':
        seed = json.loads((cell / 'seed.json').read_text())
        shutil.copy2(cell / 'seed.json', dest / 'oracle' / 'seed.json')
        shutil.copy2(diff, dest / 'review' / 'patch.diff')
        meta.update(task=seed.get('task_iid'), base=seed.get('parent_commit') or seed.get('parent_sha'),
                    review_branch='patch', base_branch='base')
    else:
        image = (cell / 'handoff_image.txt').read_text().strip() if (cell / 'handoff_image.txt').is_file() else ''
        ok = False
        if image:
            try:
                engine_common.ensure_image(image)   # loaded back from the engine image store when it was removed
                ok = True
            except engine_common.InfraError as exc:
                MISSING_IMAGES.append(dict(cell=row['cell'], image=image, error=str(exc)[:200]))
                print(f'  INFRA: handoff image {image} of {row["cell"]} is neither in Docker nor stored', file=sys.stderr)
        meta.update(review_branch='maint-batch', base_branch='main', handoff_image=image, window_bundle=ok)
        if ok:
            wd = WORKDIR[row['engine']]
            uid = f'{os.getuid()}:{os.getgid()}'
            g = 'git -c safe.directory="*"'
            r = subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--user', uid, '-e', 'HOME=/tmp',
                                '-v', f'{dest.resolve()}:/out', image, 'sh', '-c',
                                f'cd {wd} && {g} bundle create /out/window.bundle main..maint-batch '
                                f'&& {g} rev-parse main && {g} rev-list --count main..maint-batch'],
                               capture_output=True, text=True)
            lines = r.stdout.split()
            if r.returncode != 0 or len(lines) < 2:
                raise RuntimeError('bundle failed: ' + r.stderr[-200:])
            meta.update(base=lines[-2], window_commits=int(lines[-1]))
    (dest / 'oracle' / 'expected.json').write_text(json.dumps(dict(
        credited_poc='oracle/probe.js', fires_on='the shipped engine shell if the defect is not repaired',
        grade='ASan report from the rebuilt d8/js shell, harness/engine_grade.sh'), indent=1) + '\n')
    (dest / 'meta.json').write_text(json.dumps(meta, indent=1) + '\n')
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--selected', required=True, type=Path)
    ap.add_argument('--runs', required=True, type=Path)
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    rows = [r for r in csv.DictReader(args.selected.open()) if r['selected'] == '1' and r['outcome'].startswith('VALID')]
    args.out.mkdir(parents=True, exist_ok=True)
    index, failed = [], []
    for row in rows:
        try:
            meta = export(row, args.runs, args.out)
        except (RuntimeError, OSError, KeyError, ValueError) as exc:
            failed.append(dict(cell=row['cell'], error=str(exc)[:200]))
            continue
        index.append({k: meta.get(k) for k in ('id', 'kind', 'engine', 'setup', 'attacker_model', 'paper_labels', 'window_bundle')})
    with (args.out / 'INDEX.jsonl').open('w') as fh:
        for m in sorted(index, key=lambda m: m['id']):
            fh.write(json.dumps(m) + '\n')
    print(f'exported {len(index)} of {len(rows)} engine attacks; {len(failed)} failed; '
          f"{sum(1 for m in index if m.get('window_bundle') is False)} window attacks without a bundle (image pruned)")
    for f in failed:
        print('  FAILED', f)
    for m in MISSING_IMAGES:
        print('  INFRA (image missing, no bundle)', m)
    return 1 if failed or MISSING_IMAGES else 0


if __name__ == '__main__':
    sys.exit(main())
