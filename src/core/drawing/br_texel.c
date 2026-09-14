/* br_texel.c -- the standalone N64 texel decode, 0x100271F0.
 *
 * br_tex3d.c keeps a static copy of the same body for VC5 to inline into
 * the expanders; the original also has this out-of-line copy, which is what
 * this file scores.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: reads one 16-bit colour out of N64 texture data. As well as
 * taking the two bytes the N64's way round, it rotates the value by one bit,
 * which moves the transparency bit from the bottom of the N64's layout to
 * the top of the layout the rest of this code uses. */
/* T2 2026-09-13 (fresh, 20 fn.py probes): 39/44 B, 14/15 insns, rows 0+1.
 * The standalone body takes the texel as an INT and masks it (`and
 * eax,0xffff`); the static copy in br_tex3d.c reads two bytes through a
 * pointer.  RESIDUE: one instruction -- the original keeps `and eax,0xff`
 * on the low byte BEFORE building the high byte with `mov dh,al`; VC5
 * folds the mask into the byte move from every `<< 8` spelling (named
 * lo/hi, byte-typed lo/hi, statement forms, the mask on both halves, the
 * combined 0xffff mask separate or cast).  `* 256` keeps the mask but
 * spends a `shl`; `(u*256) & 0xff00` is 43 B with a different shape.
 * Corpus: no proven spelling to copy. */
/* @t4-pass 0x100271F0 1 2026-09-13 probes 20 bytes 39 insns 14 regions 1 rows 1 census no  (hand, fn.py variants of the byte swap) */
/* @t4-pass 0x100271F0 2 2026-09-13 probes 11 bytes 39 insns 14 regions 1 rows 1 census no  (hand, fn.py: uchar lo local + ushort hi, ushort parameter, (uchar)u<<8 | (ushort)(u>>8), a SWAP16 macro, lo reused for the alpha bit (50 B, 2+2), w built as uchar then <<= 8, (uchar)v from the int, int lo/u masks, a byte-lane union (54 B), *0x100, (uchar)(u&0xff) -- the `and eax,0xff` never survives; end-of-TU placement inert) */
/* @t4-pass 0x100271F0 2 2026-09-13 probes 38 bytes 39 insns 14 regions 1 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100271F0 3 2026-09-13 probes 54 bytes 39 insns 14 regions 1 rows 1 census yes  (tools/crank.py) */
/* @implements 0x100271F0 glide BrTex3dTexel */
uint16_t BrTex3dTexel(int v)
{
    unsigned int u = (unsigned short)v;
    unsigned int w;

    w  = (u & 0xffu) << 8;
    w |= u >> 8;
    w &= 0xffffu;
    return (uint16_t)((w >> 1) | ((w & 1u) << 15));
}
#endif /* BR_MATCHING_BUILD */
