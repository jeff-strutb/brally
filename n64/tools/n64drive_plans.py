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


# Time attack (no weather screen, a ghost car slot) and practice (no
# opponent), one and two players.
for t, car, su in [(0, 10, SETUPS[3]), (6, 12, SETUPS[4]), (4, 11, SETUPS[5]), (8, 9, SETUPS[6])]:
    plan('ta_%s_car%d' % (SLUG[t], car),
         'Time attack: %s, car %d, driven on the racing line.' % (TRACKS[t], car),
         START + race_steps('timeattack', t, None, [dict(car=car, setup=su)]) + [('race', RACE)], 9000)
plan('prac_mcoast_rain', 'Practice, one player: Mirror Coastline in the rain, car 7.',
     START + race_steps('practice', 7, 2, [dict(car=7, setup=SETUPS[7])]) + [('race', RACE)], 9000)
plan('prac2p_mine_night', 'Practice, two players split screen: Strip Mine at night, cars 2 and 5.',
     START + race_steps('practice', 3, 4, [dict(car=2, setup=SETUPS[8]), dict(car=5, setup=SETUPS[9])],
                        players=2) + [('race', RACE, dict(drivers=('auto', 'auto')))], 9000, pads=2)

# Championship from a fresh season: the round's own track and weather.
plan('champ_round1', 'Championship, a fresh season: the first round (its own track and '
     'weather), car 1 kept as set, driven on the racing line.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['championship']), ('menu', 'Season', 2),
      ('cars', [dict(car=1)]), ('race', RACE)], 6000)
plan('champ_round1_car8', 'Championship, a fresh season, first round, car 8 with a manual '
     'gearbox and grippy tires.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['championship']), ('menu', 'Season', 2),
      ('cars', [dict(car=8, setup=SETUPS[2])]), ('race', RACE)], 6000)

# The view buttons and gear buttons during a race: C-up changes the view,
# the other C buttons, L and R pressed and held in turn.
VIEWS = [(300, 0, 'CU', 6), (700, 0, 'CU', 6), (1100, 0, 'CU', 6), (1500, 0, 'CU', 6),
         (1700, 0, 'CD', 90), (1900, 0, 'CL', 90), (2100, 0, 'CR', 90), (2300, 0, 'L', 60),
         (2500, 0, 'CU', 6), (2700, 0, 'CD+CL', 60)]
plan('views_coast_manual', 'Arcade on Coastline, car 3 with a manual gearbox: the view '
     'buttons cycled and held, the other C buttons and L during the race.',
     START + race_steps('arcade', 2, 1, [dict(car=3, setup=SETUPS[0])]) +
     [('race', 3000, dict(extras=VIEWS))], 9000)
plan('views2p_desert', 'Arcade, two players on Desert in the snow: each player cycles '
     'the views and holds the C buttons.',
     START + race_steps('arcade', 0, 3, [dict(car=4), dict(car=6)], players=2) +
     [('race', 3000, dict(drivers=('auto', 'auto'),
                          extras=VIEWS + [(o + 150, 1, b, h) for o, _, b, h in VIEWS]))], 9000, pads=2)

# Controller types B, C, D and the steering wheel, set on the Controller
# screen, then a race driven through that type's controls.
for row, name, t, w in [(1, 'typeb', 1, 1), (2, 'typec', 2, 3), (3, 'typed', 4, 2), (4, 'wheel', 5, 4)]:
    plan('ctl_%s_%s' % (name, SLUG[t]),
         'Options -> Controller: %s for player one, then an arcade race on %s in %s weather '
         'driven through its controls.' % (['TYPE A', 'TYPE B', 'TYPE C', 'TYPE D', 'STEERING WHEEL'][row],
                                          TRACKS[t], WEATHER[w]),
         START + [('menu', 'TOP GEAR', MAIN['options']), ('menu', 'OPTIONS', 3),
                  ('menu', 'Controller 1', row), ('wait', 60), ('tap', 'B', 0, 4, 60)] +
         race_steps('arcade', t, w, [dict(car=row + 1, setup=SETUPS[row])]) + [('race', RACE)], 11000)

