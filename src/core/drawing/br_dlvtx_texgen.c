/* br_dlvtx_texgen.c -- drawing: lit + texgen vertex transform handler.
 *
 * ONE function, 0x10022600 (BrDlVtxGen), the TEXTURE_GEN (not _LINEAR)
 * vertex handler.  The sibling of 0x10022BF0 BrDlVtxGenLin (br_dlvtx_gen.c):
 * same light cache, same transform, same three per-vertex helpers; the
 * texture coordinates are a straight linear projection of the world-space
 * normal on the two lookat directions, with no asin.
 *
 * Filed on its own for the same reason as its sibling: br_dl.c's TU context
 * carries eleven byte-exact functions whose x87 scheduling moves when a body
 * is added there.
 */
#include <stdint.h>
#include "br_dl.h"
#include "br_vec.h"

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

extern uint8_t *DAT_105ce2d8;     /* lookat ptr 1 */
extern uint8_t *DAT_105ce2dc;     /* lookat ptr 2 */

extern BrDlVtx DAT_105ce318[];

extern float DAT_10077420;        /* 128.0f */
extern float DAT_10077424;        /* texgen offset */

/* Glide texture globals (outside BRGlide, in the Glide library). */
extern int DAT_1186c958;          /* tex dim A (fild) */
extern int DAT_118ed198;          /* tex offset B (fild) */
extern float DAT_118ed1a4;        /* tex scale A (fdiv) */
extern int DAT_118ed1ac;          /* tex dim C (fild) */
extern int DAT_1186c950;          /* tex offset D (fild) */
extern float DAT_118ed1a8;        /* tex scale B (fdiv) */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtxT;

extern void    FUN_100344D0(void *);
extern void    FUN_10022AC0(const void *, void *);
extern int32_t FUN_10022120(void *);
extern void    FUN_10022070(void *, void *, float, float, float);

/* T2 2026-09-24 (hand transcription from the asm), compiled like the rest of
 * its original TU with /O2 /Op: 1200/1212 B, register-blind multiset 14+17.
 * Spellings that each moved a whole class (all measured):
 *  - the lights are N64 Light records read as BYTES: VC5 merges col[0]/col[1]
 *    into one dword load + `and`/`mov dl,ah`, exactly as the original;
 *  - x87 operand roles follow VC5's operand-kind ladder, not source order:
 *    the z/y MVP columns are absolute-address derefs (they take the fld
 *    side over the vertex fields), the x/w columns stay extern symbols, and
 *    x is read through the INDEXED struct pointer so it beats the x column;
 *  - the source vertex is walked through two pointers (the struct base and
 *    `pn = &n1`), giving the original's ebx / esi = ebx+0x18 pair;
 *  - the model-matrix pointer is taken first, then colours, then direction.
 * Open: `lea esi,[eax+base]` vs our direct esi; m[0]*n0 operand role; the
 * texgen tail's stack layout (frame 0x1c vs 0x28). */
/* WHAT IT DOES: transforms a batch of vertices through the combined matrix,
 * generates texture coordinates by rotating each normal into world space and
 * projecting it on the two view-direction vectors (a straight linear map to
 * the current texture's size), then lights and projects each vertex. */
