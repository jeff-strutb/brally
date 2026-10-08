"""n64probe.py -- what a n64box script actually does, frame by frame.

A script is blind timed input; this says where it went.  It runs the
original ROM under n64box.py and prints a timeline of:

  menu     every generic-menu screen (BrMenu) the game shows: its title and
           the highlighted row, whenever either changes
  mode     every BrModeSet (the game's top-level state function)
  var      the race-setup globals (track, weather, mirror, players, mode, car
           choices, laps, views ...) whenever one changes
  print    the game's own osSyncPrintf output (with --prints)

and, with --cover, which game functions the run reached (entries executed),
by tier, against the T3/T4 denominators from n64tiers.

    .venv/bin/python tools/tgrally/n64probe.py tools/tgrally/n64box_scripts/timeattack.txt
    .venv/bin/python tools/tgrally/n64probe.py SCRIPT --frames 2000 --prints
    .venv/bin/python tools/tgrally/n64probe.py --cover            # every script, the suite's coverage
"""
import argparse
import csv
import os
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import n64box as NB  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

SCRIPTS = os.path.join(HERE, 'n64box_scripts')

# The race-setup globals, by what they hold.
WATCH = [
    (0x8026FF18, 'mode'),           # the game mode
    (0x8026FF08, 'players'),        # human players
    (0x8026FF1C, 'demo'),           # which demo
    (0x8028B940, 'track'),          # the chosen track (5.. = mirrors)
    (0x8028A8AC, 'mirror'),         # the track is mirrored
    (0x8028C800, 'weather'),        # Sunny Fog Rain Snow Night
    (0x8031D7BC, 'carP1'),          # player one's car (car record +0x205C)
    (0x8031F84C, 'carP2'),          # player two's car
    (0x803162B0, 'stepP1'),         # car select step: 0 car, 1-5 setup, 6 ready
    (0x803162B4, 'stepP2'),
    (0x8028B304, 'laps'),           # laps in the race
    (0x8028B7F4, 'cars'),           # cars in the race
    (0x8028AB0C, 'views'),          # views on screen
    (0x8028AA54, 'rearview'),       # the rear-view mirror size
    (0x8026FF20, 'cheatDemo'),      # the toggle cheats
    (0x8028AA68, 'cheatFilter'),
    (0x8028AA94, 'cheatFx'),
]
BYTES = [(0x8036A8E0 + 0x25, 'ctlP1'),         # controller type: A B C D wheel
         (0x8036A8E0 + 0x15C + 0x25, 'ctlP2')]


def names():
    out = {}
    for r in csv.DictReader(open(os.path.join(NB.ROOT, 'config/tgrally/symbols_tgr.csv'))):
        out[int(r['va'], 16)] = r['name']
    return out


def tiers():
    """-> ({va: tier}, {va: bytes}) for the game's functions, from n64tiers
    (as last built; FENCED library code is left out)."""
    import n64tiers
    fmap, tier = n64tiers.classify(fresh=False)
    tm = {int(k, 16): t for k, t in tier.items() if t in ('T1', 'T2', 'T3', 'T4')}
    return tm, {va: fmap[va] for va in tm}


class ProbeBox(NB.Box):
    def __init__(self, script, cover=None, **kw):
        super().__init__(script=script, **kw)
        self.events_out = []
        self.names = names()
        self.last_menu = None
        self.last_vars = {}
        self.uc.hook_add(UC_HOOK_CODE, self.on_menu, begin=NB.sx(0x8020AD5C), end=NB.sx(0x8020AD5C))
        self.uc.hook_add(UC_HOOK_CODE, self.on_mode, begin=NB.sx(0x8021C6E4), end=NB.sx(0x8021C6E4))
        self.reached = {}
        self._cover_hooks = {}
        for va in cover or ():
            self._cover_hooks[va] = self.uc.hook_add(UC_HOOK_CODE, self.on_cover,
                                                     begin=NB.sx(va), end=NB.sx(va))

    def s32(self, va):
        v = self.r32(va)
        return v - (1 << 32) if v & 0x80000000 else v

    def cstr(self, va, n=64):
        b = self.read(va, n)
        return b.split(b'\0')[0].decode('latin-1')

    def on_menu(self, uc, addr, size, data):
        title = self.cstr(self.arg(0))
        # the highlighted row: the caller's on the menu's first frame, then its own
        sel = self.r32(0x80316244) if self.r32(0x80271FD4) else self.r32(self.arg(3))
        key = (title, sel)
        if key != self.last_menu:
            self.last_menu = key
            self.events_out.append((self.frame, 'menu', '%-22s row %d of %d' % (title, sel, self.arg(1))))

    def on_mode(self, uc, addr, size, data):
        fn = self.arg(0)
        self.events_out.append((self.frame, 'mode', self.names.get(fn, 'func_%08X' % fn)))
        self.snap = True                        # the setup, once this frame is over

    def os_print(self):
        """The game's debug print, formatted (integer arguments only)."""
        import re
        fmt = self.cstr(self.arg(0), 160)
        args, i = [], 1
        for m in re.finditer(r'%[-0-9.]*[a-zA-Z]', fmt):
            if m.group()[-1] in 'dxXuci':
                v = self.arg(i)
                args.append(v - (1 << 32) if m.group()[-1] in 'di' and v & 0x80000000 else v)
            else:
                args.append(0)
            i += 1
        try:
            txt = re.sub(r'%[-0-9.]*[a-zA-Z]', '{}', fmt).format(*args)
        except (IndexError, ValueError):
            txt = fmt
        if txt.strip(' .\n'):
            self.events_out.append((self.frame, 'print', ' '.join(txt.split())))
        super().os_print()

    def on_cover(self, uc, addr, size, data):
        va = addr & 0xffffffff
        if va not in self.reached:
            self.reached[va] = self.frame

    def advance_time(self):
        f = self.frame
        r = super().advance_time()
        if self.frame != f:
            if getattr(self, 'snap', False):
                self.snap = False
                self.events_out.append((self.frame, 'setup', ' '.join(
                    ['%s=%d' % (n, self.s32(va)) for va, n in WATCH] +
                    ['%s=%d' % (n, self.read(va, 1)[0]) for va, n in BYTES])))
            for va, name in WATCH:
                v = self.r32(va)
                v = v - (1 << 32) if v & 0x80000000 else v
                if self.last_vars.get(name) != v:
                    if name in self.last_vars:
                        self.events_out.append((self.frame, 'var', '%s %s -> %s' % (
                            name, self.last_vars[name], v)))
                    self.last_vars[name] = v
        return r


