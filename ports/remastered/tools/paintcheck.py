#!/usr/bin/env python3
"""paintcheck.py: the paint shop by pointer (src/menus/rmpaintptr.c), headless.

Drives Top Gear Rally Remastered's paint shop with RM_POINTER (the pointer as
the mouse or a finger gives it) and checks what it did against values computed
from the shop's own geometry (RM_PAINTLOG, RM_PAINTDUMP): a brush stroke
dragged across the decal in one frame painted at every texel on the way, a
tool clicked, a line dragged out, a style popup's choice clicked and taken, its
CANCEL picture, B picking the colour under it, the colour mixer's bar dragged
and OK, a tap whose button goes down and up within one retrace (the iPhone's),
and with no pointer nothing changed.

usage: ports/remastered/tools/paintcheck.py [build/remastered/null-null/remastered]
"""
import os, re, subprocess, sys, tempfile
from collections import Counter

BIN = sys.argv[1] if len(sys.argv) > 1 else 'build/remastered/null-null/remastered'
AREA = (316, 146, 256, 156)             # the decal on screen (RM_PAINTLOG's area)
END = 2200

# the title, the main menu's ring four along to the paint shop, a car: the shop is up by retrace 1094
SCRIPT = os.path.join(tempfile.mkdtemp(), 'paintshop.txt')
with open(SCRIPT, 'w') as f:
    f.write('frames %d\n0 -\n80 A\n88 -\n294 START\n302 -\n' % END)
    for i in range(4):
        f.write('%d - 80 0\n%d -\n' % (500 + 60 * i, 508 + 60 * i))
    f.write('780 A\n788 -\n1070 A\n1078 -\n')


def run(events, dump_at=END - 10):
    env = dict(os.environ, RM_PAINTLOG='1', RM_PAINTDUMP=str(dump_at),
               RM_POINTER=';'.join('%d:%.6f,%.6f,%d' % (f, x / 640.0, y / 480.0, b) for f, x, y, b in events))
    r = subprocess.run([BIN, '--headless', '--frames', str(END), '--script', SCRIPT], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, timeout=120)
    log = [l for l in r.stderr.splitlines() if l.startswith('paint')]
    frames = {}
    for l in log:
        m = re.match(r'paint: f (\d+) (.*)', l)
        if m:
            frames[int(m.group(1))] = dict(re.findall(r'(\w+) (-?\d+(?:,-?\d+)?)', m.group(2)))
    rows = [l.split(':', 1)[1] for l in log if l.startswith('paintrow:')]
    sw = {int(l.split()[1]): tuple(map(int, l.split()[7:10])) for l in log if l.startswith('paintswatch:')}
    btn = [l for l in log if l.startswith('paintbtn:')]
    rs = [int(l.split()[4]) for l in log if l.startswith('paintdump:')]
    if not frames:
        sys.exit('paintcheck: the paint shop never came up (%s)' % BIN)
    return frames, rows, sw, btn, (rs[0] if rs else None)


def texel(rows, tx, ty):                # rows are printed top row first; ty counts up from the bottom
    return int(rows[len(rows) - 1 - ty][tx], 16)


def tx_of(sx): return (sx - AREA[0]) >> 2
def ty_of(sy): return (AREA[1] + AREA[3] - sy) >> 2


def state_after(frames, f):
    return frames[min(x for x in frames if x >= f)]


fails = 0
def check(name, ok, detail=''):
    global fails
    print(('PASS ' if ok else 'FAIL ') + name + (': ' + detail if detail else ''))
    fails += not ok


base_frames, base_rows, base_sw, _, _ = run([])
COL = int(state_after(base_frames, 1100)['colour'])

# 1. a brush stroke dragged across the decal in one frame's jump: every texel on the way painted
y = 180
ev = [(1200, 330, y, 1), (1210, 560, y, 1), (1220, 560, y, 0)]
fr, rows, _, _, _ = run(ev)
ty = ty_of(y)
row = [texel(rows, x, ty) for x in range(tx_of(330), tx_of(560) + 1)]
base = [texel(base_rows, x, ty) for x in range(tx_of(330), tx_of(560) + 1)]
check('brush stroke continuous', all(c == COL for c in row) and any(c != COL for c in base),
      'row %d texels %d..%d: %s (before %s)' % (ty, tx_of(330), tx_of(560), ''.join('%x' % c for c in row),
                                                 ''.join('%x' % c for c in base)))
st = state_after(fr, 1225)
check('cursor follows pointer', st['cur'] == '560,180', 'cursor %s' % st['cur'])

# 2. a click on the line tool, then a line dragged out: the drag's end is the second click
lx, ly = 462 + 36, 384 + 11
y2 = 250
ev = [(1300, lx, ly, 1), (1304, lx, ly, 0),
      (1320, 330, y2, 1), (1330, 400, y2 + 2, 1), (1340, 560, y2, 1), (1350, 560, y2, 0)]
fr, rows, _, _, _ = run(ev)
check('click picks the line tool', state_after(fr, 1310)['tool'] == '3', 'tool %s' % state_after(fr, 1310)['tool'])
check('the drag holds the anchor until it ends', texel(rows, tx_of(450), ty_of(y2)) == COL and state_after(fr, 1345)['stroke'] == '1',
      'stroke mid-drag %s' % state_after(fr, 1345)['stroke'])
