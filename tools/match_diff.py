#!/usr/bin/env python3
"""Compare a compiled .obj's functions against the original DLL bytes.

Reads @implements tags from the source file to know which VAs to check,
extracts the corresponding code from the .obj via COFF symbol table parsing,
and diffs against the original bytes in the reference directory.

Usage:
    python3 tools/match_diff.py src/core/slice2_16.c build/match/obj/slice2_16.obj \
            build/match/orig/ build/match/verified/
"""
import os
import re
import struct
import sys


def parse_implements(src_path):
    pattern = re.compile(
        r'@implements\s+0x([0-9A-Fa-f]+)\s+\w+\s+(\w+)')
    results = []
    with open(src_path) as f:
        for line in f:
            m = pattern.search(line)
            if m:
                va = int(m.group(1), 16)
                name = m.group(2)
                results.append((va, name))
    return results


def parse_coff_obj(obj_path):
    """Parse a COFF .obj and return {symbol_name: bytes} for .text symbols."""
    with open(obj_path, 'rb') as f:
        data = f.read()

    machine, nsec, ts, symtab_off, nsyms, opthdr_sz, chars = \
        struct.unpack_from('<HHIIIHH', data, 0)

    sections = []
    off = 20
    for i in range(nsec):
        name_bytes = data[off:off+8]
        vsize, vaddr, rawsize, rawoff = struct.unpack_from('<IIII', data, off+8)
        nrelocs = struct.unpack_from('<H', data, off+32)[0]
        sec_chars = struct.unpack_from('<I', data, off+36)[0]

        if name_bytes[:4] == b'\x00\x00\x00\x00':
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            strtab_base = symtab_off + nsyms * 18
            end = data.index(b'\x00', strtab_base + str_off)
            sname = data[strtab_base + str_off:end].decode()
        else:
            sname = name_bytes.rstrip(b'\x00').decode()

        sections.append({
            'name': sname, 'rawoff': rawoff, 'rawsize': rawsize,
            'chars': sec_chars
        })
        off += 40

    strtab_base = symtab_off + nsyms * 18
    funcs = {}

    i = 0
    sym_off = symtab_off
    while i < nsyms:
        entry = data[sym_off:sym_off+18]
        name_bytes = entry[:8]
        value = struct.unpack_from('<I', entry, 8)[0]
        sec_num = struct.unpack_from('<h', entry, 12)[0]
        stype = struct.unpack_from('<H', entry, 14)[0]
        sclass = entry[16]
        naux = entry[17]

        if name_bytes[:4] == b'\x00\x00\x00\x00':
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            end = data.index(b'\x00', strtab_base + str_off)
            sname = data[strtab_base + str_off:end].decode()
        else:
            sname = name_bytes.rstrip(b'\x00').decode()

        # External function in a .text section
        if sclass == 2 and sec_num > 0 and (stype & 0x20):
            sec = sections[sec_num - 1]
            if sec['name'] == '.text':
                func_bytes = data[sec['rawoff'] + value:
                                  sec['rawoff'] + sec['rawsize']]
                clean_name = sname.lstrip('_')
                funcs[clean_name] = func_bytes

        sym_off += 18 * (1 + naux)
        i += 1 + naux

    return funcs


def hex_diff(orig, recomp, max_lines=8):
    lines = []
    for i in range(0, max(len(orig), len(recomp)), 16):
        o = orig[i:i+16] if i < len(orig) else b''
        r = recomp[i:i+16] if i < len(recomp) else b''
        if o != r:
            lines.append(f"  +0x{i:04X}  orig:   {o.hex(' ')}")
            lines.append(f"  +0x{i:04X}  recomp: {r.hex(' ')}")
            if len(lines) >= max_lines:
                lines.append("  ...")
                break
    return '\n'.join(lines)


def diff_functions(src_path, obj_path, orig_dir, verified_dir):
    implements = parse_implements(src_path)
    if not implements:
        print("  (no @implements tags)")
        return

    obj_funcs = parse_coff_obj(obj_path)

    matched = 0
    diffed = 0
    skipped = 0

    for va, name in implements:
        orig_path = os.path.join(orig_dir, f"0x{va:08X}.bin")
        if not os.path.exists(orig_path):
            skipped += 1
            continue

        with open(orig_path, 'rb') as f:
            orig_bytes = f.read()

        if name not in obj_funcs:
            print(f"  SKIP  0x{va:08X} {name} -- not in .obj symbol table")
            skipped += 1
            continue

        recomp_bytes = obj_funcs[name]

        # Trim to original size (compiler may pad .text sections)
        recomp_trimmed = recomp_bytes[:len(orig_bytes)]

        if recomp_trimmed == orig_bytes:
            print(f"  MATCH 0x{va:08X} {name} ({len(orig_bytes)} bytes)")
            os.makedirs(verified_dir, exist_ok=True)
            out_path = os.path.join(verified_dir, f"0x{va:08X}.bin")
            with open(out_path, 'wb') as f:
                f.write(recomp_trimmed)
            matched += 1
        else:
            print(f"  DIFF  0x{va:08X} {name} "
                  f"(orig={len(orig_bytes)}, recomp={len(recomp_bytes)})")
            print(hex_diff(orig_bytes, recomp_trimmed))
            diffed += 1

    print(f"  result: {matched} match, {diffed} diff, {skipped} skip")


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <source.c> <compiled.obj> "
              f"<orig_dir/> <verified_dir/>")
        sys.exit(1)
    diff_functions(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
