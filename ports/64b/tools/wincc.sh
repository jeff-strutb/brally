#!/bin/sh
# wincc.sh: clang aimed at 64-bit Windows, with mingw-w64's headers and
# libraries (Homebrew's mingw-w64, or any x86_64-w64-mingw32-gcc on PATH).
# A Windows build of the game:
#   CC=ports/64b/tools/wincc.sh OUT=build/portable_win RENDER=soft ports/64b/link64.sh
# links build/portable_win/brally64.exe with x86_64-w64-mingw32-g++.
SR=$(x86_64-w64-mingw32-gcc -print-sysroot 2>/dev/null)/x86_64-w64-mingw32
[ -d "$SR/include" ] || { echo "wincc.sh: no x86_64-w64-mingw32 toolchain found" >&2; exit 1; }
exec clang --target=x86_64-w64-mingw32 --sysroot="$SR" -isystem "$SR/include" "$@"
