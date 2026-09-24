"""brbox_drive.py -- drive the boxed game with a scripted input timeline.

A script is a list of steps run in order, one frame-hook at a time (a "frame"
is one entry to BrAppFrame 0x1001CF80, the main loop's per-frame call):

    # comment
    sleep 30              let 30 frames pass
    press RETURN          key down for 2 frames, then up (both VK and DIK views)
    press DOWN 4          ... held 4 frames
    hold SPACE / release SPACE
    wait 0x105BC740 != 0 [600]
                          block until the dword at that address satisfies the
                          test; fail the run after N frames (default 1800)
    waitb ADDR == V       same, byte-sized
    mouse X Y             move the pointer to (X, Y): slam it into the top-left
                          corner, then move by (X, Y) the next frame
    click [N]             mouse button 0 down for N frames (default 2), then up
    autopilot on|off      steer the player's car along the racing line with the
                          arrow keys (reads the car's own waypoint cursor; only
                          keyboard input is ever injected)
    waittext TEXT [N]     block until a string drawn this frame contains TEXT
                          (case-insensitive; underscores match spaces)
    text                  log every string the last frame drew, with its pen
    shot NAME             write the framebuffer to <shots>/NAME.png
    mark NAME             coverage checkpoint: record frame + state
    joystick plain|ffb    (applied before boot) attach a wheel, with or without
                          force feedback; the arrow keys steer it
    peer SCRIPT           (applied before boot) run a second game on a virtual
                          network with this one, driven by SCRIPT (a path
                          relative to this script); the two meet every frame
                          (tools/brbox_net.py)
    files NAME            (anywhere in the script; applied before boot) start
                          with the saved files in tools/brbox_saves/NAME/ --
                          the disc tree plus what an earlier script saved
    savefiles NAME        write every file the game has written so far to
                          build/brbox/saves/NAME/ (promote it to
                          tools/brbox_saves/ to make it a fixture)
    end                   stop the run successfully

Every run logs which game states it reached (STATE_VARS below sampled every
frame), so coverage is a measured number, not a claim.
"""
from __future__ import print_function

import json
import os
import struct
import sys
import time
import zlib

import brbox
from brbox import Box, GuestFault, Stop

APP_FRAME = 0x1001CF80          # BrAppFrame: one call per main-loop frame
TEXT_EMIT = 0x10015B10          # BrTextEmitString(psz): the engine's only text path
FONT_X, FONT_Y = 0x104ABB28, 0x104ABB2C
FRAME_MS = 1000.0 / 30

# name -> (VK, DIK scan code)
KEYS = {
    'RETURN': (0x0D, 0x1C), 'ENTER': (0x0D, 0x1C), 'SPACE': (0x20, 0x39),
    'ESCAPE': (0x1B, 0x01), 'ESC': (0x1B, 0x01), 'TAB': (0x09, 0x0F),
    'BACK': (0x08, 0x0E), 'UP': (0x26, 0xC8), 'DOWN': (0x28, 0xD0),
    'LEFT': (0x25, 0xCB), 'RIGHT': (0x27, 0xCD), 'LSHIFT': (0x10, 0x2A),
    'LCTRL': (0x11, 0x1D), 'LALT': (0x12, 0x38),
    'F1': (0x70, 0x3B), 'F2': (0x71, 0x3C), 'F3': (0x72, 0x3D), 'F4': (0x73, 0x3E),
    'F5': (0x74, 0x3F), 'F6': (0x75, 0x40), 'F7': (0x76, 0x41), 'F8': (0x77, 0x42),
    'F9': (0x78, 0x43), 'F10': (0x79, 0x44),
}
_DIK_ROW = {'QWERTYUIOP': 0x10, 'ASDFGHJKL': 0x1E, 'ZXCVBNM': 0x2C}
for _row, _base in _DIK_ROW.items():
    for _i, _c in enumerate(_row):
        KEYS[_c] = (ord(_c), _base + _i)
for _i, _c in enumerate('1234567890'):
    KEYS[_c] = (ord(_c), 0x02 + _i)

