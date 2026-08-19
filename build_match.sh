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
CL="wine $MSVC/cl.exe"
CFLAGS="/nologo /O2 /W3 /I include /I $MSVC/include /DBR_MATCHING_BUILD"

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

    # Compile
    $CL $CFLAGS /c "$src" /Fo"$obj" 2>&1 | grep -v "^$" || true

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
