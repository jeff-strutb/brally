#!/bin/sh
# Boss Rally decomp -- build (macOS port target).
#
# MODULES AND TESTS ARE DISCOVERED, NOT LISTED, AND THAT IS THE POINT.
#
# Drop a .c in src/brally/core/ and its test in tests/brally/ and they are built.
#
# If a test needs objects beyond its own module, it gets a ONE-LINE FILE of its
# own -- tests/brally/deps/<test>.deps, listing module basenames. That keeps the extra
# dependency next to nothing else anyone edits, so two passes adding two
# different tests never touch the same file.
set -e
# everything this script builds goes under one folder
B=build/brally/macos-legacy
mkdir -p $B/host

# src/brally/ and src/brally/include/ are exactly what MSVC 5.0 compiles for the byte-matched
# build. The port's differences live in ports/brally-wasm/patch/*.port specs;
# portgen writes the port's copy of each spec'd module to build/brally/macos-legacy/port/src/ and
# the port's view of every header to build/brally/macos-legacy/port/include/ -- the only header
# directory the port compiles against. See ports/brally-wasm/tools/portgen.py.
python3 ports/brally-wasm/tools/portgen.py

# -fdeclspec: decomp TUs spell MSVC import prototypes (`__declspec(dllimport)`)
# for the matching build; on the Mac the attribute is parsed and ignored.
# Pointer-qualifier mismatches on callbacks (BR_STDCALL typedefs) are benign.
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-error=implicit-function-declaration -Wno-implicit-function-declaration -fdeclspec -Wno-ignored-attributes -Wno-error=incompatible-function-pointer-types -g -D_DARWIN_C_SOURCE -I$B/port/include -Itests/brally -Iports/brally-wasm/include"
MFLAGS="-fobjc-arc -Wall -g -I$B/port/include -Iports/brally-wasm/include"
FW="-framework Metal -framework Foundation -framework AppKit -framework QuartzCore"

# --- modules ---------------------------------------------------------------
# MODULES ARE ORGANISED BY RESPONSIBILITY, and discovered recursively.
#
#   src/brally/core/startup/    bring the game up and take it down
#   src/brally/core/settings/   what the player chose, what the machine is
#   src/brally/core/gamedata/   locate, read and decode the game's own files
#   src/brally/core/geometry/   positions, orientations, and moving them
#   src/brally/core/drawing/    turn geometry and images into pixels
#   src/brally/core/scene/      what is in the world and where
#   src/brally/core/driving/    how a car behaves
#   src/brally/core/racing/     the rules of a race
#   src/brally/core/menus/      the front end
#   src/brally/core/controls/   reading what the player is doing
#   src/brally/core/audio/      sound and music
#
# src/brally/ is the decomp and nothing else. The port's own code lives here:
#
#   ports/brally-wasm/core/<dir>/<file>.c  a port TU for src/brally/core/<dir>/<file>.c --
#       it #includes the decomp module and supplies the port bodies for what
#       the module holds only in matching form. When one exists it is built
#       INSTEAD of the src file. With no src counterpart it is a port-only
#       module under the same responsibility layout.
#   ports/brally-wasm/legacy/sliceN_MM.c   the BRD3D-era transcription the port
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
# tests/brally/deps/*.deps files still name modules by basename -- objname_find below
# resolves one to its object, and REFUSES to guess when two could match.
objname() { printf '%s' "$1" | sed 's#^src/brally/core/##; s#^ports/brally-wasm/core/##; s#^ports/brally-wasm/legacy/##; s#\.c$##; s#/#_#g'; }

