#!/usr/bin/env python3
"""trace_decal.py SCRIPT OUT.txt FRAME0 NFRAMES -- ordered event trace of
combines, vertex-routine selections and vertex-handler entries for NFRAMES
frames from FRAME0, with a stack snapshot on every DECAL combine."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import brbox, brbox_drive
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP

HANDLERS = {0x10021C70: 'VTX lit', 0x100221D0: 'VTX LIT-DECAL', 0x10021A20: 'VTX unlit',
            0x10022BF0: 'VTX texgen-lin', 0x10022600: 'VTX texgen', 0x10023110: 'VTX noZ',
            0x10023360: 'VTX noZ-LIT'}
script, out, f0, nf = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
o = open(out, 'w')
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
live = lambda: f0 <= box.hs.frame < f0 + nf
last = [None]

win = [0]
def ev(s):
    if s.startswith('COMBINE FC317E02'):
        win[0] = 1
    elif s.startswith('COMBINE') and win[0]:
        o.write('%6d %s  <- window closes\n' % (box.hs.frame, s)); win[0] = 0; last[0] = s; return
    if not win[0]:
        return
    if s == last[0]:
        return
    last[0] = s
    o.write('%6d %s\n' % (box.hs.frame, s))

def on_comb(uc, a, sz, _):
    if not live(): return
    sp = uc.reg_read(UC_X86_REG_ESP)
    w0, w1 = box.rd32(sp + 4), box.rd32(sp + 8)
    ev('COMBINE %08X %08X' % (w0, w1))
    if w0 == 0xFC317E02:
        words = [box.rd32(sp + 4 * k) for k in range(40)]
        o.write('        stack: %s\n' % ' '.join('%08X' % w for w in words if 0x10001000 <= w < 0x1007F000))
        o.write('        raw:   %s\n' % ' '.join('%08X' % w for w in words[:16]))
box.uc.hook_add(UC_HOOK_CODE, on_comb, begin=0x1001E7A0, end=0x1001E7A0)

def on_sel(uc, a, sz, _):
    if live(): ev('GEOMODE %08X decal=%d' % (box.rd32(0x105D17C8), box.rd32(0x105CDA04)))
box.uc.hook_add(UC_HOOK_CODE, on_sel, begin=0x1001FD70, end=0x1001FD70)

def on_h(uc, a, sz, _):
    if live(): ev(HANDLERS[a])
for va in HANDLERS:
    box.uc.hook_add(UC_HOOK_CODE, on_h, begin=va, end=va)

def on_frame(b):
    if b.hs.frame >= f0 + nf:
        raise brbox.Stop('trace done')
box.on_frame = on_frame
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e:
    o.write('end: %s\n' % e)
