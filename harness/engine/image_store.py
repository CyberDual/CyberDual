#!/usr/bin/env python3
"""image_store.py — keep engine Docker images (shipped / handoff attack trees, 25-40 GB each, mostly shared layers)
OUT of the Docker root, in a content-addressed store on a big disk, and load them back on demand.

Store layout (CYBERDUAL_IMAGE_STORE, default ~/.cache/cyberdual/engine-images):
  blobs/sha256/<digest>.zst   every OCI blob (layer, config, manifest) once, zstd-compressed; shared layers stored once
  images/<tag-file>.json      one image: tag, image id, manifest descriptor, blob list with uncompressed sizes
  index.jsonl                 append-only log of stored / removed / loaded images

Commands:
  save TAG [TAG ...]   docker save -> store; every blob is hashed while it is written and must equal its digest; the
                       image id (config digest) must equal `docker image inspect` Id; then the image record is written.
  verify TAG [...]     re-read every blob of the image: zstd integrity + sha256 of the decompressed bytes == digest.
  remove TAG [...]     docker rmi, only if the image is stored AND verifies (never otherwise).
  ensure TAG [...]     if the tag is missing in Docker, rebuild its OCI archive from the store and `docker load` it,
                       then check the loaded image id. Exit 0 when every tag is present afterwards.
  list                 stored images.
Python callers: `from image_store import ensure; ensure(tag)` (returns True when the tag is usable).
"""
import hashlib, io, json, os, subprocess, sys, tarfile, time

os.umask(0o077)   # the store holds image configs (some old engine images carry an agent credential): owner-only
STORE = os.environ.get('CYBERDUAL_IMAGE_STORE', os.path.expanduser('~/.cache/cyberdual/engine-images'))
SMALL = 1 << 20          # blobs below this size (manifests, configs, index) are also kept in memory while saving
now = lambda: time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())


def _p(*a): return os.path.join(STORE, *a)
def _blob(d): return _p('blobs', 'sha256', d + '.zst')
def _rec(tag): return _p('images', tag.replace('/', '_').replace(':', '__') + '.json')


def _log(**kw):
    os.makedirs(STORE, exist_ok=True)
    with open(_p('index.jsonl'), 'a') as f: f.write(json.dumps(dict(at=now(), **kw)) + '\n')


