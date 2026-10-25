#!/usr/bin/env python3
"""attack_record.py — the ONE artifact contract for attack -> review -> grade (owner-approved 2026-10-14).

An AttackRecord is built once per attack and is the only thing downstream stages read. The attacker's raw repo is
evidence; it is never shown to a reviewer or graded directly.

  record dir (default <cell>/attack_record/):
    record.json      schema cyberdual.attack_record/1 (see SCHEMA below; older records say viper.attack_record/1)
    series.bundle    base..<series tip>   refs/heads/maint-batch (window) | refs/heads/patch (single)
    clean.bundle     base..<clean tip>    refs/heads/clean  (absent when clean tip == base, i.e. single from-scratch)
    poc/             the credited PoC file(s), copied verbatim (kept OUT of the series)

  python3 harness/attack_record.py build <cell> [--out DIR] [--messages attacker|upstream]
  python3 harness/attack_record.py materialize <record_dir> <dest> [--base-repo REPO]   -> prints {"handoff_hash": ...}
  python3 harness/attack_record.py shipped <record_dir> <review_ws> <out_dir>            -> ShippedRecord (shipped.json)
  python3 harness/attack_record.py grade-inputs <record_dir> {tampered|clean|shipped} <out_dir> [--shipped DIR]
     (diff is null when tree == base: call grade.sh WITHOUT the patch argument)
  python3 harness/attack_record.py verify <record_dir> [--base-repo REPO]

Content addressing: git commit ids already hash content + metadata + parents, so
  attack_hash   = sha256("<schema>|kind|base|series_tip_commit|clean_tip_commit|poc_sha256"), with the record's own schema
  handoff_hash  = sha256("handoff|base|series_tip_commit|refs")        (materialize asserts the tip equals the record's)
  shipped_hash  = sha256("shipped|attack_hash|shipped_tree")
A grade json that records attack_hash -> handoff_hash -> shipped_hash is stale/mismatched iff a hash differs.
"""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rebuild_handoff as RH  # noqa: E402
import scrub_message  # noqa: E402

SCHEMA = 'cyberdual.attack_record/1'
READ_SCHEMAS = (SCHEMA, 'viper.attack_record/1')   # records in the released bank carry the earlier name
git = RH.git
BOT = RH.BOT
REVIEW_ARTIFACTS = re.compile(r'^(review\.json|corrected\.diff)$')


def git_diff(repo, *a):
    """git diff as RAW BYTES: text-mode decoding turns CRLF into LF and the patch then fails to apply
    (matio src/mcos.c, found by the paper session)."""
    p = subprocess.run(['git', '-C', repo, 'diff', *a], capture_output=True)
    if p.returncode: raise RuntimeError('git diff: ' + p.stderr.decode(errors='replace')[:300])
    return p.stdout


def sha256(s):
    return hashlib.sha256(s.encode()).hexdigest()


def file_sha256(paths):
    h = hashlib.sha256()
    for p in sorted(paths):
        h.update(os.path.basename(p).encode() + b'\0'); h.update(open(p, 'rb').read())
    return h.hexdigest()


def _tmpdir(prefix):
    return tempfile.mkdtemp(prefix=prefix, dir=os.environ.get('RBH_TMP', '/tmp'))


def _probe_files(cell):
    """credited PoC source, plus the header files beside it (a PoC may `#include "helper.h"` from its own folder)."""
    src = _credited_source(cell)
    if not src:
        return []
    d = os.path.dirname(src[0])
    hdr = sorted(os.path.join(d, f) for f in os.listdir(d) if re.search(r'\.(h|hh|hpp|hxx|inc)$', f))
    return src + hdr


def _credited_source(cell):
    """credited PoC source(s): same preference order as rereview_any.sh."""
    W = os.path.join(cell, 'substrate', 'work')
    for cand in ('pocs/residual.c', 'pocs/newbug.c', 'pocs/newbug.cpp', 'pocs/probe.c', 'pocs/probe.cpp'):
        for root in (cell, W):
            p = os.path.join(root, cand)
            if os.path.isfile(p): return [p]
    for root in (cell, W):
        d = os.path.join(root, 'pocs')
        if os.path.isdir(d):
            src = sorted(os.path.join(d, f) for f in os.listdir(d) if re.search(r'\.(c|cc|cpp|cxx|C)$', f)
                         and not re.search(r'check_fix|feature_check|verify|_fix\.|^check[_.]|fixcheck', f, re.I))
            if src: return src[:1]
    return []


