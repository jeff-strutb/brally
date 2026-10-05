# Lockstep oracle arbitration

*Recorded 2026-09-19.*

> 113/118 T3 place (81eef86d); lockstep_rows.py + reloc_overrides.csv hand lanes + A5 oracle as arbiter over placed values; two lockstep hazards (register-role swap, edge absorption) caught BY THE ORACLE; 5 blocked = structural respells

**2026-09-19 (…4a6afec6, e2ba40d9, 81eef86d), sequel to [reloc-pair-lever](reloc-pair-lever.md):**
second real-Win98 crash traced to STALE HEARSAY WRITES the per-function audit
couldn't reach (BrCdEarChannelOpen CD-track cells, BrFrameDraw B4E1E0-family,
BrSfxBankLoad BrSndPDS - one global, two addresses in one image). Fixed by
PROVENANCE GATING (4a6afec6): evidence sources (fnmap, learned, Ghidra
DAT_/FUN_, imports **IAT only for __imp__ indirect form - direct calls go to
the jmp-thunk**, content/thunk/jumptable/string identity, pairing) place
directly; hearsay (g_<HEX>/suffix names, glmap rows, /* VA */ comments)
places only where pairing CONFIRMED or CORRECTED it anywhere (pooled phase A
→ phase B), else the function blocks.

**The lane machinery (all committed):**
- `config/reloc_overrides.csv` = the per-site hand channel; `fill_function`
  AND the T3 gate consume it (values are FINAL SLOT DWORDS: REL32 rows store
  the displacement). Offsets may be 0x-hex. `BR_UNRES=1` names every
  blocking slot; `BR_DUMP_SITES=f` exports a build's final site values.
- `tools/lockstep_rows.py`: full-stream or block-wise (SequenceMatcher,
  blocks ≥4, boundary guard) positional recovery - site i = original insn
  i's operand. ~950 rows.
- **THE ARBITER: `t3b_verify.py <va>` tests the PLACED values** (fill honours
  the CSV). EQUIVALENT-with-real-addresses achieved for FadeTick, CtlAiBody,
  TextEmitString, InputPoll, EarLoad, SceneDlBuild, GlNavPoll, FfbEnum,
  FfbSpring, TexScan, RcaFixup, MenuCap0730, MenuText15A0, Sub_100173F0.

** TWO LOCKSTEP HAZARDS - never trust positional fill without the oracle:**
1. **Register-role swap:** identical shape streams can recolor esi/edi so
   values route crosswise (BrFadeTick: `sub eax,edi` vs orig `sub eax,esi`;
   six rows crossed pos/pos2 + hist tables; oracle DIFF at 0x104B16A8;
   fixed by dataflow → EQUIVALENT).
2. **Edge absorption:** a homogeneous same-shape run at a diff-block edge
   pairs one slot early (BrCarDrawVehicle byte-scramble); boundary guard
   refuses those edges; the scramble words were hand-derived from pack
   order (bits24/16/8).
Also: verify-side UNCLASSIFIED "no known address" on CSV-covered fns =
the oracle picked a DIFFERENT OBJ VARIANT whose offsets don't match the
CSV (HudDraw, FfbInit, KeyTableFind) - verify-tooling lane, not placement.

**2026-09-19b (ac4a4c8e): ALL FIVE RESPELLED - 118/118 PLACE.** Method per
lane: CtrlCfgAssign dispatch open-coded; MenuTime0C00 full inline
retranscription (A5 EQUIVALENT w/ real addresses); CarDraw's two "structural"
slots were ordinary (hoisted load + matching cmp - RE-READ before declaring
structural); the two BrExt kits went `static __inline` (+one macro where VC5's
inline budget quit) with matching-only original shapes: unchecked stores,
fall-through-into-fatal, ONE-arg 0x100378C0 err, EH-new 0x10074572, and every
env/hook/style read replaced by the original's immediate via FUN_/DAT_ names
(maps verified by use-count bijection + cross-builder recurrence + two known
glide VAs).  pattern: PORT SCAFFOLDING (env structs runtime-initialised by
port code) can never place - matching build must use the original's
immediates.  Ext-pair oracle verdict: DIFF inside the seeded allocation-
failure cascade (every new fails; divergence starts at an allocation OUTCOME
in real allocator code) - recorded honestly, not claimed EQUIVALENT; a heap-
seeding profile would settle it. x87emu gained stosw.  raced-gate note: the
parallel session's lanes (BrObjDlBuild + BrSnapInterpDraw now CERTIFIED by
them - ObjDlBuild's "not certifiable" verdict is DEAD) race the gate; EXIT=2
verdicts cover their in-flight rows, re-run when the tree is still.  wine
slowdowns: a dying wineserver costs ~40x - `tools/wine/...bin/wineserver -p`
with WINEPREFIX=build/wineprefix persists it.

**Superseded 2026-09-19b - the five WERE respelled (kept for the method):**
- BrCtrlCfgAssign 0x10062B80 + BrMenuTime0C00 0x1003A140: source calls
  helpers the original INLINES (BrCtrlProfileIndex doesn't exist in the
  image; Time0C00 passes &g_menu into helpers indexing OUR fused layout)  - 
  respell + re-certify lanes.
- BrCarDrawVehicle 0x1000A110: two slots where the transcription
  materialises an index global the original never stores (off 0x1a59
  RefIndex vs orig's `movsx [ecx+edx*2+0x100a5c78]`, off 0x1bae extra
  Suppress cmp) - transcription-shape lane.
- BrExt_10052030 / BrExt_10054B50: 24-vs-76 / 32-vs-78 call-count drift
  (inlining) - deep lanes.
**Fusion findings:** g_BrDPlay$S710 = 3 original objects (0x100ABA54 config,
0x10AC5840 ring, 0x105BC72C cell); g_menu$S445 ≥3; g_brFfb layout conflict
(0x118eeeec/f00/f0c) unresolved. Un-fusing these in source is the clean fix.
Final image audit: 113 placed, ZERO non-original slot dwords.
