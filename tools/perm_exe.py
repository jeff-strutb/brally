#!/usr/bin/env python3
"""Drive the deterministic permuter (tools/permute.py) at an in-scope EXE
function. permute.py's anneal() is parameterized on orig_bytes/opts/func_name;
only its seed/orig loaders are DLL-keyed. This supplies EXE orig bins + /ML|/MT
opts so the free CPU grinder can attack the EXE coloring/frame walls too.

    python3 tools/perm_exe.py --exe setvideo --va 0x00401150 --name CHK_FGets \
        --src build/setvideo_work/0x00401150.c --secs 600 --seed 1
On a byte-exact hit it writes build/<exe>_work/<VA>.permuted.c and prints MATCH.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import permute

CRT = {'brally': '/MD', 'setvideo': '/ML', 'bossrally': '/MT'}
ROOT = permute.ROOT

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', required=True)
    ap.add_argument('--va', required=True)
    ap.add_argument('--name', required=True)
    ap.add_argument('--src', required=True)
    ap.add_argument('--secs', type=int, default=600)
    ap.add_argument('--iters', type=int, default=200000)
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--sfx', default=None)
    ap.add_argument('--objective', default='bytes')
    a = ap.parse_args()
    permute._WHOLE_FILE = True
    permute._WORKER_SFX = a.sfx if a.sfx else ('_ex%d' % a.seed)

    if a.objective == 'region':
        import regioncount
        _orig_score = permute.score_src
        def _region_score(src, func_name, orig_bytes, opts, tag):
            # compile via original path to get (nd,opt,rb,relocs); replace nd with region count
            nd, opt, rb, relocs = _orig_score(src, func_name, orig_bytes, opts, tag)
            if rb is None:
                return (None, opt, rb, relocs)
            rc = regioncount.region_count(orig_bytes, rb, relocs)
            if rc == 0:
                # regions aligned; fall back to true byte-diff so match == byte-exact
                import match_sweep
                _, bd, _ = match_sweep.score(orig_bytes, rb, relocs)
                return (bd, opt, rb, relocs)
            return (rc, opt, rb, relocs)
        permute.score_src = _region_score
    va = '0x%08X' % int(a.va, 16)
    orig = open(os.path.join(ROOT, 'build', 'match',
                'orig_%s' % a.exe, va + '.bin'), 'rb').read()
    opts = ['/O2 %s' % CRT[a.exe], '/O2 /Oy- %s' % CRT[a.exe]]
    src = open(a.src).read()
    res = permute.anneal(va, src, a.name, orig, opts, a.iters,
                         seed=a.seed, max_seconds=a.secs)
    if res and res['diffs'] == 0:
        out = os.path.join(ROOT, 'build', '%s_work' % a.exe, va + '.permuted.c')
        open(out, 'w').write(res['src'])
        print('WROTE', out)

if __name__ == '__main__':
    main()
