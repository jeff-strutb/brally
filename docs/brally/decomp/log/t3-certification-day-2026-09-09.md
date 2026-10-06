# T3 certification day

*Recorded 2026-09-09.*

> 2026-09-09 session - 19 T3 certifications + 1 T4 in one day; the READY-row harvest, the position sweep that cracked BrVec3Dot, return-this on a thiscall Init, and the hand-pass ledger workflow

**2026-09-09: the project lead asked for 20 "contract-valid" (T3) functions in a day. Delivered 19 T3 + 1 T4 (BrVec3Dot).**

- `tools/brally/t3.py --qualify --all` listed 17 rows already passing gates 0+A+B and only lacking the tag (16 untagged); tagging them was 16 commits in ~10 minutes. **Check for READY rows before doing any probing** - the crank daemon leaves certifiable rows behind.
- Scratch helper (recreate if needed): run `--qualify VA`, replace the `<...>` placeholder lines of the emitted tag with residue prose, insert directly above the FIRST `@implements` line among `t3.twins(va)` (tags are often the d3d VA), validate with bare `tools/brally/t3.py`, commit with a pathspec.
- Hand Gate-B passes: `tools/brally/fnmatch/fn.py VA --make TAG` then edit `build/brally/win32/match/t3d/fn_VA_TAG.c` and `--var TAG`; ~2 s each. A ledger line per pass of ≥10 probes at the CURRENT numbers (`bytes`=recomp bytes, `insns`=recomp insns, `regions`, `rows`=miss+extra), `census yes` when a corpus query / N64 read / mechanism experiment (e.g. position sweep) was part of it.
- **Position sweep through every slot of the TU** (not just start/end) took 0x10034310 BrVec3Dot to byte-exact at 3 of 23 slots after ~60 spelling probes were inert; two of those slots regressed a neighbour (BrVec3Add) - re-read every row of the TU. Inert on the other three that day.
- **`return this` on a thiscall Init** explains a leading `mov eax,ecx` + all stores through eax (0x1006CDA0). Idiom entries at the tail of docs/brally/VC5-IDIOMS.md.
- zsh: `echo ====` fails (`=` expansion) and `echo "\n"` interprets escapes - use heredocs with quoted EOF for probe edits.

**Why:** the fastest path to the project lead's daily T3 target is harvesting READY rows first, then hand passes with a cheap position sweep before any spelling grind.
**How to apply:** at session start run `t3.py --qualify --all`; tag READY rows; for gate-B-only rows sweep position early. See [file-position-regalloc-lever](../levers/file-position-regalloc-lever.md), [t3-certified-standard](../rules/t3-certified-standard.md), [t1-intake-lane-method](../triage/t1-intake-lane-method.md).

**Session 2 same day (asked for 20 more): +9 T3 + 6 T4 hand-worked; tree hit 38 T3 / 1,149 T4.** New levers minted (tail of docs/brally/VC5-IDIOMS.md): (1) an inline helper RETURNING the updated global costs the accumulator byte - spell the cycle on the global (4 cyclers T4 from one corpus hit); (2) re-read a global in its own increment store and VC5's CSE emits the original's `mov eax,ecx` copy (BrReplayAdvance T4); (3) an explicit `(float)` cast on a product pins the rounding store (BrMat4Perspective7 T4). Count-first declaration + destructive bias keeps a param out of eax (BrKeyTableFind).
**OPEN CHORE:** fileaudit FAILs at 16 assigned-not-moved vs baseline 11 - the 4 cyclers (slice2_25.c → menus) and BrReplayAdvance (slice3_42.c → racing) need hand refiles with both-file sweeps.  They matched partly BY FILE POSITION - sweep both files and re-read every row after each move, expect regressions.
**Collision:** another session certified 0x10003430/0x10002460 in parallel despite my claim_lane lock; merge ledgers by renumbering passes, never delete theirs.
