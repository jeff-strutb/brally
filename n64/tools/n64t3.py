"""T3 for Top Gear Rally: the live oracle, the whole-image run, and the gate.

    .venv/bin/python n64/tools/n64t3.py --live 0x8022439C [VA ...]   # A5
    .venv/bin/python n64/tools/n64t3.py --image [--with VA ...]      # A7
    .venv/bin/python n64/tools/n64t3.py --qualify 0x8022439C          # the gate

Same standard as the PC lane (CLAUDE.md rule 12).  T3 means certified
complete and behaving exactly like the original, not byte-exact.

A5 -- LIVE ORACLE.  The original ROM runs headless (n64box.py) on every script
in n64/tools/n64box_scripts.  Each time the game calls a function under test,
the whole machine is snapshotted; the ORIGINAL body runs to its return; the
machine is put back and the CANDIDATE body (compiled from n64/src, linked into
the annex) runs from the identical state to the same return.  Memory (all of
it except the dead stack below the caller), the return and callee-saved
registers and every side effect the box records must agree.  The original's
result is kept and the run goes on, so every later call sees real game state.
A call that blocks on a message queue is not compared (counted apart).
Verdicts: EQUIVALENT (compared >= 1, no divergence), DIVERGENT, UNCOVERED.
They land in n64/config/t3_live.csv with the source's hash.

A7 -- WHOLE IMAGE.  Every T3 body placed at once (n64image.py --t3, plus
--with for bodies being qualified), every script run for its full length, and
the log -- each display list with the data it uses, each audio buffer, the
game's RAM at every retrace -- compared with the original's.  IDENTICAL or the
first frame that differs.  Recorded in n64/config/whole_image.csv.

THE GATE (--qualify).  Gate 0: a WHAT IT DOES comment, an @t3 tag, no
unfinished markers.  Gate A5: EQUIVALENT for the current source.  Gate A7:
the newest whole-image run includes this body at its current source and is
IDENTICAL.  Gate B: two `@t4-pass` lines in the source from
n64/tools/n64permute.py (>= 10 compiles each), the later one moving nothing --
the byte grind was tried and stalled.  An unreached function never passes.
"""
import argparse
import copy
import csv
import datetime
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
N64 = os.path.join(ROOT, 'n64')
sys.path.insert(0, os.path.join(N64, 'tools'))
import n64build as B  # noqa: E402
import n64link as L  # noqa: E402
import n64box as NB  # noqa: E402
import n64image as IMG  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn import mips_const as M  # noqa: E402

SCRIPTS = os.path.join(N64, 'tools/n64box_scripts')
LIVE = os.path.join(N64, 'config/t3_live.csv')
WHOLE = os.path.join(N64, 'config/whole_image.csv')
FRAMES = {'attract.txt': 2400}
TEST_CODE, TEST_DATA = 0x80700000, 0x80780000
CODE_LO, CODE_HI = 0x80200000, 0x8026FAB0
DEAD_STACK = 0x4000
MAX_CALLS = 150


def scripts():
    return sorted(f for f in os.listdir(SCRIPTS) if f.endswith('.txt'))


def source_of(va):
    """-> (path, name, source sha) of the @implements tag for va."""
    for f in B.all_sources():
        src = open(f).read()
        for v, n, _ in B.tags_in(src):
            if v == va:
                return f, n, hashlib.sha1(function_text(src, n).encode()).hexdigest()[:12]
    return None, None, None


def function_text(src, name):
    m = re.search(r'^[^\n;{}]*\b%s\s*\([^;{]*\)\s*\{' % re.escape(name), src, re.M)
    if not m:
        return ''
    depth, i = 0, m.end() - 1
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[m.start():i + 1]
        i += 1
    return src[m.start():]


def return_kind(va):
    f, name, sha = source_of(va)
    text = function_text(open(f).read(), name)
    head = text.split('(')[0]
    if re.search(r'\bvoid\s+\w+\s*$', head) and '*' not in head:
        return 'void'
    if re.search(r'\bdouble\b', head) and '*' not in head:
        return 'double'
    if re.search(r'\bfloat\b', head) and '*' not in head:
        return 'float'
    if re.search(r'\blong\s+long\b', head) and '*' not in head:
        return 'llong'
    return 'int'


