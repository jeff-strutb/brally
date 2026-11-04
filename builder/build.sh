#!/bin/sh
# builder/build.sh: build Rally Builder for macOS, Windows and Linux.
#
#   builder/build.sh [macos] [windows] [linux]   (none named: all three)
#
#   build/builder/dist/RallyBuilder-VERSION-macOS.zip    Rally Builder.app, universal: builds
#                                                        Boss Rally or Top Gear Rally for the Mac,
#                                                        or Top Gear Rally Remastered for the Mac
#                                                        or the iPhone
#   build/builder/dist/RallyBuilder-VERSION-Windows.zip  Rally Builder\RallyBuilder.exe
#                                                        and games\ (the payload), x64
#   build/builder/dist/RallyBuilder-VERSION-Linux-x86_64.AppImage
#                                                        the GTK builder with the payload
#                                                        in it (usr/share/rally-builder/games)
#
# Each carries the games (ports/brally, ports/tgrally for each of its three
# cartridges, ports/remastered) built with none of the games' data in them:
# ports/brally with IMAGE=runtime, ports/tgrally with ROMDATA=file.
# check_payload.py proves that before anything is packaged.
# VERSION is builder/version.py's (the average M2 percentage over Top Gear
# Rally's three cartridges and Boss Rally, or its RELEASE while that is ahead)
# unless set.
#
# Needs the toolchains the ports already use: Xcode's clang and the iOS SDK
# with xcodegen (macOS), mingw-w64 (x86_64-w64-mingw32-*) and SDL 3 for the
# controllers (tools/toolchains/windows/setup.sh) for Windows, and for Linux the
# Steam Runtime sysroot, SDL 3 and the AppImage runtime
# (tools/toolchains/linux/setup.sh), LLVM's ld.lld and mksquashfs (Homebrew's
# lld and squashfs).
set -e
DO_MAC=1 DO_WIN=1 DO_LINUX=1
if [ $# -gt 0 ]; then
    DO_MAC= DO_WIN= DO_LINUX=
    for a in "$@"; do
        case $a in macos) DO_MAC=1;; windows) DO_WIN=1;; linux) DO_LINUX=1;;
                   *) echo "build.sh: macos, windows or linux (got $a)" >&2; exit 2;; esac
    done
fi
cd "$(dirname "$0")/.."
PY=.venv/bin/python
[ -x $PY ] || PY=python3
VERSION=${VERSION:-$($PY builder/version.py)}
export VERSION                     # the game executables' version info too (Windows)
B=build/builder
DIST=$B/dist
WINCC=$PWD/ports/brally/tools/wincc.sh
mkdir -p $DIST
echo "Rally Builder $VERSION"
# the games' data read from the build at start-up, never compiled in
IMAGE=runtime ROMDATA=file
export IMAGE ROMDATA

# ---- the payload: the Mac ----------------------------------------------------------------------
if [ -n "$DO_MAC" ]; then
rm -rf $B/payload-macos
mkdir -p $B/payload-macos/ios
for A in arm64 x86_64; do
    MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" LDCXX="clang++ -arch $A" OUT=$B/brally-macos-$A \
        HOST=macos RENDER=metal GFLAG=-g0 ports/brally/link64.sh | grep -E "FAIL|linked"
    for G in usa eur jpn; do        # Top Gear Rally: each cartridge's game (tgrally, tgrally-eur, tgrally-jpn)
        X=; [ $G != usa ] && X=-$G
        TGR_REGION=$G MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" LDFLAGS_TGR="-arch $A" \
            OUT=$B/tgrally$X-macos-$A HOST=macos RENDER=metal GFLAG=-g0 ports/tgrally/link.sh | grep -E "FAIL|linked"
    done
    MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" LDCXX="clang++ -arch $A" OUT=$B/remastered-macos-$A \
        HOST=macos RENDER=metal GFLAG=-g0 ports/remastered/link.sh | grep -E "FAIL|linked"
done
lipo -create $B/brally-macos-arm64/brally64 $B/brally-macos-x86_64/brally64 -output $B/payload-macos/brally64
for X in "" -eur -jpn; do
    lipo -create $B/tgrally$X-macos-arm64/tgrally $B/tgrally$X-macos-x86_64/tgrally -output $B/payload-macos/tgrally$X
done
lipo -create $B/remastered-macos-arm64/remastered $B/remastered-macos-x86_64/remastered \
    -output $B/payload-macos/remastered
strip -S $B/payload-macos/brally64 $B/payload-macos/tgrally $B/payload-macos/tgrally-eur $B/payload-macos/tgrally-jpn \
    $B/payload-macos/remastered
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
fi

