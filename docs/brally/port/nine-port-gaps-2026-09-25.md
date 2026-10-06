# Nine port gaps

*Recorded 2026-09-25.*

> 2026-09-25 the nine functions the mac port trapped on -- 4 T4, 5 T2 with residue; VC5 levers found (dead inline flips roles, in-place conversion pun, while-loop kills SR, propagated const vector)

The nine functions the wasm port trapped on (listed in ports/brally-wasm/wasm/FINDINGS.csv) were hand-transcribed from the Glide bytes on 2026-09-25:
- **T4:** 0x10028820 BrGbiTexScanRun, 0x100335A0 BrCarPfxSpawn (gamedata/br_pfx.c), 0x10059A80 BrCarGhostApply, 0x1000E150 BrScrPtKeepNearest (drawing/br_vertlerp.c).
- **T2**, committed, each with a residue note in its source: 0x1000C9E0 (br_textmode.c, 4+4), 0x1005A500 (br_imgtint.c, 2+1), 0x10068070 + 0x100682C0 (br_wheelvel.c, 12+12 and 2+2), 0x100686D0 (scene/br_collgrid.c, 4+4).
- None of the five passes T3 Gate A yet: A2 needs rows ≤ ~4 and A3 needs every row classified.
- t3live only hooks functions that already carry an `@t3` tag, so A5 can't be run before tagging.

**Why:** all nine are needed for the macOS port to boot past the race.

**How to apply:** resume the T2s through `fn.py --var` variants. The levers proven this session were:
- An UNUSED `static __inline` definition earlier in the TU flipped the operand roles of all three transform rows. Removing it made 0x1000E150 exact.
- `aOut[idx].f` versus a `pOut` pointer changes the order of the final stores.
- `rec.iNext = head; head = iRec` in each arm stops VC5 tail-merging the two stores.
- `((float)n * K) * rate` with explicit parens.
- An int that converts IN PLACE into a float's own slot needs the `*(int *)&f` spelling. That spelling reproduced the 0x14 frame on 0x1000C9E0.
- A `while` row loop with `y++` before the pointer advance stops strength reduction. This matches BrImgRegionHasKey.
- A local `(0,0,-1)` direction vector that VC5 const-propagates keeps `0.0f + x` in the code, where a literal would fold away.
- Hit point written as scale-then-add.

Related: [x87-wall-mechanism-2026-09-13](../decomp/levers/x87-wall-mechanism-2026-09-13.md), [tyre-t4-volatile-parens-tu-2026-09-24](../decomp/levers/tyre-t4-volatile-parens-tu-2026-09-24.md), [macos-port-32bit-wasm-lane](macos-port-32bit-wasm-lane.md).
