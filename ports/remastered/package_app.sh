#!/bin/sh
# Build "Top Gear Rally Remastered.app": link.sh with the macOS host and the
# Metal renderer, Boss Rally's data track as its CD root and the icon.
# Top Gear Rally's data (from your ROM) and Boss Rally's BRGlide.dll data are
# built into the executable, so the app needs neither ROM nor disc once built.
#
#   Contents/MacOS/remastered            the two games and the host
#   Contents/Resources/disc              Boss Rally's data track, less the installer and the video
#   Contents/Resources/skies             the lighting's skymaps: skies.txt and its 8K HDRIs (ports/common/skies)
#   Contents/Resources/surfaces          the Remastered textures' photographed sets, 2048 (ports/common/surfaces)
#   Contents/Resources/music/cd          Boss Rally's CD audio, the PC's music (music.sh)
#   Contents/Resources/AppIcon.icns      from icon.png
#
# Saves go to ~/Library/Application Support/Top Gear Rally Remastered (Boss
# Rally's in its Boss Rally folder).
#
# Usage: ports/remastered/package_app.sh [--rom FILE] [--bin FILE] [--no-build]
#   --rom FILE  Top Gear Rally's ROM (default: $TGR_ROM, else reference/tgrally/Top Gear Rally (USA).z64)
#   --bin FILE  Boss Rally's disc image, when its data track or its CD audio is not
#               already extracted (default reference/brally/BossRally.BIN; the .cue beside it)
set -e
cd "$(dirname "$0")/../.."
BUILD=1
BIN=reference/brally/BossRally.BIN
while [ $# -gt 0 ]; do
    case $1 in
        --rom) TGR_ROM=$2; export TGR_ROM; shift 2 ;;
        --bin) BIN=$2; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        *) echo "package_app: unknown argument $1" >&2; exit 2 ;;
    esac
done
PY=.venv/bin/python
[ -x $PY ] || PY=python3

OUT=build/remastered/macos-metal
APP="$OUT/Top Gear Rally Remastered.app"
if [ $BUILD = 1 ]; then
    OUT=$OUT HOST=macos RENDER=metal GFLAG=-g0 ports/remastered/link.sh
fi
[ -x $OUT/remastered ] || { echo "package_app: no build at $OUT/remastered" >&2; exit 1; }

# ---- Boss Rally's data track: the tree's, else the disc image's ---------------------
DISC=reference/brally/data/disc
if [ ! -f $DISC/BRGlide.dll ]; then
    DISC=build/brally/macos-metal/extract/disc
    if [ ! -f $DISC/BRGlide.dll ]; then
        [ -f "$BIN" ] || { echo "package_app: no Boss Rally disc image at $BIN" >&2; exit 1; }
        mkdir -p $DISC
        $PY tools/brally/extract_disc.py "$BIN" $DISC
    fi
fi

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp $OUT/remastered "$APP/Contents/MacOS/remastered"
rsync -a --exclude 'brally.avi' --exclude '*.cab' --exclude '*.exe' --exclude '*.EXE' \
    --exclude '_INST32I.EX_' --exclude '_SETUP.DLL' --exclude 'dsetup*.dll' --exclude 'Msvcrt.dll' \
    --exclude 'BRD3D.dll' --exclude 'SETUP.INI' --exclude 'setup.*' --exclude 'Setup.BMP' \
    --exclude 'Autorun.inf' --exclude '*.url' --exclude 'layout.bin' \
    $DISC/ "$APP/Contents/Resources/disc/"

# ---- the PC's music: Boss Rally's CD audio (host_music_dir: Resources/music/cd) ------
BIN=$BIN ports/remastered/music.sh "$APP/Contents/Resources/music/cd"

# ---- the skymaps (the Remastered lighting's skies, ports/common/skies) ---------------
if [ -f ports/common/skies/skies.txt ]; then   # ports/common/skies/fetch.sh brings them
    mkdir -p "$APP/Contents/Resources/skies/ph8k"
    cp ports/common/skies/skies.txt "$APP/Contents/Resources/skies/"
    grep -v '^#' ports/common/skies/skies.txt | awk 'NF >= 3 { print $3 }' | sort -u | while read id; do
        if [ -f "ports/common/skies/ph8k/${id}_8k.hdr" ]; then
            cp "ports/common/skies/ph8k/${id}_8k.hdr" "$APP/Contents/Resources/skies/ph8k/"
        else
            echo "package_app: sky $id missing (ports/common/skies/fetch.sh)" >&2
        fi
    done
else
    echo "package_app: no ports/common/skies/skies.txt: the Remastered lighting falls back to a grey day" >&2
fi

# ---- the Remastered textures' sets (ports/common/surfaces: fetch.py, then bake.py) ---
mkdir -p "$APP/Contents/Resources/surfaces/baked"
cp ports/common/surfaces/surfaces.txt "$APP/Contents/Resources/surfaces/"
grep -v '^#' ports/common/surfaces/surfaces.txt | awk 'NF >= 2 { print $1 }' | while read id; do
    for m in a b; do
        if [ -f "ports/common/surfaces/baked/${id}_$m.raw" ]; then
            cp "ports/common/surfaces/baked/${id}_$m.raw" "$APP/Contents/Resources/surfaces/baked/"
        else
            echo "package_app: surface set $id missing (ports/common/surfaces/fetch.py, bake.py): Textures off" >&2
        fi
    done
done

# ---- the icon ------------------------------------------------------------------------
IS=$OUT/AppIcon.iconset
rm -rf $IS && mkdir -p $IS
for s in 16 32 128 256 512; do
    sips -z $s $s ports/remastered/icon.png --out $IS/icon_${s}x${s}.png >/dev/null
    sips -z $((s * 2)) $((s * 2)) ports/remastered/icon.png --out $IS/icon_${s}x${s}@2x.png >/dev/null
done
iconutil -c icns $IS -o "$APP/Contents/Resources/AppIcon.icns"
rm -rf $IS

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Top Gear Rally Remastered</string>
    <key>CFBundleDisplayName</key><string>Top Gear Rally Remastered</string>
    <key>CFBundleIdentifier</key><string>com.strutb.tgrally-remastered</string>
    <key>CFBundleExecutable</key><string>remastered</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundleVersion</key><string>$(git rev-parse --short HEAD)</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>GCSupportsControllerUserInteraction</key><true/>
</dict>
</plist>
EOF
codesign --force --sign - "$APP" >/dev/null 2>&1 || true
echo "packaged $APP"