# Game-state words sampled every frame for coverage.  Named from the tree.
STATE_VARS = {
    'step_fn': 0x106E79F4,       # g_pfnStep: the current activity (br_gamestep.c)
    'game_mode': 0x100A9360,     # g_brCfgGameMode
    'race_lights': 0x105BC8F8,   # g_brRaceLights: the start-light state machine
    'race_script': 0x105BC750,   # g_brRaceScript: index into the light script
    'race_paused': 0x105CCB5C,   # g_brRacePaused
}


class Driver(object):
    def __init__(self, steps, shots=None, frames=0, log=print):
        self.steps = steps
        self.pc = 0
        self.shots = shots
        self.max_frames = frames
        self.log = log
        self.sleep_until = 0
        self.releases = []          # (frame, key)
        self.wait_since = None
        self.marks = []
        self.states = {}
        self.done = False
        self.fb = bytearray(640 * 480 * 3)
        self.lfb = []
        self.text_now = []          # (x, y, string) drawn during this frame
        self.text_last = []         # ... during the previous frame
        self.seen_text = {}         # string -> first frame it was drawn
        self.mouse_phase = 0
        self.raster = None
        self.autopilot = False
        self.ap_log = []
        self.ap_stuck = 0
        self.ap_reverse = 0

    # -------------------------------------------------------------- input --
    def key(self, box, name, down):
        import brbox_imports as I
        vk, dik = KEYS[name]
        hs = box.hs
        if down:
            hs.keys.add(vk)
            hs.dikeys.add(dik)
            I.post(box, hs.focus, I.WM_KEYDOWN, vk, 1 | (dik << 16))
        else:
            hs.keys.discard(vk)
            hs.dikeys.discard(dik)
            I.post(box, hs.focus, I.WM_KEYUP, vk, 1 | (dik << 16) | 0xC0000000)

    # --------------------------------------------------------- per frame --
    def on_text(self, box, psz):
        t = box.cstr(psz, 256) or ''
        self.text_now.append((box.rd32(FONT_X), box.rd32(FONT_Y), t))
        self.seen_text.setdefault(t, box.hs.frame)

    def screen_text(self):
        return [t for _x, _y, t in self.text_last]

    def on_frame(self, box):
        f = box.hs.frame
        for rel in [r for r in self.releases if r[0] <= f]:
            if rel[1] == '@mouse0':
                box.hs.mouse_btn.discard(0)
            else:
                self.key(box, rel[1], False)
            self.releases.remove(rel)
        for name, va in STATE_VARS.items():
            v = box.rd32(va)
            self.states.setdefault(name, {}).setdefault(v, f)
        if self.max_frames and f >= self.max_frames:
            raise Stop('frame budget %d reached' % self.max_frames)
        if self.autopilot:
            self.steer(box)
        while self.pc < len(self.steps) and f >= self.sleep_until:
            op, args, line = self.steps[self.pc]
            if op == 'sleep':
                self.sleep_until = f + int(args[0])
            elif op == 'press':
                n = int(args[1]) if len(args) > 1 else 2
                self.key(box, args[0].upper(), True)
                self.releases.append((f + n, args[0].upper()))
                self.sleep_until = f + n + 1
            elif op == 'mouse':
                if self.mouse_phase == 0:
                    box.hs.mouse[0] -= 100000
                    box.hs.mouse[1] -= 100000
                    self.mouse_phase = 1
                    return
                box.hs.mouse[0] += int(args[0])
                box.hs.mouse[1] += int(args[1])
                self.mouse_phase = 0
                self.sleep_until = f + 2
            elif op == 'click':
                n = int(args[0]) if args else 2
                box.hs.mouse_btn.add(0)
                self.releases.append((f + n, '@mouse0'))
                self.sleep_until = f + n + 1
            elif op == 'autopilot':
                self.autopilot = args[0].lower() == 'on'
                if not self.autopilot:
                    for k in ('UP', 'LEFT', 'RIGHT', 'DOWN'):
                        if KEYS[k][1] in box.hs.dikeys:
                            self.key(box, k, False)
            elif op == 'hold':
                self.key(box, args[0].upper(), True)
            elif op == 'release':
                self.key(box, args[0].upper(), False)
            elif op in ('wait', 'waitb'):
                if not self._test(box, op, args):
                    if self.wait_since is None:
                        self.wait_since = f
                    lim = int(args[3]) if len(args) > 3 else 1800
                    if f - self.wait_since > lim:
                        raise GuestFault('script line %d timed out: %s' % (line, ' '.join(args)))
                    return
                self.wait_since = None
            elif op == 'waittext':
                want = args[0].replace('_', ' ').lower()
                if not any(want in t.lower() for t in self.screen_text()):
                    if self.wait_since is None:
                        self.wait_since = f
                    lim = int(args[1]) if len(args) > 1 else 1800
                    if f - self.wait_since > lim:
                        raise GuestFault('script line %d: text %r never drawn; last frame drew %r'
                                         % (line, args[0], self.screen_text()[:40]))
                    return
                self.wait_since = None
            elif op == 'text':
                self.log('frame %d text: %s' % (f, ' | '.join(
                    '%s@%d,%d' % (t, x, y) for x, y, t in self.text_last)))
            elif op == 'shot':
                self.shot(box, args[0])
            elif op in ('files', 'peer', 'joystick'):
                pass                                    # applied before boot
            elif op == 'savefiles':
                save_files(box, os.path.join(brbox.ROOT, 'build', 'brbox', 'saves', args[0]))
                self.log('saved %d file(s) as %s' % (len(box.hs.written), args[0]))
            elif op == 'mark':
                self.marks.append((args[0], f, int(box.hs.ms)))
                self.log('mark %s at frame %d' % (args[0], f))
            elif op == 'end':
                self.done = True
                raise Stop('script end')
            self.pc += 1

    # ------------------------------------------------------------ autopilot --
    ENTRANTS = 0x10AF0858            # driver slots, 0x80 bytes, car pointer first

    def steer(self, box):
        """Hold UP, and LEFT/RIGHT toward a waypoint ~lookahead metres down
        the car's own racing-line cursor (car+0xF8C node, +0xF90 point) --
        the walk BrCtlAiBody does for the computer cars."""
        import math
        car = box.rd32(self.ENTRANTS)
        want = {'UP'}
        if car:
            node, i = box.rd32(car + 0xF8C), box.rd32(car + 0xF90)
            f = lambda o: struct.unpack('<f', box.rd(car + o, 4))[0]
            pos = (f(0x30), f(0x34))
            right = (f(0x10), f(0x14))
            vel = (f(0x1024), f(0x1028))
            speed = math.hypot(*vel)
            if node:
                t = 12.0 + speed * 0.6
                n, k = node, i
                for _ in range(400):
                    cnt = box.rd16(n + 0x14)
                    a0 = struct.unpack('<f', box.rd(n + 0x40 + 0x28 * k + 0x24, 4))[0]
                    a1 = struct.unpack('<f', box.rd(n + 0x40 + 0x28 * (k + 1) + 0x24, 4))[0]
                    t -= a0 - a1
                    k += 1
                    if k >= cnt:
                        n = box.rd32(n)
                        guard = 0
                        while box.rd16(n + 0x16) & 1 and guard < 16:
                            n = box.rd32(n + 4)
                            guard += 1
                        k = 0
                    if t < 0:
                        break
                c = n + 0x40 + 0x28 * k + 0x0C
                tx, ty = struct.unpack('<ff', box.rd(c, 8))
                dx, dy = tx - pos[0], ty - pos[1]
                d = math.hypot(dx, dy) or 1.0
                lat = (right[0] * dx + right[1] * dy) / d
                if lat > 0.08:
                    want.add(self.ap_right)
                elif lat < -0.08:
                    want.add(self.ap_left)
                if abs(lat) > 0.6 and speed > 25:
                    want.discard('UP')
                # stuck against something: back off with the wheel reversed
                if speed < 2.0:
                    self.ap_stuck += 1
                else:
                    self.ap_stuck = 0
                if self.ap_stuck > 45 or self.ap_reverse > 0:
                    if self.ap_reverse == 0:
                        self.ap_reverse = 40
                    self.ap_reverse -= 1
                    self.ap_stuck = 0
                    flip = {self.ap_left: self.ap_right, self.ap_right: self.ap_left}
                    want = {'DOWN'} | {flip[k] for k in want if k in flip}
                self.ap_log.append((box.hs.frame, round(pos[0], 1), round(pos[1], 1),
                                    round(speed, 1), round(lat, 2)))
        for k in ('UP', 'DOWN', 'LEFT', 'RIGHT'):
            down = KEYS[k][1] in box.hs.dikeys
            if (k in want) != down:
                self.key(box, k, k in want)

    # car+0x10 (frame row 1) points to the car's LEFT: a target on its
    # positive side is steered toward with the LEFT arrow
    ap_right, ap_left = 'LEFT', 'RIGHT'

    def _test(self, box, op, args):
        a = int(args[0], 0)
        v = box.rd32(a) if op == 'wait' else box.rd8(a)
        want = int(args[2], 0)
        return {'==': v == want, '!=': v != want, '>=': v >= want, '<=': v <= want,
                '>': v > want, '<': v < want}[args[1]]

    def pump(self, box):
        pass

    def idle(self, box):
        pass

    def next_event_ms(self, box):
        return None

    # -------------------------------------------------------- screenshots --
    def on_lfb(self, box, a):
        # grLfbWriteRegion(dst, x, y, fmt, w, h, stride, data): keep the raw
        # rows; they are only converted when a SHOT asks for a picture.
        x, y, fmt, w, h, stride, data = a[1], a[2], a[3], a[4], a[5], a[6], a[7]
        if fmt not in (0, 1) or h == 0:
            return
        self.lfb.append((x, y, fmt, w, h, stride, box.rd(data, stride * h)))
        if len(self.lfb) > 64:
            del self.lfb[:-64]

    def _compose(self):
        fb = self.fb
        for x, y, fmt, w, h, stride, raw in self.lfb:
            for r in range(h):
                yy = y + r
                if not (0 <= yy < 480):
                    continue
                row = raw[r * stride:r * stride + 2 * w]
                o = (yy * 640 + x) * 3
                for c in range(min(w, 640 - x)):
                    px = row[2 * c] | (row[2 * c + 1] << 8)
                    if fmt == 0:
                        fb[o] = (px >> 11) << 3
                        fb[o + 1] = ((px >> 5) & 0x3F) << 2
                    else:
                        fb[o] = ((px >> 10) & 0x1F) << 3
                        fb[o + 1] = ((px >> 5) & 0x1F) << 3
                    fb[o + 2] = (px & 0x1F) << 3
                    o += 3
        self.lfb = []

    def shot(self, box, name):
        if not self.shots:
            return
        os.makedirs(self.shots, exist_ok=True)
        path = os.path.join(self.shots, name + '.png')
        self._compose()
        img = bytes(self.fb)
        if self.raster is not None and self.raster.last:
            img = bytes(self.raster.render(img))
        write_png(path, 640, 480, img)
        self.log('shot %s (frame %d)' % (path, box.hs.frame))


