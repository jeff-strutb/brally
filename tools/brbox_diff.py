#!/usr/bin/env python3
"""brbox_diff.py -- the whole T3 image against the original, frame by frame.

The live oracle (t3live.py) swaps ONE T3 body into the original at a time,
so it cannot see faults that only exist between transcriptions (a callee
that stopped returning what its transcribed caller reads, an argument
convention two bodies disagree on).  This runs the assembled image
(build/brbox/image/BRGlide.T3.dll, every T3 body placed) and the original
DLL through the same script and compares, every frame:

  * what the game DREW -- a hash of every Glide call and its arguments;
  * what it KEEPS -- a hash of the game-state data;
  * how far it got -- frames reached, and how each run ended.

The first differing frame is the report.  With --localize, both runs are
replayed to that frame and the DLL's data area is compared page by page and
byte by byte; each differing dword is attributed to the instruction that last
wrote it on each side (a write hook over the final frame only).

  .venv/bin/python tools/brbox_diff.py tools/brbox_scripts/20_quickrace_drive.txt
  .venv/bin/python tools/brbox_diff.py S.txt --localize
"""
from __future__ import print_function

import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import brbox                                     # noqa: E402
import brbox_drive                               # noqa: E402
from brbox import GuestFault, Stop               # noqa: E402

ROOT = brbox.ROOT
T3_IMAGE = os.path.join(ROOT, 'build', 'brbox', 'image', 'BRGlide.T3.dll')
ORIG = brbox.REF_DLL
_CALLS_FRAME = int(os.environ.get('BRDIFF_CALLS', '0'))   # dump this frame's Glide calls
_calls = {}
DATA_LO, DATA_HI = 0x10077000, 0x11900000       # .rdata tail .. end of .bss


def _run(dll, script, stop_frame=None, watch_last=False, seconds=None):
    box = brbox_drive.make_box(log=lambda m: None, dll=dll)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
    brbox_drive.attach(box, drv)
    peer = brbox_drive.start_peer_if_any(box, drv, script)
    frames = []
    cur = {'gl': 0, 'raw': 0}

    prev_glide = box.on_glide

    def on_glide(b, name, a):
        if prev_glide is not None:
            prev_glide(b, name, a)
        if b.subrun:
            return
        # the stdcall suffix (@N) is the exact argument size
        try:
            n = int(name.rsplit('@', 1)[1]) // 4
        except (IndexError, ValueError):
            n = 0
        vals = []
        for k in range(n):
            try:
                vals.append(a[k])
            except Exception:
                break
        blob = raw = name.encode()
        for v in vals:
            if brbox.STACK_LO <= v < brbox.STACK_HI:
                # a vertex/array on the caller's stack: the address is layout,
                # the contents are what was drawn
                try:
                    mem = bytes(b.uc.mem_read(v, 40))
                    blob += _clean(mem)
                    raw += mem
                except Exception:
                    blob += b'?'
                    raw += b'?'
            else:
                blob += struct.pack('<I', v)
                raw += struct.pack('<I', v)
        cur['gl'] = zlib.crc32(blob, cur['gl'])
        cur['raw'] = zlib.crc32(raw, cur['raw'])
        if b.hs.frame + 1 == _CALLS_FRAME:
            _calls.setdefault(dll, []).append((name, ['%08X' % v for v in vals], zlib.crc32(blob),
                                               '%08X' % b.recent[-1][1], blob, _stackcode(b)))
    box.on_glide = on_glide

    writes = {}
    hook = [None]

    def on_frame(b):
        f = b.hs.frame
        # the whole data area (.rdata tail .. end of .bss): game state, the
        # texture cache, display lists -- everything the DLL keeps
        st = zlib.crc32(bytes(b.uc.mem_read(DATA_LO, DATA_HI - DATA_LO)))
        frames.append((f, cur['gl'], st, cur['raw']))
        cur['gl'] = cur['raw'] = 0
        if stop_frame is not None:
            if watch_last and f == stop_frame - 1 and hook[0] is None:
                from unicorn import UC_HOOK_MEM_WRITE
                from unicorn.x86_const import UC_X86_REG_EIP

                def on_w(uc, acc, addr, sz, v, ud):
                    if DATA_LO <= addr < DATA_HI:
                        eip = uc.reg_read(UC_X86_REG_EIP)
                        for k in range(sz):
                            writes[addr + k] = eip
                hook[0] = b.uc.hook_add(UC_HOOK_MEM_WRITE, on_w)
            if f >= stop_frame:
                raise Stop('stop frame')
    box.on_frame = on_frame
    end = 'returned'
    try:
        box.boot()
        box.rally_main(budget_s=seconds)
    except Stop as e:
        end = 'stopped: %s' % e
    except GuestFault as e:
        end = 'FAULT: %s' % str(e).split('\n')[0]
    finally:
        if peer is not None:
            peer[2].stop()
            peer[0].join(30)
    snap = None
    if stop_frame is not None:
        snap = bytes(box.uc.mem_read(DATA_LO, DATA_HI - DATA_LO))
    return frames, end, snap, writes, box


