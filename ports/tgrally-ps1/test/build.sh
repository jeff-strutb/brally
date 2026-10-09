#!/bin/sh
# test/build.sh NAME: a test program (test/NAME.c) with the C library and the hardware glue
set -e
cd "$(dirname "$0")/../../.."
TC=build/toolchains/ps1/bin/mipsel-none-elf; O=build/tgrally/ps1/test; Q=ports/tgrally-ps1/platform
mkdir -p $O
CF="-O2 -g -G0 -march=r3000 -msoft-float -mno-abicalls -fno-pic -ffreestanding -nostdinc -I$Q/include -I$Q/libc/include -isystem $(dirname $($TC-gcc -print-libgcc-file-name))/include -Iports/brally/platform/host -Iports/tgrally/platform/include"
for s in ports/tgrally-ps1/test/$1.c $Q/libc/libc.c $Q/hw/ps1hw.c $Q/hw/irq.c ports/tgrally/platform/libc/xprintf.c; do $TC-gcc $CF -c $s -o $O/$(basename $s .c).o; done
for a in crt0 ctx exc; do $TC-gcc -march=r3000 -msoft-float -fno-pic -mno-abicalls -c $Q/hw/$a.s -o $O/$a.o; done
$TC-ld --defsym RAM_TOP=0x80200000 -T $Q/hw/ps1.ld $O/crt0.o $O/$1.o $O/libc.o $O/ps1hw.o $O/irq.o $O/ctx.o $O/exc.o $O/xprintf.o $($TC-gcc -print-libgcc-file-name) -o $O/$1.elf
.venv/bin/python ports/tgrally-ps1/tools/mkexe.py $O/$1.elf $O/$1.exe
