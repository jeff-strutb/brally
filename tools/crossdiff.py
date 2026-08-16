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

# Bytes of normalised prologue used by the fallback match. See norm_funcs.
PREFIX_LEN = 64


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
        # A PREFIX hash as well as the whole-body one.
        #
        # The body hash covers the function's exact LENGTH, so if the two maps
        # disagree by a few bytes of trailing padding -- which they routinely
        # do, the extents coming from two independent analyses -- a genuinely
        # shared function hashes differently on the two sides and is reported
        # d3d_only. Eight separate passes hit that and each concluded, alone,
        # that shared.csv was wrong "in this one case".
        #
        # The prefix is long enough to be specific (a 64-byte normalised
        # prologue is not something two unrelated functions share) and short
        # enough to be immune to a disagreement about where the body ends.
        pre = bytes(b[:PREFIX_LEN])
        out[va] = (hashlib.sha1(bytes(b)).hexdigest(), size,
                   hashlib.sha1(pre).hexdigest() if len(pre) >= PREFIX_LEN else None)
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
    bypre_b = collections.defaultdict(list)
    for va, (h, s, pre) in B.items():
        byhash_b[h].append(va)
        if pre:
            bypre_b[pre].append(va)

    shared, only_a, by_prefix = {}, [], 0
    for va, (h, s, pre) in A.items():
        if h in byhash_b:
            shared[va] = (byhash_b[h][0], s, 'body')
        elif pre and len(bypre_b.get(pre, ())) == 1:
            # Same prologue, exactly one candidate on the other side: the two
            # maps disagree about the extent, not about the function. Requiring
            # uniqueness is what keeps this from pairing two stubs that happen
            # to share a prologue.
            shared[va] = (bypre_b[pre][0], s, 'prefix')
            by_prefix += 1
        else:
            only_a.append((va, s))

    os.makedirs('config', exist_ok=True)
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['d3d_va', 'glide_va', 'size', 'class', 'matched_by'])
        for va in sorted(A):
            if va in shared:
                gva, s, how = shared[va]
                w.writerow(['0x%08X' % va, '0x%08X' % gva, s, 'shared', how])
            else:
                w.writerow(['0x%08X' % va, '', A[va][1], 'd3d_only', ''])

    sa = sum(s for _, (h, s) in A.items())
    ss = sum(s for va, (gva, s, how) in shared.items())
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
