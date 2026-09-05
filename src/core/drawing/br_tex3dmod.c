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

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_tex3d.h"
#include "slice1_04.h"      /* BrTexFormatCode (0x10027220 == 0x10027B90) */

#include <stdlib.h>
#include <string.h>

int FUN_10024490();

extern unsigned short DAT_105e1828[];   /* the resampler's scratch image */

/* WHAT IT DOES: bake the half-resolution modulation map that sits right
 * after the base level in the staging buffer into the base level itself --
 * scale the map up to full size, then scale each texel's red, green and blue
 * by the map value over 15, leaving the alpha bit alone. */
/* @implements 0x10027CD0 glide BrTex3dMipModulate */

void BrTex3dMipModulate(int param_1, unsigned short *param_2)
{
    int local_4;
    int local_8;
    unsigned short *local_c;
    unsigned short uVar1;
    unsigned int uVar2;
    unsigned int uVar4;

    FUN_10024490(DAT_105e1828, *(int *)(param_1 + 0x2a0),
                 *(int *)(param_1 + 0x2a4),
                 param_2 + *(int *)(param_1 + 0x2a0) * *(int *)(param_1 + 0x2a4),
                 *(int *)(param_1 + 0x2a0) / 2, *(int *)(param_1 + 0x2a4) / 2,
                 11);
    local_c = DAT_105e1828;
    for (local_4 = 0; local_4 < *(int *)(param_1 + 0x2a4); local_4++) {
        for (local_8 = 0; local_8 < *(int *)(param_1 + 0x2a0); local_8++) {
            uVar1 = *param_2;
            uVar4 = *local_c;
            local_c = local_c + 1;
            uVar2 = uVar1 >> 0xf;
            uVar2 = uVar2 * 32 | ((uVar1 >> 10 & 0x1f) * uVar4) / 0xf;
            uVar2 = uVar2 * 32 | ((uVar1 >> 5 & 0x1f) * uVar4) / 0xf;
            uVar2 = uVar2 * 32 | ((uVar1 & 0x1f) * uVar4) / 0xf;
            *param_2 = (unsigned short)uVar2;
            param_2 = param_2 + 1;
        }
    }
}
