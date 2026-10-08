#!/bin/sh
# simstages.sh -- where a race tick's cycles go (a SIM_PROF build: g_simprof; the ticks counted
# by gba/race.c g_phase_cyc), retraces 600..1200
cd "$(dirname "$0")/../../../build/tgrally/gba"
P=$(grep ' g_simprof$' tgrally_poc.map | cut -c1-8)
T=$(grep ' g_phase_cyc$' tgrally_poc.map | cut -c1-8)
K="${KEYS:-60:1,64:0,140:1}"
N=$(GBARUN_NSTATS=6 GBARUN_KEYS="$K" ./gbarun tgrally_poc.gba 1200 $T /tmp 2>/dev/null | grep -E '^(600|1200) ' | awk '{print $7}' | tr '\n' ' ')
GBARUN_NSTATS=17 GBARUN_KEYS="$K" ./gbarun tgrally_poc.gba 1200 $P /tmp 2>/dev/null | grep -E '^(600|1200) ' | ../../../.venv/bin/python -c "
import sys
n0, n1 = map(int, '$N'.split())
a, b = [list(map(int, l.split())) for l in sys.stdin]
names = ['groundray', 'drive', 'springs', 'wheeltyre', 'integ+grip', 'skid+loads', 'integ2', 'advance', 'depthall',
         'wheelmats', 'build+speed', 'camera', ' broadphase', ' tipkick', ' carcar', ' stepmats', ' respwalk']
for n, k in zip(names, range(17)):
    print('%-13s %7d' % (n, (b[k + 1] - a[k + 1]) // (n1 - n0)))
"
