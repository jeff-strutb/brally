#!/usr/bin/env python3
"""fncheck.py -- compare the port with the original at the k-th entry of a
game function (finer than lockstep.py's graphics tasks): both memories are
dumped there and the .data/.bss symbols that differ are listed.

    fncheck.py FUNCTION [--nth K] [--script FILE] [--frames N]

The original stops in n64/tools/n64box.py at the function's address (from
n64/config/symbols_tgr.csv); the port stops at a breakpoint under lldb and
calls tgr_dump_state.  K counts from 1.
"""
import argparse
import csv
import os
import subprocess
import sys

from unicorn import UC_HOOK_CODE

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
sys.path.insert(0, HERE)
import tgrbox as n64box  # noqa: E402  (the port's view of the box: tools/tgrbox.py)
import lockstep  # noqa: E402


def address_of(fn):
    for r in csv.DictReader(open(os.path.join(ROOT, 'n64/config/symbols_tgr.csv'))):
        if r['name'] == fn:
            return int(r['va'], 16)
    sys.exit('fncheck: %s is not in the symbol table' % fn)


def box_memory(va, nth, script):
    box = n64box.Box(script=script)
    state = {'n': 0, 'mem': None, 'frame': None}

    def hit(uc, addr, size, data):
        state['n'] += 1
        if state['n'] == nth:
            state['mem'] = bytes(box.read(0x80000000, 0x800000))
            state['frame'] = box.frame
            box.stop_reason = 'dumped'
            uc.emu_stop()
    box.uc.hook_add(UC_HOOK_CODE, hit, begin=n64box.sx(va), end=n64box.sx(va))
    box.run(100000)
    return state['mem'], state['frame']


def port_memory(fn, nth, script, frames):
    out = os.path.join(ROOT, 'build/tgrally/fncheck.bin')
    if os.path.exists(out):
        os.remove(out)
    cmd = ['lldb', '--batch', '-o', 'breakpoint set -n %s' % fn]
    if nth > 1:
        cmd += ['-o', 'breakpoint modify -i %d 1' % (nth - 1)]
    cmd += ['-o', 'run', '-o', 'expr (void)tgr_dump_state("%s", 0)' % out, '-o', 'kill', '--',
            os.path.join(ROOT, 'build/tgrally/tgrally'), '--headless', '--frames', str(frames)]
    if script:
        cmd += ['--script', script]
    subprocess.run(cmd, cwd=ROOT, capture_output=True)
    if not os.path.exists(out):
        sys.exit('fncheck: the port never reached %s #%d' % (fn, nth))
    return open(out, 'rb').read()[4:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('fn')
    ap.add_argument('--nth', type=int, default=1)
    ap.add_argument('--script')
    ap.add_argument('--frames', type=int, default=100000)
    a = ap.parse_args()
    bmem, frame = box_memory(address_of(a.fn), a.nth, a.script)
    if bmem is None:
        sys.exit('fncheck: the original never reached %s #%d' % (a.fn, a.nth))
    pmem = lockstep.normalise(port_memory(a.fn, a.nth, a.script, a.frames))
    print('%s #%d (original frame %d): game state (.data/.bss):' % (a.fn, a.nth, frame))
    lockstep.state_diff(bmem, pmem)


if __name__ == '__main__':
    main()
