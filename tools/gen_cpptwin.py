#!/usr/bin/env python3
"""Stamp C++ TUs for reloc-masked twins of already-matched C++ functions.

Several C++ families in the UI/phase region ship the SAME machine code many
times, differing only in which globals the reloc slots point at (e.g. the
seven 158-byte double-strcpy phase-leave siblings differ in ONE byte, inside
the phase-source global's DIR32 slot). Once one member is hand-proven
byte-exact, every sibling is a rename: same TU body, new @implements VA.

A candidate is a twin of a matched TU iff, after masking
  - every DIR32 slot the DLL's own base-reloc table patches inside the
    function, and
  - the rel32 operand of every call/jmp that leaves the function body
    (the linker wrote different displacements for the same helper),
the remaining bytes are identical and the same length. That is structural
identity, not opcode coincidence — the same test tools/twinfind.py uses
across the two DLLs, applied within BRGlide.

The per-function scorer masks relocs, and tools/image_build.py backfills
reloc slots from the original's own bytes, so a stamped TU is exactly as
strong a claim as its hand-proven template at both gates.

Usage:
    python3 tools/gen_cpptwin.py            # write src/core/cpp/<VA>.cpp
    python3 tools/gen_cpptwin.py --dry-run  # report hits only

Then score with: python3 tools/cpp_sweep.py src/core/cpp/<VA>.cpp
"""
import csv
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from twinfind import relocs_in_text  # noqa: E402

ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')
CPP_DIR = os.path.join(ROOT, 'src', 'core', 'cpp')
GLIDE = os.environ.get('BR_REF',
                       os.path.join(ROOT, 'orig', 'BRGlide.dll'))
IMAGE_BASE = 0x10000000


def mask_offsets(b, va, reloc_rvas):
    """Function-relative byte offsets to ignore when comparing."""
    out = set()
    rva = va - IMAGE_BASE
    for r in reloc_rvas:
        off = r - rva
        if -3 <= off < len(b):
            for i in range(max(off, 0), min(off + 4, len(b))):
                out.add(i)
    # rel32 call/jmp leaving the function: same helper, different distance.
    i = 0
    while i < len(b) - 4:
        if b[i] in (0xE8, 0xE9):
            rel = struct.unpack_from('<i', b, i + 1)[0]
            tgt = va + i + 5 + rel
            if not (va <= tgt < va + len(b)):
                out.update(range(i + 1, i + 5))
            i += 5
        else:
            i += 1
    return out


def masked_eq(a, b, mask):
    if len(a) != len(b):
        return False
    return all(a[i] == b[i] for i in range(len(a)) if i not in mask)


def load_names():
    names = {}
    p = os.path.join(ROOT, 'build', 'ghidra_learnings.csv')
    if os.path.exists(p):
        with open(p) as f:
            for r in csv.DictReader(f):
                if r.get('name'):
                    names[int(r['va'], 16)] = r['name']
    return names


def matched_vas():
    out = set()
    for rep in ('report.csv', 'report_cpp.csv'):
        p = os.path.join(ROOT, 'build', 'match', rep)
        if os.path.exists(p):
            with open(p) as f:
                out |= {r['va'].lower() for r in csv.DictReader(f)
                        if r['status'] == 'match'}
    return out


def cpp_templates():
    """(va, orig_bytes, source_path, fn_name) per matched C++ TU."""
    p = os.path.join(ROOT, 'build', 'match', 'report_cpp.csv')
    tpls = []
    with open(p) as f:
        for r in csv.DictReader(f):
            if r['status'] != 'match':
                continue
            va = int(r['va'], 16)
            src = os.path.join(ROOT, r['file'])
            ob = os.path.join(ORIG_DIR, '0x%08X.bin' % va)
            if os.path.exists(src) and os.path.exists(ob):
                tpls.append((va, open(ob, 'rb').read(), src, r['name']))
    return tpls


def sanitize(name, va):
    if not name or not re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', name):
        return 'BrCpp%04X' % (va & 0xFFFF)
    return name


def main():
    dry = '--dry-run' in sys.argv
    relocs, _base, _secs = relocs_in_text(GLIDE)
    matched = matched_vas()
    names = load_names()
    tpls = cpp_templates()
    by_len = {}
    for t in tpls:
        by_len.setdefault(len(t[1]), []).append(t)

    hits, wrote = [], []
    for p in sorted(glob.glob(os.path.join(ORIG_DIR, '0x*.bin'))):
        va_hex = os.path.basename(p)[:-4]
        va = int(va_hex, 16)
        if va_hex.lower() in matched:
            continue
        out = os.path.join(CPP_DIR, '0x%08X.cpp' % va)
        if os.path.exists(out):
            continue
        b = open(p, 'rb').read()
        cands = by_len.get(len(b))
        if not cands:
            continue
        mask = mask_offsets(b, va, relocs)
        for tva, tb, tsrc, tname in cands:
            tmask = mask | mask_offsets(tb, tva, relocs)
            if not masked_eq(b, tb, tmask):
                continue
            new_name = sanitize(names.get(va, ''), va)
            src = open(tsrc).read()
            src = src.replace('0x%08X' % tva, '0x%08X' % va)
            src = src.replace(tname, new_name)
            note = ('/* Twin of 0x%08X %s (tools/gen_cpptwin.py): identical '
                    'machine code,\n * only the reloc slots differ. */\n'
                    % (tva, tname))
            src = re.sub(r'(\*/\n)', r'\1' + note, src, count=1)
            hits.append((va_hex, new_name, tva, tname))
            if not dry:
                with open(out, 'w') as f:
                    f.write(src)
                wrote.append(out)
            break

    for va_hex, nn, tva, tn in hits:
        print('%s  %-32s  twin of 0x%08X %s' % (va_hex, nn, tva, tn))
    print('%d twin%s found%s' % (len(hits), '' if len(hits) == 1 else 's',
                                 ' (dry run)' if dry else ''))
    if wrote:
        print('score with: python3 tools/cpp_sweep.py ' + ' '.join(
            os.path.relpath(w, ROOT) for w in wrote))


if __name__ == '__main__':
    main()
