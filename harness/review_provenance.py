"""Bind resumable reviewer output to its inputs and configuration."""
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
import re
from pathlib import Path
import stat
import subprocess
import tempfile


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=True).encode()).hexdigest()


def settings(env, names):
    # Credentials can rotate without changing the experiment. Record their names only.
    # OUTPUT_TOKEN_CAP and similar token-count settings still affect the identity.
    credential = re.compile(r'(^|_)(API_KEY|ACCESS_KEY|SECRET|PASSWORD|TOKEN|CREDENTIALS?|AUTH|KEY_FILE)(_|$)')
    return {k: None if (credential.search(k.upper()) and k != 'OUTPUT_TOKEN_CAP') else env.get(k) for k in names}


def contents(path):
    """Fingerprint names, file contents, executable bits and symlink targets."""
    path = Path(path)
    if path.is_symlink():
        return ['link', os.readlink(path)]
    if path.is_dir():
        return digest([(p.name, contents(p)) for p in sorted(path.iterdir())])
    if not path.exists():
        return None
    mode = path.stat().st_mode
    if not stat.S_ISREG(mode):
        raise ValueError(f'cannot fingerprint non-regular file {path}')
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return [mode & 0o111, h.hexdigest()]


@contextmanager
def output_lock(out):
    # Materialization and grading share cell paths even when reviewer names differ.
    out.mkdir(parents=True, exist_ok=True)
    with (out / '.review.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit(f'cyberdual review: another review is using {out}; retry after it finishes')
        yield


def bank_identity(bank):
    # An exported bank may lack a manifest. Hash the actual inputs in either layout.
    return digest({name: contents(bank / name) for name in ('INDEX.jsonl', 'single', 'multi')})


def checkout_identity(checkout):
    """The review copies these branches and tracked edits. The scrub removes other refs and untracked files."""
    def git(*args):
        data = subprocess.run(['git', '-C', str(checkout), *args], capture_output=True, check=True).stdout
        return hashlib.sha256(data).hexdigest()

    refs = git('for-each-ref', '--format=%(refname) %(objectname)', 'refs/heads/base', 'refs/heads/patch',
               'refs/heads/main', 'refs/heads/maint-batch')
    paths = ('--', '.', ':(exclude)pocs', ':(exclude)selection.json', ':(exclude)submission.json')
    diff = ('diff', '--no-ext-diff', '--no-textconv', '--binary')
    return [refs, git(*diff, 'HEAD', *paths), git(*diff, '--cached', 'HEAD', *paths)]


def run_input(cell):
    record = cell / 'attack_record'
    if (record / 'record.json').is_file():
        return contents(record)
    # Older cells predate attack records. These source files are stable across reviews.
    inputs = {name: contents(cell / name) for name in (
        'series_grade_v3.json', 'reviewer.system.md', 'selection.json', 'roleA.diff',
        'untampered.diff', 'tampered.diff', 'oracle', 'pocs', 'pocs_kept')}
    for name in ('substrate/work', 'handoff'):
        checkout = cell / name
        if (checkout / '.git').exists():
            inputs[name] = checkout_identity(checkout)
    return digest(inputs)


def bind(out, reviewer, source, config, inputs, selected):
    """Called under output_lock before reading grades. Store only opaque identities."""
    path = out / '.review-provenance.json'
    existed = path.is_file()
    state = json.loads(path.read_text()) if existed else dict(version=1, source=source, configs={}, inputs={})
    recovery = ('use a fresh --out folder' if source[0] == 'bank' else
                'review an export of this run in a fresh --out folder')
    if state.get('version') != 1 or state.get('source') != source:
        raise SystemExit(f'cyberdual review: review input source changed; {recovery}')
    prior = state['configs'].get(reviewer)
    if prior is not None and prior != config:
        raise SystemExit(f'cyberdual review: reviewer configuration changed; {recovery}')
    # A prior bank materialization may also be stale, even if it has no grade yet.
    if ((not existed and source[0] == 'bank' and any(out.glob('*/series_grade_v3.json'))) or
            (prior is None and any((d / f'review_grade_{reviewer}_shell.json').is_file() for d in out.iterdir() if d.is_dir()))):
        raise SystemExit(f'cyberdual review: old results have no matching configuration record; {recovery}')
    if any(inputs.get(k) != v for k, v in state['inputs'].items()):
        raise SystemExit(f'cyberdual review: previously selected run inputs changed; {recovery}')
    state['configs'][reviewer] = config
    state['inputs'].update({k: inputs[k] for k in selected if k in inputs})
    write_state(out, state)
    return state


def write_state(out, state):
    with tempfile.NamedTemporaryFile(mode='w', dir=out, delete=False) as tmp:
        json.dump(state, tmp, sort_keys=True)
    os.replace(tmp.name, out / '.review-provenance.json')
