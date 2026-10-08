#!/bin/sh
# Build the GBA proof of concept and its host harness.
#   build.sh RAM DUMP F0 F1 [ROM SND [HST [MRAM MDUMP]]]   RAM: a TGR_RAMDUMP taken in the race, DUMP: a
#                             TGR_WORLDDUMP of it (ports/tgrally), F0..F1 its race frames; ROM the cartridge
#                             and SND a TGR_SNDDUMP of the race, for the sound;
#                             HST a TGR_HUDSTATE of it, for the HUD;
#                             MRAM and MDUMP the same two of the main menu (scripts/menu_tour.txt);
#                             DUMP "-": the whole track from RAM alone (tools/trackworld.py), for the
#                             live race.  RACERAM=FILE: the console's memory at a race's start
#                             (tools/simref.py's ram.bin) for the race's simulation (tools/race.py)
set -e
cd "$(dirname "$0")/../.."
OUT=build/tgrally/gba
G=ports/tgrally-gba
PY=.venv/bin/python
mkdir -p $OUT/obj
[ $# -ge 4 ] && (cd $G/tools && ../../../$PY convert.py ../../../$1 ../../../$2 $3 $4 ../../../$OUT/world_data.c)
[ $# -ge 6 ] && $PY $G/tools/sound.py $1 "$5" $6 $3 $4 $OUT/sound_data.c
[ $# -ge 7 ] && $PY $G/tools/hud.py $1 "$5" $7 $3 $4 $OUT/hud_data.c
[ $# -ge 9 ] && (cd $G/tools && ../../../$PY menu.py ../../../$8 ../../../$9 "../../../$5" ../../../$OUT/menu_data.c)
[ -n "$RACERAM" ] && $PY $G/tools/race.py $RACERAM $OUT/race_data.c
CF="--target=armv4t-none-eabi -mcpu=arm7tdmi -marm -mfloat-abi=soft -O2 -ffreestanding -fno-common \
    -mlong-calls -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -I$G/gba -Wall"
clang $CF -c $G/gba/crt0.s -o $OUT/obj/crt0.o
clang $CF -c $G/gba/span.s -o $OUT/obj/span.o
clang $CF -c $G/gba/raster.s -o $OUT/obj/raster.o
clang $CF -c $G/gba/sound.s -o $OUT/obj/sound.o
clang $CF -c $G/gba/hud.s -o $OUT/obj/hud.o
clang $CF -c $OUT/hud_data.c -o $OUT/obj/hud_data.o
clang $CF -c $OUT/sound_data.c -o $OUT/obj/sound_data.o
clang $CF -c $G/gba/front.s -o $OUT/obj/front.o
clang $CF -c $G/gba/menu.s -o $OUT/obj/menu_s.o
clang $CF $MAINDEF -c $G/gba/main.c -o $OUT/obj/main.o
clang $CF $MAINDEF -c $G/gba/menu.c -o $OUT/obj/menu.o
clang $CF -c $OUT/menu_data.c -o $OUT/obj/menu_data.o
clang $CF -c $G/gba/libc.c -o $OUT/obj/libc.o
SIM="geom rigid coll car camera simload fxmath"
TF=$(echo "$CF" | sed 's/-marm/-mthumb/')        # the simulation runs from the cartridge: Thumb, half the fetches
clang $CF -c $G/gba/fxarm.s -o $OUT/obj/fxarm.o
clang $CF -c $G/gba/aeabi.s -o $OUT/obj/aeabi.o
for f in $SIM; do clang $TF -I$G/sim -c $G/sim/$f.c -o $OUT/obj/sim_$f.o; done
clang $CF -I$G/sim -c $G/sim/geomhot.c -o $OUT/obj/sim_geomhot.o    # ARM in IWRAM
clang $TF -I$G/sim -c $G/gba/race.c -o $OUT/obj/race.o
clang $CF -I$G/sim -I$G/gba -c $OUT/race_data.c -o $OUT/obj/race_data.o
clang $CF -c $OUT/world_data.c -o $OUT/obj/world_data.o
$PY $G/tools/gbalink.py $OUT/tgrally_poc.gba $OUT/obj/crt0.o $OUT/obj/span.o $OUT/obj/raster.o $OUT/obj/sound.o $OUT/obj/hud.o $OUT/obj/front.o $OUT/obj/main.o $OUT/obj/libc.o $OUT/obj/world_data.o $OUT/obj/sound_data.o $OUT/obj/hud_data.o \
    $OUT/obj/menu.o $OUT/obj/menu_s.o $OUT/obj/menu_data.o $OUT/obj/race.o $OUT/obj/race_data.o $OUT/obj/fxarm.o $OUT/obj/aeabi.o \
    $(for f in $SIM geomhot; do echo $OUT/obj/sim_$f.o; done)
