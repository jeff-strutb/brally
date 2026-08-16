"""Match functions between BRD3D.dll and BRGlide.dll.

Both DLLs are the same game built against different renderers, so most functions
are byte-identical apart from embedded addresses. We normalise each function by
zeroing every relocated dword and every rel32 CALL/JMP displacement, then hash.

Equal hash across the two builds => shared game code, which is the real
decompilation target. BRD3D-only code is DirectDraw plus the statically linked
CRT; BRGlide-only code is Glide.

Writes config/shared.csv.
"""
import sys, os, csv, struct, hashlib, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

md = Cs(CS_ARCH_X86, CS_MODE_32)


def norm_funcs(dll, fcsv):
    p = pelib.load(dll)
    text, text_va = p.text()
    relocs = {p.image_base + r for r in p.relocs}
    out = {}
    for r in csv.DictReader(open(fcsv)):
        va, size = int(r['va'], 16), int(r['size'])
        if size < 8:
            continue
        off = va - text_va
        b = bytearray(text[off:off + size])
        # zero relocated dwords (absolute addresses)
        for k in range(size - 3):
            if (va + k) in relocs:
                b[k:k + 4] = b'\0\0\0\0'
        # zero rel32 displacements of E8/E9
        i = 0
        while i < len(b) - 4:
            if b[i] in (0xE8, 0xE9):
                b[i + 1:i + 5] = b'\0\0\0\0'
                i += 5
            else:
                i += 1
        out[va] = (hashlib.sha1(bytes(b)).hexdigest(), size)
    return p, out


def main():
    # Both sides are overridable so the two maps can be varied independently.
    # That matters because a hash match requires BOTH maps to agree about the
    # function's extent: swapping one side for a rebuilt map and watching the
    # match count move measures the OTHER side's extent errors.
    # config/functions.csv is now the FLOW-derived map. The original
    # sweep-derived one is kept as config/functions_d3d_sweep.csv for
    # comparison; it has 181 defective entries in 2,632.
    map_a = os.environ.get('BR_MAP_D3D', 'config/functions.csv')
    map_b = os.environ.get('BR_MAP_GLIDE', 'config/functions_glide.csv')
    out = os.environ.get('BR_SHARED_OUT', 'config/shared.csv')
    pa, A = norm_funcs('orig/BRD3D.dll', map_a)
    pb, B = norm_funcs('orig/BRGlide.dll', map_b)
    byhash_b = collections.defaultdict(list)
    for va, (h, s) in B.items():
        byhash_b[h].append(va)

    shared, only_a = {}, []
    for va, (h, s) in A.items():
        if h in byhash_b:
            shared[va] = (byhash_b[h][0], s)
        else:
            only_a.append((va, s))

    os.makedirs('config', exist_ok=True)
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['d3d_va', 'glide_va', 'size', 'class'])
        for va in sorted(A):
            if va in shared:
                gva, s = shared[va]
                w.writerow(['0x%08X' % va, '0x%08X' % gva, s, 'shared'])
            else:
                w.writerow(['0x%08X' % va, '', A[va][1], 'd3d_only'])

    sa = sum(s for _, (h, s) in A.items())
    ss = sum(s for va, (gva, s) in shared.items())
    print("BRD3D functions (>=8 bytes)  %d   %d bytes" % (len(A), sa))
    print("BRGlide functions            %d" % len(B))
    print("matched (shared game code)   %d   %d bytes  (%.1f%% of BRD3D .text)"
          % (len(shared), ss, 100.0 * ss / sa))
    print("BRD3D-only (D3D + static CRT) %d   %d bytes" % (len(only_a), sa - ss))

    # where does the BRD3D-only code sit?
    buckets = collections.Counter()
    for va, s in only_a:
        buckets[va & ~0x7FFF] += s
    print("\nBRD3D-only bytes by 32KB block:")
    for k in sorted(buckets):
        bar = '#' * min(60, buckets[k] // 400)
        print("  %08X %7d %s" % (k, buckets[k], bar))


if __name__ == '__main__':
    main()
