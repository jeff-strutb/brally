"""n64drive.py -- play Top Gear Rally toward a goal; write the input down as a script.

A n64box script is blind timed input: it goes wherever the timing happens to
land.  This plays the ORIGINAL ROM under n64box.py with a controller that
looks at the game every frame -- which menu is up and which row is lit, each
player's car-select step, where the car is and which way it points -- and
steers toward a plan: these menu rows, these cheats, this car with this
setup, then drive the race on a racing line.  Every pad state it sends is
recorded, and the recording IS the script (the box is deterministic, so
replaying it reproduces the run instruction for instruction; `verify`
proves that by comparing the RAM digest stream).

The racing line is the computer drivers' own path, read out of RAM at the
green light (a closed chain of segments from the car's +0xF5C).  The
autopilot chases a point a little ahead of the car along it (pure pursuit),
brakes into sharp turns and backs out when it is stuck.

Plans live in n64drive_plans.py; each writes build/n64/drive/<name>.txt (with
--install, n64/tools/n64box_scripts/<name>.txt)
with a header saying what the run covers and what the probe saw.

    .venv/bin/python n64/tools/n64drive.py show arc_desert_fog_car1         # play it, print the probe
    .venv/bin/python n64/tools/n64drive.py gen arc_desert_fog_car1          # one plan, to build/n64/drive
    .venv/bin/python n64/tools/n64drive.py gen --install arc_desert_fog_car1  # into the suite
    .venv/bin/python n64/tools/n64drive.py gen --all                       # every plan, in parallel
    .venv/bin/python n64/tools/n64drive.py verify arc_desert_fog_car1       # replay == recording
    .venv/bin/python n64/tools/n64drive.py list
"""
import argparse
import math
import os
import struct
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import n64box as NB  # noqa: E402
import n64probe as P  # noqa: E402
import orphanguard  # noqa: E402

SCRIPTS = os.path.join(HERE, 'n64box_scripts')
# generated scripts land here until `gen --install` puts them in the suite
# (anything in SCRIPTS is played by the A5/A7 oracle)
DRAFTS = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'build/n64/drive')

BITS = NB.Script.BITS
CAR0, CAR_STRIDE = 0x8031B760, 0x2090
RACE_TICK = 0x8020082C
CAR_SELECT = 0x8020D004
TITLE = 0x8020686C

TRACKS = ['Desert', 'Mountain', 'Coastline', 'Strip Mine', 'Jungle',
          'Mirror Desert', 'Mirror Mountain', 'Mirror Coastline', 'Mirror Strip Mine', 'Mirror Jungle']
WEATHER = ['Sunny', 'Fog', 'Rain', 'Snow', 'Night']
MAIN = dict(championship=0, arcade=1, timeattack=2, practice=3, paintshop=4, loadsave=5, options=6)
# car-select setup rows, in the order the screen asks for them
SETUP = [('handling', ['Type 1', 'Type 2', 'Type 3']),
         ('transmission', ['Manual', 'Automatic']),
         ('tires', ['Slippy', 'Normal', 'Grippy']),
         ('suspension', ['Softer', 'Normal', 'Harder'])]

# The cheat table (0x8028DF48), oldest press first; the matcher compares the
# history of held-button changes, so each press is released before the next.
CHEATS = {
    'cars':      'A DL DL CD A DR Z',              # cars 0-8
    'car9':      'DU DU Z B A DL DL',              # unlock bit 0x200
    'car10':     'DD A DR Z DR DU CD',             # 0x400
    'car11':     'CD DU B DR A CD A DR',           # 0x800
    'car12':     'B B A DL DL CD A DR',            # 0x1000
    'car15':     'DR DU DL CD CD A DR Z',          # 0x8000
    'tracks':    'A DL DL DR DD Z',                # tracks 0-4
    'mirrors':   'DR DU DL CD DR DD Z',            # tracks 5-9
    'filter':    'B DL DR DU DL Z DR',             # toggle 0x8028AA68 (texture filter)
    'fbfx':      'CD Z B DU DU DR',                # toggle 0x8028AA94 (frame-end effect)
    'demo2':     'CD DR DD Z',                     # toggle 0x8026FF20 (the other demo)
}
UNLOCK_ALL = ['cars', 'car9', 'car10', 'car11', 'car12', 'car15', 'tracks', 'mirrors']