def link_candidate(va, code_va, data_va):
    f, name, sha = source_of(va)
    if f is None:
        raise L.LinkError('no source tags %08X' % va)
    obj, err = B.compile_c(f)
    if obj is None:
        raise L.LinkError('compile error: %s' % err.strip().split('\n')[0])
    fnvas = {n: v for v, n, _ in B.tags_in(open(f).read())}
    code, data = L.link_function(obj, name, code_va, data_va, fnvas, B.load_symbols())
    return code, data, sha


# ---------------------------------------------------------------- sandbox
SENTINEL = 0x80000400          # return address the sandboxes stop at
BUDGET = 20000000              # instructions one call may take
DEAD_LO = 0x80000400


class Sandbox:
    """A second CPU that runs one call from a copy of the live machine.

    The live run is only ever READ: at each call under test its memory and
    registers are copied into two sandboxes, the original body runs in one
    and the candidate in the other, each to a sentinel return address, and the
    two end states are compared.  Library code runs for real in the sandbox;
    the few OS services a game function may touch without blocking are
    modelled; anything else makes the call uncomparable (counted apart)."""

    def __init__(self):
        from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS64, UC_MODE_BIG_ENDIAN
        self.uc = Uc(UC_ARCH_MIPS, UC_MODE_MIPS64 | UC_MODE_BIG_ENDIAN)
        self.uc.ctl_set_cpu_model(M.UC_CPU_MIPS64_R4000)
        self.uc.mem_map(0, NB.RDRAM)
        self.stop = None
        self.count = 0
        for va, (name, fn) in NB.Box.__dict__.get('_hle_names', {}).items():
            pass

    def hle(self, box):
        """Hook every modelled library entry the live box knows."""
        for va, (name, fn) in box.hle.items():
            self.uc.hook_add(UC_HOOK_CODE, self.on_os, begin=NB.sx(va), end=NB.sx(va),
                             user_data=name)

    def reg(self, i):
        return self.uc.reg_read(NB.GPR[i]) & 0xffffffff

    def ret(self, v0=None, v1=None):
        if v0 is not None:
            self.uc.reg_write(NB.GPR[2], NB.sx(v0))
        if v1 is not None:
            self.uc.reg_write(NB.GPR[3], NB.sx(v1))
        self.uc.reg_write(M.UC_MIPS_REG_PC, NB.sx(self.reg(31)))

    def on_os(self, uc, addr, size, name):
        if name in ('osSyncPrintf', 'osInvalDCache', 'osWritebackDCacheAll'):
            self.ret()
        elif name == 'osGetCount':
            self.ret(self.count & 0xffffffff)
        elif name.startswith('__') and ('_to_' in name):
            self.convert(name)
        else:
            self.stop = 'calls %s' % name
            uc.emu_stop()

    def convert(self, name):
        import struct
        def f32(r):
            return struct.unpack('>f', struct.pack('>I', self.uc.reg_read(NB.FPR[r]) & 0xffffffff))[0]
        def f64(r):
            lo = self.uc.reg_read(NB.FPR[r]) & 0xffffffff
            hi = self.uc.reg_read(NB.FPR[r + 1]) & 0xffffffff
            return struct.unpack('>d', struct.pack('>II', hi, lo))[0]
        if name.endswith('_to_ll') or name.endswith('_to_ull'):
            x = f64(12) if name.startswith('__d') else f32(12)
            v = (int(x) if x == x else 0) & 0xffffffffffffffff
            self.ret(v >> 32, v & 0xffffffff)
            return
        v = (self.reg(4) << 32) | self.reg(5)
        if name.startswith('__ll') and v & (1 << 63):
            v -= 1 << 64
        if name.endswith('_d'):
            hi, lo = struct.unpack('>II', struct.pack('>d', float(v)))
            self.uc.reg_write(NB.FPR[0], lo)
            self.uc.reg_write(NB.FPR[1], hi)
        else:
            self.uc.reg_write(NB.FPR[0], struct.unpack('>I', struct.pack('>f', float(v)))[0])
        self.ret()

    def on_end(self, uc, addr, size, data):
        self.stop = 'returned'
        uc.emu_stop()

    def run(self, ram, regs, pc, count):
        uc = self.uc
        uc.mem_write(0, ram)
        for r, v in zip(NB.GPR[1:], regs['gpr'][1:]):
            uc.reg_write(r, v)
        for r, v in zip(NB.FPR, regs['fpr']):
            uc.reg_write(r, v)
        uc.reg_write(M.UC_MIPS_REG_HI, regs['hi'])
        uc.reg_write(M.UC_MIPS_REG_LO, regs['lo'])
        uc.reg_write(M.UC_MIPS_REG_FCSR, regs['fcsr'])
        uc.reg_write(NB.GPR[31], NB.sx(SENTINEL))
        self.stop, self.count = None, count
        try:
            uc.emu_start(NB.sx(pc), NB.sx(SENTINEL), count=BUDGET)
        except Exception as e:
            return 'faulted (%s at %08X)' % (e, uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff)
        if self.stop:
            return self.stop
        if uc.reg_read(M.UC_MIPS_REG_PC) & 0xffffffff == SENTINEL:
            return 'returned'
        return 'did not return within the budget'

    def state(self):
        uc = self.uc
        return (bytes(uc.mem_read(0, NB.RDRAM)),
                dict(gpr=[uc.reg_read(r) for r in NB.GPR], fpr=[uc.reg_read(r) for r in NB.FPR]))


