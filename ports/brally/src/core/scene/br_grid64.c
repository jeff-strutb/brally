/* br_grid64.c -- the coarse 64x64 track grid sample.
 *
 * RESPONSIBILITY: scene -- the track's spatial tables (see br_collgrid.c for
 * the fine collision grid this one sits above).
 *
 * Moved here out of src/brally/core/slice1_01.c (an address batch, not a module),
 * whose preamble is carried over verbatim below.  In the matching build the
 * header's port prototype (three arguments) is renamed out of the way: the
 * original takes (x, y) and reads the grid base from a global.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrGrid64Sample BrGrid64Sample_port
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "slice1_01.h"
#undef BrGrid64Sample
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x106EECFC */

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
 * port arm takes it as a parameter.  The matching arm reads BR_PTR32(const uint16_t *, g_brTrkHdr.aGridStart)
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
uint32_t BrGrid64Sample(float x, float y)
{
    unsigned short row, col, idx;
    uint32_t t0, t1;

    if (x < 0.0f || x >= 2048.0f || y < 0.0f || y >= 2048.0f)
        return 0u;

    row = (unsigned char)(int)(y * 0.03125f);
    col = (unsigned char)(int)(x * 0.03125f);
    idx = (unsigned short)((row << 6) + col);
    t0 = BR_PTR32(const uint16_t *, g_brTrkHdr.aGridStart)[idx];
    t1 = BR_PTR32(const uint16_t *, g_brTrkHdr.aGridStart)[(unsigned short)(idx + 1)];
    /* Literally `t1 + t0*65535`, shifted up 16 -- the low 16 bits of that
     * sum are (t1 - t0) mod 65536, which is the per-cell step. */
    return ((t1 + t0 * 65535u) << 16) | t0;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* 0x106EED44 -- 64 x 64 u16 */

/* WHAT IT DOES: reads cell (a, b) of a 64x64 table of 16-bit values and the
 * cell after it, and packs them as `value | ((next - value) << 16)` -- the
 * base and step a caller interpolates between.  Out-of-range coordinates
 * give 0. */
/* @implements 0x100031D0 glide BrGrid16Pair */
unsigned int BrGrid16Pair(int a, int b)
{
    unsigned short x, y, i, i2;
    unsigned int   lo, hi;

    if (a < 0 || a >= 0x40 || b < 0 || b >= 0x40)
        return 0;
    x  = (unsigned char)a;
    y  = (unsigned char)b;
    i  = (y << 6) + x;
    i2 = i + 1;
    lo = BR_PTR32(unsigned short *, g_brTrkHdr.aGrid16)[i];
    hi = BR_PTR32(unsigned short *, g_brTrkHdr.aGrid16)[i2];
    return ((hi + lo * 0xFFFF) << 16) | lo;
}