def _clean(vtx):
    """A vertex field the original never writes holds stale stack: a small
    integer or pointer left by an earlier call, read as a float it is a
    denormal-sized number (BrDlClipTriFlatNoZ's alpha: 0x7B in the original,
    0x10 under the T3 image's frame layout; the 2D blit's ooz: 0 vs -1).
    Denormal, tiny and NaN/Inf patterns are compared as 0 -- no computed
    vertex value is one -- and every frame that differs ONLY in them is
    reported separately, never silently counted as agreeing."""
    out = bytearray(vtx)
    for k in range(0, len(out) - 3, 4):
        u = struct.unpack_from('<I', out, k)[0]
        e = u & 0x7F800000
        if e == 0 or e == 0x7F800000 or abs(struct.unpack_from('<f', out, k)[0]) < 1e-30:
            struct.pack_into('<I', out, k, 0)
    return bytes(out)


def _write_seq(dll, script, frame, seconds, replaced):
    """Every data-area write made by code both images share, during one frame,
    in order: (address, value, eip).  Writes by replaced (T3) code and to the
    stack are left out -- their order and scratch values are allowed to differ;
    what the shared code stores next is not."""
    box = brbox_drive.make_box(log=lambda m: None, dll=dll)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
    brbox_drive.attach(box, drv)
    seq = []
    calls = []
    hook = [None]
    from unicorn import UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_EIP

    def on_w(uc, acc, addr, sz, v, ud):
        if box.subrun or not (DATA_LO <= addr < DATA_HI):
            return
        eip = uc.reg_read(UC_X86_REG_EIP)
        if _in(replaced, eip):
            # remember which replaced body was running
            if not calls or calls[-1][1] != len(seq):
                calls.append((eip, len(seq)))
            return
        seq.append((addr, v & ((1 << (8 * sz)) - 1), eip))

    def on_frame(b):
        f = b.hs.frame
        if f == frame and hook[0] is None:
            hook[0] = b.uc.hook_add(UC_HOOK_MEM_WRITE, on_w)
        if f > frame:
            raise Stop('frame done')
    box.on_frame = on_frame
    try:
        box.boot()
        box.rally_main(budget_s=seconds)
    except (Stop, GuestFault):
        pass
    return seq, calls


def _t3_calls(dll, script, frame, seconds, entries):
    """Every call to a T3 function during one frame, in completion order:
    (va, return value, crc of the non-stack data it wrote (nested calls
    included), number of such bytes).  Both images run the same control
    flow up to the first real divergence, so the lists pair by index."""
    box = brbox_drive.make_box(log=lambda m: None, dll=dll)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
    brbox_drive.attach(box, drv)
    from unicorn import UC_HOOK_MEM_WRITE, UC_HOOK_CODE, UC_HOOK_BLOCK
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
    live = []          # [va, ret, esp, {addr: byte}]
    done = []
    on = [False]

    def ent(uc, a, s, u):
        if not on[0] or box.subrun or a not in entries:
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        live.append([a, box.rd32(esp), esp, {}])

    def blk(uc, a, s, u):
        while live and a == live[-1][1] and uc.reg_read(UC_X86_REG_ESP) > live[-1][2]:
            va, _r, _e, w = live.pop()
            items = bytes(b for k in sorted(w) for b in struct.pack('<IB', k, w[k]))
            done.append((va, uc.reg_read(UC_X86_REG_EAX), zlib.crc32(items), len(w)))

    def wr(uc, acc, addr, sz, v, u):
        if not live or box.subrun or brbox.STACK_LO <= addr < brbox.STACK_HI:
            return
        for k in range(sz):
            b = (v >> (8 * k)) & 0xFF
            for f in live:
                f[3][addr + k] = b

    hooks = []

    def on_frame(b):
        f = b.hs.frame
        if f == frame and not on[0]:
            on[0] = True
            for va in entries:
                hooks.append(b.uc.hook_add(UC_HOOK_CODE, ent, begin=va, end=va))
            hooks.append(b.uc.hook_add(UC_HOOK_BLOCK, blk))
            hooks.append(b.uc.hook_add(UC_HOOK_MEM_WRITE, wr))
            # blocks translated before the hooks existed would skip them
            b.uc.ctl_flush_tb()
        if f > frame:
            raise Stop('frame done')
    box.on_frame = on_frame
    try:
        box.boot()
        box.rally_main(budget_s=seconds)
    except (Stop, GuestFault):
        pass
    return done


