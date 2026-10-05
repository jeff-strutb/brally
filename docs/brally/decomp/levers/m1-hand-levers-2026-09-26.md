# M1 hand levers

*Recorded 2026-09-26.*

> 2026-09-26 M1 hand-transcription session - four "walls" broken by cheap levers (float local, decl order, zero-in-register polarity, /Od local NAMES) + the parallel sweep harness

M1 session 2026-09-26 (token 214e4f54, smallest-first; peers go largest-first). Old DEAD-probe essays were wrong; each fell to a lever found by sweeping hundreds of variants in parallel:

- **0x1006DD20 BrMat3Mul**: `float s = a0*b0 + a1*b1 + a2*b2; out = s;`. The float LOCAL (not term order) moves VC5's strength-reduction anchor.
- **0x10029D70 BrMat4Mul**: declare `int i, j;` BEFORE `BrMat4 tmp;`. The symbol-table order fixes the SR anchor and the load order; natural 0..3 terms. Refiled to geometry/br_mat.c.
- **0x10029710 BrGbiTexScanOtherModeL**: VC5 tail-merges identical exit blocks. Hold the zero in a local, write the last test in positive form, and put the zero store last (`mov [g],ecx` differs from `mov [g],0`), so the merge is prevented.
- **0x1002A957 BrFloat12MaxAbs (/Od)**: slots follow local NAMES (hash), and an inner-block local always gets the deepest slot. Put ~200 renamed copies in ONE TU per compile (~300k sets in 2 min). Refiled to br_mat.c.