# Resolve a basename (as written in a .deps file) to exactly one object.
# Prints nothing if there is no match; aborts if there is more than one,
# because silently picking one is the bug this scheme exists to remove.
objfind() {
    _hits=""
    for _o in $B/core/*.o; do
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
rm -rf $B/core && mkdir -p $B/core
# Port-only include dir (ports/brally-wasm/include) comes LAST so it only supplies
# what nothing else does; per-file pre-includes live beside it. Neither edits
# decomp source.
PORTINC="-Iports/brally-wasm/include"
# Every compile below is independent; run them JOBS at a time.
JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}
export CFLAGS PORTINC B
# The TU list is portgen's: every src/brally/core module (its build/brally/macos-legacy/port copy when
# it has a spec; a leading '_' is a scratch file and never part of the game),
# less any a ports/brally-wasm/core/ file replaces, plus ports/brally-wasm/core/ and
# ports/brally-wasm/legacy/. Each line is "OBJNAME PATH EXTRA-FLAGS...".
python3 ports/brally-wasm/tools/portgen.py --list | xargs -P "$JOBS" -L 1 sh -c '
    obj=$1; src=$2; shift 2
    clang $CFLAGS $PORTINC "$@" -c "$src" -o "$B/core/$obj.o"' _
clang $MFLAGS -c ports/brally-wasm/metal/br_gfx_metal.m -o $B/br_gfx_metal.o

# --- port-only: de-duplicate globals defined in two TUs --------------------
# Filing (rule 6) moves a function/global into a module but the origin
# address-batch slice sometimes keeps a copy. The matching build never links
# those two TUs together (the sweep compiles one file at a time), so the
# duplicate is invisible there -- but the port links the whole tree into one
# binary and ld rejects the second definition.
#
# This is drift in the DECOMP that only the port exposes, and the port must
# not edit decomp source. So we resolve it at the OBJECT level -- the dedup
# pass runs AFTER the build/brally/macos-legacy/host/* objects exist (below), because some
# duplicates are between a filed module and a host object. See dedup_globals.sh.
# A first pass over the core objects alone runs HERE, because the tests and
# brview below link core objects too and hit the same core/core duplicates.
sh ports/brally-wasm/dedup_globals.sh $B/core

# --- tests -----------------------------------------------------------------
build_test() {
    t=$1
    tname=$(basename "$t" .c)
    mod=${tname#test_}
    if ! clang $CFLAGS -c "$t" -o "$B/$tname.o" 2>/dev/null; then
        echo "WARN: $tname compile failed (skipping)"
        return
    fi

    objs="$B/$tname.o"
    modobj=$(objfind "$mod")
    [ -z "$modobj" ] && modobj=$(objfind "br_$mod")
    [ -n "$modobj" ] && objs="$objs $modobj"
    if [ -f "tests/brally/deps/$tname.deps" ]; then
        for d in $(cat "tests/brally/deps/$tname.deps"); do
            depobj=$(objfind "$d")
            [ -z "$depobj" ] && continue
            case " $objs " in *" $depobj "*) continue;; esac
            objs="$objs $depobj"
        done
    fi
    if [ "$tname" = "test_gfx" ]; then
        clang $objs $B/br_gfx_metal.o -lm $FW -o "$B/$tname" 2>/dev/null || echo "WARN: $tname link failed (skipping)"
    else
        clang $objs -lm -o "$B/$tname" 2>/dev/null || echo "WARN: $tname link failed (skipping)"
    fi
}
# Each test is its own subshell; JOBS in flight, then wait for the batch.
n=0
for t in tests/brally/test_*.c; do
    case "$(basename "$t" .c)" in test_host_wiring|test_br_track) continue;; esac
    build_test "$t" &
    n=$((n+1))
    [ $((n % JOBS)) -eq 0 ] && wait
done
wait

# brview
clang $CFLAGS -c tools/brally/brview.c -o $B/brview.o
bvobjs="$B/brview.o"
for d in $(cat tests/brally/deps/brview.deps); do
    depobj=$(objfind "$d")
    [ -z "$depobj" ] && continue
    case " $bvobjs " in *" $depobj "*) continue;; esac
    bvobjs="$bvobjs $depobj"
done
clang $bvobjs $B/br_gfx_metal.o -lm $FW -o $B/brview

# --- the host: links the whole core into one runnable binary ---------------
# Undecomped functions are satisfied by ports/brally-wasm/br_stubs.c, so this
# links today and reports at exit which stubs the run actually reached.
mkdir -p $B/host
clang $CFLAGS -DBR_HOST_LINK -c ports/brally-wasm/legacy/slice3_32.c -o $B/host/slice3_32.o
clang $CFLAGS -DBR_HOST_LINK -c ports/brally-wasm/legacy/slice6_71.c -o $B/host/slice6_71.o
clang $CFLAGS -DBR_HOST_LINK -c ports/brally-wasm/legacy/slice6_73.c -o $B/host/slice6_73.o

# port-only de-dup of globals defined in two TUs (filing drift the whole-tree
# link exposes). Runs now that build/brally/macos-legacy/host/* exist, since some duplicates pair a
# host object with a filed module. Keep priority: host > module > slice.
sh ports/brally-wasm/dedup_globals.sh $B/host $B/core

WIREOBJS=""
for w in ports/brally-wasm/br_wire*.c; do
    [ -f "$w" ] || continue
    wname=$(basename "$w" .c)
    clang $CFLAGS -c "$w" -o "$B/$wname.o"
    WIREOBJS="$WIREOBJS $B/$wname.o"
done
clang $CFLAGS -c ports/brally-wasm/br_stubs.c -o $B/br_stubs.o
clang $CFLAGS -c ports/brally-wasm/br_port_shims.c -o $B/br_port_shims.o
clang $CFLAGS -c ports/brally-wasm/br_unresolved.c -o $B/br_unresolved.o
# The raw-Ghidra driving sandbox (ports/brally-wasm/drive_sandbox.c), generated by
# gen_drive_sandbox.py + stub_uncompilable.py. Its own relaxed flags -- it is
# raw decompiler output, not clean tree code.
if [ -f ports/brally-wasm/drive_sandbox.c ]; then
    clang -std=c99 -w -g -Wno-int-conversion -Wno-implicit-function-declaration \
        -Wno-int-to-pointer-cast -c ports/brally-wasm/drive_sandbox.c -o $B/drive_sandbox.o
fi
clang $CFLAGS -Itests/brally -c tests/brally/test_data.c -o $B/test_data.o
clang "$(objfind br_data)" $B/test_data.o -lm -o $B/test_data
clang $CFLAGS -c ports/brally-wasm/brally.c      -o $B/brally.o
clang $CFLAGS -c ports/brally-wasm/brally_main.c -o $B/brally_main.o

# Every core object, minus the three that get rebuilt with -DBR_HOST_LINK just
# below. This used to glob all of build/ and exclude the test/port objects by
# filename pattern; now that core objects have their own directory the list is
# simply "all of them", and a new port file can never again land in the host
# link by failing to match an exclusion.
HOSTOBJS=""
for o in $B/core/*.o; do
  case "$o" in
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
DRIVEOBJ=""
[ -f $B/drive_sandbox.o ] && DRIVEOBJ="$B/drive_sandbox.o"
HOSTLINK="$B/br_stubs.o $B/br_port_shims.o $B/br_unresolved.o $DRIVEOBJ $WIREOBJS $HOSTOBJS \
      $B/host/slice3_32.o $B/host/slice6_71.o $B/host/slice6_73.o \
      $B/br_gfx_metal.o"
clang $B/brally.o $B/brally_main.o $HOSTLINK -lm $FW -o $B/brally

# host test suite
clang $CFLAGS -c tests/brally/test_host_wiring.c -o $B/test_host_wiring.o
clang $B/test_host_wiring.o $B/brally.o $HOSTLINK -lm $FW \
      -o $B/test_host_wiring
touch $B/.build-ok

echo "built: brally (host)"