def _in(ranges, a):
    import bisect
    i = bisect.bisect_right(ranges, (a, 0xFFFFFFFF)) - 1
    return i >= 0 and ranges[i][0] <= a < ranges[i][1]


def _replaced_ranges(image):
    """The original extents of every T3 function (the ledger's VAs, each as
    long as its original body), plus the whole annex.  A byte comparison of
    the two images is not enough: a T3 body can match the original for a
    stretch and still be T3 code."""
    import csv
    out = [(0x1190D000, 0x11950000)]
    with open(os.path.join(ROOT, 'config', 't3_live.csv')) as f:
        for r in csv.DictReader(f):
            va = int(r['va'], 16)
            p = os.path.join(ROOT, 'build', 'match', 'orig', '0x%08X.bin' % va)
            if os.path.exists(p):
                out.append((va, va + os.path.getsize(p)))
    return sorted(out)


def _stackcode(b):
    """Return-address-looking words on the stack (text or the T3 annex)."""
    from unicorn.x86_const import UC_X86_REG_ESP
    esp = b.uc.reg_read(UC_X86_REG_ESP)
    out = []
    for k in range(0, 0x600, 4):
        try:
            v = struct.unpack('<I', bytes(b.uc.mem_read(esp + k, 4)))[0]
        except Exception:
            break
        if 0x10001000 <= v < 0x10077000 or 0x1190D000 <= v < 0x11950000:
            out.append('%08X' % v)
    return out[:14]


