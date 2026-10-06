#!/bin/sh
# builder/build.sh: build Rally Builder for macOS and Windows.
#
#   build/builder/dist/RallyBuilder-VERSION-macOS.zip    Rally Builder.app, universal
#   build/builder/dist/RallyBuilder-VERSION-Windows.zip  Rally Builder\RallyBuilder.exe
#                                                        and games\ (the payload), x64
#
# Each carries the two games' executables (the payload) built with none of the
# games' data in them: ports/brally with IMAGE=runtime, ports/tgrally with
# ROMDATA=file. check_payload.py proves that before anything is packaged.
# VERSION is builder/version.py's (the lower M2 percentage) unless set.
#
# Needs the toolchains the ports already use: Xcode's clang (macOS), and
# mingw-w64 (x86_64-w64-mingw32-*) for Windows.
set -e
cd "$(dirname "$0")/.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3
VERSION=${VERSION:-$($PY builder/version.py)}
export VERSION                     # the game executables' version info too (Windows)
B=build/builder
DIST=$B/dist
WINCC=$PWD/ports/brally/tools/wincc.sh
mkdir -p $DIST $B/payload-macos $B/payload-windows $B/win
echo "Rally Builder $VERSION"

# ---- the payload ---------------------------------------------------------------------------
for A in arm64 x86_64; do
    MACOSX_DEPLOYMENT_TARGET=11.0 CC="clang -arch $A" LDCXX="clang++ -arch $A" OUT=$B/brally-macos-$A \
        HOST=macos RENDER=metal IMAGE=runtime GFLAG=-g0 ports/brally/link64.sh | tail -1
    MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" LDFLAGS_TGR="-arch $A" OUT=$B/tgrally-macos-$A \
        HOST=macos RENDER=metal ROMDATA=file GFLAG=-g0 ports/tgrally/link.sh | grep -E "FAIL|linked"
done
lipo -create $B/brally-macos-arm64/brally64 $B/brally-macos-x86_64/brally64 -output $B/payload-macos/brally64
lipo -create $B/tgrally-macos-arm64/tgrally $B/tgrally-macos-x86_64/tgrally -output $B/payload-macos/tgrally
strip -S $B/payload-macos/brally64 $B/payload-macos/tgrally

CC=$WINCC OUT=$B/brally-windows HOST=windows RENDER=soft IMAGE=runtime GFLAG=-g0 ports/brally/link64.sh | tail -1
CC=$WINCC OUT=$B/tgrally-windows HOST=windows RENDER=soft ROMDATA=file GFLAG=-g0 ports/tgrally/link.sh | grep -E "FAIL|linked"
x86_64-w64-mingw32-strip -o $B/payload-windows/brally64.exe $B/brally-windows/brally64.exe
x86_64-w64-mingw32-strip -o $B/payload-windows/tgrally.exe $B/tgrally-windows/tgrally.exe

# x86-64 game code must not assume 16-byte alignment of the original's data
$PY builder/check_alignment.py $B/brally-macos-x86_64/obj $B/tgrally-macos-x86_64/obj \
    $B/brally-windows/obj $B/tgrally-windows/obj
$PY builder/check_payload.py $B/payload-macos/brally64 $B/payload-macos/tgrally \
    $B/payload-windows/brally64.exe $B/payload-windows/tgrally.exe

# ---- macOS: Rally Builder.app -------------------------------------------------------------------
APP="$B/mac/Rally Builder.app"
rm -rf "$B/mac"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/payload"
clang -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 -O2 -Wall -fobjc-arc -DRB_VERSION="\"$VERSION\"" \
    -Ibuilder/src/core builder/src/core/*.c builder/src/mac/main.m -framework Cocoa \
    -o "$APP/Contents/MacOS/RallyBuilder"
sed "s/@VERSION@/$VERSION/g" builder/src/mac/Info.plist > "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
cp $B/payload-macos/brally64 $B/payload-macos/tgrally "$APP/Contents/Resources/payload/"
codesign --force --sign - "$APP/Contents/Resources/payload/brally64" "$APP/Contents/Resources/payload/tgrally"
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
# the games beside it as plain files: the builder copies them, it carries no
# executable inside itself
cp $B/payload-windows/brally64.exe $B/payload-windows/tgrally.exe "$B/win/Rally Builder/games/"
(cd $B/win && zip -qrX "$PWD/../dist/RallyBuilder-$VERSION-Windows.zip" "Rally Builder")
ls -la $DIST
