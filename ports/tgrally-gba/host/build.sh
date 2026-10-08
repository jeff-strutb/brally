#!/bin/sh
# Build mGBA's library (build/tgrally/gba/mgba-src, an mGBA checkout) and gbarun against it.
set -e
cd "$(dirname "$0")/../../../build/tgrally/gba"
mkdir -p mgba-build
cd mgba-build
[ -f Makefile ] || cmake ../mgba-src -DCMAKE_BUILD_TYPE=Release -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_SHARED=OFF \
    -DBUILD_STATIC=ON -DBUILD_PERF=ON -DUSE_FFMPEG=OFF -DUSE_LUA=OFF -DUSE_SQLITE3=OFF -DUSE_ELF=OFF \
    -DUSE_DISCORD_RPC=OFF -DUSE_EDITLINE=OFF -DUSE_LIBZIP=OFF -DUSE_EPOXY=OFF -DBUILD_GL=OFF \
    -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DM_CORE_GB=OFF -DUSE_JSON_C=OFF -DENABLE_SCRIPTING=OFF > cmake.log
make -j14 mgba mgba-perf > make.log
F=$(find . -name flags.make -path "*mgba-perf.dir*")
DEFS=$(grep "^C_DEFINES" $F | cut -d= -f2-)
INC=$(grep "^C_INCLUDES" $F | cut -d= -f2-)
# the library's own defines and flags: its structures change shape with them
cc $DEFS $INC -Wall -fwrapv -O3 -DNDEBUG -flto -std=c11 ../../../../ports/tgrally-gba/host/gbarun.c -o ../gbarun \
    libmgba.a -framework Foundation -lm -lz /opt/homebrew/lib/libpng.dylib /opt/homebrew/lib/libfreetype.dylib
echo "built build/tgrally/gba/gbarun"
