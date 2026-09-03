"""Screen for the 'state-pointer argument is really absolute globals' class.

    python3 tools/screen_absglobals.py

Needs the capstone venv (.venv/bin/python3).  Reads build/match/report.csv and
build/match/orig/, so refresh the report first if it is stale.

A diff row qualifies when the ORIGINAL bytes never read an incoming argument
off the stack, yet the C body in the tree declares one or more parameters --
i.e. the port invented a state pointer the original does not have.  Ranked by
how many absolute memory operands the original uses, because that is the
number of globals the twin has to name.
"""
import csv, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32); md.skipdata = True

# A VA already byte-exact in the C++ or EXE workstream is NOT an open target,
# however its C-side row reads: 0x100414B0 sat at the top of this list while
# src/core/cpp/0x100414B0.cpp had already matched it exactly.  Those C rows are
# superseded twins that should be untagged, not re-solved.
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
    # EH screen (playbook): `push -1 / push <handler> / mov eax, fs:[0]` is a
    # C++/SEH exception frame, unreachable from C.  Those belong to the C++
    # workstream, not this class.
    if code[:2] == b'\x6a\xff':
        continue
    ins = list(md.disasm(code, 0))
    if not ins or len(ins) > 120:
        continue
    # Capstone prints small displacements in DECIMAL, so an [esp + 4] param
    # read is invisible to a hex-only pattern -- that bug once let every
    # function that does read its argument through the screen.
    #
    # The whole body is scanned, not just the head: 0x10058900 reads its one
    # argument forty instructions in, well past any prologue window, and
    # slipped through when only the first fifteen were inspected.  Scanning
    # everything also rejects functions that merely have esp-relative LOCALS,
    # which costs a few true members -- acceptable, because the class this
    # screens for touches the stack not at all.
    # x87 argument reads count too: a `float` parameter arrives as
    # `fld dword ptr [esp + N]`, which no integer-mov pattern sees.
    stackargs = sum(1 for i in ins
                    if (i.mnemonic in ('mov', 'movsx', 'movzx', 'lea')
                        and not i.op_str.startswith(('dword', 'word', 'byte'))
                        or i.mnemonic in ('fld', 'fild'))
                    and re.search(r'\[(?:esp|ebp)(?: \+ (?:0x[0-9a-f]+|\d+))?\]', i.op_str))
    absops = sum(1 for i in ins if re.search(r'\[0x[0-9a-f]{7,8}\]', i.op_str))
    if stackargs or absops < 2:
        continue
    f = r['file']
    if f not in srccache:
        fp = os.path.join(ROOT, f)
        srccache[f] = open(fp).read() if os.path.exists(fp) else ''
    m = re.search(r'^[A-Za-z_].*\b' + re.escape(r['name']) + r'\s*\(([^)]*)\)',
                  srccache[f], re.M)
    if m is None:
        continue
    # A register-argument function reads nothing off the stack BY DESIGN, so
    # it looks exactly like this class and is not it (0x10008AB0 BrPodOpen).
    if re.search(r'__fastcall|BR_THISCALL', m.group(0)):
        continue
    params = m.group(1).strip()
    if params in ('void', '') or params == '?':
        continue
    # The class is a param the C body USES (emitting [reg+disp]) where the
    # original addresses globals absolutely.  A merely unused parameter costs
    # nothing and is not this class, so require every declared name to appear
    # in the body.
    names = [re.sub(r'.*[^A-Za-z0-9_]', '', a.strip().rstrip('[]'))
             for a in params.split(',')]
    names = [n for n in names if n and n not in ('void',)]
    body = srccache[f][m.end():]
    depth, end = 0, len(body)
    for k, ch in enumerate(body):
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                end = k
                break
    body = body[:end]
    used = [n for n in names if re.search(r'\b' + re.escape(n) + r'\b', body)]
    if not used:
        continue
    out.append((absops, int(r['diffs']), r['va'], r['name'], f, params))

out.sort(key=lambda t: (-t[0], t[1]))
print('%-12s %5s %6s  %-28s %s' % ('va', 'abs', 'diffs', 'name', 'params'))
for absops, d, va, name, f, params in out[:40]:
    print('%-12s %5d %6d  %-28s %s' % (va, absops, d, name, params[:60]))
print('\n%d candidates' % len(out))
