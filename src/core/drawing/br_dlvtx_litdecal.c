/* br_dlvtx_litdecal.c -- drawing: lit vertex transform handler, DECAL row.
 *
 * ONE function, 0x100221D0 (BrDlVtxLitDecal), the G_VTX handler 0x1001E8FB
 * installs in place of the lit handler 0x10021C70 when the DECAL combiner row
 * is selected.  Same light cache and transform as its siblings
 * (br_dlvtx_texgen.c), with the per-vertex lighting open-coded.
 *
 * Filed on its own, like its siblings: br_dl.c's TU context carries
 * byte-exact functions whose x87 scheduling moves when a body is added there.
 */
#include <stdint.h>
#include "br_dl.h"

#ifdef BR_MATCHING_BUILD

/* MVP matrix, 4x4 row-major at 0x105D1760. */
extern float DAT_105d1760, DAT_105d1764, DAT_105d1768, DAT_105d176c;
extern float DAT_105d1790, DAT_105d1794, DAT_105d1798, DAT_105d179c;
#define DAT_105d1770 (*(float *)0x105d1770)
#define DAT_105d1774 (*(float *)0x105d1774)
#define DAT_105d1778 (*(float *)0x105d1778)
#define DAT_105d177c (*(float *)0x105d177c)
#define DAT_105d1780 (*(float *)0x105d1780)
#define DAT_105d1784 (*(float *)0x105d1784)
#define DAT_105d1788 (*(float *)0x105d1788)
#define DAT_105d178c (*(float *)0x105d178c)

extern int DAT_105d17d0;          /* fLightCached */
extern int DAT_105ccfd0;          /* nLights */
extern int DAT_100a9a50;          /* iModel */
typedef struct { float m[16]; } BrDlMtx;
extern BrDlMtx DAT_105ccd10[];    /* model matrices, 0x40 each */

/* The two N64 Light records at 0x105CCC78 (directional) and 0x105CCC88
 * (ambient): colour bytes, their copy, signed direction bytes. */
typedef struct {
    unsigned char col[4];
    unsigned char colc[4];
    signed char   dir[4];
    unsigned char pad[4];
} BrDlLight;
extern BrDlLight DAT_105ccc78[2];

extern float DAT_105ce210, DAT_105ce214, DAT_105ce218;  /* lightScale[3] */
extern float DAT_105ce21c, DAT_105ce220, DAT_105ce224;  /* lightDir[3] */
extern float DAT_105ce228, DAT_105ce22c, DAT_105ce230;  /* lightAmb[3] */
extern float DAT_105d17a4, DAT_105d17b4, DAT_105ce2d0;  /* unlit colour */
#define DAT_105ccd44 (*(float *)0x105ccd44)
#define DAT_105cd9f4 (*(float *)0x105cd9f4)
#define DAT_105cccf8 (*(float *)0x105cccf8)

extern BrDlVtx DAT_105ce318[];

extern float DAT_10077420;        /* 128.0f */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtxL;

extern void    FUN_100344D0(void *);
extern int32_t FUN_10022120(void *);
extern void    FUN_10022070(void *, void *, float, float, float);

/* T2 2026-09-24 (hand transcription from the asm), compiled like the rest of
 * its original TU with /O2 /Op: 1055/1059 B, 296/300 insns, 84 differing
 * lines in a straight instruction-stream diff (~100 compiles).  Spellings
 * that moved bytes (all measured):
 *  - the source vertex is WALKED (`pSrc++`), not indexed: `pSrc[i]` makes VC5
 *    bias the induction pointer by +4 and reorders every transform row;
 *  - the back-facing test is `t >= 0.0f` with the lit arm first;
 *  - the three colour-scale globals are absolute-address derefs: that is the
 *    operand kind that takes the fld side over the vertex field;
 *  - the direction bytes are `int` temps read before the colour stores and
 *    cast at the use; float temps store the colours before the loads.
 * Open (same class as 0x10022600, family-wide):
 *  - the light setup: the original fild's all five light values before the
 *    first store and reads the x direction byte late; and `lea esi,[eax+
 *    matrices]` where every spelling gives `add`;
 *  - the x term of each transform row: the original loads the vertex x and
 *    multiplies by the x column from memory; ours loads the column.  Dead:
 *    operand order, row order x association (24 variants), declaration order
 *    (locals and externs), pad count, pSrc[0]/float-pointer/array spellings,
 *    a pointer copy of the column, per-iteration `base + i`;
 *  - `mov ecx,0` for the original's `xor ecx,ecx` before the index byte. */
