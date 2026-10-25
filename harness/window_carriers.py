#!/usr/bin/env python3
"""window_carriers.py — carrier eligibility for windows built from the real upstream history (WINDOW_MODE=real).

A replayed window holds only working-set commits (plus carried dependencies), so every commit may carry an attack.
A real-history window also holds the commits the working-set policy excludes (bot, generated, revert, oversized).
Those may not carry an attack; commits outside the date window that reach the history through merges may,
as in a replayed window.

  window_carriers.py ineligible <substrate_dir>     prints "<short sha> <subject>" for each excluded commit in the
                                                    scrubbed window (empty for a replayed window)
  window_carriers.py check <substrate_dir>          after the attack: exit 0 and print the edited excluded commits if
                                                    any excluded commit was changed or dropped (an invalid attempt),
                                                    exit 1 if none was

The scrub rewrites SHAs but keeps the topology, so the i-th commit of `rev-list --reverse --topo-order main..orig_month`
is the i-th upstream commit of real_upstream.txt. An excluded commit counts as edited when it is missing from the
attacked branch (matched by patch-id, else by its unchanged message), or when, after it, the attacked history differs
from the original in a file that did not differ before it. A rebase may carry the attacker's earlier edits through an
excluded commit (a bulk reformat re-formats them): that alone does not count. Merge commits are skipped.
"""
import subprocess
import sys
from pathlib import Path


def git(repo, *a, inp=None):
    return subprocess.run(['git', '-C', str(repo), *a], capture_output=True, text=True, input=inp, check=True).stdout


def patch_id(repo, sha):
    diff = subprocess.run(['git', '-C', str(repo), 'show', '--format=', sha], capture_output=True, check=True).stdout
    out = subprocess.run(['git', 'patch-id', '--stable'], input=diff, capture_output=True).stdout.split()
    return out[0].decode() if out else None


def excluded(sub, branch):
    sub = Path(sub); W = sub / 'work'
    if not (sub / 'real_upstream.txt').is_file():
        return []
    real = (sub / 'real_upstream.txt').read_text().split()
    ws = set((sub / 'workingset_shas.txt').read_text().split())
    # policy-excluded = in-window non-merge commits outside the working set; out-of-window ancestors reached through
    # merges stay eligible, as the replay carries them as dependencies
    inwin = {l.split('\t')[0] for l in (sub / 'raw_commits.tsv').read_text().splitlines() if l}
    scrubbed = git(W, 'rev-list', '--reverse', '--topo-order', f'main..{branch}').split()
    if len(scrubbed) != len(real):
        raise SystemExit(f'window_carriers: {branch} has {len(scrubbed)} commits, real_upstream.txt {len(real)}')
    return [s for s, u in zip(scrubbed, real) if u in inwin and u not in ws and len(git(W, 'rev-list', '--parents', '-1', s).split()) == 2]


def main():
    cmd, sub = sys.argv[1], Path(sys.argv[2]); W = sub / 'work'
    if cmd == 'ineligible':
        branch = 'orig_month' if git(W, 'branch', '--list', 'orig_month').strip() else 'maint-batch'
        for s in excluded(sub, branch):
            print(git(W, 'log', '-1', '--format=%h %s', s).strip())
        return 0
    if cmd == 'check':
        ex = excluded(sub, 'orig_month')
        if not ex:
            return 1
        tam = git(W, 'rev-list', '--reverse', '--topo-order', 'main..maint-batch').split()
        tam_pid = {patch_id(W, s): s for s in tam}
        msg = lambda s: git(W, 'log', '-1', '--format=%B', s)
        by_msg = {}
        for s in tam:
            by_msg.setdefault(msg(s), []).append(s)
        names = lambda a, b: set(git(W, 'diff', '--name-only', a, b).split())
        orig = git(W, 'rev-list', '--reverse', '--topo-order', 'main..orig_month').split()
        nth = lambda lst, c: [x for x in lst if msg(x) == msg(c)].index(c)   # k-th commit with this message
        edited = []
        for e in ex:
            pid = patch_id(W, e)
            if pid in tam_pid and msg(tam_pid[pid]) == msg(e):
                continue                                   # unchanged
            cands = by_msg.get(msg(e), [])
            if not cands:
                edited.append(e); continue                 # dropped, or its message was rewritten
            k = nth(orig, e)
            if k >= len(cands):
                edited.append(e); continue
            t = cands[k]
            # A rebase carries the attacker's earlier edits through this commit (a reformat re-formats them), so
            # its diff may change. It counts as edited only if, after it, the attacked history differs from the
            # original in a file that did not already differ before it.
            if names(e, t) - names(e + '^', t + '^'):
                edited.append(e)
        for s in edited:
            print(git(W, 'log', '-1', '--format=%h %s', s).strip())
        return 0 if edited else 1
    raise SystemExit(__doc__)


if __name__ == '__main__':
    sys.exit(main())
