#!/bin/sh
# Build "Top Gear Rally.app": the native game (link.sh, macOS host, Metal
# renderer) with the builder's own cartridge ROM in its Resources, so the
# app runs with no ROM or source tree beside it.
#
#   Contents/MacOS/tgrally               the game
#   Contents/Resources/TopGearRally.z64  the ROM (the builder's own copy)
#
# Saves (the Controller Pak) go to ~/Library/Application Support/Top Gear Rally.
# Without a ROM in the app the game looks there for one (.z64, .v64, .n64).
#
# Usage: ports/tgrally/package_app.sh [--rom FILE] [--no-build]
set -e
cd "$(dirname "$0")/../.."
ROM="reference/tgrally/Top Gear Rally (USA).z64"
BUILD=1
while [ $# -gt 0 ]; do
    case $1 in
        --rom) ROM=$2; shift 2 ;;
        --no-build) BUILD=0; shift ;;
        *) echo "package_app: unknown argument $1" >&2; exit 2 ;;
    esac
done
[ -f "$ROM" ] || { echo "package_app: no ROM at $ROM (--rom FILE)" >&2; exit 1; }

OUT=build/tgapp
APP="$OUT/Top Gear Rally.app"
if [ $BUILD = 1 ]; then
    OUT=$OUT/build HOST=macos RENDER=metal GFLAG=-g0 ports/tgrally/link.sh
fi
[ -x $OUT/build/tgrally ] || { echo "package_app: no build at $OUT/build/tgrally" >&2; exit 1; }

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp $OUT/build/tgrally "$APP/Contents/MacOS/tgrally"
cp "$ROM" "$APP/Contents/Resources/TopGearRally.z64"
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Top Gear Rally</string>
    <key>CFBundleDisplayName</key><string>Top Gear Rally</string>
    <key>CFBundleIdentifier</key><string>com.strutb.tgrally</string>
    <key>CFBundleExecutable</key><string>tgrally</string>
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
