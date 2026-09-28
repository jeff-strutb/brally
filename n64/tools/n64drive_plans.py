"""n64drive_plans.py -- the plans n64drive.py plays, one script each.

A plan is a list of steps (see Driver.run_plan): boot, title, unlock (the
cheats, typed in a throwaway race), menu rows by title, car select per
player, then the race with the autopilot and any extra presses.
"""
from n64drive import MAIN, TRACKS, WEATHER, UNLOCK_ALL, SETUP  # noqa: F401

PLANS = {}


def plan(name, doc, steps, frames, **kw):
    PLANS[name] = dict(doc=doc, plan=steps, frames=frames, **kw)


def setup(h=0, t=1, ti=1, s=1):
    """Car setup rows: handling Type 1-3, transmission Manual/Automatic,
    tires Slippy/Normal/Grippy, suspension Softer/Normal/Harder."""
    return dict(handling=h, transmission=t, tires=ti, suspension=s)


START = [('boot',), ('title',), ('unlock',)]


def race_steps(mode, track, weather, cars, players=1):
    s = [('menu', 'TOP GEAR', MAIN[mode])]
    if mode in ('arcade', 'practice'):
        s.append(('menu', '1P/2P', players - 1))
    s.append(('menu', 'TRACK SELECT', track))
    if mode in ('arcade', 'practice') and weather is not None:
        s.append(('menu', 'WEATHER', weather))
    s.append(('cars', cars))
    return s


# ------------------------------------------------------------ the catalogue
NCARS = 13          # 0-8 rally cars, 9 Helmet, 10 Milk Truck, 11 CUPRA, 12 Beach Ball
SLUG = ['desert', 'mountain', 'coast', 'mine', 'jungle',
        'mdesert', 'mmountain', 'mcoast', 'mmine', 'mjungle']
WSLUG = ['sunny', 'fog', 'rain', 'snow', 'night']


def weather_ok(track, w, players=1):
    """Sunny (row 0) is only offered to one player, off the Strip Mine."""
    return w != 0 or (players == 1 and track not in (3, 8))


# every (handling, transmission, tires, suspension) value turns up, and the
# two transmissions alternate
SETUPS = [setup(h, t, ti, s) for h, t, ti, s in
          [(0, 0, 0, 0), (1, 1, 1, 1), (2, 0, 2, 2), (0, 1, 2, 1), (1, 0, 0, 2),
           (2, 1, 1, 0), (0, 0, 1, 2), (1, 1, 2, 0), (2, 0, 0, 1), (0, 1, 0, 1)]]

RACE = 2400          # frames driven after the green light

# One player, arcade: every track and mirror, each with two weathers, so a
# track and its mirror together see four of the five; cars and setups rotate.
_k = 0
for t in range(10):
    base = t % 5
    ws = [(base + (0 if t < 5 else 2)) % 5, (base + (1 if t < 5 else 3)) % 5]
    ws = [w if weather_ok(t, w) else 4 - (_k % 2) for w in ws]
    for w in ws:
        car = _k % NCARS
        su = SETUPS[_k % len(SETUPS)]
        plan('arc_%s_%s_car%d' % (SLUG[t], WSLUG[w], car),
             'Arcade, one player: %s in %s weather, car %d (%s transmission), '
             'driven on the computer driver\'s racing line.' % (
                 TRACKS[t], WEATHER[w], car, SETUP[1][1][su['transmission']]),
             START + race_steps('arcade', t, w, [dict(car=car, setup=su)]) +
             [('race', RACE)], 9000)
        _k += 1

# Two players, split screen: both on the autopilot, a different car each.
for i, (t, w) in enumerate([(0, 3), (1, 4), (2, 2), (3, 1), (4, 3), (6, 2), (9, 1)]):
    c1, c2 = (3 * i) % 9, (3 * i + 4) % 9
    plan('arc2p_%s_%s' % (SLUG[t], WSLUG[w]),
         'Arcade, two players split screen: %s in %s weather, cars %d and %d, '
         'both driven on the racing line.' % (TRACKS[t], WEATHER[w], c1, c2),
         START + race_steps('arcade', t, w, [dict(car=c1, setup=SETUPS[i]),
                                             dict(car=c2, setup=SETUPS[-1 - i])], players=2) +
         [('race', RACE, dict(drivers=('auto', 'auto')))], 9000, pads=2)
