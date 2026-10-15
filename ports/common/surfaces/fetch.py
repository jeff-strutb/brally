"""fetch.py: the 2K maps of every set surfaces.txt names (Poly Haven, CC0) into src/:
<id>_diff.jpg, _nor_gl, _rough, _disp; files already present are kept. Then bake.py."""
import json, os, sys, urllib.request
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
os.makedirs(D + '/src', exist_ok=True)
UA = {'User-Agent': 'brally-remaster/1.0'}
def get(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read()
ids = [l.split()[0] for l in open(D + '/surfaces.txt') if l.strip() and not l.startswith('#')]
jobs = []
for i in ids:
    files = json.loads(get('https://api.polyhaven.com/files/' + i))
    for key, name in (('Diffuse', 'diff'), ('nor_gl', 'nor_gl'), ('Rough', 'rough'), ('Displacement', 'disp')):
        e = files[key]['2k']
        fmt = 'png' if name in ('disp', 'nor_gl') and 'png' in e else 'jpg'
        out = '%s/src/%s_%s.%s' % (D, i, name, fmt)
        if not os.path.exists(out):
            jobs.append((e[fmt]['url'], out, e[fmt].get('md5')))
def one(j):
    url, out, md5 = j
    b = get(url)
    if md5:
        import hashlib
        if hashlib.md5(b).hexdigest() != md5:
            return 'FAIL md5 ' + out
    open(out + '.part', 'wb').write(b); os.replace(out + '.part', out)
    return 'ok ' + os.path.basename(out)
with ThreadPoolExecutor(6) as ex:
    for r in ex.map(one, jobs):
        print(r)
print(len(ids), 'sets,', len(jobs), 'files fetched')