def write_png(path, w, h, rgb):
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def parse_script(path):
    steps = []
    if not path:
        return steps
    for n, ln in enumerate(open(path), 1):
        ln = ln.split('#', 1)[0].strip()
        if not ln:
            continue
        parts = ln.split()
        steps.append((parts[0].lower(), parts[1:], n))
    return steps


def make_box(log=print, trace_imports=False, dll=None):
    box = Box(dll=dll or brbox.REF_DLL, log=log, trace_imports=trace_imports)
    return box


SAVES = os.path.join(brbox.ROOT, 'tools', 'brbox_saves')


def save_files(box, out):
    """Dump the run's written files, one host file per canonical guest path
    (c:\\bossrally\\x.brf -> <out>/c/bossrally/x.brf)."""
    for k, data in box.hs.written.items():
        rel = k.replace(':', '').replace('\\', '/')
        dst = os.path.join(out, *rel.split('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'wb') as fh:
            fh.write(bytes(data))


def load_files(box, name):
    base = os.path.join(SAVES, name)
    if not os.path.isdir(base):
        raise GuestFault('no save fixture %s' % base)
    for dp, _dn, fn in os.walk(base):
        for f in sorted(fn):
            rel = os.path.relpath(os.path.join(dp, f), base).split(os.sep)
            key = rel[0] + ':\\' + '\\'.join(rel[1:])
            box.hs.written[key.lower()] = bytearray(open(os.path.join(dp, f), 'rb').read())


def attach(box, driver):
    from unicorn import UC_HOOK_CODE
    box.driver = driver
    for op, args, _line in driver.steps:
        if op == 'files':
            load_files(box, args[0])
        elif op == 'joystick':
            box.hs.joystick = args[0]             # 'plain' or 'ffb'
    box.on_lfb = driver.on_lfb
    if driver.shots:
        import brbox_glraster
        driver.raster = brbox_glraster.Recorder()

    def on_glide(b, name, a):
        # a presented frame is one buffer swap: the text it drew is complete
        if name == '_grBufferSwap@4' and not b.subrun:
            driver.text_last, driver.text_now = driver.text_now, []
        if driver.raster is not None:
            driver.raster.on_glide(b, name, a)
    box.on_glide = on_glide

    def frame_hook(uc, addr, size, _ud):
        if box.subrun:
            # A frame boundary inside a live-oracle sub-run: the captured
            # function wraps a whole frame loop (a loader with a progress
            # screen).  Frames, virtual time and script input are driven here,
            # so a sub-run spanning one could not replay identically on both
            # sides -- and would shift the drive for everything after it.
            # The oracle rolls the capture back and lets the call run for real.
            box.fault = GuestFault('frame boundary inside a sub-run')
            uc.emu_stop()
            return
        box.hs.frame += 1
        # A machine that presents exactly 30 frames a second: each frame costs
        # 1/30 s of virtual time on top of whatever the game's own clock
        # queries cost.
        box.hs.ms += FRAME_MS
        try:
            driver.on_frame(box)
            if box.on_frame is not None:
                box.on_frame(box)
        except (GuestFault, Stop) as e:
            box.fault = e
            uc.emu_stop()
    box.uc.hook_add(UC_HOOK_CODE, frame_hook, begin=APP_FRAME, end=APP_FRAME)

    def text_hook(uc, addr, size, _ud):
        if not box.subrun:
            driver.on_text(box, box.rd32(uc.reg_read(brbox.UC_X86_REG_ESP) + 4))
    box.uc.hook_add(UC_HOOK_CODE, text_hook, begin=TEXT_EMIT, end=TEXT_EMIT)


def start_peer_if_any(box, driver, script, log=lambda m: None):
    """(thread, peer box, net, result) when the script names a peer."""
    import brbox_net
    ps = brbox_net.peer_script(driver.steps)
    if ps is None:
        return None
    path = ps if os.path.isabs(ps) else os.path.join(os.path.dirname(os.path.abspath(script)), ps)
    return brbox_net.start_peer(box, path, log=lambda m: log('[peer] ' + m), shots=driver.shots)


def run_cli(a):
    t0 = time.time()
    logf = open(os.path.join(brbox.ROOT, 'build', 'brbox', 'run.log'), 'w')

    def log(msg):
        logf.write(msg + '\n')
        logf.flush()
        if a.trace_imports is False and not msg.startswith('imp '):
            print(msg)
    box = make_box(log=log, trace_imports=a.trace_imports)
    if box.unmodelled:
        print('unmodelled imports: %s' % ', '.join(box.unmodelled))
    drv = Driver(parse_script(a.script), shots=a.shots, frames=a.frames, log=log)
    attach(box, drv)
    net = start_peer_if_any(box, drv, a.script, log)
    rc = 0
    try:
        box.boot()
        log('DllMain ok (%.1fs)' % (time.time() - t0))
        r = box.rally_main(budget_s=a.seconds or None)
        log('RallyMain returned %d' % r)
    except Stop as e:
        log('stopped: %s' % e)
    except GuestFault as e:
        log(str(e))
        rc = 1
    if net is not None:
        net[2].stop()
        net[0].join(30)
        log('peer: frames %d, %s' % (net[1].hs.frame, net[3].get('end', 'running')))
    dt = time.time() - t0
    log('frames %d, virtual %.1fs, wall %.1fs' % (box.hs.frame, box.hs.ms / 1000.0, dt))
    if box.hs.messageboxes:
        log('message boxes: %r' % box.hs.messageboxes)
    top = sorted(box.import_counts.items(), key=lambda kv: -kv[1])[:25]
    log('imports: ' + ', '.join('%s=%d' % (k.split('!')[-1], v) for k, v in top))
    if a.coverage:
        json.dump({'frames': box.hs.frame, 'marks': drv.marks,
                   'states': {k: {'%08X' % s: f for s, f in v.items()} for k, v in drv.states.items()},
                   'imports': dict(box.import_counts)}, open(a.coverage, 'w'), indent=1)
    return rc
