#!/bin/sh
# Link Top Gear Rally Remastered: Top Gear Rally's menus with Boss Rally's
# engine running the races, one executable over one host (README.md).
#   the games:  Top Gear Rally (ports/tgrally/link.sh) and Boss Rally
#               (ports/brally/link64.sh) as Remastered changes them (tree.sh:
#               the two ports, its own files over them, its patches applied,
#               in OUT/tree), each built there as a library (HOST=lib) and
#               linked into one object with its own thunks to the host
#               (platform/thunks.c) and only its entry points global: the two
#               share some hundred function names
#   the host:   one real host under both (host/macos, host/windows, host/null, or
#               the iPhone app's Swift: ios/Sources), shared out by platform/mux.c;
#               platform/main.c is the process
#
#   env: HOST    null (default, headless: Linux has no window host yet) | macos | windows |
#                ios (libgames.a for the iPhone app, ios/build.sh: CC=ports/remastered/ios/cc.sh;
#                main() is rm_main, run on a thread of the app's)
#        RENDER  null (default) | metal (HOST=macos) | vulkan | gl (OpenGL 3.3: Windows'
#                fallback for a PC without Vulkan; offscreen on macOS, for checks) |
#                soft (deprecated: the CPU reference the renderers are checked against)
#        CC      a compiler aimed at Windows (ports/brally/tools/wincc.sh) links
#                "Top Gear Rally Remastered.exe"
#        OUT     build directory (default build/remastered/HOST-RENDER)
#        TGR_ROM Top Gear Rally's ROM, built in (ports/tgrally/link.sh)
#        DLL     Boss Rally's BRGlide.dll, its data built in (ports/brally/link64.sh)
set -e
cd "$(dirname "$0")/../.."
HOST=${HOST:-null}
RENDER=${RENDER:-null}
OUT=${OUT:-build/remastered/$HOST-$RENDER}
case "$OUT" in build/*) ;; *) echo "link: OUT must be under build/ (got $OUT)" >&2; exit 2;; esac
CC=${CC:-clang}
R=ports/remastered/platform
T=$OUT/tree                           # the two ports as Remastered builds them (tree.sh)
H=$T/ports/brally/platform/host
case "$($CC -dumpmachine 2>/dev/null)" in
  *mingw*|*windows*) OS=windows;;
  *apple*|*darwin*)  OS=macos;;
  *)                 OS=elf;;
esac
case "$HOST" in
  null) ;;
  macos) [ $OS = macos ] || { echo "link: HOST=macos needs a compiler for macOS" >&2; exit 2; };;
  windows) [ $OS = windows ] || { echo "link: HOST=windows needs CC=ports/brally/tools/wincc.sh" >&2; exit 2; };;
  ios) [ $OS = macos ] || { echo "link: HOST=ios needs CC=ports/remastered/ios/cc.sh" >&2; exit 2; };;
  *) echo "link: unknown HOST $HOST" >&2; exit 2;;
esac
mkdir -p $OUT/plat
case "$CC" in /*|*/*) CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC");; esac   # (the ports' scripts run in the tree)

# ---- the two games, as libraries ----------------------------------------------------
ports/remastered/tree.sh make $T
LIB=build/lib                         # under the tree
# Top Gear Rally twice: from the USA cartridge (Menus TGR (USA)) and from
# Japan's (TGR (PAL/Japan)), each with what it takes from the other
# cartridge (tools/regionextras.py: its front-end tune, its three cars); the
# second's entry points tgrp_* (platform/rm_tgrp.h).  The USA and Japan ROMs.
PY=${PYTHON:-.venv/bin/python}
$PY ports/remastered/tools/regionextras.py usa $OUT/extras-usa.bin
$PY ports/remastered/tools/regionextras.py jpn $OUT/extras-jpn.bin
RM_EXTRAS=$(pwd)/$OUT/extras-usa.bin OUT=$LIB/tgrally HOST=lib RENDER=$RENDER CC=$CC $T/ports/tgrally/link.sh
RM_EXTRAS=$(pwd)/$OUT/extras-jpn.bin TGR_REGION=jpn RM_CFLAGS="-include $(pwd)/$R/rm_tgrp.h -DTGR_RM_EN=1" \
  OUT=$LIB/tgrally-jpn HOST=lib RENDER=$RENDER CC=$CC $T/ports/tgrally/link.sh
