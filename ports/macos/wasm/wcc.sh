#!/bin/sh
# One TU -> wasm32 object (build_wasm.sh runs this in parallel).
#
#   source --clang wasm32 -O0--> wasm IR --+
#   source --clang i686   -O0--> x86 IR  --+--> ccmark.py --> marked IR
#   marked IR --clang wasm32 -O2--> object
#
# Two IR views because calling conventions are gone in wasm but the tree
# spells MSVC conventions in whatever form reproduces the x86 bytes: the x86
# IR (i686-w64-windows-gnu: Win32 data layout, Itanium names, thiscall C++
# methods) records each call's convention; ccmark.py carries the conventions
# of indirect calls into the wasm IR, and w2c.py reads the rest from the IRs.
#
# The object name encodes the source path. A TU clang rejects for MSVC-only
# laxness goes through msvc_lax.py: a port-side rewrite of a PREPROCESSED copy
# under build/wasm/lax. The source file itself is never touched.
#   env: LLVM (clang dir), OUT (build/wasm)
src="$1"
n=$(echo "$src" | sed 's#/#__#g')
obj="$OUT/obj/$n.o"
case "$src" in
  # -fapple-kext: every virtual call goes through the vtable, as the original
  # makes it; the decomp's class declarations are views over memory whose
  # real vptr the original set, so clang must not devirtualize on them.
  *.cpp) X="-x c++ -std=c++98 -fno-exceptions -fno-rtti -fapple-kext"; XI=c++-cpp-output; K="-fno-exceptions -fno-rtti -fapple-kext";;
  *)     X="-x c -std=gnu89"; XI=cpp-output; K="";;
esac
COMMON="-fms-extensions -fshort-wchar -fno-builtin -femit-all-decls
  -fno-strict-aliasing -fwrapv -w
  -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
  -Wno-error=return-mismatch -Wno-error=incompatible-function-pointer-types
  -Wno-error=implicit-int -Wno-error=implicit-function-declaration
  -D_M_IX86=500 -D_X86_ -D_WIN32 -DWIN32 -D_MSC_VER=1100
  -D_INTEGRAL_MAX_BITS=64 -DBR_MATCHING_BUILD
  -Iinclude -I$OUT/inc -Iports/macos/wasm/inc -Itools/msvc5-compat
  -Itools/msvc5/include"
IR="-O0 -Xclang -disable-O0-optnone -S -emit-llvm"

build() {   # $1 = input, $2 = language flags
  "$LLVM/clang" --target=wasm32 $COMMON $IR $2 "$1" -o "$obj.wasm.ll" 2>"$obj.err" &&
  "$LLVM/clang" --target=i686-w64-windows-gnu -mlong-double-64 $COMMON $IR $2 "$1" \
      -o "$obj.x86.ll" 2>>"$obj.err" &&
  python3 ports/macos/wasm/ccmark.py "$obj.wasm.ll" "$obj.x86.ll" "$obj.mk.ll" 2>>"$obj.err" &&
  "$LLVM/clang" --target=wasm32 -O2 -w -c "$obj.mk.ll" -o "$obj" 2>>"$obj.err"
}

rm -f "$obj"
if "$LLVM/clang" --target=wasm32 $COMMON $X -fsyntax-only "$src" 2>"$obj.err"; then
  build "$src" "$X" && exit 0
  rm -f "$obj"; echo "FAIL $src"; exit 0
fi
mkdir -p "$OUT/lax"
if python3 ports/macos/wasm/msvc_lax.py "$LLVM/clang" "$src" "$OUT/lax/$n.i" -- \
       --target=wasm32 $COMMON $X >"$obj.err" 2>&1 &&
   build "$OUT/lax/$n.i" "$K -x $XI"; then
  echo "LAX $src"
else
  rm -f "$obj"; echo "FAIL $src"
fi
