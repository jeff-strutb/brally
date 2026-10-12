#!/bin/sh
# builder/build.sh: build Rally Builder for macOS and Windows.
#
#   build/builder/dist/RallyBuilder-VERSION-macOS.zip    Rally Builder.app, universal: builds
#                                                        Top Gear Rally Remastered for the Mac or
#                                                        the iPhone
#   build/builder/dist/RallyBuilder-VERSION-Windows.zip  Rally Builder\RallyBuilder.exe
#                                                        and games\ (the payload), x64
#
# Each carries Top Gear Rally Remastered (ports/remastered) built with none of
# the games' data in it: ports/brally with IMAGE=runtime, ports/tgrally with
# ROMDATA=file. check_payload.py proves that before anything is packaged.
# VERSION is builder/version.py's (the average M2 percentage) unless set.
#
# Needs the toolchains the ports already use: Xcode's clang and the iOS SDK
# with xcodegen (macOS), and mingw-w64 (x86_64-w64-mingw32-*) for Windows.
set -e
cd "$(dirname "$0")/.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3
VERSION=${VERSION:-$($PY builder/version.py)}
export VERSION                     # the game executables' version info too (Windows)
B=build/builder
DIST=$B/dist
WINCC=$PWD/ports/brally/tools/wincc.sh
rm -rf $B/payload-macos $B/payload-windows
mkdir -p $DIST $B/payload-macos/ios $B/payload-windows $B/win
echo "Rally Builder $VERSION"
# the games' data read from the build at start-up, never compiled in
IMAGE=runtime ROMDATA=file
export IMAGE ROMDATA

# ---- the payload: the Mac ----------------------------------------------------------------------
for A in arm64 x86_64; do
    MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" LDCXX="clang++ -arch $A" OUT=$B/remastered-macos-$A \
        HOST=macos RENDER=metal GFLAG=-g0 ports/remastered/link.sh | grep -E "FAIL|linked"
done
lipo -create $B/remastered-macos-arm64/remastered $B/remastered-macos-x86_64/remastered \
    -output $B/payload-macos/remastered
strip -S $B/payload-macos/remastered
IS=$B/AppIcon.iconset
rm -rf $IS && mkdir -p $IS
for s in 16 32 128 256 512; do
    sips -z $s $s ports/remastered/icon.png --out $IS/icon_${s}x${s}.png >/dev/null
    sips -z $((s * 2)) $((s * 2)) ports/remastered/icon.png --out $IS/icon_${s}x${s}@2x.png >/dev/null
done
iconutil -c icns $IS -o $B/payload-macos/AppIcon.icns
rm -rf $IS

# ---- the payload: the iPhone (unsigned; the builder adds the data and signs it) -------------------
ports/remastered/ios/build.sh --payload | grep -E "FAIL|built"
cp -R "build/remastered/ios-payload/derived/Build/Products/Release-iphoneos/Top Gear Rally Remastered.app" \
    $B/payload-macos/ios/app
cp builder/src/mac/ios_sign.sh $B/payload-macos/ios_sign.sh

# ---- the payload: Windows ---------------------------------------------------------------------
# drawn with Vulkan (the GPU, as Metal on the Mac) and, for a PC without a
# Vulkan driver, in software; the builder picks
for R in vulkan soft; do
    S=; [ $R = soft ] && S=-soft
    CC=$WINCC OUT=$B/remastered-windows-$R HOST=windows RENDER=$R GFLAG=-g0 ports/remastered/link.sh | grep -E "FAIL|linked"
    x86_64-w64-mingw32-strip -o $B/payload-windows/remastered$S.exe $B/remastered-windows-$R/remastered.exe
done
cp $B/remastered-windows-vulkan/icon.ico $B/payload-windows/icon.ico

# x86-64 game code must not assume 16-byte alignment of the original's data
$PY builder/check_alignment.py $B/remastered-macos-x86_64/brally/obj $B/remastered-macos-x86_64/tgrally/obj \
    $B/remastered-windows-vulkan/brally/obj $B/remastered-windows-vulkan/tgrally/obj
$PY builder/check_payload.py $B/payload-macos/remastered "$B/payload-macos/ios/app/Top Gear Rally Remastered" \
    $B/payload-windows/*.exe

# ---- macOS: Rally Builder.app -------------------------------------------------------------------
APP="$B/mac/Rally Builder.app"
rm -rf "$B/mac"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/payload"
clang -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 -O2 -Wall -fobjc-arc -DRB_VERSION="\"$VERSION\"" \
    -Ibuilder/src/core builder/src/core/*.c builder/src/mac/main.m -framework Cocoa \
    -o "$APP/Contents/MacOS/RallyBuilder"
sed "s/@VERSION@/$VERSION/g" builder/src/mac/Info.plist > "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
cp -R $B/payload-macos/ "$APP/Contents/Resources/payload/"
codesign --force --sign - "$APP/Contents/Resources/payload/remastered"
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
rm -f $DIST/RallyBuilder-*-macOS.zip
ditto -c -k --keepParent "$APP" $DIST/RallyBuilder-$VERSION-macOS.zip

# ---- Windows: RallyBuilder.exe -----------------------------------------------------------------
MAJOR=${VERSION%%.*}
MINOR=$(echo "${VERSION#*.}" | sed 's/^0*\([0-9]\)/\1/')
cat > $B/win/payload.h <<EOF
#define RB_VER_MAJOR $MAJOR
#define RB_VER_MINOR $MINOR
#define RB_VERSION_STR "$VERSION"
EOF
x86_64-w64-mingw32-windres -I$B/win -Ibuilder/src/win builder/src/win/builder.rc -O coff -o $B/win/builder.res.o
rm -rf $DIST/RallyBuilder-*-Windows.* "$B/win/Rally Builder"
mkdir -p "$B/win/Rally Builder/games"
OBJS=
for s in builder/src/core/*.c builder/src/win/main.c; do
    o=$B/win/$(basename $s .c).o
    $WINCC -O2 -Wall -DUNICODE -D_UNICODE -DRB_VERSION="\"$VERSION\"" -Ibuilder/src/core -c $s -o $o
    OBJS="$OBJS $o"
done
x86_64-w64-mingw32-gcc -municode -mwindows -static -s -o "$B/win/Rally Builder/RallyBuilder.exe" \
    $OBJS $B/win/builder.res.o -lcomctl32 -lole32 -lshell32 -luuid
# the game and its icon beside it as plain files: the builder copies them, it
# carries no executable inside itself
cp $B/payload-windows/*.exe $B/payload-windows/icon.ico "$B/win/Rally Builder/games/"
(cd $B/win && zip -qrX "$PWD/../dist/RallyBuilder-$VERSION-Windows.zip" "Rally Builder")
ls -la $DIST
