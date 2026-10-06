"""Build an IDO 5.3 matched-function corpus from other N64 decomps.

    .venv/bin/python tools/tgrally/n64corpus/build_corpus.py [repo ...]

Each repo's C files are compiled with our IDO 5.3 (tools/toolchains/ido53/cc) under a
few flag sets; every carved function whose relocation-masked words occur in
that game's ROM is kept (the ROM confirms the match, whatever the flags).
Writes build/tgrally/ext/n64corpus/corpus.jsonl (one function per line).
"""
import glob
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'tgrally'))
import n64build as B  # noqa: E402

CC = os.path.join(ROOT, 'tools', 'toolchains', 'ido53', 'cc')
BASE = ['-c', '-G', '0', '-non_shared', '-w', '-Wab,-r4300_mul', '-Xcpluscomm']
OPTS = [['-O2', '-mips2'], ['-O1', '-mips2'], ['-O2', '-mips1'], ['-O2', '-mips2', '-g3'], ['-g', '-mips2'],
        ['-O3', '-mips2'], ['-O1', '-mips1']]

REPOS = {
    'sm64': dict(rom='Super Mario 64 (USA).z64', src=['src', 'lib/src'],
                 inc=['include', 'include/libc', 'src', '.', 'lib/src'],
                 defs=['VERSION_US=1', 'F3D_OLD=1', '_FINALROM=1', 'TARGET_N64=1', 'NON_MATCHING=0', 'AVOID_UB=0',
                       '_LANGUAGE_C=1', 'BUILD_DIR=build/us']),
    '007': dict(rom='GoldenEye 007 (USA).z64', src=['src'],
                inc=['.', 'include', 'include/ultra64', 'include/PR', 'src', 'src/game', 'src/inflate'],
                defs=['TARGET_N64', 'VERSION_US', '_FINALROM', 'BUGFIX_R1=0', 'NDEBUG']),
    'banjo-kazooie': dict(rom='Banjo-Kazooie (USA).z64', src=['src'],
                          inc=['.', 'include', 'lib/ultralib/include', 'lib/ultralib/include/PR',
                               'lib/ultralib/include/PRinternal', 'lib/ultralib/include/compiler/ido',
                               'include/n_audio/PR', 'lib/ultralib/src/audio'],
                          defs=['_FINALROM', 'F3DEX_GBI', 'VERSION=VERSION_USA', 'VERSION_USA=0', 'NDEBUG',
                                'BUILD_VERSION=VERSION_I', 'BKDIFFS', 'ANTI_TAMPER=1', 'ANTI_PIRACY=1']),
    'mk64': dict(rom='Mario Kart 64 (USA).z64', src=['src', 'courses'],
                 inc=['include', 'include/libc', 'src', 'src/racing', 'src/ending', '.', 'courses'],
                 defs=['VERSION_US=1', 'TARGET_N64=1', '_FINALROM=1', 'F3DEX_OLD=1', 'GBI_FLOATS=0',
                       '_LANGUAGE_C=1', 'AVOID_UB=0', 'NON_MATCHING=0']),
}


def rare_blobs(d):
    """Rare's rarezip blocks (0x1172, 4-byte length, raw deflate), inflated."""
    import zlib
    out = []
    i = d.find(b'\x11\x72')
    while i >= 0:
        try:
            z = zlib.decompressobj(wbits=-15)
            r = z.decompress(d[i + 6:i + 6 + 0x200000])
            if len(r) >= 0x400:
                out.append(r)
        except zlib.error:
            pass
        i = d.find(b'\x11\x72', i + 2)
    return out


