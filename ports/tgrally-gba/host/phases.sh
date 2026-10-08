#!/bin/sh
# phases.sh -- a race tick's cycles by phase (gba/race.c g_phase_cyc), retraces 600..1200
cd "$(dirname "$0")/../../../build/tgrally/gba"
P=$(grep ' g_phase_cyc$' tgrally_poc.map | cut -c1-8)
GBARUN_NSTATS=6 GBARUN_KEYS="${KEYS:-60:1,64:0,140:1}" ./gbarun tgrally_poc.gba 1200 $P /tmp 2>/dev/null | grep -E '^(600|1200) ' | ../../../.venv/bin/python -c "
import sys
a, b = [list(map(int, l.split())) for l in sys.stdin]
n = b[6] - a[6]
for k, name in enumerate(['0 input', '1 forces', '2 collide', '3 after', 'overlays']):
    print('%-10s %7d' % (name, (b[1 + k] - a[1 + k]) // n))
"