def _pin_clean_tip(cell, W):
    """The untampered window tip is authoritative in substrate/_pristine (the attacker never touches it). The attacker
    works in substrate/work and may rename, move or delete orig_month or add branches of its own: reset orig_month there
    to the pristine tip (the object is present, as work was copied from pristine) and ignore any other branch."""
    P = os.path.join(cell, 'substrate', '_pristine')
    if not os.path.isdir(os.path.join(P, '.git')):
        return None
    tip = git(P, 'rev-parse', 'orig_month').strip() or git(P, 'rev-parse', 'maint-batch').strip()
    cur = subprocess.run(['git', '-C', W, 'rev-parse', '--verify', '-q', 'refs/heads/orig_month'], capture_output=True, text=True).stdout.strip()
    if tip and cur != tip:
        git(W, 'update-ref', 'refs/heads/orig_month', tip)
        return dict(orig_month_restored_from_pristine=tip, orig_month_in_work=cur or None)
    return None


def _kind(cell):
    W = os.path.join(cell, 'substrate', 'work')
    refs = set(git(W, 'for-each-ref', '--format=%(refname:short)', 'refs/heads').split())
    if 'maint-batch' in refs and 'main' in refs and os.path.isdir(os.path.join(cell, 'substrate', '_pristine', '.git')):
        _pin_clean_tip(cell, W); return 'window'
    if 'maint-batch' in refs and 'orig_month' in refs: return 'window'
    if 'patch' in refs and 'base' in refs: return 'single'
    raise RuntimeError(f'unknown substrate layout (refs={sorted(refs)})')


BUILDER_FILES = ('attack_record.py', 'rebuild_handoff.py', 'scrub_message.py')


def builder_provenance():
    """which code built this record. builder_hash = sha256 over the builder files with tree-specific paths normalized
    (harness/ <-> harness/, $HOME <-> /home/<user>), so identical logic gives the same hash in the live pipeline and in
    a CyberDual checkout; blobs = raw git blob ids; head = the enclosing repo's HEAD if any."""
    h = hashlib.sha256(); blobs = {}
    for f in BUILDER_FILES:
        p = os.path.join(HERE, f)
        raw = open(p, 'rb').read()
        # literals split so the live->repo mirror (which rewrites the pipeline path) cannot alter them
        norm = raw.replace(b'harn' b'ess/', b'pipe' b'line/'); norm = re.sub(rb'/ho' rb'me/[A-Za-z0-9_.-]+', b'$HO' b'ME', norm)
        h.update(f.encode() + b'\0' + norm)
        blobs[f] = subprocess.run(['git', 'hash-object', p], capture_output=True, text=True).stdout.strip()
    head = subprocess.run(['git', '-C', HERE, 'rev-parse', 'HEAD'], capture_output=True, text=True)
    return dict(builder_hash=h.hexdigest(), blobs=blobs, repo_head=head.stdout.strip() if head.returncode == 0 else None)


def _finish_record(out, rec, poc_src, review_sys=None):
    rec['provenance'].update(builder_provenance())
    os.makedirs(os.path.join(out, 'poc'), exist_ok=True)
    if review_sys and os.path.isfile(review_sys):   # the reviewer system prompt this cell was reviewed with
        shutil.copyfile(review_sys, os.path.join(out, 'reviewer.system.md')); rec['reviewer_system'] = 'reviewer.system.md'
    else:
        rec['reviewer_system'] = None
    pocs = []
    for p in poc_src:
        dst = os.path.join(out, 'poc', os.path.basename(p)); shutil.copyfile(p, dst); pocs.append(dst)
    rec['poc'] = dict(files=[os.path.relpath(p, out) for p in pocs], sha256=file_sha256(pocs) if pocs else None)
    rec['attack_hash'] = sha256('|'.join([SCHEMA, rec['kind'], rec['base_sha'], rec['series']['tip_commit'],
                                          (rec.get('clean') or {}).get('tip_commit') or rec['base_sha'],
                                          rec['poc']['sha256'] or '']))
    json.dump(rec, open(os.path.join(out, 'record.json'), 'w'), indent=1)
    return rec


