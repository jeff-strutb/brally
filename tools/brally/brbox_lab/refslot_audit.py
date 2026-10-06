#!/usr/bin/env python3
"""refslot_audit.py -- verify every T4 relocation slot the image gate fills
from the ORIGINAL's own bytes (ref_fill), which "0 differing bytes" cannot see.

Runs image_build's own collect_dll with one recording hook at the ref-fill
line, then for each recorded slot derives what OUR object says the target is
and checks it against what the ORIGINAL's dword points at:

  SAME-TEXT   target in the function's own .text section (jump table / $L):
              original dword must equal va + plen + (sym.val + addend - fn.val)
  CONTENT     target in initialised data (.rdata/.data/other .text): our bytes
              at sym.val+addend, up to the next symbol in that section (cap
              256), must equal the original image's bytes at the dword
  BSS         uninitialised target: no content to compare; checked for
              CONSISTENCY instead -- every slot naming the same (object,
              symbol) must decode to the same original address
"""
import collections, os, struct, sys, types
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'brally'))
os.chdir(ROOT)

src = open(os.path.join(ROOT, 'tools', 'brally', 'image_build.py')).read()
needle = "                    code[off:off + 4] = body_orig[off:off + 4]\n                    fromref += 1\n                    continue\n                addend"
assert src.count(needle) == 1, 'ref-fill site moved; update the hook'
src = src.replace("            yield va, name, pre + bytes(code), unres, fromref\n", "            YIELDED.append((path, va, pre + bytes(code)))\n            yield va, name, pre + bytes(code), unres, fromref\n", 1)
src = src.replace(needle, "                    REC.append((path, sy, t, off, rt, va, plen, bytes(code[off:off + 4]), secs, syms, d))\n" + needle, 1)
mod = types.ModuleType('image_build_aud')
mod.__file__ = os.path.join(ROOT, 'tools', 'brally', 'image_build.py')
mod.REC = []
mod.YIELDED = []
exec(compile(src, mod.__file__, 'exec'), mod.__dict__)
import image_build as _real
mod._compile_dll_obj = _real._compile_dll_obj
def RELOCS_OF(d, secs, si):
    nsec = struct.unpack_from('<H', d, 2)[0]
    o = 20 + (si - 1) * 40
    roff = struct.unpack_from('<I', d, o + 24)[0]; nrel = struct.unpack_from('<H', d, o + 32)[0]
    return [struct.unpack_from('<IIH', d, roff + r * 10) for r in range(nrel)]


SITES = []

