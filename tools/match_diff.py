#!/usr/bin/env python3
"""Compare a compiled .obj's functions against the original DLL bytes.

Reads @implements tags from the source file to know which VAs to check,
extracts the corresponding code from the .obj via COFF symbol table parsing,
and diffs against the original bytes in the reference directory.

Relocations in the .obj (call/jmp targets, global addresses) are masked
during comparison since the linker resolves them — only instruction opcodes
and non-relocated operands are compared.

Usage:
    python3 tools/match_diff.py src/core/slice2_16.c build/match/obj/slice2_16.obj \
            build/match/orig/ build/match/verified/
"""
import csv as csv_mod
import os
import re
import struct
import sys


def load_shared_map():
    """d3d_va -> glide_va.

    GLIDE IS CANONICAL (rule 0). This map used to run the other way, and with
    it parse_implements() translated glide-tagged addresses INTO D3D space --
    which is how the whole corpus came to be keyed to the wrong binary by
    design rather than by accident. Both were inverted together; inverting one
    alone silently scores every function against the wrong bytes.
    """
    csv_path = os.path.join(os.path.dirname(os.path.dirname(__file__)),
                            'config', 'shared.csv')
    mapping = {}
    if not os.path.exists(csv_path):
        return mapping
    with open(csv_path) as f:
        for row in csv_mod.DictReader(f):
            gva = row.get('glide_va', '').strip()
            dva = row.get('d3d_va', '').strip()
            if gva and dva:
                mapping[int(dva, 16)] = int(gva, 16)
    return mapping


_shared_map = None


def get_shared_map():
    global _shared_map
    if _shared_map is None:
        _shared_map = load_shared_map()
    return _shared_map


def parse_implements(src_path):
    pattern = re.compile(
        r'@implements\s+0x([0-9A-Fa-f]+)\s+(\w+)\s+(\w+)')
    results = []
    seen = set()
    shared = get_shared_map()
    with open(src_path) as f:
        for line in f:
            m = pattern.search(line)
            if m:
                va = int(m.group(1), 16)
                build = m.group(2)
                name = m.group(3)
                # Glide is canonical: a d3d-tagged address is translated INTO
                # Glide space. This is the inverse of what it used to do.
                if build == 'd3d' and va in shared:
                    va = shared[va]
                if (va, name) not in seen:
                    results.append((va, name))
                    seen.add((va, name))
    return results


def undecorate(sname):
    """Strip MSVC calling-convention decoration to the bare C identifier.

    cdecl is _Name, stdcall is _Name@N, fastcall is @Name@N, where N is the
    argument byte count.  Only the leading sigil and the trailing @<digits>
    are decoration; what sits between them is the identifier an @implements
    tag names.  Keying on the raw symbol makes every non-cdecl function
    report not_in_obj, so it can never score a match no matter how correct
    the bytes are.
    """
    name = sname[1:] if sname.startswith('@') else sname
    name = name.lstrip('_')
    at = name.rfind('@')
    if at > 0 and name[at + 1:].isdigit():
        name = name[:at]
    return name


