# SPEC B — callconv-class rebatch

Call-shape class: unmatched orig with FF15 stdcall, Glide E8 thunk,
COM/thiscall vtable, or `.data` funcptr. ~308 unmatched / ~223 at the
seed's count; refine only saw the 71 already in learnings with 1..200
diffs.

## Three representatives

### 0x100283C0 BrTex3dDownloadAt — Glide stdcall (FUN_ name)

Seed rewrite: `void __stdcall grTexDownloadMipMap(int, int, int, void *);`
as-is 7 → callconv 4. Size matches. Remaining 4 diffs are instruction
scheduling: orig stores `[+0xD0]/[+0x10]` then loads `[+0x48]`; recomp
hoists the load (separate `extern char` DATs, no alias). Documented
wall in `br_tex3d.c`. Downstream generators do **not** undo the stdcall
decl.

### 0x1003F940 family — thiscall vtable K>0

Seed rewrite: 0-stack site MATCHES (`__fastcall` BR_THISCALL1). 1-stack
site emits `xor edx,edx` from `__fastcall(this, 0, 1)`. Orig is
`push 1; call [eax]` with edx untouched. 23 → 14, all 14 the xor+je
cascade. VC5 C cannot spell thiscall-with-stack-args at a **call site**
(`include/br_match.h`: dummy edx is a callee idiom). Struct-stack trick
is worse (17). Dummy=vtable is 12, eax-vs-edx coloring. 13 siblings at
DIFF(14)/DIFF(18). Not C-generator-tractable.

### 0x100703D0 / 0x10017F10 — named wrap vs FUN_ insert

Work-file 0x100703D0 already has hand stdcall typedefs; transform is a
no-op on `( *(BrDi*Fn *)...)`. From-decomp wrap +1 (stosd residue).

0x10017F10 BrFadeRelease (25 B, 0-arg funcptr): wrap **as-is MATCH**.
Callconv rewrote stdcall-0 and inserted the decl BETWEEN
`int BrFadeRelease(void)` and `{` — C2444. Wrap renamed FUN_ to the
report.csv name; the FUN_/THUNK_ insert regex missed it. Same C2444 on
0x1003D4F0 / 0x100706B0.

This is why the seed produced 0 MATCH last pass: remaining learnings
rows still named FUN_ (callconv applied, other residue), and the
DIFF-tagged named functions never entered learnings (`load_report_vas`
skips every report row, not just match).

## Fixes

1. Insert extra after the last forward-decl semicolon, before the
   signature. Never append after the signature.
2. Skip 0-arg stdcall funcptr (identical at the caller; notes already
   said this).
3. Re-offer callconv as a hill-climb candidate each round (combines
   with generators). Seed stays only-if-better (`<=`, protect 0-diff;
   0x100368A0 poison).
4. Wrap unlearned call-shape DIFF-tagged VAs into ghidra_work and
   refine them. Skip climb when as-is diffs > `--max-diffs`.
5. Autofile: a DIFF tag is not "already matched". File generated/
   standalone and drop the stale `@implements` (d3d remap included).

## 0x100035E0 duplicate

`generated/0x100035E0.c` MATCH. `slice6_78.c` had
`@implements 0x10003290 d3d BrChkFClose` remapped via shared.csv to the
same Glide VA (DIFF 94). Port body kept; tag dropped. report.csv: one
MATCH row.

## Batch delta

(filled after `--refine --max-diffs 200 --min-size 16`)