class Fail(Exception):
    pass


def f32(b, va):
    return struct.unpack('>f', b.read(va, 4))[0]


class Driver(P.ProbeBox):
    """The box with a controller that plays a plan."""

    def __init__(self, plan, pads=1, pak=False, name='plan', rumble=False):
        super().__init__(None)
        self.pad.pads, self.pad.pak, self.pad.rumble = pads, pak, rumble
        self.name = name
        self.out = [(0, 0, 0)] * 2          # the pad state this frame, per port
        self.rec = [[], []]                 # (frame, b, x, y) whenever it changes
        self.decided = -1
        self.menu = (-1, None, -1)          # (frame, title, row) last seen
        self.mode = None                    # the last BrModeSet target
        self.mode_frame = 0
        self.notes = []                     # what the plan did, for the header
        self.gen = self.run_plan(plan)
        self.done = False
        self.hold_p = [(0, 0, 0), (0, 0, 0)]
        self.arg_n = {}                     # menu title -> rows

    # --------------------------------------------------------- observing
    def on_menu(self, uc, addr, size, data):
        super().on_menu(uc, addr, size, data)
        title, row = self.last_menu
        self.menu = (self.frame, title, row)
        self.arg_n[title] = self.arg(1)

    def on_mode(self, uc, addr, size, data):
        super().on_mode(uc, addr, size, data)
        self.mode, self.mode_frame = self.arg(0), self.frame

    def menu_up(self, title=None):
        f, t, r = self.menu
        return f >= self.frame - 2 and (title is None or (t and title.lower() in t.lower()))

    def car(self, p):
        return CAR0 + p * CAR_STRIDE

    def season(self):
        return self.r32(0x8031C5BC)

    # ---------------------------------------------------------- the pad
    def os_cont_get_read(self):
        if self.frame != self.decided:
            self.decided = self.frame
            want = [(0, 0, 0), (0, 0, 0)]
            if not self.done:
                try:
                    step = next(self.gen)
                except StopIteration:
                    self.done = True
                    self.end_frame = self.frame
                    self.stop_reason = 'plan done'
                    self.uc.emu_stop()
                    step = {}
                except Fail as e:
                    self.done = True
                    self.fault = 'plan: %s' % e
                    self.uc.emu_stop()
                    step = {}
                for port, v in (step or {}).items():
                    want[port] = v
            for port in range(2):
                if want[port] != self.out[port]:
                    self.out[port] = want[port]
                    self.rec[port].append((self.frame,) + want[port])
        out = self.arg(0)
        rec = b''
        for port in range(self.pad.pads):
            b, x, y = self.out[port]
            rec += struct.pack('>HbbBx', b, x, y, 0)
        self.write(out, rec + b'\x00\x00\x00\x00\x08\x00' * (4 - self.pad.pads))
        self.ret()

    # ------------------------------------------------------ plan pieces
    @staticmethod
    def btn(names):
        b = 0
        for k in names.split('+'):
            if k and k != '-':
                b |= BITS[k]
        return b

    def idle(self, n):
        for _ in range(n):
            yield {}

    def tap(self, names, port=0, hold=4, gap=6, x=0, y=0):
        for _ in range(hold):
            yield {port: (self.btn(names), x, y)}
        yield from self.idle(gap)

    def wait_for(self, cond, timeout, what):
        t0 = self.frame
        while not cond():
            if self.frame - t0 > timeout:
                raise Fail('%s: not reached in %d frames (menu %r, mode %08X)' % (
                    what, timeout, self.menu[1:], self.mode or 0))
            yield {}

    def do_menu(self, title, row, confirm='A', timeout=900):
        """Bring the lit row of the menu titled `title` to `row` (the shorter
        way round, stepping on the stick's x axis), then confirm."""
        yield from self.wait_for(lambda: self.menu_up(title), timeout, 'menu %s' % title)
        yield from self.idle(8)
        name = self.menu[1]
        n = self.arg_n.get(name, 0) or 1
        steps = 0
        while self.menu[2] != row:
            before = self.menu[2]
            fwd = (row - before) % n
            yield from self.tap('-', x=80 if fwd <= n - fwd else -80, hold=4, gap=2)
            t0 = self.frame
            while self.menu[2] == before and self.frame - t0 < 60:
                yield {}
            yield from self.idle(10)
            steps += 1
            if steps > 2 * n + 2:
                raise Fail('menu %s: row %d cannot be chosen' % (title, row))
        yield from self.idle(6)
        if confirm:
            # a screen still running its opening ignores the press: try again
            left = self.frame
            gone = lambda: not self.menu_up(title) or self.mode_frame >= left  # noqa: E731
            for _ in range(4):
                yield from self.tap(confirm)
                t0 = self.frame
                while not gone() and self.frame - t0 < 90:
                    yield {}
                if gone():
                    break
            else:
                raise Fail('menu %s: row %d not taken' % (title, row))
        self.notes.append('%s: row %d' % (name, row))

    def do_cheats(self, names):
        """Type cheats on the title screen (the matcher runs on every pad read)."""
        for n in names:
            yield from self.idle(4)
            for k in CHEATS[n].split():
                yield from self.tap(k, hold=4, gap=4)
        self.notes.append('cheats: ' + ' '.join(names))

    def do_unlock(self, names):
        """The title screen and the menus act on A, B, Z and the d-pad, so the
        cheats are typed where those are harmless: a time-attack race (the
        default track and car, setup kept), then pause -> exit to the menu."""
        yield from self.do_menu('TOP GEAR', MAIN['timeattack'])
        yield from self.do_menu('TRACK SELECT', 2)
        yield from self.do_cars([dict(car=1)])
        yield from self.idle(20)
        yield from self.do_cheats(names)
        yield from self.do_pause_exit()

    def do_pause_exit(self):
        """Pause, walk up to the last row (exit to the main menu), take it."""
        yield from self.tap('START', gap=30)
        yield from self.tap('DU', gap=20)
        yield from self.tap('START')
        yield from self.wait_for(lambda: self.mode == 0x802111E0, 600, 'back at the main menu')

    PAINT = 0x80243260

    def paint_return(self):
        """Back into the paint shop after a press that left it."""
        yield from self.wait_for(lambda: self.mode in (self.PAINT, 0x802111E0, CAR_SELECT, TITLE),
                                 1500, 'back to the paint shop')
        if self.mode == TITLE:                  # the shop's exit goes to the title
            yield from self.idle(60)
            yield from self.tap('START')
            yield from self.wait_for(lambda: self.mode == 0x802111E0, 900, 'main menu')
        if self.mode == 0x802111E0:
            yield from self.do_menu('TOP GEAR', MAIN['paintshop'])
        if self.mode == CAR_SELECT:
            yield from self.idle(40)
            yield from self.tap('A', gap=30)
        yield from self.wait_for(lambda: self.mode == self.PAINT, 900, 'paint shop')
        yield from self.idle(60)
        self.paint_exits += 1

    def cursor_to(self, tx, ty, hold=0):
        """Steer the paint cursor (0x8028D12C, 0x8028D130) to a point, holding
        `hold` buttons on the way (a stroke when it is A)."""
        last, still = None, 0
        for _ in range(150):
            cx, cy = self.s32(0x8028D12C), self.s32(0x8028D130)
            dx, dy = tx - cx, ty - cy
            if abs(dx) <= 3 and abs(dy) <= 3:
                return
            still = still + 1 if (cx, cy) == last else 0
            last = (cx, cy)
            if still > 12 or self.mode != self.PAINT:
                return
            sx = 0 if abs(dx) <= 3 else int(math.copysign(min(80, max(28, abs(dx) * 2)), dx))
            sy = 0 if abs(dy) <= 3 else int(math.copysign(min(80, max(28, abs(dy) * 2)), -dy))
            yield {0: (hold, sx, sy)}

    def do_paint_sweep(self, seed=1, moves=300, budget=16000):
        """The paint shop explored: a seeded random walk of the cursor over the
        screen; half the moves hold A (strokes), each stop presses A, and now
        and then B, Z, L, R or a C button.  A press that leaves the shop is
        followed back in (main menu row 4, car select A)."""
        import random
        rng = random.Random(seed)
        t0, n = self.frame, 0
        self.paint_exits = 0
        other = ['Z', 'CU', 'CD', 'CL', 'CR', 'L', 'R', 'B']
        while n < moves and self.frame - t0 < budget:
            if self.mode != self.PAINT:
                yield from self.paint_return()
            tx, ty = rng.randrange(24, 616), rng.randrange(24, 456)   # hi-res: 640 x 480
            yield from self.cursor_to(tx, ty, BITS['A'] if rng.random() < 0.5 else 0)
            yield from self.idle(2)
            r = rng.random()
            yield from self.tap('A' if r < 0.75 else rng.choice(other), hold=4, gap=20)
            n += 1
        self.notes.append('paint shop: %d random moves (seed %d), left and re-entered %d times'
                          % (n, seed, self.paint_exits))

    def car_state(self, p):
        return (self.s32(0x803162B0 + 4 * p),            # step: 0 car, 1-5 setup, 6 ready
                f32(self, 0x80316260 + 4 * p),            # turning (input ignored)
                self.s32(self.car(p) + 0x2058),           # the car shown
                self.s32(0x80316278 + 4 * p))             # the setup row shown

    def do_cars(self, specs, timeout=3000):
        """Car select for every player at once.  spec: dict(car=N, setup=None
        (START: keep the setup) or {handling,transmission,tires,suspension}
        row numbers, decal=row)."""
        yield from self.wait_for(lambda: self.mode == CAR_SELECT, timeout, 'car select')
        self.car_specs = specs
        yield from self.idle(30)
        t0 = self.frame
        busy = [0] * len(specs)
        while True:
            if self.mode == RACE_TICK:
                break
            if self.frame - t0 > timeout:
                raise Fail('car select: stuck at %r' % [self.car_state(p) for p in range(len(specs))])
            step = {}
            for p, spec in enumerate(specs):
                if busy[p] > 0:
                    busy[p] -= 1
                    if busy[p] > 6:
                        step[p] = self.hold_p[p]
                    continue
                stage, turning, shown, row = self.car_state(p)
                if stage == 6 or turning != 0.0:
                    continue
                if stage == 0:
                    if shown != spec['car']:
                        self.hold_p[p] = (0, 80, 0)
                    else:
                        self.hold_p[p] = (BITS['START'] if spec.get('setup') is None else BITS['A'], 0, 0)
                else:
                    if stage <= 4:
                        want = (spec.get('setup') or {}).get(SETUP[stage - 1][0], row)
                    else:
                        want = spec.get('decal', row)
                    self.hold_p[p] = (0, 80, 0) if row != want else (BITS['A'], 0, 0)
                busy[p] = 6 + 12
                step[p] = self.hold_p[p]
            yield step
        for p, spec in enumerate(specs):
            s = 'P%d car %d' % (p + 1, spec['car'])
            if spec.get('setup'):
                s += ' ' + ' '.join('%s=%s' % (k, dict(SETUP)[k][v]) for k, v in spec['setup'].items())
            self.notes.append(s)

    # ------------------------------------------------------------ racing
    def car_pose(self, p):
        c = self.car(p)
        pos = struct.unpack('>3f', self.read(c + 0x1C0, 12))
        vel = struct.unpack('>3f', self.read(c + 0x1CC, 12))
        fwd = struct.unpack('>3f', self.read(c + 0x204, 12))
        return pos, vel, fwd

    def load_trail(self, p=0):
        """The track's racing line: the computer drivers' path, a closed chain
        of segments (next at +0, point count at +0x14, points of 0x28 bytes
        from +0x4C, position first) reached from the car's current segment
        (+0xF5C)."""
        seg0 = seg = self.r32(self.car(p) + 0xF5C)
        pts, seen = [], set()
        while seg and seg not in seen:
            seen.add(seg)
            n = struct.unpack('>H', self.read(seg + 0x14, 2))[0]
            for i in range(n):
                pts.append(struct.unpack('>3f', self.read(seg + 0x4C + 0x28 * i, 12)))
            seg = self.r32(seg)
        if seg != seg0 or len(pts) < 20:
            raise Fail('no closed racing line from segment %08X' % seg0)
        # evenly spaced, ~4 units apart
        out = [pts[0]]
        for k in range(1, len(pts) + 1):
            a, b = pts[k - 1], pts[k % len(pts)]
            d = math.hypot(b[0] - a[0], b[1] - a[1])
            for j in range(1, max(1, int(d / 4.0)) + 1):
                t = j / max(1, int(d / 4.0))
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t))
        return out[:-1]

    MIRROR = -1

    def pilot(self, p, trail, st):
        """Pure pursuit along the closed racing line -> (buttons, x, y): chase
        a point further ahead the faster the car goes, lift and brake into
        sharp turns, and back out (accelerator + stick down) when the car has
        stopped making progress along the line."""
        pos, vel, fwd = self.car_pose(p)
        n = len(trail)
        speed = math.hypot(vel[0], vel[1])

        def d2(k):
            q = trail[k % n]
            return (q[0] - pos[0]) ** 2 + (q[1] - pos[1]) ** 2
        i = st.get('i')
        if i is None or d2(i) > 60.0 ** 2:
            i = min(range(n), key=d2)
        else:
            i = min(range(i - 5, i + 80), key=d2) % n
            step = (i - st['i']) % n
            st['prog'] = st.get('prog', 0) + (step if step < n // 2 else step - n)
        off = math.sqrt(d2(i))
        st['i'] = i
        look = max(3, int((12.0 + 0.35 * speed) / 4.0)) if off < 25 else 2
        tgt = trail[(i + look) % n]
        # after a stall, take the line a few units to one side for a while
        # (the sides alternate and widen with each stall at the same place)
        lane = 0.0
        if st.get('lane_until') is not None and (st['lane_until'] - i) % n < n // 2:
            a0, a1 = trail[(i + look) % n], trail[(i + look + 1) % n]
            dx, dy = a1[0] - a0[0], a1[1] - a0[1]
            dl = math.hypot(dx, dy) or 1.0
            lane = st['lane']
            tgt = (tgt[0] - dy / dl * lane, tgt[1] + dx / dl * lane, tgt[2])
        head = math.atan2(fwd[1], fwd[0])
        err = math.atan2(tgt[1] - pos[1], tgt[0] - pos[0]) - head
        err = (err + math.pi) % (2 * math.pi) - math.pi
        if self.s32(0x8028A8AC):                # a mirrored track steers the other way
            err = self.MIRROR * err
        # progress: once the race clock runs, the index should keep moving
        st['t'] = st.get('t', 0) + 1
        going = f32(self, self.car(p) + 0xF80) > 1.0
        if st.get('rev', 0) > 0:
            st['rev'] -= 1
            if st['rev'] == 0:
                st['mark'], st['calm'] = (st['t'], i), 60
            return self.reverse(p, int(max(-80, min(80, 150 * err))))
        mt, mi = st.setdefault('mark', (st['t'], i))
        if not going or ((i - mi) % n > 4 and (i - mi) % n < n // 2):
            st['mark'] = (st['t'], i)
        elif st['t'] - mt > 75:
            st['rev'] = 90
            k = st['stalls'] = st.get('stalls', 0) + 1 if abs(i - st.get('stall_i', -999)) < 40 else 1
            st['stall_i'] = i
            st['lane'] = (1 if k % 2 else -1) * (7.0 + 5.0 * ((k - 1) // 2 % 3))
            st['lane_until'] = (i + 60) % n
            return self.reverse(p, 0)
        x = int(max(-80, min(80, -150 * err)))
        # the bends coming up within about a second and a half of travel:
        # the most the line turns away from where the car is heading
        reach = int((20 + 1.5 * speed) / 4.0)
        bend = 0.0
        for k in range(look, reach + 1, 2):
            a0, a1 = trail[(i + k) % n], trail[(i + k + 2) % n]
            dv = math.atan2(a1[1] - a0[1], a1[0] - a0[0]) - head
            bend = max(bend, abs((dv + math.pi) % (2 * math.pi) - math.pi))
        want = 90 if bend < 0.45 else 55 if bend < 0.8 else 38 if bend < 1.3 else 26
        if st.get('calm', 0) > 0:               # just backed out: ease away
            st['calm'] -= 1
            want = min(want, 25)
        pedal = 'gas'
        if speed > want + 8 or (abs(err) > 0.6 and speed > 25):
            pedal = 'brake'
        elif speed > want:
            pedal = 'coast'
        return self.controls(p, st, pedal, x, speed)

    def reverse(self, p, x):
        kind = self.read(0x8036A8E0 + 0x15C * p + 0x25, 1)[0]
        if kind == 3:
            return 0, x, -80
        if kind == 2:
            return BITS['A'] | (BITS['DL'] if x < -20 else BITS['DR'] if x > 20 else 0) | BITS['DD'], 0, -80
        return BITS['A'], x, -80

    # The controller types (the pad record's +0x25, set on the Controller
    # screen) map the pad differently (0x80255120): A and B/wheel take A to
    # accelerate and B to brake with the stick steering; C steers on the
    # d-pad; D accelerates with the stick pushed up and brakes on Z.
    def controls(self, p, st, pedal, x, speed):
        kind = self.read(0x8036A8E0 + 0x15C * p + 0x25, 1)[0]
        b, y = 0, 0
        if kind == 3:
            y = 80 if pedal == 'gas' else 0
            b = BITS['Z'] if pedal == 'brake' else 0
        else:
            b = BITS['A'] if pedal == 'gas' else BITS['B'] if pedal == 'brake' else 0
        if kind == 2:
            b |= BITS['DL'] if x < -20 else BITS['DR'] if x > 20 else 0
            x = 0
        # a manual gearbox: up on R above 16 units a gear, down on Z below it
        if st.get('manual'):
            g = st.setdefault('gear', 1)
            if st.get('shift', 0) > 0:
                st['shift'] -= 1
                if st['shift'] >= 6:
                    b |= BITS[st['shift_btn']]
            elif g < 5 and speed > 16 * g + 4:
                st['gear'], st['shift'], st['shift_btn'] = g + 1, 12, 'R'
            elif g > 1 and speed < 16 * (g - 1) - 4:
                st['gear'], st['shift'], st['shift_btn'] = g - 1, 12, 'Z' if kind != 3 else 'L'
        return b, x, y

    def do_race(self, frames=None, drivers=('auto',), extras=(), until_mode_leaves=False, manual=None,
                until_finish=False):
        """Drive the race.  drivers: per port 'auto' | 'idle' | 'sweep'.
        extras: (frame offset from the green light, port, buttons, hold)."""
        yield from self.wait_for(lambda: self.mode == RACE_TICK, 3000, 'race start')
        yield from self.idle(10)
        trail = self.load_trail() if 'auto' in drivers else None
        if manual is None:
            manual = [(sp.get('setup') or {}).get('transmission') == 0
                      for sp in getattr(self, 'car_specs', [])]
        manual = list(manual) + [False] * (len(drivers) - len(manual))
        st = [{'manual': m} for m in manual]
        t0 = self.frame
        ex = sorted(extras)
        k = 0
        while frames is None or self.frame - t0 < frames:
            if until_mode_leaves and self.mode != RACE_TICK:
                break
            if until_finish and self.s32(self.car(0) + 0xF78) >= self.s32(0x8028B304):
                break
            t = self.frame - t0
            step = {}
            for p, d in enumerate(drivers):
                if d == 'auto':
                    step[p] = self.pilot(p, trail, st[p])
                elif d == 'sweep':
                    step[p] = (BITS['A'], int(60 * math.sin(t / 25.0)), 0)
                else:
                    step[p] = (0, 0, 0)
            for off, port, names, hold in ex:
                if off <= t < off + hold:
                    b, x, y = step.get(port, (0, 0, 0))
                    if names.startswith('only:'):
                        step[port] = (self.btn(names[5:]), 0, 0)
                    else:
                        step[port] = (b | self.btn(names), x, y)
            yield step
        self.notes.append('race %d frames%s%s; %s' % (
            self.frame - t0, '' if self.mode == RACE_TICK else ' (race over)',
            ' (P1 finished, %d laps)' % self.s32(self.car(0) + 0xF78) if until_finish else '',
            ', '.join('P%d drove %.2f laps' % (p + 1, s.get('prog', 0) / float(len(trail)))
                      for p, s in enumerate(st) if trail)))

    # -------------------------------------------------------------- plan
    def run_plan(self, plan):
        for step in plan:
            op, args = step[0], step[1:]
            if op == 'boot':                    # dismiss the Controller Pak warning
                yield from self.idle(40)
                if not self.pad.pak:
                    yield from self.tap('A')
            elif op == 'unlock':                # cheats, typed in a throwaway race
                yield from self.do_unlock(args[0] if args else UNLOCK_ALL)
            elif op == 'title':                 # START on the title screen
                yield from self.idle(max(0, 200 - self.frame))
                yield from self.tap('START')
            elif op == 'menu':
                yield from self.do_menu(*args)
            elif op == 'cars':
                yield from self.do_cars(*args)
            elif op == 'race':
                yield from self.do_race(*args[:1], **(args[1] if len(args) > 1 else {}))
            elif op == 'tap':
                yield from self.tap(*args)
            elif op == 'wait':
                yield from self.idle(args[0])
            elif op == 'until_mode':            # wait for a BrModeSet target
                fn, timeout = args
                yield from self.wait_for(lambda: self.mode == fn, timeout, 'mode %08X' % fn)
            elif op == 'hold':                  # (port, buttons, x, y, frames)
                port, names, x, y, n = args
                for _ in range(n):
                    yield {port: (self.btn(names), x, y)}
            elif op == 'paint':
                yield from self.do_paint_sweep(*args)
            elif op == 'gen':                   # a custom generator: fn(driver)
                yield from args[0](self)
            else:
                raise ValueError(op)


# ---------------------------------------------------------------- scripts
def write_script(path, d, header, frames):
    lines = ['# ' + l if l else '#' for l in header]
    lines.append('frames %d' % frames)
    if d.pad.pak:
        lines.append('pak')
    if d.pad.rumble:
        lines.append('rumble')
    for port in range(d.pad.pads):
        pre = 'p2 ' if port else ''
        ev = d.rec[port] or [(0, 0, 0, 0)]
        if ev[0][0] != 0:
            ev = [(0, 0, 0, 0)] + ev
        for f, b, x, y in ev:
            names = '+'.join(k for k, v in BITS.items() if b & v) or '-'
            lines.append('%s%d %s%s' % (pre, f, names, (' %d %d' % (x, y)) if (x or y) else ''))
    open(path, 'w').write('\n'.join(lines) + '\n')


def summarise(path, frames):
    """The probe's view of a script: modes, menus and each race's setup."""
    r, box, ev = P.probe(path, frames)
    seen = []
    for f, kind, s in ev:
        if kind == 'setup' and 'BrRaceTick' in (seen[-1] if seen else ''):
            seen.append('  %5d  %s' % (f, s))
        elif kind == 'mode':
            seen.append('  %5d  mode %s' % (f, s))
    return r, box, seen


def generate(name, install=False):
    spec = plans()[name]
    plan, frames = spec['plan'], spec['frames']
    d = Driver(plan, pads=spec.get('pads', 1), pak=spec.get('pak', False), name=name,
               rumble=spec.get('rumble', False))
    r = d.run(frames)
    if d.fault:
        return name, 'FAIL %s' % d.fault
    err = None if r in ('frames', 'plan done') else 'stopped: %s' % r
    frames = d.frame + 1
    os.makedirs(DRAFTS, exist_ok=True)
    path = os.path.join(SCRIPTS if install else DRAFTS, name + '.txt')
    header = spec['doc'].strip().splitlines() + [
        '', 'Written by n64/tools/n64drive.py (plan %r); the driver saw:' % name] + [
        '  ' + n for n in d.notes]
    write_script(path, d, header, frames)
    return name, err or 'ok (%d frames, %s)' % (d.frame, '; '.join(d.notes))


def verify(name, frames=None):
    """Replaying the script must give the recording's RAM digest stream."""
    spec = plans()[name]
    frames = frames or spec['frames']
    d = Driver(spec['plan'], pads=spec.get('pads', 1), pak=spec.get('pak', False),
               rumble=spec.get('rumble', False))
    d.run(frames)
    path = os.path.join(SCRIPTS, name + '.txt')
    if not os.path.exists(path):
        path = os.path.join(DRAFTS, name + '.txt')
    b = NB.Box(script=path)
    b.run(d.frame)
    a_ram = [x for x in d.log if x[1] == 'ram']
    b_ram = [x for x in b.log if x[1] == 'ram']
    for x, y in zip(a_ram, b_ram):
        if x != y:
            return 'DIFFERS at frame %d' % x[0]
    return 'IDENTICAL over %d frames' % len(b_ram)


def plans():
    import n64drive_plans
    return n64drive_plans.PLANS


def _gen(job):
    name, install = job
    try:
        return generate(name, install)
    except Exception as e:                      # noqa: BLE001
        return name, 'ERROR %r' % e


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['gen', 'verify', 'list', 'show'])
    ap.add_argument('names', nargs='*')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--install', action='store_true', help='gen: write into the oracle\'s script suite')
    a = ap.parse_args()
    PLANS = plans()
    if a.cmd == 'list':
        for n, s in PLANS.items():
            print('%-32s %s' % (n, s['doc'].strip().splitlines()[0]))
        return
    names = list(PLANS) if a.all else a.names
    if a.cmd == 'show':                         # play a plan, print what the probe saw
        for n in names:
            spec = PLANS[n]
            d = Driver(spec['plan'], pads=spec.get('pads', 1), pak=spec.get('pak', False),
                       rumble=spec.get('rumble', False))
            r = d.run(spec['frames'])
            for f, kind, s in sorted(d.events_out, key=lambda e: e[0]):
                if kind != 'print':
                    print('%5d  %-5s %s' % (f, kind, s))
            print('stopped: %s after %d frames%s' % (r, d.frame, d.fault and ' -- ' + d.fault or ''))
            print('notes: ' + '; '.join(d.notes))
        return
    if a.cmd == 'gen':
        with ProcessPoolExecutor(min(14, max(1, len(names))), initializer=orphanguard.watch_parent) as ex:
            for n, r in ex.map(_gen, [(n, a.install) for n in names]):
                print('%-32s %s' % (n, r))
    else:
        with ProcessPoolExecutor(min(14, max(1, len(names))), initializer=orphanguard.watch_parent) as ex:
            for n, r in zip(names, ex.map(verify, names)):
                print('%-32s %s' % (n, r))


if __name__ == '__main__':
    main()
