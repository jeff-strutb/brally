#!/usr/bin/env python3
"""Read a symbol's original address back OUT of the original image.

reloc_fill.py runs the linker's direction: take an address from the tree's maps,
write it into the relocation slot, compare against the original.  That direction
is blocked on the map -- 74% of relocations name a symbol no map gives an address
for, and for human-named globals no map even could: config/globals.csv holds 2569
symbols and every one of them is a `g_<HEX>` name derived from an address.  There
is no row anywhere that says where `BrUiItem` lives.

But the original binary already contains the answer.  Where our .obj has a
relocation, the original has the address the linker wrote there.  The relocation
record says which symbol that slot names, and our own bytes carry the addend, so
the equation inverts:

    DIR32 : target = orig_dword - addend
    REL32 : target = orig_dword - addend + (va + off + 4)

The result is an address for a symbol we had no address for, learned rather than
guessed -- provided the slot in the original really is the slot we think it is.
Two guards make that safe:

  * Only learn from a function whose bytes match the original with every
    relocation slot masked out.  If the encoding agrees everywhere except the
    addresses, our reloc offsets are the original's reloc offsets.
  * Cross-check.  A symbol referenced from several matched functions must yield
    the same address every time, and a symbol we already have an address for
    must reproduce it.  Agreement across independent references is what turns
    the inversion into evidence; a conflict marks the symbol untrustworthy and
    it is reported, never written.

Usage:
    python3 tools/reloc_learn.py [file.obj ...]      # default: all staged objs
    python3 tools/reloc_learn.py --csv out.csv       # write learned addresses
"""
import csv
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import normalize, classify, REL_DIR32, REL_REL32  # noqa: E402
from reloc_fill import parse, load_maps, resolve  # noqa: E402

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')


def learn_from(path, fnmap, glmap):
    """Yield (symbol, learned_va, from_func, from_va) for one .obj."""
    d, secs, syms, relocs = parse(path)
    byidx = {s['idx']: s for s in syms}

    for sy in syms:
        sec = secs.get(sy['sec'])
        if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
            continue
        name = sy['name'].lstrip('_')
        if name not in fnmap:
            continue
        va = fnmap[name]
        ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
        if not os.path.exists(ob):
            continue
        orig = open(ob, 'rb').read()
        start = sec['praw'] + sy['val']
        code = d[start:start + len(orig)]
        if len(code) != len(orig):
            continue

        mine = [(rva - sy['val'], si, rt) for rva, si, rt in relocs[sy['sec']]
                if 0 <= rva - sy['val'] <= len(orig) - 4]
        if not mine:
            continue

        # GUARD 1: with every relocation slot masked, the bytes must be equal.
        # Then our slots are the original's slots and the inversion is sound.
        mask = bytearray(b'\xff') * len(orig)
        for off, _, _ in mine:
            mask[off:off + 4] = b'\x00\x00\x00\x00'
        if any((code[i] ^ orig[i]) & mask[i] for i in range(len(orig))):
            continue

        for off, si, rt in mine:
            tsym = byidx.get(si)
            if not tsym:
                continue
            addend = struct.unpack_from('<i', code, off)[0]
            slot = struct.unpack_from('<I', orig, off)[0]
            if rt == REL_DIR32:
                target = slot - addend
            elif rt == REL_REL32:
                target = struct.unpack_from('<i', orig, off)[0] - addend \
                    + va + off + 4
            else:
                continue
            yield tsym['name'], target & 0xFFFFFFFF, name, va


LEARNED_CSV = os.path.join(ROOT, 'config', 'globals_learned.csv')

# The variants match_sweep.py compiles. Passed in explicitly when the sweep
# calls learn_and_write(), so the sweep stays the authority on its own list;
# this default is only for running the tool by hand.
DEFAULT_TAGS = ('O2', 'Od')


def live_objs(tags=DEFAULT_TAGS):
    """Every variant .obj whose source file still exists.

    A deleted .c leaves its .obj on disk forever: br_smallfn.obj outlived
    br_smallfn.c by five modules and is still sitting in both variant dirs.
    The masked-match guard means a stale obj cannot teach a WRONG address --
    it still has to reproduce the original's bytes -- but it can teach one for
    a symbol the tree no longer contains, and that is how report.csv came to
    double-count. Same rule applies here: no live source, not read.
    """
    live = set()
    for _, _, files in os.walk(os.path.join(ROOT, 'src')):
        for fn in files:
            if fn.endswith('.c'):
                live.add(os.path.splitext(fn)[0])
    out, dropped = [], []
    for tag in tags:
        d = os.path.join(ROOT, 'build', 'match', 'obj_' + tag)
        for p in sorted(glob.glob(os.path.join(d, '*.obj'))):
            if os.path.splitext(os.path.basename(p))[0] in live:
                out.append(p)
            else:
                dropped.append(os.path.basename(p))
    return out, sorted(set(dropped))


