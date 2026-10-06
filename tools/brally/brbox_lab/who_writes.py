import os, sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
from unicorn import UC_HOOK_MEM_WRITE, UC_HOOK_CODE
script, f0 = sys.argv[1], int(sys.argv[2])
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
seen = {}
def on_w(uc, access, addr, size, value, _):
    if box.hs.frame >= f0 and size == 4 and (value & 0xffffffff) in (0xFC317E02, 0x5FFEF3FA, 0x51FEF3FA):
        pc = uc.reg_read(brbox.UC_X86_REG_EIP)
        k = (pc, value & 0xffffffff)
        if k not in seen:
            seen[k] = (box.hs.frame, addr)
            print('frame %d pc %08X writes %08X at %08X' % (box.hs.frame, pc, value & 0xffffffff, addr), flush=True)
box.uc.hook_add(UC_HOOK_MEM_WRITE, on_w)
def on_frame(b):
    if b.hs.frame >= f0 + 400: raise brbox.Stop('done')
box.on_frame = on_frame
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e:
    print('end', e)
