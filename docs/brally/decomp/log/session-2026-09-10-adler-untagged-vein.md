# Session 2026 09 10 adler untagged vein

*Recorded 2026-09-10.*

> 2026-09-10 : +1 byte-exact (BrAdler32), +1 T3 (BrFramePresent 98->99), +1 T2-EQUIVALENT parked; the rep-stosd/adler DO16 idioms; and the hard truth that most 'documented-but-untagged' bodies are untagged BECAUSE they are inline/switch-fusion walls.

Session banked THREE, and the ceiling was exactly what [t3-frontier-map-2026-09-10b](t3-frontier-map-2026-09-10b.md)
and the index already said: the readily-certifiable pool is spent.

**Landed (all committed):**
- **0x10023B70 BrFramePresent** -> T3 CERTIFIED (98->99). Frame-present +
  FPS-B ring. The ONLY block was a spelling: the seed loop must be an indexed
  `for (i=0;i<count;i++) arr[i]=v;` to lower to `rep stosd` -- a `do{*p++=v;}
  while(--n)` pointer walk emits a manual `dec/jne` loop. Residue after that is
  register-only (edi/ecx vs edx/esi in the timestamp tail). Certified via two
  `crank.py 0x10023B70 --budget 40` passes (Pool A, single VA is sanctioned).
- **0x10001000 BrAdler32** -> BYTE-EXACT (986->987 match), filed to a NEW
  module `src/brally/core/gamedata/br_adler.c` (was an untagged body in the slice1_01
  batch; the hook refuses a new match in a batch, so refile). It IS zlib
  adler32. Three spelling facts made it byte-exact: DO16 as sixteen INDEXED
  reads `pBuf[0..15]` with a single `pBuf += 16` (post-increment `*p++` emits
  inc/[R] per byte); gate the block on `(int)k >= 16` (NOT `> 15` -- `>15`
  gives `cmp 15;jle`, `>=16` gives `cmp 16;jl` which is what the original has);
  count the unrolled loop DOWN (`count=k>>4; do{...}while(--count)`).
- **0x10060A30 BrRaceSaveLastLapInfo** -> T2 EQUIVALENT, parked in
  br_objlife.c. Insn-exact (80/80), oracle EQUIVALENT, gates 0/A1/A2/A4/A5 all
  pass; only A3 fails on 2 unpaired rows = the literal-pooling wall (orig
  materialises the save base 0x102066C8 with `mov reg,imm` and accumulates onto
  it; VC5 folds mine as `add reg,imm`). Base used only twice -> too few to pool.
  Two levers that DID move it to 2+2: inline `*(int*)(p+0x140)` at every use
  (do NOT cache in a local -- orig reloads it 3x, caching drops 2 insns);
  advance the record pointer at the TOP of the loop and read `p[-0xada]` (orig
  pre-advances and reads `[ecx-0x2b68]`).

** The 'documented-but-untagged' vein is mostly fool's gold.** A scan for
src bodies with a `0x........` header but no `@implements` (and not in
report.csv) found ~35. BrAdler32 was the ONE clean win. The others are untagged
BECAUSE they are hard matches: `BrCtlNameInit` (0x10058AF0, br_ctlname.c) uses
helper funcs + arithmetic where the original inlines two jump-table switches and
hoists sprintf to edi (-126 B when tagged); `BrUiText100400E0` (glide 0x10039620,
slice2_23.c) calls `BrCfgLookupIndex` where the original INLINES it 3x (-196 B).
Both reverted. The scan was not kept.

** VC5 strength-reduces an address expression with a runtime-variable
multiplicand into induction variables** (0x1005A500 BrImgTintFlipCopy: my
`(srcH-y0-row-1)*dstStride*4+base` blew up +52 insns with neg/shl/spills where
the original recomputes `imul` each row). Source can't easily stop it. Abandoned.

**The four closest single-gate FAIL rows are ALL already parked with thorough
dead-probe notes** (0x10039D20 movsx-order, 0x1003C950 hoisted-global,
0x10038A80 or-(-1), 0x10015550 neg-quotient). Do not reopen -- rule 12 +
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).

Related: [t3-frontier-map-2026-09-10b](t3-frontier-map-2026-09-10b.md), [tag-untagged-functions](../triage/tag-untagged-functions.md),
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), [crank-daemon](../toolchain/crank-daemon.md), [filing-py-drops-rows](../traps/filing-py-drops-rows.md).
