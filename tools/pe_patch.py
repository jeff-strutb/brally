#!/usr/bin/env python3
"""Patch recompiled functions into the original DLL with relocations resolved.

Takes the original DLL, a directory of matched .bin files (one per function,
named 0x<VA>.bin), and produces a patched DLL with those functions replaced.
Relocations in each function's .obj are resolved to their original addresses
so the patched bytes are bit-exact, not just encoding-exact.

Usage:
    python3 tools/pe_patch.py orig/BRGlide.dll build/match/verified/ build/BRGlide_patched.dll
"""
import csv
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from relocmap import load_maps, load_learned  # noqa: E402
from reloc_fill import fill_function  # noqa: E402


def read_pe_text_info(path):
    with open(path, 'rb') as f:
        f.seek(0x3C)
        pe_off = struct.unpack('<I', f.read(4))[0]
        f.seek(pe_off + 6)
        nsec = struct.unpack('<H', f.read(2))[0]
        f.seek(pe_off + 20)
        opt_size = struct.unpack('<H', f.read(2))[0]
        f.seek(pe_off + 24 + 28)
        image_base = struct.unpack('<I', f.read(4))[0]
        sections = []
        for i in range(nsec):
            f.seek(pe_off + 24 + opt_size + i * 40)
            name = f.read(8).rstrip(b'\x00').decode()
            vsize, rva, rawsize, rawoff = struct.unpack('<IIII', f.read(16))
            sections.append((name, rva, vsize, rawoff, rawsize))
    return image_base, sections


def build_obj_index():
    """Map (function_name, variant) -> .obj path, and name -> [variants]."""
    from reloc_learn import live_objs
    from match_diff import parse_coff_obj
    idx = {}
    for variant in ('O2', 'Od'):
        objs, _ = live_objs((variant,))
        for obj_path in objs:
            try:
                funcs = parse_coff_obj(obj_path)
            except Exception:
                continue
            for name in funcs:
                idx[(name, variant)] = obj_path
    return idx


def patch(orig_dll, match_dir, out_dll, funcs_csv=None):
    image_base, sections = read_pe_text_info(orig_dll)

    with open(orig_dll, 'rb') as f:
        data = bytearray(f.read())

    func_sizes = {}
    if funcs_csv:
        with open(funcs_csv) as f:
            for row in csv.DictReader(f):
                va = int(row['va'], 16)
                func_sizes[va] = int(row['size'])

    fnmap, glmap = load_maps()
    lrmap = load_learned()
    glmap.update(lrmap)

    # Build reverse map: VA -> (name, variant)
    va_to_info = {}
    rep = os.path.join(ROOT, 'build', 'match', 'report.csv')
    if os.path.exists(rep):
        for r in csv.DictReader(open(rep)):
            if r.get('name') and r.get('va') and r.get('status') == 'match':
                va_to_info[int(r['va'], 16)] = (r['name'], r.get('opt', 'O2'))

    obj_idx = build_obj_index()

    patched = 0
    resolved = 0
    unresolved = 0
    skipped = 0

    for bin_path in sorted(glob.glob(os.path.join(match_dir, '0x*.bin'))):
        basename = os.path.basename(bin_path)
        va = int(basename.replace('.bin', ''), 16)

        with open(bin_path, 'rb') as f:
            raw_bytes = f.read()

        rva = va - image_base
        file_off = None
        for name, sec_rva, vsize, rawoff, rawsize in sections:
            if sec_rva <= rva < sec_rva + vsize:
                file_off = rawoff + (rva - sec_rva)
                break

        if file_off is None:
            skipped += 1
            continue

        orig_size = func_sizes.get(va)
        if orig_size is not None and len(raw_bytes) != orig_size:
            skipped += 1
            continue

        info = va_to_info.get(va)
        filled = None
        if info:
            func_name, variant = info
            obj_key = (func_name, variant)
            if obj_key in obj_idx:
                filled = fill_function(obj_idx[obj_key], func_name, va,
                                       fnmap, glmap, len(raw_bytes))

        if filled is not None:
            data[file_off:file_off + len(filled)] = filled
            resolved += 1
        else:
            data[file_off:file_off + len(raw_bytes)] = raw_bytes
            unresolved += 1
            print(f"  WARN 0x{va:08X}: relocs unresolvable, patching raw bytes")
        patched += 1

    with open(out_dll, 'wb') as f:
        f.write(data)

    print(f"patched {patched} functions ({resolved} reloc-resolved, "
          f"{unresolved} encoding-only), skipped {skipped}")
    print(f"wrote {out_dll}")


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(f"usage: {sys.argv[0]} <original.dll> <match_dir/> <output.dll> "
              f"[functions.csv]")
        sys.exit(1)
    funcs = sys.argv[4] if len(sys.argv) > 4 else None
    patch(sys.argv[1], sys.argv[2], sys.argv[3], funcs)
