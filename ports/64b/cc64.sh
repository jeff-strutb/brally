#!/bin/sh
# One core TU -> native object (build64.sh runs this in parallel).
#   env: CC, CFLAGS, OUT
f="$1"
n=$(echo "$f" | sed 's#ports/64b/src/core/##; s#/#__#g')
case "$f" in
  *.cpp) X="-x c++ -std=c++98 -fno-exceptions -fno-rtti";;
  *)     X="-x c -std=gnu89";;
esac
AL=
[ -f "ports/64b/alias/$n.h" ] && AL="-include ports/64b/alias/$n.h"
if $CC $CFLAGS $AL $X -c "$f" -o "$OUT/obj/$n.o" 2> "$OUT/obj/$n.err"; then
  echo "OK $f"
else
  rm -f "$OUT/obj/$n.o"; echo "FAIL $f"
fi
