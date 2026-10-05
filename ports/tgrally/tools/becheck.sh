#!/bin/sh
# Every place a big-endian value (cartridge data, RSP memory) reaches code
# that reads it natively: a BE pointer passed where a native one is wanted,
# a BE value given to a printf format.  The compiler flags these as warnings
# only; this makes them a list (and build.sh a failure).
cd "$(dirname "$0")/../../.."
P=ports/tgrally/platform
for f in $(find ports/tgrally/src -name '*.c' | sort); do
  clang -O0 -fsyntax-only -std=gnu89 -Wno-everything -Wincompatible-pointer-types -Wformat \
    -fno-caret-diagnostics -ferror-limit=0 -Iports/tgrally/include -I$P/include \
    -include $P/include/ultra64.h -include $P/include/tgr_core.h "$f" 2>&1 |
    grep -E "warning:.*'(be16_t|be32_t|BrVec3be|Gfx|Vtx|Mtx|struct BrTrack|BrTrack|BrPath|BrGate|BrSpecial|BrTexSlot|BrTexAnim|BrModel|BrAnim|BrCarModel)[^']*'.* to (parameter of type|.*type) '(float|int|short|unsigned|BrVec3 |double|char)"
done
