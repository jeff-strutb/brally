#!/usr/bin/env python3
"""probe_excl.py SCRIPT OUT.json [--seconds S]

Run one brbox script on the ORIGINAL BRGlide.dll with extra code hooks and
record what the two EXCLUDED functions depend on:

  calls        entries of 0x100221D0 (LitDecal), 0x10023360 (NoZLit), and
               their only install sites 0x1001E8C0 (decal selector write) and
               0x1001FE9E (no-Z lit install); 0x10023110 (unlit no-Z twin) as
               a positive control
  geomodes     distinct geometry-mode words 0x105D17C8 seen at the vertex-
               routine selector 0x1001FD70, with counts
  combines     distinct (w0, w1) pairs passed to BrGlSetCombine 0x1001E7A0
  content      distinct (gameMode, cinematic, track, car, weather) per frame
"""
import json, os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import brbox, brbox_drive
if os.environ.get('PROBE_SAVES'):
    brbox_drive.SAVES = os.environ['PROBE_SAVES']
from unicorn import UC_HOOK_CODE
brbox_drive.KEYS['RSHIFT'] = (0xA1, 0x36); brbox_drive.KEYS['DELETE'] = (0x2E, 0xD3); brbox_drive.KEYS['RCTRL'] = (0xA3, 0x9D)
brbox_drive.KEYS.update({'END': (0x23, 0xCF), 'PGDN': (0x22, 0xD1), 'INSERT': (0x2D, 0xD2), 'HOME': (0x24, 0xC7), 'PGUP': (0x21, 0xC9), 'KP5': (0x65, 0x4C), 'KP7': (0x67, 0x47), 'KP9': (0x69, 0x49), 'KP1': (0x61, 0x4F), 'KP3': (0x63, 0x51), 'KP0': (0x60, 0x52)})
from unicorn.x86_const import UC_X86_REG_ESP

CALLS = {0x100221D0: 'LitDecal', 0x10023360: 'NoZLit', 0x1001E8C0: 'decal_select',
         0x1001FE9E: 'nozlit_install', 0x10023110: 'NoZUnlit(control)'}
SEL, COMB = 0x1001FD70, 0x1001E7A0
GEO = 0x105D17C8
CONTENT = (('mode', 0x100A9360), ('cine', 0x105BC760), ('track', 0x100B3014),
           ('car', 0x10226E7C), ('weather', 0x10226E80))


def main():
    script, out = sys.argv[1], sys.argv[2]
    secs = float(sys.argv[sys.argv.index('--seconds') + 1]) if '--seconds' in sys.argv else None
    logf = open(out + '.log', 'w')
    log = lambda m: (logf.write(m + '\n'), logf.flush())
    box = brbox_drive.make_box(log=log, trace_imports=False)
    drv = brbox_drive.Driver(brbox_drive.parse_script(script), log=log)
    brbox_drive.attach(box, drv)
    calls = {v: 0 for v in CALLS.values()}
    first = {}
    geos, combs, content, seq = {}, {}, {}, []

    def on_call(uc, addr, size, _):
        n = CALLS[addr]
        calls[n] += 1
        first.setdefault(n, box.hs.frame)
    for va in CALLS:
        box.uc.hook_add(UC_HOOK_CODE, on_call, begin=va, end=va)

    def on_sel(uc, addr, size, _):
        g = box.rd32(GEO)
        geos[g] = geos.get(g, 0) + 1
    box.uc.hook_add(UC_HOOK_CODE, on_sel, begin=SEL, end=SEL)

    def on_comb(uc, addr, size, _):
        sp = uc.reg_read(UC_X86_REG_ESP)
        k = '%08X %08X' % (box.rd32(sp + 4), box.rd32(sp + 8))
        combs[k] = combs.get(k, 0) + 1
    box.uc.hook_add(UC_HOOK_CODE, on_comb, begin=COMB, end=COMB)

    t3 = {int(l.split()[0], 16): l.split()[1] for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gap', 't3list.txt'))}
    t3hits = {}
    def on_t3(uc, addr, size, _):
        t3hits[addr] = t3hits.get(addr, 0) + 1
    for va in t3:
        box.uc.hook_add(UC_HOOK_CODE, on_t3, begin=va, end=va)

    prev = box.on_frame
    def on_frame(b):
        k = ' '.join('%s=%d' % (n, b.rd32(a)) for n, a in CONTENT)
        content[k] = content.get(k, 0) + 1
        if not seq or seq[-1][1] != k:
            seq.append((b.hs.frame, k))
        if prev: prev(b)
    box.on_frame = on_frame

    net = brbox_drive.start_peer_if_any(box, drv, script, log)
    t0 = time.time(); end = 'running'
    try:
        box.boot()
        end = 'RallyMain returned %d' % box.rally_main(cmdline=os.environ.get('PROBE_CMDLINE', ''), budget_s=secs)
    except brbox.Stop as e:
        end = 'stopped: %s' % e
    except brbox.GuestFault as e:
        end = 'FAULT: %s' % e
    if net is not None:
        net[2].stop(); net[0].join(30)
    lz = lambda g: bool(g & 0x20000) and not (g & 1)
    json.dump({'script': os.path.basename(script), 'end': end, 'frames': box.hs.frame,
               'wall': round(time.time() - t0, 1), 'calls': calls, 'first_frame': first,
               'geomodes': {'%08X' % g: c for g, c in sorted(geos.items())},
               'lit_without_z': sum(c for g, c in geos.items() if lz(g)),
               'combines': combs, 'content': content, 'seq': seq, 't3': {'%08X' % a: n for a, n in sorted(t3hits.items())}}, open(out, 'w'), indent=1)


main()