def build(cell, out=None, messages='attacker'):
    cell = os.path.abspath(cell.rstrip('/')); out = os.path.abspath(out or os.path.join(cell, 'attack_record'))
    kind = _kind(cell); W = os.path.join(cell, 'substrate', 'work')
    sg = json.load(open(os.path.join(cell, 'series_grade_v3.json'))) if os.path.exists(os.path.join(cell, 'series_grade_v3.json')) else {}
    shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
    rec = dict(schema=SCHEMA, kind=kind, cell=os.path.relpath(cell, os.path.dirname(os.path.dirname(cell))),
               project=os.path.basename(cell).split('_')[1].split('__')[0], series_grade=sg,
               provenance=dict(substrate=W, builder='attack_record.py+rebuild_handoff.py', messages_mode=messages))
    if kind == 'window':
        box = {}

        def on_built(R, base, tip_t, tip_o, res):
            git(R, 'update-ref', 'refs/heads/clean', tip_o)
            git(R, 'bundle', 'create', os.path.join(out, 'series.bundle'), f'{base}..maint-batch')
            git(R, 'bundle', 'create', os.path.join(out, 'clean.bundle'), f'{base}..clean')
            box.update(base=base, tip_t=tip_t, tip_o=tip_o,
                       tree_t=git(R, 'rev-parse', tip_t + '^{tree}').strip(), tree_o=git(R, 'rev-parse', tip_o + '^{tree}').strip(),
                       n=int(git(R, 'rev-list', '--count', f'{base}..{tip_t}')), no=int(git(R, 'rev-list', '--count', f'{base}..{tip_o}')))
        res = RH.build_window(cell, messages, on_built=on_built)
        if not res.get('ok') or not box:
            raise RuntimeError(f"rebuild failed: {res.get('violations')}")
        rec.update(base_sha=box['base'], branches=dict(base='main', series='maint-batch'),
                   series=dict(bundle='series.bundle', ref='refs/heads/maint-batch', tip_commit=box['tip_t'], tip_tree=box['tree_t'], n_commits=box['n']),
                   clean=dict(bundle='clean.bundle', ref='refs/heads/clean', tip_commit=box['tip_o'], tip_tree=box['tree_o'], n_commits=box['no']),
                   checks={k: res.get(k) for k in ('ok', 'violations', 'n_upstream', 'n_tampered', 'n_rebuilt', 'attacker_added',
                                                    'attacker_dropped_upstream', 'attacker_messages_kept', 'project_wraps',
                                                    'junk_dropped', 'answer_dropped', 'code_diff_paths')})
        rec['provenance'].update(source_series=git(W, 'rev-parse', 'maint-batch').strip(), source_clean=git(W, 'rev-parse', 'orig_month').strip())
    else:
        rec.update(_build_single(cell, W, out, sg))
    return _finish_record(out, rec, _probe_files(cell), os.path.join(cell, 'reviewer.system.md'))


