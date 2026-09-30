#!/bin/sh
# Build "Boss Rally.app": the 32-bit lane (build_wasm.sh) plus everything it
# reads at run time, extracted once from the builder's own media, so the app
# runs with no disc image, no ROM and no source tree beside it.
#
#   Contents/MacOS/brally           the build_wasm.sh binary
#   Contents/Resources/portdata.bin the port's data segments
#   Contents/Resources/disc/        the whole data track (the CD root the game
#                                   opens tracks/, cars/, sfx/ ... under; its
#                                   BRGlide.dll is the image w_init loads)
#   Contents/Resources/music/cd/    the PC soundtrack, CD audio tracks 2..13
#   Contents/Resources/music/n64/   the N64 soundtrack: the ROM's six modules
#                                   and their cues
#   Contents/Resources/assets/      the PC and N64 version icons (Tab)
#   Contents/Resources/remaster/    the Remastered player car's model and maps
#   Contents/Resources/Licenses/    libopenmpt and the codecs linked with it
#
# Sources, all required (a partial app must not look like a complete one):
#   --bin  BossRally.BIN (its .cue beside it)  default reference/brally/
#   --rom  Top Gear Rally (USA).z64            default reference/tgrally/
# The app plays the ROM's modules. Parked, off unless asked for: --ost DIR
# (the composer's recordings of the six pieces, FLAC) plays those instead,
# looped where each recording repeats (ost_loops.py; needs numpy in
# python3). The recordings carry the PAL capture hardware's noise, so they
# are not used by default.
# Also off unless asked for: --hq-samples DIR swaps seven of the modules'
# samples for better copies of the same recordings (upgrade_samples.py,
# hq_samples.json; DIR holds those WAVs, e.g. reference/tgrally/XM). By
# default the modules are exactly the ROM's.
# ffmpeg on PATH (FLAC encoder for the CD audio), and Homebrew's libopenmpt,
# mpg123, libogg and libvorbis (linked statically by build_wasm.sh).
#
# Extraction is cached in build/app/extract, keyed on the MD5 of every source,
# so repackaging after a code change does not re-rip ~300 MB of audio.
#
# Usage: ports/macos/wasm/package_app.sh [--bin X] [--rom Y] [--ost D] [--hq-samples D] [--no-build]
set -e
cd "$(dirname "$0")/../../.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3

BIN=reference/brally/BossRally.BIN
ROM="reference/tgrally/Top Gear Rally (USA).z64"
OST=
HQ=
BUILD=1
while [ $# -gt 0 ]; do
    case $1 in
        --bin) BIN=$2; shift 2 ;;
        --rom) ROM=$2; shift 2 ;;
        --ost) OST=$2; shift 2 ;;
        --hq-samples) HQ=$2; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        *) echo "package_app: unknown argument $1" >&2; exit 2 ;;
    esac
done
CUE=
for c in "${BIN%.*}.cue" "${BIN%.*}.CUE"; do
    [ -f "$c" ] && CUE=$c && break
done
[ -f "$BIN" ] || { echo "package_app: no disc image at $BIN" >&2; exit 1; }
[ -n "$CUE" ] || { echo "package_app: no .cue beside $BIN (the soundtrack needs it)" >&2; exit 1; }
[ -f "$ROM" ] || { echo "package_app: no Top Gear Rally ROM at $ROM" >&2; exit 1; }
command -v ffmpeg >/dev/null || { echo "package_app: ffmpeg not on PATH (FLAC encoder)" >&2; exit 1; }

OUT=build/app
EX=$OUT/extract
APP="$OUT/Boss Rally.app"
mkdir -p $OUT

# ---- extract, once per set of sources -------------------------------------
KEY="3 $(md5 -q "$BIN" "$CUE" | tr '\n' ' ')"   # 3: the extract's layout
if [ ! -f $EX/.complete ] || [ "$(cat $EX/.complete)" != "$KEY" ]; then
    rm -rf $EX
    mkdir -p $EX/music
    echo "extract: data track <- $BIN"
    $PY tools/extract_disc.py "$BIN" $EX/disc
    echo "extract: CD audio <- $CUE"
    $PY tools/extract_cdaudio.py -q "$CUE" $EX/music/cd
    # the stamp goes last: a run that died partway claims nothing
    echo "$KEY" > $EX/.complete
else
    echo "extract: cached ($EX)"
fi

# ---- the N64 soundtrack, once per ROM, recordings and tools ----------------
N64=$EX/music/n64
NKEY="1 $(md5 -q "$ROM" ports/macos/wasm/extract_modules.py ports/macos/wasm/ost_loops.py \
            ports/macos/wasm/xmloop.c | tr '\n' ' ')"
