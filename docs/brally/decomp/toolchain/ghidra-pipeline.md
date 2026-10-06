# Ghidra pipeline

*Recorded 2026-08-26.*

> Ghidra auto-decomp pipeline - tree 474/1047 tagged match; sigaudit.py epilogue audit produced +18 matches in one pass; BrTex3dRegister parked at allocator frontier; resume by working the remaining flagged audit rows.

## Ghidra auto-decomp pipeline (updated 2026-08-23 late)

Ghidra 12.1 via brew, OpenJDK 21. Project at `/private/tmp/ghidra_brglide/BRGlide_proj`.
Decompiled corpus: `build/brally/analysis/ghidra_decomp/` (2139 .c files, one per glide VA).

### Engine: tools/brally/ghidra_to_match.py
Clean Ghidra C → wrap → compile (Wine+MSVC5, /O2 and /Od) → diff vs
`build/brally/win32/match/orig/<va>.bin`. Results merge into `build/brally/analysis/ghidra_learnings.csv`.
The junk classes in CLOSE (1B fragments, 5/6B thunks, 11B EH funclets  - 
DIFF(10)×479, DIFF(6)×91 buckets) are NOT veins; unreachable from C.

### State (all committed, tree clean at 8183f98)
- Tree: **474/1047 tagged match (45.3%)** per report.csv - query it, not this line.
- **0x10028BB0 BrTex3dRegister (1755B): CONFIRMED ALLOCATOR WALL - do NOT
  re-attempt.** A 2026-08-26 pass (33 min) proved the orig base=edx vs ours
  esi is first-region graph-coloring of a 7-live-value web, NOT a
  live-across-call choice; no C-level lever moves it. Ruled out: decl
  rename, late r.iLevel store, clamp-via-global, hand pTile, __asm nop,
  volatile post-call loads. Landed orig-shaped source (h=1 web unify,
  first-region global re-deref, 1223->1219; other 8 fns in file still
  match). No N64 twin (no debug string; ERROR str = log2 helper
  0x10027290). Three idioms in VC5-IDIOMS.md. Only revisit with a genuinely
  NEW first-region variable insight - not another split of this web.
- 0x1002CB49 still parked (allocator-input mystery, wip file exists).

### NEW IDIOMS proven 2026-08-23 (in VC5-IDIOMS.md - read them):
1. **Redundant mid-return blocks shrink-wrap** - a bytes-level second
   epilogue is VC5 return-duplication, NOT a source return; writing the
   redundant `return` forces prologue pushes and rotates ALL allocation.
2. **Stack slots per-variable, register webs identity-blind** - merge
   locals that Ghidra reuses one iVar for (shrinks frame to match);
   decl order/renames NEVER change /O2 output (60-variant proof).
3. Guard-if + for (jle backedge + jmp) vs do-while+break (tail-dup latch).
4. Cross-jump requires branch-symmetric temps (w2/h2 pattern, hoisted init).

### Method that worked (reuse it)
- `structdiff.py` + `gdiff.py` in the session scratch (recreate: seq-align
  capstone disasm normalized over regs/addrs; side-by-side dump).
- **Return-insertion bisection**: insert `return 0;` at successive statements,
  compile each, watch frame size / first-push offset / key cmp registers.
  This is what found the mid-return root cause. Script shape in notes file.

