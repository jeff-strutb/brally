#!/usr/bin/env python3
"""Patch recompiled functions into the original DLL.

Takes the original DLL, a directory of matched .bin files (one per function,
named 0x<VA>.bin), and produces a patched DLL with those functions replaced.

Usage:
    python3 tools/pe_patch.py orig/BRD3D.dll build/match/verified/ build/BRD3D_patched.dll

Each .bin file must be EXACTLY the same size as the original function at that
VA.  If it differs in size, the function is skipped with a warning -- size
mismatches mean the function doesn't truly match and needs more work.

The patched DLL is byte-identical to the original except at the patched
function locations.  Headers, imports, relocations, and data sections are
untouched.
"""
import csv
import glob
import os
import struct
import sys


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

    patched = 0
    skipped = 0

    for bin_path in sorted(glob.glob(os.path.join(match_dir, '0x*.bin'))):
        basename = os.path.basename(bin_path)
        va = int(basename.replace('.bin', ''), 16)

        with open(bin_path, 'rb') as f:
            new_bytes = f.read()

        rva = va - image_base
        file_off = None
        for name, sec_rva, vsize, rawoff, rawsize in sections:
            if sec_rva <= rva < sec_rva + vsize:
                file_off = rawoff + (rva - sec_rva)
                break

        if file_off is None:
            print(f"  SKIP 0x{va:08X} -- VA not in any section")
            skipped += 1
            continue

        orig_size = func_sizes.get(va)
        if orig_size is not None and len(new_bytes) != orig_size:
            print(f"  SKIP 0x{va:08X} -- size mismatch: "
                  f"original {orig_size}, recompiled {len(new_bytes)}")
            skipped += 1
            continue

        data[file_off:file_off + len(new_bytes)] = new_bytes
        patched += 1

    with open(out_dll, 'wb') as f:
        f.write(data)

    print(f"patched {patched} functions, skipped {skipped}")
    print(f"wrote {out_dll}")


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(f"usage: {sys.argv[0]} <original.dll> <match_dir/> <output.dll> "
              f"[functions.csv]")
        sys.exit(1)
    funcs = sys.argv[4] if len(sys.argv) > 4 else None
    patch(sys.argv[1], sys.argv[2], sys.argv[3], funcs)