def regs_of(uc):
    return dict(gpr=[uc.reg_read(r) for r in NB.GPR], fpr=[uc.reg_read(r) for r in NB.FPR],
                hi=uc.reg_read(M.UC_MIPS_REG_HI), lo=uc.reg_read(M.UC_MIPS_REG_LO),
                fcsr=uc.reg_read(M.UC_MIPS_REG_FCSR))


def compare(a, b, sp, kind='int'):
    """-> None if the two end states agree, else a description."""
    ram_a, regs_a = a
    ram_b, regs_b = b
    if ram_a != ram_b:
        dlo, dhi = (sp - DEAD_STACK) & 0x1FFFFFFF, sp & 0x1FFFFFFF
        # compare page by page, skipping the dead stack below the caller
        for off in range(0, len(ram_a), 0x1000):
            x, y = ram_a[off:off + 0x1000], ram_b[off:off + 0x1000]
            if x == y:
                continue
            for i in range(len(x)):
                pa = off + i
                if x[i] != y[i] and not (dlo <= pa < dhi) and pa >= (DEAD_LO & 0x1FFFFFFF):
                    return 'memory %08X: original %02X, candidate %02X' % (0x80000000 | pa, x[i], y[i])
    ret_gpr = {'int': (2,), 'llong': (2, 3)}.get(kind, ())
    ret_fpr = {'float': (0,), 'double': (0, 1)}.get(kind, ())
    for r in ret_gpr + (16, 17, 18, 19, 20, 21, 22, 23, 29, 30):
        if regs_a['gpr'][r] & 0xffffffff != regs_b['gpr'][r] & 0xffffffff:
            return 'register $%d: original %08X, candidate %08X' % (
                r, regs_a['gpr'][r] & 0xffffffff, regs_b['gpr'][r] & 0xffffffff)
    for r in ret_fpr + (20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31):
        if regs_a['fpr'][r] & 0xffffffff != regs_b['fpr'][r] & 0xffffffff:
            return 'register $f%d: original %08X, candidate %08X' % (
                r, regs_a['fpr'][r] & 0xffffffff, regs_b['fpr'][r] & 0xffffffff)
    return None


class Lockstep:
    """Compares candidates with the original at every real call of a live run."""

    def __init__(self, box, targets):
        self.box = box
        self.t = {}
        self.a, self.b = Sandbox(), Sandbox()
        self.a.hle(box)
        self.b.hle(box)
        code = TEST_CODE
        for va, (cblob, dblocks, sha) in targets.items():
            self.t[va] = dict(code=code, sha=sha, compared=0, divergent=0, blocked=0,
                              first=None, calls=0, kind=return_kind(va))
            box.uc.mem_write(code & 0x1FFFFFFF, cblob)
            for dva, blob in dblocks:
                box.uc.mem_write(dva & 0x1FFFFFFF, blob)
            box.uc.hook_add(UC_HOOK_CODE, self.on_entry, begin=NB.sx(va), end=NB.sx(va),
                            user_data=va)
            code += (len(cblob) + 15) & ~15

    def on_entry(self, uc, addr, size, va):
        t = self.t[va]
        t['calls'] += 1
        if t['compared'] + t['blocked'] >= MAX_CALLS and t['calls'] % 97:
            return
        if t['compared'] >= MAX_CALLS * 4:
            return
        ram = bytes(uc.mem_read(0, NB.RDRAM))          # reads only
        regs = regs_of(uc)
        sp = uc.reg_read(NB.GPR[29]) & 0xffffffff
        ra_ = self.a.run(ram, regs, va, self.box.count)
        if ra_ != 'returned':
            t['blocked'] += 1                          # the original cannot be compared
            return
        sa = self.a.state()
        rb_ = self.b.run(ram, regs, t['code'], self.box.count)
        t['compared'] += 1
        if rb_ != 'returned':
            t['divergent'] += 1
            t['first'] = t['first'] or 'frame %d: candidate %s' % (self.box.frame, rb_)
            return
        why = compare(sa, self.b.state(), sp, t['kind'])
        if why:
            t['divergent'] += 1
            t['first'] = t['first'] or 'frame %d: %s' % (self.box.frame, why)