# ---- the payload: Windows ---------------------------------------------------------------------
# drawn with Vulkan (the GPU, as Metal on the Mac) and, for a PC without a
# Vulkan driver, with OpenGL 3.3; the builder picks
if [ -n "$DO_WIN" ]; then
tools/toolchains/windows/setup.sh
rm -rf $B/payload-windows
mkdir -p $B/payload-windows $B/win
for R in vulkan gl; do
    S=; [ $R = gl ] && S=-gl
    CC=$WINCC OUT=$B/brally-windows-$R HOST=windows RENDER=$R GFLAG=-g0 ports/brally/link64.sh | grep -E "FAIL|linked"
    for G in usa eur jpn; do
        X=; [ $G != usa ] && X=-$G
        TGR_REGION=$G CC=$WINCC OUT=$B/tgrally$X-windows-$R HOST=windows RENDER=$R GFLAG=-g0 ports/tgrally/link.sh \
            | grep -E "FAIL|linked"
        x86_64-w64-mingw32-strip -o $B/payload-windows/tgrally$X$S.exe $B/tgrally$X-windows-$R/tgrally.exe
    done
    CC=$WINCC OUT=$B/remastered-windows-$R HOST=windows RENDER=$R GFLAG=-g0 ports/remastered/link.sh | grep -E "FAIL|linked"
    x86_64-w64-mingw32-strip -o $B/payload-windows/brally64$S.exe $B/brally-windows-$R/brally64.exe
    x86_64-w64-mingw32-strip -o $B/payload-windows/remastered$S.exe $B/remastered-windows-$R/remastered.exe
done
cp $B/remastered-windows-vulkan/icon.ico $B/payload-windows/icon.ico
fi

# ---- the payload: Linux -----------------------------------------------------------------------
# the same two renderers (the Steam Runtime's glibc 2.31 and SDL 3 in each:
# any x86-64 Linux from 2020 on, the Steam Deck among them); the builder picks
LCC=$PWD/ports/brally/tools/linuxcc.sh
LSTRIP=x86_64-w64-mingw32-strip       # (mingw-w64's binutils read ELF too)
if [ -n "$DO_LINUX" ]; then
tools/toolchains/linux/setup.sh
rm -rf $B/payload-linux
mkdir -p $B/payload-linux
for R in vulkan gl; do
    S=; [ $R = gl ] && S=-gl
    CC=$LCC OUT=$B/brally-linux-$R HOST=linux RENDER=$R GFLAG=-g0 ports/brally/link64.sh 2>&1 | grep -E "FAIL|linked"
    for G in usa eur jpn; do
        X=; [ $G != usa ] && X=-$G
        TGR_REGION=$G CC=$LCC OUT=$B/tgrally$X-linux-$R HOST=linux RENDER=$R GFLAG=-g0 ports/tgrally/link.sh 2>&1 \
            | grep -E "FAIL|linked"
        $LSTRIP -o $B/payload-linux/tgrally$X$S $B/tgrally$X-linux-$R/tgrally
    done
    CC=$LCC OUT=$B/remastered-linux-$R HOST=linux RENDER=$R GFLAG=-g0 ports/remastered/link.sh 2>&1 | grep -E "FAIL|linked"
    $LSTRIP -o $B/payload-linux/brally64$S $B/brally-linux-$R/brally64
    $LSTRIP -o $B/payload-linux/remastered$S $B/remastered-linux-$R/remastered
