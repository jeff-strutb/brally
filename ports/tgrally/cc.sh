#!/bin/sh
# One core TU -> native object (build.sh runs this in parallel).
#   env: CC, CFLAGS, OUT
f="$1"
n=$(echo "$f" | sed 's#ports/tgrally/src/##; s#/#__#g')
if $CC $CFLAGS -x c -std=gnu89 -c "$f" -o "$OUT/obj/$n.o" 2> "$OUT/obj/$n.err"; then
  echo "OK $f"
else
  rm -f "$OUT/obj/$n.o"; echo "FAIL $f"
fi
