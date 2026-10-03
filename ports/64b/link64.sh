#!/bin/sh
# Link the portable 64-bit game: the core (build64.sh), the data lifted from
# the user's BRGlide.dll (tools/datalift.py, generated under build/), and
# the platform layer for one host.
#   env: HOST   null (default, headless) | macos
#        RENDER null (default) | soft | metal (metal needs HOST=macos)
#        DLL    the user's BRGlide.dll (default orig/BRGlide.dll)
#        WARN / LDFLAGS64  extra compile / link flags (an ASan build: WARN="-fsanitize=address -fsanitize-recover=address" LDFLAGS64=-fsanitize=address OUT=build/portable_asan)
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/portable}
HOST=${HOST:-null}
RENDER=${RENDER:-null}
CC=${CC:-clang}
P=ports/64b/platform
PFLAGS="-O2 ${GFLAG:--g} -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -I$P/include -I$P/host -I$P/render -I$P/common"
mkdir -p $OUT/plat

ports/64b/build64.sh
if grep -q '^FAIL' $OUT/compile.txt; then
  grep '^FAIL' $OUT/compile.txt >&2
  echo "link64: core TUs failed to compile; not linking" >&2
  exit 1
fi
python3 ports/64b/tools/datalift.py ${DLL:+--dll "$DLL"} >/dev/null
ports/64b/build64.sh $OUT/gen/br_data.c >/dev/null
# the script commands that read the game (core types, so core flags)
ports/64b/build64.sh ports/64b/platform/common/script_game.c | grep -v "^OK" >&2 || true

SRCS="$P/common/main.c $P/common/crt.c $P/common/win_kernel.c $P/common/win_user.c \
      $P/common/win_mm.c $P/common/win_rsrc.c $P/common/dx.c $P/common/dsound.c $P/common/audio.c $P/common/dplay.c $P/common/script.c $P/common/ear.c $P/common/glide.c \
      $P/render/$RENDER/brr_$RENDER.*"
case "$HOST" in
  null)  SRCS="$SRCS $P/host/posix/host_posix.c $P/host/null/host_null.c";;
  macos) SRCS="$SRCS $P/host/posix/host_posix.c $P/host/macos/host_macos.m"
         LIBS="-framework Cocoa -framework Metal -framework QuartzCore -framework ImageIO -framework AudioToolbox";;
esac
OBJS=""
for s in $SRCS; do
  o=$OUT/plat/$(basename "$s").o
  case "$s" in *.m) X="-x objective-c -fobjc-arc";; *) X="";; esac
  $CC $PFLAGS $X -c "$s" -o "$o"
  OBJS="$OBJS $o"
done
clang++ ${LDFLAGS64} -o $OUT/brally64 $OUT/obj/*.o $OBJS $LIBS
echo "linked $OUT/brally64 (host $HOST, renderer $RENDER)"
