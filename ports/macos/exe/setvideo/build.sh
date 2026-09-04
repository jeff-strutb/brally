#!/bin/sh
# Build the macOS port of SetVideo.exe.
#
# Twenty-nine of SetVideo's 42 byte-exact functions are compiled straight out
# of src/exe/setvideo/ with BR_MATCHING_BUILD defined and a shim <windows.h>
# on the include path. The other thirteen (five dialog procedures, WinMain,
# the registry lookup, three CRT hooks) are replaced by setvideo_port.c.
#
# This produces a normal macOS binary. It is NOT byte-matched and carries no
# @implements tags; the match tooling never sees any of it.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
SRC="$ROOT/src/exe/setvideo"
OUT="$ROOT/build/macos/setvideo"

# The portable 29. The excluded thirteen are listed in setvideo_port.c.
MATCHED="
0x00401000 0x00401050 0x00401150 0x00401230 0x00401310 0x00401370
0x00401400 0x00401470 0x004014D0 0x00401560 0x004015B0 0x00401600
0x00401610 0x00401650 0x00401680 0x004016A0 0x004016D0 0x00401720
0x00401740 0x00401910 0x00401920 0x00401AC0 0x00401AD0 0x00401AF0
0x00401B10 0x00401B20 0x00402360 0x004023B0 0x00402CE0
"

CFLAGS="-std=c99 -O2 -g -DBR_MATCHING_BUILD -I$HERE/include -I$HERE
        -Wall -Wno-deprecated-declarations -Wno-unused-variable"

mkdir -p "$OUT/obj"

echo "building the byte-matched half ($(echo $MATCHED | wc -w | tr -d ' ') functions)"
OBJS=""
for f in $MATCHED; do
    clang $CFLAGS -c "$SRC/$f.c" -o "$OUT/obj/$f.o"
    OBJS="$OBJS $OUT/obj/$f.o"
done

echo "building the port driver"
clang $CFLAGS -c "$HERE/setvideo_port.c" -o "$OUT/obj/setvideo_port.o"

clang -o "$OUT/setvideo" $OBJS "$OUT/obj/setvideo_port.o"
echo "-> $OUT/setvideo"
