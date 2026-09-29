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

# src/ and include/ are exactly what MSVC 5.0 compiles for the byte-matched
# build. The port's differences live in ports/macos/patch/*.port specs;
# portgen writes the port's copy of each spec'd module to build/port/src/ and
# the port's view of every header to build/port/include/ -- the only header
# directory the port compiles against. See ports/macos/tools/portgen.py.
python3 ports/macos/tools/portgen.py

# -fdeclspec: decomp TUs spell MSVC import prototypes (`__declspec(dllimport)`)
# for the matching build; on the Mac the attribute is parsed and ignored.
# Pointer-qualifier mismatches on callbacks (BR_STDCALL typedefs) are benign.
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-error=implicit-function-declaration -Wno-implicit-function-declaration -fdeclspec -Wno-ignored-attributes -Wno-error=incompatible-function-pointer-types -g -D_DARWIN_C_SOURCE -Ibuild/port/include -Itests -Iports/macos/include"
MFLAGS="-fobjc-arc -Wall -g -Ibuild/port/include -Iports/macos/include"
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
#
# src/ is the decomp and nothing else. The port's own code lives here:
#
#   ports/macos/core/<dir>/<file>.c  a port TU for src/core/<dir>/<file>.c --
#       it #includes the decomp module and supplies the port bodies for what
#       the module holds only in matching form. When one exists it is built
#       INSTEAD of the src file. With no src counterpart it is a port-only
#       module under the same responsibility layout.
#   ports/macos/legacy/sliceN_MM.c   the BRD3D-era transcription the port
#       still runs on, by address batch. Never part of the match.
#
# The object name is the module's PATH under its root with '/' turned into
# '_' -- gamedata/br_track.c becomes gamedata_br_track.o.
#
# It used to be the bare BASENAME, and that silently broke the moment two
# modules held the same filename: the second compile overwrote the first's
# object and the first module's symbols vanished from every link that wanted
# them. It happened for real. Filing work (rule 6) put a br_track.c in both
# gamedata/ and startup/; the startup one is entirely inside
# #ifdef BR_MATCHING_BUILD, so for the port it compiles to an EMPTY object,
# which then replaced the real track module and took the AI test link down
# with an undefined-symbol error pointing at neither file. Filing moves files
# by design, so this collision was going to keep happening.
#
# build.d/*.deps files still name modules by basename -- objname_find below
# resolves one to its object, and REFUSES to guess when two could match.
objname() { printf '%s' "$1" | sed 's#^src/core/##; s#^ports/macos/core/##; s#^ports/macos/legacy/##; s#\.c$##; s#/#_#g'; }

