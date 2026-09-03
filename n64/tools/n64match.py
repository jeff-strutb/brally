"""Compile PC decomp source with IDO and search the N64 ROM for each function.

The PC grind's deliverable is SOURCE.  Top Gear Rally (N64, 1997) is the same
source lineage built by IDO for MIPS, so a function we already own on the PC
side can be compiled a second time and looked for in the ROM directly -- which
pairs it and matches it in one step, with no anchor and no hand work.

  .venv/bin/python n64/tools/n64match.py src/core/geometry/br_vec.c
  .venv/bin/python n64/tools/n64match.py --all --csv build/n64/report.csv

Scoring, weakest to strongest:
  SHAPE   the candidate's normalised-opcode multiset is within tolerance
  EXACT   every instruction equal once relocated fields are masked
The mask covers what a linker fills in and a matcher cannot know: jal/j
targets, the lui/addiu-lo halves of an address, and %gp offsets.
"""
import sys, os, csv, struct, subprocess, collections, bisect, argparse, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
os.environ.setdefault('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))

CC = os.path.join(ROOT, 'tools/ido/cc')
CC53 = os.path.join(ROOT, 'tools/ido53/cc')
INC = ['-I' + os.path.join(ROOT, 'include'),
       '-I' + os.path.join(ROOT, 'n64/include')]
CFLAGS = ['-c', '-O2', '-mips2', '-non_shared', '-G', '0', '-w']
# x86 calling-convention keywords are meaningless on MIPS and appear in headers
# that never include <windows.h>, so they have to die on the command line.
CFLAGS += ['-D__fastcall=', '-D__stdcall=', '-D__cdecl=', '-D_fastcall=',
           '-D_stdcall=', '-D_cdecl=', '-D__declspec(x)=', '-DWINAPI=',
           '-DAPIENTRY=', '-DCALLBACK=', '-DPASCAL=', '-DFAR=', '-DNEAR=',
           '-DN64_MATCH=1']

# The macOS/Metal port is new platform code, not a byte-matched target, and it
# pulls in Apple frameworks that will never exist here.  Skip it by header.
SKIP_INCLUDES = ('AudioToolbox/', 'CoreFoundation/', 'Metal/', 'simd/')


# ------------------------------------------------------------------ ELF32 BE
def elf_functions(path):
    """-> {symbol name: (bytes, [(offset, reloc_type)])} for .text."""
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('>I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('>HHH', d, 0x2e)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsz = \
            struct.unpack_from('>10I', d, o)
        secs.append(dict(name=name, typ=typ, off=off, size=size,
                         link=link, info=info, entsz=entsz))
    strtab = secs[shstrndx]

    def sname(s):
        b = strtab['off'] + s['name']
        return d[b:d.index(b'\0', b)].decode('latin1')

    byname = {sname(s): s for s in secs}
    text = byname.get('.text')
    if text is None:
        return {}
    tblob = d[text['off']:text['off'] + text['size']]
    ti = secs.index(text)

    # symbols
    sym = byname.get('.symtab')
    syms = []
    if sym:
        sstr = secs[sym['link']]
        n = sym['size'] // 16
        for i in range(n):
            o = sym['off'] + i * 16
            nm, val, size, info, other, shndx = struct.unpack_from('>IIIBBH', d, o)
            b = sstr['off'] + nm
            name = d[b:d.index(b'\0', b)].decode('latin1')
            if shndx == ti and (info & 0xf) == 2:      # STT_FUNC in .text
                syms.append((val, size, name))
    syms.sort()

    # relocations against .text
    relocs = []
    for s in secs:
        if s['typ'] == 9 and s['info'] == ti:          # SHT_REL
            for i in range(s['size'] // 8):
                o = s['off'] + i * 8
                off, info = struct.unpack_from('>II', d, o)
                relocs.append((off, info & 0xff))
    relocs.sort()

    out = {}
    for i, (val, size, name) in enumerate(syms):
        end = val + size if size else (syms[i + 1][0] if i + 1 < len(syms)
                                       else len(tblob))
        body = tblob[val:end]
        rs = [(o - val, t) for o, t in relocs if val <= o < end]
        out[name] = (body, rs)
    return out


# ------------------------------------------------------- MIPS normalisation
def words(b):
    return [struct.unpack_from('>I', b, i)[0] for i in range(0, len(b) - 3, 4)]


def norm_op(w):
    """Opcode identity with registers and immediates dropped."""
    op = w >> 26
    if op == 0:
        return ('S', w & 0x3f)
    if op == 1:
        return ('R', (w >> 16) & 0x1f)
    if op == 0x11:                                     # COP1
        fmt = (w >> 21) & 0x1f
        if fmt in (0, 4, 2, 6, 8):                     # mfc1/mtc1/cfc1/ctc1/bc1
            return ('C', fmt)
        return ('F', fmt, w & 0x3f)
    return ('O', op)


def sig(b):
    return collections.Counter(norm_op(w) for w in words(b))


# masks applied before an EXACT comparison: a linker owns these fields
def mask_words(ws, relocs=None):
    out = []
    reloc_at = {o // 4 for o, t in (relocs or [])}
    for i, w in enumerate(ws):
        op = w >> 26
        if op in (2, 3):                               # j / jal target
            w &= 0xFC000000
        elif i in reloc_at:                            # lui/addiu/lw hi-lo half
            w &= 0xFFFF0000
        out.append(w)
    return out


# ------------------------------------------------------------------ N64 side
import n64rom  # noqa: E402


def n64_funcs():
    """-> [(vram, bytes)] for every function in .text."""
    out = []
    F = n64rom.F
    end = n64rom.r2v(n64rom.TEXT_E)
    for i, v in enumerate(F):
        e = F[i + 1] if i + 1 < len(F) else end
        out.append((v, n64rom.d[n64rom.v2r(v):n64rom.v2r(e)]))
    return out


def trim(b):
    """Drop the trailing nop padding IDO/the linker inserts."""
    ws = words(b)
    while ws and ws[-1] == 0:
        ws.pop()
    return b''.join(struct.pack('>I', w) for w in ws)


class Rom:
    def __init__(self):
        self.fns = [(v, trim(b)) for v, b in n64_funcs()]
        self.sigs = [(v, b, sig(b), len(b) // 4) for v, b in self.fns]

    def search(self, body, topn=3):
        s, n = sig(body), len(words(body))
        hits = []
        for v, b, cs, cn in self.sigs:
            if not (0.55 * n <= cn <= 1.8 * n + 4):
                continue
            d = sum((s - cs).values()) + sum((cs - s).values())
            hits.append((d, v, b))
        hits.sort(key=lambda x: (x[0], abs(len(words(x[2])) - n)))
        return hits[:topn]


def exact(body, relocs, cand):
    a = mask_words(words(body), relocs)
    b = mask_words(words(cand))
    return len(a) == len(b) and a == b


# --------------------------------------------------------------------- main
def compile_file(cfile, cc=CC, extra=()):
    o = tempfile.NamedTemporaryFile(suffix='.o', delete=False).name
    p = subprocess.run([cc] + CFLAGS + list(extra) + INC + ['-o', o, cfile],
                       capture_output=True, text=True)
    if p.returncode:
        os.unlink(o)
        return None, (p.stderr or p.stdout).strip().splitlines()[:3]
    fns = elf_functions(o)
    os.unlink(o)
    return fns, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*')
    ap.add_argument('--all', action='store_true', help='every .c under src/core')
    ap.add_argument('--csv')
    ap.add_argument('--ido53', action='store_true')
    ap.add_argument('--tol', type=int, default=6,
                    help='max opcode-multiset distance to report as SHAPE')
    args = ap.parse_args()

    files = list(args.files)
    if args.all:
        for dp, dn, fn in os.walk(os.path.join(ROOT, 'src')):
            if 'generated' in dp or '/cpp' in dp:
                continue
            files += [os.path.join(dp, f) for f in fn if f.endswith('.c')]
    files = sorted(set(files))
    files = [f for f in files
             if not any(h in open(f, errors='ignore').read(4000)
                        for h in SKIP_INCLUDES)]

    rom = Rom()
    rows, nfail, ncomp, nfn = [], 0, 0, 0
    cand = []           # (row index, [(dist, va, is_exact)]) for 1:1 assignment
    for cf in files:
        fns, err = compile_file(cf, CC53 if args.ido53 else CC)
        if fns is None:
            nfail += 1
            rows.append(dict(file=os.path.relpath(cf, ROOT), fn='', status='CCFAIL',
                             insns=0, n64_va='', dist='', note=err[0][:90] if err else ''))
            continue
        ncomp += 1
        for name, (body, relocs) in sorted(fns.items()):
            body = trim(body)
            n = len(words(body))
            if n < 4:
                continue
            nfn += 1
            hits = [(d, v, exact(body, relocs, b))
                    for d, v, b in rom.search(body, topn=6)]
            rows.append(dict(file=os.path.relpath(cf, ROOT), fn=name, status='MISS',
                             insns=n, n64_va='', dist='', note=''))
            cand.append((len(rows) - 1, hits))

    # ---- 1:1 assignment.  Two source functions that differ only in whether
    # they write through an out-pointer (Add/AddTo, Div/DivBy) compile to
    # near-identical code and would otherwise both claim the same ROM address;
    # only one of them can be the function that is actually in the ROM.
    order = []
    for ri, hits in cand:
        for rank, (d, v, ex) in enumerate(hits):
            order.append((0 if ex else 1, d, rank, ri, v, ex))
    order.sort()
    taken, done = set(), set()
    for _, d, _, ri, v, ex in order:
        if ri in done or v in taken:
            continue
        if not ex and d > args.tol:
            continue
        done.add(ri)
        taken.add(v)
        rows[ri].update(status='EXACT' if ex else 'SHAPE',
                        n64_va='%08X' % v, dist=d)
    for ri, hits in cand:
        if ri not in done and hits:
            rows[ri]['dist'] = hits[0][0]

    if args.csv:
        os.makedirs(os.path.dirname(args.csv), exist_ok=True)
        with open(args.csv, 'w', newline='') as f:
            w = csv.DictWriter(f, ['file', 'fn', 'status', 'insns', 'n64_va',
                                   'dist', 'note'])
            w.writeheader()
            w.writerows(rows)

    c = collections.Counter(r['status'] for r in rows)
    print("files: %d compiled, %d failed" % (ncomp, nfail))
    print("functions compiled: %d" % nfn)
    for k in ('EXACT', 'SHAPE', 'MISS'):
        print("  %-6s %d" % (k, c.get(k, 0)))
    if args.csv:
        print("-> %s" % args.csv)


if __name__ == '__main__':
    main()
