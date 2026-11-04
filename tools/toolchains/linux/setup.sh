#!/bin/sh
# tools/toolchains/linux/setup.sh: the Linux (x86-64) build environment the
# ports' Linux builds use (ports/brally/tools/linuxcc.sh), staged here and
# nowhere else on the machine but for LLVM's linker (brew install lld).
#
#   sysroot/   Valve's Steam Runtime 3 "sniper" SDK (Debian 11, glibc 2.31):
#              the runtime Steam Deck games build against, with the GTK 3,
#              Wayland, X11, PipeWire and Vulkan headers. Its absolute
#              symbolic links are made relative so they resolve inside it.
#   sdl3/      SDL 3, static and position-independent, every system library
#              (X11, Wayland, ALSA, PulseAudio, PipeWire, udev, D-Bus) loaded
#              at run time, so a game needs none of them to start.
#
#   appimage/runtime-x86_64   the AppImage runtime (type2-runtime, pinned): the
#              Linux Rally Builder is it with a squashfs of the builder after it
#              (builder/build.sh; mksquashfs: brew install squashfs)
#
# Downloads (once, kept in dl/): the SDK, 1.23 GB, SDL's source, 16 MB, and the
# AppImage runtime, 1 MB.
set -e
cd "$(dirname "$0")"
L=$(pwd)
R=$(cd ../../.. && pwd)
SNIPER=3.0.20261021.266025
SNIPER_URL="https://repo.steampowered.com/steamrt3/images/$SNIPER/com.valvesoftware.SteamRuntime.Sdk-amd64%2Ci386-sniper-sysroot.tar.gz"
SDL=3.4.20
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64"
RUNTIME_SHA256="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz"
mkdir -p dl

if [ ! -f sysroot/usr/include/stdio.h ]; then
    [ -f dl/sniper-sdk.tar.gz ] || curl -L --fail -o dl/sniper-sdk.tar.gz "$SNIPER_URL"
    mkdir -p sysroot
    # (FLTK's FL/ and Fl/ collide on a case-insensitive disk; nothing here uses them)
    tar xzf dl/sniper-sdk.tar.gz -C sysroot --exclude './dev/*' --exclude './proc/*' 2>/dev/null || true
    (cd sysroot && find . -type l -lname '/*' | while read -r l; do
        t=$(readlink "$l")
        ln -sfn "$(python3 -c 'import os,sys; print(os.path.relpath("."+sys.argv[1], sys.argv[2]))' "$t" "$(dirname "$l")")" "$l"
    done)
    echo "[ok] sysroot (Steam Runtime sniper $SNIPER)"
fi

if [ ! -f sdl3/lib/libSDL3.a ]; then
    [ -f dl/sdl3.tar.gz ] || curl -L --fail -o dl/sdl3.tar.gz "$SDL_URL"
    rm -rf sdl3-src sdl3-build && mkdir -p sdl3-src
    tar xzf dl/sdl3.tar.gz -C sdl3-src --strip-components 1
    cat > sdl3-toolchain.cmake <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSROOT $L/sysroot)
set(CMAKE_C_COMPILER $R/ports/brally/tools/linuxcc.sh)
set(CMAKE_CXX_COMPILER $R/ports/brally/tools/linuxcc.sh)
# (the Mac's own ar indexes Mach-O only; mingw-w64's binutils read ELF too)
set(CMAKE_AR $(command -v x86_64-w64-mingw32-ar))
set(CMAKE_RANLIB $(command -v x86_64-w64-mingw32-ranlib))
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_SYSROOT_DIR} $L/sysroot)
set(ENV{PKG_CONFIG_LIBDIR} $L/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$L/sysroot/usr/share/pkgconfig)
EOF
    cmake -S sdl3-src -B sdl3-build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$L/sdl3-toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$L/sdl3 -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
        -DSDL_DEPS_SHARED=ON -DSDL_WAYLAND_LIBDECOR=ON -DSDL_X11=ON -DSDL_WAYLAND=ON -DSDL_KMSDRM=OFF \
        -DSDL_ALSA=ON -DSDL_PULSEAUDIO=ON -DSDL_PIPEWIRE=ON -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
        -DSDL_VULKAN=ON -DSDL_OPENGL=ON -DSDL_OPENGLES=ON -DSDL_HIDAPI=ON -DSDL_HIDAPI_LIBUSB=OFF \
        -DSDL_LIBUDEV=ON -DSDL_DBUS=ON -DSDL_IBUS=OFF -DSDL_RPATH=OFF >sdl3-cmake.log
    cmake --build sdl3-build >sdl3-build.log
    cmake --install sdl3-build >/dev/null
    echo "[ok] SDL $SDL (static)"
fi

if [ ! -f appimage/runtime-x86_64 ]; then
    mkdir -p appimage
    curl -L --fail -o appimage/runtime-x86_64.part "$RUNTIME_URL"
    [ "$(shasum -a 256 appimage/runtime-x86_64.part | cut -d' ' -f1)" = "$RUNTIME_SHA256" ] ||
        { echo "setup: the AppImage runtime's checksum is not the pinned one" >&2; rm -f appimage/runtime-x86_64.part; exit 1; }
    mv appimage/runtime-x86_64.part appimage/runtime-x86_64
    echo "[ok] AppImage runtime"
fi
