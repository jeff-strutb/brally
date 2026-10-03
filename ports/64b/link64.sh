#!/bin/sh
# Link the portable 64-bit game: the core (build64.sh), the data lifted from
# the user's BRGlide.dll (tools/datalift.py, generated under build/), and
# the platform layer for one host.
#   env: HOST   null (default, headless) | macos
#        RENDER null (default) | soft | metal (metal needs HOST=macos) | vulkan
#               (Vulkan headers and loader: VULKAN_SDK, else Homebrew's;
#               on macOS it runs on MoltenVK)
#        DLL    the user's BRGlide.dll (default orig/BRGlide.dll)
#        CC     a compiler targeting Windows (x86_64-w64-mingw32) builds
#               brally64.exe: host/win32 replaces host/posix, LDCXX links
#               (default x86_64-w64-mingw32-g++)
#        WARN / LDFLAGS64  extra compile / link flags (an ASan build: WARN="-fsanitize=address -fsanitize-recover=address" LDFLAGS64=-fsanitize=address OUT=build/portable_asan)
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/portable}
HOST=${HOST:-null}
RENDER=${RENDER:-null}
CC=${CC:-clang}
P=ports/64b/platform
PFLAGS="-O2 ${GFLAG:--g} -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -I$P/include -I$P/host -I$P/render -I$P/common"
case "$($CC -dumpmachine 2>/dev/null)" in *mingw*|*windows*) PFLAGS="-include $P/include/br_winemu.h $PFLAGS -Ddllimport=";; esac
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
      $P/common/win_mm.c $P/common/win_rsrc.c $P/common/dx.c $P/common/dsound.c $P/common/audio.c $P/common/dplay.c $P/common/peersync.c $P/common/script.c $P/common/ear.c $P/common/glide.c \
      $P/render/$RENDER/brr_$RENDER.* $P/render/brr_png.c"
if [ "$RENDER" = vulkan ]; then
  VK=${VULKAN_SDK:-$(brew --prefix 2>/dev/null)}
  PFLAGS="$PFLAGS -I$VK/include"
  VKLIBS="-L$VK/lib -lvulkan"
fi
# the OS layer under the host: Windows or POSIX
case "$($CC -dumpmachine 2>/dev/null)" in
  *mingw*|*windows*) OSHOST=$P/host/win32/host_win32.c; EXE=.exe
                     LDCXX=${LDCXX:-x86_64-w64-mingw32-g++}; OSLIBS="-static -lws2_32";;
  *)                 OSHOST=$P/host/posix/host_posix.c; EXE=; LDCXX=${LDCXX:-clang++}; OSLIBS=;;
esac
case "$HOST" in
  null)  SRCS="$SRCS $OSHOST $P/host/null/host_null.c";;
  macos) SRCS="$SRCS $OSHOST $P/host/macos/host_macos.m"
         LIBS="-framework Cocoa -framework Metal -framework QuartzCore -framework ImageIO -framework AudioToolbox -framework GameController";;
esac
OBJS=""
for s in $SRCS; do
  o=$OUT/plat/$(basename "$s").o
  case "$s" in *.m) X="-x objective-c -fobjc-arc";; *) X="";; esac
  if [ "$s" = "$OSHOST" ]; then
    # the OS layer sees the real system headers, not the game's Win32 surface
    $CC -O2 ${GFLAG:--g} -std=gnu11 -Wall -I$P/host -c "$s" -o "$o"
  else
    $CC $PFLAGS $X -c "$s" -o "$o"
  fi
  OBJS="$OBJS $o"
done
$LDCXX ${LDFLAGS64} -o $OUT/brally64$EXE $OUT/obj/*.o $OBJS $LIBS $VKLIBS $OSLIBS
echo "linked $OUT/brally64$EXE (host $HOST, renderer $RENDER)"
