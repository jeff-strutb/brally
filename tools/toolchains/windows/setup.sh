#!/bin/sh
# tools/toolchains/windows/setup.sh: SDL 3 for the Windows builds' game
# controllers (ports/brally/platform/host/sdl/sdl_pads.c, in host/windows),
# static, built with the toolchain the Windows builds use
# (ports/brally/tools/wincc.sh: clang with mingw-w64), with only what its
# gamepad API needs: no video or rendering, which host/windows does itself
# (its audio compiled in, never started: SDL allows DirectInput only with it).
# The controllers' drivers are all SDL's own: XInput, Windows.Gaming.Input,
# RawInput, DirectInput (a plain USB pad) and HID (PlayStation, Switch, 8BitDo...).
#
#   sdl3/   the library and its headers
#
# Download (once): SDL's source, 16 MB (the Linux setup's copy if it is there).
set -e
cd "$(dirname "$0")"
W=$(pwd)
R=$(cd ../../.. && pwd)
SDL=3.4.20
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz"
mkdir -p dl
if [ ! -f sdl3/lib/libSDL3.a ]; then
    [ -f dl/sdl3.tar.gz ] || { [ -f ../linux/dl/sdl3.tar.gz ] && cp ../linux/dl/sdl3.tar.gz dl/; } || true
    [ -f dl/sdl3.tar.gz ] || curl -L --fail -o dl/sdl3.tar.gz "$SDL_URL"
    rm -rf sdl3-src sdl3-build && mkdir -p sdl3-src
    tar xzf dl/sdl3.tar.gz -C sdl3-src --strip-components 1
    cat > sdl3-toolchain.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $R/ports/brally/tools/wincc.sh)
set(CMAKE_CXX_COMPILER $R/ports/brally/tools/wincc.sh)
set(CMAKE_RC_COMPILER $(command -v x86_64-w64-mingw32-windres))
set(CMAKE_AR $(command -v x86_64-w64-mingw32-ar))
set(CMAKE_RANLIB $(command -v x86_64-w64-mingw32-ranlib))
# (a library alone: the compiler's checks link nothing, mingw-w64's gcc links the games)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF
    cmake -S sdl3-src -B sdl3-build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$W/sdl3-toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$W/sdl3 \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
        -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_GPU=OFF -DSDL_AUDIO=ON -DSDL_CAMERA=OFF -DSDL_DIALOG=OFF \
        -DSDL_TRAY=OFF -DSDL_DIRECTX=ON -DSDL_JOYSTICK=ON -DSDL_HIDAPI=ON -DSDL_HAPTIC=ON -DSDL_SENSOR=ON \
        -DSDL_POWER=OFF >sdl3-cmake.log
    cmake --build sdl3-build >sdl3-build.log
    cmake --install sdl3-build >/dev/null
    echo "[ok] SDL $SDL (static, Windows, gamepads)"
fi