if [ -n "$OST" ]; then
    ls "$OST"/*.flac >/dev/null 2>&1 || { echo "package_app: no FLACs in $OST" >&2; exit 1; }
    NKEY="$NKEY $(cat "$OST"/*.flac | md5 -q)"
fi
if [ -n "$HQ" ]; then
    [ -d "$HQ" ] || { echo "package_app: no sample directory at $HQ" >&2; exit 1; }
    NKEY="$NKEY hq $(md5 -q ports/macos/wasm/upgrade_samples.py ports/macos/wasm/hq_samples.json | tr '\n' ' ')"
    NKEY="$NKEY $(cat "$HQ"/*.wav 2>/dev/null | md5 -q)"
fi
if [ ! -f $N64/.complete ] || [ "$(cat $N64/.complete)" != "$NKEY" ]; then
    rm -rf $N64
    echo "extract: N64 soundtrack <- $ROM"
    $PY ports/macos/wasm/extract_modules.py "$ROM" $N64
    if [ -n "$HQ" ]; then
        echo "extract: N64 samples upgraded <- $HQ"
        $PY ports/macos/wasm/upgrade_samples.py "$HQ" $N64
    fi
    if [ -n "$OST" ]; then
        BREW=${BR_BREW:-/opt/homebrew/opt}
        python3 -c 'import numpy' 2>/dev/null ||
            { echo "package_app: the recordings need numpy in python3 (or pass --ost '')" >&2; exit 1; }
        clang -O2 -I$BREW/libopenmpt/include ports/macos/wasm/xmloop.c $BREW/libopenmpt/lib/libopenmpt.a \
            $BREW/mpg123/lib/libmpg123.a $BREW/libvorbis/lib/libvorbisfile.a $BREW/libvorbis/lib/libvorbis.a \
            $BREW/libogg/lib/libogg.a -lz -lc++ -o $OUT/xmloop
        echo "extract: N64 recordings <- $OST"
        python3 ports/macos/wasm/ost_loops.py "$OST" $N64 $OUT/xmloop $EX/music/cd
    fi
    echo "$NKEY" > $N64/.complete
else
    echo "extract: N64 soundtrack cached ($N64)"
fi
[ -f $EX/disc/BRGlide.dll ] || { echo "package_app: data track has no BRGlide.dll" >&2; exit 1; }
[ -f $EX/music/cd/cdaudio.manifest.json ] || { echo "package_app: CD audio incomplete" >&2; exit 1; }
[ -f $EX/music/n64/modules.json ] || { echo "package_app: N64 soundtrack incomplete" >&2; exit 1; }

# ---- build -----------------------------------------------------------------
[ $BUILD = 0 ] || sh ports/macos/wasm/build_wasm.sh
for f in build/wasm/brally build/wasm/c/portdata.bin; do
    [ -f $f ] || { echo "package_app: $f missing (run without --no-build)" >&2; exit 1; }
done

# ---- assemble --------------------------------------------------------------
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/music"
cp build/wasm/brally "$APP/Contents/MacOS/brally"
cp build/wasm/c/portdata.bin "$APP/Contents/Resources/portdata.bin"
# -c clones on APFS: the extract and the app share blocks until one changes
cp -Rc $EX/disc "$APP/Contents/Resources/disc" 2>/dev/null || cp -R $EX/disc "$APP/Contents/Resources/disc"
for m in cd n64; do
    cp -Rc $EX/music/$m "$APP/Contents/Resources/music/$m" 2>/dev/null || cp -R $EX/music/$m "$APP/Contents/Resources/music/$m"
done

# the PC / N64 version icons (native/version.m), from the shared port assets,
# rasterised, so macOS releases without SVG in NSImage show them
mkdir -p "$APP/Contents/Resources/assets"
for i in n64 retro_pc; do
    sips -s format png -Z 512 ports/common/assets/$i.svg --out "$APP/Contents/Resources/assets/$i.png" >/dev/null
done

# the Remastered player car (host/host_car.m): the pack remaster_car.py writes;
# without it the Remastered renderer draws the original car
if [ -f ports/common/models/es/pack/car.cfg ]; then
    rm -rf "$APP/Contents/Resources/remaster"
    cp -Rc ports/common/models/es/pack "$APP/Contents/Resources/remaster" 2>/dev/null || cp -R ports/common/models/es/pack "$APP/Contents/Resources/remaster"
fi

# the licences of the libraries linked into the binary (build_wasm.sh MPT_LIBS)
BREW=${BR_BREW:-/opt/homebrew/opt}
mkdir -p "$APP/Contents/Resources/Licenses"
for l in libopenmpt:LICENSE mpg123:COPYING libogg:COPYING libvorbis:COPYING; do
    cp "$BREW/${l%%:*}/${l#*:}" "$APP/Contents/Resources/Licenses/${l%%:*}.txt"
done

ICO=$(find $EX/disc -maxdepth 1 -iname boss.ico | head -1)
ICON_KEY=
if [ -n "$ICO" ]; then
    rm -rf $OUT/BossRally.iconset
    $PY ports/macos/wasm/mkicns.py "$ICO" $OUT/BossRally.iconset
    iconutil -c icns $OUT/BossRally.iconset -o "$APP/Contents/Resources/BossRally.icns"
    ICON_KEY="<key>CFBundleIconFile</key><string>BossRally</string>"
fi

VERSION=$(git log -1 --format=%cd --date=format:%Y.%m.%d 2>/dev/null || echo 1.0)
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Boss Rally</string>
    <key>CFBundleDisplayName</key><string>Boss Rally</string>
    <key>CFBundleIdentifier</key><string>com.strutb.bossrally</string>
    <key>CFBundleExecutable</key><string>brally</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    $ICON_KEY
    <key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>GCSupportsControllerUserInteraction</key><true/>
</dict>
</plist>
EOF

codesign --force --sign - "$APP"
echo "packaged: $APP ($(du -sh "$APP" | cut -f1))"