class RomIndex:
    def __init__(self, path):
        d = open(path, 'rb').read()
        for b in rare_blobs(d):
            d += b'\0' * ((-len(d)) % 16) + b'\0' * 16 + b
        self.w = list(struct.unpack('>%dI' % (len(d) // 4), d[:len(d) // 4 * 4]))
        self.ix = {}
        for i, x in enumerate(self.w):
            self.ix.setdefault(x, []).append(i)

    def find(self, words, mask):
        best = None
        for k, (x, m) in enumerate(zip(words, mask)):
            if m == 0xffffffff and x not in (0, 0x03e00008, 0x27bdffe8):
                n = len(self.ix.get(x, ()))
                if n == 0:
                    return None
                if best is None or n < best[1]:
                    best = (k, n)
        if best is None:
            return None
        k = best[0]
        hits = []
        for p in self.ix[words[k]]:
            s = p - k
            if s < 0 or s + len(words) > len(self.w):
                continue
            if all((self.w[s + j] & m) == (x & m) for j, (x, m) in enumerate(zip(words, mask))):
                hits.append(s * 4)
                if len(hits) > 3:
                    break
        return hits or None


def masked(obj, start, end):
    ti, text = obj.sec('.text')
    ws = B.words(text[start:end])
    while ws and ws[-1] == 0:
        ws.pop()
    mask = [0xffffffff] * len(ws)
    for o, t, si in obj.rels.get(ti, []):
        if start <= o < start + 4 * len(ws):
            k = (o - start) // 4
            if t == 4:
                mask[k] = 0xfc000000
            elif t in (5, 6):
                mask[k] = 0xffff0000
            elif t == 2:
                mask[k] = 0
    return ws, mask


def func_source(src, name):
    m = re.search(r'^[^\n;{}#]*\b%s\s*\([^;{]*\)\s*\{' % re.escape(name), src, re.M)
    if not m:
        return None
    i = src.index('{', m.end() - 1)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == '{':
            depth += 1
        elif src[j] == '}':
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


def compile_file(repo, cfg, path, opt):
    fd, o = tempfile.mkstemp(suffix='.o')
    os.close(fd)
    cmd = [CC] + BASE + opt + ['-I' + i for i in cfg['inc']] + ['-D' + d for d in cfg['defs']] + ['-o', o, path]
    p = subprocess.run(cmd, cwd=os.path.join(HERE, repo), capture_output=True, text=True)
    if p.returncode or not os.path.exists(o) or not os.path.getsize(o):
        if os.path.exists(o):
            os.unlink(o)
        return None
    obj = B.Obj(o)
    os.unlink(o)
    return obj


def main():
    repos = sys.argv[1:] or list(REPOS)
    out = open(os.path.join(HERE, 'corpus.jsonl'), 'a')
    for repo in repos:
        cfg = REPOS[repo]
        rom = RomIndex(os.path.join(HERE, repo, cfg['rom']))
        files = []
        for s in cfg['src']:
            files += glob.glob(os.path.join(HERE, repo, s, '**', '*.c'), recursive=True)
        allsrc = {f: open(f, encoding='latin1').read() for f in files}
        files = sorted(set(os.path.relpath(f, os.path.join(HERE, repo)) for f in files if not f.endswith('.inc.c')))

        def find_c(name, own):
            c = func_source(own, name)
            if c:
                return c
            for t in allsrc.values():
                if name + '(' in t:
                    c = func_source(t, name)
                    if c:
                        return c
            return None
        seen = {}
        stats = dict(files=len(files), ccfail=0, funcs=0, found=0)

        def work(f):
            src = open(os.path.join(HERE, repo, f), encoding='latin1').read()
            res = {}
            compiled = False
            for opt in OPTS:
                obj = compile_file(repo, cfg, f, opt)
                if obj is None:
                    continue
                compiled = True
                for name, s, e in B.carve(obj):
                    if not name or name in res:
                        continue
                    ws, mask = masked(obj, s, e)
                    if len(ws) < 3:
                        continue
                    hits = rom.find(ws, mask)
                    if hits:
                        res[name] = dict(repo=repo, file=f, name=name, flags=' '.join(opt), rom=hits[0],
                                         n=len(ws), words=ws, mask=mask, c=find_c(name, src))
            return f, compiled, res

        with ThreadPoolExecutor(14) as ex:
            for f, compiled, res in ex.map(work, files):
                if not compiled:
                    stats['ccfail'] += 1
                for name, r in res.items():
                    key = (r['rom'], name)
                    if key in seen:
                        continue
                    seen[key] = 1
                    stats['found'] += 1
                    out.write(json.dumps(r) + '\n')
        out.flush()
        print(repo, stats, flush=True)


if __name__ == '__main__':
    main()
