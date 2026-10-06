import os, sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
sys.path.insert(0, 'tools/brally'); from pe import *  # noqa: F401,F403
script, f0 = sys.argv[1], int(sys.argv[2])
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
watch = {}; out = []
def on_clr(uc, a, sz, _):
    if box.hs.frame < f0: return
    p = box.rd32(uc.reg_read(UC_X86_REG_ESP) + 4)
    g = box.rd32(0x105D17C8)
    if (box.rd32(p + 4) & 1) and (g & 0x20000) and (g & 1) and len(out) < 3:
        cmds = ['%08X:%08X' % (box.rd32(p + 8*k), box.rd32(p + 8*k + 4)) for k in range(-4, 6)]
        out.append('frame %d cmd @%08X geomode %08X\n   %s' % (box.hs.frame, p, g, '\n   '.join(cmds)))
        watch[p] = box.hs.frame; watch[p + 4] = box.hs.frame
box.uc.hook_add(UC_HOOK_CODE, on_clr, begin=0x1001FD40, end=0x1001FD40)
writers = {}
def on_w(uc, acc, addr, size, value, _):
    if addr in watch and box.hs.frame > watch[addr]:
        pc = uc.reg_read(UC_X86_REG_EIP); sp = uc.reg_read(UC_X86_REG_ESP)
        st = [box.rd32(sp + 4*k) for k in range(40)]
        chain = [fn(w).split()[1] for w in st if 0x10001000 <= w < 0x1007F000][:6]
        writers[(pc, addr - min(watch))] = (value & 0xffffffff, chain)
box.uc.hook_add(UC_HOOK_MEM_WRITE, on_w)
def on_frame(b):
    if b.hs.frame >= f0 + 40: raise brbox.Stop('done')
box.on_frame = on_frame
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e: pass
print('\n'.join(out))
for (pc, off), (v, ch) in sorted(writers.items()):
    print('writer pc %08X (%s) +%d = %08X  stack: %s' % (pc, fn(pc), off, v, ' <- '.join(ch)))
