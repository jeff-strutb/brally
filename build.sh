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
# MODULES ARE ORGANISED BY RESPONSIBILITY, and discovered recursively.
#
#   port/src/startup/    bring the game up and take it down
#   port/src/settings/   what the player chose, what the machine is
#   port/src/gamedata/   locate, read and decode the game's own files
#   port/src/geometry/   positions, orientations, and moving them
#   port/src/drawing/    turn geometry and images into pixels
#   port/src/scene/      what is in the world and where
#   port/src/driving/    how a car behaves
#   port/src/racing/     the rules of a race
#   port/src/menus/      the front end
#   port/src/controls/   reading what the player is doing
#   port/src/audio/      sound and music
#   port/src/gfx/        the HOST Metal backend -- not game code
#   port/src/            <- still named after an ADDRESS BATCH
#
# Folders name WHAT a subsystem is responsible for, never the technique it uses
# or the layer it sits in. An earlier attempt used math/, physics/, platform/
# and render/ -- all of which name a discipline or a layer, and platform/ duly
# collected the entry point, the archive reader and the allocator, which share
# nothing but being unglamorous. See port/src/README.md.
#
# The last line is the point: a `sliceN_MM.c` is a batch of whatever happened
# to occupy one address range, so it mixes responsibilities and cannot be filed
# until its functions are split. Those files staying loose is the visible
# measure of that work, and it is deliberate that they look out of place.
#
# The object name is the BASENAME, so a module's folder can change without
# touching any build.d/*.deps file.
for src in $(find port/src -name '*.c' -not -path 'port/src/gfx/*' | sort); do
    clang $CFLAGS -c "$src" -o "build/$(basename "$src" .c).o"
done
clang $MFLAGS -c port/src/gfx/metal/br_gfx_metal.m -o build/br_gfx_metal.o

# --- tests -----------------------------------------------------------------
for t in port/tests/test_*.c; do
    tname=$(basename "$t" .c)
    mod=${tname#test_}
    # test_host_wiring is the one suite that links port/host, so it needs the
    # host's whole object set and cannot be matched to a single module by name.
    # It is built at the bottom of this script, off the SAME object list the
    # brally binary is linked from -- see there for why that matters.
    if [ "$tname" = "test_host_wiring" ]; then continue; fi
    clang $CFLAGS -c "$t" -o "build/$tname.o"

    # A test is matched to its module by name. Most are test_<mod> for <mod>.c,
    # but several modules carry the br_ prefix while their test does not
    # (test_race -> br_race.c, test_pod -> br_pod.c). Try both rather than
    # emitting a bare link error: a pass that adds test_foo.c for br_foo.c
    # should not have to know this rule, and the failure it produced was an
    # undefined-symbol dump with no hint of the cause.
    objs="build/$tname.o"
    if [ -f "build/$mod.o" ]; then
        objs="$objs build/$mod.o"
    elif [ -f "build/br_$mod.o" ]; then
        objs="$objs build/br_$mod.o"
    fi
    if [ -f "build.d/$tname.deps" ]; then
        for d in $(cat "build.d/$tname.deps"); do
            # Skip anything already on the line. Several .deps files name the
            # module the fallback above just added (test_audio.deps lists
            # br_audio, which IS test_audio's module), and listing an object
            # twice is a duplicate-symbol error, not a no-op.
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

# brview's objects come from build.d/brview.deps, NOT a line here.
# This line was hand-maintained and broke twice: a pass added a BrDlColourScale
# call to the Metal backend (needs br_dl.o), and br_dl.o in turn needs the
# clipper planes in slice1_03.o. Neither pass could have known -- brview is not
# their file. That is precisely the lock build.d/ exists to remove, so brview
# now uses it too and a pass that adds a dependency edits one line of its own.
clang $CFLAGS -c port/tools/brview.c -o build/brview.o
bvobjs="build/brview.o"
for d in $(cat build.d/brview.deps); do
    case " $bvobjs " in *" build/$d.o "*) continue;; esac
    [ -f "build/$d.o" ] && bvobjs="$bvobjs build/$d.o"
done
clang $bvobjs build/br_gfx_metal.o -lm $FW -o build/brview

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

# WIRING TUs ARE DISCOVERED, for the same reason modules and tests are.
#
# These four used to be named one clang line each, and that made port/host/ a
# lock exactly like the one this script's header describes: a pass told to add
# a new wiring TU would create the file, see a clean build, and never notice
# its code was not compiled at all -- the link succeeds because nothing
# references it yet. That is a silent no-op, which is worse than a link error.
# It nearly happened: a pass was briefed to add port/host/br_wireaudio.c.
WIREOBJS=""
for w in port/host/br_wire*.c; do
    [ -f "$w" ] || continue
    wname=$(basename "$w" .c)
    clang $CFLAGS -c "$w" -o "build/$wname.o"
    WIREOBJS="$WIREOBJS build/$wname.o"
done
clang $CFLAGS -c port/host/br_stubs.c -o build/br_stubs.o
# real definitions for the cross-module data objects (was br_stubs' 1 MiB blocks)
# br_data.c is already built by the recursive loop above; this line named its
# old flat path and broke the moment the tree was organised by concern. The
# object it produces is identical, so the rebuild is dropped rather than
# repathed -- one fewer explicit path to go stale.
clang $CFLAGS -Iport/tests -c port/tests/test_data.c -o build/test_data.o
clang build/br_data.o build/test_data.o -lm -o build/test_data
clang $CFLAGS -c port/host/brally.c      -o build/brally.o
# THE ENTRY POINT IS A SEPARATE OBJECT, and that separation is what puts the
# wiring under test. brally.c is now `main`-free, so it links into a test
# binary; brally_main.o supplies `main` to the game binary and to nothing else.
# See the banner in port/host/brally_main.c.
clang $CFLAGS -c port/host/brally_main.c -o build/brally_main.o

HOSTOBJS=""
for o in build/*.o; do
  case "$o" in
    *test_*|*brview*|*br_gfx_metal*|*brally.o|*brally_main.o|*br_stubs.o|*br_wire7*.o) continue;;
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;   # host uses build/host/ copies
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
HOSTLINK="build/br_stubs.o $WIREOBJS $HOSTOBJS \
      build/host/slice3_32.o build/host/slice6_71.o build/host/slice6_73.o \
      build/br_gfx_metal.o"
clang build/brally.o build/brally_main.o $HOSTLINK -lm $FW -o build/brally

# THE HOST'S OWN SUITE. Linked from $HOSTLINK -- the SAME list the binary above
# is linked from -- with test_host_wiring.o in place of brally_main.o. Building
# it from a hand-picked subset instead would let the suite pass against an
# object set the game never runs, which is the class of green report this
# project has already been burned by twice (see tools/regress.sh's header).
clang $CFLAGS -c port/tests/test_host_wiring.c -o build/test_host_wiring.o
clang build/test_host_wiring.o build/brally.o $HOSTLINK -lm $FW \
      -o build/test_host_wiring
# SUCCESS STAMP. Written ONLY here, as the very last act of a build that did
# not exit early -- `set -e` at the top guarantees any failure above skips it.
# tools/regress.sh refuses to report a pass count unless this stamp is newer
# than every source file, because this runner once printed "104 passed, 0
# failed" from stale binaries while this script was failing partway through,
# and that green report was used to confirm a change that had never been
# compiled. A test result about code that was not built is worse than none.
touch build/.build-ok

echo "built: brally (host)"
