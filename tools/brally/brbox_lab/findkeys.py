import sys, struct
sys.path.insert(0, 'tools')
import brbox, brbox_drive
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script('build/brally/win32/brbox/gap/rs.txt'), log=lambda m: None)
brbox_drive.attach(box, drv)
try:
    box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault): pass
for base, size in [(0x10000000, 0x01A00000)]:
    for off in range(0, size, 0x10000):
        try: d = bytes(box.uc.mem_read(base + off, 0x10000))
        except Exception: continue
        for pat, w in ((struct.pack("<III", 0x25, 0x27, 0x26), 4), (struct.pack("<II", 0xCB, 0xCD), 4), (bytes([0x25, 0x27, 0x26]), 1)):
            i = d.find(pat)
            while i >= 0:
                a = base + off + i
                vals = [box.rd32(a + 4*k) if w == 4 else (d[i+k] if w == 1 else struct.unpack_from('<H', d, i+2*k)[0]) for k in range(24)]
                print('%08X w%d %s' % (a, w, ' '.join('%X' % v for v in vals)))
                i = d.find(pat, i + 1)
