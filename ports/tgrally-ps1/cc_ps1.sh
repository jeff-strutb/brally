#!/bin/sh
# One core TU -> object for the PlayStation (build.sh runs this in parallel):
# the PlayStation's own version of the TU when ports/tgrally-ps1/game has one
# (the game's code made native for the console), else the core's.
#   env: CC, CFLAGS, OUT
f="$1"
n=$(echo "$f" | sed 's#ports/tgrally/src/##; s#/#__#g')
src="$f"
fork=ports/tgrally-ps1/game/$(echo "$f" | sed 's#ports/tgrally/src/##')
[ -f "$fork" ] && src="$fork"
if $CC $CFLAGS -iquote "$(dirname "$f")" -x c -std=gnu89 -c "$src" -o "$OUT/obj/$n.o" 2> "$OUT/obj/$n.err"; then
  echo "OK $f"
else
  rm -f "$OUT/obj/$n.o"; echo "FAIL $f"
fi
