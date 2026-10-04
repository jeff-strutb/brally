"""Look inside IDO's register allocator for one function, and force it.

The global register allocator in IDO 5.3's uopt (globalcolor) decides which
live range gets which register.  When a function is byte-exact except for
register names, or is one saved register short, the decision that differs
is invisible in the object.  This tool compiles a source with an instrumented
uopt (build it with n64/tools/build_ido_trace.sh) and

    trace   prints every allocator decision for the function: the live range
            (web), its estimated saving, its use divisor, the register it got
            and the per-register costs
    force   recompiles with chosen webs forced to chosen registers and grades
            the function against the ROM
    sweep   tries every saved register (and the no-register path) for every
            web, one at a time, and lists the forces that lower the diff

A force answers "if this live range had that register, would the function
match?".  It isolates the cause; it is never itself a decompilation (the
original compiler had no such control).  Take what it shows back to source.

    .venv/bin/python n64/tools/n64alloc.py trace 0x80243260
    .venv/bin/python n64/tools/n64alloc.py force 0x80243260 p1:w597=c22 --diff
    .venv/bin/python n64/tools/n64alloc.py sweep 0x80243260
    .venv/bin/python n64/tools/n64alloc.py sweep 0x80243260 --file my_draft.c

Force keys: p1:wN=cK (phase one, web N, colour K) or p1:wN=s (split path);
phase two is p2.  Colours: c1-c5 v0 v1 a0 a1 a2, c6 a3, c7-c12 t0-t5,
c14-c22 s0-s8, c23 ra.  Web numbers are run-local: take them from a trace of
the same source.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64build as B  # noqa: E402

TRACE_CC = os.environ.get('TGR_TRACE_CC', os.path.join(B.ROOT, 'build/ext/instr/out/cc'))
REG = {1: 'v0', 2: 'v1', 3: 'a0', 4: 'a1', 5: 'a2', 6: 'a3', 23: 'ra'}
REG.update({7 + i: 't%d' % i for i in range(6)})
REG.update({14 + i: 's%d' % i for i in range(9)})
SAVED = list(range(14, 23))
REC = re.compile(r'\[CDX\] (\w+) (.*)')


def regname(c):
    return REG.get(c, 'c%d' % c)


def source_for(va, path=None):
    if path:
        src = open(path).read()
        for v, n, _ in B.tags_in(src):
            if v == va:
                return path, n
        sys.exit('%s has no @implements tag for %08X' % (path, va))
    for f in B.all_sources():
        for v, n, _ in B.tags_in(open(f).read()):
            if v == va:
                return f, n
    sys.exit('no source tags %08X' % va)


def compile_traced(path, env_extra):
    """-> (Obj or None, cdx log text)"""
    src = open(path, encoding='utf-8', errors='replace').read()
    fd, o = tempfile.mkstemp(suffix='.o')
    os.close(fd)
    fd, log = tempfile.mkstemp(suffix='.cdx')
    os.close(fd)
    env = dict(os.environ, CDX_OUT=log, **env_extra)
    cmd = [TRACE_CC] + B.BASE_FLAGS + B.cflags_for(src) + ['-o', o, path]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    text = open(log).read()
    os.unlink(log)
    if p.returncode:
        if os.path.exists(o):
            os.unlink(o)
        return None, text + p.stderr
    obj = B.Obj(o)
    os.unlink(o)
    return obj, text + p.stderr


class Grader:
    def __init__(self, va, path, aligned=True):
        self.va, self.path, self.aligned = va, path, aligned
        self.f, self.name = source_for(va, path)
        self.rom, self.fmap, self.syms = B.Rom(), B.function_map(), B.load_symbols()
        self.fnvas = {n: v for v, n, _ in B.tags_in(open(self.f).read())}
        obj, _ = compile_traced(self.f, {})
        if obj is None:
            sys.exit('compile failed')
        names = [p[0] for p in B.carve(obj)]
        self.proc = names.index(self.name)       # globalcolor runs once per procedure, in order

    def grade(self, env_extra, want_diff=False):
        env = dict(env_extra, CDX_PROC=str(self.proc))
        obj, log = compile_traced(self.f, env)
        if obj is None:
            return None, log, None
        have = {p[0]: p for p in B.carve(obj)}
        _, s, e = have[self.name]
        st, nd, notes, ours, theirs = B.grade(obj, self.rom, self.name, s, e, self.va,
                                              self.fmap[self.va], self.syms, self.fnvas)
        if want_diff:
            B.print_diff(self.va, ours, theirs)
        if self.aligned and st != 'EXACT':
            nd = aligned_diff(ours, theirs)
        return nd, log, st


def aligned_diff(ours, theirs):
    """Instructions that differ after aligning the two bodies, so an inserted
    or deleted instruction counts once instead of shifting everything after
    it (the positional count the T4 grade reports)."""
    import difflib
    sm = difflib.SequenceMatcher(None, list(ours), list(theirs), autojunk=False)
    return sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != 'equal')


def objdump_text(name, va, words):
    """GNU-objdump-shaped text of a linked body (what the workbench's
    *-dumps commands read); offsets are function-relative."""
    import struct
    from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_BIG_ENDIAN
    md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
    out = ['%08x <%s>:' % (0, name)]
    for k, w in enumerate(words):
        txt = '.word 0x%08x' % w
        for i in md.disasm(struct.pack('>I', w), va + 4 * k):
            txt = '%s %s' % (i.mnemonic, i.op_str.replace(', ', ','))
        out.append('%4x:\t%08x \t%s' % (4 * k, w, txt))
    return '\n'.join(out) + '\n'


def cmd_diagnose(g, a):
    """Write ROM and candidate dumps and run the workbench's diagnosis on them
    (uopt webs vs ugen temp ring, first divergence, hunks)."""
    obj, log = compile_traced(g.f, {})
    have = {p[0]: p for p in B.carve(obj)}
    _, s, e = have[g.name]
    st, nd, notes, ours, theirs = B.grade(obj, g.rom, g.name, s, e, g.va, g.fmap[g.va], g.syms, g.fnvas)
    os.makedirs(a.out, exist_ok=True)
    t = os.path.join(a.out, '%08X.target.objdump' % g.va)
    c = os.path.join(a.out, '%08X.candidate.objdump' % g.va)
    open(t, 'w').write(objdump_text(g.name, g.va, theirs))
    open(c, 'w').write(objdump_text(g.name, g.va, ours))
    wb = os.path.join(B.ROOT, 'build/ext/wbvenv/bin/decomp-workbench')
    cmd = [wb, 'diagnose-dumps', t, c, '--color', 'never', '--pager', 'never']
    if a.trace:
        nd2, tlog, _ = g.grade({'CDX_LOG': '1', 'CDX_DETAIL_WEB': 'all'})
        tp = os.path.join(a.out, '%08X.cdx.log' % g.va)
        open(tp, 'w').write(tlog)
        cmd += ['--trace', tp, '--trace-proc', str(g.proc)]
    subprocess.run(cmd)


def decisions(log):
    out = []
    for line in log.splitlines():
        m = REC.search(line)
        if m and m.group(1) in ('p1dec', 'p2dec'):
            d = dict(kv.split('=', 1) for kv in m.group(2).split() if '=' in kv)
            out.append(d)
    return out


def cmd_trace(g, a):
    nd, log, st = g.grade({'CDX_LOG': '1'})
    print('%08X %s  proc %d  %s %s' % (g.va, g.name, g.proc, st, nd))
    rows = decisions(log)
    rows.sort(key=lambda d: (d['phase'], -float(d['save'])))
    print('%-5s %5s %10s %5s %-8s %-6s %s' % ('phase', 'web', 'save', 'nocs', 'decision', 'reg', 'regsleft'))
    for d in rows:
        c = int(d.get('bestcolor', '-1'))
        if a.saved and not (14 <= c <= 22) and d['decision'] == 'color':
            continue
        print('%-5s %5s %10.3f %5s %-8s %-6s %s' % (d['phase'], d['web'], float(d['save']), d['nocs'],
                                                  d['decision'], regname(c) if c > 0 else '-',
                                                  d.get('regsleft', '')))
    for w in a.occ or []:
        print('occurrences of web %s (uses defs x block weight - charges = contrib):' % w)
        for line in log.splitlines():
            m = REC.search(line)
            if m and m.group(1) == 'saveocc':
                d = dict(kv.split('=', 1) for kv in m.group(2).split() if '=' in kv)
                if d['sym'] == w:
                    print('  bb %s  uses %s defs %s  weight %s  nl %s  -> %s' % (
                        d['bb'], d['uses'], d['defs'], d['weight'], d['nl'], d['contrib']))


def cmd_force(g, a):
    base, _, st0 = g.grade({})
    nd, log, st = g.grade({'CDX_FORCE': a.keys}, want_diff=a.diff)
    declined = [l for l in log.splitlines() if 'force_declined' in l]
    print('%08X %s: baseline %s %s -> forced %s %s  (%s)' % (g.va, g.name, st0, base, st, nd, a.keys))
    for l in declined:
        print('  ' + l.strip())


def sweep_once(g, a, fixed):
    """-> (baseline diff, [(diff, key)] better single forces on top of fixed)"""
    env = {'CDX_LOG': '1'}
    if fixed:
        env['CDX_FORCE'] = ','.join(fixed)
    base, log, st = g.grade(env)
    rows = [d for d in decisions(log) if a.phase in (None, d['phase'])]
    colors = [int(c) for c in a.colors.split(',')] if a.colors else SAVED
    taken = {k.split('=')[0] for k in fixed}
    keys = []
    for d in rows:
        w = '%s:w%s' % (d['phase'], d['web'])
        if w in taken:
            continue
        cur = int(d.get('bestcolor', '-1')) if d['decision'] == 'color' else None
        for c in colors:
            if c != cur:
                keys.append('%s=c%d' % (w, c))
        if d['decision'] == 'color':
            keys.append('%s=s' % w)
    keys = sorted(set(keys))

    def one(k):
        nd, lg, st_ = g.grade({'CDX_FORCE': ','.join(fixed + [k])})
        return k, nd, 'force_declined' in lg

    better = []
    with ThreadPoolExecutor(a.jobs) as ex:
        for k, nd, dec in ex.map(one, keys):
            if nd is not None and not dec and nd < base:
                better.append((nd, k))
    better.sort()
    return base, st, len(rows), len(keys), better


def cmd_sweep(g, a):
    fixed = []
    for rnd in range(a.greedy or 1):
        base, st, nw, nk, better = sweep_once(g, a, fixed)
        print('%08X %s: %s %s with [%s]  (%d webs, %d forces)' % (g.va, g.name, st, base,
                                                                ','.join(fixed), nw, nk))
        if not a.greedy:
            for nd, k in better[:a.top]:
                print('  %4d  %s' % (nd, k))
        if not better:
            print('  no single force lowers the diff')
            break
        if a.greedy:
            nd, k = better[0]
            print('  + %s -> %d' % (k, nd))
            fixed.append(k)
            if nd == 0:
                print('EXACT with %s' % ','.join(fixed))
                break


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('trace', 'force', 'sweep', 'diagnose'):
        p = sub.add_parser(name)
        p.add_argument('va')
        if name == 'diagnose':
            p.add_argument('--out', default=os.path.join(B.OUT, 'alloc'))
            p.add_argument('--trace', action='store_true', help='join the allocator trace')
        p.add_argument('--file', help='a draft to compile instead of the tagged tree source')
        p.add_argument('--positional', action='store_true',
                       help='count differing words by position (the T4 grade) instead of aligned')
        if name == 'trace':
            p.add_argument('--saved', action='store_true', help='only webs in saved registers or not coloured')
            p.add_argument('--occ', action='append', metavar='WEB',
                           help='list a web\'s occurrences and what each adds to its saving')
        if name == 'force':
            p.add_argument('keys', help='p1:wN=cK[,p2:wM=s...]')
            p.add_argument('--diff', action='store_true')
        if name == 'sweep':
            p.add_argument('--phase', choices=('p1', 'p2'))
            p.add_argument('--colors', help='colour numbers to try (default the saved registers c14-c22)')
            p.add_argument('--jobs', type=int, default=12)
            p.add_argument('--top', type=int, default=25)
            p.add_argument('--greedy', type=int, metavar='N',
                           help='keep the best force and sweep again, up to N rounds')
    a = ap.parse_args()
    if not os.path.exists(TRACE_CC):
        sys.exit('no instrumented compiler at %s: run n64/tools/build_ido_trace.sh' % TRACE_CC)
    g = Grader(int(a.va, 16), a.file, aligned=not a.positional)
    {'trace': cmd_trace, 'force': cmd_force, 'sweep': cmd_sweep, 'diagnose': cmd_diagnose}[a.cmd](g, a)


if __name__ == '__main__':
    main()
