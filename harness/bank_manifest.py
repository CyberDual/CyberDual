#!/usr/bin/env python3
"""bank_manifest.py: the sha256 manifest of an attack bank folder (attack-bank/attacks/MANIFEST.sha256).

  python3 harness/bank_manifest.py write <bank>    write <bank>/MANIFEST.sha256 (sha256sum format, sorted paths)
  python3 harness/bank_manifest.py check <bank>    check every listed file; exit 1 on a missing or changed file

The manifest lists every file under the bank except itself, as `<sha256>  <path relative to the bank>`, so
`cd <bank> && sha256sum -c MANIFEST.sha256` checks it too. `./cyberdual bank manifest` writes it and
`./cyberdual verify-bank` checks a bank against it.
"""
import hashlib
import sys
from pathlib import Path

NAME = 'MANIFEST.sha256'


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for block in iter(lambda: fh.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def files(root):
    root = Path(root)
    return sorted(p.relative_to(root).as_posix() for p in root.rglob('*')
                  if p.is_file() and p.name != NAME and not p.name.endswith('.part'))


def parse(text):
    """manifest text -> {path: sha256}"""
    out = {}
    for line in text.splitlines():
        if line.strip():
            digest, path = line.split('  ', 1)
            out[path] = digest
    return out


def write(root):
    root = Path(root)
    lines = [f'{sha256(root / f)}  {f}\n' for f in files(root)]
    (root / NAME).write_text(''.join(lines))
    return len(lines)


def check(root):
    """-> (ok, changed, missing, extra) lists of paths"""
    root = Path(root)
    want = parse((root / NAME).read_text())
    ok, changed, missing = [], [], []
    for path, digest in want.items():
        p = root / path
        if not p.is_file():
            missing.append(path)
        elif sha256(p) != digest:
            changed.append(path)
        else:
            ok.append(path)
    extra = sorted(set(files(root)) - set(want))
    return ok, changed, missing, extra


def main(argv):
    if len(argv) != 3 or argv[1] not in ('write', 'check'):
        sys.exit(__doc__)
    root = Path(argv[2])
    if argv[1] == 'write':
        print(f'{root / NAME}: {write(root)} files')
        return 0
    if not (root / NAME).is_file():
        sys.exit(f'no {NAME} in {root}')
    ok, changed, missing, extra = check(root)
    for label, paths in (('changed', changed), ('missing', missing), ('not in manifest', extra)):
        for p in paths[:20]:
            print(f'  {label}: {p}')
    print(f'{len(ok)}/{len(ok) + len(changed) + len(missing)} files match {NAME}'
          + (f'; {len(changed)} changed, {len(missing)} missing' if changed or missing else '')
          + (f'; {len(extra)} files not in the manifest' if extra else ''))
    return 1 if changed or missing else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
