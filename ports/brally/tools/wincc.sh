#!/bin/sh
# wincc.sh: clang aimed at 64-bit Windows, with mingw-w64's headers and
# libraries (Homebrew's mingw-w64, or any x86_64-w64-mingw32-gcc on PATH).
# A Windows build of the game:
#   CC=ports/brally/tools/wincc.sh OUT=build/brally/windows-soft RENDER=soft ports/brally/link64.sh
# links build/brally/windows-soft/brally64.exe with x86_64-w64-mingw32-g++.
SR=$(x86_64-w64-mingw32-gcc -print-sysroot 2>/dev/null)/x86_64-w64-mingw32
[ -d "$SR/include" ] || { echo "wincc.sh: no x86_64-w64-mingw32 toolchain found" >&2; exit 1; }
# -march=x86-64-v2 (SSE4.2, any x64 PC since 2009) and -fno-math-errno let
# floorf, lrint and sqrtf compile to one instruction each, as they do on arm64;
# without them every texel of the software renderer calls into the UCRT DLL.
# Both are exact operations, so the results are the same either way.
exec clang --target=x86_64-w64-mingw32 --sysroot="$SR" -isystem "$SR/include" -march=x86-64-v2 -fno-math-errno "$@"