/* @implements 0x10022600 glide BrDlVtxGen */
const uint8_t *BrDlVtxGen(const uint8_t *p)
{
    float *m;
    float dx, dy, dz;
    uint32_t w0;
    const BrDlSrcVtxT *pSrc;
    const float *pn;
    int v0;
    int n;
    BrDlVtx *pV;
    int i;
    BrVec3 td;
    const uint8_t *look1, *look2;
    float lx0, lx1, lx2, ly0, ly1, ly2;
    float dotX_128;
    float texDimA, texOffB, texDimC, texOffD;
    int32_t oc;
    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            if (DAT_100a9a50 != 0)
                m = DAT_105ccd10[DAT_100a9a50].m;
            else
                m = NULL;

            DAT_105ce210 = (float)DAT_105ccc78[0].col[0];
            DAT_105ce214 = (float)DAT_105ccc78[0].col[1];
            DAT_105ce218 = (float)DAT_105ccc78[0].col[2];

            dx = (float)DAT_105ccc78[0].dir[0];
            dy = (float)DAT_105ccc78[0].dir[1];
            dz = (float)DAT_105ccc78[0].dir[2];

            DAT_105ce21c = ((m[1] * dy + m[2] * dz) + m[0] * dx) / DAT_10077420;
            DAT_105ce220 = ((m[4] * dx + m[6] * dz) + m[5] * dy) / DAT_10077420;
            DAT_105ce224 = ((m[8] * dx + m[10] * dz) + m[9] * dy) / DAT_10077420;

            FUN_100344D0(&DAT_105ce21c);

            DAT_105ce228 = (float)DAT_105ccc78[1].col[0];
            DAT_105ce22c = (float)DAT_105ccc78[1].col[1];
            DAT_105ce230 = (float)DAT_105ccc78[1].col[2];
        }
        DAT_105d17d0 = 1;
    }

    w0 = *(const uint32_t *)p;
    pSrc = *(const BrDlSrcVtxT **)(p + 4);
    v0 = (w0 >> 16) & 0xFF;
    n  = (w0 >> 10) & 0x3F;
    pV = &DAT_105ce318[v0];

    pn = &pSrc->n1;
    for (i = 0; i < n; i++) {
        pV[i].cx = DAT_105d1780 * pn[-4] + DAT_105d1770 * pn[-5] + pSrc[i].x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pn[-4] + DAT_105d1774 * pn[-5] + pSrc[i].x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pn[-4] + DAT_105d1778 * pn[-5] + pSrc[i].x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pn[-4] + DAT_105d177c * pn[-5] + pSrc[i].x * DAT_105d176c + DAT_105d179c;

        if (DAT_100a9a50 != 0)
            m = DAT_105ccd10[DAT_100a9a50].m;
        else
            m = NULL;

        td.x = (m[0] * pn[-1] + m[8] * pn[1]) + m[4] * pn[0];
        td.y = (m[1] * pn[-1] + m[9] * pn[1]) + m[5] * pn[0];
        td.z = (m[2] * pn[-1] + m[10] * pn[1]) + m[6] * pn[0];

        FUN_100344D0(&td);

        look1 = DAT_105ce2d8 + 8;
        look2 = DAT_105ce2dc + 8;

        texOffB = (float)DAT_118ed198;
        texDimA = (float)DAT_1186c958;

        ly2 = (float)(int)(int8_t)look2[2];
        ly0 = (float)(int)(int8_t)look2[0];
        ly1 = (float)(int)(int8_t)look2[1];

        lx2 = (float)(int)(int8_t)look1[2];
        lx0 = (float)(int)(int8_t)look1[0];
        lx1 = (float)(int)(int8_t)look1[1];

        pV[i].s = (((td.x * ly0 + ly2 * td.z) + ly1 * td.y) / DAT_10077420 * texDimA
                   - DAT_10077424 - texOffB) / DAT_118ed1a4;

        dotX_128 = ((td.x * lx0 + lx2 * td.z) + lx1 * td.y) / DAT_10077420;
        texDimC = (float)DAT_118ed1ac;
        texOffD = (float)DAT_1186c950;
        pV[i].t = (dotX_128 * texDimC - DAT_10077424 - texOffD) / DAT_118ed1a8;

        FUN_10022AC0(&pSrc[i], &pV[i].f40);

        oc = FUN_10022120(&pV[i].f40);
        pV[i].outcode = oc;

        if (oc == 0) {
            FUN_10022070(&pV[i], &pV[i].f40, pV[i].n0, pV[i].n1, pV[i].n2);
        }
        pn += 8;
    }
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
