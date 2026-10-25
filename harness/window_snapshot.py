#!/usr/bin/env python3
"""Frozen window snapshots: every window attack starts from the same bytes.

  window_snapshot.py build <project> [<project> ...]   build the window once (p0_build_month.sh, which checks it against
                                                       windows/expected.tsv), store it as
                                                       attack-bank/windows/<project>/substrate.tar + window.json
  window_snapshot.py ensure [<project> ...]           make each snapshot present (all 14 by default): keep a tar that matches
                                                       window.json, else download it from the release (sha256 checked),
                                                       else rebuild it from upstream and require the same window_id, base,
                                                       maint-batch tip and tree as window.json
  window_snapshot.py restore <project> <substrate_dir> unpack the snapshot into <substrate_dir> and verify it against
                                                       window.json (tar sha256, main / maint-batch tips, tree); exit 1
                                                       on any mismatch
  window_snapshot.py verify [<project> ...]            rebuild each window from scratch and compare it with window.json
                                                       (window_id, base, maint-batch tip, tree, list_sha1, counts);
                                                       exit 1 on any difference (all 14 when no project is given)
  window_snapshot.py hash <substrate_dir>              print the window id of a built or restored window (sha256 of
                                                       base | maint-batch tip | maint-batch tree)

window.json: project, window_id, base, maint_batch_tip, tree, n_commits, n_editable, list_sha1, tar_sha256, built_utc,
cyberdual (commit that built it). The tar is not in git (attack-bank/windows/*/substrate.tar is ignored); it ships as a
release asset (<project>.substrate.tar on the windows-v1 release; CYBERDUAL_WINDOWS_URL overrides the location). A tar
rebuilt locally has other bytes (file times), so `ensure` records its sha256 in substrate.tar.local-sha256 and `restore`
accepts it once the rebuilt window matched window.json. At attack time no network, clone, date filter or working-set rule runs.
"""
import datetime, hashlib, json, os, shutil, subprocess, sys, tarfile, tempfile, urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent; ROOT = HERE.parent
WIN = Path(os.environ.get('WINDOW_SNAPSHOT_DIR') or ROOT / 'attack-bank' / 'windows')
GH_REPO, GH_TAG = 'CyberDual/CyberDual', 'windows-v1'
RELEASE_URL = os.environ.get('CYBERDUAL_WINDOWS_URL') or f'https://github.com/{GH_REPO}/releases/download/{GH_TAG}'


def git(repo, *a):
    return subprocess.run(['git', '-C', str(repo), *a], capture_output=True, text=True, check=True).stdout.strip()


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def window_id(sub):
    W = Path(sub) / 'work'
    base, tip, tree = git(W, 'rev-parse', 'main'), git(W, 'rev-parse', 'maint-batch'), git(W, 'rev-parse', 'maint-batch^{tree}')
    return hashlib.sha256(f'{base}|{tip}|{tree}'.encode()).hexdigest(), base, tip, tree


def project_row(project):
    for l in open(HERE / 'project_windows.tsv'):
        if l.strip() and not l.startswith('#'):
            f = l.rstrip('\n').split('\t')
            if f[0] == project:
                return dict(repo=f[1], since=f[2], until=f[3])
    sys.exit(f'unknown project {project}')


def expected(project):
    for l in open(HERE / 'windows' / 'expected.tsv'):
        if l.startswith(project + '\t'):
            f = l.rstrip('\n').split('\t')
            return dict(n_commits=int(f[1]), n_editable=int(f[2]), list_sha1=f[5])
    sys.exit(f'{project} not in expected.tsv')