/* WHAT IT DOES: loads a batch of vertices for a decal-lit surface.  It first
 * refreshes the cached light (colour, direction pulled into model space and
 * normalised, ambient) if anything has invalidated it, then for each vertex:
 * transforms the position by the combined matrix, copies the texture
 * coordinates, shades it (ambient plus the light's share by how squarely the
 * normal faces it, capped at 255; back-facing gets ambient, no lights gets the
 * flat primitive colour), scales the colour, and computes the clip codes,
 * projecting the vertex straight away when it is fully on screen. */
/* @implements 0x100221D0 glide BrDlVtxLitDecal */
const uint8_t *BrDlVtxLitDecal(const uint8_t *p)
{
    float *m;
    int dx, dy, dz;
    uint32_t w0;
    const BrDlSrcVtxL *pSrc;
    int v0;
    int n;
    BrDlVtx *pV;
    BrDlVtx *pVc;
    int i;
    float t, v;
    int32_t oc;

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            if (DAT_100a9a50 != 0)
                m = DAT_105ccd10[DAT_100a9a50].m;
            else
                m = NULL;

            dx = DAT_105ccc78[0].dir[0];
            dz = DAT_105ccc78[0].dir[2];
            dy = DAT_105ccc78[0].dir[1];

            DAT_105ce210 = (float)DAT_105ccc78[0].col[0];
            DAT_105ce214 = (float)DAT_105ccc78[0].col[1];
            DAT_105ce218 = (float)DAT_105ccc78[0].col[2];


            DAT_105ce21c = ((m[1] * (float)dy + m[2] * (float)dz) + m[0] * (float)dx) / DAT_10077420;
            DAT_105ce220 = ((m[4] * (float)dx + m[6] * (float)dz) + m[5] * (float)dy) / DAT_10077420;
            DAT_105ce224 = ((m[8] * (float)dx + m[10] * (float)dz) + m[9] * (float)dy) / DAT_10077420;

            FUN_100344D0(&DAT_105ce21c);

            DAT_105ce228 = (float)DAT_105ccc78[1].col[0];
            DAT_105ce22c = (float)DAT_105ccc78[1].col[1];
            DAT_105ce230 = (float)DAT_105ccc78[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = *(const BrDlSrcVtxL **)(p + 4);
    v0 = (w0 >> 16) & 0xFF;
    n  = (w0 >> 10) & 0x3F;
    pV = &DAT_105ce318[v0];

    pVc = pV;
    for (i = 0; i < n; i++) {
        pV[i].cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;
        pV[i].s = pSrc->s;
        pV[i].t = pSrc->t;

        if (DAT_105ccfd0 != 0) {
            t = (pSrc->n1 * DAT_105ce220 + pSrc->n0 * DAT_105ce21c) + pSrc->n2 * DAT_105ce224;
            if (t >= 0.0f) {
                v = t * DAT_105ce210 + DAT_105ce228;
                pV[i].n0 = v > 255.0f ? 255.0f : v;
                v = t * DAT_105ce214 + DAT_105ce22c;
                pV[i].n1 = v > 255.0f ? 255.0f : v;
                v = t * DAT_105ce218 + DAT_105ce230;
                pV[i].n2 = v > 255.0f ? 255.0f : v;
            } else {
                pV[i].n0 = DAT_105ce228;
                pV[i].n1 = DAT_105ce22c;
                pV[i].n2 = DAT_105ce230;
            }
        } else {
            pV[i].n0 = DAT_105d17a4;
            pV[i].n1 = DAT_105d17b4;
            pV[i].n2 = DAT_105ce2d0;
        }
        pV[i].n0 = DAT_105ccd44 * pV[i].n0;
        pV[i].n1 = DAT_105cd9f4 * pV[i].n1;
        pV[i].n2 = DAT_105cccf8 * pV[i].n2;

        oc = FUN_10022120(&pV[i].f40);
        pV[i].outcode = oc;
        if (oc == 0)
            FUN_10022070(pVc, &pV[i].f40, pV[i].n0, pV[i].n1, pV[i].n2);
        pVc++;
        pSrc++;
    }
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
