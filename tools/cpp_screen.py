#!/usr/bin/env python3
"""Screen the refine residue for functions that are C++ methods in
disguise — route them to the C++ TU lane instead of burning C refine
climbs on shapes C cannot reach.

Traits read off the original bytes (each proven on landed TUs):
  this-ecx   ecx is READ (as a base or copied) before any write to it
             -> thiscall `this` (Tbl8900 family, 0x10008930..0x10008A90)
  eax-vcall  `mov eax,[ecx]` then `call [eax+N]` -- the slot-load-in-eax
             virtual call C's fastcall spelling cannot produce
             (phase-leave family, 0x1003D4A0 idiom entry)
  vtbl-cache a `mov reg,[this]` vtable load with 2+ `call [reg+N]`
             through it -- CSEd vtable across calls (0x100089F0/0x10008A70)
  ret-n      `ret imm` with no stdcall-style frame -- callee-cleaned
             stack args on a this-function (thiscall with stack args)

Usage: python3 tools/cpp_screen.py          # screen unmatched residue
       python3 tools/cpp_screen.py 0xVA...  # screen specific VAs
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
ORIG_DIR = os.path.join(ROOT, 'build', 'match', 'orig')

import capstone

MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True

ECX = capstone.x86.X86_REG_ECX


def screen(b):
    """Return the list of C++ traits the byte stream shows."""
    traits = []
    insns = list(MD.disasm(b, 0))
    if not insns:
        return traits
    # this-ecx: ecx read before written (skip functions that never touch it)
    for i in insns:
        rr, rw = i.regs_access()
        if ECX in rw and ECX not in rr:
            break
        if ECX in rr:
            traits.append('this-ecx')
            break
    # eax-vcall / vtbl-cache: track `mov r,[ecx-or-this]` then call [r+N]
    vtbl_regs = {}
    for i in insns:
        if i.mnemonic == 'mov' and len(i.operands) == 2:
            dst, src = i.operands
            if (dst.type == capstone.x86.X86_OP_REG
                    and src.type == capstone.x86.X86_OP_MEM
                    and src.mem.disp == 0 and src.mem.index == 0
                    and src.mem.base != 0):
                vtbl_regs.setdefault(dst.reg, 0)
        elif i.mnemonic == 'call' and len(i.operands) == 1 \
                and i.operands[0].type == capstone.x86.X86_OP_MEM \
                and i.operands[0].mem.base in vtbl_regs \
                and i.operands[0].mem.index == 0:
            vtbl_regs[i.operands[0].mem.base] += 1
            if i.operands[0].mem.base == capstone.x86.X86_REG_EAX:
                if 'eax-vcall' not in traits:
                    traits.append('eax-vcall')
    if any(n >= 2 for n in vtbl_regs.values()):
        traits.append('vtbl-cache')
    # ret-n on a this-using function
    last = insns[-1]
    if last.mnemonic == 'ret' and last.op_str and 'this-ecx' in traits:
        traits.append('ret-n')
    return traits


def main():
    if len(sys.argv) > 1:
        vas = [v.upper().replace('0X', '0x') for v in sys.argv[1:]]
    else:
        import ghidra_to_match as g
        matched = g.matched_vas_all_reports()
        with open(g.LEARNINGS_CSV) as f:
            vas = [r['va'] for r in csv.DictReader(f)
                   if r.get('divergence') and r['divergence'] not in ('', 'match')
                   and r['va'].lower() not in matched]
    strong, weak = 0, 0
    for va in sorted(vas):
        p = os.path.join(ORIG_DIR, va + '.bin')
        if not os.path.exists(p):
            continue
        b = open(p, 'rb').read()
        traits = screen(b)
        if not traits:
            continue
        # eax-vcall is unreachable from C, and thiscall-with-stack-args
        # (this + ret imm) beats the one-arg fastcall trick. Bare
        # this-ecx alone is __fastcall-representable in C (see the
        # thiscall-via-fastcall idiom) — only a WEAK hint.
        is_strong = ('eax-vcall' in traits
                     or ('this-ecx' in traits and 'ret-n' in traits))
        tag = 'CPP ' if is_strong else 'weak'
        if is_strong:
            strong += 1
        else:
            weak += 1
        print('%s %s  %5dB  %s' % (tag, va, len(b), ' '.join(traits)))
    print('%d strong (C++ TU lane) + %d weak (fastcall-representable) '
          'of %d screened' % (strong, weak, len(vas)))


if __name__ == '__main__':
    main()