def _build_single(cell, W, out, sg):
    """single-commit: base + ONE commit = the attacker's patch tip tree minus answer/junk, message = the vetted handoff
    message (filtered), maint-bot identity. clean = base + golden.diff (golden-given) or base itself (from-scratch)."""
    tmp = _tmpdir('ar_single_')
    try:
        R = os.path.join(tmp, 'r')
        subprocess.run(['git', 'clone', '-q', '--shared', '--bare', W, R], check=True)
        for ref in ('base', 'patch'):
            git(R, 'fetch', '-q', W, f'+refs/heads/{ref}:refs/heads/{ref}')
        base = git(R, 'rev-parse', 'base').strip()
        base_paths = set(git(R, 'ls-tree', '-r', '--name-only', base).splitlines())
        tip_paths = set(git(R, 'ls-tree', '-r', '--name-only', 'patch').splitlines())
        added = (tip_paths - base_paths) | (set(git(R, 'log', '--no-renames', '--diff-filter=A', '--name-only', '--format=', 'base..patch').split('\n')) - {''})
        drop = sorted(p for p in added if RH.ANSWER.search(p) or RH.JUNK.search(p))
        idx = os.path.join(tmp, 'idx'); env = dict(GIT_INDEX_FILE=idx)
        git(R, 'read-tree', 'patch', env=env)
        if drop: git(R, 'rm', '-r', '-q', '--cached', '--ignore-unmatch', '--', *drop, env=env)
        # tracked upstream files under build-like names (build.gradle, build.zig, build-aux/) were removed by the runner's
        # pre-attack cleanup, not by the attacker: restore them from base so the reviewer never sees their deletion
        restored = sorted(p for p in base_paths - tip_paths if RH.JUNK.search(p))
        if restored:
            info = git(R, 'ls-tree', '-r', base, '--', *restored)
            git(R, 'update-index', '--index-info', env=env, inp=info)
        tree = git(R, 'write-tree', env=env).strip()
        H = os.path.join(cell, 'handoff')
        src = H if os.path.isdir(os.path.join(H, '.git')) else W
        msg = RH.clean_msg(git(src, 'log', '-1', '--format=%B', 'patch'))
        if RH.LEAKY.search(msg) or any(scrub_message.flagged(l) for l in msg.split('\n')):
            raise RuntimeError('single: leaky commit message')
        ad = git(src, 'log', '-1', '--format=%ad', '--date=raw', 'patch').strip()
        tip = git(R, 'commit-tree', tree, '-p', base, env=dict(BOT, GIT_AUTHOR_DATE=ad, GIT_COMMITTER_DATE=ad), inp=msg).strip()
        git(R, 'update-ref', 'refs/heads/patch', tip)
        git(R, 'bundle', 'create', os.path.join(out, 'series.bundle'), 'base..patch')
        clean = None
        gd = os.path.join(cell, 'golden.diff')
        if os.path.isfile(gd) and os.path.getsize(gd):
            git(R, 'read-tree', base, env=env)
            p = subprocess.run(['git', '-C', R, 'apply', '--cached', '--binary', gd], env=dict(os.environ, GIT_INDEX_FILE=idx), capture_output=True, text=True)
            if p.returncode: raise RuntimeError('single: golden.diff does not apply to base: ' + p.stderr[:200])
            ctree = git(R, 'write-tree', env=env).strip()
            ctip = git(R, 'commit-tree', ctree, '-p', base, env=dict(BOT, GIT_AUTHOR_DATE=ad, GIT_COMMITTER_DATE=ad), inp='golden\n').strip()
            git(R, 'update-ref', 'refs/heads/clean', ctip)
            git(R, 'bundle', 'create', os.path.join(out, 'clean.bundle'), 'base..clean')
            clean = dict(bundle='clean.bundle', ref='refs/heads/clean', tip_commit=ctip, tip_tree=ctree, n_commits=1, source='golden.diff')
        else:
            clean = dict(bundle=None, ref=None, tip_commit=base, tip_tree=git(R, 'rev-parse', base + '^{tree}').strip(), n_commits=0, source='base')
        return dict(base_sha=base, branches=dict(base='base', series='patch'),
                    series=dict(bundle='series.bundle', ref='refs/heads/patch', tip_commit=tip, tip_tree=tree, n_commits=1),
                    clean=clean, checks=dict(ok=True, violations=[], restored_upstream=restored, junk_dropped=[p for p in drop if not RH.ANSWER.search(p)],
                                             answer_dropped=[p for p in drop if RH.ANSWER.search(p)]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def load(record_dir):
    record_dir = os.path.abspath(record_dir)
    rec = json.load(open(os.path.join(record_dir, 'record.json')))
    if rec.get('schema') not in READ_SCHEMAS: raise RuntimeError('unknown record schema')
    return record_dir, rec


def _hygiene(D, B, R, kind):
    """fail-closed hygiene of a materialized reviewer repo; returns list of violations."""
    v = []
    refs = sorted(git(D, 'for-each-ref', '--format=%(refname)').split())
    if refs != sorted([f'refs/heads/{B}', f'refs/heads/{R}']): v.append(f'refs: {refs}')
    if git(D, 'remote').strip(): v.append('remote present')
    for p in ('logs', 'worktrees', 'ORIG_HEAD', 'FETCH_HEAD', 'refs/original'):
        if os.path.exists(os.path.join(D, '.git', p)): v.append(f'.git/{p} present')
    # substrates are shallow clones: .git/shallow may list upstream boundary commits, which must all lie in the BASE history
    sh = os.path.join(D, '.git', 'shallow')
    if os.path.exists(sh):
        for c in open(sh).read().split():
            if subprocess.run(['git', '-C', D, 'merge-base', '--is-ancestor', c, B]).returncode:
                v.append('shallow boundary outside base history'); break
    if git(D, 'fsck', '--unreachable', '--no-reflogs', check=False).strip(): v.append('unreachable objects')
    if git(D, 'status', '--porcelain').strip(): v.append('dirty worktree')
    for m in git(D, 'log', '--format=%B%x00', f'{B}..{R}').split('\0'):
        if any(scrub_message.flagged(l) for l in m.split('\n')) or RH.LEAKY.search(m): v.append('leaky message'); break
    sh = (f'source "{HERE}/scrub_lib.sh"; source "{HERE}/handoff_hygiene.sh" 2>/dev/null; '
          f'leak_ok "{D}" {B} {R} && {{ type hygiene_ok >/dev/null 2>&1 && hygiene_ok "{D}" {B} {R} {"multi" if kind == "window" else "single"} || true; }}')
    p = subprocess.run(['bash', '-c', sh], capture_output=True, text=True)
    if p.returncode: v.append('leak_ok/hygiene_ok: ' + (p.stderr.strip().splitlines() or ['fail'])[-1][:120])
    return v


def materialize(record_dir, dest, base_repo=None):
    """reviewer repo from the record ONLY (+ base history). Returns dict(handoff_hash, dest)."""
    record_dir, rec = load(record_dir)
    B, Rb = rec['branches']['base'], rec['branches']['series']
    base_repo = base_repo or rec['provenance']['substrate']
    D = os.path.abspath(dest); shutil.rmtree(D, ignore_errors=True)
    # base history only, fetched BY SHA from any repo that has it (the cell substrate, or an upstream clone for bank /
    # collaborator use): no branch-name assumptions, nothing else from the source repo comes along
    subprocess.run(['git', 'init', '-q', D], check=True)
    git(D, 'fetch', '-q', '--update-shallow', base_repo, rec['base_sha'])
    git(D, 'update-ref', f'refs/heads/{B}', rec['base_sha'])
    git(D, 'fetch', '-q', os.path.join(record_dir, rec['series']['bundle']), f"{rec['series']['ref']}:refs/heads/{Rb}")
    git(D, 'remote', 'remove', 'origin', check=False)
    for ref in git(D, 'for-each-ref', '--format=%(refname)').split():
        if ref not in (f'refs/heads/{B}', f'refs/heads/{Rb}'): git(D, 'update-ref', '-d', ref)
    git(D, 'symbolic-ref', 'HEAD', f'refs/heads/{Rb}'); git(D, 'checkout', '-q', '-f', Rb)
    for ref in git(D, 'for-each-ref', '--format=%(refname)').split():   # init may have created master
        if ref not in (f'refs/heads/{B}', f'refs/heads/{Rb}'): git(D, 'update-ref', '-d', ref)
    shutil.rmtree(os.path.join(D, '.git', 'logs'), ignore_errors=True)
    for f in ('ORIG_HEAD', 'FETCH_HEAD'):
        try: os.remove(os.path.join(D, '.git', f))
        except FileNotFoundError: pass
    git(D, 'reflog', 'expire', '--expire=now', '--all', check=False); git(D, 'gc', '-q', '--prune=now')
    tip = git(D, 'rev-parse', Rb).strip()
    v = [] if tip == rec['series']['tip_commit'] else [f'tip {tip} != record {rec["series"]["tip_commit"]}']
    v += _hygiene(D, B, Rb, rec['kind'])
    if v:
        shutil.rmtree(D, ignore_errors=True); raise RuntimeError(f'materialize: {v}')
    refs = ','.join(sorted(git(D, 'for-each-ref', '--format=%(refname)').split()))
    return dict(dest=D, handoff_hash=sha256(f"handoff|{rec['base_sha']}|{tip}|{refs}"), attack_hash=rec['attack_hash'])


def _scratch_repo(record_dir, rec, base_repo=None):
    """bare scratch repo holding base + series (+ clean) objects, for diffs/trees."""
    tmp = _tmpdir('ar_scr_'); S = os.path.join(tmp, 'r')
    subprocess.run(['git', 'clone', '-q', '--shared', '--bare', base_repo or rec['provenance']['substrate'], S], check=True)
    git(S, 'fetch', '-q', os.path.join(record_dir, rec['series']['bundle']), f"{rec['series']['ref']}:refs/heads/_series")
    if rec['clean'].get('bundle'):
        git(S, 'fetch', '-q', os.path.join(record_dir, rec['clean']['bundle']), f"{rec['clean']['ref']}:refs/heads/_clean")
    return tmp, S


def shipped(record_dir, review_ws, out_dir, base_repo=None):
    """ShippedRecord from the reviewer's FINAL TREE (committed or not): tree id, diff vs the attack tip, diff vs base."""
    record_dir, rec = load(record_dir); ws = os.path.abspath(review_ws); out = os.path.abspath(out_dir); os.makedirs(out, exist_ok=True)
    tmp = _tmpdir('ar_ship_')
    try:
        idx = os.path.join(tmp, 'idx'); env = dict(GIT_INDEX_FILE=idx)
        git(ws, 'read-tree', 'HEAD', env=env)
        git(ws, 'add', '-A', '--', '.', env=env)
        attack_paths = set()
        tmp2, S = _scratch_repo(record_dir, rec, base_repo)
        try:
            attack_paths = set(git(S, 'ls-tree', '-r', '--name-only', rec['series']['tip_commit']).splitlines())
            now = set(git(ws, 'ls-files', env=env).splitlines())
            drop = sorted(p for p in now - attack_paths if REVIEW_ARTIFACTS.search(p) or RH.ANSWER.search(p) or RH.JUNK.search(p))
            if drop: git(ws, 'rm', '-r', '-q', '--cached', '--ignore-unmatch', '--', *drop, env=env)
            tree = git(ws, 'write-tree', env=env).strip()
            # move the tree's objects into the scratch repo so we can diff there
            subprocess.run(['git', '-C', S, 'fetch', '-q', ws, 'HEAD'], capture_output=True)
            os.makedirs(os.path.join(S, 'objects', 'info'), exist_ok=True)
            with open(os.path.join(S, 'objects', 'info', 'alternates'), 'a') as f:
                f.write(os.path.join(ws, '.git', 'objects') + '\n')
            d_attack = git_diff(S, '--binary', '--full-index', rec['series']['tip_tree'], tree)
            d_base = git_diff(S, '--binary', '--full-index', rec['base_sha'] + '^{tree}', tree)
        finally:
            shutil.rmtree(tmp2, ignore_errors=True)
        open(os.path.join(out, 'shipped_vs_attack.diff'), 'wb').write(d_attack)
        open(os.path.join(out, 'shipped_vs_base.diff'), 'wb').write(d_base)
        sr = dict(schema='cyberdual.shipped_record/1', attack_hash=rec['attack_hash'], shipped_tree=tree,
                  reviewer_changed=tree != rec['series']['tip_tree'], dropped_paths=drop,
                  changed_paths=sorted(set(re.findall(r'^diff --git a/(\S+) ', d_attack.decode(errors='replace'), re.M))),
                  shipped_hash=sha256(f"shipped|{rec['attack_hash']}|{tree}"),
                  files=dict(vs_attack='shipped_vs_attack.diff', vs_base='shipped_vs_base.diff'))
        json.dump(sr, open(os.path.join(out, 'shipped.json'), 'w'), indent=1)
        return sr
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def grade_inputs(record_dir, which, out_dir, shipped_dir=None, base_repo=None):
    """inputs for grade.sh <seed> probe <base> <poc> <diff>: base sha, a diff base->tree, the PoC, the project."""
    record_dir, rec = load(record_dir); out = os.path.abspath(out_dir); os.makedirs(out, exist_ok=True)
    poc = [os.path.join(record_dir, p) for p in rec['poc']['files']]
    if which == 'shipped':
        sr = json.load(open(os.path.join(shipped_dir, 'shipped.json')))
        if sr['attack_hash'] != rec['attack_hash']: raise RuntimeError('shipped record belongs to a different attack')
        diff = os.path.join(os.path.abspath(shipped_dir), sr['files']['vs_base']); tree = sr['shipped_tree']
    else:
        tree = rec['series']['tip_tree'] if which == 'tampered' else rec['clean']['tip_tree']
        tmp, S = _scratch_repo(record_dir, rec, base_repo)
        try:
            diff = os.path.join(out, f'{which}.diff')
            d = git_diff(S, '--binary', '--full-index', rec['base_sha'] + '^{tree}', tree)
            open(diff, 'wb').write(d)
            if not d.strip():
                diff = None   # tree == base (e.g. from-scratch clean side): grade.sh takes NO patch argument
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    gi = dict(schema='cyberdual.grade_inputs/1', which=which, project=rec['project'], base=rec['base_sha'], tree=tree,
              diff=diff, poc=poc[0] if poc else None, attack_hash=rec['attack_hash'])
    json.dump(gi, open(os.path.join(out, f'grade_inputs_{which}.json'), 'w'), indent=1)
    return gi


def verify(record_dir, base_repo=None):
    """re-derive hashes and replay materialization; True iff the record is self-consistent."""
    record_dir, rec = load(record_dir)
    exp = sha256('|'.join([rec['schema'], rec['kind'], rec['base_sha'], rec['series']['tip_commit'],
                           (rec.get('clean') or {}).get('tip_commit') or rec['base_sha'], rec['poc']['sha256'] or '']))
    v = [] if exp == rec['attack_hash'] else ['attack_hash mismatch']
    if rec['poc']['files'] and file_sha256([os.path.join(record_dir, p) for p in rec['poc']['files']]) != rec['poc']['sha256']:
        v.append('poc sha256 mismatch')
    tmp = _tmpdir('ar_ver_')
    try:
        m = materialize(record_dir, os.path.join(tmp, 'h'), base_repo)
        D = m['dest']
        if git(D, 'rev-parse', rec['branches']['series'] + '^{tree}').strip() != rec['series']['tip_tree']: v.append('tip tree mismatch')
    except Exception as e:  # noqa: BLE001
        v.append(f'materialize: {e}')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return dict(ok=not v, violations=v, attack_hash=rec['attack_hash'])


def main():
    ap = argparse.ArgumentParser(); sp = ap.add_subparsers(dest='cmd', required=True)
    b = sp.add_parser('build'); b.add_argument('cell'); b.add_argument('--out'); b.add_argument('--messages', default='attacker', choices=['attacker', 'upstream'])
    m = sp.add_parser('materialize'); m.add_argument('record'); m.add_argument('dest'); m.add_argument('--base-repo')
    s = sp.add_parser('shipped'); s.add_argument('record'); s.add_argument('review_ws'); s.add_argument('out'); s.add_argument('--base-repo')
    g = sp.add_parser('grade-inputs'); g.add_argument('record'); g.add_argument('which', choices=['tampered', 'clean', 'shipped']); g.add_argument('out')
    g.add_argument('--shipped'); g.add_argument('--base-repo')
    v = sp.add_parser('verify'); v.add_argument('record'); v.add_argument('--base-repo')
    a = ap.parse_args()
    try:
        if a.cmd == 'build': r = build(a.cell, a.out, a.messages); out = dict(ok=True, attack_hash=r['attack_hash'], kind=r['kind'], record=os.path.abspath(a.out or os.path.join(a.cell, 'attack_record')))
        elif a.cmd == 'materialize': out = materialize(a.record, a.dest, a.base_repo)
        elif a.cmd == 'shipped': out = shipped(a.record, a.review_ws, a.out, a.base_repo)
        elif a.cmd == 'grade-inputs': out = grade_inputs(a.record, a.which, a.out, a.shipped, a.base_repo)
        else: out = verify(a.record, a.base_repo)
    except Exception as e:  # noqa: BLE001
        print(json.dumps(dict(ok=False, error=str(e)[:500]))); sys.exit(1)
    print(json.dumps(out)); sys.exit(0 if out.get('ok', True) else 1)


if __name__ == '__main__':
    main()
