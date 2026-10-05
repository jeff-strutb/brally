#!/bin/sh
# Link Top Gear Rally: the core (build.sh), the symbol table and arena
# generated from the source (tools/globals.py), the platform layer, and one
# host (shared with the Boss Rally port: ports/brally/platform/host).
#   env: HOST    null (default, headless) | macos
#        RENDER  null (default) | soft | metal (needs HOST=macos)
#        OUT     build directory (default build/tgrally/HOST-RENDER)
#        TGR_ROM your cartridge's ROM (default reference/tgrally/Top Gear Rally (USA).z64):
#                its data is built into the executable (tools/assets.py); the game
#                needs no ROM once built
set -e
cd "$(dirname "$0")/../.."
HOST=${HOST:-null}
RENDER=${RENDER:-null}
OUT=${OUT:-build/tgrally/$HOST-$RENDER}
export OUT
CC=${CC:-clang}
P=ports/tgrally/platform
H=ports/brally/platform/host
PFLAGS="-O2 ${GFLAG:--g} -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -fno-strict-aliasing \
  -I$P/include -I$P/os -I$H -Iports/brally/platform/render -Iports/tgrally/include"
mkdir -p $OUT/plat

ports/tgrally/build.sh
if grep -q '^FAIL' $OUT/compile.txt; then
  grep '^FAIL' $OUT/compile.txt >&2
  echo "link: core TUs failed to compile; not linking" >&2
  exit 1
fi
$CC -c $PFLAGS -fno-builtin -w $OUT/gen/tgr_syms.c -o $OUT/plat/tgr_syms.o
$CC -c $OUT/gen/arena.s -o $OUT/plat/arena.o
${PYTHON:-.venv/bin/python} ports/tgrally/tools/assets.py $OUT/assets
[ $OUT/plat/romdata.o -nt $OUT/assets/romdata.bin ] || $CC -c $OUT/assets/romdata.S -o $OUT/plat/romdata.o

SRCS="$P/os/addr.c $P/os/lift.c $P/os/thread.c $P/os/io.c $P/os/si.c $P/os/main.c $P/os/trace.c $P/os/sha1.c \
      $P/audio/mixer.c $P/audio/out.c $P/gfx/gfx.c $P/gfx/rcp.c $P/libc/xprintf.c \
      ports/brally/platform/render/brr_png.c $H/posix/host_posix.c"
case "$RENDER" in
  null)  SRCS="$SRCS $P/render/null/rdr_null.c";;
  soft)  SRCS="$SRCS $P/render/soft/rdr_soft.c";;
  metal) SRCS="$SRCS $P/render/metal/rdr_metal.m";;
  *) echo "link: unknown RENDER $RENDER" >&2; exit 2;;
esac
LIBS="-lz"
case "$HOST" in
  null)  SRCS="$SRCS $H/null/host_null.c";;
  macos) SRCS="$SRCS $H/macos/host_macos.m"
         LIBS="$LIBS -framework Cocoa -framework AudioToolbox -framework CoreAudio -framework GameController -framework QuartzCore -framework Metal -framework AVFoundation";;
  *) echo "link: unknown HOST $HOST" >&2; exit 2;;
esac
OBJS=
for s in $SRCS; do
  o=$OUT/plat/$(echo "$s" | sed 's#/#__#g').o
  case "$s" in */render/metal/*) XF=-fobjc-arc;; *) XF=;; esac   # the Metal renderer is ARC
  $CC -c $PFLAGS $XF "$s" -o "$o"
  OBJS="$OBJS $o"
done
# the core objects of the TUs build.sh compiled (not a stale one of a deleted TU)
CORE=$(sed "s#ports/tgrally/src/##; s#/#__#g; s#^#$OUT/obj/#; s#\$#.o#" $OUT/tus.txt)
$CC ${LDFLAGS_TGR} -o $OUT/tgrally $CORE $OUT/plat/tgr_syms.o $OUT/plat/arena.o $OUT/plat/romdata.o $OBJS $LIBS
echo "linked $OUT/tgrally (host $HOST, renderer $RENDER)"
