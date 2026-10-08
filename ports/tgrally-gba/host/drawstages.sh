#!/bin/sh
# drawstages.sh -- where a drawn race frame's cycles go (gba/main.c g_dstat), retraces 600..1200
cd "$(dirname "$0")/../../../build/tgrally/gba"
P=$(grep ' g_dstat$' tgrally_poc.map | cut -c1-8)
GBARUN_NSTATS=16 GBARUN_KEYS="${KEYS:-60:1,64:0,140:1}" ./gbarun tgrally_poc.gba 1200 $P /tmp 2>/dev/null | grep -E '^(600|1200) ' | ../../../.venv/bin/python -c "
import sys
a, b = [list(map(int, l.split())) for l in sys.stdin]
n = b[16] - a[16]
for k, name in enumerate(['overlay', 'view', 'cells', 'walk', '', 'clear', 'raster']):
    if name:
        print('%-8s %7d' % (name, (b[9 + k] - a[9 + k]) // n))
"
