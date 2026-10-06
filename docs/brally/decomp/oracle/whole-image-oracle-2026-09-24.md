# Whole image oracle

*Recorded 2026-09-24.*

> tools/brally/brbox_diff.py runs the assembled T3 image vs the original frame by frame; it found 10 real defects the per-function live oracle had passed, and how to drive it

2026-09-23/24: the per-function live oracle (t3live) is NOT sufficient. `tools/brally/brbox_diff.py` runs the whole placed T3 image (build/brally/win32/brbox/image/BRGlide.T3.dll) and reference/brally/orig/BRGlide.dll through the same brbox script, and compares every frame: each Glide call with its args (stack vertex arrays by content) plus a crc of the WHOLE data area 0x10077000..0x11900000.

Quick race (20_quickrace_drive) went from "freezes at frame 344" to IDENTICAL over all 1691 frames. Defects it found, all passed by t3live:
- BrRaceStep time-sync loop re-tested a stale cached stamp → drew forever (never returned to BrAppFrame).
- `const_slot_values` content-located `$T` float constants inside .text (instruction immediates); placed bodies overwrote them (BrRbQuatDerivative read 0.5 from NOP padding). Now searches past .text only.
- BrRbQuatDerivative row 3 reassociated to chase bytes (different sum).
- BrCarPhysDriveMatch / BrCtlInputApply (x3) / BrCrImpulseSolve: x87 rounding points (register value vs stored float slot) and one inverted sign test (`==` vs `!=`).
- lockstep_rows: width mismatch (qword literal pinned to the original's float cell), stale rows after source edits (now a gate failure in image_build_t3), a D3D-era name (g_AC300) pinned by the 0x1000 "near" window, and crossed commutative operands where a hand row fixed only one side (BrPfxUpdateB4AC dt).

Round 2 (all scripts), more classes:
- Inverted branches: BrRaceStep `je` on 0x105CCB8C read as `!= 0`; an `st == 4` guard dropped. BrCarCarCollide passed `d` to the second solve where the original passes `sd = -d`.
- A Ghidra-form `(&DAT_x)[i*0xada]` with `extern unsigned char DAT_x`: a byte compare at a byte stride (BrGhostPlaybackStep). The original compares a dword at stride 0x2B68.
- Forwarded return registers: sin used unrounded where the original stores it and reloads it (BrCarPhysDriveMatch).
- Original UB: BrFixDecodeRecord leaves record +0x1C..+0x33 unwritten and BrCarGhostApply copies that stale stack into the car. Handled by `config/brally/ub_scrub.csv`: both runs get the same zeros at function entry.
- The harness must also hash DirectPlay payloads (stack-built) and run the peer in every mode.

End of 2026-09-24: all 25 scripts IDENTICAL (network scripts with the T3 image on BOTH machines). The gate is `tools/brally/brbox_diff.py --all`; its ledger is `config/brally/whole_image.csv`. The multiplayer harness had been nondeterministic: brbox_net delivered in publish order. It is now sorted by (arrival, source, sequence) and original-vs-original is stable.

Why t3live missed them: it certifies from a handful of captures (branch paths never taken), and it cannot see image-level faults (constants, stale reloc rows).

Loop that works (per divergence, ~20 min): `brbox_diff.py S.txt` → first differing frame F → `--t3calls F-1` names the first T3 call whose writes differ → `t3live.py explain S.txt 0xVA --frame F-1` (+T3LIVE_SKIP=n, T3LIVE_HEX, T3LIVE_PROBE_O/_T with PROBE_MEM) → read orig asm vs the ANNEX body (check `jmp` thunk first: annexed bodies live at 0x1190xxxx; build.log may be stale - derive the annex from the thunks).

Gotchas: after editing a T3 function that has lockstep rows, regenerate them (image_build_t3 now fails on stale rows). C++-lane objs need cpp_score.compile_cpp regen. Unwritten vertex fields (stale stack) differ legitimately - reported separately, not failures.

Housekeeping: commit c1e2cd1a accidentally included another session's uncommitted force-annex code in tools/brally/image_build_t3.py (no-op without config/brally/force_annex.csv).

Related: [live-oracle-2026-09-23](live-oracle-2026-09-23.md), [t3-certified-standard](../rules/t3-certified-standard.md), [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md).