_calls = None


def NB_calls():
    global _calls
    if _calls is None:
        import n64rom
        _calls = n64rom.calls
    return _calls


def verdict(t):
    if t['divergent']:
        return 'DIVERGENT'
    if t['compared'] == 0:
        return 'UNCOVERED'
    return 'EQUIVALENT'


def run_live(vas):
    targets = {}
    for i, va in enumerate(vas):
        code, data, sha = link_candidate(va, TEST_CODE + 0x4000 * i, TEST_DATA + 0x1000 * i)
        targets[va] = (code, data, sha)
    agg = {va: dict(compared=0, divergent=0, blocked=0, first=None, sha=targets[va][2],
                    scripts=[]) for va in vas}
    for sc in scripts():
        box = NB.Box(script=os.path.join(SCRIPTS, sc))
        ls = Lockstep(box, targets)
        r = box.run(FRAMES.get(sc, 2400))
        if r not in ('frames',):
            print('  %s: run ended early: %s' % (sc, r))
        for va in vas:
            t, g = ls.t[va], agg[va]
            for k in ('compared', 'divergent', 'blocked'):
                g[k] += t[k]
            g['first'] = g['first'] or t['first']
            g['scripts'].append('%s:%d' % (sc, t['compared']))
    rows = {}
    if os.path.exists(LIVE):
        rows = {r['va']: r for r in csv.DictReader(open(LIVE))}
    today = datetime.date.today().isoformat()
    for va, g in agg.items():
        v = verdict(g)
        rows['%08X' % va] = dict(va='%08X' % va, verdict=v, compared=g['compared'],
                                 divergent=g['divergent'], blocked=g['blocked'],
                                 src=g['sha'], date=today, scripts=' '.join(g['scripts']),
                                 first=g['first'] or '')
        print('%08X %-10s compared %d, divergent %d, blocked %d  %s'
              % (va, v, g['compared'], g['divergent'], g['blocked'], g['first'] or ''))
    with open(LIVE, 'w', newline='') as f:
        w = csv.DictWriter(f, ['va', 'verdict', 'compared', 'divergent', 'blocked', 'src',
                               'date', 'scripts', 'first'], lineterminator='\n')
        w.writeheader()
        w.writerows(sorted(rows.values(), key=lambda r: r['va']))


def run_image(with_vas=()):
    only_extra = set(with_vas)
    cert = IMG.t3_certified() | only_extra
    img, extra, rep = IMG.build(t3=True)
    if only_extra:
        # place the bodies being qualified as well
        img, extra, rep = build_with(cert)
    placed = sorted(cert)
    results = []
    for sc in scripts():
        a = NB.Box(script=os.path.join(SCRIPTS, sc))
        ra = a.run(FRAMES.get(sc, 2400))
        b = NB.Box(script=os.path.join(SCRIPTS, sc), image=img, extra=extra)
        rb = b.run(FRAMES.get(sc, 2400))
        first = None
        for x, y in zip(a.log, b.log):
            if x != y:
                first = 'frame %d %s' % (x[0], x[1])
                break
        if first is None and (len(a.log) != len(b.log) or ra != rb):
            first = 'run ended differently (%s / %s)' % (ra, rb)
        results.append((sc, first, a.frame))
        print('%s: %s over %d frames' % (sc, 'IDENTICAL' if first is None else 'DIFFERS at ' + first, a.frame))
    verdict_ = 'IDENTICAL' if all(r[1] is None for r in results) else 'DIFFERENT'
    shas = {}
    for va in placed:
        shas['%08X' % va] = source_of(va)[2]
    rows = list(csv.DictReader(open(WHOLE))) if os.path.exists(WHOLE) else []
    rows.append(dict(date=datetime.datetime.now().isoformat(timespec='seconds'),
                     verdict=verdict_, bodies=' '.join('%s:%s' % kv for kv in sorted(shas.items())),
                     scripts=' '.join('%s:%d' % (r[0], r[2]) for r in results),
                     first='; '.join('%s %s' % (r[0], r[1]) for r in results if r[1])))
    with open(WHOLE, 'w', newline='') as f:
        w = csv.DictWriter(f, ['date', 'verdict', 'bodies', 'scripts', 'first'], lineterminator='\n')
        w.writeheader()
        w.writerows(rows)
    print('A7:', verdict_, '(%d T3 bodies placed)' % len(placed))
    return verdict_


