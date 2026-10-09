#!/bin/sh
# Top Gear Rally, native: compile every core TU (and, with link.sh, the
# platform layer).  A TU that owns data symbols compiles through its wrapper
# in build/tgrally/null-null/gen/own (tools/globals.py) so it defines them.
#   env: JOBS (default 14), CC (default clang), OUT (default build/tgrally/null-null;
#        link.sh passes its own, build/tgrally/null-null/HOST-RENDER)
#   build.sh FILE...   compile those TUs only and print their errors
set -e
cd "$(dirname "$0")/../.."
OUT=${OUT:-build/tgrally/null-null}
JOBS=${JOBS:-14}
CC=${CC:-clang}
PY=.venv/bin/python
[ -x $PY ] || PY=python3
P=ports/tgrally/platform
CFLAGS="-O2 ${GFLAG:--g} -Wno-everything -Werror=implicit-function-declaration -Werror=implicit-int \
  -Werror=incompatible-library-redeclaration -Werror=int-conversion -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=void-pointer-to-int-cast -Werror=int-to-void-pointer-cast -Werror=incompatible-function-pointer-types -Wno-error=int-to-pointer-cast-not-really -fno-strict-aliasing -fwrapv -ffp-contract=off \
  -fno-common -ferror-limit=0 -fno-builtin-sinf -fno-builtin-cosf ${WARN} \
  -Iports/tgrally/include -I$P/include -Iports/brally/platform/host -include $P/include/ultra64.h -include $P/include/tgr_core.h -DTGR_CORE -include $P/include/tgr_libc.h"
export CC CFLAGS OUT
mkdir -p $OUT/obj
$PY ports/tgrally/tools/globals.py >&2
if [ $# -gt 0 ]; then
  for f in "$@"; do
    ports/tgrally/cc.sh "$f"
    n=$(echo "$f" | sed 's#ports/tgrally/src/##; s#/#__#g')
    grep -A3 "error:" $OUT/obj/$n.err | head -${ERRS:-12}
  done
  exit 0
fi
find ports/tgrally/src -name '*.c' | sort > $OUT/tus.txt
xargs -P $JOBS -n 1 ports/tgrally/cc.sh < $OUT/tus.txt | sort > $OUT/compile.txt
echo "core: $(grep -c '^OK' $OUT/compile.txt) of $(wc -l < $OUT/tus.txt | tr -d ' ') TUs compiled"
