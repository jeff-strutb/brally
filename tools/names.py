"""Recover function names from string references.

Boss Rally's error strings are function-scoped, e.g.

    CHK_FReadOpen(): error opening file %s.
    DDraw_DoInit: Bitmap %d failed to load!

Every relocated dword inside .text that points at a C string is a string
reference; we attribute it to the enclosing function. When a referenced string
carries a `Name():` or `Name:` prefix, that is the enclosing function's own
name with very high probability, so we adopt it.

Writes config/names.csv and config/strings.csv.
"""
import sys, os, csv, re, struct, bisect, collections
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib

# "Foo():" or "Foo_Bar:" at the start of a diagnostic string
NAME_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]{2,63})\s*(?:\(\))?\s*:')


def collect_strings(p, min_len=4):
    """Every printable NUL-terminated string in initialised sections."""
    out = {}
    for s in p.sections:
        if s.name not in ('.rdata', '.data', '.text'):
            continue
        blob = p.data[s.raw_ptr:s.raw_ptr + s.raw_size]
        base = p.image_base + s.vaddr
        for m in re.finditer(rb'[\x20-\x7e\t]{%d,}\x00' % min_len, blob):
            out[base + m.start()] = m.group()[:-1].decode('latin1')
    return out


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else 'orig/BRD3D.dll'
    fcsv = sys.argv[2] if len(sys.argv) > 2 else 'config/functions.csv'
    p = pelib.load(src)
    text, text_va = p.text()
    strings = collect_strings(p)

    funcs = [(int(r['va'], 16), int(r['size']), r['name'])
             for r in csv.DictReader(open(fcsv))]
    funcs.sort()
    starts = [f[0] for f in funcs]

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None
        s, size, _ = funcs[i]
        return s if va < s + size else None

    # every relocation inside .text whose value is a string address
    refs = collections.defaultdict(set)      # func va -> {string va}
    strefs = collections.Counter()           # string va -> ref count
    for rva in p.relocs:
        va = p.image_base + rva
        if not (text_va <= va < text_va + len(text)):
            continue
        o = p.rva_to_off(rva)
        if o is None:
            continue
        val = struct.unpack('<I', p.data[o:o + 4])[0]
        if val in strings:
            f = owner(va)
            if f is not None:
                refs[f].add(val)
                strefs[val] += 1

    # adopt Name(): prefixes
    names = {}
    conflicts = collections.defaultdict(set)
    for f, svas in refs.items():
        for sv in svas:
            m = NAME_RE.match(strings[sv])
            if not m:
                continue
            nm = m.group(1)
            if nm.lower() in ('error', 'warning', 'note', 'http', 'https', 'usage'):
                continue
            if f in names and names[f] != nm:
                conflicts[f].add(nm)
            else:
                names[f] = nm

    # de-duplicate: same name on several functions gets a numeric suffix
    byname = collections.defaultdict(list)
    for f, nm in names.items():
        byname[nm].append(f)
    final = {}
    for nm, fs in byname.items():
        fs.sort()
        for i, f in enumerate(fs):
            final[f] = nm if len(fs) == 1 else "%s__%d" % (nm, i)

    os.makedirs('config', exist_ok=True)
    with open('config/names.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'name', 'source'])
        for va, size, exp in funcs:
            nm = exp or final.get(va, '')
            if nm:
                w.writerow(['0x%08X' % va, nm, 'export' if exp else 'string'])
    with open('config/strings.csv', 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['va', 'refs', 'text'])
        for sv, n in strefs.most_common():
            w.writerow(['0x%08X' % sv, n, strings[sv]])

    named = sum(1 for va, size, exp in funcs if exp or va in final)
    print("strings found        %d" % len(strings))
    print("string refs resolved %d  (in %d functions)" % (sum(strefs.values()), len(refs)))
    print("functions named      %d / %d  (%.1f%%)" % (named, len(funcs), 100.0 * named / len(funcs)))
    print("name conflicts       %d" % len(conflicts))
    print("wrote config/names.csv, config/strings.csv")
    print("\nsample recovered names:")
    for va in sorted(final)[:25]:
        print("   %08X  %s" % (va, final[va]))


if __name__ == '__main__':
    main()