def image_id(tag):
    r = subprocess.run(['docker', 'image', 'inspect', tag, '--format', '{{.Id}}'], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def _store_blob(digest, fobj, size):
    """write one blob (stream) compressed; return True if its sha256 equals digest. Existing blobs are skipped."""
    dst = _blob(digest)
    if os.path.exists(dst):
        while fobj.read(1 << 22): pass
        return True
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    tmp = f'{dst}.part{os.getpid()}'
    z = subprocess.Popen(['zstd', '-q', '-T8', '-3', '-o', tmp, '-f'], stdin=subprocess.PIPE)
    h = hashlib.sha256(); n = 0
    for chunk in iter(lambda: fobj.read(1 << 22), b''):
        h.update(chunk); n += len(chunk); z.stdin.write(chunk)
    z.stdin.close()
    if z.wait() != 0 or h.hexdigest() != digest or n != size:
        os.path.exists(tmp) and os.remove(tmp)
        return False
    os.replace(tmp, dst)
    return True


def save(tags):
    for t in tags:
        if not image_id(t): sys.exit(f'not in docker: {t}')
    ids = {t: image_id(t) for t in tags}
    p = subprocess.Popen(['docker', 'save'] + list(tags), stdout=subprocess.PIPE)
    small, sizes, bad = {}, {}, []
    with tarfile.open(fileobj=p.stdout, mode='r|') as tf:
        for m in tf:
            if not m.isfile(): continue
            f = tf.extractfile(m)
            if m.name.startswith('blobs/sha256/'):
                d = m.name.rsplit('/', 1)[1]; sizes[d] = m.size
                if m.size < SMALL:
                    data = f.read(); small[d] = data
                    if not _store_blob(d, io.BytesIO(data), m.size): bad.append(d)
                elif not _store_blob(d, f, m.size): bad.append(d)
            else:
                small[m.name] = f.read()
    if p.wait() != 0: sys.exit('docker save failed')
    if bad: sys.exit(f'{len(bad)} blobs did not match their digest: {bad[:3]}')
    index = json.loads(small['index.json'])
    os.makedirs(_p('images'), exist_ok=True)
    for desc in index['manifests']:
        name = desc.get('annotations', {}).get('io.containerd.image.name', '')
        tag = next((t for t in tags if name.endswith('/' + t) or name == t or name.endswith('/library/' + t)), None)
        if tag is None: continue
        man = json.loads(small[desc['digest'].split(':')[1]])
        cfg = man['config']['digest']
        if cfg != ids[tag]: sys.exit(f'{tag}: config {cfg} != docker image id {ids[tag]}')
        blobs = [desc['digest'], cfg] + [l['digest'] for l in man['layers']]
        rec = dict(tag=tag, image_id=ids[tag], manifest=desc, legacy_layers=[l['digest'] for l in man['layers']],
                   blobs=[dict(digest=b, size=sizes[b.split(':')[1]]) for b in blobs], saved_at=now(),
                   bytes_uncompressed=sum(sizes[b.split(':')[1]] for b in blobs))
        json.dump(rec, open(_rec(tag) + '.tmp', 'w'), indent=1); os.replace(_rec(tag) + '.tmp', _rec(tag))
        _log(op='saved', tag=tag, image_id=ids[tag], blobs=len(blobs))
        print(f'saved {tag} ({len(blobs)} blobs, {rec["bytes_uncompressed"] / 1e9:.1f} GB uncompressed)')
    missing = [t for t in tags if not os.path.exists(_rec(t))]
    if missing: sys.exit(f'not found in the saved archive: {missing}')


def record(tag):
    try: return json.load(open(_rec(tag)))
    except (OSError, ValueError): return None


def verify(tag, quiet=False):
    rec = record(tag)
    if not rec: print(f'not stored: {tag}'); return False
    for b in rec['blobs']:
        d = b['digest'].split(':')[1]; path = _blob(d)
        if not os.path.exists(path): print(f'{tag}: missing blob {d}'); return False
        z = subprocess.Popen(['zstd', '-q', '-dc', path], stdout=subprocess.PIPE)
        h = hashlib.sha256(); n = 0
        for chunk in iter(lambda: z.stdout.read(1 << 22), b''): h.update(chunk); n += len(chunk)
        if z.wait() != 0 or h.hexdigest() != d or n != b['size']:
            print(f'{tag}: blob {d} does not verify'); return False
    quiet or print(f'verified {tag} ({len(rec["blobs"])} blobs)')
    return True


def remove(tags):
    for t in tags:
        if not verify(t, quiet=True): print(f'NOT removing {t}: not stored or does not verify'); continue
        if image_id(t) != record(t)['image_id']: print(f'NOT removing {t}: docker id differs from the stored one'); continue
        r = subprocess.run(['docker', 'rmi', t], capture_output=True, text=True)
        if r.returncode: print(f'NOT removed {t}: {r.stderr.strip()[:200]}'); continue
        _log(op='removed_from_docker', tag=t, image_id=record(t)['image_id']); print(f'removed {t} from docker (stored)')


def _emit(tf, name, data):
    ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = 0; tf.addfile(ti, io.BytesIO(data))


def ensure(tag):
    """the tag is present in Docker afterwards (loaded from the store if needed); True on success"""
    if image_id(tag): return True
    rec = record(tag)
    if not rec: print(f'image_store: {tag} is neither in docker nor stored', file=sys.stderr); return False
    p = subprocess.Popen(['docker', 'load'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    with tarfile.open(fileobj=p.stdin, mode='w|') as tf:
        _emit(tf, 'oci-layout', b'{"imageLayoutVersion":"1.0.0"}')
        _emit(tf, 'index.json', json.dumps(dict(schemaVersion=2, mediaType='application/vnd.oci.image.index.v1+json',
                                                manifests=[rec['manifest']])).encode())
        _emit(tf, 'manifest.json', json.dumps([dict(Config='blobs/sha256/' + rec['image_id'].split(':')[1], RepoTags=[tag],
                                                    Layers=['blobs/sha256/' + l.split(':')[1] for l in rec['legacy_layers']])]).encode())
        seen = set()
        for b in rec['blobs']:
            d = b['digest'].split(':')[1]
            if d in seen: continue
            seen.add(d)
            z = subprocess.Popen(['zstd', '-q', '-dc', _blob(d)], stdout=subprocess.PIPE)
            ti = tarfile.TarInfo('blobs/sha256/' + d); ti.size = b['size']; ti.mtime = 0
            tf.addfile(ti, z.stdout)
            if z.wait() != 0: print(f'image_store: blob {d} failed to decompress', file=sys.stderr)
    p.stdin.close(); out = p.stdout.read().decode(errors='replace'); rc = p.wait()
    ok = rc == 0 and image_id(tag) == rec['image_id']
    _log(op='loaded' if ok else 'load_failed', tag=tag, image_id=rec['image_id'], docker=out.strip()[-200:])
    print(f'image_store: {"loaded" if ok else "FAILED to load"} {tag}', file=sys.stderr)
    return ok


def main(a):
    if not a: sys.exit(__doc__)
    cmd, tags = a[0], a[1:]
    if cmd == 'save': save(tags)
    elif cmd == 'verify': sys.exit(0 if all([verify(t) for t in tags]) else 1)
    elif cmd == 'remove': remove(tags)
    elif cmd == 'ensure': sys.exit(0 if all([ensure(t) for t in tags]) else 1)
    elif cmd == 'list':
        d = _p('images')
        for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            r = json.load(open(os.path.join(d, f))); print(r['tag'], r['image_id'][:19], f"{r['bytes_uncompressed'] / 1e9:.1f}GB")
    else: sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
