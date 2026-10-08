#!/bin/sh
# racefps.sh -- the live race's speed under gbarun: from the menu into an Arcade race, the
# accelerator held; frames drawn a second, a tick's cycles and a frame's drawing, over
# retraces FROM..END (600..1200)
cd "$(dirname "$0")/../../../build/tgrally/gba"
S=$(grep ' g_stats$' tgrally_poc.map | cut -c1-8)
GBARUN_KEYS="${KEYS:-60:1,64:0,140:1}" ./gbarun tgrally_poc.gba ${END:-1200} $S /tmp ${SHOT:-} 2>/dev/null | awk -v A=${FROM:-600} '
    $1 == A { f0 = $2 }
    $1 > A && $11 > 0 { sim += $10 / $11; draw += $3 - $10; n++ }
    { last = $1; f1 = $2 }
    END { printf "%.1f frames a second; a tick %d cycles; a frame drawn %d cycles\n", (f1 - f0) / ((last - A) / 59.73), sim / n, draw / n }'
