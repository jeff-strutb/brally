# Narrow-width `sar` at a bitstream write site + `__inline` float-flag test

Context: 0x10006510 BrCarStateEncode (slice2_12.c), the s16/s8 quantiser
writes and the 13 boolean flag writes. Restored the tree to its documented
shape-exact spelling (REGNORM 108+110 -> 33+0) 2026-08-31.

## Proven levers (shape-exact)

1. **Shift a quantiser result at its OWN width, spilled to a named narrow
   local before the call.** The original emits `sar ax,8; movsx ecx,ax;
   push ecx` (16-bit shift, widened only at the push). To reproduce:
   ```c
   #ifdef BR_MATCHING_BUILD
   #define BrSar16(v,n) ((short)(v) >> (n))
   #define BrSar8(v,n)  ((signed char)(v) >> (n))
   #endif
   short s; signed char c;
   s = BrSar16(BrFixPackS16Q15Neg(pSrc->f00), 8);
   WriteBits(pBs, s, 8);          /* NOT WriteBits(pBs, s = BrSar16(...), 8) */
   ```
   The quantiser must return `short`/`signed char` (narrow) not `int32_t`.
   The named-local two-statement form is what makes VC5 keep the shift at
   16/8-bit and add the `movsx` only at the push.

2. **A `!= 0.0f` flag test must be `static __inline`, or VC5 emits a CALL.**
   The original inlines `fld; fcomp 0.0; fnstsw; test ah,0x40; jne; mov r,1`
   at each of 13 flag writes. A plain `static int BrIsNonZero(float)` is
   NOT inlined at the sweep's opt and shows up as 13 `call` + spill shapes
   MISSING the fld/fcomp/fnstsw/test/jne set. Guard the `__inline` +
   `!= 0.0f` form under BR_MATCHING_BUILD; keep the `(v<0)||(v>0)` form for
   the port build.

## The wall these DO NOT solve (documented, do not re-grind)

REGNORM is an order-blind multiset: `push I` is the same shape whether the
width byte is pushed EARLY (before the quantiser call, as the original does
at all ~24 sites) or LATE (just before WriteBits, as every C/C++ spelling
does). So 33+0 hides two residues:

- **push-early vs push-late.** Narrow-shift (lever 1) and push-early are
  MUTUALLY EXCLUSIVE under VC5 / VS97 SP3 / VC6 RTM across 18 spellings.
  Assignment-in-arg gives narrow shift but push-late; pure-expression arg
  gives push-early but wide (32-bit) shift. See the 40-line note in
  `src/core/cpp/0x10006510.cpp`. Hypothesis: a 4th (VC4.2-era) front end.
- **thiscall edx.** WriteBits (0x1006D0B0) is `this` in ecx + both args on
  the stack, callee-cleaned (`ret 8`) = a C++ member. The C `__fastcall`
  dead-edx shim costs one `xor edx,edx` per site (the 33 EXTRA). The C++
  TU with a declared-not-defined class method removes these but still hits
  the push-early wall above.

Lesson: **never call a bitstream quantiser sequence "one xor away" from
REGNORM alone - verify push ordering with divergence.py bytes.**