def fresh_build(project):
    """build the window from upstream into a temp dir; returns (tmp, substrate, log) or exits on failure"""
    row = project_row(project)
    tmp = Path(tempfile.mkdtemp(prefix=f'winsnap_{project}_', dir=os.environ.get('TMPDIR', '/tmp')))
    sub = tmp / 'substrate'
    env = dict(os.environ, REPO=row['repo'], SINCE=row['since'], UNTIL=row['until'], OUT=str(sub), WINDOW_SNAPSHOT='none')
    r = subprocess.run(['bash', str(HERE / 'p0_build_month.sh')], env=env, capture_output=True, text=True)
    log = r.stdout + r.stderr
    if r.returncode or f'window check: exactly the expected {project} window' not in log:
        print(log[-1500:]); shutil.rmtree(tmp, ignore_errors=True); return None, None, log
    return tmp, sub, log


def verify(projects):
    bad = 0
    for project in projects:
        meta = json.loads((WIN / project / 'window.json').read_text())
        tmp, sub, log = fresh_build(project)
        if sub is None:
            print(f'{project}: REBUILD FAILED'); bad += 1; continue
        wid, base, tip, tree = window_id(sub); shutil.rmtree(tmp, ignore_errors=True)
        diffs = [k for k, a, b in (('window_id', wid, meta['window_id']), ('base', base, meta['base']),
                                   ('maint_batch_tip', tip, meta['maint_batch_tip']), ('tree', tree, meta['tree'])) if a != b]
        print(f"{project}: {'IDENTICAL' if not diffs else 'DIFFERS in ' + ', '.join(diffs)} (window {wid[:16]})"); bad += bool(diffs)
    return 1 if bad else 0


def build(project):
    exp = expected(project)
    tmp, sub, log = fresh_build(project)
    if sub is None:
        sys.exit(f'{project}: build failed or did not pass the window check')
    (sub / 'substrate.build.log').write_text(log)
    wid, base, tip, tree = window_id(sub)
    out = WIN / project; out.mkdir(parents=True, exist_ok=True)
    tar = out / 'substrate.tar'
    with tarfile.open(tar, 'w') as t:
        t.add(sub, arcname='substrate')
    meta = dict(project=project, window_id=wid, base=base, maint_batch_tip=tip, tree=tree, **exp,
                tar_sha256=sha256_file(tar), tar_bytes=tar.stat().st_size,
                built_utc=datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None).strftime('%Y-%m-%dT%H:%M:%SZ'),
                cyberdual=subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--short=8', 'HEAD'], capture_output=True, text=True).stdout.strip())
    (out / 'window.json').write_text(json.dumps(meta, indent=1) + '\n')
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{project}: window {wid[:16]} ({exp['n_commits']} commits, {exp['n_editable']} editable) -> {tar} ({meta['tar_bytes'] >> 20} MB)")


def local_sha(project):
    p = WIN / project / 'substrate.tar.local-sha256'
    return p.read_text().strip() if p.is_file() else None


def tar_ok(project, meta):
    tar = WIN / project / 'substrate.tar'
    return tar.is_file() and sha256_file(tar) in (meta['tar_sha256'], local_sha(project))


def download(project, meta):
    """the release asset, kept only if its sha256 is the one in window.json"""
    tar = WIN / project / 'substrate.tar'; part = tar.with_name('substrate.tar.part'); name = f'{project}.substrate.tar'
    part.unlink(missing_ok=True)
    try:
        urllib.request.urlretrieve(f'{RELEASE_URL}/{name}', part)
    except Exception as exc:
        part.unlink(missing_ok=True)
        if shutil.which('gh') and not os.environ.get('CYBERDUAL_WINDOWS_URL'):   # a private repository needs the GitHub login
            subprocess.run(['gh', 'release', 'download', GH_TAG, '-R', GH_REPO, '-p', name, '-O', str(part), '--clobber'],
                           capture_output=True)
        if not part.is_file():
            print(f'{project}: download failed ({exc})'); return False
    if sha256_file(part) != meta['tar_sha256']:
        print(f'{project}: downloaded tar does not match window.json (sha256); discarded'); part.unlink(); return False
    part.rename(tar)
    return True


