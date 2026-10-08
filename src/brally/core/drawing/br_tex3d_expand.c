/* 0x100250D0 BrTex3dExpand: hand transcribed from the disassembly of
 * build/brally/win32/match/orig/0x100250D0.bin, byte-exact under MSVC 5.0 /O2.
 *
 * WHAT IT DOES: expand one of the N64 texture formats into the 16-bit pixels
 * the 3dfx card wants -- walks each tile's source in its own bit layout
 * (colour-indexed CI4/CI8, intensity I4/I8, intensity-alpha IA8, or a raw
 * 16-bit blit) and writes out a plain texture, optionally mirroring each row
 * (param_7) and the whole tile block (param_8). The game ships N64-format art,
 * so nothing can be drawn until this has run over it.
 *
 * ABI (verified: argN at [esp+0x78+4*N] after the 0x68 frame + 4 pushes):
 *   param_1  dst        unsigned short*   output pixels (esi)
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
 * What the bytes fix in the source:
 *   - The front end numbers every declaration in the file with one counter and
 *     C2 breaks order-only ties (commutative copy coalescing, local register
 *     picks) on those ids mod 65536. In the original TU the counter wrapped
 *     65536 between the I4 blend arm's first and second bodies: body 1's
 *     deltas carry large ids, body 2's small ones. BR_ID* below declares the
 *     typedefs that put this file's counter at the same place; the count is
 *     exact (one more or one fewer changes the code).
 *   - Per-channel low endpoints are inline (param_X & 0xff) CSE temps shared
 *     by all three blend bodies and the IA8 arm; only alpha's delta is a named
 *     loop local (frame slots are ordered by reference count).
 *   - Byte nibble merges: x >> 4 | x & 0xf0 puts the shift in the OR
 *     destination iff bit 7 of x's id is set, x << 4 | x & 0xf iff bit 3.
 *   - I4 blend body 2, second pixel: alpha is computed inside the pack and the
 *     byte counter is advanced after the store. With the counter ahead of the
 *     store, the constant 2 falls inside the region where C2 splits g2/b2, its
 *     piece is freed there, and every later split piece takes a different id,
 *     which swaps the CI8 and RGBA param_8 mirror loops' preheader layouts.
 */

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned int FUN_100271f0(unsigned short);

