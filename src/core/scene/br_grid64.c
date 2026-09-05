/* br_grid64.c -- the coarse 64x64 track grid sample.
 *
 * RESPONSIBILITY: scene -- the track's spatial tables (see br_collgrid.c for
 * the fine collision grid this one sits above).
 *
 * Moved here out of src/core/slice1_01.c (an address batch, not a module),
 * whose preamble is carried over verbatim below.  In the matching build the
 * header's port prototype (three arguments) is renamed out of the way: the
 * original takes (x, y) and reads the grid base from a global.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrGrid64Sample BrGrid64Sample_port
#endif
#include "slice1_01.h"
#ifdef BR_MATCHING_BUILD
#undef BrGrid64Sample
extern const uint16_t *g_pBrGrid64;      /* 0x106EECFC */
#endif

#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * 0x10002DE0 -- 64x64 u16 grid sample.
 *
 * The four guards are `fcomp` against 0.0f (0x1008F09C) and 2048.0f
 * (0x1008F0A0), read back with fnstsw / `test ah,1`, i.e. the C0 bit, i.e.
 * "ST < operand". Order in the original is x>=0, x<2048, y>=0, y<2048; x is
 * the one loaded twice through [esp+8] once esi has been pushed.
 *
 * The scale is 0x1008F0A4 = 0.03125f = 1/32, exact in binary, so no rounding
 * question arises before the truncation. 0x1007C8A0 is __ftol (it sets the
 * x87 rounding field to 0xC00 = toward zero and does `fistp qword`).
 *
 * Only AL of each conversion is consumed (`movzx si, al` / `movzx ax, al`),
 * so a value of 256 or more would wrap -- unreachable given the 2048 guard,
 * but reproduced with the mask below rather than assumed away.
 *
 * DEVIATION (port arm only): the grid base was the global at 0x106C7C6C; the
 * port arm takes it as a parameter.  The matching arm reads g_pBrGrid64
 * exactly where the original does.
 *
 * Byte-exact 2026-09-04 (Glide 0x10003120, 170 B), three source facts:
 *  - the four guards are ONE `||` chain returning 0 -- all four jump to the
 *    single `xor eax,eax; pop esi; ret` block.  Four sequential early
 *    returns emit four exits (+16 B).
 *  - row and col are `unsigned short` locals assigned an `(unsigned char)`
 *    cast: that is the `movzx si, al` / `movzx ax, al` pair.  `& 0xFF` into
 *    unsigned int locals is an `and`, two instructions longer; idx must be
 *    `unsigned short` too (the `& 0xFFFF` spelling is +12 B).
 *  - the grid pointer is read from the global AT THE USE, after both
 *    __ftol calls; caching it in a local at entry makes VC5 load it into
 *    edi in the prologue (push edi, +5 B).
 */
/* WHAT IT DOES: looks up a place in the world on a coarse 64-by-64 grid --
 * each square covering thirty-two world units, so the grid spans a square
 * region a couple of thousand units across -- and hands back both that
 * square's value and the difference to the square next along, so a caller can
 * blend between the two. WHAT THE GRID HOLDS IS NOT ESTABLISHED HERE. A
 * position outside the covered region answers zero, which is indistinguishable
 * from a square whose value genuinely is zero. */
/* @implements 0x10002DE0 d3d BrGrid64Sample */
#ifdef BR_MATCHING_BUILD
uint32_t BrGrid64Sample(float x, float y)
{
    unsigned short row, col, idx;
    uint32_t t0, t1;

    if (x < 0.0f || x >= 2048.0f || y < 0.0f || y >= 2048.0f)
        return 0u;

    row = (unsigned char)(int)(y * 0.03125f);
    col = (unsigned char)(int)(x * 0.03125f);
    idx = (unsigned short)((row << 6) + col);
    t0 = g_pBrGrid64[idx];
    t1 = g_pBrGrid64[(unsigned short)(idx + 1)];
    /* Literally `t1 + t0*65535`, shifted up 16 -- the low 16 bits of that
     * sum are (t1 - t0) mod 65536, which is the per-cell step. */
    return ((t1 + t0 * 65535u) << 16) | t0;
}
#else
uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y)
{
    unsigned int col, row, idx;
    uint32_t t0, t1, acc;

    /* Written as negated comparisons so NaN takes the reject path, which is
     * what the original does: fcomp with a NaN sets C0, and the first guard
     * rejects on C0. */
    if (!(x >= 0.0f))    { return 0u; }
    if (!(x < 2048.0f))  { return 0u; }
    if (!(y >= 0.0f))    { return 0u; }
    if (!(y < 2048.0f))  { return 0u; }

    col = (unsigned int)(long)(x * 0.03125f) & 0xFFu;
    row = (unsigned int)(long)(y * 0.03125f) & 0xFFu;

    /* esi holds row<<6 with the caller's leftover high bits still in it and
     * eax holds __ftol's high bits; both are discarded by `and 0xffff`. */
    idx = ((row << 6) + col) & 0xFFFFu;

    t0 = pGrid[idx];
    t1 = pGrid[(idx + 1u) & 0xFFFFu];

    /* Literally `t1 + t0*65535`, shifted up 16 -- the low 16 bits of that sum
     * are (t1 - t0) mod 65536, which is the per-cell step. */
    acc = (uint32_t)((t1 + t0 * 65535u) << 16);
    return acc | t0;
}

#endif
