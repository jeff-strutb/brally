"""Find matched functions in the IDO 5.3 corpus whose code has the same shape
as a stretch of the TGR ROM.

    .venv/bin/python tools/tgrally/n64corpus/query.py VA [--at OFF --len N] [--top 5] [--k 6]

The shape is the instruction stream with registers reduced to their class
(t, s, a, v, f, sp, zero, ra) and large immediates blanked.  Functions are
ranked by the longest run of matching shape, then by shared k-grams.
"""
import argparse
import json
import os
import pickle
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'tgrally'))
import n64build as B  # noqa: E402
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_BIG_ENDIAN  # noqa: E402

MD = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)


def regclass(m):
    r = m.group(1)
    if r in ('sp', 'zero', 'ra', 'at', 'fp', 'gp'):
        return r
    if r.startswith('f'):
        return 'f'
    return r[0]


def _dis(w):
    for i in MD.disasm(struct.pack('>I', w), 0):
        return i
    return None


def shape(ws, masks=None):
    """Instruction tokens; relocated immediates become R (address halves A).
    With no masks (ROM words) relocations are inferred from %hi/%lo pairs."""
    out, hi = [], set()
    for k, w in enumerate(ws):
        m = masks[k] if masks else 0xffffffff
        if m == 0:
            out.append('reloc')
            continue
        i = _dis(w)
        if i is None:
            out.append('.word')
            continue
        ops = re.sub(r'\$(\w+)', regclass, i.op_str)
        regs = re.findall(r'\$(\w+)', i.op_str)
        mn = i.mnemonic
        if mn in ('jal', 'j', 'b', 'bal') or mn.startswith(('b', 'j')) and mn != 'jr':
            ops = re.sub(r'0x[0-9a-f]+', 'L', ops)
        elif mn == 'lui':
            v = w & 0xffff
            isaddr = (m != 0xffffffff) if masks else 0x8000 <= v < 0x8100
            ops = ops.split(',')[0] + ', ' + ('A' if isaddr else 'K%x' % v)
            if isaddr:
                hi.add(regs[0])
            else:
                hi.discard(regs[0])
            out.append(mn + ' ' + ops)
            continue
        else:
            rel = (m != 0xffffffff) if masks else bool(set(regs[1:]) & hi)
            if rel:
                ops = re.sub(r'-?0x[0-9a-f]+|(?<![\w$])-?\d+(?=\(|$)', 'R', ops)
            else:
                ops = re.sub(r'-?0x[0-9a-f]{3,}', 'K', ops)
            if regs and not mn.startswith(('s', 'b', 'j')) and regs[0] in hi and not rel:
                hi.discard(regs[0])
            if rel and regs and regs[0] in hi and mn.startswith(('addiu', 'l')):
                hi.discard(regs[0])
        out.append(mn + ' ' + ops)
    return out


def load(k, opt='-O2'):
    cache = os.path.join(HERE, 'corpus.k%d%s.pkl' % (k, opt.replace(' ', '')))
    src = os.path.join(HERE, 'corpus.jsonl')
    if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(src):
        return pickle.load(open(cache, 'rb'))
    recs, toks, index = [], [], {}
    for line in open(src):
        r = json.loads(line)
        if opt and not r['flags'].startswith(opt):
            continue
        t = shape(r['words'], r['mask'])
        fid = len(recs)
        recs.append({x: r[x] for x in ('repo', 'file', 'name', 'flags', 'n', 'c')})
        toks.append(t)
        for j in range(len(t) - k + 1):
            index.setdefault(tuple(t[j:j + k]), set()).add(fid)
    data = (recs, toks, index)
    pickle.dump(data, open(cache, 'wb'))
    return data


def longest_common_run(a, b):
    best, bi, bj = 0, 0, 0
    prev = [0] * (len(b) + 1)
    for i in range(1, len(a) + 1):
        cur = [0] * (len(b) + 1)
        ai = a[i - 1]
        for j in range(1, len(b) + 1):
            if ai == b[j - 1]:
                cur[j] = prev[j - 1] + 1
                if cur[j] > best:
                    best, bi, bj = cur[j], i, j
        prev = cur
    return best, bi - best, bj - best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va')
    ap.add_argument('--at', type=lambda x: int(x, 0), default=0, help='byte offset into the function')
    ap.add_argument('--len', type=int, default=0, help='instructions (default: whole function)')
    ap.add_argument('--top', type=int, default=5)
    ap.add_argument('--k', type=int, default=6)
    ap.add_argument('--no-c', action='store_true')
    ap.add_argument('--opt', default='-O2', help="flag prefix filter ('' = all)")
    a = ap.parse_args()
    va = int(a.va, 16)
    fmap = B.function_map()
    rom = B.Rom()
    size = fmap[va]
    ws = [rom.word(va + o) for o in range(0, size, 4)]
    ws = ws[a.at // 4:]
    if a.len:
        ws = ws[:a.len]
    # mask HI16/LO16 pairs and jal targets the cheap way: blank big immediates
    q = shape(ws)
    recs, toks, index = load(a.k, a.opt)
    score = {}
    for j in range(len(q) - a.k + 1):
        for fid in index.get(tuple(q[j:j + a.k]), ()):
            score[fid] = score.get(fid, 0) + 1
    cands = sorted(score, key=lambda f: -score[f])[:200]
    ranked = []
    for fid in cands:
        run, qi, fi = longest_common_run(q, toks[fid])
        ranked.append((run, score[fid], fid, qi, fi))
    ranked.sort(reverse=True)
    print('%08X: %d instructions queried, %d candidates' % (va, len(q), len(score)))
    for run, sc, fid, qi, fi in ranked[:a.top]:
        r = recs[fid]
        print('\n== run %d (query +0x%X, theirs +0x%X), %d shared %d-grams: %s %s:%s [%s] %d insns' % (
            run, 4 * qi + a.at, 4 * fi, sc, a.k, r['repo'], r['file'], r['name'], r['flags'], r['n']))
        if not a.no_c and r['c']:
            print(r['c'] if len(r['c']) < 4000 else r['c'][:4000] + '\n...')


if __name__ == '__main__':
    main()
