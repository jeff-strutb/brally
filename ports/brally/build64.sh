#!/bin/sh
# The portable 64-bit core: compile every core TU natively (no link yet).
#   env: JOBS (default 14), CC (default clang), TARGET (default host)
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/brally/null-null}
mkdir -p $OUT/obj
JOBS=${JOBS:-14}
CC=${CC:-clang}
export CC OUT
# locals start at 0: the original reads a few never-written stack slots (the
# pad axes on the pause frame, a replay record's spin), and the core must not
# take whatever an earlier call left there (AUTOINIT=pattern finds them)
CFLAGS="-O2 ${GFLAG:--g} -ftrivial-auto-var-init=${AUTOINIT:-zero} -Wno-everything -Wimplicit-function-declaration -Wimplicit-int -D_FORTIFY_SOURCE=0 -fms-extensions -fdeclspec -fno-strict-aliasing -fwrapv -ffp-contract=off -Wno-return-mismatch -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types -Werror=implicit-function-declaration -Werror=implicit-int ${WARN}
  -Iports/brally/platform/include -Iports/brally/include -include ports/brally/platform/include/win32.h -include ports/brally/platform/include/glide.h -include ports/brally/platform/include/br_x87.h -include ports/brally/include/br_crt.h -include ports/brally/include/br_addr32.h -include ports/brally/platform/include/br_lp64.h -include ports/brally/include/br_globals.h -include ports/brally/include/br_funcs.h"
# a Windows target links nothing from a DLL: the dllimport the sources
# spell (as the original's link did) must not ask the system's CRT for one
# ... and the Win32 emulation takes private names (tools/brally/winnames.py)
case "$($CC -dumpmachine 2>/dev/null)" in *mingw*|*windows*) CFLAGS="-include ports/brally/platform/include/br_winemu.h $CFLAGS -Ddllimport=";; esac
export CFLAGS
case "$($CC -dumpmachine 2>/dev/null)" in x86_64*) BR_ALIGN8=1;; *) BR_ALIGN8=0;; esac
export BR_ALIGN8
# one file: build64.sh FILE...  (prints OK/FAIL and the errors)
if [ $# -gt 0 ]; then
  for f in "$@"; do ports/brally/cc64.sh "$f"; n=$(echo "$f" | sed 's#ports/brally/src/core/##; s#/#__#g'); grep -A3 "error:" $OUT/obj/$n.err | head -${ERRS:-12}; done
  exit 0
fi
find ports/brally/src/core \( -name '*.c' -o -name '*.cpp' \) | sort > $OUT/tus.txt
# the link takes every object in $OUT/obj: drop those whose source was renamed
# or removed, or the old copy links beside the new one
sed 's#ports/brally/src/core/##; s#/#__#g; s#$#.o#' $OUT/tus.txt > $OUT/objs.txt
(cd $OUT/obj && ls | grep '\.o$' | grep -vxFf ../objs.txt | xargs rm -f)
xargs -P $JOBS -n 1 ports/brally/cc64.sh < $OUT/tus.txt | sort > $OUT/compile.txt
echo "portable core: $(grep -c '^OK' $OUT/compile.txt) of $(wc -l < $OUT/tus.txt | tr -d ' ') TUs compiled"
