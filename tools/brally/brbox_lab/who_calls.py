import os, sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
brbox_drive.KEYS.update({"END": (0x23, 0xCF), "PGDN": (0x22, 0xD1), "INSERT": (0x2D, 0xD2)})
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI
script, f0, f1 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
seen = {}
def at_store(uc, a, sz, _):
    if not (f0 <= box.hs.frame < f1): return
    sp = uc.reg_read(UC_X86_REG_ESP)
    w1 = uc.reg_read(UC_X86_REG_ESI) & 0xffffffff
    ret = box.rd32(sp + 0xC)
    if w1 in (0x5FFEF3FA, 0x51FEF3FA):
        k = (ret, w1)
        seen[k] = seen.get(k, 0) + 1
box.uc.hook_add(UC_HOOK_CODE, at_store, begin=0x1001D141, end=0x1001D141)
def on_frame(b):
    if b.hs.frame >= f1: raise brbox.Stop('done')
box.on_frame = on_frame
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e:
    print('end', e)
sys.path.insert(0, 'tools/brally'); from pe import *  # noqa: F401,F403
for (ret, w1), n in sorted(seen.items()):
    print('caller ret %08X  w1 %08X  x%d  in %s' % (ret, w1, n, fn(ret)))