/* Declaration-count pad: 43330 typedefs (see the header). */
#define BR_ID1(p)     typedef int p;
#define BR_ID10(p)    BR_ID1(p##0) BR_ID1(p##1) BR_ID1(p##2) BR_ID1(p##3) BR_ID1(p##4) \
                      BR_ID1(p##5) BR_ID1(p##6) BR_ID1(p##7) BR_ID1(p##8) BR_ID1(p##9)
#define BR_ID100(p)   BR_ID10(p##0) BR_ID10(p##1) BR_ID10(p##2) BR_ID10(p##3) BR_ID10(p##4) \
                      BR_ID10(p##5) BR_ID10(p##6) BR_ID10(p##7) BR_ID10(p##8) BR_ID10(p##9)
#define BR_ID1000(p)  BR_ID100(p##0) BR_ID100(p##1) BR_ID100(p##2) BR_ID100(p##3) BR_ID100(p##4) \
                      BR_ID100(p##5) BR_ID100(p##6) BR_ID100(p##7) BR_ID100(p##8) BR_ID100(p##9)
#define BR_ID10000(p) BR_ID1000(p##0) BR_ID1000(p##1) BR_ID1000(p##2) BR_ID1000(p##3) BR_ID1000(p##4) \
                      BR_ID1000(p##5) BR_ID1000(p##6) BR_ID1000(p##7) BR_ID1000(p##8) BR_ID1000(p##9)
BR_ID10000(br_id_a) BR_ID10000(br_id_b) BR_ID10000(br_id_c) BR_ID10000(br_id_d)
BR_ID1000(br_id_e) BR_ID1000(br_id_f) BR_ID1000(br_id_g)
BR_ID100(br_id_h) BR_ID100(br_id_i) BR_ID100(br_id_j)
BR_ID10(br_id_k) BR_ID10(br_id_l) BR_ID10(br_id_m)

/* WHAT IT DOES: expand one of the N64 texture formats into the 16-bit pixels the
 * 3dfx card wants -- walks each tile's source in its own bit layout (CI4/CI8
 * colour-index, I4/I8 intensity, IA8 intensity-alpha, or a raw 16-bit blit),
 * writes out a plain texture, and optionally mirrors each row and the tile
 * block. The game ships N64-format art, so nothing draws until this runs. */
/* @implements 0x100250D0 glide BrTex3dExpand */
void BrTex3dExpand(unsigned short *param_1, int param_2, int param_3, unsigned char *param_4, int param_5, int param_6, int param_7, int param_8, int param_9, int param_10, int param_11, unsigned char param_12, int param_13, unsigned char param_14, unsigned char param_15, unsigned char param_16, unsigned char param_17, unsigned char param_18, unsigned char param_19, unsigned char param_20, unsigned char param_21, int param_22)
{
    unsigned char bL0;
    unsigned short *puVar9;     /* mirror read cursor                    */
    unsigned short *puVar2;     /* mirror-block read cursor              */
    unsigned char *pbVar12;     /* source cursor within a row            */
    int iVar3;                  /* (int)src base                         */
    int iVar5;                  /* current tile record ptr / scratch     */
    int iVar10;                 /* tile index (outer loop)               */
    int iVar22;
    int ctr1;
    unsigned int ci8_row;
    unsigned int rgba_row;                 /* bytes written so far                  */
    int iVar15, iVar17;         /* rows / columns of the tile            */
    int iVar16, iVar13, iVar18, iVar20;   /* per-channel deltas / counters */
    unsigned int uVar6, uVar7, uVar8, uVar14, uVar19;
    unsigned int local_38, local_3c, local_44;
    int local_24, local_34, local_54;
    unsigned char *local_64;
    unsigned char bVar11;
    unsigned char b224;
    unsigned char b241;
    unsigned char b259;
    unsigned char b690;
    unsigned char b709;
    unsigned char b731;
    unsigned char chR, chG, chB, chA, bI4inten;
    unsigned short uVar4, pal;
    int lo0, loIA8;
    unsigned int uW2;
    unsigned int ia_r, ia_g, ia_b;

    iVar3 = (int)param_4;
    iVar22 = 0;
    iVar10 = param_9;
    if (param_9 >= param_10) {
        return;
    }

    for (; iVar10 < param_10; iVar10 = iVar10 + 1) {
        iVar5 = iVar10 * 0x40 + param_11;
        param_4 = (unsigned char *)(iVar3 + *(int *)(iVar10 * 0x40 + 0xc + param_11) * 8);

        if (param_3 == 0) {
            /* ---- mode 0: expand to 16-bit ---- */
            if (param_6 == 2) {
                /* CI: 4-bit colour index */
                if (((param_12 & 2) != 0) && (iVar10 == 1)) {
                    /* CI4 raw-index arm (tile 1, interleave-flagged) */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n0_ctr1;
                    int rows;
                    param_9 = 1 << (*(int *)(param_11 + 0x60) - 1);
                    rows = 1 << *(int *)(param_11 + 0x64);
                    n0_ctr1 = 0;
                    if (0 < rows) {
                        do {
                            pbVar12 = param_4;
                            if (((unsigned int)n0_ctr1 & param_22) != 0) {
                                iVar16 = 0;
                                if (0 < param_9) {
                                    do {
                                        pbVar12 = pbVar12 + 4;
                                        for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                                            if (iVar16 >= param_9) break;
                                            bVar11 = *pbVar12;
                                            iVar22 = iVar22 + 2;
                                            *param_1 = (unsigned short)(bVar11 >> 4);
                                            param_1 = param_1 + 1;
                                            if (iVar22 >= param_2) return;
                                            iVar22 = iVar22 + 2;
                                            *param_1 = (unsigned short)(bVar11 & 0xf);
                                            param_1 = param_1 + 1;
                                            if (iVar22 >= param_2) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + -8;
                                        for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                                            if (iVar16 >= param_9) break;
                                            bVar11 = *pbVar12;
                                            iVar22 = iVar22 + 2;
                                            *param_1 = (unsigned short)(bVar11 >> 4);
                                            param_1 = param_1 + 1;
                                            if (iVar22 >= param_2) return;
                                            iVar22 = iVar22 + 2;
                                            *param_1 = (unsigned short)(bVar11 & 0xf);
                                            param_1 = param_1 + 1;
                                            if (iVar22 >= param_2) return;
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
                                        *param_1 = (unsigned short)(bVar11 >> 4);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        iVar22 = iVar22 + 2;
                                        *param_1 = (unsigned short)(bVar11 & 0xf);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    } while (iVar16 < param_9);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = param_1 + -1, 0 < param_9)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < param_9);
                            }
                            param_4 = param_4 + *(int *)(param_11 + 0x48);
                            n0_ctr1 = n0_ctr1 + 1;
                        } while (n0_ctr1 < rows);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, n0_ctr1 = 0, 0 < rows)) {
                        do {
                            iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
                            param_4 = (unsigned char *)((unsigned short *)param_4 + iVar16 * -2);
                            puVar2 = (unsigned short *)param_4;
                            iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                            }
                            n0_ctr1 = n0_ctr1 + 1;
                        } while (n0_ctr1 < rows);
                    }
                } else {
                    /* CI4 palettized (hi/lo nibble -> CLUT -> FUN_100271f0) */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n1_ctr1;
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_44 = 0;
                    if (0 < iVar15) {
                        do {
                            param_9 = (int)param_4;
                            if ((local_44 & param_22) != 0) {
                                for (n1_ctr1 = 0; n1_ctr1 < iVar17;) {
                                    param_9 = param_9 + 4;
                                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                                        if (n1_ctr1 >= iVar17) break;
                                        b224 = *(unsigned char *)param_9;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(b224 >> 4) * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (b224 & 0xf) * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        param_9 = param_9 + 1;
                                        n1_ctr1 = n1_ctr1 + 1;
                                    }
                                    param_9 = param_9 + -8;
                                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                                        if (n1_ctr1 >= iVar17) break;
                                        b241 = *(unsigned char *)param_9;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(b241 >> 4) * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (b241 & 0xf) * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        param_9 = param_9 + 1;
                                        n1_ctr1 = n1_ctr1 + 1;
                                    }
                                    param_9 = param_9 + 4;
                                }
                            } else {
                                for (n1_ctr1 = 0; n1_ctr1 < iVar17;) {
                                    b259 = *(unsigned char *)param_9;
                                    pal = *(unsigned short *)(param_5 + (unsigned int)(b259 >> 4) * 2);
                                    *param_1 = FUN_100271f0(pal);
                                    iVar22 = iVar22 + 2;
                                    param_1 = param_1 + 1;
                                    if (iVar22 >= param_2) return;
                                    pal = *(unsigned short *)(param_5 + (b259 & 0xf) * 2);
                                    *param_1 = FUN_100271f0(pal);
                                    iVar22 = iVar22 + 2;
                                    param_1 = param_1 + 1;
                                    if (iVar22 >= param_2) return;
                                    param_9 = param_9 + 1;
                                    n1_ctr1 = n1_ctr1 + 1;
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = param_1 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, local_44 = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            param_4 = (unsigned char *)((unsigned short *)param_4 + iVar5 * -2);
                            puVar2 = (unsigned short *)param_4;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                            }
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                }
            } else if (param_6 == 4) {
                if (param_13 == 1) {
                    /* I4 intensity-blend arm (4-bit intensity blended between
                     * two RGBA endpoints, packed to 5-5-5-1). */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2;
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_3c = 0;
                    if (0 < iVar15) {
                        do {
                            local_64 = param_4;
                            if ((local_3c & param_22) != 0) {
                                for (local_54 = 0; local_54 < iVar17;) {
                                    local_64 = local_64 + 4;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        unsigned char bI4i1;
                                        unsigned char c1, r1, g1, b1, a1;
                                        if (local_54 >= iVar17) break;


                                        c1 = *local_64;
                                        bI4i1 = (unsigned char)(c1 >> 4 | c1 & 0xf0);
                                        r1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);


                                        g1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                        b1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                        uVar8 = param_21 & 0xff;
                                        a1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_17 & 0xff) - (param_21 & 0xff))) / 0xff + uVar8) >> 7);
                                        iVar22 = iVar22 + 2;
                                        *param_1 = (unsigned short)(((((unsigned int)a1 << 5 | (unsigned int)r1) << 5 |
                                                            (unsigned int)g1) << 5) | (unsigned int)b1);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        bI4i1 = c1 << 4 | c1 & 0xf;
                                        r1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);
                                        g1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                        b1 = (unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                        iVar22 = iVar22 + 2;
                                        *param_1 = (unsigned short)(((((unsigned int)(unsigned char)((int)((int)((unsigned int)bI4i1 * ((param_17 & 0xff) - (param_21 & 0xff))) / 0xff + uVar8) >> 7) << 5 | (unsigned int)r1) << 5 |
                                                            (unsigned int)g1) << 5) | (unsigned int)b1);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        local_64 = local_64 + 1;
                                        local_54 = local_54 + 1;
                                    }
                                    local_64 = local_64 + -8;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        unsigned char bI4i2;
                                        unsigned char r2, g2, b2, a2;
                                        int dA;
                                        if (local_54 >= iVar17) break;
                                        bVar11 = *local_64;
                                        bI4i2 = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                                        r2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);
                                        g2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                        b2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                        dA = (param_17 & 0xff) - (param_21 & 0xff);
                                        uVar8 = param_21 & 0xff;
                                        a2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * dA) / 0xff + uVar8) >> 7);
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        param_1[-1] = (unsigned short)(((((unsigned int)a2 << 5 | (unsigned int)r2) << 5 |
                                                            (unsigned int)g2) << 5) | (unsigned int)b2);
                                        if (iVar22 >= param_2) return;
                                        bI4i2 = bVar11 << 4 | bVar11 & 0xf;
                                        r2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);
                                        g2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                        b2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                        *param_1 = (unsigned short)(((((unsigned int)(a2 = (unsigned char)((int)((int)((unsigned int)bI4i2 * dA) / 0xff + uVar8) >> 7)) << 5 | (unsigned int)r2) << 5 |
                                                            (unsigned int)g2) << 5) | (unsigned int)b2);
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        local_64 = local_64 + 1;
                                        local_54 = local_54 + 1;
                                    }
                                    local_64 = local_64 + 4;
                                }
                            } else {
                                for (local_54 = 0; local_54 < iVar17;) {
                                        unsigned char r3, g3, b3, a3;
        int dA3;
                                    bVar11 = *local_64;
                                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                                    r3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);
                                    g3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                    b3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                    dA3 = (param_17 & 0xff) - (param_21 & 0xff);
                                    uVar8 = param_21 & 0xff;
                                    a3 = (unsigned char)((int)((int)((unsigned int)bI4inten * dA3) / 0xff + uVar8) >> 7);
                                    iVar22 = iVar22 + 2;
                                    param_1 = param_1 + 1;
                                    param_1[-1] = (unsigned short)(((((unsigned int)a3 << 5 | (unsigned int)r3) << 5 |
                                                        (unsigned int)g3) << 5) | (unsigned int)b3);
                                    if (iVar22 >= param_2) return;
                                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                                    r3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_14 & 0xff) - (param_18 & 0xff))) / 0xff + (param_18 & 0xff)) >> 3);
                                    g3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_15 & 0xff) - (param_19 & 0xff))) / 0xff + (param_19 & 0xff)) >> 3);
                                    b3 = (unsigned char)((int)((int)((unsigned int)bI4inten * ((param_16 & 0xff) - (param_20 & 0xff))) / 0xff + (param_20 & 0xff)) >> 3);
                                    a3 = (unsigned char)((int)((int)((unsigned int)bI4inten * dA3) / 0xff + uVar8) >> 7);
                                    iVar22 = iVar22 + 2;
                                    param_1 = param_1 + 1;
                                    param_1[-1] = (unsigned short)(((((unsigned int)a3 << 5 | (unsigned int)r3) << 5 |
                                                          (unsigned int)g3) << 5) | (unsigned int)b3);
                                    if (iVar22 >= param_2) return;
                                    local_64 = local_64 + 1;
                                    local_54 = local_54 + 1;
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = param_1 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_3c = local_3c + 1;
                        } while ((int)local_3c < iVar15);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, local_3c = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            param_4 = (unsigned char *)((unsigned short *)param_4 + iVar5 * -2);
                            puVar2 = (unsigned short *)param_4;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                            }
                            local_3c = local_3c + 1;
                        } while ((int)local_3c < iVar15);
                    }
                } else {
                    /* I4 -> 8-bit direct (two nibbles per byte, expanded in place) */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n3_ctr1;
                    iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_38 = 0;
                    if (0 < iVar15) {
                        do {
                            pbVar12 = param_4;
                            if ((local_38 & param_22) != 0) {
                                for (puVar2 = 0; (int)puVar2 < iVar17;) {
                                    pbVar12 = pbVar12 + 4;
                                    for (n3_ctr1 = 0; n3_ctr1 < 4; n3_ctr1 = n3_ctr1 + 1) {
                                        if ((int)puVar2 >= iVar17) break;
                                        bL0 = *pbVar12;
                                        *(unsigned char *)param_1 = bL0 >> 4 | bL0 & 0xf0;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        *(unsigned char *)param_1 = bL0 << 4 | bL0 & 0xf;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        puVar2 = (unsigned short *)((int)puVar2 + 1);
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (n3_ctr1 = 0; n3_ctr1 < 4; n3_ctr1 = n3_ctr1 + 1) {
                    unsigned char b1;
                                        if ((int)puVar2 >= iVar17) break;
                                        b1 = *pbVar12;
                                        *(unsigned char *)param_1 = b1 & 0xf0 | b1 >> 4;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        *(unsigned char *)param_1 = b1 & 0xf | b1 << 4;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        puVar2 = (unsigned short *)((int)puVar2 + 1);
                                    }
                                    pbVar12 = pbVar12 + 4;
                                }
                            } else {
                                for (puVar2 = 0; (int)puVar2 < iVar17;) {
                                    bVar11 = *pbVar12;
                                    *(unsigned char *)param_1 = bVar11 & 0xf0 | bVar11 >> 4;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    *(unsigned char *)param_1 = bVar11 << 4 | bVar11 & 0xf;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    pbVar12 = pbVar12 + 1;
                                    puVar2 = (unsigned short *)((int)puVar2 + 1);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)param_1 + -1), 0 < iVar17)) {
                                do {
                                    *(unsigned char *)param_1 = *(unsigned char *)puVar9;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    *(unsigned char *)param_1 = *(unsigned char *)puVar9;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_38 = local_38 + 1;
                        } while ((int)local_38 < iVar15);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, local_38 = 0, 0 < iVar15)) {
                        do {
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            param_4 = (unsigned char *)((unsigned short *)param_4 + -iVar16);
                            puVar2 = (unsigned short *)param_4;
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                *(unsigned char *)param_1 = *(unsigned char *)puVar2;
                                param_1 = (unsigned short *)((int)param_1 + 1);
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= param_2) return;
                                *(unsigned char *)param_1 = *(unsigned char *)puVar2;
                                param_1 = (unsigned short *)((int)param_1 + 1);
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= param_2) return;
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
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n4_ctr1;
                iVar15 = 1 << *(int *)(iVar5 + 0x20);
                ci8_row = 0;
                iVar17 = 1 << *(int *)(iVar5 + 0x24);
                if (0 < iVar17) {
                    do {
                        pbVar12 = param_4;
                        if ((param_22 & ci8_row) != 0) {
                                    n4_ctr1 = 0;
                            if (0 < iVar15) {
                                do {
                                    pbVar12 = pbVar12 + 4;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        if (n4_ctr1 >= iVar15) break;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        n4_ctr1 = n4_ctr1 + 1;
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        if (n4_ctr1 >= iVar15) break;
                                        *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                        iVar22 = iVar22 + 2;
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        n4_ctr1 = n4_ctr1 + 1;
                                    }
                                    pbVar12 = pbVar12 + 4;
                                } while (n4_ctr1 < iVar15);
                            }
                        } else {
                            n4_ctr1 = 0;
                                    if (0 < iVar15) {
                                do {
                                    *param_1 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                                    iVar22 = iVar22 + 2;
                                    param_1 = param_1 + 1;
                                    if (iVar22 >= param_2) return;
                                    pbVar12 = pbVar12 + 1;
                                    n4_ctr1 = n4_ctr1 + 1;
                                } while (n4_ctr1 < iVar15);
                            }
                        }
                        if ((param_7 != 0) && (iVar16 = 0, puVar9 = param_1 + -1, 0 < iVar15)) {
                            do {
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar9;
                                param_1 = param_1 + 1;
                                puVar9 = puVar9 + -1;
                                if (iVar22 >= param_2) return;
                                iVar16 = iVar16 + 1;
                            } while (iVar16 < iVar15);
                        }
                        param_4 = param_4 + *(int *)(iVar5 + 8);
                        ci8_row = ci8_row + 1;
                    } while ((int)ci8_row < iVar17);
                }
                if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, ci8_row = 0, 0 < iVar17)) {
                    do {
                        iVar5 = (param_7 != 0) ? iVar15 * 2 : iVar15;
                        param_4 = (unsigned char *)((unsigned short *)param_4 + -iVar5);
                        puVar2 = (unsigned short *)param_4;
                        iVar5 = (param_7 != 0) ? iVar15 * 2 : iVar15;
                        for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                            iVar22 = iVar22 + 2;
                            *param_1 = *puVar2;
                            param_1 = param_1 + 1;
                            puVar2 = puVar2 + 1;
                            if (iVar22 >= param_2) return;
                        }
                        ci8_row = ci8_row + 1;
                    } while ((int)ci8_row < iVar17);
                }
            } else if (param_6 == 3) {
                if (param_13 == 1) {
                    /* IA8 blend: 8-bit source = 4-bit intensity + 4-bit alpha,
                     * intensity blended between two RGB endpoints, alpha in low nibble */
                    int n5_ctr1;
                    iVar17 = 1 << *(int *)(iVar5 + 0x20);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    local_44 = 0;
                    if (0 < iVar15) {
                        do {
                            param_9 = (int)param_4;
                            if ((local_44 & param_22) != 0) {
                                for (n5_ctr1 = 0; n5_ctr1 < iVar17;) {
                                    param_9 = param_9 + 4;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                                        if (n5_ctr1 >= iVar17) break;
                                        iVar22 = iVar22 + 2;
                                        b690 = *(unsigned char *)param_9;
                                        uVar14 = ((unsigned int)b690 >> 4) | ((unsigned int)b690 & 0xf0);
                                        loIA8 = (unsigned int)b690 & 0xf;
                                        ia_r = (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff + (param_18 & 0xff) >> 4;
                                        ia_g = (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff + (param_19 & 0xff) >> 4;
                                        ia_b = (((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff + (param_20 & 0xff) >> 4;
                                        *param_1 = (unsigned short)((((loIA8 << 4 | ia_r) << 4 | ia_g) << 4) | ia_b);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        param_9 = param_9 + 1;
                                        n5_ctr1 = n5_ctr1 + 1;
                                    }
                                    param_9 = param_9 + -8;
                                    for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    unsigned int ia_g2;
                                        if (n5_ctr1 >= iVar17) break;
                                        iVar22 = iVar22 + 2;
                                        b709 = *(unsigned char *)param_9;
                                        uVar14 = ((unsigned int)b709 >> 4) | ((unsigned int)b709 & 0xf0);
                                        loIA8 = (unsigned int)b709 & 0xf;
                                        ia_r = (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff + (param_18 & 0xff) >> 4;
                                        ia_g2 = (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff + (param_19 & 0xff) >> 4;
                                        ia_b = (((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff + (param_20 & 0xff) >> 4;
                                        *param_1 = (unsigned short)((((loIA8 << 4 | ia_r) << 4 | ia_g2) << 4) | ia_b);
                                        param_1 = param_1 + 1;
                                        if (iVar22 >= param_2) return;
                                        param_9 = param_9 + 1;
                                        n5_ctr1 = n5_ctr1 + 1;
                                    }
                                    param_9 = param_9 + 4;
                                }
                            } else {
                                for (n5_ctr1 = 0; n5_ctr1 < iVar17;) {
                    unsigned int ia_g3;
                                    iVar22 = iVar22 + 2;
                                    b731 = *(unsigned char *)param_9;
                                    uVar14 = b731;
                                    uVar14 = uVar14 >> 4 | uVar14 & 0xf0;
                                    loIA8 = (unsigned int)b731 & 0xf;
                                    ia_r = (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff + (param_18 & 0xff) >> 4;
                                    ia_g3 = (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff + (param_19 & 0xff) >> 4;
                                    ia_b = (((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff + (param_20 & 0xff) >> 4;
                                    *param_1 = (unsigned short)((((loIA8 << 4 | ia_r) << 4 | ia_g3) << 4) | ia_b);
                                    param_1 = param_1 + 1;
                                    if (iVar22 >= param_2) return;
                                    param_9 = param_9 + 1;
                                    n5_ctr1 = n5_ctr1 + 1;
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = param_1 + -1, 0 < iVar17)) {
                                do {
                                    iVar22 = iVar22 + 2;
                                    *param_1 = *puVar9;
                                    param_1 = param_1 + 1;
                                    puVar9 = puVar9 + -1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, local_44 = 0, 0 < iVar15)) {
                        do {
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            param_4 = (unsigned char *)((unsigned short *)param_4 + -iVar5);
                            puVar2 = (unsigned short *)param_4;
                            iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                                iVar22 = iVar22 + 2;
                                *param_1 = *puVar2;
                                param_1 = param_1 + 1;
                                puVar2 = puVar2 + 1;
                                if (iVar22 >= param_2) return;
                            }
                            local_44 = local_44 + 1;
                        } while ((int)local_44 < iVar15);
                    }
                } else {
                    /* IA4 -> 8-bit nibble-swap direct copy */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n6_ctr1;
                    iVar17 = 1 << *(int *)(iVar5 + 0x20);
                    iVar15 = 1 << *(int *)(iVar5 + 0x24);
                    n6_ctr1 = 0;
                    if (0 < iVar15) {
                        do {
                            pbVar12 = param_4;
                            if (((unsigned int)n6_ctr1 & param_22) != 0) {
                                iVar16 = 0;
                                if (0 < iVar17) {
                                    do {
                                        pbVar12 = pbVar12 + 4;
                                        for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                    unsigned char b0;
                                            if (iVar16 >= iVar17) break;
                                            b0 = *pbVar12;
                                            *(unsigned char *)param_1 = b0 << 4 | b0 >> 4;
                                            param_1 = (unsigned short *)((int)param_1 + 1);
                                            iVar22 = iVar22 + 1;
                                            if (iVar22 >= param_2) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + -8;
                                        for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                                            if (iVar16 >= iVar17) break;
                                            bVar11 = *pbVar12;
                                            *(unsigned char *)param_1 = bVar11 >> 4 | bVar11 << 4;
                                            param_1 = (unsigned short *)((int)param_1 + 1);
                                            iVar22 = iVar22 + 1;
                                            if (iVar22 >= param_2) return;
                                            pbVar12 = pbVar12 + 1;
                                            iVar16 = iVar16 + 1;
                                        }
                                        pbVar12 = pbVar12 + 4;
                                    } while (iVar16 < iVar17);
                                }
                            } else {
                                iVar16 = 0;
                                if (0 < iVar17) {
                                    do {
                                        bVar11 = *pbVar12;
                                        *(unsigned char *)param_1 = bVar11 >> 4 | bVar11 << 4;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    } while (iVar16 < iVar17);
                                }
                            }
                            if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)param_1 + -1), 0 < iVar17)) {
                                do {
                                    *(unsigned char *)param_1 = *(unsigned char *)puVar9;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    puVar9 = (unsigned short *)((int)puVar9 + -1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < iVar17);
                            }
                            param_4 = param_4 + *(int *)(iVar5 + 8);
                            n6_ctr1 = n6_ctr1 + 1;
                        } while (n6_ctr1 < iVar15);
                    }
                    if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, n6_ctr1 = 0, 0 < iVar15)) {
                        do {
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            param_4 = param_4 - iVar16;
                            puVar2 = (unsigned short *)param_4;
                            iVar16 = (param_7 != 0) ? iVar17 * 2 : iVar17;
                            for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                                *(unsigned char *)param_1 = (unsigned char)*puVar2;
                                param_1 = (unsigned short *)((int)param_1 + 1);
                                puVar2 = (unsigned short *)((int)puVar2 + 1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= param_2) return;
                            }
                            n6_ctr1 = n6_ctr1 + 1;
                        } while (n6_ctr1 < iVar15);
                    }
                }
            } else if (param_6 == 4) {
                /* I8 -> 8-bit direct copy */
                {
                    int h, x, i;
                    unsigned char *p, *q;
                    
                    param_9 = 1 << *(int *)(iVar5 + 0x20);
                    h = 1 << *(int *)(iVar5 + 0x24);
                    for (ctr1 = 0; ctr1 < h; ctr1++) {
                        p = param_4;
                        if (ctr1 & param_22) {
                            for (x = 0; x < param_9; ) {
                                p += 4;
                                for (i = 0; i < 4; i++) {
                                    if (x >= param_9) break;
                                    *(unsigned char *)param_1 = *p;
                                    param_1 = (unsigned short *)((unsigned char *)param_1 + 1);
                                    iVar22++;
                                    if (iVar22 >= param_2) return;
                                    p++;
                                    x++;
                                }
                                p -= 8;
                                for (i = 0; i < 4; i++) {
                                    if (x >= param_9) break;
                                    *(unsigned char *)param_1 = *p;
                                    param_1 = (unsigned short *)((unsigned char *)param_1 + 1);
                                    iVar22++;
                                    if (iVar22 >= param_2) return;
                                    p++;
                                    x++;
                                }
                                p += 4;
                            }
                        } else {
                            for (x = 0; x < param_9; x++) {
                                *(unsigned char *)param_1 = *p;
                                param_1 = (unsigned short *)((unsigned char *)param_1 + 1);
                                iVar22++;
                                if (iVar22 >= param_2) return;
                                p++;
                            }
                        }
                        if (param_7) {
                            q = (unsigned char *)param_1 - 1;
                            for (x = 0; x < param_9; x++) {
                                *(unsigned char *)param_1 = *q;
                                param_1 = (unsigned short *)((unsigned char *)param_1 + 1);
                                q--;
                                iVar22++;
                                if (iVar22 >= param_2) return;
                            }
                        }
                        param_4 += *(int *)(iVar5 + 8);
                    }
                    if (param_8) {
                        param_4 = (unsigned char *)param_1;
                        for (ctr1 = 0; ctr1 < h; ctr1++) {
                            iVar5 = param_7 ? param_9 * 2 : param_9;
                            param_4 -= iVar5;
                            q = param_4;
                            for (iVar5 = param_7 ? param_9 * 2 : param_9; iVar5 > 0; iVar5--) {
                                *(unsigned char *)param_1 = *q;
                                param_1 = (unsigned short *)((unsigned char *)param_1 + 1);
                                q++;
                                iVar22++;
                                if (iVar22 >= param_2) return;
                            }
                        }
                    }
                }
            } else if (param_6 == 4) {
                /* I8 -> 8-bit direct copy */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; unsigned int local_44; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2; int n7_ctr1;
                param_9 = 1 << *(int *)(iVar5 + 0x20);
                iVar15 = 1 << *(int *)(iVar5 + 0x24);
                n7_ctr1 = 0;
                if (0 < iVar15) {
                    do {
                        pbVar12 = param_4;
                        if ((param_22 & (unsigned int)n7_ctr1) != 0) {
                            iVar16 = 0;
                            if (0 < param_9) {
                                do {
                                    pbVar12 = pbVar12 + 4;
                                    for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                                        if (iVar16 >= param_9) break;
                                        *(unsigned char *)param_1 = *pbVar12;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
                                        pbVar12 = pbVar12 + 1;
                                        iVar16 = iVar16 + 1;
                                    }
                                    pbVar12 = pbVar12 + -8;
                                    for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                                        if (iVar16 >= param_9) break;
                                        *(unsigned char *)param_1 = *pbVar12;
                                        param_1 = (unsigned short *)((int)param_1 + 1);
                                        iVar22 = iVar22 + 1;
                                        if (iVar22 >= param_2) return;
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
                                    *(unsigned char *)param_1 = *pbVar12;
                                    param_1 = (unsigned short *)((int)param_1 + 1);
                                    iVar22 = iVar22 + 1;
                                    if (iVar22 >= param_2) return;
                                    pbVar12 = pbVar12 + 1;
                                    iVar16 = iVar16 + 1;
                                } while (iVar16 < param_9);
                            }
                        }
                        if ((param_7 != 0) && (local_24 = 0, puVar9 = (unsigned short *)((int)param_1 + -1), 0 < param_9)) {
                            do {
                                *(unsigned char *)param_1 = *(unsigned char *)puVar9;
                                param_1 = (unsigned short *)((int)param_1 + 1);
                                puVar9 = (unsigned short *)((int)puVar9 + -1);
                                iVar22 = iVar22 + 1;
                                if (iVar22 >= param_2) return;
                                local_24 = local_24 + 1;
                            } while (local_24 < param_9);
                        }
                        param_4 = param_4 + *(int *)(iVar5 + 8);
                        n7_ctr1 = n7_ctr1 + 1;
                    } while (n7_ctr1 < iVar15);
                }
                if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, n7_ctr1 = 0, 0 < iVar15)) {
                    do {
                        iVar5 = (param_7 != 0) ? param_9 * 2 : param_9;
                        param_4 = param_4 - iVar5;
                        puVar2 = (unsigned short *)param_4;
                        iVar5 = (param_7 != 0) ? param_9 * 2 : param_9;
                        for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                            *(unsigned char *)param_1 = (unsigned char)*puVar2;
                            param_1 = (unsigned short *)((int)param_1 + 1);
                            iVar22 = iVar22 + 1;
                            if (iVar22 >= param_2) return;
                            puVar2 = (unsigned short *)((int)puVar2 + 1);
                        }
                        n7_ctr1 = n7_ctr1 + 1;
                    } while (n7_ctr1 < iVar15);
                }
            }
        } else if ((param_3 == 2) && (param_6 == 0)) {
            /* ---- mode 2: raw 16-bit blit through FUN_100271f0 ---- */
                    int iVar15; int iVar17; int iVar16; int iVar13; int iVar18; int iVar20; unsigned int uVar6; unsigned int uVar7; unsigned int uVar8; unsigned int uVar14; unsigned int uVar19; unsigned int local_38; unsigned int local_3c; int local_24; int local_34; int local_54; int lo0; int loIA8; unsigned char *local_64; unsigned char *pbVar12; unsigned short *puVar9; unsigned short *puVar2; unsigned char bVar11; unsigned char chR; unsigned char chG; unsigned char chB; unsigned char chA; unsigned char bI4inten; unsigned int ia_r; unsigned int ia_g; unsigned int ia_b; unsigned int uW2;
            uVar14 = 1 << *(int *)(iVar5 + 0x20);
            rgba_row = 0;
            iVar15 = 1 << *(int *)(iVar5 + 0x24);
            if (0 < iVar15) {
                do {
                    param_9 = 0;
                    puVar9 = param_1;
                    iVar17 = iVar22;
                    pbVar12 = param_4;
                    if ((param_22 & rgba_row) != 0) {
                        if (0 < (int)uVar14) {
                            while (1) {
                                *param_1 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                                param_1 = param_1 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= param_2) return;
                                *param_1 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                                param_1 = param_1 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= param_2) return;
                                *param_1 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                                param_1 = param_1 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= param_2) return;
                                *param_1 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                                param_1 = param_1 + 1;
                                pbVar12 = pbVar12 + 2;
                                iVar22 = iVar22 + 2;
                                param_9 = param_9 + 1;
                                if (param_9 >= (int)uVar14) break;
                                if (iVar22 >= param_2) return;
                            }
                            puVar9 = param_1;
                            iVar17 = iVar22;
                        }
                    } else {
                        if (0 < (int)uVar14) {
                            do {
                                *param_1 = FUN_100271f0(*(unsigned short *)pbVar12);
                                iVar22 = iVar22 + 2;
                                param_1 = param_1 + 1;
                                pbVar12 = pbVar12 + 2;
                                if (iVar22 >= param_2) return;
                                param_9 = param_9 + 1;
                                puVar9 = param_1;
                                iVar17 = iVar22;
                            } while (param_9 < (int)uVar14);
                        }
                    }
                    iVar22 = iVar17;
                    param_1 = puVar9;
                    if ((param_7 != 0) && (iVar17 = 0, puVar9 = param_1 + -1, 0 < (int)uVar14)) {
                        do {
                            iVar22 = iVar22 + 2;
                            *param_1 = *puVar9;
                            param_1 = param_1 + 1;
                            puVar9 = puVar9 + -1;
                            if (iVar22 >= param_2) return;
                            iVar17 = iVar17 + 1;
                        } while (iVar17 < (int)uVar14);
                    }
                    param_4 = param_4 + *(int *)(iVar5 + 8);
                    rgba_row = rgba_row + 1;
                } while ((int)rgba_row < iVar15);
            }
            if ((param_8 != 0) && (param_4 = (unsigned char *)param_1, rgba_row = 0, 0 < iVar15)) {
                do {
                    uVar6 = (param_7 != 0) ? uVar14 * 2 : uVar14;
                    param_4 = (unsigned char *)((unsigned short *)param_4 + -(int)uVar6);
                    puVar2 = (unsigned short *)param_4;
                    uVar6 = (param_7 != 0) ? uVar14 * 2 : uVar14;
                    for (; 0 < (int)uVar6; uVar6 = uVar6 - 1) {
                        iVar22 = iVar22 + 2;
                        *param_1 = *puVar2;
                        param_1 = param_1 + 1;
                        puVar2 = puVar2 + 1;
                        if (iVar22 >= param_2) return;
                    }
                    rgba_row = rgba_row + 1;
                } while ((int)rgba_row < iVar15);
            }
        }
    }
}