def build_with(vas):
    """The M1 image with exactly the given non-exact bodies placed."""
    return IMG.build(t3=True, all_tagged=False, only=None) if not vas else _build_only(vas)


def _build_only(vas):
    img, extra, rep = IMG.build(t3=False)
    img = bytearray(img)
    fmap = B.function_map()
    code_va, data_va = IMG.ANNEX_CODE, IMG.ANNEX_DATA
    for va in sorted(vas):
        code, data, sha = link_candidate(va, va, data_va)
        off = va - B.BASE
        if len(code) > fmap[va]:
            code, data, sha = link_candidate(va, code_va, data_va)
            extra.append((code_va, code))
            img[off:off + 8] = (0x08000000 | ((code_va >> 2) & 0x03FFFFFF)).to_bytes(4, 'big') + bytes(4)
            code_va += (len(code) + 15) & ~15
        else:
            img[off:off + fmap[va]] = code.ljust(fmap[va], b'\0')
        for dva, blob in data:
            extra.append((dva, blob))
            data_va = max(data_va, (dva + len(blob) + 15) & ~15)
    return bytes(img), extra, rep


PASS = re.compile(r'@t4-pass\s+(0x[0-9A-Fa-f]{8})\s+\S+\s+(\S+)\s+compiles\s+(\d+)\s+best\s+(\d+)\s+moved\s+(\d+)')


def qualify(va):
    f, name, sha = source_of(va)
    ok = True

    def gate(n, passed, why):
        nonlocal ok
        ok &= passed
        print('  Gate %-3s %s  %s' % (n, 'pass' if passed else 'FAIL', why))
    print('%08X %s  (%s)' % (va, name, f and os.path.relpath(f, ROOT)))
    if f is None:
        print('  no source')
        return False
    src = open(f).read()
    body = function_text(src, name)
    idx = src.find('@implements 0x%08X' % va)
    head = src[max(0, idx - 1500):idx]
    gate('0', 'WHAT IT DOES:' in head and ('@t3 0x%08X' % va) in src
         and not re.search(r'\b(TODO|FIXME|XXX)\b', body),
         'description, @t3 tag, no unfinished markers')
    live = {r['va']: r for r in csv.DictReader(open(LIVE))} if os.path.exists(LIVE) else {}
    r = live.get('%08X' % va)
    gate('A5', bool(r) and r['verdict'] == 'EQUIVALENT' and r['src'] == sha,
         r and '%s over %s calls (%s)%s' % (r['verdict'], r['compared'], r['scripts'],
                                            '' if r['src'] == sha else ', STALE: source changed')
         or 'no live-oracle row')
    whole = list(csv.DictReader(open(WHOLE))) if os.path.exists(WHOLE) else []
    last = next((w for w in reversed(whole) if ('%08X:' % va) in w['bodies']), None)
    cur = last and ('%08X:%s' % (va, sha)) in last['bodies']
    gate('A7', bool(last) and last['verdict'] == 'IDENTICAL' and bool(cur),
         last and '%s on %s%s' % (last['verdict'], last['date'], '' if cur else ', STALE') or
         'never placed in a whole-image run')
    passes = [m for m in PASS.findall(src) if int(m[0], 16) == va]
    good = [m for m in passes if int(m[2]) >= 10]
    gate('B', len(good) >= 2 and good[-1][4] == '0',
         '%d counted @t4-pass lines' % len(good))
    print('  =>', 'QUALIFIES for T3' if ok else 'not T3')
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--live', nargs='+')
    ap.add_argument('--image', action='store_true')
    ap.add_argument('--with', dest='with_', nargs='*', default=[])
    ap.add_argument('--qualify', nargs='+')
    a = ap.parse_args()
    if a.live:
        run_live([int(v, 16) for v in a.live])
    if a.image:
        run_image([int(v, 16) for v in a.with_])
    if a.qualify:
        for v in a.qualify:
            qualify(int(v, 16))


if __name__ == '__main__':
    main()
