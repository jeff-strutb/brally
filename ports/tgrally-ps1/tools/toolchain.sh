#!/bin/sh
# The PlayStation toolchain: binutils, GCC and gdb for mipsel-none-elf (the R3000A),
# built from source into build/toolchains/ps1.  Nothing is installed on the host.
#   env: JOBS (default 14)
set -e
cd "$(dirname "$0")/../../.."
ROOT=$PWD
PREFIX=$ROOT/build/toolchains/ps1
SRC=$ROOT/build/toolchains/ps1-src
JOBS=${JOBS:-14}
BINUTILS=binutils-2.43
GCC=gcc-14.2.0
TARGET=mipsel-none-elf
mkdir -p "$PREFIX" "$SRC"
cd "$SRC"
[ -f $BINUTILS.tar.xz ] || curl -fLO https://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.xz
[ -f $GCC.tar.xz ] || curl -fLO https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.xz
[ -d $BINUTILS ] || tar xf $BINUTILS.tar.xz
[ -d $GCC ] || tar xf $GCC.tar.xz
(cd $GCC && [ -e gmp ] || ./contrib/download_prerequisites)

if [ ! -x "$PREFIX/bin/$TARGET-ld" ]; then
  rm -rf build-binutils && mkdir build-binutils && cd build-binutils
  ../$BINUTILS/configure --prefix="$PREFIX" --target=$TARGET \
    --disable-nls --disable-werror --disable-docs --disable-gdb --disable-sim \
    --with-float=soft --with-system-zlib MAKEINFO=true
  make -j$JOBS MAKEINFO=true && make install MAKEINFO=true
  cd ..
fi

if [ ! -x "$PREFIX/bin/$TARGET-gcc" ]; then
  rm -rf build-gcc && mkdir build-gcc && cd build-gcc
  PATH="$PREFIX/bin:$PATH" ../$GCC/configure --prefix="$PREFIX" --target=$TARGET \
    --disable-nls --disable-werror --disable-docs --disable-libssp --disable-libquadmath \
    --disable-libgomp --disable-threads --disable-shared --disable-multilib \
    --enable-languages=c --without-headers --with-newlib=no \
    --with-arch=r3000 --with-abi=32 --with-float=soft --with-system-zlib MAKEINFO=true
  PATH="$PREFIX/bin:$PATH" make -j$JOBS all-gcc all-target-libgcc MAKEINFO=true
  PATH="$PREFIX/bin:$PATH" make install-gcc install-target-libgcc MAKEINFO=true
  cd ..
fi


# gdb for the R3000, against DuckStation's GDB server (the port's debugging)
GDB=gdb-16.3
if [ ! -x "$PREFIX/bin/$TARGET-gdb" ]; then
  cd "$SRC"
  [ -f $GDB.tar.xz ] || curl -fLO https://ftp.gnu.org/gnu/gdb/$GDB.tar.xz
  [ -d $GDB ] || tar xf $GDB.tar.xz
  rm -rf build-gdb && mkdir build-gdb && cd build-gdb
  ../$GDB/configure --prefix="$PREFIX" --target=$TARGET --disable-nls --disable-werror --disable-docs \
    --disable-sim --without-python --without-guile --with-system-zlib \
    --with-gmp-include="$SRC/build-gcc/gmp" --with-gmp-lib="$SRC/build-gcc/gmp/.libs" \
    --with-mpfr-include="$SRC/$GCC/mpfr/src" --with-mpfr-lib="$SRC/build-gcc/mpfr/src/.libs" MAKEINFO=true
  make -j$JOBS all-gdb MAKEINFO=true && make install-gdb MAKEINFO=true
  cd ..
fi
"$PREFIX/bin/$TARGET-gcc" --version | head -1