def _dis(box, eip):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    try:
        i = next(Cs(CS_ARCH_X86, CS_MODE_32).disasm(bytes(box.uc.mem_read(eip, 16)), eip))
        return '%s %s' % (i.mnemonic, i.op_str)
    except Exception:
        return '?'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('script')
    ap.add_argument('--image', default=T3_IMAGE)
    ap.add_argument('--localize', action='store_true')
    ap.add_argument('--seconds', type=float, default=1800)
    ap.add_argument('--frame', type=int, help='localize at this frame instead of the first difference')
    ap.add_argument('--writes', type=int, metavar='F',
                    help='first differing data write by shared code during frame F')
    ap.add_argument('--t3calls', type=int, metavar='F',
                    help='first T3 call during frame F whose writes or return value differ')
    a = ap.parse_args()

    if a.t3calls is not None:
        import csv
        with open(os.path.join(ROOT, 'config', 't3_live.csv')) as f:
            ent = {int(r['va'], 16): r['name'] for r in csv.DictReader(f)}
        co = _t3_calls(ORIG, a.script, a.t3calls, a.seconds, set(ent))
        ct = _t3_calls(a.image, a.script, a.t3calls, a.seconds, set(ent))
        print('frame %d: %d / %d T3 calls' % (a.t3calls, len(co), len(ct)))
        # eax is not compared: a void body leaves whatever it last computed,
        # and a return value that matters shows up in the caller's writes
        for i, (x, y) in enumerate(zip(co, ct)):
            if x[0] != y[0] or x[2:] != y[2:]:
                print('first differing T3 call #%d: %s 0x%08X  eax %08X/%08X  writes crc %08X/%08X (%d/%d bytes)' % (
                    i, ent.get(x[0], '?'), x[0], x[1], y[1], x[2], y[2], x[3], y[3]))
                if x[0] != y[0]:
                    print('  (call order parted: t3 ran %s 0x%08X)' % (ent.get(y[0], '?'), y[0]))
                return 1
        print('every T3 call agrees')
        return 0

    if a.writes is not None:
        rep = _replaced_ranges(a.image)
        so, _co = _write_seq(ORIG, a.script, a.writes, a.seconds, rep)
        st, ct = _write_seq(a.image, a.script, a.writes, a.seconds, rep)
        print('frame %d: %d / %d shared-code writes' % (a.writes, len(so), len(st)))
        # Every differing write, grouped by the instruction that made it.
        # The first of each group is shown with the replaced bodies that ran
        # just before it; a value read from uninitialised stack (a stale
        # saved register) shows up here too -- triage each group.
        groups = {}
        path = None
        for i, (x, y) in enumerate(zip(so, st)):
            if x[0] != y[0] or x[2] != y[2]:
                path = i
                break
            if x[1] != y[1]:
                g = groups.setdefault(x[2], [0, i, x, y])
                g[0] += 1
        for eip, (n, i, x, y) in sorted(groups.items(), key=lambda kv: kv[1][1])[:40]:
            before = [c for c in ct if c[1] <= i]
            print('  %6d x by %08X  first #%d [%08X] orig %08X t3 %08X   after %s' % (
                n, eip, i, x[0], x[1], y[1], ' '.join('%08X' % c[0] for c in before[-3:])))
        if path is not None:
            print('control flow parts at write #%d: orig [%08X] by %08X, t3 [%08X] by %08X' % (
                path, so[path][0], so[path][2], st[path][0], st[path][2]))
        if not groups and path is None and len(so) == len(st):
            print('shared-code writes agree for the whole frame')
            return 0
        return 1

    fo, eo, _s, _w, _b = _run(ORIG, a.script, seconds=a.seconds)
    ft, et, _s, _w, _b = _run(a.image, a.script, seconds=a.seconds)
    if _CALLS_FRAME:
        co, ct = _calls.get(ORIG, []), _calls.get(a.image, [])
        print('frame %d Glide calls: %d vs %d' % (_CALLS_FRAME, len(co), len(ct)))
        for i, (x, y) in enumerate(zip(co, ct)):
            if x[2] != y[2]:
                print('  first differing call #%d (from %s / %s):\n    orig %s %s\n    t3   %s %s' % (
                    i, x[3], y[3], x[0], x[1], y[0], y[1]))
                print('    orig stack code: %s\n    t3   stack code: %s' % (' '.join(x[5]), ' '.join(y[5])))
                for k in range(0, min(len(x[4]), len(y[4])), 4):
                    if x[4][k:k + 4] != y[4][k:k + 4]:
                        print('    byte %d: orig %s t3 %s' % (k, x[4][k:k + 4].hex(), y[4][k:k + 4].hex()))
                print('    orig blob %s\n    t3   blob %s' % (x[4].hex(), y[4].hex()))
                break
    print('original: %d frames, %s' % (len(fo), eo))
    print('T3 image: %d frames, %s' % (len(ft), et))
    first = None
    n_ = min(len(fo), len(ft))
    drawn = [x[0] for x, y in zip(fo, ft) if x[1] != y[1]]
    state = [x[0] for x, y in zip(fo, ft) if x[2] != y[2]]
    garbage = [x[0] for x, y in zip(fo, ft) if x[1] == y[1] and x[3] != y[3]]
    print('frames that drew differently: %d/%d %s' % (len(drawn), n_, drawn[:12]))
    print('frames whose state differs:   %d/%d %s' % (len(state), n_, state[:12]))
    print('frames differing ONLY in vertex fields the original leaves unwritten '
          '(stale-stack denormals/NaN): %d/%d %s' % (len(garbage), n_, garbage[:12]))
    fo = [x[:3] for x in fo]
    ft = [x[:3] for x in ft]
    for (f1, g1, s1), (f2, g2, s2) in zip(fo, ft):
        if g1 != g2 or s1 != s2:
            first = (f1, 'drawn' if g1 != g2 else '', 'state' if s1 != s2 else '')
            break
    if first is None:
        if len(fo) == len(ft) and eo == et:
            print('IDENTICAL: every frame drew and kept the same')
            return 0
        n = min(len(fo), len(ft))
        print('frames agree up to %d; then %s' % (n, 'the T3 image stops early' if len(ft) < len(fo)
                                                   else 'the original stops early'))
        first = (fo[n - 1][0] if n else 1, 'end', '')
    else:
        print('FIRST DIFFERENCE at frame %d (%s)' % (first[0], ' '.join(x for x in first[1:] if x)))
    if not a.localize:
        return 1
    F = a.frame or first[0]
    _f, _e, so, wo, bo = _run(ORIG, a.script, stop_frame=F, watch_last=True, seconds=a.seconds)
    _f, _e, st, wt, bt = _run(a.image, a.script, stop_frame=F, watch_last=True, seconds=a.seconds)
    diffs = [DATA_LO + i for i in range(0, len(so), 4) if so[i:i + 4] != st[i:i + 4]]
    print('%d differing dwords in the data area at the start of frame %d' % (len(diffs), F))
    runs = []
    for d in diffs:
        if runs and d - runs[-1][1] <= 16:
            runs[-1][1] = d
        else:
            runs.append([d, d])
    for lo, hi in runs[:40]:
        eo_, et_ = wo.get(lo), wt.get(lo)
        print('  %08X..%08X  orig %08X  t3 %08X   written by orig %s [%s] / t3 %s [%s]' % (
            lo, hi + 3, struct.unpack_from('<I', so, lo - DATA_LO)[0],
            struct.unpack_from('<I', st, lo - DATA_LO)[0],
            '%08X' % eo_ if eo_ else '-', _dis(bo, eo_) if eo_ else '',
            '%08X' % et_ if et_ else '-', _dis(bt, et_) if et_ else ''))
    return 1


if __name__ == '__main__':
    sys.exit(main())