done
chmod 755 $B/payload-linux/*
cp ports/remastered/icon.png $B/payload-linux/icon.png
fi

# x86-64 game code must not assume 16-byte alignment of the original's data
OBJS_X64=
[ -n "$DO_MAC" ] && OBJS_X64="$OBJS_X64 $B/brally-macos-x86_64/obj $B/tgrally-macos-x86_64/obj \
    $B/tgrally-eur-macos-x86_64/obj $B/tgrally-jpn-macos-x86_64/obj \
    $B/remastered-macos-x86_64/brally/obj $B/remastered-macos-x86_64/tgrally/obj"
[ -n "$DO_WIN" ] && OBJS_X64="$OBJS_X64 $B/brally-windows-vulkan/obj $B/tgrally-windows-vulkan/obj \
    $B/tgrally-eur-windows-vulkan/obj $B/tgrally-jpn-windows-vulkan/obj \
    $B/remastered-windows-vulkan/brally/obj $B/remastered-windows-vulkan/tgrally/obj"
[ -n "$DO_LINUX" ] && OBJS_X64="$OBJS_X64 $B/brally-linux-vulkan/obj $B/tgrally-linux-vulkan/obj \
    $B/tgrally-eur-linux-vulkan/obj $B/tgrally-jpn-linux-vulkan/obj \
    $B/remastered-linux-vulkan/brally/obj $B/remastered-linux-vulkan/tgrally/obj"
$PY builder/check_alignment.py $OBJS_X64
[ -n "$DO_MAC" ] && $PY builder/check_payload.py $B/payload-macos/brally64 $B/payload-macos/tgrally \
    $B/payload-macos/tgrally-eur $B/payload-macos/tgrally-jpn $B/payload-macos/remastered \
    "$B/payload-macos/ios/app/Top Gear Rally Remastered"
[ -n "$DO_WIN" ] && $PY builder/check_payload.py $B/payload-windows/*.exe
[ -n "$DO_LINUX" ] && $PY builder/check_payload.py $(ls $B/payload-linux/* | grep -v '\.png$')

# ---- macOS: Rally Builder.app -------------------------------------------------------------------
if [ -n "$DO_MAC" ]; then
APP="$B/mac/Rally Builder.app"
rm -rf "$B/mac"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/payload"
clang -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 -O2 -Wall -fobjc-arc -DRB_VERSION="\"$VERSION\"" \
    -Ibuilder/src/core builder/src/core/*.c builder/src/mac/main.m -framework Cocoa \
    -o "$APP/Contents/MacOS/RallyBuilder"
sed "s/@VERSION@/$VERSION/g" builder/src/mac/Info.plist > "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
cp -R $B/payload-macos/ "$APP/Contents/Resources/payload/"
codesign --force --sign - "$APP/Contents/Resources/payload/brally64" "$APP/Contents/Resources/payload/tgrally" \
    "$APP/Contents/Resources/payload/remastered"
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
rm -f $DIST/RallyBuilder-*-macOS.zip
ditto -c -k --keepParent "$APP" $DIST/RallyBuilder-$VERSION-macOS.zip
fi

# ---- Windows: RallyBuilder.exe -----------------------------------------------------------------
if [ -n "$DO_WIN" ]; then
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
fi

# ---- Linux: RallyBuilder-VERSION-Linux-x86_64.AppImage --------------------------------------------
# the AppImage runtime (tools/toolchains/linux/appimage) with a squashfs of the
# AppDir after it: usr/bin/RallyBuilder (GTK 3, the system's), the payload in
# usr/share/rally-builder/games, its launcher and icon at the top
if [ -n "$DO_LINUX" ]; then
AD=$B/linux/RallyBuilder.AppDir
rm -rf $B/linux
mkdir -p $AD/usr/bin $AD/usr/share/rally-builder/games
SR=$PWD/tools/toolchains/linux/sysroot
GTK=$(PKG_CONFIG_SYSROOT_DIR=$SR PKG_CONFIG_LIBDIR=$SR/usr/lib/x86_64-linux-gnu/pkgconfig:$SR/usr/share/pkgconfig \
      pkg-config --cflags --libs gtk+-3.0)
$LCC -O2 -Wall -DRB_VERSION="\"$VERSION\"" -Ibuilder/src/core builder/src/core/*.c builder/src/linux/main.c \
    -Wl,--as-needed $GTK -ldl -o $AD/usr/bin/RallyBuilder 2>&1 | grep -v "unused-command-line" || true
[ -x $AD/usr/bin/RallyBuilder ] || { echo "build.sh: the Linux builder did not link" >&2; exit 1; }
$LSTRIP $AD/usr/bin/RallyBuilder
cp -p $B/payload-linux/* $AD/usr/share/rally-builder/games/
ln -s usr/bin/RallyBuilder $AD/AppRun
cp ports/remastered/icon.png $AD/rally-builder.png
ln -s rally-builder.png $AD/.DirIcon
cat > $AD/rally-builder.desktop <<DESKTOP
[Desktop Entry]
Type=Application
Name=Rally Builder
Comment=Builds Boss Rally, Top Gear Rally or Top Gear Rally Remastered from your own copies
Exec=RallyBuilder
Icon=rally-builder
Terminal=false
Categories=Game;
X-AppImage-Version=$VERSION
DESKTOP
mksquashfs $AD $B/linux/RallyBuilder.squashfs -root-owned -noappend -no-xattrs -comp zstd -quiet -no-progress
rm -f $DIST/RallyBuilder-*-Linux-x86_64.AppImage
cat tools/toolchains/linux/appimage/runtime-x86_64 $B/linux/RallyBuilder.squashfs \
    > $DIST/RallyBuilder-$VERSION-Linux-x86_64.AppImage
chmod 755 $DIST/RallyBuilder-$VERSION-Linux-x86_64.AppImage
fi
ls -la $DIST
