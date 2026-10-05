# Objdl a5 limit

*Recorded 2026-09-16.*

> SUPERSEDED 2026-09-19: BrObjDlBuild 0x1000CBA0 is now CERTIFIED T3 (commit 04f91a74). The 'no teeth on the clipper' verdict was WRONG -- the oracle profile seeded ZERO vertices (a byte-masking bug), so every transform coefficient was x0 and no bug could be caught. Fixed + negative-controlled. LESSON: a degenerate seed world = a silent false EQUIVALENT.

** RESOLVED 2026-09-19 -- CERTIFIED T3 (commit 04f91a74). The wall was a
SEEDING BUG, not a real limit.** The oracle profile (`_odl_bss`) seeded the G_VTX
vertices as ZERO: the gate `if off in (0,4,8)` served only byte 0 of each
coordinate float and returned 0 for bytes 1-3, collapsing every vertex to ~0.
With zero vertices `pVtx[3]=m00*vx+...+m30` reduces to the translation `m30`
alone -- every COEFFICIENT is multiplied by zero -- so no coefficient/clip bug
could change the output. That IS the "opaque transform, can't straddle the clip
threshold" symptom the old note blamed on the clipper: it was degenerate seeds.
Fix: gate on the word (`off < 0xc`). Then the negative controls that were silent
ALL fire (DIFF): m00<->m20 swap, corner-index shift, clip-flag bit, counter
x3->x2, case-0x04->0x05 dispatch mislabel; correct code stays EQUIVALENT.
Certified via t3.py --qualify (GATE 0/A/B PASS). Also fixed t3.py to supersede
gate **A6** (jump-table bytes) on a clean A5 as A1-A4 are -- A6 compares the
orig table zone against the same offset in our differently-sized obj (a layout
artefact); A5 verifies dispatch by running each side off its own in-.text table.

** THE REUSABLE LESSON: a DEGENERATE seed world produces a FALSE EQUIVALENT,
silently.** Zero/constant/symmetric seeds make whole computations vanish (x*0,
a-a, identity) so bugs in them are invisible. ALWAYS negative-control EVERY
observable path before trusting EQUIVALENT; when a control does NOT fire, suspect
the SEED world first (are inputs actually non-degenerate?), not the oracle's
reach. Here the entire "wall" was `in (0,4,8)` vs `< 0xc`. Re-triage other
"oracle can't reach it" parks with this lens. See [oracle-runs-orchestrators](oracle-runs-orchestrators.md),
[brtex3dexpand-t3-2026-09-16](../functions/brtex3dexpand-t3-2026-09-16.md), feedback-outcomes-not-excuses,
[walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md).

--- superseded 2026-09-16/09-19 notes below (kept for the diagnosis detail) ---

**2026-09-19 T4-side diagnosis (hand pass from asm, sharpens the wall):** the
−48-insn / −196-B gap is NOT missing code. It is that our build emits only **2**
`FUN_1000dc00`/CLIPTRI call sites where the orig emits **5** (source has 5 textual
CLIPTRI: b1 sites 417/428/440/442 + bf 462). Our two singleton sites both
`jmp` into a SHARED push+call tail (the compiler shares the final arg-push+call);
orig keeps all 5 distinct. **Proved MSVC5 /O2 does NOT cross-jump/tail-merge in
general** - orig's site-A(D539) and site-B(D5E0) blocks are byte-identical (differ
only by one trailing `jmp D711` vs fall-through) yet orig kept them SEPARATE. So the
5→2 collapse is driven by the UPSTREAM register-allocation state entering the walk
loop, not by a mergeable-tail source shape: same source spelling compiles to merged
(us) vs unmerged (orig). Defeating it = matching the whole walk-loop entry allocation
= the full byte-grind; no bounded C-level lever found. **Frame lever CONFIRMED:**
hoisting `pObj+0xb0`/`pObj+0xab` into locals (beside pObjBase) makes VC5 cache them
in stack slots and the frame flips `sub esp,0x60`→`0x68` (matches orig exactly),
FIRSTDIV +0x2→+0x1e, bytes −200→−188 - BUT it forces those pointers to be computed
every pass (orig computes them LAZILY on first clip need, a CSE artifact) so it is
LESS faithful and was NOT kept. The faithful source is the direct `pObj+0xb0` macro
already in the tree. Progressive `shr8;shr8` vs our `shr0x10` (third tri-index) is a
separate small class; orig extracts the 3 corner indices by progressive shift, we use
`>>16`. Bottom line: T4 here is an upstream-alloc/layout wall on a 1133-insn giant,
independent of the T3/oracle wall below.

