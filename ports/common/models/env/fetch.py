#!/usr/bin/env python3
"""Fetch the Poly Haven (CC0) environment sources listed in manifest.json.

Models land as polyhaven/models/<id>/<id>_4k.blend plus the textures the .blend
references (kept at the relative paths the .blend expects). Surfaces land as
polyhaven/textures/<id>/<id>_<map>_4k.<ext>. Every file is checked against the
API's md5; a file that already matches is skipped, so a rerun resumes.
"""
import hashlib, json, os, sys, urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'polyhaven')
RES = '4k'
UA = {'User-Agent': 'brally-remaster-fetch'}
MAPS = {'Diffuse': 'jpg', 'nor_gl': 'png', 'Rough': 'jpg', 'AO': 'jpg', 'Displacement': 'png', 'arm': 'jpg'}

def api(path):
    rq = urllib.request.Request('https://api.polyhaven.com/' + path, headers=UA)
    with urllib.request.urlopen(rq, timeout=60) as r:
        return json.load(r)

def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for blk in iter(lambda: f.read(1 << 20), b''):
            h.update(blk)
    return h.hexdigest()

def fetch(url, dest, want_md5):
    if os.path.exists(dest) and md5(dest) == want_md5:
        return 'skip'
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + '.part'
    for attempt in range(4):
        try:
            rq = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(rq, timeout=120) as r, open(tmp, 'wb') as f:
                while True:
                    blk = r.read(1 << 20)
                    if not blk:
                        break
                    f.write(blk)
            if md5(tmp) != want_md5:
                raise IOError('md5 mismatch')
            os.replace(tmp, dest)
            return 'ok'
        except Exception as e:
            err = e
    raise RuntimeError('%s: %s' % (url, err))

def jobs_for_model(i):
    b = api('files/' + i)['blend'][RES]['blend']
    root = os.path.join(OUT, 'models', i)
    yield b['url'], os.path.join(root, os.path.basename(b['url'])), b['md5']
    for rel, inc in b.get('include', {}).items():
        yield inc['url'], os.path.join(root, rel), inc['md5']

def jobs_for_texture(i):
    f = api('files/' + i)
    root = os.path.join(OUT, 'textures', i)
    for key, ext in MAPS.items():
        if key not in f or RES not in f[key]:
            continue
        forms = f[key][RES]
        e = ext if ext in forms else sorted(forms)[0]
        x = forms[e]
        yield x['url'], os.path.join(root, os.path.basename(x['url'])), x['md5']

def main():
    m = json.load(open(os.path.join(HERE, 'manifest.json')))
    ids_m = [i for v in m['models'].values() for i in v]
    ids_t = [i for v in m['textures'].values() for i in v]
    with ThreadPoolExecutor(16) as ex:
        lists = list(ex.map(lambda i: list(jobs_for_model(i)), ids_m)) + \
                list(ex.map(lambda i: list(jobs_for_texture(i)), ids_t))
    jobs = [j for l in lists for j in l]
    print('%d files for %d models + %d surfaces' % (len(jobs), len(ids_m), len(ids_t)), flush=True)
    done = fail = 0
    with ThreadPoolExecutor(12) as ex:
        futs = {ex.submit(fetch, *j): j for j in jobs}
        for fu in as_completed(futs):
            try:
                fu.result(); done += 1
            except Exception as e:
                fail += 1; print('FAIL', e, flush=True)
            if (done + fail) % 50 == 0:
                print('%d/%d' % (done + fail, len(jobs)), flush=True)
    for i in ids_m:
        with open(os.path.join(OUT, 'models', i, 'info.json'), 'w') as f:
            json.dump(api('info/' + i), f, indent=1)
    print('done %d, failed %d' % (done, fail))
    sys.exit(1 if fail else 0)

if __name__ == '__main__':
    main()
