#!/bin/sh
# Top Gear Rally for the PlayStation: compile the game's core (the native
# port's, ports/tgrally/src, unchanged) for the R3000A with the toolchain
# from ports/tgrally-ps1/tools/toolchain.sh.
#   env: JOBS (default 14), OUT (default build/tgrally/ps1)
#   build.sh FILE...   compile those TUs only and print their errors
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/tgrally/ps1}
JOBS=${JOBS:-14}
TC=build/toolchains/ps1/bin/mipsel-none-elf
[ -x $TC-gcc ] || { echo "build: run ports/tgrally-ps1/tools/toolchain.sh first" >&2; exit 2; }
PY=.venv/bin/python
[ -x $PY ] || PY=python3
P=ports/tgrally/platform
Q=ports/tgrally-ps1/platform
CC=$TC-gcc
CFLAGS="-O2 -g -G0 -march=r3000 -mtune=r3000 -msoft-float -mno-abicalls -fno-pic -ffreestanding -nostdinc \
  -fno-strict-aliasing -fwrapv -ffp-contract=off -fno-common -fno-builtin-sinf -fno-builtin-cosf \
  -w -Werror=implicit-function-declaration -Werror=implicit-int -Werror=int-conversion \
  -Werror=incompatible-pointer-types \
  -I$Q/include -I$Q/libc/include -isystem $(dirname $($CC -print-libgcc-file-name))/include \
  -Iports/tgrally/include -I$P/include -Iports/brally/platform/host \
  -include $P/include/ultra64.h -include $P/include/tgr_core.h -DTGR_CORE -DTGR_PS1 -include $P/include/tgr_libc.h"
export CC CFLAGS OUT
mkdir -p $OUT/obj
OUT=$OUT TGR_TARGET=mipsel-unknown-none-elf $PY ports/tgrally/tools/globals.py >&2
if [ $# -gt 0 ]; then
  for f in "$@"; do
    ports/tgrally/cc.sh "$f"
    n=$(echo "$f" | sed 's#ports/tgrally/src/##; s#/#__#g')
    grep -A3 "error:" $OUT/obj/$n.err | head -${ERRS:-12}
  done
  exit 0
fi
find ports/tgrally/src -name '*.c' | grep -v -f ports/tgrally-ps1/skip.txt | sort > $OUT/tus.txt
xargs -P $JOBS -n 1 ports/tgrally/cc.sh < $OUT/tus.txt | sort > $OUT/compile.txt
echo "core: $(grep -c '^OK' $OUT/compile.txt) of $(wc -l < $OUT/tus.txt | tr -d ' ') TUs compiled"
