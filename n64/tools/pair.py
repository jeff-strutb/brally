"""String-anchored function pairing: Top Gear Rally (N64) <-> BRGlide.dll (PC).

Boss Rally (PC, 1999) and Top Gear Rally (N64, 1997) are the same source
lineage; 195 string literals survive verbatim in both images.  A string
referenced by exactly one function on each side pairs those two functions with
no matching work at all.  That is the bridge this file builds.

  .venv/bin/python tools/tgr/pair.py [--csv build/n64_work/pairs.csv]

Output columns: n64_va, pc_va, pc_name, n_anchors, anchor
  n_anchors  how many distinct shared strings support the pair
  anchor     the longest supporting string (truncated)

Both sides use the same rule: find the NUL-terminated printable runs in the
data region, find every code site that materialises the string's address, and
attribute that site to its containing function.  On MIPS the address arrives as
a lui/addiu pair (n64rom.py already tracks these); on x86 it is a 4-byte
absolute immediate, so a raw little-endian scan of .text finds every one.
"""
import sys, os, csv, struct, collections, bisect, argparse

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
os.environ.setdefault('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))

MIN_LEN = 6          # shorter runs pair by accident, not by lineage


# ---------------------------------------------------------------- N64 side
import n64rom  # noqa: E402  (analyses the ROM at import time)

DATA_S, DATA_E = n64rom.TEXT_E, 0xAD400


def n64_strings():
    """vram -> bytes, for every NUL-terminated printable run in .data/.rodata."""
    out, d = {}, n64rom.d
    r = DATA_S
    while r < DATA_E:
        if 0x20 <= d[r] < 0x7f:
            e = r
            while e < DATA_E and 0x20 <= d[e] < 0x7f:
                e += 1
            if e < DATA_E and d[e] == 0 and e - r >= MIN_LEN:
                out[n64rom.r2v(r)] = d[r:e]
            r = e + 1
        else:
            r += 1
    return out


# ------------------------------------------------------------------ PC side
def pe_load(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, ro = struct.unpack_from('<IIII', d, o + 8)
        secs.append((name, base + va, vsz, ro, rsz))
    return d, base, secs


def pc_strings(d, secs):
    out = {}
    for name, va, vsz, ro, rsz in secs:
        if name.startswith('.text'):
            continue
        blob = d[ro:ro + rsz]
        i = 0
        while i < len(blob):
            if 0x20 <= blob[i] < 0x7f:
                e = i
                while e < len(blob) and 0x20 <= blob[e] < 0x7f:
                    e += 1
                if e < len(blob) and blob[e] == 0 and e - i >= MIN_LEN:
                    out[va + i] = blob[i:e]
                i = e + 1
            else:
                i += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--csv', default=os.path.join(ROOT, 'build/n64_work/pairs.csv'))
    ap.add_argument('--dll', default=os.path.join(ROOT, 'orig/BRGlide.dll'))
    ap.add_argument('--min-len', type=int, default=MIN_LEN)
    args = ap.parse_args()

    ns = n64_strings()
    d, base, secs = pe_load(args.dll)
    ps = pc_strings(d, secs)

    # shared literals, keyed by the bytes themselves
    n_by_text = collections.defaultdict(list)
    for va, t in ns.items():
        n_by_text[t].append(va)
    p_by_text = collections.defaultdict(list)
    for va, t in ps.items():
        p_by_text[t].append(va)
    shared = [t for t in n_by_text if t in p_by_text and len(t) >= args.min_len]

    # ---- PC function map + a VA -> containing function lookup
    fns = []
    with open(os.path.join(ROOT, 'config/functions_glide.csv')) as f:
        for row in csv.DictReader(f):
            fns.append((int(row['va'], 16), int(row['size']), row['name']))
    fns.sort()
    fstarts = [f[0] for f in fns]

    def pc_fn(va):
        i = bisect.bisect_right(fstarts, va) - 1
        if i < 0:
            return None
        s, sz, nm = fns[i]
        return (s, nm) if va < s + sz else None

    # ---- every x86 site that materialises a string address
    text = [s for s in secs if s[0].startswith('.text')][0]
    tname, tva, tvsz, tro, trsz = text
    tblob = d[tro:tro + trsz]
    pc_refs = collections.defaultdict(set)      # string va -> {func va}
    want = {va for t in shared for va in p_by_text[t]}
    for off in range(0, len(tblob) - 3):
        v = struct.unpack_from('<I', tblob, off)[0]
        if v in want:
            fn = pc_fn(tva + off)
            if fn:
                pc_refs[v].add(fn)

    # ---- every MIPS site that materialises a string address
    n64_refs = collections.defaultdict(set)
    for t in shared:
        for va in n_by_text[t]:
            for pc in n64rom.xref.get(va, ()):
                f = n64rom.fstart(pc)
                if f is not None:
                    n64_refs[va].add(f)

    # ---- vote: a (n64 fn, pc fn) pair scores one per shared string that lands
    #      in exactly one function on each side
    votes = collections.defaultdict(list)
    anchors_used = 0
    for t in shared:
        nfs = set()
        for va in n_by_text[t]:
            nfs |= n64_refs.get(va, set())
        pfs = set()
        for va in p_by_text[t]:
            pfs |= pc_refs.get(va, set())
        if len(nfs) == 1 and len(pfs) == 1:
            anchors_used += 1
            votes[(nfs.pop(), pfs.pop())].append(t)

    # ---- keep only mutually-best pairs (a function pairs with one partner)
    best_n, best_p = {}, {}
    for (nv, (pv, nm)), ts in votes.items():
        k = (len(ts), max(len(x) for x in ts))
        if k > best_n.get(nv, ((-1, -1), None))[0]:
            best_n[nv] = (k, (pv, nm, ts))
        if k > best_p.get(pv, ((-1, -1), None))[0]:
            best_p[pv] = (k, (nv, nm, ts))
    pairs = []
    for nv, (k, (pv, nm, ts)) in best_n.items():
        if best_p.get(pv, (None, (None,)))[1][0] == nv:
            pairs.append((nv, pv, nm, len(ts), max(ts, key=len)))
    pairs.sort()

    os.makedirs(os.path.dirname(args.csv), exist_ok=True)
    with open(args.csv, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['n64_va', 'pc_va', 'pc_name', 'n_anchors', 'anchor'])
        for nv, pv, nm, n, a in pairs:
            w.writerow(['%08X' % nv, '%08X' % pv, nm, n,
                        a.decode('latin1')[:60]])

    print("shared string literals (>=%d chars): %d" % (args.min_len, len(shared)))
    print("  usable as anchors (1 fn each side): %d" % anchors_used)
    print("N64 functions: %d    PC functions: %d" % (len(n64rom.F), len(fns)))
    print("PAIRED: %d functions  ->  %s" % (len(pairs), args.csv))
    named = sum(1 for p in pairs if p[2])
    print("  of which the PC side already has a name: %d" % named)


if __name__ == '__main__':
    main()
