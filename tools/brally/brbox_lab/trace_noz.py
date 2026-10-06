import os, sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
brbox_drive.KEYS.update({"END": (0x23, 0xCF), "PGDN": (0x22, 0xD1), "INSERT": (0x2D, 0xD2)})
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP
sys.path.insert(0, 'tools/brally'); from pe import *  # noqa: F401,F403
script, f0, nf = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
win = [False]; ev = []
def live(): return f0 <= box.hs.frame < f0 + nf
def on_sel(uc, a, sz, _):
    if not live(): return
    g = box.rd32(0x105D17C8); lz = bool(g & 0x20000) and not (g & 1)
    if lz and not win[0]:
        sp = uc.reg_read(UC_X86_REG_ESP)
        st = [box.rd32(sp + 4*k) for k in range(24)]
        ev.append('%d OPEN geomode %08X  callers %s' % (box.hs.frame, g, ' '.join(fn(w).split()[1] for w in st if 0x10001000 <= w < 0x1007F000)[:200]))
    elif win[0] and not lz:
        sp = uc.reg_read(UC_X86_REG_ESP)
        st = [box.rd32(sp + 4*k) for k in range(24)]
        ev.append('%d close geomode %08X  by %s' % (box.hs.frame, g, ' '.join(fn(w).split()[1] for w in st if 0x10001000 <= w < 0x1007F000)[:120]))
    elif win[0] and lz:
        ev.append('%d   still-open geomode %08X' % (box.hs.frame, g))
    win[0] = lz
box.uc.hook_add(UC_HOOK_CODE, on_sel, begin=0x1001FD70, end=0x1001FD70)
def on_vtx(uc, a, sz, _):
    if live() and win[0]: ev.append('%d   VTX handler %08X' % (box.hs.frame, a))
for va in (0x10021C70, 0x100221D0, 0x10021A20, 0x10022BF0, 0x10022600, 0x10023110, 0x10023360):
    box.uc.hook_add(UC_HOOK_CODE, on_vtx, begin=va, end=va)
def on_frame(b):
    if b.hs.frame >= f0 + nf: raise brbox.Stop('done')
box.on_frame = on_frame
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e: pass
from collections import Counter
print('\n'.join(ev[:14])); print('...'); print(Counter(' '.join(e.split()[1:]) for e in ev).most_common(12))