def learn(objs, fnmap=None, glmap=None):
    """Read addresses out of the image. Returns a result dict; writes nothing."""
    if fnmap is None or glmap is None:
        fnmap, glmap = load_maps()

    obs = {}      # symbol -> {address: [(func, va), ...]}
    for path in objs:
        try:
            found = list(learn_from(path, fnmap, glmap))
        except Exception as e:
            print(f"  ! {os.path.basename(path)}: {e}", file=sys.stderr)
            continue
        for sym, target, fn, fnva in found:
            obs.setdefault(sym, {}).setdefault(target, []).append((fn, fnva))

    # A learned address is only as good as its agreement.
    agreed, conflict = {}, {}
    for sym, cand in obs.items():
        if len(cand) == 1:
            addr = next(iter(cand))
            agreed[sym] = (addr, len(cand[addr]))
        else:
            conflict[sym] = cand

    # GUARD 2: symbols we already have an address for must reproduce it.
    checked = ok = 0
    wrong = []
    for sym, (addr, _) in agreed.items():
        known = resolve(sym, fnmap, glmap, learned=False)
        if known is None:
            continue
        checked += 1
        if known == addr:
            ok += 1
        else:
            wrong.append((sym, known, addr))

    new = {s: v for s, v in agreed.items()
           if resolve(s, fnmap, glmap, learned=False) is None}
    return {'obs': obs, 'agreed': agreed, 'conflict': conflict, 'new': new,
            'checked': checked, 'ok': ok, 'wrong': wrong, 'nobjs': len(objs)}


def write_map(path, res):
    """Write the learned addresses, and ONLY those that survived both guards."""
    new, obs = res['new'], res['obs']
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['symbol', 'addr', 'refs', 'corroborated', 'class',
                    'sources'])
        # Store the undecorated name: every consumer looks up after
        # lstrip('_'), so keeping the COFF underscore here would make the
        # learned map the one map that misses.
        #
        # `sources` lists the functions the address was read out of. It is what
        # lets reloc_fill.py refuse to score a function bit-exact on the
        # strength of an address learned from that same function -- without it
        # the check would be circular for every symbol with one reference.
        # Sort by (address, symbol), NOT address alone. Several symbols can
        # share one address -- the linker folds identical stubs, so 0x10008B80
        # carries four names -- and ties then fell back on dict insertion
        # order, which varies with the order objs happen to be read. The sweep
        # rewrites this file on every run, so that churned 273 lines each time
        # and made a real change indistinguishable from reshuffling.
        for s, (a, n) in sorted(new.items(), key=lambda kv: (kv[1][0], kv[0])):
            src = ' '.join(sorted({'0x%08X' % v for _, v in obs[s][a]}))
            w.writerow([s.lstrip('_'), '0x%08X' % a, n, int(n > 1),
                        classify(s), src])
    return len(new)


def learn_and_write(tags=DEFAULT_TAGS, path=LEARNED_CSV):
    """One call for the sweep: relearn from live objs, rewrite the map.

    A refusal to write on a contradiction is deliberate. If the image ever
    disagrees with a surveyed address, something upstream is wrong and a
    silently-rewritten map would bury it.
    """
    objs, dropped = live_objs(tags)
    res = learn(objs)
    if res['wrong']:
        return res, dropped, None
    return res, dropped, write_map(path, res)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = None
    if '--csv' in sys.argv:
        out = sys.argv[sys.argv.index('--csv') + 1]
        args = [a for a in args if a != out]
    dropped = []
    if args:
        objs = args
    else:
        objs, dropped = live_objs()

    res = learn(objs)
    obs, conflict, new = res['obs'], res['conflict'], res['new']

    print(f"objs read: {res['nobjs']}"
          + (f"   (skipped {len(dropped)} with no live source: "
             f"{', '.join(dropped)})" if dropped else ''))
    print(f"symbols observed in a masked-match function: {len(obs)}")
    print(f"  single consistent address : {len(res['agreed'])}")
    print(f"  conflicting addresses     : {len(conflict)}")
    print("\nVALIDATION -- learned vs. the address the tree already knows")
    print(f"  checkable: {res['checked']}   agree: {res['ok']}   "
          f"disagree: {len(res['wrong'])}")
    for s, k, l in res['wrong'][:10]:
        print(f"    ! {s}: map says {hex(k)}, image says {hex(l)}")

    print(f"\nNEW addresses (no map had one): {len(new)}")
    print(f"  of those, seen from >1 function: "
          f"{sum(1 for v in new.values() if v[1] > 1)}")
    byc = {}
    for s in new:
        byc.setdefault(classify(s), []).append(s)
    for c, ss in sorted(byc.items(), key=lambda kv: -len(kv[1])):
        print(f"    {len(ss):5d}  {c}")

    if conflict:
        print(f"\nCONFLICTS (reported, never written): {len(conflict)}")
        for s, cand in list(sorted(conflict.items(),
                                   key=lambda kv: -len(kv[1])))[:10]:
            print(f"    {s}: {' '.join(hex(a) for a in sorted(cand))}")

    if out:
        if res['wrong']:
            sys.exit("REFUSING to write: the image contradicts a surveyed "
                     "address. Fix that before trusting any learned map.")
        print(f"\nwrote {write_map(out, res)} learned addresses -> {out}")


if __name__ == '__main__':
    main()
