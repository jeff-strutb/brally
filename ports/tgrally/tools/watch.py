#!/usr/bin/env python3
"""watch.py -- which of the original's instructions write a range of RDRAM,
up to the k-th graphics task (n64/tools/n64box.py, a write hook).

    watch.py ADDR [LEN] [--task K] [--script FILE] [--all]

Each distinct writer (pc, the function from n64/config/symbols_tgr.csv) is
printed once with the first value it stored and the task it stored it in;
--all prints every store.
"""
import argparse
import bisect
import csv
import os
import sys

from unicorn import UC_HOOK_MEM_WRITE

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
import tgrbox as n64box  # noqa: E402  (the port's view of the box: tools/tgrbox.py)


def functions():
    out = []
    for r in csv.DictReader(open(os.path.join(ROOT, 'n64/config/symbols_tgr.csv'))):
        try:
            out.append((int(r['va'], 16), r['name']))
        except (KeyError, ValueError):
            pass
    out.sort()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('addr', type=lambda s: int(s, 16))
    ap.add_argument('len', type=lambda s: int(s, 0), nargs='?', default=4)
    ap.add_argument('--task', type=int, default=0)
    ap.add_argument('--script')
    ap.add_argument('--all', action='store_true')
    a = ap.parse_args()
    fns = functions()
    starts = [f[0] for f in fns]
    box = n64box.Box(script=a.script)
    state = {'n': 0}
    seen = set()
    orig = box.os_sp_task

    def task():
        t = box.arg(0)
        if box.r32(t) == 1:
            if state['n'] == a.task:
                box.stop_reason = 'watched'
                box.uc.emu_stop()
            state['n'] += 1
        orig()
    box.hle[0x80264C7C] = ('osSpTaskLoad', task)

    def on_write(uc, access, addr, size, value, data):
        pc = uc.reg_read(n64box.M.UC_MIPS_REG_PC) & 0xFFFFFFFF
        if not a.all and pc in seen:
            return
        seen.add(pc)
        i = bisect.bisect_right(starts, pc) - 1
        fn = '%s+0x%X' % (fns[i][1], pc - fns[i][0]) if i >= 0 else '?'
        ra = uc.reg_read(n64box.GPR[31]) & 0xFFFFFFFF
        j = bisect.bisect_right(starts, ra) - 1
        caller = '%s+0x%X' % (fns[j][1], ra - fns[j][0]) if j >= 0 else '?'
        print('task %3d  pc %08X %-28s %08X <- %0*X (%d)  ra %s' % (
            state['n'], pc, fn, addr & 0xFFFFFFFF, size * 2, value & ((1 << (8 * size)) - 1), size, caller))
    lo = n64box.sx(a.addr)
    box.uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=lo, end=lo + a.len - 1)
    lo = a.addr & 0x1FFFFFFF                # a hook can see the physical address
    box.uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=lo, end=lo + a.len - 1)
    # stores the models make (DMA, the controllers, the Pak) are not the CPU's
    def wrap(name):
        f = getattr(box.uc, name)

        def g(addr, data, *rest):
            p = addr & 0x1FFFFFFF
            if p < (a.addr & 0x1FFFFFFF) + a.len and p + len(data) > (a.addr & 0x1FFFFFFF):
                pc = box.uc.reg_read(n64box.M.UC_MIPS_REG_PC) & 0xFFFFFFFF
                print('task %3d  model store at %08X, %d bytes, pc %08X: %s' % (
                    state['n'], addr | 0x80000000, len(data), pc, bytes(data[:16]).hex()))
            return f(addr, data, *rest)
        setattr(box.uc, name, g)
    wrap('mem_write')
    box.run(100000)


if __name__ == '__main__':
    main()
