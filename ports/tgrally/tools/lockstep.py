#!/usr/bin/env python3
"""lockstep.py -- compare the port with the original ROM (tools/tgrally/n64box.py)
at the k-th graphics task: walk both display lists command by command and
report the first difference (in a command, or in the data it names), and
which game symbols differ in memory.

    lockstep.py --task K [--script FILE]

The port is run with TGR_DUMP_TASK=K; n64box is run to the same task.  Both
memories are compared as the original's bytes: the port keeps scalars native,
so its .data/.bss are turned back to big-endian through the same runs the
lift uses (build/tgrally/null-null/gen) before symbols are compared.
"""
import argparse
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'tools/tgrally'))
import tgrbox as n64box  # noqa: E402  (the port's view of the box: tools/tgrbox.py)


def box_memory(task, script):
    """the original's RDRAM when its task-th graphics task is submitted"""
    box = n64box.Box(script=script)
    state = {'n': 0, 'mem': None, 'dl': None}
    orig = box.os_sp_task

    def hook():
        t = box.arg(0)
        if box.r32(t) == 1:
            if state['n'] == task:
                state['dl'] = box.r32(t + 0x30)
                state['mem'] = bytes(box.read(0x80000000, 0x800000))
                box.stop_reason = 'dumped'
            state['n'] += 1
        orig()
    box.hle[0x80264C7C] = ('osSpTaskLoad', hook)
    box.run(100000)
    return state['dl'], state['mem']


def port_memory(task, script):
    out = os.path.join(ROOT, 'build/tgrally/null-null/lockstep')
    env = dict(os.environ, TGR_DUMP_TASK=str(task), TGR_DUMP=out)
    cmd = [os.path.join(ROOT, 'build/tgrally/null-null/tgrally'), '--headless', '--frames', str(2 * task + 10)]
    if script:
        cmd += ['--script', script]
    subprocess.run(cmd, env=env, cwd=ROOT, capture_output=True)      # a crash after the dump is fine
    blob = open('%s.task%d.bin' % (out, task), 'rb').read()
    return struct.unpack('<I', blob[:4])[0], blob[4:]


def walk(mem, dl, limit=200000):
    """[(address, w0, w1, data)] of a display list as n64box's digest walks
    it: segments resolved, branches followed, with the bytes each command
    names (a matrix, vertices, a movemem)"""
    def r32(a):
        a &= 0x7FFFFF
        return struct.unpack('>I', mem[a:a + 4])[0]

    def rd(a, n):
        a &= 0x7FFFFF
        return mem[a:a + n]
    seg = [0] * 16

    def addr(a):
        return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) | 0x80000000
    out, stack = [], [dl]
    while stack and len(out) < limit:
        pc = stack.pop()
        while len(out) < limit:
            w0, w1 = r32(pc), r32(pc + 4)
            op = w0 >> 24
            if op == 0xF5 and (w0 >> 21) & 7 != 2:
                w1 &= ~0x00F00000         # a non-CI tile's palette: not read (BrTexLoad)
            data = None
            pc += 8
            if op == 0xDB and (w0 >> 16) & 0xFF == 0x06:
                seg[((w0 & 0xFFFF) >> 2) & 0xF] = w1 & 0x1FFFFFFF
            elif op == 0x01:
                data = (addr(w1), rd(addr(w1), 64))
            elif op == 0x04:
                n = ((w0 >> 10) & 0x3F) or 1
                data = (addr(w1), rd(addr(w1), 16 * n))
            elif op == 0x03:
                data = (addr(w1), rd(addr(w1), 16))
            out.append((pc - 8, w0, w1, data))
            if op == 0x06:
                if (w0 >> 16) & 0xFF != 1:
                    stack.append(pc)
                pc = addr(w1)
            elif op == 0xB8:
                break
    return out


DATA_VA, DATA_ROM, DATA_END = 0x8026FAB0, 0x70AB0, 0xAD400


def normalise(pmem):
    """the port's memory as the original's bytes: every multi-byte unit the
    lift turned native back to big-endian -- each symbol's, then those of
    the .data objects reached through pointer tables (tgr_lift's order, each
    byte once)"""
    import json
    m = bytearray(pmem)
    done = bytearray(DATA_END - DATA_ROM)

    def unit(a, size, mark):
        if mark:
            d = a - DATA_VA
            if not (0 <= d and d + size <= len(done)) or any(done[d:d + size]):
                return
            done[d:d + size] = b'\x01' * size
        p = a & 0x7FFFFF
        m[p:p + size] = m[p:p + size][::-1]
    for sym in json.load(open(os.path.join(ROOT, 'build/tgrally/null-null/symunits.json'))):
        for off, size in sym['units']:
            a = sym['addr'] + off
            if DATA_VA <= a < DATA_VA + len(done):
                unit(a, size, True)
            else:
                unit(a, size, False)
    rom = open(n64box.ROM_PATH, 'rb').read()
    for nat in json.load(open(os.path.join(ROOT, 'build/tgrally/null-null/natunits.json'))):
        for k in range(nat['count']):
            r = DATA_ROM + nat['addr'] - DATA_VA + 4 * k
            a = struct.unpack('>I', rom[r:r + 4])[0]
            if DATA_VA <= a < DATA_VA + len(done):
                for off, size in nat['units']:
                    unit(a + off, size, True)
    return bytes(m)


