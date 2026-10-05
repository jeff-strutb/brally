# Equivalence oracle in image

*Recorded 2026-09-10.*

> 2026-09-10: t3b_verify now runs both sides inside the mapped BRGlide image (relocations resolved, globals real, calls executing the original's callees). Three false-DIFF sources found and fixed; found a REAL logic bug in 0x1006D850 the byte-diff pipeline cannot see.

** THE ORACLE IS THE ONLY GATE THAT TESTS WHAT T3 CLAIMS.** A1..A4 compare
compiled bytes and argue the differences look like compiler choices - an
inference about instruction SHAPE. Gate A5 (`tools/t3b_verify.py`) executes
both functions on identical inputs and compares what they produce.

**Why it was almost useless:** it refused any object carrying a relocation  - 
any function reading a global, using a string or float constant, or calling
anything. Measured over the T2 pile that was **213 of 233 (91%)**, against 11
for every other reason combined. Only 6 of 98 certified tags carried a
behavioural verdict; the other 92 said UNCLASSIFIED, and **UNCLASSIFIED PASSES
the gate**.

**What it does now** (commit 3e38a95, new `tools/t3b_env.py`): both sides run
inside the real DLL, mapped at its image base. Our relocations are resolved to
the original's own addresses via `reloc_fill.fill_function`, so both name the
same globals and callees; globals hold their real contents; and **a call
executes the ORIGINAL's callee on both sides**, so the function under test is
the only thing that differs and calling the wrong helper is a divergence, not
a stub. `.text` is disassembled once and shared (x87emu.Machine now takes a
prebuilt index - rebuilding a 130k index per run is what made it unusable).

 **THREE FALSE-DIFF SOURCES, each of which reported a bug that was not
there.** Any future comparison harness must handle all three:
1. our function is usually LONGER than the original, so substituted bytes bury
   the next function's entry point - `shadows_a_neighbour` refuses;
2. the callee's OWN STACK FRAME was compared, though two compilations lay out
   spill slots differently by design;
3. so were the INCOMING ARGUMENT SLOTS - in cdecl the callee owns its
   parameter copies and reusing one as scratch is ordinary.
A fourth is reported, not judged: x87 rounding gets its own verdict
**EQUIV-MODULO-FP**, because the interpreter has 64-bit registers where the
hardware has 80-bit, so a last-place disagreement is evidence about the
interpreter, not the transcription.

** IT FOUND A REAL BUG THE BYTE PIPELINE CANNOT SEE: 0x1006D850
BrRbIntegrateState** (slice3_44.c, a T2 row, NOT certified - no tag is
invalidated). The tree's spelling holds all four `dt * qDot[i]` products live
before the four adds. Compiled by MSVC 5.0 /O2 that produces:
`quat[2]_out = quat[2] + qDot[3]` (**the dt multiply is missing entirely**) and
`quat[3]_out = quat[3] + dt*dt*qDot[2]`. The original computes
`normalise(quat + dt*qDot)`, which the oracle reproduces to 7 significant
figures by hand. Ruled out before believing it: obj force-rebuilt; relocation
resolution provably non-corrupting (only the 4-byte call target differs, 98/98
instructions identical); every instruction in the failing block decoded against
its Intel encoding (`d8c3` IS `fadd st(0),st(3)`). A spelling that consumes
each product immediately instead of holding four live comes out
EQUIV-MODULO-FP against the original - so the current spelling compiles to
wrong code. **NOT FIXED - it changes the byte residue (-13 B, rows 57 -> 60)
and the project lead should choose.**

**Coverage is still the limit, not the design.** T2 pile went 9 EQUIVALENT ->
14 + 1 EQUIV-MODULO-FP + 1 DIFF; certified tags 6 -> 8. Remaining blockers,
measured: **96** name a symbol with no known address (file-static stand-ins
like `g_menu$S445`, `$T` jump tables, and globals referenced ONLY from
functions that are not byte-exact, so `reloc_learn.py` can never learn them);
**48** have a by-value struct parameter; **28** hit an opcode x87emu does not
model.  The next real unlock is recovering those addresses POSITIONALLY from
the T2 function itself - `reloc_learn`'s inversion (`target = orig_dword -
addend`) guarded by divergence.py's proven alignment rather than by
whole-function masked-match.

Cheap win already taken: names that spell out their own address (`DAT_10697a58`,
`FUN_10024490`, `BrSub10071130`) are resolved by `t3b_env.address_in_name`,
accepted ONLY when the address lands in a real mapped section. That alone took
resolvable functions 44 -> 86. Also honours `__fastcall`/`BR_THISCALL1` when
placing args (ecx then edx to the first two register-eligible params).

Related: [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [t3-frontier-map-2026-09-10b](../log/t3-frontier-map-2026-09-10b.md),
[t3-certified-standard](../rules/t3-certified-standard.md), [byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md).
