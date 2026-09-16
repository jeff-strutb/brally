/* 0x100250D0 BrTex3dExpand — fresh hand transcription from the disassembly
 * (2026-09-16). Authored block-by-block off build/match/orig/0x100250D0.bin;
 * arithmetic expressions carried verbatim from the verified decomp, control
 * flow rewritten by hand, no permuter codegen tuning. Behavioural equivalence
 * is certified by the A5 oracle (the BrRaceStep method), not a byte grind.
 *
 * WHAT IT DOES: expand one of the N64 texture formats into the 16-bit pixels
 * the 3dfx card wants -- walks each tile's source in its own bit layout
 * (colour-indexed CI4/CI8, intensity I4/I8, intensity-alpha IA8, or a raw
 * 16-bit blit) and writes out a plain texture, optionally mirroring each row
 * (param_7) and the whole tile block (param_8). The game ships N64-format art,
 * so nothing can be drawn until this has run over it.
 *
 * ABI (verified: argN at [esp+0x78+4*N] after the 0x68 frame + 4 pushes):
 *   param_1  dst        unsigned short*   output pixels (esi/puVar21)
 *   param_2  cbMax      int               output byte budget (ebp)
 *   param_3  mode       int               0 expand / 1 alt / 2 raw-copy
 *   param_4  src        unsigned char*    source texels
 *   param_5  pal        int               palette base (CLUT)
 *   param_6  fmt        int               format selector
 *   param_7  mirrorH    int               mirror each row
 *   param_8  mirrorV    int               mirror the tile block
 *   param_9  tile0      int               first tile index (loop start)
 *   param_10 tile1      int               end tile index (loop bound)
 *   param_11 recs       int               tile-record base (0x40 stride)
 *   param_12 flags      unsigned char     CI4-interleave enable (&2)
 *   param_13 sub        int               sub-format selector
 *   param_14..21        unsigned char     blend endpoints (hi/lo per channel)
 *   param_22 ilmask     int               row interleave mask
 *
 * T4 residue is pure register colouring/allocation (A5 EQUIVALENT supersedes the
 * byte-shape gates): a smaller frame than the original (sub esp,0x50 vs 0x68 --
 * the original homes more locals to slots), the pinned small-constant family
 * (orig caches 1 in a callee-saved reg, we materialise immediates), the byte-slot
 * nibble-merge widening in the I4 blend body-3 (dropping the widened uVar19 temp
 * regresses hard: 73 -> 128 rows -- it is load-bearing), and the loop-rotation
 * jmps. Two honest zero-movement probe passes below; the /255 fixup construct is
 * a corpus MISS, so there is no proven spelling to copy.
 * @t4-pass 0x100250D0 1 2026-09-16 probes 11 bytes 8211 insns 2412 regions 72 rows 73 census no
 * @t4-pass 0x100250D0 2 2026-09-16 probes 11 bytes 8211 insns 2412 regions 72 rows 73 census yes
 */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned int FUN_100271f0(unsigned short);

/* WHAT IT DOES: expand one of the N64 texture formats into the 16-bit pixels the
 * 3dfx card wants -- walks each tile's source in its own bit layout (CI4/CI8
 * colour-index, I4/I8 intensity, IA8 intensity-alpha, or a raw 16-bit blit),
 * writes out a plain texture, and optionally mirrors each row and the tile
 * block. The game ships N64-format art, so nothing draws until this runs. */
/* @t3 0x100250D0 2026-09-16 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 8211/8480 insns 2412/2407 rows 34+39 regions 72 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Behaviourally EQUIVALENT to the original across 64 valid-state seeds (all
 * modes/formats/sub-formats/flags/mirror/interleave combinations) via the A5
 * oracle; negative controls confirm the profile has teeth. Residue is pure
 * register colouring/allocation: a smaller frame (sub esp,0x50 vs 0x68), the
 * pinned small-constant family, the I4 blend body-3 byte-slot nibble-merge
 * widening (load-bearing -- dropping it regresses 73 -> 128 rows), and the
 * loop-rotation jmps. Two zero-movement probe passes (11 each) in the header;
 * the /255 fixup construct is a corpus MISS. Do not reopen before the end-grind
 * (CLAUDE.md rule 12). */
