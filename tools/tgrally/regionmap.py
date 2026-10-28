"""Map every USA address the decomp names to its home in another region's ROM.

    .venv/bin/python tools/tgrally/regionmap.py eur|jpn [-v]

Writes config/tgrally/<region>/addrmap.csv: usa,va,kind,how,votes.

Functions.  Both ROMs link the same objects in the same order, so their
function lists are aligned as sequences.  A function's signature is its body
with every address field blanked (jal/j targets, lui immediates, the low half
of each %hi/%lo pair); equal signatures pair as `same`.  Between two `same`
anchors, gaps of equal length pair one to one as `changed`; uneven gaps pair
greedily, in order, by body similarity (>= 0.5) as `changed`, and what is left
is `gone` (USA only) or new to the region (not listed: nothing in the source
names it yet).

Data.  Walking each `same` pair in step, every jal target and every
%hi/%lo address the USA body forms is paired with the address the region's
body forms at the same instruction; a `changed` pair contributes through an
opcode alignment, at lower weight.  The address with the most votes wins;
a USA address whose votes disagree is listed with how=`conflict`.
"""
import collections
import csv
import difflib
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import region  # noqa: E402

LO_OPS = (0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x31, 0x35, 0x39, 0x3D)


class Image:
    def __init__(self, reg):
        self.reg = reg
        self.d = open(region.rom_path(reg), 'rb').read()
        region.check_rom(region.rom_path(reg), reg)
        out = subprocess.run([sys.executable, os.path.join(HERE, 'n64rom.py'), 'funcs'],
                             env=dict(os.environ, TGR_REGION=reg, TGR_ROM=''),
                             capture_output=True, text=True, check=True).stdout
        self.F = [int(x, 16) for x in out.split()]
        end = region.REGIONS[reg]['text_e'] - region.ROMOFF + region.BASE
        self.size = {f: (self.F[i + 1] if i + 1 < len(self.F) else end) - f
                     for i, f in enumerate(self.F)}

    def body(self, va):
        r = va - region.BASE + region.ROMOFF
        n = self.size[va]
        return list(struct.unpack('>%dI' % (n // 4), self.d[r:r + n]))


def lui_pairs(ws):
    """-> {index: ('lo'|'jal', address)} for every address the body forms."""
    out, hi = {}, {}
    for i, w in enumerate(ws):
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op == 3:
            out[i] = ('jal', 0x80000000 | ((w & 0x03FFFFFF) << 2))
            continue
        if op == 0x0F:
            hi[rt] = (w & 0xffff) << 16
            continue
        if op in LO_OPS and rs in hi:
            imm = w & 0xffff
            a = (hi[rs] | imm) if op == 0x0D else hi[rs] + (imm - 0x10000 if imm & 0x8000 else imm)
            out[i] = ('lo', a & 0xffffffff)
        if op in (0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25) and rt in hi and rt != rs:
            hi.pop(rt, None)
        if op in (0x09, 0x0D) and rt == rs:
            hi.pop(rt, None)
    return out


def signature(ws):
    pairs = lui_pairs(ws)
    out = []
    for i, w in enumerate(ws):
        op = w >> 26
        if op in (2, 3):
            w = op << 26
        elif op == 0x0F:
            w &= 0xFFFF0000
        elif i in pairs:
            w &= 0xFFFF0000
        out.append(w)
    return tuple(out)


def opkey(w):
    return (w >> 26, w & 0x3f if w >> 26 == 0 else 0)


def jal_votes(U, X, pairs):
    """USA callee -> Counter(region callee) over the jals of `same` pairs."""
    out = collections.defaultdict(collections.Counter)
    for fu, (fx, how) in pairs.items():
        if how != 'same':
            continue
        pu, px = lui_pairs(U.body(fu)), lui_pairs(X.body(fx))
        for i, (kind, au) in pu.items():
            if kind == 'jal' and i in px and px[i][0] == 'jal':
                out[au][px[i][1]] += 1
    return out


def pair_functions(U, X):
    su = [signature(U.body(f)) for f in U.F]
    sx = [signature(X.body(f)) for f in X.F]
    sm = difflib.SequenceMatcher(None, su, sx, autojunk=False)
    anchors = {}
    for a, b, n in sm.get_matching_blocks():
        for k in range(n):
            anchors[a + k] = (b + k, 'same')
    # calls outrank the sequence: a USA function every unchanged caller
    # reaches at one region function is anchored there, if that keeps the
    # order (an inserted region function shifts a run of small changed ones)
    iu = {f: i for i, f in enumerate(U.F)}
    ix = {f: j for j, f in enumerate(X.F)}
    calls = jal_votes(U, X, {U.F[i]: (X.F[j], h) for i, (j, h) in anchors.items()})
    moved = {}          # library objects the region links in another order
    for au, c in calls.items():
        if au not in iu or len(c) != 1:
            continue
        (bx, n), = c.most_common(1)
        if bx not in ix:
            continue
        i, j = iu[au], ix[bx]
        if i in anchors:
            continue
        lo = max((jj for ii, (jj, _) in anchors.items() if ii < i), default=-1)
        hi = min((jj for ii, (jj, _) in anchors.items() if ii > i), default=len(X.F))
        if lo < j < hi:
            anchors[i] = (j, 'same' if su[i] == sx[j] else 'changed')
        elif n >= 3:
            moved[i] = (j, 'same' if su[i] == sx[j] else 'changed')
    pairs = {U.F[i]: (X.F[j], h) for i, (j, h) in moved.items()}
    taken = {j for j, _ in moved.values()}
    order = sorted(anchors)
    prev_i, prev_j = -1, -1
    for i in order + [len(U.F)]:
        j = anchors[i][0] if i < len(U.F) else len(X.F)
        gu = [a for a in range(prev_i + 1, i) if a not in moved]
        gx = [b for b in range(prev_j + 1, j) if b not in taken]
        if gu and len(gu) == len(gx):
            for a, b in zip(gu, gx):
                pairs[U.F[a]] = (X.F[b], 'changed')
        elif gu and gx:
            j0 = 0
            for a in gu:
                best = None
                for b in gx[j0:]:
                    r = difflib.SequenceMatcher(None, [opkey(w) for w in U.body(U.F[a])],
                                                [opkey(w) for w in X.body(X.F[b])],
                                                autojunk=False).ratio()
                    if r >= 0.5 and (best is None or r > best[0]):
                        best = (r, b)
                if best:
                    pairs[U.F[a]] = (X.F[best[1]], 'changed')
                    j0 = gx.index(best[1]) + 1
        if i < len(U.F):
            pairs[U.F[i]] = (X.F[j], anchors[i][1])
        prev_i, prev_j = i, j
    # a function still unpaired whose exact signature occurs once, unclaimed,
    # in the region moved with its object
    claimed = {v[0] for v in pairs.values()}
    bysig = collections.defaultdict(list)
    for j, f in enumerate(X.F):
        if f not in claimed:
            bysig[sx[j]].append(f)
    for i, f in enumerate(U.F):
        if f not in pairs and len(bysig.get(su[i], ())) == 1:
            pairs[f] = (bysig[su[i]][0], 'same')
    return pairs


def main():
    reg = sys.argv[1] if len(sys.argv) > 1 else ''
    if reg not in region.REGIONS or reg == 'usa':
        sys.exit('usage: regionmap.py eur|jpn')
    U, X = Image('usa'), Image(reg)
    fpairs = pair_functions(U, X)
    votes = collections.defaultdict(collections.Counter)
    for fu, (fx, how) in fpairs.items():
        wu, wx = U.body(fu), X.body(fx)
        pu, px = lui_pairs(wu), lui_pairs(wx)
        if how == 'same':
            idx = {i: i for i in pu}
            weight = 10
        else:
            sm = difflib.SequenceMatcher(None, [opkey(w) for w in wu], [opkey(w) for w in wx],
                                         autojunk=False)
            idx = {}
            for a, b, n in sm.get_matching_blocks():
                for k in range(n):
                    idx[a + k] = b + k
            weight = 1
        for i, (kind, au) in pu.items():
            j = idx.get(i)
            if j is None or j not in px or px[j][0] != kind:
                continue
            votes[au][px[j][1]] += weight
    rows = {}
    for fu, (fx, how) in fpairs.items():
        rows[fu] = dict(usa='%08X' % fu, va='%08X' % fx, kind='func', how=how, votes='')
    for fu in U.F:
        if fu not in fpairs:
            rows[fu] = dict(usa='%08X' % fu, va='', kind='func', how='gone', votes='')
    fset = set(U.F)
    for au, c in votes.items():
        if au in fset:
            continue                         # functions pair by sequence, not by vote
        (best, n), = c.most_common(1)
        how = 'conflict' if len(c) > 1 else 'xref'
        rows[au] = dict(usa='%08X' % au, va='%08X' % best, kind='data', how=how,
                        votes=' '.join('%08X:%d' % kv for kv in c.most_common(3)))
    # a jal that disagrees with the sequence pairing is a pairing error
    bad = [(au, c) for au, c in votes.items() if au in fpairs and fpairs[au][0] not in c]
    out = os.path.join(region.config_dir(reg), 'addrmap.csv')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', newline='') as fh:
        w = csv.DictWriter(fh, ['usa', 'va', 'kind', 'how', 'votes'], lineterminator='\n')
        w.writeheader()
        w.writerows(rows[k] for k in sorted(rows))
    cnt = collections.Counter((r['kind'], r['how']) for r in rows.values())
    print('%s: USA functions %d, %s functions %d' % (reg, len(U.F), reg, len(X.F)))
    print('  ' + '  '.join('%s/%s %d' % (k[0], k[1], v) for k, v in sorted(cnt.items())))
    print('  %s functions not paired from USA: %d' % (reg, len(set(X.F) - {v[0] for v in fpairs.values()})))
    print('  jal votes disagreeing with the function pairing: %d' % len(bad))
    for au, c in bad[:20]:
        print('    %08X paired %08X, calls say %s' % (au, fpairs[au][0], dict(c)))
    print('wrote', os.path.relpath(out, region.ROOT))


if __name__ == '__main__':
    main()
