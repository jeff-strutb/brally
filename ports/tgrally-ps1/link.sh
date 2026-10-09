#!/bin/sh
# Link Top Gear Rally for the PlayStation: the core (build.sh), the symbol
# table and arena generated from the source (ports/tgrally/tools/globals.py),
# the platform layer shared with the native port where it carries over
# (ports/tgrally/platform), and the console's own (platform/).
#   env: OUT (default build/tgrally/ps1), RAM (2 | 8 MB, default 8 while the
#        game's memory does not yet fit the retail console's 2)
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/tgrally/ps1}
RAM=${RAM:-8}
export OUT
TC=build/toolchains/ps1/bin/mipsel-none-elf
P=ports/tgrally/platform
Q=ports/tgrally-ps1/platform
H=ports/brally/platform/host
PY=.venv/bin/python
ports/tgrally-ps1/build.sh
if grep -q '^FAIL' $OUT/compile.txt; then
  grep '^FAIL' $OUT/compile.txt >&2
  echo "link: core TUs failed to compile; not linking" >&2
  exit 1
fi
CF="-O2 -g -G0 -march=r3000 -mtune=r3000 -msoft-float -mno-abicalls -fno-pic -ffreestanding -nostdinc \
  -fno-strict-aliasing -fwrapv -ffp-contract=off -std=gnu11 -Wall -Wno-unused-function \
  -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion \
  -I$Q/include -I$Q/libc/include -isystem $(dirname $($TC-gcc -print-libgcc-file-name))/include \
  -I$P/include -I$P/os -I$H -Iports/tgrally/include -DTGR_PS1"
mkdir -p $OUT/plat
# the arena: the N64's 4 MB (the game never addresses past it)
sed 's/^\.space 0x800000/.space 0x400000/; s/^\.size tgr_rdram, 0x800000/.size tgr_rdram, 0x400000/' \
  $OUT/gen/arena.s > $OUT/plat/arena.s
SRCS="$P/os/addr.c $P/os/thread.c $P/os/si.c $P/os/view.c $P/os/touch.c $P/os/sha1.c $P/audio/mixer.c $P/libc/xprintf.c \
      $Q/hw/ps1hw.c $Q/hw/irq.c $Q/os/host_ps1.c $Q/os/main_ps1.c $Q/os/io_ps1.c $Q/os/lift_ps1.c $Q/os/trace_ps1.c \
      $Q/gfx/gfx_ps1.c $Q/gfx/rcp_ps1.c $Q/audio/out_ps1.c $Q/libc/libc.c $OUT/gen/tgr_syms.c"
OBJS=
fail=0
for s in $SRCS; do
  o=$OUT/plat/$(echo $s | sed 's#/#_#g; s#\.c$#.o#')
  OBJS="$OBJS $o"
  if [ ! -f $o ] || [ $s -nt $o ] || [ -n "$(find $Q/include $Q/libc/include $P/include -newer $o -name '*.h' 2>/dev/null | head -1)" ]; then
    $TC-gcc $CF -c $s -o $o || fail=1
  fi
done
[ $fail = 0 ] || { echo "link: platform failed to compile" >&2; exit 1; }
for s in $Q/hw/crt0.s $Q/hw/ctx.s $Q/hw/exc.s $OUT/plat/arena.s; do
  o=$OUT/plat/$(basename $s .s).o
  OBJS="$OBJS $o"
  $TC-gcc -march=r3000 -msoft-float -fno-pic -mno-abicalls -c $s -o $o
done
RAMTOP=$( [ "$RAM" = 8 ] && echo 0x80800000 || echo 0x80200000 )
$TC-ld --defsym RAM_TOP=$RAMTOP -T $Q/hw/ps1.ld --gc-sections -Map $OUT/tgrally.map -o $OUT/tgrally.elf \
  $OBJS $OUT/obj/*.o $($TC-gcc -print-libgcc-file-name)
$TC-size $OUT/tgrally.elf
$PY ports/tgrally-ps1/tools/mkexe.py $OUT/tgrally.elf $OUT/tgrally.exe