# Resolve a basename (as written in a .deps file) to exactly one object.
# Prints nothing if there is no match; aborts if there is more than one,
# because silently picking one is the bug this scheme exists to remove.
objfind() {
    _hits=""
    for _o in build/core/*.o; do
        [ -f "$_o" ] || continue
        _b=$(basename "$_o" .o)
        case "$_b" in
            "$1"|*_"$1") _hits="$_hits $_o";;
        esac
    done
    _n=0
    for _h in $_hits; do _n=$((_n+1)); done
    if [ "$_n" -gt 1 ]; then
        echo "build.sh: WARN '$1' is ambiguous -- matches:$_hits (using first)" >&2
    fi
    for _h in $_hits; do printf '%s' "$_h"; return; done
}

# Core objects live in their own directory, wiped each run. Keeping them apart
# from the test/port objects means the host link can just take ALL of them
# instead of excluding by filename pattern, and wiping means a renamed or
# deleted module cannot leave an object behind that still satisfies a link.
rm -rf build/core && mkdir -p build/core
# Port-only include dir (ports/macos/include) comes LAST so it only supplies
# what nothing else does; per-file pre-includes live beside it. Neither edits
# decomp source.
PORTINC="-Iports/macos/include"
# Every compile below is independent; run them JOBS at a time.
JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}
export CFLAGS PORTINC
# The TU list is portgen's: every src/core module (its build/port copy when
# it has a spec; a leading '_' is a scratch file and never part of the game),
# less any a ports/macos/core/ file replaces, plus ports/macos/core/ and
# ports/macos/legacy/. Each line is "OBJNAME PATH EXTRA-FLAGS...".
python3 ports/macos/tools/portgen.py --list | xargs -P "$JOBS" -L 1 sh -c '
    obj=$1; src=$2; shift 2
    clang $CFLAGS $PORTINC "$@" -c "$src" -o "build/core/$obj.o"' _
clang $MFLAGS -c ports/macos/metal/br_gfx_metal.m -o build/br_gfx_metal.o

# --- port-only: de-duplicate globals defined in two TUs --------------------
# Filing (rule 6) moves a function/global into a module but the origin
# address-batch slice sometimes keeps a copy. The matching build never links
# those two TUs together (the sweep compiles one file at a time), so the
# duplicate is invisible there -- but the port links the whole tree into one
# binary and ld rejects the second definition.
#
# This is drift in the DECOMP that only the port exposes, and the port must
# not edit decomp source. So we resolve it at the OBJECT level -- the dedup
# pass runs AFTER the build/host/* objects exist (below), because some
# duplicates are between a filed module and a host object. See dedup_globals.sh.
# A first pass over the core objects alone runs HERE, because the tests and
# brview below link core objects too and hit the same core/core duplicates.
sh ports/macos/dedup_globals.sh build/core

# --- tests -----------------------------------------------------------------
build_test() {
    t=$1
    tname=$(basename "$t" .c)
    mod=${tname#test_}
    if ! clang $CFLAGS -c "$t" -o "build/$tname.o" 2>/dev/null; then
        echo "WARN: $tname compile failed (skipping)"
        return
    fi

    objs="build/$tname.o"
    modobj=$(objfind "$mod")
    [ -z "$modobj" ] && modobj=$(objfind "br_$mod")
    [ -n "$modobj" ] && objs="$objs $modobj"
    if [ -f "build.d/$tname.deps" ]; then
        for d in $(cat "build.d/$tname.deps"); do
            depobj=$(objfind "$d")
            [ -z "$depobj" ] && continue
            case " $objs " in *" $depobj "*) continue;; esac
            objs="$objs $depobj"
        done
    fi
    if [ "$tname" = "test_gfx" ]; then
        clang $objs build/br_gfx_metal.o -lm $FW -o "build/$tname" 2>/dev/null || echo "WARN: $tname link failed (skipping)"
    else
        clang $objs -lm -o "build/$tname" 2>/dev/null || echo "WARN: $tname link failed (skipping)"
    fi
}
# Each test is its own subshell; JOBS in flight, then wait for the batch.
n=0
for t in tests/test_*.c; do
    case "$(basename "$t" .c)" in test_host_wiring|test_br_track) continue;; esac
    build_test "$t" &
    n=$((n+1))
    [ $((n % JOBS)) -eq 0 ] && wait
done
wait

# brview
clang $CFLAGS -c tools/brview.c -o build/brview.o
bvobjs="build/brview.o"
for d in $(cat build.d/brview.deps); do
    depobj=$(objfind "$d")
    [ -z "$depobj" ] && continue
    case " $bvobjs " in *" $depobj "*) continue;; esac
    bvobjs="$bvobjs $depobj"
done
clang $bvobjs build/br_gfx_metal.o -lm $FW -o build/brview

# --- the host: links the whole core into one runnable binary ---------------
# Undecomped functions are satisfied by ports/macos/br_stubs.c, so this
# links today and reports at exit which stubs the run actually reached.
mkdir -p build/host
clang $CFLAGS -DBR_HOST_LINK -c ports/macos/legacy/slice3_32.c -o build/host/slice3_32.o
clang $CFLAGS -DBR_HOST_LINK -c ports/macos/legacy/slice6_71.c -o build/host/slice6_71.o
clang $CFLAGS -DBR_HOST_LINK -c ports/macos/legacy/slice6_73.c -o build/host/slice6_73.o

# port-only de-dup of globals defined in two TUs (filing drift the whole-tree
# link exposes). Runs now that build/host/* exist, since some duplicates pair a
# host object with a filed module. Keep priority: host > module > slice.
sh ports/macos/dedup_globals.sh build/host build/core

WIREOBJS=""
for w in ports/macos/br_wire*.c; do
    [ -f "$w" ] || continue
    wname=$(basename "$w" .c)
    clang $CFLAGS -c "$w" -o "build/$wname.o"
    WIREOBJS="$WIREOBJS build/$wname.o"
done
clang $CFLAGS -c ports/macos/br_stubs.c -o build/br_stubs.o
clang $CFLAGS -c ports/macos/br_port_shims.c -o build/br_port_shims.o
clang $CFLAGS -c ports/macos/br_unresolved.c -o build/br_unresolved.o
# The raw-Ghidra driving sandbox (ports/macos/drive_sandbox.c), generated by
# gen_drive_sandbox.py + stub_uncompilable.py. Its own relaxed flags -- it is
# raw decompiler output, not clean tree code.
if [ -f ports/macos/drive_sandbox.c ]; then
    clang -std=c99 -w -g -Wno-int-conversion -Wno-implicit-function-declaration \
        -Wno-int-to-pointer-cast -c ports/macos/drive_sandbox.c -o build/drive_sandbox.o
fi
clang $CFLAGS -Itests -c tests/test_data.c -o build/test_data.o
clang "$(objfind br_data)" build/test_data.o -lm -o build/test_data
clang $CFLAGS -c ports/macos/brally.c      -o build/brally.o
clang $CFLAGS -c ports/macos/brally_main.c -o build/brally_main.o

# Every core object, minus the three that get rebuilt with -DBR_HOST_LINK just
# below. This used to glob all of build/ and exclude the test/port objects by
# filename pattern; now that core objects have their own directory the list is
# simply "all of them", and a new port file can never again land in the host
# link by failing to match an exclusion.
HOSTOBJS=""
for o in build/core/*.o; do
  case "$o" in
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
DRIVEOBJ=""
[ -f build/drive_sandbox.o ] && DRIVEOBJ="build/drive_sandbox.o"
HOSTLINK="build/br_stubs.o build/br_port_shims.o build/br_unresolved.o $DRIVEOBJ $WIREOBJS $HOSTOBJS \
      build/host/slice3_32.o build/host/slice6_71.o build/host/slice6_73.o \
      build/br_gfx_metal.o"
clang build/brally.o build/brally_main.o $HOSTLINK -lm $FW -o build/brally

# host test suite
clang $CFLAGS -c tests/test_host_wiring.c -o build/test_host_wiring.o
clang build/test_host_wiring.o build/brally.o $HOSTLINK -lm $FW \
      -o build/test_host_wiring
touch build/.build-ok

echo "built: brally (host)"
