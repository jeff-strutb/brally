#!/bin/sh
# menuicons.sh: the Remaster Options' menu icons (menuicon.py) from the source
# models in ports/common/assets, into ports/remastered/tgrally/platform/os/rm_icons.c
# (the icons as the game's own model format, big-endian, offsets from their start).
# Blender runs them one at a time (each source is large: ~8 GB at its peak).
#   crt: the PC's monitor, 2000 triangles, subdivided smooth, coloured on the middle mesh
#   n64: the console, 8000 triangles, from a printable model (Nintendo 64 by Oliver_Budd_ID,
#        thingiverse.com/thing:3534035, CC BY: ports/common/assets/n64-3534035), its two
#        halves put together, turned Z-up, fitted to n64.blend's proportions (the print is
#        flatter), only the faces seen from outside kept (a print is hollow inside); the
#        console's charcoal, its four sockets and cartridge slot grey (regions measured on
#        the print: n64.blend's texture lands a few millimetres off on it)
set -e
cd "$(dirname "$0")/../../.."
T=${TMPDIR:-/tmp}/menuicons.$$
mkdir -p $T
B=${BLENDER:-blender}
$B -b -P ports/remastered/tools/menuicon.py -- ports/common/assets/crt.blend crt $T/crt.bin 2000 HEIGHT=1160 \
    2>&1 | grep '^menuicon: wrote'
A=ports/common/assets/n64-3534035
# its sockets and cartridge slot, measured off straight-on renders of the print (fractions of its box)
SOCK=0.235,0.31:0.335,0.41:0.59,0.665:0.69,0.77
BOXES=$(for x in $(echo $SOCK | tr : ' '); do printf '%s,0,0.25,0.31,0.59,1;' $x; done)0.275,0.725,0.645,0.795,0.6,1
MENUICON_FIT=ports/common/assets/n64.blend MENUICON_FIT_COLOURS=0 MENUICON_ALIGN=z MENUICON_YUP=1 \
MENUICON_EXTERIOR=1 MENUICON_MID=20000 MENUICON_SUBSURF=0 MENUICON_COLOURFIRST=1 \
MENUICON_GREY=0.85,0.85,0.85 MENUICON_GREY_BOXES="$BOXES" \
    $B -b -P ports/remastered/tools/menuicon.py -- "$A/Nintendo_64_bottom_.STL,$A/Nintendo_64_top.STL" n64 $T/n64.bin \
    8000 WIDTH=2080 2>&1 | grep '^menuicon: wrote'
OUTC=ports/remastered/tgrally/platform/os/rm_icons.c
python3 - $T/crt.bin $T/n64.bin > $OUTC <<'PY'
import sys
print("/* rm_icons.c: the Remaster Options' menu icons, made by ports/remastered/tools/menuicons.sh")
print(" * from ports/common/assets (crt.blend; n64-3534035: Nintendo 64 by Oliver_Budd_ID, CC BY): each")
print(" * the game's own icon model format")
print(" * (tgr/model.h), big-endian, every address an offset from its start (rmoptions.c places")
print(" * and rebases them).  Generated: do not edit. */")
for name, path in (("crt", sys.argv[1]), ("n64", sys.argv[2])):
    b = open(path, "rb").read()
    print(f"const unsigned int rm_icon_{name}_len = {len(b)};")
    print(f"const unsigned char rm_icon_{name}[{len(b)}] = {{")
    for i in range(0, len(b), 24):
        print("    " + ",".join(str(x) for x in b[i:i + 24]) + ",")
    print("};")
PY
rm -rf $T
ls -la $OUTC
