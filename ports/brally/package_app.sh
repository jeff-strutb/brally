#!/bin/sh
# Build "Boss Rally 64.app": the native 64-bit game (link64.sh, macOS host,
# Metal renderer) plus what it reads at run time, extracted once from the
# builder's own disc, so the app runs with no disc image or source tree
# beside it.
#
#   Contents/MacOS/brally64        the game; BRGlide.dll's data is compiled in
#   Contents/Resources/disc/       the data track (the CD root the game opens
#                                  tracks/, cars/, sfx/ ... under)
#   Contents/Resources/music/cd/   the soundtrack, CD audio tracks 2..13
#   Contents/Resources/BossRally.icns  the disc's icon
#
# Saves and settings go to ~/Library/Application Support/Boss Rally 64.
#
# Source: --bin BossRally.BIN with its .cue beside it (default
# reference/brally/). ffmpeg on PATH (FLAC encoder for the CD audio).
# The extract is cached in build/app64/extract, keyed on the MD5 of the
# image and cue; an extract of the same disc already made for the 32-bit
# app (build/app/extract) is cloned instead of ripped again.
#
# Usage: ports/brally/package_app.sh [--bin X] [--no-build]
set -e
cd "$(dirname "$0")/../.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3

BIN=reference/brally/BossRally.BIN
BUILD=1
while [ $# -gt 0 ]; do
    case $1 in
        --bin) BIN=$2; shift 2 ;;
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

OUT=build/app64
EX=$OUT/extract
APP="$OUT/Boss Rally 64.app"
mkdir -p $OUT

# ---- extract, once per disc --------------------------------------------------
KEY="3 $(md5 -q "$BIN" "$CUE" | tr '\n' ' ')"   # 3: the extract's layout
if [ ! -f $EX/.complete ] || [ "$(cat $EX/.complete)" != "$KEY" ]; then
    rm -rf $EX
    if [ -f build/app/extract/.complete ] && [ "$(cat build/app/extract/.complete)" = "$KEY" ]; then
        echo "extract: cloned from build/app/extract"
        mkdir -p $EX/music
        cp -Rc build/app/extract/disc $EX/disc 2>/dev/null || cp -R build/app/extract/disc $EX/disc
        cp -Rc build/app/extract/music/cd $EX/music/cd 2>/dev/null || cp -R build/app/extract/music/cd $EX/music/cd
    else
        command -v ffmpeg >/dev/null || { echo "package_app: ffmpeg not on PATH (FLAC encoder)" >&2; exit 1; }
        mkdir -p $EX/music
        echo "extract: data track <- $BIN"
        $PY tools/extract_disc.py "$BIN" $EX/disc
        echo "extract: CD audio <- $CUE"
        $PY tools/extract_cdaudio.py -q "$CUE" $EX/music/cd
    fi
    # the stamp goes last: a run that died partway claims nothing
    echo "$KEY" > $EX/.complete
else
    echo "extract: cached ($EX)"
fi
[ -f $EX/disc/BRGlide.dll ] || { echo "package_app: data track has no BRGlide.dll" >&2; exit 1; }
ls $EX/music/cd/track02.* >/dev/null 2>&1 || { echo "package_app: CD audio incomplete" >&2; exit 1; }

# ---- build -------------------------------------------------------------------
BINOUT=build/portable_app
if [ $BUILD = 1 ]; then
    OUT=$BINOUT HOST=macos RENDER=metal GFLAG=-g0 DLL=$EX/disc/BRGlide.dll ports/brally/link64.sh
fi
[ -f $BINOUT/brally64 ] || { echo "package_app: $BINOUT/brally64 missing (run without --no-build)" >&2; exit 1; }

# ---- assemble ----------------------------------------------------------------
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/music"
cp $BINOUT/brally64 "$APP/Contents/MacOS/brally64"
strip -S "$APP/Contents/MacOS/brally64" 2>/dev/null || true
# -c clones on APFS: the extract and the app share blocks until one changes
cp -Rc $EX/disc "$APP/Contents/Resources/disc" 2>/dev/null || cp -R $EX/disc "$APP/Contents/Resources/disc"
cp -Rc $EX/music/cd "$APP/Contents/Resources/music/cd" 2>/dev/null || cp -R $EX/music/cd "$APP/Contents/Resources/music/cd"

ICO=$(find $EX/disc -maxdepth 1 -iname boss.ico | head -1)
ICON_KEY=
if [ -n "$ICO" ]; then
    rm -rf $OUT/BossRally.iconset
    $PY ports/brally/tools/mkicns.py "$ICO" $OUT/BossRally.iconset
    iconutil -c icns $OUT/BossRally.iconset -o "$APP/Contents/Resources/BossRally.icns"
    ICON_KEY="<key>CFBundleIconFile</key><string>BossRally</string>"
fi

VERSION=$(git log -1 --format=%cd --date=format:%Y.%m.%d 2>/dev/null || echo 1.0)
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Boss Rally 64</string>
    <key>CFBundleDisplayName</key><string>Boss Rally</string>
    <key>CFBundleIdentifier</key><string>com.strutb.bossrally64</string>
    <key>CFBundleExecutable</key><string>brally64</string>
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
