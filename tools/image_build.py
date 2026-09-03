#!/usr/bin/env python3
"""Assemble a whole image from our compiled code plus the original's bytes, and
diff it against the original.

This is the test the per-function checks cannot perform.  match_sweep.py asks
"do these bytes equal the original's bytes at this address", one function at a
time, and a function that answers yes can still be wrong about WHERE it goes:
two functions can claim the same address, a claimed range can overrun into its
neighbour, a size can be wrong.  Those are not hypothetical -- a stale row set
double-claiming 38 addresses is what the report was quietly doing until it was
caught by hand.

So this does not check functions.  It checks the map: every claim is laid into
one image, collisions are refused rather than resolved, and the result is
compared with the original.  Undecompiled functions are not stubbed or skipped;
the original's own bytes stand in for them, which is the standard matching-decomp
arrangement -- coverage decides how much of the image is OURS, not whether the
image can be built at all.

A clean run means: everything we claim to have reproduced, assembled at the
addresses we claim, reproduces the original image exactly.

Usage:
    python3 tools/image_build.py [--out build/BRGlide_rebuilt.dll]
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import REL_DIR32, REL_REL32                      # noqa: E402
from reloc_fill import parse, load_maps, resolve               # noqa: E402
from reloc_learn import live_objs                              # noqa: E402
from pe_patch import read_pe_text_info                         # noqa: E402
from match_sweep import PREAMBLES                              # noqa: E402

ORIG_DLL = os.environ.get('BR_REF',
             os.path.join(ROOT, 'orig', 'BRGlide.dll'))
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')


def compiled_functions(objs, fnmap, glmap, only=None):
    """Yield (va, name, filled_bytes, n_unresolved) for every function we build.

    `only` is a {raw COFF symbol name -> va} map. When given, symbols are
    placed by that map alone and the C undecorator is not consulted -- the
    C++ lane's symbols are MSVC-mangled ('?Name@@YAH...@Z'), which the
    cdecl/stdcall/fastcall undecoration below would mince into a name that
    is in no map.  Its addresses come from report_cpp.csv, not from fnmap.
    """
    for path in objs:
        try:
            d, secs, syms, relocs = parse(path)
        except Exception:
            continue
        byidx = {s['idx']: s for s in syms}
        for sy in syms:
            sec = secs.get(sy['sec'])
            if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
                continue
            if only is not None:
                if sy['name'] not in only:
                    continue
                name, va = sy['name'], only[sy['name']]
            else:
                # Undecorate: cdecl '_f', stdcall '_f@12', fastcall '@f@12'.
                name = sy['name'].lstrip('_@').split('@')[0]
                if name not in fnmap:
                    continue
                va = fnmap[name]
            ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
            if not os.path.exists(ob):
                continue
            orig = open(ob, 'rb').read()
            # A few originals carry a 16-byte link-stage preamble (jmp +0x0b
            # over nops) fused into the map entry; the compiler's output is the
            # body, starting at +len(pre).  match_sweep strips+verifies the
            # preamble and matches the body -- mirror that here or the body lands
            # at offset 0 and the whole function reads as differing.  The
            # preamble bytes are link output, laid down verbatim.
            pre = PREAMBLES.get('0x%08x' % va, b'')
            plen = len(pre)
            body_n = len(orig) - plen
            body_orig = orig[plen:]
            start = sec['praw'] + sy['val']
            code = bytearray(d[start:start + body_n])
            if len(code) != body_n:
                continue
            unres = 0
            fromref = 0
            for rva, si, rt in relocs[sy['sec']]:
                off = rva - sy['val']            # body-relative
                if not (0 <= off <= body_n - 4):
                    continue
                t = byidx.get(si)
                tgt = resolve(t['name'], fnmap, glmap) if t else None
                if tgt is None:
                    # No name-level address (typically a per-file STATIC, whose
                    # name is not unique across objects). The function is a
                    # masked-match claim, so the reference image's own dword is
                    # the correct slot value -- the same original-bytes
                    # arrangement the rest of the image build stands on.
                    # Counted separately: these slots are taken from the
                    # reference, not derived from a surveyed name.
                    code[off:off + 4] = body_orig[off:off + 4]
                    fromref += 1
                    continue
                addend = struct.unpack_from('<i', code, off)[0]
                if rt == REL_DIR32:
                    val = tgt + addend
                elif rt == REL_REL32:
                    # The body sits at va+plen in the image, so a pc-relative
                    # site resolves against its post-preamble address.
                    val = tgt + addend - (va + plen + off + 4)
                else:
                    code[off:off + 4] = body_orig[off:off + 4]
                    fromref += 1
                    continue
                struct.pack_into('<I', code, off, val & 0xFFFFFFFF)
            yield va, name, pre + bytes(code), unres, fromref


def cpp_claims():
    """(obj_path, {mangled symbol: va}, {va: name}) for every 4/4 C++ match.

    The C++ lane is scored by tools/cpp_sweep.py into report_cpp.csv and never
    reaches report.csv, so before this it was the one body of verified work the
    image gate could not see: 149 functions inside this same DLL whose bytes
    were checked one at a time but whose ADDRESSES were never laid down beside
    the C claims.  Placement and overlap are exactly what this tool exists to
    check, so it has to place them too.

    Only 4/4 rows count -- a 3/4 row's .text may well be right, but 'match' on
    this lane means all four pieces, and the image gate does not get to use a
    looser bar than the sweep that produced the row.
    """
    import csv
    import cpp_score
    report = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    if not os.path.exists(report):
        return []
    tags = [_opt_tag(o) for o in cpp_score.DEFAULT_OPTS]
    out = []
    for r in csv.DictReader(open(report)):
        if r.get('status') != 'match' or r.get('pieces') != '4/4':
            continue
        va = int(r['va'], 16)
        src = os.path.join(ROOT, r['file'])
        base = os.path.splitext(os.path.basename(r['file']))[0]
        # cpp_sweep tags each obj with the INDEX into DEFAULT_OPTS it was built
        # at; the report records only the opt's short tag, so map back.
        try:
            i = tags.index(r['opt'])
        except ValueError:
            continue
        obj = os.path.join(cpp_score.OBJ_DIR,
                           '%s_sweep_%08X_%d.obj' % (base, va, i))
        if not os.path.exists(obj):
            continue
        _impl, sym, kind = cpp_score.parse_implements_name(src, va)
        raw = _raw_symbol(obj, sym, kind)
        if raw is None:
            continue
        out.append((obj, {raw: va}, r['name'] or sym or raw))
    return out


def _raw_symbol(obj, sym, kind):
    """The RAW COFF symbol cpp_sweep scored, for this obj.

    The @cpp_symbol tag is not always the literal COFF name: a cdecl one is
    written without its leading underscore, and cpp_sweep resolves the rest
    through cpp_score.find_symbol (class name -> ??1Foo@@..., prefer=kind).
    parse_coff_code's keys are a mix of raw and undecorated, so ask it which
    symbol was scored and then map that answer back to the raw name the COFF
    parser here will see.  Placing anything else would place a function the
    sweep never scored.
    """
    import cpp_score
    import match_diff
    try:
        d, secs, syms, _relocs = parse(obj)
    except Exception:
        return None
    raws = [s['name'] for s in syms
            if s['sec'] > 0 and secs.get(s['sec'], {}).get(
                'name', '').startswith('.text')]
    if sym in raws:
        return sym
    try:
        key = cpp_score.find_symbol(cpp_score.parse_coff_code(obj), sym,
                                    prefer=kind or 'dtor')
    except Exception:
        key = None
    if key is None:
        return None
    if key in raws:
        return key
    for n in raws:
        if match_diff.undecorate(n) == key:
            return n
    return None


def _opt_tag(opt):
    import cpp_sweep
    return cpp_sweep._opt_tag(opt)


def main():
    out = None
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]

    fnmap, glmap = load_maps()
    objs, _ = live_objs()

    # Only place what the tree actually CLAIMS to have reproduced. fnmap holds
    # every tagged function including the ones still diffing; laying those in
    # would measure the size of the undone work, not the truth of the claims.
    import csv
    claimed = {}
    names_at = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'build', 'match',
                                              'report.csv'))):
        if r.get('va') and r.get('status') == 'match':
            va = int(r['va'], 16)
            claimed[va] = r
            names_at.setdefault(va, set()).add(r['name'])

    # Take the build the SCORER matched -- its optimisation level, from its
    # source file -- not whichever build happens to resolve most relocations.
    # Every file is compiled at both /O2 and /Od and a function matches under
    # one of them; picking the other yields different code at the same address
    # and reads as a failed claim when nothing is wrong with the claim.
    want = {}
    for va, r in claimed.items():
        if not r.get('opt'):
            continue
        obj = os.path.join(ROOT, 'build', 'match', 'obj_' + r['opt'],
                           os.path.basename(r['file'])[:-2] + '.obj')
        want.setdefault(obj, []).append((va, r['name']))

    best = {}
    for obj, wanted in want.items():
        if not os.path.exists(obj):
            continue
        byname = {n: va for va, n in wanted}
        for va, name, code, unres, fromref in compiled_functions(
                [obj], fnmap, glmap):
            if byname.get(name) == va:
                best[va] = (name, code, unres, fromref, 'C')
    n_c = len(best)

    # The C++ lane, placed beside the C so a cross-lane address collision is
    # visible.  Same reloc filling, same collision refusal, same diff.
    n_cpp = 0
    for obj, symmap, dispname in cpp_claims():
        for va, _sym, code, unres, fromref in compiled_functions(
                [obj], fnmap, glmap, only=symmap):
            names_at.setdefault(va, set()).add(dispname)
            best[va] = (dispname, code, unres, fromref, 'C++')
            n_cpp += 1

    usable = {va: v for va, v in best.items() if v[2] == 0}
    blocked = len(best) - len(usable)
    n_fromref_fns = sum(1 for v in usable.values() if v[3])
    n_fromref_slots = sum(v[3] for v in usable.values())

    # How much of each lane is OURS vs taken from the reference image. A slot
    # filled from the reference cannot fail the diff, so it is not evidence --
    # quoting the image's byte total without this split overstates the C++
    # lane, whose mangled symbols resolve through no surveyed map.
    lane = {}
    for _va, (_n, c, _u, fr, ln) in usable.items():
        d = lane.setdefault(ln, [0, 0, 0])
        d[0] += 1
        d[1] += len(c)
        d[2] += fr * 4

    # GLOBAL CHECK 1 -- overlapping claims. Per-function diffing cannot see it.
    spans = sorted((va, va + len(c), n)
                   for va, (n, c, _u, _f, _l) in usable.items())
    overlaps = [(a, b) for a, b in zip(spans, spans[1:]) if a[1] > b[0]]

    # GLOBAL CHECK 2 -- two DIFFERENT functions claiming one address. The same
    # function appearing twice is not a conflict: every source file is built at
    # both /O2 and /Od, so each function legitimately has two builds. Only
    # distinct names at one address mean a tagging error.
    conflicting = [(va, sorted(ns)) for va, ns in sorted(names_at.items())
                   if len(ns) > 1 and va in usable]

    print(f"functions compiled and addressed : {len(best)}"
          f"  ({n_c} C + {n_cpp} C++)")
    print(f"  every relocation resolved      : {len(usable)}")
    print(f"    of those, {n_fromref_fns} functions fill {n_fromref_slots} "
          f"static/local slots from the reference image")
    print(f"  blocked on an unknown address  : {blocked}")
    print(f"\noverlapping address claims       : {len(overlaps)}")
    for a, b in overlaps[:10]:
        print(f"    {a[2]} [{a[0]:#x}..{a[1]:#x}) runs into {b[2]} at {b[0]:#x}")
    print(f"two names claiming one address   : {len(conflicting)}")
    for va, ns in conflicting[:10]:
        print(f"    {va:#x}: {' vs '.join(ns)}")

    # Lay every usable function into the original image at its claimed address.
    base, secs = read_pe_text_info(ORIG_DLL)
    img = bytearray(open(ORIG_DLL, 'rb').read())
    text = [s for s in secs if s[0].startswith('.text')][0]
    _, trva, tvsize, traw, _ = text

    placed = bytes_placed = 0
    outside = 0
    diffs = []
    for va, (name, code, _u, _f, _l) in sorted(usable.items()):
        off = va - base - trva
        if off < 0 or off + len(code) > tvsize:
            outside += 1
            continue
        fo = traw + off
        if img[fo:fo + len(code)] != code:
            nd = sum(1 for i in range(len(code)) if img[fo + i] != code[i])
            diffs.append((va, name, nd, len(code)))
        img[fo:fo + len(code)] = code
        placed += 1
        bytes_placed += len(code)

    orig = open(ORIG_DLL, 'rb').read()
    delta = sum(1 for i in range(len(orig)) if orig[i] != img[i])

    print(f"\nplaced into the image            : {placed} functions, "
          f"{bytes_placed:,} bytes ({100*bytes_placed/tvsize:.2f}% of .text)")
    for ln in ('C', 'C++'):
        if ln not in lane:
            continue
        n, b, rb = lane[ln]
        print(f"    {ln:<4} {n:4} fns  {b:8,} B  "
              f"({rb:,} B of that filled from the reference"
              f", {100*rb/b:.1f}%)")
    print(f"  landed outside .text           : {outside}")
    print(f"functions differing from original: {len(diffs)}")
    for va, name, nd, sz in sorted(diffs, key=lambda x: -x[2])[:10]:
        print(f"    {va:#x} {name}: {nd}/{sz} bytes")

    print(f"\nASSEMBLED IMAGE vs ORIGINAL: {delta} differing bytes")
    if delta == 0 and not overlaps and not conflicting:
        print("  -> every claim holds at image level.")

    if out:
        os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
        open(out, 'wb').write(bytes(img))
        print(f"\nwrote {out}")


if __name__ == '__main__':
    main()