** BrObjDlBuild 0x1000CBA0 (4180 B, largest ex-T2) is NOT A5-certifiable T3;
it stays a T4 byte-grind target** (as its own br_objdl.c header already concluded).
The existing decomp is a clean, faithful hand transcription -- the blocker is the
ORACLE cannot prove the geometry half with teeth, not that the code is wrong.

**Why (real limitation, don't retry the same way):** the function's observable
output is (a) an INTEGER display-list command buffer and (b) transformed vertices
+ clipped triangles. Two independent problems:
1. Integer command output was FALSE-EQUIVALENT because `_classify_diff` reads
   every differing dword as a float and masks a 1-integer command difference as
   1-ULP rounding (0xbb001001 vs 0xbb001002 ARE 1 ULP apart as floats). FIXED
   generally -> see the lever below.
2. The clip/transform half delegates to the sprite-matrix pipeline (memcpy +
   FUN_10034af0 + k-normalise) and the FUN_1000dc00 CLIPPER subsystem
   (-> FUN_1000dec0, own float buffers ~0x102e0Cxx). A vertex-transform
   coefficient swap (m00<->m20) or a clip-flag/corner-index bug (TRIOUT `>>8`)
   produces NO observable diff because you cannot place the transformed coords
   astride the 0/1024 clip thresholds from the input seeds -- the transform is
   opaque, so TRIOUT's drop/keep decision never flips on a bug. Verified: forcing
   pVtx[3]=vx or a command word IS caught (path reached, output compared); the
   subtle swaps are not, across 16 varied-geometry seeds, even with the clipper's
   output region marked exact. Full teeth would need OUTM ~= identity (crack the
   pipeline) or the T4 grind. COPY2 copies command WORDS, not clip flags, so a
   clip bug is only visible when it flips TRIOUT -- another reason flag bugs hide.

** REUSABLE LEVER (committed 927982ff): Profile `exact_regions=[(lo,hi),...]`.**
Dwords in those ranges are compared EXACTLY by the A5 oracle, never float-masked.
REQUIRED for any function whose output is integer command/data buffers (DL
builders, packet writers). Mark the integer arenas + counters exact; leave FLOAT
arenas (vertex/matrix) tolerant. Wired in tools/t3b_verify.py `_classify_diff` +
`verify_img`; field on oracle_profiles.Profile. No regressions (certified 0 DIFF).

** ALWAYS negative-control a new profile** ([brtex3dexpand-t3-2026-09-16](../functions/brtex3dexpand-t3-2026-09-16.md)):
inject a bug in EACH observable path, confirm DIFF, BEFORE trusting EQUIVALENT.
Here the controls exposed both the float-masking (bug 1) and the clipper teeth
gap (bug 2) -- an unchecked EQUIVALENT would have been a false cert.

**Emulator note:** one-operand `mul`/`imul` (edx:eax) was already added for
[brtex3dexpand-t3-2026-09-16](../functions/brtex3dexpand-t3-2026-09-16.md); BrObjDlBuild also needs it (the /255-style and
64-bit ops). trace_calls entries are TUPLES ('0xADDR','d'/'i','arg') -- str them.

See [oracle-runs-orchestrators](oracle-runs-orchestrators.md), [t3-certified-standard](../rules/t3-certified-standard.md) (T3 = PROVEN
same-in/same-out; a path without teeth is not certified), [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md).
