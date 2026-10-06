#!/bin/sh
# One core TU -> native object (build64.sh runs this in parallel).
#   env: CC, CFLAGS, OUT
f="$1"
n=$(echo "$f" | sed 's#ports/brally/src/core/##; s#/#__#g')
case "$f" in
  *.cpp) X="-x c++ -std=c++98 -fno-exceptions -fno-rtti";;
  *)     X="-x c -std=gnu89";;
esac
# x86-64, C++: the original's data promises only natural alignment, but clang
# takes any array of 16 bytes or more to be 16-byte aligned (br_crt.h says
# why). C files get an explicit 8 from a macro on `extern`; a C++ file cannot
# (`extern "C"`), so it is preprocessed and every other extern given the
# attribute before it is compiled.
if [ "$BR_ALIGN8" = 1 ] && [ "${f%.cpp}" != "$f" ]; then
  $CC $CFLAGS $X -E "$f" 2> "$OUT/obj/$n.err" |
    perl -pe 's/\bextern\b(?!\s*")/extern __attribute__((aligned(8)))/g' > "$OUT/obj/$n.ii" &&
  $CC $CFLAGS -x c++-cpp-output -std=c++98 -fno-exceptions -fno-rtti -c "$OUT/obj/$n.ii" -o "$OUT/obj/$n.o" 2>> "$OUT/obj/$n.err" &&
  rm -f "$OUT/obj/$n.ii" && echo "OK $f" || { rm -f "$OUT/obj/$n.o"; echo "FAIL $f"; }
  exit 0
fi
if $CC $CFLAGS $X -c "$f" -o "$OUT/obj/$n.o" 2> "$OUT/obj/$n.err"; then
  echo "OK $f"
else
  rm -f "$OUT/obj/$n.o"; echo "FAIL $f"
fi
