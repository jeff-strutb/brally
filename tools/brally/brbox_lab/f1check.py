import sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(sys.argv[1]), log=lambda m: None)
brbox_drive.attach(box, drv)
pend = {}; hits = []; calls = [0]
def on_entry(uc, a, sz, _):
    sp = uc.reg_read(UC_X86_REG_ESP); act = box.rd32(sp + 4)
    if 0x11 <= act <= 0x14:
        calls[0] += 1; pend[box.rd32(sp)] = act
def on_ret(uc, a, sz, _):
    pass
box.uc.hook_add(UC_HOOK_CODE, on_entry, begin=0x100719D0, end=0x100719D0)
# catch returns: hook each return address lazily
seen = set()
def add_ret(ra):
    def h(uc, a, sz, _):
        v = uc.reg_read(UC_X86_REG_EAX) & 0xff
        if v: hits.append((box.hs.frame, pend.get(ra), v))
    box.uc.hook_add(UC_HOOK_CODE, h, begin=ra, end=ra)
orig = on_entry
def on_entry2(uc, a, sz, _):
    sp = uc.reg_read(UC_X86_REG_ESP); ra = box.rd32(sp)
    if 0x11 <= box.rd32(sp + 4) <= 0x14 and ra not in seen:
        seen.add(ra); add_ret(ra)
box.uc.hook_add(UC_HOOK_CODE, on_entry2, begin=0x100719D0, end=0x100719D0)
try: box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault) as e: print(e)
print('calls with action 0x11..0x14:', calls[0], ' nonzero returns:', hits[:10])
