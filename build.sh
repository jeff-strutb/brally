#!/bin/sh
# Boss Rally port -- build. Requires only clang + macOS SDK.
#
# MODULES AND TESTS ARE DISCOVERED, NOT LISTED, AND THAT IS THE POINT.
#
# This script used to name every module and every test explicitly -- 235 clang
# lines. Every parallel pass had to edit it to add its files, so every pass
# collided with every other one, and batch after batch reported the tree
# transiently unbuildable "through no action of mine". The build file was a
# lock, and it was the main thing serialising parallel work.
#
# Now: drop a .c in port/src/ and its test in port/tests/ and they are built.
#
# If a test needs objects beyond its own module, it gets a ONE-LINE FILE of its
# own -- build.d/<test>.deps, listing module basenames. That keeps the extra
# dependency next to nothing else anyone edits, so two passes adding two
# different tests never touch the same file.
#
# Why not one archive and let the linker choose? Tried, and it does not work
# here: many tests deliberately define their own stand-ins for dependencies
# (test_slice2_20 has its own BrVec3Sub), and an archive supplies the real one
# as well, so every such test collides with itself. The explicit list is not
# clutter -- it is what lets a test substitute a dependency.
set -e
mkdir -p build build/host

CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -g -D_DARWIN_C_SOURCE -Iport/include -Iport/src/gfx -Iport/tests"
MFLAGS="-fobjc-arc -Wall -g -Iport/include -Iport/src/gfx"
FW="-framework Metal -framework Foundation -framework AppKit -framework QuartzCore"

# --- modules ---------------------------------------------------------------
for src in port/src/*.c; do
    clang $CFLAGS -c "$src" -o "build/$(basename "$src" .c).o"
done
clang $MFLAGS -c port/src/gfx/metal/br_gfx_metal.m -o build/br_gfx_metal.o

# --- tests -----------------------------------------------------------------
for t in port/tests/test_*.c; do
    tname=$(basename "$t" .c)
    mod=${tname#test_}
    clang $CFLAGS -c "$t" -o "build/$tname.o"

    objs="build/$tname.o"
    [ -f "build/$mod.o" ] && objs="$objs build/$mod.o"
    if [ -f "build.d/$tname.deps" ]; then
        for d in $(cat "build.d/$tname.deps"); do
            [ -f "build/$d.o" ] && objs="$objs build/$d.o"
        done
    fi
    if [ "$tname" = "test_gfx" ]; then
        clang $objs build/br_gfx_metal.o -lm $FW -o "build/$tname"
    else
        clang $objs -lm -o "build/$tname"
    fi
done

clang $CFLAGS -c port/tools/brview.c -o build/brview.o
clang build/brview.o build/br_img.o build/br_gfx_metal.o -lm $FW -o build/brview

# --- the host: links the whole ported core into one runnable binary ---------
# Unported functions are satisfied by port/host/br_stubs.c, so this links today
# and reports at exit which stubs the run actually reached. That report is the
# work queue: it is measured from a real boot rather than guessed.
# slice6_71 and slice6_73 each duplicate a function slice6_70 also defines.
# Their TESTS link each module alone and need the real names, so the plain
# objects above keep them. The host links all three at once, so it needs the
# renamed copies -- built here, into a subdirectory the host glob skips.
mkdir -p build/host
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice3_32.c -o build/host/slice3_32.o
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice6_71.c -o build/host/slice6_71.o
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice6_73.c -o build/host/slice6_73.o

clang $CFLAGS -c port/host/br_wire71.c -o build/br_wire71.o
clang $CFLAGS -c port/host/br_wire72.c -o build/br_wire72.o
clang $CFLAGS -c port/host/br_wire75.c -o build/br_wire75.o
clang $CFLAGS -c port/host/br_wire77.c -o build/br_wire77.o
clang $CFLAGS -c port/host/br_stubs.c -o build/br_stubs.o
# real definitions for the cross-module data objects (was br_stubs' 1 MiB blocks)
clang $CFLAGS -c port/src/br_data.c -o build/br_data.o
clang $CFLAGS -Iport/tests -c port/tests/test_data.c -o build/test_data.o
clang build/br_data.o build/test_data.o -lm -o build/test_data
clang $CFLAGS -c port/host/brally.c   -o build/brally.o

HOSTOBJS=""
for o in build/*.o; do
  case "$o" in
    *test_*|*brview*|*br_gfx_metal*|*brally.o|*br_stubs.o|*br_wire7*.o) continue;;
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;   # host uses build/host/ copies
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
clang build/brally.o build/br_stubs.o build/br_wire71.o build/br_wire72.o build/br_wire75.o build/br_wire77.o $HOSTOBJS \
      build/host/slice3_32.o build/host/slice6_71.o build/host/slice6_73.o \
      build/br_gfx_metal.o -lm $FW -o build/brally
echo "built: brally (host)"
