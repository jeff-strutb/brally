#!/bin/sh
# DuckStation for the PlayStation port's checks: the official macOS release,
# unpacked into build/tools/ps1 and run in portable mode (its settings, BIOS
# and logs inside the copy, nothing in the user's Library).  The BIOS is the
# builder's own: reference/ps1/bios/*.BIN.
set -e
cd "$(dirname "$0")/../../.."
D=build/tools/ps1
APP=$D/DuckStation.app
if [ ! -x $APP/Contents/MacOS/DuckStation ]; then
  rm -rf $D/dl && mkdir -p $D/dl
  curl -fsSL -o $D/dl/ds.zip https://github.com/stenzek/duckstation/releases/download/latest/duckstation-mac-release.zip
  (cd $D/dl && unzip -q ds.zip)
  rm -rf $APP && mv $D/dl/DuckStation.app $APP && rm -rf $D/dl
fi
touch $APP/Contents/MacOS/portable.txt
mkdir -p $APP/Contents/MacOS/bios
cp reference/ps1/bios/*.BIN $APP/Contents/MacOS/bios/ 2>/dev/null || echo "duckstation: no BIOS in reference/ps1/bios" >&2
$APP/Contents/MacOS/DuckStation -version 2>&1 | head -1
