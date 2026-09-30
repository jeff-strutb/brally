#!/usr/bin/env python3
"""The verified T3 build's placement, as data the port links against.

tools/image_build_t3.py is the Milestone-1 build: every T4 and certified T3
function compiled and placed at its original address, verified (A7 whole-image
IDENTICAL). It decides, with no overlaps, which object and symbol owns each
VA, and what address every relocation slot of every placed function resolves
to (T4 slots from the maps and reference bytes, T3 slots from the same
per-object resolution the A5 oracle certified them under).

This runs that build's collectors unchanged (no image is written) with its
placer wrapped, and records:

  build/wasm/placement.csv  va,name,lane,src      one row per placed function
  build/wasm/sites.csv      src,symbol,addend,va  every resolved slot: the
                            original address `symbol + addend` means in that
                            source file

The port's linker (w2c.py) takes function ownership and addresses from these
first; anything they do not cover is code outside the verified build.
"""
import csv
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
os.chdir(ROOT)

import image_build as ib            # noqa: E402
import image_build_t3 as t3b        # noqa: E402
from relocmap import REL_DIR32, REL_REL32   # noqa: E402
from reloc_fill import parse        # noqa: E402

placements = {}     # va -> (name, lane, obj)
cands = {}          # va -> {name: obj}, every compile that yielded the VA
site_fn = {}        # site key -> VA of a function it came from
sites = {}          # (obj, symbol, addend) -> set(va)
# (va, placed bytes) -> [(obj, symbol, addend, target)]: the slots of every
# body any compile yielded.  The build tries several compiles per function
# (variants, both lanes) and config/reloc_overrides.csv rows are keyed by
# function offset, so in a compile the build did NOT place a row's offset can
# sit on a different symbol: BrCarDrawVehicle's variants bound BrG_6C335C to
# the ambient's blue byte and g_BrDrawByte80 to the light's, the port kept
# whichever row sorted last, and every car was lit orange.  Only the slots of
# the body actually placed are answers.
cand_sites = {}
_lane = ['T4']


def pick(va, name):
    """The obj that produced the function the build finally placed at va.
    C lane candidates carry the placed name; the C++ lane yields its raw
    mangled symbol while the build records a display name, so when no
    candidate matches by name, the C++ candidate is the one."""
    c = cands.get(va, {})
    if name in c:
        return c[name]
    cpp = [o for n, o in c.items() if n.startswith('?')]
    if cpp:
        return cpp[0]
    if len(c) == 1:
        return next(iter(c.values()))
    return None


def undecorate(raw):
    if raw.startswith('?'):
        m = re.match(r'\?\?([01])([A-Za-z_]\w*)@@', raw)
        if m:
            c = m.group(2)
            return '%s::%s%s' % (c, '~' if m.group(1) == '1' else '', c)
        m = re.match(r'\?([A-Za-z_$][\w$]*)@(?:([A-Za-z_]\w*)@)?@', raw)
        if not m:
            return raw
        return m.group(2) + '::' + m.group(1) if m.group(2) else m.group(1)
    s = raw[1:] if raw[:1] in '_@' else raw
    return s.split('@')[0]


_orig_cf = ib.compiled_functions


def recording(objs, fnmap, glmap, only=None, **kw):
    objs = list(objs)
    for va, name, code, unres, fromref in _orig_cf(objs, fnmap, glmap,
                                                    only=only, **kw):
        # which obj produced it: the (single) obj passed in, in practice
        obj = objs[0] if len(objs) == 1 else None
        if obj is not None:
            try:
                d, secs, syms, relocs = parse(obj)
            except Exception:
                d = None
            if d is not None:
                cand_sites[(va, bytes(code))] = record_sites(
                    obj, d, secs, syms, relocs, va, name, code, only)
        placements[va] = (name, _lane[0], obj)
        cands.setdefault(va, {})[name] = obj
        yield va, name, code, unres, fromref


def record_sites(obj, d, secs, syms, relocs, va, name, code, only):
    """The resolved slots of one compiled body: [(obj, symbol, addend, va)]."""
    out = []
    byidx = {s['idx']: s for s in syms}
    pre = ib.PREAMBLES.get('0x%08x' % va, b'')
    plen = len(pre)
    body = code[plen:]
    for sy in syms:
        sec = secs.get(sy['sec'])
        if sy['sec'] <= 0 or not sec or not sec['name'].startswith('.text'):
            continue
        n = sy['name'] if only is not None else sy['name'].lstrip('_@').split('@')[0]
        if (only is not None and only.get(sy['name']) != va) or \
           (only is None and n != name):
            continue
        start = sec['praw'] + sy['val']
        for rva, si, rt in relocs[sy['sec']]:
            off = rva - sy['val']
            if not (0 <= off <= len(body) - 4):
                continue
            t = byidx.get(si)
            if not t:
                continue
            addend = struct.unpack_from('<i', d, start + off)[0]
            val = struct.unpack_from('<I', body, off)[0]
            if val == (addend & 0xFFFFFFFF):
                continue            # a slot the build left unresolved
            if rt == REL_DIR32:
                tgt = (val - addend) & 0xFFFFFFFF
            elif rt == REL_REL32:
                tgt = (val + va + plen + off + 4 - addend) & 0xFFFFFFFF
            else:
                continue
            got = (tgt + addend) & 0xFFFFFFFF
            # Every address the placed image can hold is inside it: the
            # original's image (0x10000000 + SizeOfImage 0x190D000) or the
            # annex just above.  A value outside that is this reader's
            # mistake, not the image's -- a slot read one byte off because
            # the placed body came from a different compile variant than the
            # object whose relocations we walk (0x106B7A98 read as
            # 0x6B7A9815) -- and recording it made the real address look
            # ambiguous, so the port dropped it (g_brTexScanSrcSeen).
            if not (0x10000000 <= got < 0x11A00000):
                continue
            out.append((obj, t['name'], addend, got))
        return out
    return out


