# Hand transcribe batch levers

*Recorded 2026-09-24.*

> 2026-09-24 smallest-first hand-transcription batch : new VC5 source levers proven byte-exact, plus the residue classes that did NOT move.

Batch session 2026-09-24 (the 71 smallest open T1/T2 rows). Levers PROVEN byte-exact:

- **Named copy of ONE operand steers fld/fadd roles + param-register order** (BrVec3Add/Midpoint): `{float ax = pA->x; out->x = ax + pB->x;}` per component, brute-force n/a/b per component. Operand order alone is canonicalised.
- **TU position re-sweep** after a neighbour changes (BrVec3Dot after Cross; BrCrPlaneDist before ImpulseSolve). Write a slot sweep (move fn to every `}` slot, score all rows).
- **Struct-pointer PARAMETER walk** anchors the IV at +0 (BrVtxSwap); a local cast from void* does not. Header clash → `#define Name Name_hdr` / `#undef`.
- **u16 VALUE swap `(u8)(s>>8) | ((u8)s<<8)`** loads bytes lo-first; byte pack `(b0<<8)|b1` loads hi-first.
- **Loose DAT_ globals + car record indexed INLINE per statement** (`DAT_10af1208 + i*0x2B68 + off`) - a `car` local reorders IV bumps (BrCarStateSave/Restore). Struct-array form `&((Rec*)DAT)[n].b[off]` folds base into displacements (BrCarTableAdd).
- **Named RHS temp** flips byte-load order of an index pair (BrCarStateRestore).
- **thiscall-with-stack-arg from C**: `__fastcall f(void *this, struct{int n} arg)` (ecx=this, edx untouched).
- **Loop-local pointer copy `char *buf = pBuf;`** makes the loaded pointer the SIB base (BrBitStreamReadBits, C++ lane). Does NOT fix member-array `this+const` bases (0x100540D0/0x10054280 still 1 B).
- **memset for small clears** keeps the zero local (no esi zero-CSE) (BrInputPollPressed, BrDPlayStartup, BrStrResLoad).
- **Unused first parameter**: check `[esp+N]` of the first read (BrDPlayStartup reads arg 2).
- **Forward-goto shared failure path** lays out fail-after-success (BrDPlayStartup); **nested success block** does it for BrSfxChanStart.
- **`(unsigned char)(w0>>16)` cast** vs `& 0xFF` keeps w0 in a register (BrGbiMoveMem).
- **Inline `__asm fld t / fistp g`** explains EBP frame + memory temps in /Od-looking bodies; `/O2 /Op` + dword copy `*(int*)&dst = *(int*)&w` breaks float CSE (br_dl_project, own file br_dlproject.c).
- **goto loop** escapes VC5 induction-variable strength reduction (BrF3DListFixup: 74 -> 0 regnorm). Does NOT stop loop rotation.
- **Loop null test that IS the inner walk's guard** (`while (p && flag) p = p->sib;`, no separate early return) stops loop rotation (BrRacePathAdvance).
- **Block-scoped float temps x,y,z + field-wise vector copy** fix the x87 cross-product ladder (BrRbVelAtPoint 5+22 -> 1 byte spill-slot residue).
- **if/else two calls with the `<=` arm first** → VC5 tail-merges into `push -1/jmp/push 1/call` (ternary gives setcc) (BrRaceDriverPost, C++ lane).

- ** POOLED CONSTANTS ARE LITERALS** (BrCarSub9020 REGNORM 13→1; BrWeatherStepWind T4): when the original's float constants sit in a per-function `.rdata` run (0x10077000 - 0x1007ADF6, first-use order, duplicates shared), write them as `1.0f`-style literals, NOT `extern float DAT_1007xxxx[]` / `static const kF…`. VC5 ranks a literal (`$T####` symbol) below a memory field, so `fld [field]; fmul [const]` comes out right, and `x = acc; if (acc > 0.25f)` gives fcom-then-fstp. The old "operand-kind wall" verdicts were this. Scan: an obj reloc whose original target is in .rdata and whose symbol is not `$T`/`__real`/`??_C`. Caveat: the T3 image build can't resolve `$T` (hit on BrCarPhysSpring), so literals are fine for T4 and need the resolver for T3.
- **Two block-scoped `volatile float` temps** reproduce "round both trig results through the one frame slot" (`fst [esp]` twice, no reload) (BrWeatherStepWind); both in one block, or one alone, does not.
- **ext-permutation "EXACT" can be a false match**: the sweep masks relocs, so a store-order permutation can swap WHICH globals are written. Always check reloc symbols against independently-corroborated addresses (BrExt_1005FBC0: reverted). A sweep of a false match also teaches globals_learned.csv wrong addresses - re-sweep after reverting.

Residue classes that did NOT move (T3 candidates): VC5 cross-jump tail merging where the original kept identical blocks (0x10029710, 0x10059350); IV anchor (BrMat3Mul); member-array SIB order; spill-slot choice; reassociation `(base - i*K) + off` (0x10060A30).

Filing: new rows via a private-index commit (never sweep other sessions' uncommitted filing.csv lines). Slice-file matches recorded with empty module (undecided queue) to avoid new STRANDED rows. See [t1-intake-lane-method](../triage/t1-intake-lane-method.md), [tyre-t4-volatile-parens-tu-2026-09-24](tyre-t4-volatile-parens-tu-2026-09-24.md).
