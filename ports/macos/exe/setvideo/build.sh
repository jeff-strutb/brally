#!/bin/sh
# Build the macOS port of SetVideo.exe — the real Display Wizard.
#
# Thirty-eight of SetVideo's 42 byte-exact functions are compiled straight out
# of src/exe/setvideo/ with BR_MATCHING_BUILD defined and a shim <windows.h>
# on the include path — including WinMain and all five dialog procedures. The
# four that are not: the registry install-dir lookup and three MSVC CRT hooks.
#
# The wizard's dialog layouts live only inside the retail SetVideo.exe, so
# they are extracted at build time and never committed. Same policy as
# tools/extract_assets.sh: the content stays with whoever owns the game.
#
# The build works without the disc image; it just comes out headless, and says
# so. A missing asset must never look like a passing extraction.
#
# Usage: ports/macos/exe/setvideo/build.sh [path/to/BossRally.BIN]
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
SRC="$ROOT/src/exe/setvideo"
OUT="$ROOT/build/macos/setvideo"
GEN="$OUT/gen"

cd "$ROOT"
PY=python3
[ -x "$ROOT/.venv/bin/python" ] && PY="$ROOT/.venv/bin/python"

mkdir -p "$OUT/obj" "$GEN" testdata/setvideo work

# ---------------------------------------------------------------- assets ---

BIN="${1:-}"
if [ -z "$BIN" ]; then
    for c in reference/brally/BossRally.BIN \
             ../../../strutb/brally/reference/brally/BossRally.BIN; do
        [ -f "$c" ] && BIN="$c" && break
    done
fi

# The device database the wizard lists cards from. 78 sections on the retail
# disc; ports/macos/exe/setvideo/sample/ has a small hand-written stand-in.
if [ ! -f testdata/setvideo/BossRally.vdb ]; then
    if [ -n "$BIN" ] && [ -f "$BIN" ]; then
        $PY tools/extract_iso.py --extract-path "$BIN" BossRally.vdb \
            testdata/setvideo/BossRally.vdb
    else
        echo "assets: no disc image -- using the sample device database"
        cp "$HERE/sample/BossRally.vdb" testdata/setvideo/BossRally.vdb
    fi
else
    echo "assets: testdata/setvideo/BossRally.vdb already present"
fi

# The seven dialog templates. orig/SetVideo.exe is the reference copy; if it
# is not staged, pull the executable off the disc instead.
RSRC=""
if [ -f orig/SetVideo.exe ]; then
    RSRC=orig/SetVideo.exe
elif [ -n "$BIN" ] && [ -f "$BIN" ]; then
    [ -f work/SetVideo.exe ] || \
        $PY tools/extract_iso.py --extract-path "$BIN" SetVideo.exe \
            work/SetVideo.exe
    RSRC=work/SetVideo.exe
fi

if [ -n "$RSRC" ]; then
    $PY tools/rsrc_dump.py "$RSRC" --emit-c "$GEN/br_dialogres_gen.c"
else
    echo "assets: no SetVideo.exe -- building WITHOUT the wizard dialogs."
    echo "        The command-line modes work; running with no arguments will"
    echo "        report that no dialog templates are available."
    cat > "$GEN/br_dialogres_gen.c" <<'EOF'
/* GENERATED: no retail SetVideo.exe was available to read dialogs from. */
#include "br_dialogres.h"
const BrDlgTemplate br_dialogs[] = { { 0, "", 0, 0, 0, 0 } };
const int br_dialog_count = 0;
EOF
fi

# ----------------------------------------------------------------- build ---

# The 38 byte-matched functions. Excluded, with reasons, in setvideo_port.c.
MATCHED="
0x00401000 0x00401050 0x00401150 0x00401230 0x00401310 0x00401370
0x00401400 0x00401470 0x004014D0 0x00401560 0x004015B0 0x00401600
0x00401610 0x00401650 0x00401680 0x004016A0 0x004016D0 0x00401720
0x00401740 0x00401910 0x00401920 0x00401AC0 0x00401AD0 0x00401AF0
0x00401B10 0x00401B20 0x00401C10 0x00401C70 0x00401DC0 0x00401EC0
0x00401F00 0x00402030 0x00402160 0x00402260 0x00402360 0x004023B0
0x00402480 0x00402CE0
"

CFLAGS="-std=c99 -O2 -g -DBR_MATCHING_BUILD -I$HERE/include -I$HERE
        -Wall -Wno-deprecated-declarations -Wno-unused-variable
        -Wno-int-to-void-pointer-cast -Wno-pointer-to-int-cast"

echo "building the byte-matched half ($(echo $MATCHED | wc -w | tr -d ' ') functions)"
OBJS=""
for f in $MATCHED; do
    clang $CFLAGS -c "$SRC/$f.c" -o "$OUT/obj/$f.o"
    OBJS="$OBJS $OUT/obj/$f.o"
done

echo "building the port driver and the AppKit dialog shim"
clang $CFLAGS -c "$HERE/setvideo_port.c"   -o "$OUT/obj/setvideo_port.o"
clang $CFLAGS -c "$GEN/br_dialogres_gen.c" -o "$OUT/obj/br_dialogres_gen.o"
clang -std=gnu11 -O2 -g -fobjc-arc -I"$HERE/include" -I"$HERE" -Wall \
      -Wno-deprecated-declarations \
      -c "$HERE/win32_dialog.m" -o "$OUT/obj/win32_dialog.o"

clang -o "$OUT/setvideo" $OBJS \
      "$OUT/obj/setvideo_port.o" "$OUT/obj/br_dialogres_gen.o" \
      "$OUT/obj/win32_dialog.o" -framework Cocoa
echo "-> $OUT/setvideo"
