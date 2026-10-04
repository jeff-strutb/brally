/* br_tex3dmod.c -- the load-time texel MODULATION pass, 0x10027CD0.
 *
 * 0x10027B60 runs this after BrTex3dExpand has filled the staging buffer at
 * 0x1186C988, and only when the descriptor's flag byte +0x260 has BOTH bit 1
 * and bit 7 set.  In that layout the decoded image is followed immediately by
 * a HALF-RESOLUTION second image -- the modulation map.  This pass resamples
 * that map back up to the base level's size (through the same resampler
 * BrTex3dMipChainLoad uses, 0x10024490, with format 11 == ARGB1555) into the
 * scratch buffer at 0x105E1828, then multiplies every 5-bit channel of the
 * base level by the map's word and divides by 15.  The 1-bit alpha rides
 * through untouched.
 *
 * Note that the map word is used WHOLE: the original masks nothing off it,
 * so a map texel is only well behaved while its value stays in 0..15.  That
 * is a property of the data the flag pair selects, not of this transcription.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_tex3d.h"
#include "br_coretypes.h"
#include "slice1_04.h"      /* BrTexFormatCode (0x10027220 == 0x10027B90) */

#include <stdlib.h>
#include <string.h>

/* FUN_10024490: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the resampler's scratch image */

/* WHAT IT DOES: bake the half-resolution modulation map that sits right
 * after the base level in the staging buffer into the base level itself --
 * scale the map up to full size, then scale each texel's red, green and blue
 * by the map value over 15, leaving the alpha bit alone. */
/* @implements 0x10027CD0 glide BrTex3dMipModulate */

void BrTex3dMipModulate(BrTexReq272 *param_1, unsigned short *param_2)
{
    /* Pixels are loaded into 16-bit locals (the original's `mov si,[..];
     * and esi,0xffff` widening) and the channel math is unsigned (`mul`).
     * The alpha bit is its own local: packed straight from `px >> 15`, VC5
     * folds its three 5-bit shifts into `px & 0x8000`, where the original
     * keeps shr 15 / shl 5.  The map texel is read before the pixel, and
     * the destination steps before the source -- that order is what homes
     * the destination cursor in param_2's dead argument slot. */
    int y, x;
    unsigned short *pSrc;
    unsigned short *pDst;
    BrTexResample(DAT_105e1828, param_1->w2a0,
                 param_1->h2a4,
                 param_2 + param_1->w2a0 * param_1->h2a4,
                 param_1->w2a0 / 2, param_1->h2a4 / 2,
                 11);
    pDst = param_2;
    pSrc = DAT_105e1828;
    for (y = 0; y < param_1->h2a4; y++) {
        for (x = 0; x < param_1->w2a0; x++) {
            unsigned short m  = *pSrc;
            unsigned short px = *pDst;
            unsigned int a = px >> 15;
            unsigned int r  = (unsigned)((px >> 10) & 0x1f) * m / 15;
            unsigned int g  = (unsigned)((px >> 5) & 0x1f) * m / 15;
            unsigned int b  = (unsigned)(px & 0x1f) * m / 15;

            *pDst = (unsigned short)((((a << 5) | r) << 5 | g) << 5 | b);
            pDst++;
            pSrc++;
        }
    }
}
