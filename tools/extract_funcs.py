#!/usr/bin/env python3
"""Extract original function bytes from a PE DLL for matching comparison.

Given a DLL and the functions CSV, extracts each function's raw bytes from the
.text section and writes them to a directory.  A matching build compiles C to
.obj, then this tool's output is what the .obj's code section gets diffed
against.

Usage:
    python3 tools/extract_funcs.py orig/BRD3D.dll config/functions.csv build/match/orig/
    python3 tools/extract_funcs.py orig/BRGlide.dll config/functions_glide.csv build/match/orig_glide/

Each function is written as:
    <outdir>/0x<VA>.bin    -- raw bytes
    <outdir>/0x<VA>.dis    -- disassembly (if capstone available)
"""
import csv
import os
import struct
import sys


def read_pe_sections(path):
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
        return image_base, sections, f.name


def va_to_fileoff(va, image_base, sections):
    rva = va - image_base
    for name, sec_rva, vsize, rawoff, rawsize in sections:
        if sec_rva <= rva < sec_rva + vsize:
            return rawoff + (rva - sec_rva)
    return None


def extract(dll_path, csv_path, out_dir):
    image_base, sections, _ = read_pe_sections(dll_path)
    os.makedirs(out_dir, exist_ok=True)

    with open(dll_path, 'rb') as dll:
        data = dll.read()

    with open(csv_path) as f:
        reader = csv.DictReader(f)
        count = 0
        for row in reader:
            va = int(row['va'], 16)
            size = int(row['size'])
            if size == 0:
                continue
            off = va_to_fileoff(va, image_base, sections)
            if off is None:
                print(f"  SKIP 0x{va:08X} -- not in any section", file=sys.stderr)
                continue
            func_bytes = data[off:off + size]
            out_path = os.path.join(out_dir, f"0x{va:08X}.bin")
            with open(out_path, 'wb') as out:
                out.write(func_bytes)
            count += 1
        print(f"extracted {count} functions to {out_dir}")


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <dll> <functions.csv> <outdir>")
        sys.exit(1)
    extract(sys.argv[1], sys.argv[2], sys.argv[3])