def parse_coff_obj(obj_path):
    """Parse a COFF .obj and return {name: (bytes, reloc_offsets)} for .text symbols.

    reloc_offsets is a set of byte offsets within the function where the linker
    will patch in an address — these bytes will always differ from the original
    and should be masked during comparison.
    """
    with open(obj_path, 'rb') as f:
        data = f.read()

    machine, nsec, ts, symtab_off, nsyms, opthdr_sz, chars = \
        struct.unpack_from('<HHIIIHH', data, 0)

    sections = []
    off = 20
    for i in range(nsec):
        name_bytes = data[off:off+8]
        vsize, vaddr, rawsize, rawoff = struct.unpack_from('<IIII', data, off+8)
        relocs_off = struct.unpack_from('<I', data, off+24)[0]
        nrelocs = struct.unpack_from('<H', data, off+32)[0]
        sec_chars = struct.unpack_from('<I', data, off+36)[0]

        strtab_base = symtab_off + nsyms * 18
        if name_bytes[:4] == b'\x00\x00\x00\x00':
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            end = data.index(b'\x00', strtab_base + str_off)
            sname = data[strtab_base + str_off:end].decode()
        else:
            sname = name_bytes.rstrip(b'\x00').decode()

        # Read relocation entries for this section
        relocs = set()
        for r in range(nrelocs):
            r_off = relocs_off + r * 10
            r_vaddr = struct.unpack_from('<I', data, r_off)[0]
            r_type = struct.unpack_from('<H', data, r_off + 8)[0]
            # IMAGE_REL_I386_DIR32 = 0x06, IMAGE_REL_I386_REL32 = 0x14
            if r_type in (0x06, 0x14):
                for b in range(4):
                    relocs.add(r_vaddr + b)

        sections.append({
            'name': sname, 'rawoff': rawoff, 'rawsize': rawsize,
            'chars': sec_chars, 'relocs': relocs
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

        if sclass == 2 and sec_num > 0 and (stype & 0x20):
            sec = sections[sec_num - 1]
            if sec['name'] == '.text':
                func_bytes = data[sec['rawoff'] + value:
                                  sec['rawoff'] + sec['rawsize']]
                clean_name = undecorate(sname)
                # Reloc offsets in sec['relocs'] are section-relative.
                # With /O2 COMDAT each function is its own section (value=0),
                # so they're also function-relative -- no adjustment needed.
                # With /Od all functions share one .text section so the
                # function starts at 'value' bytes in: subtract to make
                # function-relative.  Offsets that fall below value are from
                # a different function and become negative -- they never match
                # the [0, len) range used during comparison, so they're safe
                # to include in the set.
                relocs = {r - value for r in sec['relocs']}
                funcs[clean_name] = (func_bytes, relocs)

        sym_off += 18 * (1 + naux)
        i += 1 + naux

    return funcs


def hex_diff(orig, recomp, mask, max_lines=8):
    lines = []
    for i in range(0, max(len(orig), len(recomp)), 16):
        o = orig[i:i+16] if i < len(orig) else b''
        r = recomp[i:i+16] if i < len(recomp) else b''
        # Check if any non-masked byte differs
        differs = False
        for j in range(min(len(o), len(r))):
            if (i+j) not in mask and o[j] != r[j]:
                differs = True
                break
        if differs or len(o) != len(r):
            def fmt(bs, off):
                parts = []
                for j, b in enumerate(bs):
                    if (off+j) in mask:
                        parts.append('__')
                    else:
                        parts.append(f'{b:02x}')
                return ' '.join(parts)
            lines.append(f"  +0x{i:04X}  orig:   {fmt(o, i)}")
            lines.append(f"  +0x{i:04X}  recomp: {fmt(r, i)}")
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

        recomp_bytes, relocs = obj_funcs[name]
        recomp_trimmed = recomp_bytes[:len(orig_bytes)]

        # Compare with relocation masking
        real_diff = False
        for i in range(min(len(recomp_trimmed), len(orig_bytes))):
            if i in relocs:
                continue
            if recomp_trimmed[i] != orig_bytes[i]:
                real_diff = True
                break

        size_match = len(recomp_trimmed) >= len(orig_bytes)

        if not real_diff and size_match:
            print(f"  MATCH 0x{va:08X} {name} ({len(orig_bytes)} bytes)")
            os.makedirs(verified_dir, exist_ok=True)
            out_path = os.path.join(verified_dir, f"0x{va:08X}.bin")
            with open(out_path, 'wb') as f:
                f.write(recomp_trimmed)
            matched += 1
        else:
            nreloc = sum(1 for i in range(len(orig_bytes)) if i in relocs)
            ndiff = sum(1 for i in range(min(len(recomp_trimmed), len(orig_bytes)))
                       if i not in relocs and recomp_trimmed[i] != orig_bytes[i])
            print(f"  DIFF  0x{va:08X} {name} "
                  f"(orig={len(orig_bytes)}, recomp={len(recomp_bytes)}, "
                  f"{ndiff} real diffs, {nreloc} reloc)")
            print(hex_diff(orig_bytes, recomp_trimmed, relocs))
            diffed += 1

    print(f"  result: {matched} match, {diffed} diff, {skipped} skip")


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <source.c> <compiled.obj> "
              f"<orig_dir/> <verified_dir/>")
        sys.exit(1)
    diff_functions(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