ty2 = ty_of(y2)
row = [texel(rows, x, ty2) for x in range(tx_of(330), tx_of(560) + 1)]
check('line drawn by the drag', all(c == COL for c in row) and state_after(fr, 1360)['stroke'] == '0',
      'row %d: %s' % (ty2, ''.join('%x' % c for c in row)))

# 3. the rectangle tool's style popup: a double click opens it, a click chooses, a second click selects
rx, ry = 314 + 36, 408 + 11
ev = [(1400, rx, ry, 1), (1402, rx, ry, 0), (1406, rx, ry, 1), (1408, rx, ry, 0)]
fr0, _, _, btn0, _ = run(ev)
check('double click opens the style popup', state_after(fr0, 1415)['popup'] == '1', 'popup %s' % state_after(fr0, 1415)['popup'])
cx, cy = 2 * 76 + 175 + 10, 213
ev2 = ev + [(1450, cx, cy, 1), (1452, cx, cy, 0)]
fr, _, _, _, _ = run(ev2)
check('a click chooses style 2', state_after(fr, 1460)['choice'] == '2' and state_after(fr, 1460)['popup'] == '1',
      'choice %s popup %s' % (state_after(fr, 1460)['choice'], state_after(fr, 1460)['popup']))
ev3 = ev2 + [(1480, cx, cy, 1), (1482, cx, cy, 0)]
fr, _, _, _, rs = run(ev3)
check('a second click on it selects it', rs == 2 and state_after(fr, 1490)['popup'] == '0', 'rect style %s popup %s' % (rs, state_after(fr, 1490)['popup']))
# the popup's CANCEL button (its B picture) closes it without choosing
bs = Counter(tuple(l.split()[2:5]) for l in btn0 if l.split()[1] == 'B').most_common(1)
if bs:
    bx, by, bh = map(int, bs[0][0])
    ev4 = ev + [(1450, cx, cy, 1), (1452, cx, cy, 0), (1470, bx + 20, by + bh // 2, 1), (1472, bx + 20, by + bh // 2, 0)]
    fr, _, _, _, rs = run(ev4)
    check('CANCEL picture cancels', rs == 0 and state_after(fr, 1480)['popup'] == '0', 'rect style %s popup %s' % (rs, state_after(fr, 1480)['popup']))
else:
    check('CANCEL picture recorded', False)

# 4. the right button on the decal: the colour under it
ex, ey = 330, 290
want = texel(base_rows, tx_of(ex), ty_of(ey))
ev = [(1550, ex, ey, 2), (1556, ex, ey, 0)]
fr, rows, _, _, _ = run(ev)
check('right button picks the colour', int(state_after(fr, 1560)['colour']) == want and want != COL,
      'colour %s want %d' % (state_after(fr, 1560)['colour'], want))
check('right button paints nothing', rows == base_rows)

# 5. the colour mixer: double click a swatch, drag the red bar, OK
sx, sy = 412 + 8, 364 + 7
ev = [(1600, sx, sy, 1), (1602, sx, sy, 0), (1606, sx, sy, 1), (1608, sx, sy, 0)]
fr0, _, _, btn0, _ = run(ev)
check('double click opens the mixer', state_after(fr0, 1615)['mix'] == '1')
bar_y = 0xc2 + 8
ev2 = ev + [(1640, 0x11d + 64, bar_y, 1), (1650, 0x11d + 100, bar_y + 3, 1), (1660, 0x11d + 100, bar_y + 3, 0)]
ax, ay, ah = map(int, Counter(tuple(l.split()[2:5]) for l in btn0 if l.split()[1] == 'A').most_common(1)[0][0])   # the mixer's OK: drawn every frame it is open
ev2 += [(1700, ax + 20, ay + ah // 2, 1), (1702, ax + 20, ay + ah // 2, 0)]
fr, _, sw, _, _ = run(ev2)
check('bar drag sets red to 200, OK keeps it', sw[4][0] == 200 and state_after(fr, 1710)['mix'] == '0',
      'swatch 4 %s (was %s) mix %s' % (sw[4], base_sw[4], state_after(fr, 1710)['mix']))

# 6. the iPhone's quick tap: down and up within one retrace still clicks; two fingers, B
ev = [(1300, lx, ly, 1), (1300, lx, ly, 0)]
fr, _, _, _, _ = run(ev)
check('a tap within one retrace clicks', state_after(fr, 1310)['tool'] == '3', 'tool %s' % state_after(fr, 1310)['tool'])
ev = [(1550, ex, ey, 2), (1550, ex, ey, 0)]
fr, rows, _, _, _ = run(ev)
check('a two-finger tap (B) picks the colour', int(state_after(fr, 1560)['colour']) == want, 'colour %s' % state_after(fr, 1560)['colour'])

# 7. with no pointer the game is as it was (the stick path untouched)
fr, rows, sw, _, _ = run([])
check('no pointer: no change', rows == base_rows and sw == base_sw)

print('%d failed' % fails)
sys.exit(1 if fails else 0)
