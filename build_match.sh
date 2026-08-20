#!/bin/sh
# Boss Rally decomp -- matching build.
#
# Compiles each decomped C file with the ORIGINAL compiler (MSVC 5.0 via Wine)
# and diffs the resulting .obj code section against the extracted original
# function bytes.  A clean diff = the function matches bit-for-bit.
#
# PREREQUISITES:
#   brew install wine-stable
#   Copy VC5's compiler files into tools/msvc5/:
#     cl.exe, c1.exe, c2.exe, link.exe
#     include/   (CRT headers)
#     lib/       (CRT libraries)
#
# Usage:
#   sh build_match.sh                    -- build + diff everything
#   sh build_match.sh src/core/slice2_16.c  -- build + diff one file
set -e

MSVC="tools/msvc5"

# The VC5 files get staged one of two ways: flattened into tools/msvc5/ (what
# setup.sh's instructions describe), or as a wholesale copy of VC's bin/ (which
# keeps the original uppercase names).  Accept either.
for cand in "$MSVC/bin/cl.exe" "$MSVC/cl.exe"; do
    [ -f "$cand" ] && { CL_EXE="$cand"; break; }
done
if [ -z "$CL_EXE" ]; then
    echo "cl.exe not found under $MSVC/ -- run: sh setup.sh" >&2
    exit 1
fi

CL="sh tools/wine.sh $CL_EXE"
# msvc5-compat supplies stdint.h/stdbool.h, which VC5 predates.  It is tracked
# in git, unlike $MSVC/include, which setup.sh re-extracts from the disc.
CFLAGS="/nologo /O2 /W3 /I include /I tools/msvc5-compat /I $MSVC/include /DBR_MATCHING_BUILD"

mkdir -p build/match/obj build/match/verified

# Extract original function bytes if not already done
if [ ! -d build/match/orig ] || [ -z "$(ls build/match/orig/ 2>/dev/null)" ]; then
    python3 tools/extract_funcs.py orig/BRD3D.dll config/functions.csv build/match/orig/
fi

match_file() {
    src="$1"
    base=$(basename "$src" .c)
    obj="build/match/obj/$base.obj"

    echo "--- $base ---"

    # Drop any previous object FIRST.  Without this a compile that fails leaves
    # the last successful build in place, and the diff below happily reports
    # stale results as though they were fresh -- a failing build that looks
    # like a passing one is the worst possible outcome here.
    rm -f "$obj"

    # Compile.  cl.exe parses /Fo as a Windows path, so it needs backslashes --
    # given a forward-slash path it reports "cannot open compiler generated
    # file" and writes nothing.
    win_obj=$(echo "$obj" | tr '/' '\\')
    $CL $CFLAGS /c "$src" "/Fo$win_obj" 2>&1 | grep -v "^$" || true

    if [ ! -f "$obj" ]; then
        echo "  FAIL: compile error"
        return 1
    fi

    # Extract code section from .obj and compare against original bytes.
    # Each @implements in the source maps to a VA; the diff tool checks
    # each one against build/match/orig/0x<VA>.bin.
    python3 tools/match_diff.py "$src" "$obj" build/match/orig/ build/match/verified/
}

if [ -n "$1" ]; then
    match_file "$1"
else
    for src in $(find src/core -name '*.c' | sort); do
        # Skip files with no @implements -- nothing to match
        grep -q '@implements' "$src" 2>/dev/null || continue
        match_file "$src"
    done
fi
