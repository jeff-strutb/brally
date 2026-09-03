"""Screen for thiscall originals whose C body is still cdecl.

    python3 tools/screen_thiscall.py

Needs the capstone venv (.venv/bin/python3).  Reads build/match/report.csv and
build/match/orig/, so refresh the report first if it is stale.

A diff row qualifies when the ORIGINAL captures `this` out of ecx before ecx is
written -- `mov <reg>, ecx` in the first few instructions -- and the C body in
the tree declares no register-argument convention.  Those are DEFINITIONS, and
defining a multi-argument thiscall callee is exact:

    void __fastcall F(T *pThis, int _edx_unused, <stack args>)

puts `this` in ecx and every other argument on the stack, and the callee simply
ignores edx.  The unreachable half of thiscall is the CALL side (a vtable send
through a function pointer), not this.  See the VC5-IDIOMS entry.

`ret <imm>` is reported alongside because it pins the stack-argument BYTE
count: imm/4 arguments after `this`.  A `ret` with no immediate and a
`mov reg,ecx` means one register argument and nothing on the stack, which is
BR_THISCALL1 and needs no dummy.
"""
import csv, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

# A VA already byte-exact in the C++ or EXE workstream is not an open target,
# however its C-side row reads.
done = set()
for name in ('report_cpp.csv', 'report_exe.csv'):
    fp = os.path.join(ROOT, 'build/match', name)
    if not os.path.exists(fp):
        continue
    for r in csv.DictReader(open(fp)):
        va = (r.get('va') or '').strip()
        if va and r.get('status') == 'match':
            done.add(va.lower())

rows = [r for r in csv.DictReader(open(os.path.join(ROOT, 'build/match/report.csv')))
        if r.get('status') == 'diff' and r.get('va')
        and r['va'].lower() not in done]

srccache = {}
out = []
for r in rows:
    p = os.path.join(ROOT, 'build/match/orig', r['va'] + '.bin')
    if not os.path.exists(p):
        continue
    code = open(p, 'rb').read()
    if code[:2] == b'\x6a\xff':          # C++/SEH frame -- other workstream
        continue
    ins = list(md.disasm(code, 0))
    if not ins:
        continue
    # `this` capture: a read of ecx before anything writes it.  Only the head
    # is inspected; a later ecx read is ordinary register traffic.
    capture = None
    for i in ins[:8]:
        if i.mnemonic == 'mov' and re.fullmatch(r'e[a-d]x|e[sd]i|ebp, ecx',
                                                i.op_str.replace(' ', '')
                                                if False else i.op_str):
            pass
        if i.mnemonic in ('mov', 'lea') and re.search(r',\s*ecx\b', i.op_str):
            capture = i
            break
        if re.match(r'(mov|xor|pop|lea|add|sub)\b', i.mnemonic) and \
           re.match(r'ecx\b', i.op_str):
            break                         # ecx written first: not a this-capture
    if capture is None:
        continue
    retimm = ''
    for i in ins:
        if i.mnemonic == 'ret' and i.op_str:
            retimm = i.op_str
            break

    f = r['file']
    if f not in srccache:
        fp = os.path.join(ROOT, f)
        srccache[f] = open(fp).read() if os.path.exists(fp) else ''
    m = re.search(r'^[A-Za-z_].*\b' + re.escape(r['name']) + r'\s*\(([^)]*)\)',
                  srccache[f], re.M)
    if m is None:
        continue
    if re.search(r'__fastcall|BR_THISCALL', m.group(0)):
        continue                          # already spelled as a register call
    out.append((int(r['diffs']), r['va'], r['name'], retimm or '-',
                m.group(1).strip()))

out.sort()
print('%-12s %6s %6s  %-30s %s' % ('va', 'diffs', 'ret', 'name', 'params'))
for d, va, name, retimm, params in out[:40]:
    print('%-12s %6d %6s  %-30s %s' % (va, d, retimm, name, params[:52]))
print('\n%d candidates' % len(out))