def main():
    best, names_at, unplaced, unbuildable = mod.collect_dll(False)
    REC = mod.REC
    chosen = {}
    for path, va, code in mod.YIELDED:
        if va in best and best[va][1] == code:
            chosen.setdefault(va, set()).add(path)
    REC = [r for r in REC if r[0] in chosen.get(r[5], ())]
    print('placed T4 with a chosen object:', len(chosen), ' records kept:', len(REC))
    if os.environ.get('DUMPVA'):
        dv = int(os.environ['DUMPVA'], 16)
        for (path, sy, t, off, rt, va, plen, our4, secs, syms, d) in REC:
            if va == dv:
                ob = open(os.path.join(mod.ORIG_DIR, '0x%08X.bin' % va), 'rb').read()
                print('%s sym %s val %#x plen %d slot +%#x rt %d -> %s  our4 %s orig4 %s' % (path, sy['name'], sy['val'], plen, off, rt, t['name'] if t else '?', our4.hex(), ob[plen+off:plen+off+4].hex()))
        return
    placed = set(best)

    img = open(os.path.join(ROOT, 'reference', 'brally', 'orig', 'BRGlide.dll'), 'rb').read()
    pe = struct.unpack_from('<I', img, 0x3c)[0]
    nsec = struct.unpack_from('<H', img, pe + 6)[0]
    ohs = struct.unpack_from('<H', img, pe + 20)[0]
    base = struct.unpack_from('<I', img, pe + 52)[0]
    S = [struct.unpack_from('<8sIIII', img, pe + 24 + ohs + 40 * k) for k in range(nsec)]
    def at(va, n):
        for nm, vs, v, rs, ro in S:
            if base + v <= va < base + v + vs:
                o = va - base - v
                if o >= rs:
                    return None               # past raw data: bss
                return img[ro + o: ro + min(o + n, rs)]
        return 'OUT'

    res = collections.Counter(); bad = []; bss = collections.defaultdict(set); ext = collections.defaultdict(set); bsssites = collections.defaultdict(list); stat = collections.defaultdict(set); statsites = collections.defaultdict(list); extsites = collections.defaultdict(list)
    seen = set()
    for (path, sy, t, off, rt, va, plen, our4, secs, syms, d) in REC:
        if va not in placed:
            continue
        key = (va, off)
        if key in seen:
            continue
        seen.add(key)
        origdw = struct.unpack_from('<I', img, 0)[0]
        ob = open(os.path.join(mod.ORIG_DIR, '0x%08X.bin' % va), 'rb').read()
        O = struct.unpack_from('<I', ob, plen + off)[0]
        A = struct.unpack_from('<i', our4, 0)[0]
        if t is None:
            res['no-symbol'] += 1; bad.append((va, off, '?', 'no target symbol')); continue
        tsec = secs.get(t['sec'])
        if rt == mod.REL_REL32:
            tgt_abs = (va + plen + off + 4 + struct.unpack('<i', struct.pack('<I', O))[0]) & 0xffffffff
        else:
            tgt_abs = O
        if t['sec'] == sy['sec']:
            want = va + plen + (t['val'] + A - sy['val'])
            if rt == mod.REL_REL32:
                want = va + plen + (t['val'] - sy['val'])  # addend is -4 style for rel; compare target
                ok = tgt_abs == want or tgt_abs == (want + A + 4) & 0xffffffff
            else:
                ok = tgt_abs == want
            res['same-text ' + ('OK' if ok else 'BAD')] += 1
            if not ok:
                bad.append((va, off, t['name'], 'same-text: orig %08X want %08X' % (tgt_abs, want)))
            continue
        if tsec is None or t['sec'] <= 0:
            b0 = (tgt_abs - A) & 0xffffffff if rt != mod.REL_REL32 else tgt_abs
            SITES.append((t['name'], va, off, A, tgt_abs, os.path.basename(path)))
            ext[t['name']].add(b0); extsites[t['name']].append((va, off, b0)); res['external'] += 1; continue
        toff = t['val'] + (A if rt != mod.REL_REL32 else 0)
        if tsec['flags'] & 0x80:           # uninitialised data
            SITES.append((t['name'], va, off, toff - t['val'], tgt_abs, os.path.basename(path))); bss[(path, t['name'], toff)].add(tgt_abs); bsssites[(path, t['name'], toff)].append((va, off, tgt_abs)); res['bss'] += 1; continue
        nxt = sorted(s['val'] for s in syms if s['sec'] == t['sec'] and s['val'] > t['val'])
        end = min(nxt[0] if nxt else tsec['size'], t['val'] + 256, tsec['size'])
        n = max(0, end - toff)
        ours = d[tsec['praw'] + toff: tsec['praw'] + toff + n]
        if t['name'].startswith('$SG') or t['name'].startswith('??_C'):
            z = ours.find(b'\0'); ours = ours[:z + 1] if z >= 0 else ours
        elif len(ours) > 4 and not any(ours[4:]):
            ours = ours[:4]
        elif len(ours) > 8 and not any(ours[8:]):
            ours = ours[:8]
        SITES.append((t['name'], va, off, toff - t['val'], tgt_abs, os.path.basename(path)))
        theirs = at(tgt_abs, n)
        if theirs == 'OUT':
            res['content BAD'] += 1; bad.append((va, off, t['name'], 'orig dword %08X outside the image' % tgt_abs)); continue
        if theirs is None:
            res['content BAD'] += 1; bad.append((va, off, t['name'], 'orig points into bss, ours is initialised data')); continue
        relo = set()
        for (rv, _si, _rt) in (mod.parse.__globals__ and []) or []:
            pass
        mask = set()
        for (rv, _si, _rt) in RELOCS_OF(d, secs, t['sec']):
            for q in range(rv - toff, rv - toff + 4):
                mask.add(q)
        ok = all(i in mask or i >= len(theirs) or ours[i] == theirs[i] for i in range(len(ours))) and len(theirs) >= len(ours)
        res['content ' + ('OK' if ok else 'BAD')] += 1
        sb = (tgt_abs - (toff - t['val'])) & 0xffffffff
        stat[(path, t['name'])].add(sb); statsites[(path, t['name'])].append((va, off, sb, ok))
        if not ok:
            full = d[tsec['praw'] + t['val']: tsec['praw'] + end]
            there = at(sb, len(full)) or b''
            same = sum(1 for i in range(min(len(full), len(there))) if full[i] == there[i])
            bad.append((va, off, t['name'], 'content differs (static %d/%d bytes equal at base %08X): ours %s orig %s' % (same, len(full), sb, ours[:16].hex(), theirs[:16].hex())))
    for k, v in bss.items():
        if len(v) != 1:
            res['bss INCONSISTENT'] += 1
            bad.append((0, 0, k[1], 'bss symbol (%s +%d) decodes to %d original addresses: %s' % (os.path.basename(k[0]), k[2], len(v), ' '.join('%08X<-%08X+%x' % (a, fva, fo) for fva, fo, a in bsssites[k]))))
    if os.environ.get('EXTNAMES'):
        for nm in os.environ['EXTNAMES'].split(','):
            print('EXT', nm, sorted(set('%08X' % b for b in ext.get(nm, ()))), [('%08X+%x' % (a, o)) for a, o, _ in extsites.get(nm, [])][:6])
    if os.environ.get('SITEDUMP'):
        import json
        json.dump(SITES, open(os.environ['SITEDUMP'], 'w'))
    print('reference-filled T4 slots audited:', len(seen))
    for k, v in sorted(res.items()):
        print('  %-22s %d' % (k, v))
    print('bss symbols checked for consistency:', len(bss))
    for k, v in ext.items():
        if len(v) != 1:
            res['external INCONSISTENT'] += 1
            by = collections.defaultdict(list)
            for (fva, foff, a) in extsites[k.split('+')[0]]:
                by[a].append('%08X+%x' % (fva, foff))
            bad.append((0, 0, k, 'external decodes to %d original addresses: %s' % (len(v), ' | '.join('%08X<-%s' % (a, ','.join(s[:4])) for a, s in sorted(by.items())))))
    for k, v in stat.items():
        if len(v) != 1:
            res['static INCONSISTENT'] += 1
            bad.append((0, 0, k[1], 'static (%s) base decodes to %d addresses: %s' % (os.path.basename(k[0]), len(v), ' '.join('%08X<-%08X+%x' % (b, fva, fo) for fva, fo, b, _ in statsites[k][:8]))))
    print('initialised statics checked for base consistency:', len(stat), ' inconsistent:', res['static INCONSISTENT'])
    print('external names checked for consistency:', len(ext), ' inconsistent:', res['external INCONSISTENT'])
    for b in sorted(bad, key=lambda b: b[3][:12]):
        print('  BAD %08X +%#x %s: %s' % b)
    print('TOTAL BAD:', len(bad))


if __name__ == '__main__':
    main()
