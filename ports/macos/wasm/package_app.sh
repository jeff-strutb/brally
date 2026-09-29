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
#   Contents/Resources/music/n64/   the N64 soundtrack, the ROM's six modules
#
# Sources, all required (a partial app must not look like a complete one):
#   --bin  BossRally.BIN (its .cue beside it)  default reference/brally/
#   --rom  Top Gear Rally (USA).z64            default reference/tgrally/
# and ffmpeg on PATH (FLAC encoder for both soundtracks).
#
# Extraction is cached in build/app/extract, keyed on the MD5 of every source,
# so repackaging after a code change does not re-rip ~400 MB of audio.
#
# Usage: ports/macos/wasm/package_app.sh [--bin X] [--rom Y] [--no-build]
set -e
cd "$(dirname "$0")/../../.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3

BIN=reference/brally/BossRally.BIN
ROM="reference/tgrally/Top Gear Rally (USA).z64"
BUILD=1
while [ $# -gt 0 ]; do
    case $1 in
        --bin) BIN=$2; shift 2 ;;
        --rom) ROM=$2; shift 2 ;;
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
KEY=$(md5 -q "$BIN" "$CUE" "$ROM" | tr '\n' ' ')
if [ ! -f $EX/.complete ] || [ "$(cat $EX/.complete)" != "$KEY" ]; then
    rm -rf $EX
    mkdir -p $EX/music
    echo "extract: data track <- $BIN"
    $PY tools/extract_disc.py "$BIN" $EX/disc
    echo "extract: CD audio <- $CUE"
    $PY tools/extract_cdaudio.py -q "$CUE" $EX/music/cd
    echo "extract: N64 soundtrack <- $ROM"
    $PY tools/extract_xm.py -q "$ROM" $EX/music/n64
    # the stamp goes last: a run that died partway claims nothing
    echo "$KEY" > $EX/.complete
else
    echo "extract: cached ($EX)"
fi
[ -f $EX/disc/BRGlide.dll ] || { echo "package_app: data track has no BRGlide.dll" >&2; exit 1; }
[ -f $EX/music/cd/cdaudio.manifest.json ] || { echo "package_app: CD audio incomplete" >&2; exit 1; }
[ -f $EX/music/n64/xm.manifest.json ] || { echo "package_app: N64 soundtrack incomplete" >&2; exit 1; }

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