def probe(script, frames, prints=False):
    box = ProbeBox(script)
    r = box.run(frames)
    ev = [e for e in box.events_out if prints or e[1] != 'print']
    ev.sort(key=lambda e: e[0])
    return r, box, ev


def _cover_worker(args):
    sc, frames, vas = args
    box = ProbeBox(sc, cover=vas)
    r = box.run(frames)
    return sc, r, box.frame, box.reached


def script_frames(sc):
    import n64t3
    return n64t3.script_frames(sc)


def cover(dirs=(SCRIPTS,)):
    tm, size = tiers()
    vas = sorted(tm)
    scs = [os.path.join(os.path.abspath(d), s) for d in dirs for s in sorted(os.listdir(d))
           if s.endswith('.txt')]
    seen = {}
    per = {}
    with ProcessPoolExecutor(min(14, len(scs))) as ex:
        for sc, r, fr, reached in ex.map(_cover_worker, [(s, script_frames(s), vas) for s in scs]):
            sc = os.path.basename(sc)
            per[sc] = reached
            for va in reached:
                seen.setdefault(va, []).append(sc)
            print('%-28s %-8s %5d frames  %4d fns reached' % (sc, r, fr, len(reached)))
    print()
    print('  %-4s %9s %14s   %9s %16s' % ('tier', 'fns hit', '', 'bytes hit', ''))
    for t in ('T4', 'T3', 'T2', 'T1'):
        all_ = [v for v in vas if tm[v] == t]
        hit = [v for v in all_ if v in seen]
        b_all = sum(size.get(v, 0) for v in all_)
        b_hit = sum(size.get(v, 0) for v in hit)
        print('  %-4s %4d / %-4d %5.1f%%   %7d / %-7d %5.1f%%' % (
            t, len(hit), len(all_), 100.0 * len(hit) / max(1, len(all_)),
            b_hit, b_all, 100.0 * b_hit / max(1, b_all)))
    return tm, seen, size, per


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('script', nargs='?')
    ap.add_argument('--frames', type=int)
    ap.add_argument('--prints', action='store_true')
    ap.add_argument('--cover', action='store_true', help='the whole suite: functions reached, by tier')
    ap.add_argument('--dir', action='append', help='with --cover: script directories (default the suite)')
    ap.add_argument('--unreached', metavar='TIERS', help='with --cover: list unreached functions of these tiers (e.g. T3,T4)')
    a = ap.parse_args()
    if a.cover:
        tm, seen, size, per = cover(a.dir or (SCRIPTS,))
        if a.unreached:
            nm = names()
            want = a.unreached.split(',')
            print()
            for va in sorted(tm, key=lambda v: -size.get(v, 0)):
                if tm[va] in want and va not in seen:
                    print('  %08X %-4s %6d B  %s' % (va, tm[va], size.get(va, 0), nm.get(va, '')))
        return
    sc = a.script
    if not os.path.exists(sc):
        sc = os.path.join(SCRIPTS, sc)
    frames = a.frames or script_frames(os.path.abspath(sc))
    r, box, ev = probe(sc, frames, a.prints)
    for f, kind, s in ev:
        print('%5d  %-5s %s' % (f, kind, s))
    print('stopped: %s after %d frames%s' % (r, box.frame, box.fault and ' -- ' + box.fault or ''))


if __name__ == '__main__':
    main()