### Bulk idiom application (2026-08-23 night): +18 matches in one pass
tools/brally/sigaudit.py compares frame/epilogue/push signatures of every diff row
against orig bytes; structural mismatches on small-diff rows are idiom-fixable.
Worked: the 6-member DirectPlay send family (tags 2-7, ||-merged return-0
early-outs, inlined BrComCallLocked68 call), BrStrGet, BrRaceClockReset
(tail-jmp), BrGameStepIs (/Od plain equality), BrScratchRingNull/Drain,
BrHookTakeA/B, BrDlCmdFogColour (arg order swapped by port), the two C++
deleting dtors (thiscall twins + vtable-slot casts in slice8_86),
BrS17BankFlip (tag moved to body; memset triple), BrRenderCountersReset.
match_sweep now has an O2y (/O2 /Oy-) third variant.
PARKED: BrExt_10079550 twin (inverse zero-reg knot, 71 structural-clean
diffs); BrUiOptHook_100436B0 at 12 (tail reg rotation); BrBitEdgeSplit 13
(and-orientation); BrFadeTick 4 (esi/edi pair). EH-funclet rows (orig starts
`push -1`) are C++ walls - sigaudit flags them, skip.
The audit list still has ~340 unworked flagged rows (most >25 diffs).

### Pile triage (2026-08-24, build/pipeline_triage.csv - regenerate, do not trust prose)
The 1,095 unmatched pipeline rows classify as: 363 plain-int CANDIDATES
(116.8KB - the real target, four 2-4KB heads), 53 float/x87 (43.2KB, wall),
26 compile-errors (38.7KB, tooling), 14 C++ EH frames (16.9KB, .cpp
workstream), 584 tiny-junk + 55 thunks (6.6KB, link-stage). Only 7 of the
363 candidates are <=5 diffs - the rest need structural rewrites, i.e. the
idiom playbook, not reruns. O2y-variant rerun produced 0 (gains exhausted
until mid-return/wrapper-split transforms are coded into the engine).

### Wide-batch loop (2026-08-25, committed f2421d7 + min-size commit)
The cadence is now: machine batch → hand-solve one rep per failure class →
mint a generator → re-batch. NEVER hand-match what a generator could sweep.
- **Wide run:** `python3 tools/brally/ghidra_to_match.py --refine --max-diffs 200
  --min-size 16` (~248 rows, hours, zero tokens; crash-safe - learnings CSV
  written back after every function, biggest-first). First run launched
  2026-08-25; log at `build/refine_wide.log`.
- **Auto-filer:** `python3 tools/brally/autofile.py` files each MATCH into the
  bracketing slice, verifies via single-file sweep (no regressions allowed),
  commits per function; refusals/flags → `build/autofile_log.csv`. Proven
  end-to-end on 0x10010F80 (reproduced the hand-filed content exactly).
- **Residue triage:** `python3 tools/brally/ghidra_to_match.py --residue` groups
  unmatched rows by divergence class (scattered = hill-climbable, needs a
  generator; frame/short/dense = structural, hand-solve one rep).
- Generators live in `_refine_candidates`; every hand-proven idiom must
  become one.
- **Structural-class residue (short/dense/frame - NOT scattered/regalloc):
  consult the N64 twin first.** Top Gear Rally (same source lineage, IDO
  -O2, near-transparent MIPS) pairs functions to BRGlide via 195 shared
  debug strings; resolves unknown structure/inlined helpers/field widths
  in minutes. Proven on BrVarSave; does NOT move register-allocation
  walls. Full recipe: [tgr-n64-viability](../../../tgrally/decomp/tgr-n64-viability.md).

### Next steps
1. **Apply idioms 1+2 to the walled near-misses** (BrPhaseLeave 13 diffs,
   BrFixUnpackS6Q7Neg 2 diffs, the sub-vs-add and edx/edi-pair walls,
   report.csv rows with small diffs): re-check each for redundant
   mid-returns and mergeable locals before accepting the wall.
2. BrTex3dRegister rotation: next probes = early-block store pairing seed,
   or hidden base/max variable merges. Do NOT permute decls (proven no-op).
3. BRally.exe launcher (3.5KB) - untouched second binary in scope.
4. FAR pile by recomp_size/orig_size; float/x87 stays walled.

**Why:** [goal-coverage-not-playability](../rules/goal-coverage-not-playability.md), no-token-thrashing
**How to apply:** Resume with "continue ghidra decomp work".
