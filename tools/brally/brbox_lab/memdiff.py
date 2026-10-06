import sys
sys.path.insert(0, 'tools')
import brbox, brbox_drive
script = sys.argv[1]; fa, fb, fc = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
box = brbox_drive.make_box(log=lambda m: None, trace_imports=False)
drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=lambda m: None)
brbox_drive.attach(box, drv)
snaps = {}
LO, HI = 0x10077000, 0x11900000
def snap():
    out = bytearray()
    for a in range(LO, HI, 0x1000):
        try: out += box.uc.mem_read(a, 0x1000)
        except Exception: out += b'\xee' * 0x1000
    return bytes(out)
def on_frame(b):
    if b.hs.frame in (fa, fb, fc): snaps[b.hs.frame] = snap()
    if b.hs.frame > fc: raise brbox.Stop('done')
box.on_frame = on_frame
try: box.boot(); box.rally_main()
except (brbox.Stop, brbox.GuestFault): pass
import struct
A, B, C = snaps[fa], snaps[fb], snaps[fc]
# dwords stable before (fa==fb... no: A,B both before press), changed after, small values
for i in range(0, len(A) - 3, 4):
    a, bb, c = (struct.unpack_from('<I', x, i)[0] for x in (A, B, C))
    if a == bb and bb != c and c < 16 and bb < 16:
        print('%08X  %d -> %d' % (LO + i, bb, c))
