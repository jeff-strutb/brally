# N64 collgridcell t4

*Recorded 2026-10-07.*

> BrCollGridCellAcquire 0x8025F18C T4 (9dbb227b, 66d9b9): mpy-by-constant hoist avoided by row-array types; a-reg webs = distinct locals; one invisible temp-ring get from a redundant & 0xffff

BrCollGridCellAcquire 0x8025F18C went 208 -> EXACT by hand on 2026-10-07 (commit 9dbb227b, gate 721/0).

Levers, each measured:
- **uopt hoists ANY mpy-by-constant into a register** (f_constinreg context 5 = mpy: in reg iff the constant is not a power of 2). This happens even for one use with no loop. Spellings like idx*12, casts, char* arithmetic and helper locals all hoist. The fix: index through a pointer-to-row type (`float (*verts)[3]`). cfe then emits ixa with the row size, and as1 expands the multiply as shifts in place.
- Pointer-to-row ixa adds base+off. The ROM's `addu x, off, base` came from byte arithmetic (`char *tris; tris + tri * 8`).
- **Single-use address expressions in distinct a-regs (a1/a2/a3)** are three different locals (t0, t1, t2), each assigned once in the same block. uopt interference is block-granular, so they get distinct colours. One reused local gets one register. The locals also keep frame words, which replaced the old unused "spare" fillers.
- Temp-ring residue (all int temp names, 61 words) came from one missing get before the srl. A diagnostic DKWB_INJECT=11:85:11 (instrE5 ugen, move t3 to tail) gave EXACT. Source: `cur[1] = (packed >> 16) & 0xffff;`. The redundant mask costs one invisible ring get ([n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md)).
- Tools: scratch g/rp.sh (replay x.O, grade this fn), g/mv.py (hoist an expression into a str Rmt reg in uopt output), g/ij.sh (instrE5 DKWB_INJECT grade), g/sim.py (simulate GP FIFO from DKWB_UGEN_TRACE=1 FREELIST events).
