import sys, zlib, json
sys.path.insert(0, 'tools/brally')
import brbox_drive, t3live
from brbox import Stop, GuestFault
mode = sys.argv[1]
box=brbox_drive.make_box(log=lambda m: None)
drv=brbox_drive.Driver(brbox_drive.parse_script('tools/brally/brbox_scripts/20_quickrace_drive.txt'), log=lambda m: None)
brbox_drive.attach(box, drv)
if mode != 'plain':
    only = None if mode == 'all' else {mode}
    orc = t3live.Oracle(box, t3live.t3_targets(only), per_fn=6, log=lambda m: None, image=t3live.DEFAULT_IMAGE)
hashes = []
def of(b):
    if b.hs.frame > 420: raise Stop('enough')
    h = zlib.crc32(bytes(b.uc.mem_read(0x10AF0000, 0x10000)))
    h = zlib.crc32(bytes(b.uc.mem_read(0x105BC000, 0x12000)), h)
    hashes.append((b.hs.frame, h, round(b.hs.ms, 3)))
box.on_frame = of
try:
    box.boot(); box.rally_main()
except (Stop, GuestFault) as e: pass
json.dump(hashes, open('build/brally/win32/brbox/det_%s.json' % mode, 'w'))
