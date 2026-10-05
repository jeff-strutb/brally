# Scenevis a5 lever

*Recorded 2026-09-20.*

> 2026-09-20 session - BrSceneVisPrepare 0x1000E320 + BrModelSwap 0x100302A0 certified T3; A5 oracle infra levers (floor/asin, Glide stdcall-import resolve+model, memory adc/sbb, indirect-call target, non-degenerate + model-graph seeding)

2026-09-20: **BrSceneVisPrepare 0x1000E320 (1992 B, the largest un-tiered C
function) certified T3** (commits: A5 infra `2406eb1d`, ledger + tag, reproducibility
`a727290e`). It was already 543/543-insn byte-identical; T4 is a real
canonicalization wall (63 B, 2 regions: region 1 = address-taken pt.x/pt.y load
schedule, region 2 = commutative 16-bit int-add operand order). The 2026-09-15
"NOT certifiable, A2 wall" verdict was a **dated byte-gate verdict**
([walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md)); the live lever is A5 EQUIVALENT superseding the
byte gates ([upgrade-byteshape-t3-to-a5-proven](upgrade-byteshape-t3-to-a5-proven.md), [equivalence-oracle-in-image-2026-09-10](equivalence-oracle-in-image-2026-09-10.md)).

**Two reusable levers (both landed in tools/):**
1. **MSVCRT floor/asin now modelled in x87emu.py** (keyed by BRGlide IAT slots
   0x118F059C/0x118F0504). They return a double in st(0) (cdecl, caller-cleaned);
   with no model the callee's following `_ftol` popped an EMPTY x87 stack → the
   run "escaped oracle (IndexError: pop from empty list)". Only these two libm
   fns are IAT imports; sqrt/sin/cos are inline x87 the emu already handles.
   Reusable for any pass reaching BrLightDirsFromLookAt's angle packing.
2. **Non-degenerate seeding for projection/matrix orchestrators.** The classic
   [objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md) degenerate-world trap bit twice here: (a) a
   zero view matrix (g_BrDrawView) collapses BrMat4TransformPoint4 so the
   projection body - where the divergent regions live - is skipped; fix = let
   DATA globals fall through to tame non-zero floats, pin only POINTER globals
   null and COUNT globals small. (b) pt.x/pt.y are copied bit-for-bit as ints
   but consumed as FLOATS downstream - seeding them as small ints makes them ~0
   denormals; seed as tame FLOAT BITS. **Always prove teeth with per-region
   negative controls** (swap pt.x/pt.y → DIFF; edge-sum .x→.y → DIFF) before
   trusting EQUIVALENT.

**Gotchas hit:** g_brRaceNDriver/g_BrCarCount are initialized `.data` (is_bss
False) → the bss hook is IGNORED for them (real values 1/2 read; harmless small
here, but a landmine if a count needed pinning). Driver structs placed in the
pRace ARG buffer (buf hook, deterministic base HEAP_BASE=0x300000, stride 0x400)
so both regions execute and their writes are compared. Grid callees
(BrGrid16Pair/BrU16QueuePop) walk uninitialized static grid tables → runaway;
stubbed (that span region is already byte-exact, so stubbing is sound).

**Shared-tree race:** crank's `git commit -- file` swept my mid-iteration
oracle_profiles.py (seeds=12, degenerate) into HEAD under another commit
([shared-tree-partial-commit](../traps/shared-tree-partial-commit.md), [parallel-session-clobber](../traps/parallel-session-clobber.md)); a parallel
session (author jeff@strutb.com) then committed the @t3 tag but DROPPED the 2nd
@t4-pass line its own message claimed - left the cert non-reproducible until
`a727290e`. **Always re-run `t3.py --qualify` on the committed HEAD state to
confirm a cert reproduces; a committed @t3 tag can be self-inconsistent.**

## Second batch (same session) - BrModelSwap 0x100302A0 (1062 B) CERTIFIED T3 + more infra

More reusable A5-oracle infra landed (commits 942d56b7, b08d6490, b8641ace, 87cc4909):
1. **Directly-called stdcall imports** (e.g. glide2x `grTexCalcMemRequired`) appear
   in the obj as a reloc to the decorated thunk `_Name@N`; the resolver only tried
   `__imp__`-prefixed/split forms → UNCLASSIFIED. Fix: `imports.get('__imp__'+n)`
   fallback in t3b_env, and model the import in x87emu (stdcall, args at [esp..],
   result in eax, esp+=argbytes) as a deterministic fn of its args.
2. **Memory-operand adc/sbb**: they used register-only rd_reg/wr_reg unlike
   add/sub's memory-capable `_rd/_wr/_val`; a callee doing carry-add on `[ebx]`
   escaped. Now byte-width + memory-capable like add/sub.
3. **Indirect-call target recording**: a black-boxed icall recorded the SLOT
   address for `call [mem]` but the pointer VALUE for `call reg` → the same null
   callback compiled two ways gave a false DIFF (0x118ED1DC vs 0). Now records the
   dispatch TARGET uniformly. Fixes a whole class of false DIFFs.
4. **Serialized-object-graph seeding** (BrModelSwap): reverse the relocation
   scheme first - BrSegPtrFixup 0x100189E0 is `*p = BASE + (offset - LO)` with
   BASE=0x104B16E0, LO=0x104B16E4 (both seedable). Seed LO=0, BASE=arg-buffer
   base, then lay a minimal well-formed graph in the arg buffer with big-endian
   counts (BrRdBe32 converts in place) and big-endian offset "pointers". Put
   per-seed non-zero data in the byte-swapped fields so swaps are OBSERVABLE, and
   prove teeth with a per-field negative control.

**Symbol-mapping gotcha**: placeholder callee/global names in a transcription
(BrModelFixupDirect, BrModelVtxResolve, g_BrGfxSubmitB) block the oracle with "no
known address"; find the real VAs from the original's call sites and annotate
`Name(args); /* 0xVA */` (functions) or `extern T Name; /* 0xVA */` (data) - the
resolver reads these even inside comments. Fn-pointer globals don't match the
data regex, so use a paren-free comment hint.

 **crank auto-cert REVERTS a valid @t3 tag** when `tools/t3.py` (whole-tree
validator) trips on unrelated pre-existing bad tags (parallel churn) - it commits
the @t4-pass ledger line but not the tag. Add the tag by hand from
`t3.py --qualify` output and commit it yourself; re-`--qualify` the VA to confirm.

**BrTex3dRegister 0x10028BB0 (1755 B, the largest) has a REAL behavioral bug** the
oracle now catches (DIFF: recomp changes r.w/r.h in the mip-reduction block on
some inputs when the original doesn't; recomp is 3 insns short). Honest T2, handed
off (task_e17f5dd2). NOT an oracle lever - a source fix.
