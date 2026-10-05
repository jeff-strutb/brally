#!/bin/sh
# Link Top Gear Rally: the core (build.sh), the symbol table and arena
# generated from the source (tools/globals.py), the platform layer, and one
# host (shared with the Boss Rally port: ports/brally/platform/host).
#   env: HOST    null (default, headless) | macos
#        RENDER  null (default) | metal (needs HOST=macos)
#        OUT     build directory (default build/tgrally)
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/tgrally}
HOST=${HOST:-null}
RENDER=${RENDER:-null}
CC=${CC:-clang}
P=ports/tgrally/platform
H=ports/brally/platform/host
PFLAGS="-O2 ${GFLAG:--g} -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -fno-strict-aliasing \
  -I$P/include -I$P/os -I$H -Iports/tgrally/include"
mkdir -p $OUT/plat

ports/tgrally/build.sh
if grep -q '^FAIL' $OUT/compile.txt; then
  grep '^FAIL' $OUT/compile.txt >&2
  echo "link: core TUs failed to compile; not linking" >&2
  exit 1
fi
$CC -c $PFLAGS -fno-builtin -w $OUT/gen/tgr_syms.c -o $OUT/plat/tgr_syms.o
$CC -c $OUT/gen/arena.s -o $OUT/plat/arena.o

SRCS="$P/os/addr.c $P/os/lift.c $P/os/thread.c $P/os/io.c $P/os/si.c $P/os/main.c $P/os/trace.c $P/os/sha1.c \
      $P/audio/mixer.c $P/audio/out.c $P/gfx/gfx.c $P/libc/xprintf.c $H/posix/host_posix.c"
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
  $CC -c $PFLAGS "$s" -o "$o"
  OBJS="$OBJS $o"
done
$CC ${LDFLAGS_TGR} -o $OUT/tgrally $OUT/obj/*.o $OUT/plat/tgr_syms.o $OUT/plat/arena.o $OBJS $LIBS
echo "linked $OUT/tgrally (host $HOST, renderer $RENDER)"
