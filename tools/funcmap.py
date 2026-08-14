"""Discover function boundaries in a PE's .text by recursive-descent disassembly.

Seeds come from: the PE entry point, exports, every direct CALL target, and every
relocated 32-bit word whose value lands in .text (i.e. a function pointer taken).
From each seed we follow intra-function control flow, then take function extents to
run from one start to the next, trimming 0xCC padding.

Emits config/functions.csv and reports .text coverage.
"""
import sys, os, struct, csv
sys.path.insert(0, os.path.dirname(__file__))
import pe as pelib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM

TERMINATORS = {'ret', 'retf', 'iret', 'jmp', 'ud2'}


def build(path):
    p = pelib.load(path)
    text, text_va = p.text()
    tlo, thi = text_va, text_va + len(text)

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    def in_text(va):
        return tlo <= va < thi

    def at(va):
        return text[va - text_va:]

    # ---------------- seeds -------------------------------------------
    seeds = set()
    entry = p.image_base + p.entry_rva
    if in_text(entry):
        seeds.add(entry)
    for va in p.exports:
        if in_text(va):
            seeds.add(va)

    # every relocated dword whose value points into .text = function pointer
    ptr_seeds = set()
    for rva in p.relocs:
        o = p.rva_to_off(rva)
        if o is None or o + 4 > len(p.data):
            continue
        v = struct.unpack('<I', p.data[o:o + 4])[0]
        if in_text(v):
            ptr_seeds.add(v)

    # sweep for direct CALLs (E8 rel32) across the whole section
    call_seeds = set()
    i = 0
    while i < len(text) - 5:
        if text[i] == 0xE8:
            rel = struct.unpack('<i', text[i + 1:i + 5])[0]
            tgt = text_va + i + 5 + rel
            if in_text(tgt):
                call_seeds.add(tgt)
            i += 5
        else:
            i += 1

    # Certain function starts: entry, exports, direct call targets.
    seeds |= call_seeds
    # Address-taken pointers are only accepted as *starts* later, once switch
    # tables are known -- switch entries are relocated too but point
    # mid-function. They are still used as traversal seeds so that code only
    # reachable through a function pointer still gets decoded (and its switch
    # tables discovered).

    # ---------------- recursive traversal ------------------------------
    # Walk code from every seed, following intra-procedural control flow.
    visited = set()          # addresses of decoded instruction starts
    after_term = set()       # addresses immediately following a terminator
    jump_tables = {}         # table va -> [targets]
    queue = list(seeds | ptr_seeds)

    while queue:
        va = queue.pop()
        while True:
            if va in visited or not in_text(va):
                break
            buf = at(va)
            insns = list(md.disasm(buf, va, count=1))
            if not insns:
                break
            ins = insns[0]
            visited.add(va)
            m = ins.mnemonic

            if m == 'call':
                op = ins.operands[0] if ins.operands else None
                if op is not None and op.type == CS_OP_IMM and in_text(op.imm):
                    if op.imm not in visited:
                        queue.append(op.imm)
                va = ins.address + ins.size
                continue

            if m.startswith('j'):
                op = ins.operands[0] if ins.operands else None
                if op is not None and op.type == CS_OP_IMM:
                    if in_text(op.imm) and op.imm not in visited:
                        queue.append(op.imm)
                    if m == 'jmp':
                        after_term.add(ins.address + ins.size)
                        break                      # tail of a block
                    va = ins.address + ins.size
                    continue
                # indirect jump: try to recover a switch table
                if op is not None and op.type == CS_OP_MEM and op.mem.scale == 4:
                    tbl = op.mem.disp & 0xFFFFFFFF
                    tgts = []
                    k = 0
                    while True:
                        v = p.u32(tbl + k * 4)
                        if v is None or not in_text(v):
                            break
                        # a real table entry is relocated
                        if (tbl + k * 4 - p.image_base) not in p.relocs:
                            break
                        tgts.append(v)
                        k += 1
                        if k > 512:
                            break
                    if tgts:
                        jump_tables[tbl] = tgts
                        for t in tgts:
                            if t not in visited:
                                queue.append(t)
                break

            if m in TERMINATORS:
                after_term.add(ins.address + ins.size)
                break
            va = ins.address + ins.size

    # ---------------- accept address-taken function pointers ------------
    # A relocated .text pointer is a function start only if it is not a switch
    # target and it sits after inter-function padding or a ret (96% of known
    # call targets satisfy this; switch-table targets usually do not).
    table_targets = set()
    for tgts in jump_tables.values():
        table_targets.update(tgts)
    accepted_ptrs = set()
    for v in ptr_seeds:
        if v in seeds or v in table_targets:
            continue
        prev = text[v - text_va - 1] if v > tlo else 0xCC
        if prev in (0xCC, 0x90) or v in after_term:
            accepted_ptrs.add(v)
    seeds |= accepted_ptrs

    # ---------------- function extents ---------------------------------
    starts = sorted(s for s in seeds if in_text(s))
    funcs = []
    for idx, s in enumerate(starts):
        end = starts[idx + 1] if idx + 1 < len(starts) else thi
        # trim trailing int3 / nop padding
        e = end
        while e > s and text[e - text_va - 1] in (0xCC, 0x90):
            e -= 1
        funcs.append((s, e - s))

    covered = len(visited)
    return p, text, text_va, funcs, jump_tables, visited, dict(
        seeds=len(seeds), calls=len(call_seeds), ptrs=len(ptr_seeds))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'orig/BRD3D.dll'
    out = sys.argv[2] if len(sys.argv) > 2 else 'config/functions.csv'
    p, text, text_va, funcs, jt, visited, stats = build(path)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['va', 'size', 'name'])
        for va, size in funcs:
            w.writerow(['0x%08X' % va, size, p.exports.get(va, '')])
    total = len(text)
    sizes = [s for _, s in funcs]
    print("%s" % path)
    print("  .text            %d bytes at %08X" % (total, text_va))
    print("  seeds            %d  (call targets %d, reloc'd fn pointers %d)"
          % (stats['seeds'], stats['calls'], stats['ptrs']))
    print("  functions        %d" % len(funcs))
    print("  switch tables    %d" % len(jt))
    print("  median size      %d bytes   max %d" % (sorted(sizes)[len(sizes) // 2], max(sizes)))
    print("  sum of extents   %d (%.1f%% of .text)" % (sum(sizes), 100.0 * sum(sizes) / total))
    print("  wrote %s" % out)


if __name__ == '__main__':
    main()
