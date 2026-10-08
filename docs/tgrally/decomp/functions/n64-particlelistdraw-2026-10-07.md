# N64 particlelistdraw

*Recorded 2026-10-07.*

> BrParticleListDraw 0x8023D134 (particles.c) T4 by hand 2026-10-07, ed699642, gate 709/0; levers: index copy var, table-indexed fields, mirror test spelled A ^ B (folded op = ring pop), gDPSetPrimDepth masks, multi-line texrect block for as1 line ties

**DONE 2026-10-07, commit ed699642 (session 79d5c5), image gate 709/0.** 352 -> 0 by hand in one session.

Levers, each read from the listing:
- ROM `beqz a0; move v0,a0` = a loop index `i = n` separate from the param. Fields read as `D_80366A80[i].f` (the file's own style in BrParticleStep), not through `p`: with `p = &D[i]` uopt keeps p AND the address CSE (extra saved reg + copy). An unread declared `p` keeps the frame slot.
- Homes of uncoloured vars (x, y, h, sx at fixed sp offsets) give the declaration order directly (L53).
- Ring one pop behind with identical prologue regs: the game spells mirror tests `D_8028A8A8 ^ D_8028A8AC` (also `-`); the folded xor costs one invisible ring pop. Grep sibling files for the game's idiom before inventing phantom pops.
- cfe swaps indexed-operand products: `D[i].x1e * D[i].x1f` emits x1f-first.
- G_SETPRIMDEPTH (0xEE) = `gDPSetPrimDepth(pkt, z, 0)`; its _SHIFTL masks are ring pops (added to tgr/gbi.h).
- as1 ties: key chain is (time, besttime-as-running-min, aftercycles, latency, lineno, ready pos). Hoisted nodes keep their own lineno; a macro on one line gives all w0/w1 nodes one lineno. Writing gSPScisTextureRectangle as a block with w0 and w1 on separate lines let w0 nodes win. `cc -Wa,-R` trace + hoist tracker in build/tgrally/n64/search/8023D134/s79/ (hoist.py, mk.sh, ss.py ugen -S, ugtrace.py fixed paths).

Related: [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md), [n64-trackdrawsetup-2026-10-06](n64-trackdrawsetup-2026-10-06.md).