OUT=$LIB/brally HOST=lib RENDER=$RENDER CC=$CC $T/ports/brally/link64.sh

# ---- each as one object: its thunks in, only its entry points global ----------------
XFLAGS=
# the window's Metal layer (host/macos, the iPhone app's Swift) for the Metal renderers
case "$HOST" in macos|ios) XFLAGS=-DMUX_METAL_LAYER;; esac
case $OS in macos) PFX=_;; *) PFX=;; esac
lump() {   # lump NAME LIB ENGINE SYMBOLS...
    name=$1; lib=$(pwd)/$2; eng=$3; shift 3
    d=$OUT/$name.x
    rm -rf $d && mkdir -p $d
    case $OS in windows) (cd $d && x86_64-w64-mingw32-ar x "$lib");; *) (cd $d && ar x "$lib");; esac
    $CC -O2 ${GFLAG:--g} -std=gnu11 -Wall -I$H $XFLAGS -DENGINE=$eng -c $R/thunks.c -o $d/zz_mux_thunks.o
    for s in "$@"; do echo "$PFX$s"; done > $OUT/$name.exp
    case $OS in
      macos)   $CC -r -nostdlib -Wl,-exported_symbols_list,$OUT/$name.exp -o $OUT/$name.o $d/*.o;;
      windows) x86_64-w64-mingw32-ld -r -o $OUT/$name.o $d/*.o
               # (clang's address-significance table: a link drops it, ld -r keeps it as a section)
               x86_64-w64-mingw32-objcopy --remove-section=.llvm_addrsig --keep-global-symbols=$OUT/$name.exp $OUT/$name.o;;
      elf)     ${LD_R:-ld} -r -o $OUT/$name.o $d/*.o
               ${OBJCOPY:-objcopy} --keep-global-symbols=$OUT/$name.exp $OUT/$name.o;;
    esac
    rm -rf $d
}
# (and what a touch host asks of each: ios/Sources/Bridge.h)
# (and the Remaster Options': options.c, mux.c)
lump tgrally $T/$LIB/tgrally/libtgrally.a tgr tgr_main tgr_race_engine tgr_menus_engine tgr_flag tgr_flag_set \
    tgr_view_driving tgr_touch_tap tgr_touch_swipe tgr_touch_press tgr_pointer tgr_pointer_wanted tgr_audio_lowpass tgr_race_speed \
    tgr_music_mute tgr_other_engine tgr_boot_to_menu
lump tgrally-jpn $T/$LIB/tgrally-jpn/libtgrally.a tgrp tgrp_main tgrp_race_engine tgrp_menus_engine tgrp_flag tgrp_flag_set \
    tgrp_view_driving tgrp_touch_tap tgrp_touch_swipe tgrp_touch_press tgrp_pointer tgrp_pointer_wanted tgrp_audio_lowpass \
    tgrp_race_speed tgrp_music_mute tgrp_other_engine tgrp_boot_to_menu
lump brally $T/$LIB/brally/libbrally.a br br_main br_race_serve br_race_run br_menus_run br_race_driving br_race_key \
    br_race_tap br_race_click br_flag_set br_music_gain br_effects_quiet br_music_volume

# ---- the host and the process -----------------------------------------------------------
SRCS="$R/mux.c $R/main.c $R/options.c $R/controls.c"
case $OS in
  windows) SRCS="$SRCS $H/win32/host_win32.c"; EXE=.exe; LDXX=${LDCXX:-x86_64-w64-mingw32-g++}
           OSLIBS="-static -lws2_32";;
  *)       SRCS="$SRCS $H/posix/host_posix.c"; EXE=; LDXX=${LDCXX:-clang++}; OSLIBS="-lz -lpthread";;
esac
LIBS=
case "$HOST" in
  null)    SRCS="$SRCS $H/null/host_null.c"
           # headless Metal (frames drawn offscreen): the frameworks the renderer uses
           [ "$RENDER" = metal ] && LIBS="-framework Metal -weak_framework MetalFX -framework QuartzCore -framework ImageIO -framework CoreGraphics -framework Foundation";;
  ios)     ;;                                  # the app's Swift (ios/Sources)
  macos)   SRCS="$SRCS $H/macos/host_macos.m"
           LIBS="-framework Cocoa -framework AudioToolbox -framework CoreAudio -framework GameController -framework CoreHaptics -framework QuartzCore -framework Metal -weak_framework MetalFX -framework ImageIO -framework AVFoundation";;
  windows) SRCS="$SRCS $H/windows/host_windows.c"
           LIBS="-lgdi32 -luser32 -lshell32 -lole32 -luuid -lmfplat -lmfreadwrite -lmfuuid -lxinput9_1_0 -mwindows";;
esac
VKLIBS=
if [ "$RENDER" = vulkan ]; then
  if [ $OS = windows ]; then
    # vulkan-1.dll through an import library made from the functions both
    # renderers call (no Vulkan SDK needed to build)
    { echo "LIBRARY vulkan-1.dll"; echo "EXPORTS"
      cat $T/ports/tgrally/platform/render/vulkan/rdr_vulkan.c $T/ports/brally/platform/render/vulkan/brr_vulkan.c |
        grep -o 'vk[A-Z][A-Za-z0-9]*(' | tr -d '(' | sort -u; } > $OUT/vulkan-1.def
    x86_64-w64-mingw32-dlltool -d $OUT/vulkan-1.def -l $OUT/libvulkan-1.a
    VKLIBS="-L$OUT -lvulkan-1"
  else
    VK=${VULKAN_SDK:-$(brew --prefix 2>/dev/null)}
    VKLIBS="-L$VK/lib -lvulkan"
  fi
fi
if [ "$RENDER" = gl ]; then   # (each game's renderer finds its functions at run time: gl_ctx.c)
  if [ $OS = windows ]; then VKLIBS="-lopengl32 -lgdi32"; else VKLIBS="-framework OpenGL"; fi
fi
OBJS=
for s in $SRCS; do
  o=$OUT/plat/$(basename "$s").o
  case "$s" in *.m) X="-x objective-c -fobjc-arc";; *) X=;; esac
  [ "$HOST" = ios ] && [ "$s" = $R/main.c ] && X=-Dmain=rm_main     # UIKit owns main()
  # the host and the multiplexer see the real system headers, not either game's
  # (the options see each game's switches: tgr_view.h, br_flags.h)
  [ "$s" = $R/options.c ] && X="-I$T/ports/tgrally/platform/include -I$T/ports/brally/platform/include"
  $CC -O2 ${GFLAG:--g} -std=gnu11 -Wall $X -I$H -I$R $XFLAGS -c "$s" -o "$o"
  OBJS="$OBJS $o"
done
if [ $OS = windows ]; then          # name, version and manifest
  $H/windows/game_rc.sh "Top Gear Rally Remastered" "Top Gear Rally Remastered.exe" $OUT/plat/game.res.o
  OBJS="$OBJS $OUT/plat/game.res.o"
  # the window's icon, beside the exe (host/windows reads icon.ico there)
  ${PYTHON:-.venv/bin/python} -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2], sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])" \
      $R/../icon.png $OUT/icon.ico
fi
if [ "$HOST" = ios ]; then
  rm -f $OUT/libgames.a
  ar rcs $OUT/libgames.a $OUT/tgrally.o $OUT/tgrally-jpn.o $OUT/brally.o $OBJS
  echo "archived $OUT/libgames.a (host ios, renderer $RENDER)"
  exit 0
fi
$LDXX ${LDFLAGS_RM} -o "$OUT/remastered$EXE" $OUT/tgrally.o $OUT/tgrally-jpn.o $OUT/brally.o $OBJS $LIBS $VKLIBS $OSLIBS
echo "linked $OUT/remastered$EXE (host $HOST, renderer $RENDER)"
