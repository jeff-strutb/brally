import os, re, subprocess, sys, concurrent.futures as cf
sys.path.insert(0, 'tools')
from reloc_fill import parse
SRC = 'src/brally/core/drawing/br_hudscene.c'
base = open(SRC).read()
ANCH = '#define SCR_iView DAT_106ec798\n'
assert base.count(ANCH) == 1
# (function, body offset, wanted symbol prefix at that site)
WANT = [('_BrWeatherStepParticles', 0x1a2, 'g_weather'), ('_BrWeatherStepParticles', 0x2ca, 'g_weather'),
        ('_BrWeatherStepParticles', 0x214, 'g_weather'), ('_BrWeatherStepLightning', 0xc, 'g_weather')]

def site_syms(obj):
    d, secs, syms, relocs = parse(obj)
    by = {s['idx']: s for s in syms}
    out = {}
    for fn in set(w[0] for w in WANT):
        fs = next(s for s in syms if s['name'] == fn and s['sec'] > 0)
        out[fn] = {rva - fs['val']: by[si]['name'] for rva, si, rt in relocs[fs['sec']]}
    return out

def trial(n):
    pad = ''.join('extern int br_hud_pad_%03d;\n' % i for i in range(n))
    t = base.replace(ANCH, ANCH + pad)
    d = 'build/brally/win32/match/t3d/pad/%d' % n
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, 'br_hudscene.c')
    open(src, 'w').write(t)
    obj = os.path.join(d, 'h.obj')
    subprocess.run(['sh', 'tools/toolchains/wine.sh', 'tools/toolchains/msvc5/bin/cl.exe', '/nologo', '/O2', '/W3', '/I', 'src/brally/core/drawing',
                    '/I', 'include', '/I', 'tools/toolchains/msvc5-compat', '/I', 'tools/toolchains/msvc5/include', '/DBR_MATCHING_BUILD',
                    '/c', src, '/Fo' + obj.replace('/', '\\')], capture_output=True)
    if not os.path.exists(obj):
        return n, None
    s = site_syms(obj)
    ok = all(s[f].get(o, '').lstrip('_').startswith(w) for f, o, w in WANT)
    return n, ok

ns = [int(a) for a in sys.argv[1:]] or list(range(0, 300, 1))
with cf.ThreadPoolExecutor(12) as ex:
    for n, ok in ex.map(trial, ns):
        if ok:
            print('PAD', n, 'OK', flush=True)
print('done')