def rebuild(project, meta):
    """build the window from upstream; keep it only if it is the window in window.json"""
    tmp, sub, log = fresh_build(project)
    if sub is None:
        print(f'{project}: rebuild from upstream failed'); return False
    ident = window_id(sub)
    if ident != (meta['window_id'], meta['base'], meta['maint_batch_tip'], meta['tree']):
        print(f'{project}: rebuilt window {ident[0][:16]} differs from window.json {meta["window_id"][:16]}; discarded')
        shutil.rmtree(tmp, ignore_errors=True); return False
    (sub / 'substrate.build.log').write_text(log)
    tar = WIN / project / 'substrate.tar'
    with tarfile.open(tar, 'w') as t:
        t.add(sub, arcname='substrate')
    (WIN / project / 'substrate.tar.local-sha256').write_text(sha256_file(tar) + '\n')
    shutil.rmtree(tmp, ignore_errors=True)
    return True


def ensure(projects):
    bad = 0
    for project in projects:
        meta_p = WIN / project / 'window.json'
        if not meta_p.is_file():
            print(f'{project}: no window.json'); bad += 1; continue
        meta = json.loads(meta_p.read_text())
        if tar_ok(project, meta):
            continue
        print(f"{project}: fetching the window snapshot ({meta['tar_bytes'] >> 20} MB)", flush=True)
        if download(project, meta):
            print(f'{project}: snapshot downloaded and checked'); continue
        print(f'{project}: rebuilding the window from upstream (network, a few minutes)', flush=True)
        if rebuild(project, meta):
            print(f'{project}: rebuilt; identical to window.json'); continue
        bad += 1
    return 1 if bad else 0


def restore(project, dest):
    meta_p = WIN / project / 'window.json'; tar = WIN / project / 'substrate.tar'
    if not meta_p.is_file() or not tar.is_file():
        print(f'no window snapshot for {project} (run: ./cyberdual window fetch {project})'); return 2
    meta = json.loads(meta_p.read_text())
    if sha256_file(tar) not in (meta['tar_sha256'], local_sha(project)):
        print(f'WINDOW SNAPSHOT MISMATCH: {tar} sha256 differs from window.json'); return 1
    dest = Path(dest); shutil.rmtree(dest, ignore_errors=True); dest.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(tar) as t:
        t.extractall(dest.parent / '.winsnap_tmp', filter='tar') if hasattr(tarfile, 'data_filter') else t.extractall(dest.parent / '.winsnap_tmp')
    os.rename(dest.parent / '.winsnap_tmp' / 'substrate', dest); shutil.rmtree(dest.parent / '.winsnap_tmp', ignore_errors=True)
    wid, base, tip, tree = window_id(dest)
    if (wid, base, tip, tree) != (meta['window_id'], meta['base'], meta['maint_batch_tip'], meta['tree']):
        print(f'WINDOW SNAPSHOT MISMATCH: restored {project} window {wid[:16]} != {meta["window_id"][:16]}'); return 1
    (dest / 'window_snapshot.json').write_text(json.dumps(meta, indent=1) + '\n')
    print(f">> window snapshot {project} {wid[:16]} restored ({meta['n_commits']} commits, {meta['n_editable']} editable; "
          f"built {meta['built_utc']} by CyberDual {meta.get('cyberdual')})")
    return 0


def main():
    if len(sys.argv) < 2 or (len(sys.argv) < 3 and sys.argv[1] not in ('verify', 'ensure')):
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == 'build':
        for p in sys.argv[2:]:
            build(p)
    elif cmd == 'restore':
        sys.exit(restore(sys.argv[2], sys.argv[3]))
    elif cmd in ('verify', 'ensure'):
        ps = sys.argv[2:] or sorted(p.name for p in WIN.iterdir() if (p / 'window.json').is_file())
        sys.exit(verify(ps) if cmd == 'verify' else ensure(ps))
    elif cmd == 'hash':
        print(window_id(sys.argv[2])[0])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
