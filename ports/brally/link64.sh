#!/bin/sh
# Link the portable 64-bit game: the core (build64.sh), the data lifted from
# the user's BRGlide.dll (tools/brally/datalift.py, generated under build/), and
# the platform layer for one host.
#   env: HOST   null (default, headless) | macos | windows
#        RENDER null (default) | soft | metal (metal needs HOST=macos) | vulkan
#               (Vulkan headers and loader: VULKAN_SDK, else Homebrew's;
#               on macOS it runs on MoltenVK)
#        DLL    the user's BRGlide.dll (default reference/brally/orig/BRGlide.dll)
#        IMAGE  embed (default) | runtime: the lifted data's bytes are read at
#               start-up from the CD root's BRGlide.dll instead of compiled in
#               (datalift.py --runtime-image; what the release builder ships)
#        CC     a compiler targeting Windows (x86_64-w64-mingw32) builds
#               brally64.exe: host/win32 replaces host/posix, LDCXX links
#               (default x86_64-w64-mingw32-g++)
#        WARN / LDFLAGS64  extra compile / link flags (an ASan build: WARN="-fsanitize=address -fsanitize-recover=address" LDFLAGS64=-fsanitize=address OUT=build/brally/null-asan)
set -e
cd "$(dirname "$0")/../.."
HOST=${HOST:-null}
RENDER=${RENDER:-null}
OUT=${OUT:-build/brally/$HOST-$RENDER}
case "$OUT" in build/*) ;; *) echo "link64: OUT must be under build/ (got $OUT)" >&2; exit 2;; esac
export OUT
CC=${CC:-clang}
P=ports/brally/platform
PFLAGS="-O2 ${GFLAG:--g} -std=gnu11 -Wall -Wno-unused-function -ffp-contract=off -I$P/include -I$P/host -I$P/render -I$P/common"
case "$($CC -dumpmachine 2>/dev/null)" in *mingw*|*windows*) PFLAGS="-include $P/include/br_winemu.h $PFLAGS -Ddllimport=";; esac
mkdir -p $OUT/plat

ports/brally/build64.sh
if grep -q '^FAIL' $OUT/compile.txt; then
  grep '^FAIL' $OUT/compile.txt >&2
  echo "link64: core TUs failed to compile; not linking" >&2
  exit 1
fi
python3 ports/brally/tools/datalift.py ${DLL:+--dll "$DLL"} $([ "${IMAGE:-embed}" = runtime ] && echo --runtime-image) >/dev/null
ports/brally/build64.sh $OUT/gen/br_data.c >/dev/null
# the script commands that read the game (core types, so core flags)
ports/brally/build64.sh ports/brally/platform/common/script_game.c | grep -v "^OK" >&2 || true

SRCS="$P/common/main.c $P/common/crt.c $P/common/win_kernel.c $P/common/win_user.c \
      $P/common/win_mm.c $P/common/win_rsrc.c $P/common/dx.c $P/common/dsound.c $P/common/audio.c $P/common/dplay.c $P/common/peersync.c $P/common/script.c $P/common/ear.c $P/common/flags.c $P/common/glide.c $P/common/data_image.c \
      $P/render/$RENDER/brr_$RENDER.* $P/render/brr_png.c"
# the OS layer under the host: Windows or POSIX
case "$($CC -dumpmachine 2>/dev/null)" in
  *mingw*|*windows*) OSHOST=$P/host/win32/host_win32.c; EXE=.exe
                     LDCXX=${LDCXX:-x86_64-w64-mingw32-g++}; OSLIBS="-static -lws2_32";;
  *)                 OSHOST=$P/host/posix/host_posix.c; EXE=; LDCXX=${LDCXX:-clang++}; OSLIBS=;;
esac
if [ "$RENDER" = vulkan ]; then
  VK=${VULKAN_SDK:-$(brew --prefix 2>/dev/null)}
  PFLAGS="$PFLAGS -I$VK/include"
  VKLIBS="-L$VK/lib -lvulkan"
  if [ -n "$EXE" ]; then
    # Windows: link against vulkan-1.dll through an import library made
    # from the functions the renderer calls (no Vulkan SDK needed to build)
    { echo "LIBRARY vulkan-1.dll"; echo "EXPORTS"
      grep -o 'vk[A-Z][A-Za-z0-9]*(' $P/render/vulkan/brr_vulkan.c | tr -d '(' | sort -u; } > $OUT/vulkan-1.def
    x86_64-w64-mingw32-dlltool -d $OUT/vulkan-1.def -l $OUT/libvulkan-1.a
    VKLIBS="-L$OUT -lvulkan-1"
  fi
fi
case "$HOST" in
  null)  SRCS="$SRCS $OSHOST $P/host/null/host_null.c";;
  windows) SRCS="$SRCS $OSHOST $P/host/windows/host_windows.c"
           LIBS="-lgdi32 -luser32 -lshell32 -lole32 -luuid -lmfplat -lmfreadwrite -lmfuuid -lxinput9_1_0 -mwindows";;
  macos) SRCS="$SRCS $OSHOST $P/host/macos/host_macos.m"
         LIBS="-framework Cocoa -framework Metal -framework QuartzCore -framework ImageIO -framework AudioToolbox -framework GameController";;
esac
OBJS=""
for s in $SRCS; do
  o=$OUT/plat/$(basename "$s").o
  case "$s" in *.m) X="-x objective-c -fobjc-arc";; *) X="";; esac
  if [ "$s" = "$OSHOST" ] || [ "$s" = "$P/host/windows/host_windows.c" ]; then
    # the OS layer sees the real system headers, not the game's Win32 surface
    $CC -O2 ${GFLAG:--g} -std=gnu11 -Wall -I$P/host -c "$s" -o "$o"
  else
    $CC $PFLAGS $X -c "$s" -o "$o"
  fi
  OBJS="$OBJS $o"
done
if [ -n "$EXE" ]; then          # Windows: name, version and manifest
  ports/brally/platform/host/windows/game_rc.sh "Boss Rally" "Boss Rally.exe" $OUT/plat/game.res.o
  OBJS="$OBJS $OUT/plat/game.res.o"
fi
$LDCXX ${LDFLAGS64} -o $OUT/brally64$EXE $OUT/obj/*.o $OBJS $LIBS $VKLIBS $OSLIBS
echo "linked $OUT/brally64$EXE (host $HOST, renderer $RENDER)"
