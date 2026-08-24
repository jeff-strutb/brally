#!/usr/bin/env python3
"""Classify every unmatched pipeline row by what it actually is.

Reads build/ghidra_learnings.csv + build/match/orig/<va>.bin, writes
build/pipeline_triage.csv with one class per row:

  candidate      plain-int code -- the real hand-matching target
  float-x87      >25% x87 instructions -- the scheduling-wall workstream
  cxx-eh-frame   starts `push -1` (fs:[0] EH frame) -- needs .cpp units
  tiny-junk      <=16 bytes -- toolchain-generated, link stage reproduces
  thunk-stub     jmp [IAT] / jmp rel32 -- same
  compile-error  the wrapper does not compile it yet -- tooling work

Needs the capstone venv: .venv/bin/python tools/pipeline_triage.py
"""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

def classify(row):
    va = row['va']; size = int(row['orig_size'] or 0)
    p = os.path.join(ROOT, 'build', 'match', 'orig', va + '.bin')
    if not os.path.exists(p):
        return 'no-orig-bytes'
    b = open(p, 'rb').read()
    if row['result'] == 'ERROR':
        return 'compile-error'
    if b[:2] == b'\x6a\xff':
        return 'cxx-eh-frame'
    if len(b) <= 6 and (b[:2] == b'\xff\x25' or b[:1] == b'\xe9'):
        return 'thunk-stub'
    if len(b) <= 16:
        return 'tiny-junk'
    insns = list(md.disasm(bytes(b), 0)); n = len(insns) or 1
    x87 = sum(1 for i in insns if i.mnemonic.startswith('f'))
    return 'float-x87' if x87 / n > 0.25 else 'candidate'

def main():
    rows = list(csv.DictReader(open(os.path.join(ROOT, 'build', 'ghidra_learnings.csv'))))
    outp = os.path.join(ROOT, 'build', 'pipeline_triage.csv')
    out = csv.writer(open(outp, 'w'))
    out.writerow(['va', 'orig_size', 'result', 'class'])
    tally = {}
    for r in rows:
        k = classify(r)
        out.writerow([r['va'], r['orig_size'], r['result'], k])
        c, by = tally.get(k, (0, 0))
        tally[k] = (c + 1, by + int(r['orig_size'] or 0))
    print(f'{"class":16s} {"fns":>5s} {"bytes":>8s}')
    for k, (c, by) in sorted(tally.items(), key=lambda x: -x[1][1]):
        print(f'{k:16s} {c:5d} {by:8d}')
    print('wrote', outp)

if __name__ == '__main__':
    main()
