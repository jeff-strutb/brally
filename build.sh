#!/bin/sh
# Boss Rally decomp -- build (macOS port target).
#
# MODULES AND TESTS ARE DISCOVERED, NOT LISTED, AND THAT IS THE POINT.
#
# Drop a .c in src/core/ and its test in tests/ and they are built.
#
# If a test needs objects beyond its own module, it gets a ONE-LINE FILE of its
# own -- build.d/<test>.deps, listing module basenames. That keeps the extra
# dependency next to nothing else anyone edits, so two passes adding two
# different tests never touch the same file.
set -e
mkdir -p build build/host

CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -g -D_DARWIN_C_SOURCE -Iinclude -Itests"
MFLAGS="-fobjc-arc -Wall -g -Iinclude"
FW="-framework Metal -framework Foundation -framework AppKit -framework QuartzCore"

# --- modules ---------------------------------------------------------------
# MODULES ARE ORGANISED BY RESPONSIBILITY, and discovered recursively.
#
#   src/core/startup/    bring the game up and take it down
#   src/core/settings/   what the player chose, what the machine is
#   src/core/gamedata/   locate, read and decode the game's own files
#   src/core/geometry/   positions, orientations, and moving them
#   src/core/drawing/    turn geometry and images into pixels
#   src/core/scene/      what is in the world and where
#   src/core/driving/    how a car behaves
#   src/core/racing/     the rules of a race
#   src/core/menus/      the front end
#   src/core/controls/   reading what the player is doing
#   src/core/audio/      sound and music
#   src/core/            <- still named after an ADDRESS BATCH
#
# The object name is the BASENAME, so a module's folder can change without
# touching any build.d/*.deps file.
for src in $(find src/core -name '*.c' | sort); do
    clang $CFLAGS -c "$src" -o "build/$(basename "$src" .c).o"
done
clang $MFLAGS -c ports/macos/metal/br_gfx_metal.m -o build/br_gfx_metal.o

# --- tests -----------------------------------------------------------------
for t in tests/test_*.c; do
    tname=$(basename "$t" .c)
    mod=${tname#test_}
    if [ "$tname" = "test_host_wiring" ]; then continue; fi
    clang $CFLAGS -c "$t" -o "build/$tname.o"

    objs="build/$tname.o"
    if [ -f "build/$mod.o" ]; then
        objs="$objs build/$mod.o"
    elif [ -f "build/br_$mod.o" ]; then
        objs="$objs build/br_$mod.o"
    fi
    if [ -f "build.d/$tname.deps" ]; then
        for d in $(cat "build.d/$tname.deps"); do
            case " $objs " in *" build/$d.o "*) continue;; esac
            [ -f "build/$d.o" ] && objs="$objs build/$d.o"
        done
    fi
    if [ "$tname" = "test_gfx" ]; then
        clang $objs build/br_gfx_metal.o -lm $FW -o "build/$tname"
    else
        clang $objs -lm -o "build/$tname"
    fi
done

# brview
clang $CFLAGS -c tools/brview.c -o build/brview.o
bvobjs="build/brview.o"
for d in $(cat build.d/brview.deps); do
    case " $bvobjs " in *" build/$d.o "*) continue;; esac
    [ -f "build/$d.o" ] && bvobjs="$bvobjs build/$d.o"
done
clang $bvobjs build/br_gfx_metal.o -lm $FW -o build/brview

# --- the host: links the whole core into one runnable binary ---------------
# Undecomped functions are satisfied by ports/macos/br_stubs.c, so this
# links today and reports at exit which stubs the run actually reached.
mkdir -p build/host
clang $CFLAGS -DBR_HOST_LINK -c src/core/slice3_32.c -o build/host/slice3_32.o
clang $CFLAGS -DBR_HOST_LINK -c src/core/slice6_71.c -o build/host/slice6_71.o
clang $CFLAGS -DBR_HOST_LINK -c src/core/slice6_73.c -o build/host/slice6_73.o

WIREOBJS=""
for w in ports/macos/br_wire*.c; do
    [ -f "$w" ] || continue
    wname=$(basename "$w" .c)
    clang $CFLAGS -c "$w" -o "build/$wname.o"
    WIREOBJS="$WIREOBJS build/$wname.o"
done
clang $CFLAGS -c ports/macos/br_stubs.c -o build/br_stubs.o
clang $CFLAGS -Itests -c tests/test_data.c -o build/test_data.o
clang build/br_data.o build/test_data.o -lm -o build/test_data
clang $CFLAGS -c ports/macos/brally.c      -o build/brally.o
clang $CFLAGS -c ports/macos/brally_main.c -o build/brally_main.o

HOSTOBJS=""
for o in build/*.o; do
  case "$o" in
    *test_*|*brview*|*br_gfx_metal*|*brally.o|*brally_main.o|*br_stubs.o|*br_wire7*.o) continue;;
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
HOSTLINK="build/br_stubs.o $WIREOBJS $HOSTOBJS \
      build/host/slice3_32.o build/host/slice6_71.o build/host/slice6_73.o \
      build/br_gfx_metal.o"
clang build/brally.o build/brally_main.o $HOSTLINK -lm $FW -o build/brally

# host test suite
clang $CFLAGS -c tests/test_host_wiring.c -o build/test_host_wiring.o
clang build/test_host_wiring.o build/brally.o $HOSTLINK -lm $FW \
      -o build/test_host_wiring
touch build/.build-ok

echo "built: brally (host)"
