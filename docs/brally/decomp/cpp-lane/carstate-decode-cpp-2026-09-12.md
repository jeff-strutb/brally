# Carstate decode cpp

*Recorded 2026-09-12.*

> 2026-09-12: the bitstream DECODE pair routed through the C++ lane -- BrCarStateDecode 0x10007230 BYTE-EXACT (first cpp-lane match of the family), DecodeDelta 0x10007750 at 844/845 multiset 1+1. The decode side does NOT hit the encode pair's VC4.2 wall. Four reusable levers.

**2026-09-12: supply #1 of [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md) executed on the
DECODE half of the bitstream family - 0x10007230 BrCarStateDecode is
BYTE-EXACT as a C++ TU (src/core/cpp/0x10007230.cpp, cpp_sweep match 4/4),
and 0x10007750 BrCarStateDecodeDelta sits at 844/845 B with register-blind
multiset 1+1.** The ENCODE pair's VC4.2 wall ([vc42-is-the-real-compiler](../toolchain/vc42-is-the-real-compiler.md))
does NOT bite on the decode side - no quantiser-argument push-early/narrow
conflict exists in a reader.

**Levers, in reuse order (all in the two .cpp headers):**
1. Thiscall reader as a declared-not-defined class method - kills the ~32
   push/add-esp shim pairs exactly like the writer did on 0x10006BA0.
2. `uint8_t` scalar local shifted in place ([byte-slot-idiom-cracked](../levers/byte-slot-idiom-cracked.md))
   gives `shl al,N`; the REST of the idiom is a CHAR-WIDTH PROTOTYPE for the
   consuming helper in THIS TU (extern "C", ABI-identical): with
   `float BrFixUnpackS8Q3(unsigned char)` the promoted push is UNMASKED
   (`push eax` raw) - with int32 it grows `and eax,0xff`. That prototype
   trick took Decode from 411 diffs to MATCH.
3. The inlined delta-merge recomputes `prev & keepMask` INSIDE each arm,
   code==0 tested first; a hoisted `hi = prev & keep` above the chain is a
   different shape.
4. `short`-typed pack prototype makes `>> 1` narrow to `sar ax,1` (same
   assignment-gated narrowing as the encode dossier's `int16_t q`).

**DecodeDelta's last instruction:** ours `shl R,1` in place, orig
`lea eax,[esi+esi]` - every C++ spelling of the double value-numbers to the
in-place shift when the source register dies (probed 6 ways, dead list in
the file header). Park; do not re-grind spellings.

**Toolchain notes:** the cpp lane scores via `tools/cpp_sweep.py <file>` into
report_cpp.csv (3 flag variants; objs at
build/match/obj_cpp/<VA>_sweep_<va>_N.obj, symbol UNPREFIXED). fn.py/sbs.py
do not read the cpp lane; multiset via a 15-line capstone script over
parse_coff_obj.  tools/cpp_twin_retire.py NO LONGER EXISTS
([cpp-twin-retire-chore](cpp-twin-retire-chore.md) is stale) - the C twin keeps its d3d-VA tag, the
cpp file claims the glide VA; no collision.

Related: [cxx-thiscall-wall](cxx-thiscall-wall.md), [thiscall-via-fastcall](thiscall-via-fastcall.md),
[brffb-cluster-2026-09-12](../functions/brffb-cluster-2026-09-12.md).