/* @implements 0x100250D0 glide BrTex3dExpand */
void BrTex3dExpand(unsigned short *param_1, int param_2, int param_3, unsigned char *param_4, int param_5, int param_6, int param_7, int param_8, int param_9, int param_10, int param_11, unsigned char param_12, int param_13, unsigned char param_14, unsigned char param_15, unsigned char param_16, unsigned char param_17, unsigned char param_18, unsigned char param_19, unsigned char param_20, unsigned char param_21, int param_22)
{
    unsigned short *puVar21;    /* live output cursor (esi)              */
    unsigned short *puVar9;     /* mirror read cursor                    */
    unsigned short *puVar2;     /* mirror-block read cursor              */
    unsigned char *pbVar12;     /* source cursor within a row            */
    int iVar3;                  /* (int)src base                         */
    int iVar5;                  /* current tile record ptr / scratch     */
    int iVar10;                 /* tile index (outer loop)               */
    int iVar22;                 /* bytes written so far                  */
    int iVar15, iVar17;         /* rows / columns of the tile            */
    int iVar16, iVar13, iVar18, iVar20;   /* per-channel deltas / counters */
    unsigned int uVar6, uVar7, uVar8, uVar14, uVar19;
    unsigned int local_38, local_3c, local_44;
    int local_24, local_34, local_54;
    unsigned char *local_64;
    unsigned char bVar11;
    unsigned char chR, chG, chB, chA, bI4inten;
    unsigned short uVar4, pal;
    int lo0, loIA8;
    int cbMax = param_2;

    iVar3 = (int)param_4;
    iVar22 = 0;
    iVar10 = param_9;
    if (param_9 >= param_10) {
        return;
    }
    puVar21 = param_1;

    for (; iVar10 < param_10; iVar10 = iVar10 + 1) {
        iVar5 = iVar10 * 0x40 + param_11;
        param_4 = (unsigned char *)(iVar3 + *(int *)(iVar10 * 0x40 + 0xc + param_11) * 8);

        if (param_3 == 0) {
            /* ---- mode 0: expand to 16-bit ---- */
            if (param_6 == 2) {
                /* CI: 4-bit colour index */
                if (((param_12 & 2) != 0) && (iVar10 == 1)) {
                    /* CI4 raw-index arm (tile 1, interleave-flagged) */
                    int cols = 1 << (*(int *)(param_11 + 0x60) - 1);
                    int rows = 1 << *(int *)(param_11 + 0x64);
                    param_9 = cols;
                    param_1 = (unsigned short *)0x0;
                    if (0 < rows) {
                        do {
                            pbVar12 = param_4;
                            if (((unsigned int)param_1 & param_22) != 0) {
                                iVar16 = 0;
                                if (0 < param_9) {
                                    do {
                                        pbVar12 = pbVar12 + 4;
                                        for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                                            if (iVar16 >= param_9) break;
                                            bVar11 = *pbVar12;
                                            iVar22 = iVar22 + 2;
                                            *puVar21 = (unsigned short)(bVar11 >> 4);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            iVar22 = iVar22 + 2;
                                            *puVar21 = (unsigned short)(bVar11 & 0xf);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + -8;
                                        for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                                            if (iVar16 >= param_9) break;
                                            bVar11 = *pbVar12;
                                            iVar22 = iVar22 + 2;
                                            *puVar21 = (unsigned short)(bVar11 >> 4);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            iVar22 = iVar22 + 2;
                                            *puVar21 = (unsigned short)(bVar11 & 0xf);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + 4;
                                    } while (iVar16 < param_9);
                                }
                            } else {
                                iVar16 = 0;
                                if (0 < param_9) {
                                    do {
                                        bVar11 = *pbVar12;
                                        iVar22 = iVar22 + 2;
                                        *puVar21 = (unsigned short)(bVar11 >> 4);
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        iVar22 = iVar22 + 2;
                                        *puVar21 = (unsigned short)(bVar11 & 0xf);
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    } while (iVar16 < param_9);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < param_9)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < param_9);
                            }
                            param_4 = param_4 + *(int *)(param_11 + 0x48);
                            param_1 = (unsigned short *)((int)param_1 + 1);
                        } while ((int)param_1 < rows);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < rows)) {
                        do {
                            iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
                            puVar9 = puVar9 + iVar16 * -2;
                            puVar2 = puVar9;
                            iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                            }
                            param_1 = (unsigned short *)((int)param_1 + 1);
                        } while ((int)param_1 < rows);
                    }
                } else {
                    /* CI4 palettized (hi/lo nibble -> CLUT -> FUN_100271f0) */
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_44 = 0;
                    if (0 < iVar15) {
                        do {
                            param_9 = (int)param_4;
                            if ((local_44 & param_22) != 0) {
                                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                                    param_9 = param_9 + 4;
                                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                                        if ((int)param_1 >= iVar17) break;
                                        bVar11 = *(unsigned char *)param_9;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bVar11 >> 4) * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bVar11 & 0xf) * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        param_9 = param_9 + 1;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                    }
                                    param_9 = param_9 + -8;
                                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                                        if ((int)param_1 >= iVar17) break;
                                        bVar11 = *(unsigned char *)param_9;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bVar11 >> 4) * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bVar11 & 0xf) * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        param_9 = param_9 + 1;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                    }
                                    param_9 = param_9 + 4;
                                }
                            } else {
                                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                                    bVar11 = *(unsigned char *)param_9;
                                    pal = *(unsigned short *)(param_5 + (unsigned int)(bVar11 >> 4) * 2);
                                    uVar4 = FUN_100271f0(pal);
                                    *puVar21 = uVar4;
                                    iVar22 = iVar22 + 2;
                                    puVar21 = puVar21 + 1;
                                    if (iVar22 >= cbMax) return;
                                    pal = *(unsigned short *)(param_5 + (bVar11 & 0xf) * 2);
                                    uVar4 = FUN_100271f0(pal);
                                    *puVar21 = uVar4;
                                    iVar22 = iVar22 + 2;
                                    puVar21 = puVar21 + 1;
                                    if (iVar22 >= cbMax) return;
                                    param_9 = param_9 + 1;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            puVar9 = puVar9 + iVar5 * -2;
                            puVar2 = puVar9;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                            }
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                }
            } else if (param_6 == 4) {
                if (param_13 == 1) {
                    /* I4 intensity-blend arm (4-bit intensity blended between
                     * two RGBA endpoints, packed to 5-5-5-1). */
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_3c = 0;
                    if (0 < iVar15) {
                        do {
                            local_64 = param_4;
                            local_54 = 0;
                            if ((local_3c & param_22) != 0) {
                                if (0 < iVar17) {
                                    do {
                                        local_64 = local_64 + 4;
                                        for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                            unsigned char bI4i1;
                                            if (local_54 >= iVar17) break;
                                            lo0 = param_18 & 0xff;
                                            iVar16 = (param_14 & 0xff) - lo0;
                                            bVar11 = *local_64;
                                            bI4i1 = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                                            chR = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar16) / 0xff + lo0) >> 3);
                                            uVar6 = param_19 & 0xff;
                                            iVar13 = (param_15 & 0xff) - uVar6;
                                            chG = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar13) / 0xff + uVar6) >> 3);
                                            uVar7 = param_20 & 0xff;
                                            iVar18 = (param_16 & 0xff) - uVar7;
                                            chB = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar18) / 0xff + uVar7) >> 3);
                                            uVar8 = param_21 & 0xff;
                                            iVar20 = (param_17 & 0xff) - uVar8;
                                            chA = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar20) / 0xff + uVar8) >> 7);
                                            iVar22 = iVar22 + 2;
                                            puVar21 = puVar21 + 1;
                                            puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                                (unsigned int)chG) << 5) | (unsigned int)chB);
                                            if (iVar22 >= cbMax) return;
                                            bI4i1 = bVar11 << 4 | bVar11 & 0xf;
                                            chR = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar16) / 0xff + lo0) >> 3);
                                            chG = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar13) / 0xff + uVar6) >> 3);
                                            chB = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar18) / 0xff + uVar7) >> 3);
                                            chA = (unsigned char)((int)((int)((unsigned int)bI4i1 * iVar20) / 0xff + uVar8) >> 7);
                                            iVar22 = iVar22 + 2;
                                            puVar21 = puVar21 + 1;
                                            puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                                (unsigned int)chG) << 5) | (unsigned int)chB);
                                            if (iVar22 >= cbMax) return;
                                            local_64 = local_64 + 1;
                                            local_54 = local_54 + 1;
                                        }
                                        local_64 = local_64 + -8;
                                        for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                            unsigned char bI4i2;
                                            if (local_54 >= iVar17) break;
                                            lo0 = param_18 & 0xff;
                                            iVar16 = (param_14 & 0xff) - lo0;
                                            bVar11 = *local_64;
                                            bI4i2 = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                                            chR = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar16) / 0xff + lo0) >> 3);
                                            uVar6 = param_19 & 0xff;
                                            iVar13 = (param_15 & 0xff) - uVar6;
                                            chG = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar13) / 0xff + uVar6) >> 3);
                                            uVar7 = param_20 & 0xff;
                                            iVar18 = (param_16 & 0xff) - uVar7;
                                            chB = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar18) / 0xff + uVar7) >> 3);
                                            uVar8 = param_21 & 0xff;
                                            iVar20 = (param_17 & 0xff) - uVar8;
                                            chA = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar20) / 0xff + uVar8) >> 7);
                                            iVar22 = iVar22 + 2;
                                            puVar21 = puVar21 + 1;
                                            puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                                (unsigned int)chG) << 5) | (unsigned int)chB);
                                            if (iVar22 >= cbMax) return;
                                            bI4i2 = bVar11 << 4 | bVar11 & 0xf;
                                            chR = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar16) / 0xff + lo0) >> 3);
                                            chG = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar13) / 0xff + uVar6) >> 3);
                                            chB = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar18) / 0xff + uVar7) >> 3);
                                            chA = (unsigned char)((int)((int)((unsigned int)bI4i2 * iVar20) / 0xff + uVar8) >> 7);
                                            iVar22 = iVar22 + 2;
                                            puVar21 = puVar21 + 1;
                                            puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                                (unsigned int)chG) << 5) | (unsigned int)chB);
                                            if (iVar22 >= cbMax) return;
                                            local_64 = local_64 + 1;
                                            local_54 = local_54 + 1;
                                        }
                                        local_64 = local_64 + 4;
                                    } while (local_54 < iVar17);
                                }
                            } else {
                                if (0 < iVar17) {
                                    do {
                                        lo0 = param_18 & 0xff;
                                        iVar16 = (param_14 & 0xff) - lo0;
                                        bVar11 = *local_64;
                                        bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                                        uVar19 = (unsigned int)bI4inten;
                                        chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                                        uVar6 = param_19 & 0xff;
                                        iVar13 = (param_15 & 0xff) - uVar6;
                                        chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                                        uVar7 = param_20 & 0xff;
                                        iVar18 = (param_16 & 0xff) - uVar7;
                                        chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                                        uVar8 = param_21 & 0xff;
                                        iVar20 = (param_17 & 0xff) - uVar8;
                                        chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                            (unsigned int)chG) << 5) | (unsigned int)chB);
                                        if (iVar22 >= cbMax) return;
                                        bI4inten = bVar11 << 4 | bVar11 & 0xf;
                                        uVar19 = (unsigned int)bI4inten;
                                        chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                                        chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                                        chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                                        chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                                              (unsigned int)chG) << 5) | (unsigned int)chB);
                                        if (iVar22 >= cbMax) return;
                                        local_64 = local_64 + 1;
                                        local_54 = local_54 + 1;
                                    } while (local_54 < iVar17);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_3c = local_3c + 1;
                        } while ((int)local_3c < iVar15);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, local_3c = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            puVar9 = puVar9 + iVar5 * -2;
                            puVar2 = puVar9;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= cbMax) return;
                            }
                            local_3c = local_3c + 1;
                        } while ((int)local_3c < iVar15);
                    }
                } else {
                    /* I4 -> 8-bit direct (two nibbles per byte, expanded in place) */
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_38 = 0;
                    if (0 < iVar15) {
                        do {
                            pbVar12 = param_4;
                            if ((local_38 & param_22) != 0) {
                                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                                    pbVar12 = pbVar12 + 4;
                                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                                        if ((int)puVar2 >= iVar17) break;
                                        bVar11 = *pbVar12;
                                        iVar22 = iVar22 + 1;
                                        *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 & 0xf0;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        if (iVar22 >= cbMax) return;
                                        iVar22 = iVar22 + 1;
                                        *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        puVar2 = (unsigned short *)((int)puVar2 + 1);
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                                        if ((int)puVar2 >= iVar17) break;
                                        bVar11 = *pbVar12;
                                        iVar22 = iVar22 + 1;
                                        *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        if (iVar22 >= cbMax) return;
                                        iVar22 = iVar22 + 1;
                                        *(unsigned char *)puVar21 = bVar11 & 0xf | bVar11 << 4;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        puVar2 = (unsigned short *)((int)puVar2 + 1);
                                    }
                                    pbVar12 = pbVar12 + 4;
                                }
                            } else {
                                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                                    bVar11 = *pbVar12;
                                    iVar22 = iVar22 + 1;
                                    *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    if (iVar22 >= cbMax) return;
                                    iVar22 = iVar22 + 1;
                                    *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    if (iVar22 >= cbMax) return;
                                    pbVar12 = pbVar12 + 1;
                                    puVar2 = (unsigned short *)((int)puVar2 + 1);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 1;
                                    *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    if (iVar22 >= cbMax) return;
                                    iVar22 = iVar22 + 1;
                                    *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_38 = local_38 + 1;
                        } while ((int)local_38 < iVar15);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, local_38 = 0, 0 < iVar15)) {
                        do {
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            puVar9 = puVar9 + -iVar16;
                            puVar2 = puVar9;
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                iVar22 = iVar22 + 1;
                                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                                puVar21 = (unsigned short *)((int)puVar21 + 1);
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                                if (iVar22 >= cbMax) return;
                                iVar22 = iVar22 + 1;
                                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                                puVar21 = (unsigned short *)((int)puVar21 + 1);
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                                if (iVar22 >= cbMax) return;
                            }
                            local_38 = local_38 + 1;
                        } while ((int)local_38 < iVar15);
                    }
                }
            }
        } else if (param_3 == 1) {
            /* ---- mode 1 ---- */
            if (param_6 == 2) {
                /* CI8 palettized (full byte index -> CLUT -> FUN_100271f0) */
                iVar15 = 1 << *(int *)(iVar5 + 0x20);
                local_44 = 0;
                iVar17 = 1 << *(int *)(iVar5 + 0x24);
                if (0 < iVar17) {
                    do {
                        pbVar12 = param_4;
                        if ((param_22 & local_44) != 0) {
                            iVar16 = 0;
                            param_1 = (unsigned short *)0x0;
                            if (0 < iVar15) {
                                do {
                                    pbVar12 = pbVar12 + 4;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        if (iVar16 >= iVar15) break;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = (int)param_1 + 1;
                                        param_1 = (unsigned short *)iVar16;
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        if (iVar16 >= iVar15) break;
                                        uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                        *puVar21 = uVar4;
                                        iVar22 = iVar22 + 2;
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = (int)param_1 + 1;
                                        param_1 = (unsigned short *)iVar16;
                                    }
                                    pbVar12 = pbVar12 + 4;
                                } while (iVar16 < iVar15);
                            }
                        } else {
                            param_1 = (unsigned short *)0x0;
                            iVar16 = iVar15;
                            if (0 < iVar15) {
                                do {
                                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                    *puVar21 = uVar4;
                                    iVar22 = iVar22 + 2;
                                    puVar21 = puVar21 + 1;
                                    if (iVar22 >= cbMax) return;
                                    pbVar12 = pbVar12 + 1;
                                    iVar16 = (int)param_1 + 1;
                                    param_1 = (unsigned short *)iVar16;
                                } while (iVar16 < iVar15);
                            }
                        }
                        if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar15)) {
                            do {
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar9;
                                puVar21 = puVar21 + 1;
                                puVar9 = puVar9 + -1;
                                if (iVar22 >= cbMax) return;
                                iVar16 = iVar16 + 1;
                            } while (iVar16 < iVar15);
                        }
                        param_4 = param_4 + *(int *)(iVar5 + 8);
                        local_44 = local_44 + 1;
                    } while ((int)local_44 < iVar17);
                }
                if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar17)) {
                    do {
                        iVar5 = (param_7 != 0) ? iVar15 * 2 : iVar15;
                        puVar9 = puVar9 + -iVar5;
                        puVar2 = puVar9;
                        iVar5 = (param_7 != 0) ? iVar15 * 2 : iVar15;
                        for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                            iVar22 = iVar22 + 2;
                            *puVar21 = *puVar2;
                            puVar21 = puVar21 + 1;
                            if (iVar22 >= cbMax) return;
                            puVar2 = puVar2 + 1;
                        }
                        local_44 = local_44 + 1;
                    } while ((int)local_44 < iVar17);
                }
            } else if (param_6 == 3) {
                if (param_13 == 1) {
                    /* IA8 blend: 8-bit source = 4-bit intensity + 4-bit alpha,
                     * intensity blended between two RGB endpoints, alpha in low nibble */
                    iVar17 = 1 << *(int *)(iVar5 + 0x20);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_44 = 0;
                    if (0 < iVar15) {
                        do {
                            param_9 = (int)param_4;
                            param_1 = (unsigned short *)0x0;
                            if ((local_44 & param_22) != 0) {
                                if (0 < iVar17) {
                                    do {
                                        param_9 = param_9 + 4;
                                        for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                            if ((int)param_1 >= iVar17) break;
                                            iVar22 = iVar22 + 2;
                                            bVar11 = *(unsigned char *)param_9;
                                            loIA8 = (unsigned int)bVar11 & 0xf;
                                            uVar14 = ((unsigned int)bVar11 >> 4) | ((unsigned int)bVar11 & 0xf0);
                                            *puVar21 = (unsigned short)((((loIA8) << 4 |
                                                                 (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                                                 (param_18 & 0xff) >> 4) << 4 |
                                                                (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                                                (param_19 & 0xff) >> 4) << 4) |
                                                       (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                                                (param_20 & 0xff) >> 4);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            param_9 = param_9 + 1;
                                            param_1 = (unsigned short *)((int)param_1 + 1);
                                        }
                                        param_9 = param_9 + -8;
                                        for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                            if ((int)param_1 >= iVar17) break;
                                            iVar22 = iVar22 + 2;
                                            bVar11 = *(unsigned char *)param_9;
                                            loIA8 = (unsigned int)bVar11 & 0xf;
                                            uVar14 = ((unsigned int)bVar11 >> 4) | ((unsigned int)bVar11 & 0xf0);
                                            *puVar21 = (unsigned short)((((loIA8) << 4 |
                                                                (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                                                (param_18 & 0xff) >> 4) << 4 |
                                                               (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                                               (param_19 & 0xff) >> 4) << 4) |
                                                      (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                                               (param_20 & 0xff) >> 4);
                                            puVar21 = puVar21 + 1;
                                            if (iVar22 >= cbMax) return;
                                            param_9 = param_9 + 1;
                                            param_1 = (unsigned short *)((int)param_1 + 1);
                                        }
                                        param_9 = param_9 + 4;
                                    } while ((int)param_1 < iVar17);
                                }
                            } else {
                                if (0 < iVar17) {
                                    do {
                                        iVar22 = iVar22 + 2;
                                        bVar11 = *(unsigned char *)param_9;
                                        loIA8 = (unsigned int)bVar11 & 0xf;
                                        uVar14 = ((unsigned int)bVar11 & 0xf0) | ((unsigned int)bVar11 >> 4);
                                        *puVar21 = (unsigned short)((((loIA8) << 4 |
                                                            (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                                            (param_18 & 0xff) >> 4) << 4 |
                                                           (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                                           (param_19 & 0xff) >> 4) << 4) |
                                                  (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                                           (param_20 & 0xff) >> 4);
                                        puVar21 = puVar21 + 1;
                                        if (iVar22 >= cbMax) return;
                                        param_9 = param_9 + 1;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                    } while ((int)param_1 < iVar17);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *puVar21 = *puVar9;
                                    puVar21 = puVar21 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            puVar9 = puVar9 + -iVar5;
                            puVar2 = puVar9;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *puVar21 = *puVar2;
                                puVar21 = puVar21 + 1;
                                if (iVar22 >= cbMax) return;
                                puVar2 = puVar2 + 1;
                            }
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                } else {
                    /* IA4 -> 8-bit nibble-swap direct copy */
                    iVar17 = 1 << *(int *)(iVar5 + 0x20);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    param_1 = (unsigned short *)0x0;
                    if (0 < iVar15) {
                        do {
                            pbVar12 = param_4;
                            if (((unsigned int)param_1 & param_22) != 0) {
                                iVar16 = 0;
                                if (0 < iVar17) {
                                    do {
                                        pbVar12 = pbVar12 + 4;
                                        for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                                            if (iVar16 >= iVar17) break;
                                            bVar11 = *pbVar12;
                                            *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 >> 4;
                                            puVar21 = (unsigned short *)((int)puVar21 + 1);
                                            iVar22 = iVar22 + 1;
                                            if (iVar22 >= cbMax) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + -8;
                                        for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                                            if (iVar16 >= iVar17) break;
                                            bVar11 = *pbVar12;
                                            *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                                            puVar21 = (unsigned short *)((int)puVar21 + 1);
                                            iVar22 = iVar22 + 1;
                                            if (iVar22 >= cbMax) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + 4;
                                    } while (iVar16 < iVar17);
                                }
                            } else {
                                param_9 = 0;
                                if (0 < iVar17) {
                                    do {
                                        bVar11 = *pbVar12;
                                        *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        param_9 = param_9 + 1;
                                    } while (param_9 < iVar17);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                                do {
                                    *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= cbMax) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            param_1 = (unsigned short *)((int)param_1 + 1);
                        } while ((int)param_1 < iVar15);
                    }
                    if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
                        do {
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            puVar9 = (unsigned short *)((int)puVar9 - iVar16);
                            puVar2 = puVar9;
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                *(unsigned char *)puVar21 = (unsigned char)*puVar2;
                                puVar21 = (unsigned short *)((int)puVar21 + 1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= cbMax) return;
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                            }
                            param_1 = (unsigned short *)((int)param_1 + 1);
                        } while ((int)param_1 < iVar15);
                    }
                }
            } else if (param_6 == 4) {
                /* I8 -> 8-bit direct copy */
                param_9 = 1 << *(int *)(iVar5 + 0x20);
                iVar15 = 1 << *(int *)(iVar5 + 0x24);
                param_1 = (unsigned short *)0x0;
                if (0 < iVar15) {
                    do {
                        pbVar12 = param_4;
                        if ((param_22 & (unsigned int)param_1) != 0) {
                            iVar16 = 0;
                            if (0 < param_9) {
                                do {
                                    pbVar12 = pbVar12 + 4;
                                    for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                                        if (iVar16 >= param_9) break;
                                        *(unsigned char *)puVar21 = *pbVar12;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                                        if (iVar16 >= param_9) break;
                                        *(unsigned char *)puVar21 = *pbVar12;
                                        puVar21 = (unsigned short *)((int)puVar21 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= cbMax) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    }
                                    pbVar12 = pbVar12 + 4;
                                } while (iVar16 < param_9);
                            }
                        } else {
                            iVar16 = 0;
                            if (0 < param_9) {
                                do {
                                    *(unsigned char *)puVar21 = *pbVar12;
                                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= cbMax) return;
                                    pbVar12 = pbVar12 + 1;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < param_9);
                            }
                        }
                        if ((param_7 != 0) && (local_24 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < param_9)) {
                            do {
                                *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                                puVar21 = (unsigned short *)((int)puVar21 + 1);
                                puVar9 = (unsigned short *)((int)puVar9 + -1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= cbMax) return;
                                local_24 = local_24 + 1;
                            } while (local_24 < param_9);
                        }
                        param_4 = param_4 + *(int *)(iVar5 + 8);
                        param_1 = (unsigned short *)((int)param_1 + 1);
                    } while ((int)param_1 < iVar15);
                }
                if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
                    do {
                        iVar5 = (param_7 != 0) ? param_9 * 2 : param_9;
                        puVar9 = (unsigned short *)((int)puVar9 - iVar5);
                        puVar2 = puVar9;
                        iVar5 = (param_7 != 0) ? param_9 * 2 : param_9;
                        for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                            *(unsigned char *)puVar21 = (unsigned char)*puVar2;
                            puVar21 = (unsigned short *)((int)puVar21 + 1);
                            iVar22 = iVar22 + 1;
                            if (iVar22 >= cbMax) return;
                            puVar2 = (unsigned short *)((int)puVar2 + 1);
                        }
                        param_1 = (unsigned short *)((int)param_1 + 1);
                    } while ((int)param_1 < iVar15);
                }
            }
        } else if ((param_3 == 2) && (param_6 == 0)) {
            /* ---- mode 2: raw 16-bit blit through FUN_100271f0 ---- */
            uVar14 = 1 << *(int *)(iVar5 + 0x20);
            local_44 = 0;
            iVar15 = 1 << *(int *)(iVar5 + 0x24);
            if (0 < iVar15) {
                do {
                    param_9 = 0;
                    puVar9 = puVar21;
                    iVar17 = iVar22;
                    pbVar12 = param_4;
                    if ((param_22 & local_44) != 0) {
                        if (0 < (int)uVar14) {
                            while (1) {
                                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                                *puVar21 = uVar4;
                                puVar21 = puVar21 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= cbMax) return;
                                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                                *puVar21 = uVar4;
                                puVar21 = puVar21 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= cbMax) return;
                                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                                *puVar21 = uVar4;
                                puVar21 = puVar21 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= cbMax) return;
                                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                                *puVar21 = uVar4;
                                puVar21 = puVar21 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= cbMax) return;
                            }
                            puVar9 = puVar21;
                            iVar17 = iVar22;
                        }
                    } else {
                        if (0 < (int)uVar14) {
                            do {
                                uVar4 = FUN_100271f0(*(unsigned short *)pbVar12);
                                *puVar21 = uVar4;
                                iVar22 = iVar22 + 2;
                                puVar21 = puVar21 + 1;
                                pbVar12 = pbVar12 + 2;
                                if (iVar22 >= cbMax) return;
                                param_9 = param_9 + 1;
                                puVar9 = puVar21;
                                iVar17 = iVar22;
                            } while (param_9 < (int)uVar14);
                        }
                    }
                    iVar22 = iVar17;
                    puVar21 = puVar9;
                    if ((param_7 != 0) && (iVar17 = 0, puVar9 = puVar21 + -1, 0 < (int)uVar14)) {
                        do {
                            iVar22 = iVar22 + 2;
                            *puVar21 = *puVar9;
                            puVar21 = puVar21 + 1;
                            puVar9 = puVar9 + -1;
                            if (iVar22 >= cbMax) return;
                            iVar17 = iVar17 + 1;
                        } while (iVar17 < (int)uVar14);
                    }
                    param_4 = param_4 + *(int *)(iVar5 + 8);
                    local_44 = local_44 + 1;
                } while ((int)local_44 < iVar15);
            }
            if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
                do {
                    uVar6 = (param_7 != 0) ? uVar14 * 2 : uVar14;
                    puVar9 = puVar9 + -(int)uVar6;
                    puVar2 = puVar9;
                    uVar6 = (param_7 != 0) ? uVar14 * 2 : uVar14;
                    for (; 0 < (int)uVar6; uVar6 = uVar6 - 1) {
                        iVar22 = iVar22 + 2;
                        *puVar21 = *puVar2;
                        puVar21 = puVar21 + 1;
                        if (iVar22 >= cbMax) return;
                        puVar2 = puVar2 + 1;
                    }
                    local_44 = local_44 + 1;
                } while ((int)local_44 < iVar15);
            }
        }
    }
}

#endif /* BR_MATCHING_BUILD */