STACKS = n64box.Box.STACKS


def state_diff(bmem, pmem, limit=40):
    """the .data/.bss symbols whose bytes differ"""
    import bisect
    import json
    syms = sorted((s['addr'], s['size'], s['name']) for s in
                  json.load(open(os.path.join(ROOT, 'build/tgrally/null-null/symunits.json'))))
    starts = [x[0] for x in syms]
    lo, hi = 0x8026FAB0, 0x80382BB0
    # the threads' stacks, and the 16-byte argument home area above each
    # top: a thread's entry spills its arguments there on the VR4300
    stack = [(max(b, lo), min(t + 16, hi)) for t, b in STACKS.items()]
    stack.append((0x80316800, 0x803168E0))     # the boot code's own stack
    stack.append((0x802A6000, 0x802A6400))     # libultra's internal state
    # zlib's statics (its allocator, the fixed Huffman tables): zlib is
    # native C, its state native-sized and kept natively
    stack.append((0x80368AC8, 0x80369B70))
    stack.append((0x8028CAB8, 0x8028CABC))      # zlib's fixed_built
    stack.append((0x80324550, 0x8033CBF0))      # the unpack scratch zlib allocates from
    # bytes inside a unit some declaration gives a width: normalise() put
    # those back to big-endian; elsewhere a field's width is unknown, so a
    # word or halfword that matches once swapped is the same value
    covered = bytearray(hi - lo)
    for sym in json.load(open(os.path.join(ROOT, 'build/tgrally/null-null/symunits.json'))):
        for off, size in sym['units']:
            b = sym['addr'] + off - lo
            if 0 <= b < len(covered):
                covered[b:b + size] = b'\x01' * size

    def same_swapped(a):
        d, w, h = a & ~7 & 0x7FFFFF, a & ~3 & 0x7FFFFF, a & ~1 & 0x7FFFFF
        return bmem[w:w + 4] == pmem[w:w + 4][::-1] or bmem[h:h + 2] == pmem[h:h + 2][::-1] or \
            bmem[d:d + 8] == pmem[d:d + 8][::-1]
    out = {}
    for a in range(lo, hi):
        if bmem[a & 0x7FFFFF] == pmem[a & 0x7FFFFF]:
            continue
        if any(x <= a < y for x, y in stack):
            continue
        if not covered[a - lo] and same_swapped(a):
            continue
        i = bisect.bisect_right(starts, a) - 1
        name = '?'
        if i >= 0 and syms[i][0] <= a < syms[i][0] + max(syms[i][1], 1):
            name = '%s+0x%X' % (syms[i][2], a - syms[i][0])
            key = syms[i][2]
        else:
            key = '%08X' % (a & ~0xF)
        out.setdefault(key, []).append(a)
    for k, addrs in list(out.items())[:limit]:
        a = addrs[0]
        n = min(16, len(addrs))
        print('  %-14s %3d bytes differ from %08X: original %s port %s' % (
            k, len(addrs), a, bmem[a & 0x7FFFFF:(a & 0x7FFFFF) + n].hex(), pmem[a & 0x7FFFFF:(a & 0x7FFFFF) + n].hex()))
    print('  %d symbols differ' % len(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--task', type=int, default=0)
    ap.add_argument('--script')
    a = ap.parse_args()
    bdl, bmem = box_memory(a.task, a.script)
    pdl, pmem = port_memory(a.task, a.script)
    raw = pmem                  # what the RSP reads
    pmem = normalise(pmem)
    print('game state (.data/.bss):')
    state_diff(bmem, pmem)
    print('display list: original %08X, port %08X' % (bdl, pdl))
    bw, pw = walk(bmem, bdl), walk(raw, pdl)
    shown = 0
    for i, (x, y) in enumerate(zip(bw, pw)):
        if x[:3] != y[:3]:
            print('command %d differs: original %08X: %08X %08X / port %08X: %08X %08X' % (i, *x[:3], *y[:3]))
            for j in range(max(0, i - 4), min(len(bw), len(pw), i + 4)):
                print('   %s %08X %08X %08X | %08X %08X %08X' % ('>' if j == i else ' ', *bw[j][:3], *pw[j][:3]))
            break
        if x[3] != y[3]:
            print('data of command %d (%08X %08X) at %08X differs:' % (i, x[1], x[2], x[3][0]))
            print('   original', x[3][1].hex())
            print('   port    ', y[3][1].hex())
            shown += 1
            if shown > 4:
                break
    else:
        if len(bw) != len(pw):
            print('%d commands the same, then original %d, port %d' % (min(len(bw), len(pw)), len(bw), len(pw)))
        elif not shown:
            print('%d commands and their data, the same' % len(bw))


if __name__ == '__main__':
    main()
