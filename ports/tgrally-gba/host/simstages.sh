#!/bin/sh
# simstages.sh -- where a race tick's cycles go (a SIM_PROF build: g_simprof), retraces 600..1200
cd "$(dirname "$0")/../../../build/tgrally/gba"
P=$(grep ' g_simprof$' tgrally_poc.map | cut -c1-8)
GBARUN_NSTATS=17 GBARUN_KEYS="${KEYS:-60:1,64:0,140:1}" ./gbarun tgrally_poc.gba 1200 $P /tmp 2>/dev/null | grep -E '^(600|1200) ' | ../../../.venv/bin/python -c "
import sys
a, b = [list(map(int, l.split())) for l in sys.stdin]
names = ['groundray', 'drive', 'springs', 'wheeltyre', 'integ+grip', 'skid+loads', 'integ2', 'advance', 'depthall',
         'wheelmats', '(step+build)', 'camera', ' broadphase', ' tipkick', ' carcar', ' stepmats', ' respwalk']
d = [b[i + 1] - a[i + 1] for i in range(17)]
tot = d[0] + d[1] + d[10] + d[11]
for n, x in zip(names, d):
    print('%-13s %5.1f%%' % (n, 100.0 * x / tot))
"
