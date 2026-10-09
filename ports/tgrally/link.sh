#!/bin/sh
# Link Top Gear Rally: the core (build.sh), the symbol table and arena
# generated from the source (tools/globals.py), the platform layer, and one
# host (shared with the Boss Rally port: ports/brally/platform/host).
#   env: HOST    null (default, headless) | macos | windows | ios (a static library,
#                libtgrally.a, for the iPhone app in ports/tgrally/ios: the host is
#                its Swift code, and main() is tgr_main, run on a thread of the app's)
#        RENDER  null (default) | soft | metal (needs HOST=macos) | vulkan (Windows; on
#                macOS through MoltenVK: VULKAN_SDK, else Homebrew's)
#        CC      a compiler aimed at Windows (ports/brally/tools/wincc.sh) links
#                tgrally.exe: host/win32 replaces host/posix
#        OUT     build directory (default build/tgrally/null-null/HOST-RENDER)
#        TGR_ROM your cartridge's ROM (default reference/tgrally/Top Gear Rally (USA).z64):
#                its data is built into the executable (tools/assets.py); the game
#                needs no ROM once built
#        ROMDATA embed (default) | file: the executable carries none of the
#                cartridge's data and reads romdata.bin at start-up
#                (os/romdata.c; what the release builder ships)
set -e
cd "$(dirname "$0")/../.."
HOST=${HOST:-null}
RENDER=${RENDER:-null}
OUT=${OUT:-build/tgrally/$HOST-$RENDER}
case "$OUT" in build/*) ;; *) echo "link: OUT must be under build/ (got $OUT)" >&2; exit 2;; esac
export OUT
CC=${CC:-clang}
# the target's own symbol and struct conventions for the generated tables
TGR_TARGET=$($CC -dumpmachine 2>/dev/null); export TGR_TARGET
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
ROMDATA=${ROMDATA:-embed}
if [ "$ROMDATA" = embed ]; then
  ${PYTHON:-.venv/bin/python} ports/tgrally/tools/assets.py $OUT/assets
  [ $OUT/plat/romdata.o -nt $OUT/assets/romdata.S ] || $CC -c $OUT/assets/romdata.S -o $OUT/plat/romdata.o
  BLOB=$OUT/plat/romdata.o
  PFLAGS="$PFLAGS -DTGR_ROMDATA_EMBED"
else
  BLOB=
fi

SRCS="$P/os/addr.c $P/os/romdata.c $P/os/lift.c $P/os/thread.c $P/os/io.c $P/os/si.c $P/os/main.c $P/os/view.c $P/os/touch.c $P/os/trace.c $P/os/sha1.c \
      $P/audio/mixer.c $P/audio/out.c $P/gfx/gfx.c $P/gfx/rcp.c $P/libc/xprintf.c $P/libc/bstring.c \
      ports/brally/platform/render/brr_png.c"
# the OS layer under the host: Windows or POSIX
case "$TGR_TARGET" in
  *mingw*|*windows*) OSHOST=$H/win32/host_win32.c; EXE=.exe; LD=${LD_TGR:-x86_64-w64-mingw32-gcc}
                     OSLIBS="-static -lws2_32"; ZLIB=;;
  *)                 OSHOST=$H/posix/host_posix.c; EXE=; LD=$CC; OSLIBS=; ZLIB=-lz;;
esac
SRCS="$SRCS $OSHOST"
case "$RENDER" in
  null)  SRCS="$SRCS $P/render/null/rdr_null.c";;
  soft)  SRCS="$SRCS $P/render/soft/rdr_soft.c";;
  metal) SRCS="$SRCS $P/render/metal/rdr_metal.m";;
  vulkan) SRCS="$SRCS $P/render/vulkan/rdr_vulkan.c";;
  *) echo "link: unknown RENDER $RENDER" >&2; exit 2;;
esac
VKLIBS=
if [ "$RENDER" = vulkan ]; then
  VK=${VULKAN_SDK:-$(brew --prefix 2>/dev/null)}
  PFLAGS="$PFLAGS -I$VK/include"
  if [ -n "$EXE" ]; then
    # Windows: vulkan-1.dll through an import library made from the functions
    # the renderer calls (no Vulkan SDK needed to build)
    { echo "LIBRARY vulkan-1.dll"; echo "EXPORTS"
      grep -o 'vk[A-Z][A-Za-z0-9]*(' $P/render/vulkan/rdr_vulkan.c | tr -d '(' | sort -u; } > $OUT/vulkan-1.def
    x86_64-w64-mingw32-dlltool -d $OUT/vulkan-1.def -l $OUT/libvulkan-1.a
    VKLIBS="-L$OUT -lvulkan-1"
  else
    VKLIBS="-L$VK/lib -lvulkan"
  fi
fi
LIBS="$ZLIB"
case "$HOST" in
  null)  SRCS="$SRCS $H/null/host_null.c";;
  macos) SRCS="$SRCS $H/macos/host_macos.m"
         LIBS="$LIBS -framework Cocoa -framework AudioToolbox -framework CoreAudio -framework GameController -framework QuartzCore -framework Metal -framework AVFoundation";;
  ios)   ;;                                    # the app's Swift (ports/tgrally/ios)
  windows) SRCS="$SRCS $H/windows/host_windows.c"
           LIBS="$LIBS -lgdi32 -luser32 -lshell32 -lole32 -luuid -lmfplat -lmfreadwrite -lmfuuid -lxinput9_1_0 -mwindows";;
  *) echo "link: unknown HOST $HOST" >&2; exit 2;;
esac
OBJS=
for s in $SRCS; do
  o=$OUT/plat/$(echo "$s" | sed 's#/#__#g').o
  case "$s" in */render/metal/*) XF=-fobjc-arc;; *) XF=;; esac   # the Metal renderer is ARC
  [ "$HOST" = ios ] && [ "$s" = $P/os/main.c ] && XF=-Dmain=tgr_main   # UIKit owns main()
  if [ "$s" = "$OSHOST" ] || [ "$s" = "$H/windows/host_windows.c" ]; then
    # the OS layer sees the real system headers, not the game's
    $CC -O2 ${GFLAG:--g} -std=gnu11 -Wall -I$H -c "$s" -o "$o"
  else
    $CC -c $PFLAGS $XF "$s" -o "$o"
  fi
  OBJS="$OBJS $o"
done
# the core objects of the TUs build.sh compiled (not a stale one of a deleted TU)
CORE=$(sed "s#ports/tgrally/src/##; s#/#__#g; s#^#$OUT/obj/#; s#\$#.o#" $OUT/tus.txt)
if [ -n "$EXE" ]; then          # Windows: name, version and manifest
  $H/windows/game_rc.sh "Top Gear Rally" "Top Gear Rally.exe" $OUT/plat/game.res.o
  OBJS="$OBJS $OUT/plat/game.res.o"
fi
if [ "$HOST" = ios ]; then
  rm -f $OUT/libtgrally.a
  ar rcs $OUT/libtgrally.a $CORE $OUT/plat/tgr_syms.o $OUT/plat/arena.o $BLOB $OBJS
  echo "archived $OUT/libtgrally.a (host ios, renderer $RENDER)"
  exit 0
fi
$LD ${LDFLAGS_TGR} -o $OUT/tgrally$EXE $CORE $OUT/plat/tgr_syms.o $OUT/plat/arena.o $BLOB $OBJS $LIBS $VKLIBS $OSLIBS
echo "linked $OUT/tgrally$EXE (host $HOST, renderer $RENDER)"