ib.compiled_functions = recording


def main():
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        t4_best, names_at, _u, _b = ib.collect_dll(False, None)
    for va, (name, *_r) in t4_best.items():
        placements[va] = (name, 'T4', pick(va, name))
    _lane[0] = 'T3'
    with contextlib.redirect_stdout(buf):
        t3_best, _tu, _tb, annex = t3b.collect_t3(False, None)
    from t3 import certified
    certfile = {int(k, 16): v['file'] for k, v in certified().items()
                if not k.startswith('?') and v.get('ok')}
    for va, (name, *_r) in t3_best.items():
        # the obj that yielded THIS name at this VA -- not whichever compiled last
        placements[va] = (name, 'T3', pick(va, name), certfile.get(va))
    fa, _annex = t3b._force_annex(annex)
    for va, entry in fa.items():
        placements.setdefault(va, (entry[0], 'T4', None))

    # the slots of the bodies placed, as the placements above rank them
    placed_code = {va: e[1] for va, e in t4_best.items()}
    placed_code.update((va, e[1]) for va, e in fa.items() if va not in placed_code)
    placed_code.update((va, e[1]) for va, e in t3_best.items())
    by_va = {}
    for (va, _c), rec in cand_sites.items():
        by_va.setdefault(va, []).append(rec)
    n_annex = n_nosites = 0
    for va, code in placed_code.items():
        rec = cand_sites.get((va, bytes(code)))
        if rec is not None:
            recs = [rec]
        else:
            # An annexed body: the VA holds a jmp thunk and the annex builds
            # the body from the placement's object itself, so no compile
            # matches the placed bytes.  That object's compiles are the answer.
            obj = placements[va][2]
            recs = [r for r in by_va.get(va, []) if r and r[0][0] == obj]
            if recs:
                n_annex += 1
            else:
                n_nosites += 1
                continue
        for rec in recs:
            for obj, sym, add, got in rec:
                sites.setdefault((obj, sym, add), set()).add(got)
                site_fn[(obj, sym, add)] = va

    # obj -> source file
    src_of = {}
    for rep in ('build/match/report.csv', 'build/match/report_cpp.csv'):
        if os.path.exists(rep):
            for r in csv.DictReader(open(rep)):
                b = os.path.splitext(os.path.basename(r['file']))[0]
                src_of.setdefault(b, r['file'])

    # the reports name each VA's file with its full path; basenames collide
    # (audio/br_input.c and controls/br_input.c)
    file_of_va = {}
    for rep in ('build/match/report.csv', 'build/match/report_cpp.csv'):
        if os.path.exists(rep):
            for r in csv.DictReader(open(rep)):
                file_of_va.setdefault(int(r['va'], 16), r['file'])

    def src(obj):
        if not obj:
            return ''
        b = os.path.splitext(os.path.basename(obj))[0]
        b = re.sub(r'_sweep_[0-9A-F]{8}_\d+$', '', b)
        return src_of.get(b, b)

    def placed_file(va):
        # a certified function's @t3 tag names its file: authoritative
        p = placements.get(va)
        if p is None:
            return file_of_va.get(va)
        return (p[3] if len(p) > 3 and p[3] else
                file_of_va.get(va) or src(p[2]))

    os.makedirs('build/wasm', exist_ok=True)
    with open('build/wasm/placement.csv', 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['va', 'name', 'lane', 'src'])
        for va in sorted(placements):
            name, lane = placements[va][:2]
            w.writerow(['0x%08X' % va, name, lane, placed_file(va)])
    n_amb = 0
    with open('build/wasm/sites.csv', 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['src', 'symbol', 'name', 'addend', 'va'])
        for (obj, sym, add), vs in sorted(sites.items(), key=lambda x: (x[0][0] or '', x[0][1], x[0][2])):
            if len(vs) != 1:
                n_amb += 1
                continue
            # the file is the one the placement gives the function the site
            # sits in: obj basenames collide (audio/ and controls/ both have
            # br_input.c), and a VA the reports list under a C draft as well
            # as its certified C++ file (0x1004AEE0: br_seasonscreen.c and
            # BrExt_10052030_1004AEE0.cpp) is the certified file's
            f = placed_file(site_fn.get((obj, sym, add))) or src(obj)
            w.writerow([f, sym, undecorate(sym), add, '0x%08X' % next(iter(vs))])
    lanes = {}
    for _n, lane, *_o in placements.values():
        lanes[lane] = lanes.get(lane, 0) + 1
    print('t3manifest: %d functions placed (%s), %d resolved slots '
          '(%d ambiguous dropped); slots from %d annexed bodies\' objects, '
          '%d placed bodies with no recorded compile' % (
              len(placements), ', '.join('%s %d' % kv for kv in sorted(lanes.items())),
              len(sites) - n_amb, n_amb, n_annex, n_nosites))


if __name__ == '__main__':
    main()