# The three toggle cheats: texture filter and the frame-end effect for a
# race, and the other attract demo.
plan('toggles_filter_fbfx', 'The texture-filter and frame-end-effect cheats, then an arcade '
     'race on Mirror Mountain in fog.',
     [('boot',), ('title',), ('unlock', ['tracks', 'mirrors', 'filter', 'fbfx'])] +
     race_steps('arcade', 6, 1, [dict(car=1, setup=SETUPS[1])]) + [('race', RACE)], 9000)
plan('credits_demo', 'Options -> Credits: the credits roll over a demo race, then back to options.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['options']), ('menu', 'OPTIONS', 6),
      ('wait', 7000)], 8000)
plan('credits_demo2', 'The demo cheat, then Options -> Credits: the credits roll over the '
     'other demo race.',
     [('boot',), ('title',), ('unlock', ['demo2']), ('menu', 'TOP GEAR', MAIN['options']),
      ('menu', 'OPTIONS', 6), ('wait', 7000)], 10000)

# A whole race: the first championship round driven to the flag, then the
# results, the instant replay and on (A pressed now and then).
plan('champ_finish', 'Championship, a fresh season: the first round driven all three laps to '
     'the flag; the instant replay restarted with A and left with START, then the results '
     'screens and on, A pressed every few seconds.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['championship']), ('menu', 'Season', 2),
      ('cars', [dict(car=1)]), ('race', 40000, dict(until_finish=True)),
      ('wait', 900), ('tap', 'A', 0, 4, 900), ('tap', 'START', 0, 4, 300),
      ('gen', lambda d: _press_on(d, 5000))], 50000)


def _press_on(d, frames):
    """A tap every 150 frames, for this long."""
    for k in range(frames):
        yield {0: (d.btn('A'), 0, 0)} if k % 150 < 4 else {}


# A Rumble Pak in port 1: a race full of knocks (the motor pulsed), and the
# Load/Save screen, which finds a pak that is not a Controller Pak.
plan('rumble_jungle', 'A Rumble Pak in controller 1: an arcade race on Jungle in the rain, '
     'car 5, knocks and all.',
     START + race_steps('arcade', 4, 2, [dict(car=5, setup=SETUPS[5])]) + [('race', RACE)],
     9000, rumble=True)
plan('rumble_loadsave', 'A Rumble Pak in controller 1: Load/Save, every row tried, where the '
     'pak is not a Controller Pak.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['loadsave'])] +
     [s for r in range(3) for s in (('tap', 'A', 0, 4, 150), ('tap', 'B', 0, 4, 90),
                                    ('tap', '-', 0, 4, 60, 80, 0))] +
     [('tap', 'B', 0, 4, 120)], 3000, rumble=True)


# The paint shop explored by a seeded random walk of the cursor: strokes with
# A held, presses at each stop, the other buttons now and then.
plan('paint_walk', 'Paint shop, car 1: the cursor walked at random over the screen (seed 1), '
     'half the moves drawn with A held, A pressed at every stop and B, Z, L, R or a C '
     'button now and then; left and re-entered when a press leaves the shop.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['paintshop']), ('wait', 120),
      ('tap', 'A', 0, 4, 90), ('until_mode', 0x80243260, 900), ('wait', 60),
      ('paint', 1, 400, 20000)], 24000)
plan('paint_walk2', 'Paint shop, car 7 (the second car in the list): the same random walk with '
     'seed 2 and a longer run.',
     [('boot',), ('title',), ('menu', 'TOP GEAR', MAIN['paintshop']), ('wait', 120),
      ('tap', '-', 0, 4, 60, 80, 0), ('tap', 'A', 0, 4, 90), ('until_mode', 0x80243260, 900),
      ('wait', 60), ('paint', 2, 700, 30000)], 34000)
