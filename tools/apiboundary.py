"""Discover the renderer API by aligning the two builds' call graphs.

BRD3D.dll and BRGlide.dll are the same game against two backends. Functions
present in both (identical mnemonic sequence) are the shared game core, and can
be *paired* between the builds.

For each such pair we walk both instruction streams in lockstep -- they have the
same mnemonics by construction -- and read off the call targets at each matching
call site. That gives a correspondence:

    shared caller, site k  ->  (callee_d3d, callee_glide)

If both callees are non-shared, the call site is a genuine backend divergence:
one logical operation with two implementations. That set is the renderer API.

This is far more precise than "BRD3D-only", which misclassifies the statically
linked CRT (BRGlide imports the CRT from MSVCRT.dll instead). Here, a CRT call
shows up as `callee_glide is an MSVCRT import thunk` and is labelled as such.

Writes config/renderer_api.csv.
"""
import sys, os, csv, struct, hashlib, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True


def load(dll, fcsv):
    p = pelib.load(dll)
    text, text_va = p.text()
    funcs = [(int(r['va'], 16), int(r['size'])) for r in csv.DictReader(open(fcsv))]
    funcs.sort()
    return p, text, text_va, funcs


def decode(text, text_va, va, size):
    b = text[va - text_va: va - text_va + size]
    return list(md.disasm(b, va))


def fingerprint(ins):
    return hashlib.sha1(','.join(i.mnemonic for i in ins).encode()).hexdigest()


def import_thunks(p, text, text_va, funcs):
    """va -> 'DLL!Name' for 6-byte `jmp dword ptr [IAT]` stubs."""
    out = {}
    for va, size in funcs:
        if size > 8:
            continue
        off = va - text_va
        if text[off:off + 2] == b'\xff\x25':
            slot = struct.unpack('<I', text[off + 2:off + 6])[0]
            if slot in p.imports:
                out[va] = p.imports[slot]
    return out


def main():
    pa, ta, tva, fa = load('orig/BRD3D.dll', 'config/functions.csv')
    pb, tb, tvb, fb = load('orig/BRGlide.dll', 'config/functions_glide.csv')

    # decode everything, including sub-8-byte stubs -- they matter for the
    # equivalence test below even though they are too short to fingerprint
    da = {va: decode(ta, tva, va, s) for va, s in fa if s >= 1}
    db = {va: decode(tb, tvb, va, s) for va, s in fb if s >= 1}
    ha = {va: fingerprint(i) for va, i in da.items() if len(i) >= 4}
    hb = collections.defaultdict(list)
    for va, i in db.items():
        if len(i) >= 4:
            hb[fingerprint(i)].append(va)

    # pair shared functions; skip ambiguous fingerprints (>1 candidate)
    pairs = {va: hb[h][0] for va, h in ha.items() if len(hb.get(h, ())) == 1}
    shared_a = set(pairs)
    shared_b = set(pairs.values())
    thunks_b = import_thunks(pb, tb, tvb, fb)
    sizes_a = dict(fa)

    # walk each pair in lockstep
    api = collections.defaultdict(set)      # callee_d3d -> {shared callers}
    glide_impl = {}                         # callee_d3d -> callee_glide
    crt = collections.defaultdict(set)      # callee_d3d -> {MSVCRT names}
    for va_a, va_b in pairs.items():
        ia, ib = da[va_a], db[va_b]
        if len(ia) != len(ib):
            continue
        for x, y in zip(ia, ib):
            if x.mnemonic != 'call' or y.mnemonic != 'call':
                continue
            ox = x.operands[0] if x.operands else None
            oy = y.operands[0] if y.operands else None
            if ox is None or oy is None or ox.type != CS_OP_IMM or oy.type != CS_OP_IMM:
                continue
            ca, cb = ox.imm, oy.imm
            if ca in shared_a or cb in shared_b:
                continue                    # both sides shared: not a divergence
            if cb in thunks_b:
                crt[ca].add(thunks_b[cb])   # Glide calls the CRT here
                continue
            # Same code on both sides is the same function, not a divergence.
            # This catches compiler intrinsics and tiny helpers (_ftol, fsqrt,
            # bare `ret` stubs) that are statically linked into both builds but
            # are too short to have been paired above.
            ma = da.get(ca)
            mb = db.get(cb)
            if ma and mb and [i.mnemonic for i in ma] == [i.mnemonic for i in mb]:
                continue
            api[ca].add(va_a)
            glide_impl[ca] = cb

    nm = {}
    if os.path.exists('config/names.csv'):
        for r in csv.DictReader(open('config/names.csv')):
            nm[int(r['va'], 16)] = r['name']

    rows = sorted(api.items(), key=lambda kv: -len(kv[1]))
    with open('config/renderer_api.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['d3d_va', 'glide_va', 'size', 'shared_callers', 'name'])
        for va, cs in rows:
            w.writerow(['0x%08X' % va, '0x%08X' % glide_impl[va],
                        sizes_a.get(va, 0), len(cs), nm.get(va, '')])

    print("shared function pairs      %d" % len(pairs))
    print("CRT call sites identified  %d distinct callees (Glide routes to MSVCRT)"
          % len(crt))
    print()
    print("RENDERER API: %d functions with two implementations" % len(rows))
    print("total D3D-side code: %d bytes\n" % sum(sizes_a.get(v, 0) for v, _ in rows))
    print("  %-10s %-10s %6s %8s  %s" % ("d3d", "glide", "size", "callers", "name"))
    for va, cs in rows[:25]:
        print("  %08X   %08X %6d %8d  %s"
              % (va, glide_impl[va], sizes_a.get(va, 0), len(cs), nm.get(va, '')))
    if crt:
        print("\nsample CRT calls correctly excluded:")
        for va, s in list(crt.items())[:6]:
            print("   %08X -> %s" % (va, ', '.join(sorted(s))[:60]))


if __name__ == '__main__':
    main()