Later the same day (11 T4 total):
- **0x10019040 BrF3DListFixup**: TU SYMBOL-TABLE SIZE sets VC5 tie-breaks. Adding `#include <windows.h>` fixed it. Test by prepending 100..5000 dummy externs (`triage.py VA`). The same lever took **0x10024490 BrTexResample** from 97 to 13 (`<stdio.h>`); it then matched by stepping pDst before tx.
- **0x100642F0 BrRbVelAtBodyPointXY**: really /O2, not the graded O2p. The sums go into p (not r); vel.z is a float local.
- **0x10022AC0 br_dl_light_vertex, 0x1002A590 BrMat4RotateAxis**: the tree only had PORT-shaped bodies (helpers, BrDl*). Transcribing fresh from the asm matched quickly. Look for rows with no matching arm first.
- **0x100194C0 BrWndProc**: `case 6: ...; break;` to the shared DefWindowProcA. VC5 tail-duplicates the call AFTER constant propagation, so the copy pushes the register (vs `push 6`).
- **0x10059350 (C++)**: a linker-folded twin callee breaks the tail-merge.
- **0x1003A2B0 BrMenuTime0D70** is C++ (like its neighbour 0x1003A420): 71 -> 8 B. A5 EQUIVALENT (50_championship). Its provisional @t3 tag stays UNCOMMITTED until A7 plus a full-suite t3live run.
- Tools: `x87sym.py VA|bin` (symbolic x87 stack, prints each store's expression; compare orig vs ours), `declclimb.py` (hill-climbs the declaration order).
- The T3 gate A3 flags semantic gaps: 0x10072680 `push 4` vs `push A` (the BrFfb struct is a port gathering; addresses come via reloc_overrides).

Session 2 (same day):
- **0x10071710 BrInputIsDown**: a hand-nested if-tree was wrong. The fix was a plain `switch` (VC5 builds the cmp/jg/je tree itself) plus the sibling's `(*(u16*)(b+2) & 0xFF00) == 0` alternates (folds to `test byte [b+3],0xff`). Copy a matched SIBLING's spelling first.
- **0x10067C30 BrCarPhysAdvance** (150 → 0): the "x87 operand-role TU-state wall" was SPELLING. Pads were inert at every count, so run the pad diagnostic before believing TU state. Levers:
  - (a) Read the field into a float temp (`h = field; x = 1.0f / h;` / `m -= h;`). VC5 then does `fld field; fdivr/fsubr other`. `volatile` does it too but mis-schedules.
  - (b) A literal constant vs an extern changes fcomp scheduling (0.5f literal = orig).
  - (c) Named dx/dy/dz locals spill to separate slots. Writing the differences inline (CSE temps) shares one slot.
- **0x100311C0 BrTrackLoad** (194 → 0): Ghidra's `do { base + iVar8 ...; iVar8 += 0x54; } while` was the wall. A typed 0x54-byte record array walked by a plain `for (i..) rec[i].f` loop fixed the scheduling, the add-vs-lea, and the fld roles in one move.  For any generated/ Ghidra-shaped row: re-type the byte offsets as records and write the source loop before sweeping spellings.
- **0x100550E0 BrUiListPaint (C++)**: TU header include (symbol-table size) plus an if/else instead of a `?:` call argument.
- **0x10039990 BrCtrlBindPoll (C++)**: constant 1 cached in edi = every case `break`s to ONE `return 1` (VC5 tail-dups the return). A return per case never caches. A pointer loop vs an absolute bound: `(int)p < (int)(&g + 4)` for `jl`.
- **0x10034010 BrSpanBuildHull**: scalar inits BEFORE the fill loops (after them, 0x3F is pooled across calls). The row was graded O2y but the original uses ebp as a GP register, so it's /O2. Check `push ebp` without `mov ebp,esp` when the grade says O2y.
- **0x10033D30 BrSpanAddLineG / 0x1002A200 LightDirs**: port-shaped bodies transcribed fresh. A separate loop counter moves the fild store. A 0x30 frame with t at +0x20 = ONE struct local {v[4 doubles], t, u}.
- Parked with levers noted: 0x10034B70 MtxInvert (fresh arm, size-exact in br_mat.c after BrMtxMul, 295: product operand roles), 0x10023110 NoZ (x87 row order, pads/co-file inert), 0x10067470 PlaneResolve (x87 join).
- **0x10007AA0 BrFixDecodeRecord** (501 → 0): the caller pushes `mov ax,[..]; push eax` (no widening), so the callee's real param is short/char. Don't retype the shared callees; call through cast function types `((float (*)(short))F)(x)`. `?:` loads a float constant via the x87; if/else moves it through an integer register.
- **0x1002A200 LightDirs → T3**: t3.py A3 now pairs `fimul m` with `fild m; fmulp` (exact). Oracle needs static-CRT helpers in t3b_env `_CRT_HELPER_VA` (added CIasin 0x10074606) and hand reloc rows for pooled `$T` double constants. Refiling a @t3 out of a batch: byte-compare objects; the batch's extern DECLARATIONS are load-bearing (symbol table), so carry them; defs become externs.
- **0x1005ACE0 WheelSteer** (603 → 50): the Ghidra draft passed the wrong matrix (a behaviour bug). Retranscribe after the callees match.
-  The sweep masks reloc targets. The image gate caught port-gathered struct fields (g_brCrPlane.out really lives at 0x117781A0). Fix with a matching-arm-local extern plus a `config/globals_hand.csv` row; no header edit.

Harness: `build/match/t3d/m1/sc.py` (score one file) + `sweep.py GEN.py` (a generator module yields (tag, src), 12-way parallel). A standalone minimal TU usually reproduces the residue, so check that first, then sweep there.

Parked for the batched T3 pass: 0x10039D20 BrMenuCap07E0 (41 B, reggap 0, A4 lost-sync fail). Per-arm duplicated e3 fixes the head and tail (46 B, +3 B) but not the arms.

Related: [declaration-order-tiebreak](declaration-order-tiebreak.md), [x87-wall-mechanism-2026-09-13](x87-wall-mechanism-2026-09-13.md), [dpappmsg-callmerge-zeroweb-2026-09-25](dpappmsg-callmerge-zeroweb-2026-09-25.md).
