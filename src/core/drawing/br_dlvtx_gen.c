/* br_dlvtx_gen.c -- drawing: lit + linear-texgen vertex transform handler.
 *
 * ONE function, 0x10022BF0 (BrDlVtxGenLin), the sphere-map vertex handler.
 * Dispatch table slot for TEXGEN+LIN geometry mode.  Caches light state,
 * transforms each vertex through the MVP, computes sphere-map texture
 * coordinates by rotating the source normal into world space and dotting
 * with two lookat directions (one through asin for latitude), then calls
 * the three per-vertex helpers (light, outcode, project).
 *
 * Filed separately because adding the body to br_dl.c changes the TU
 * context and shifts x87 scheduling for 11 other byte-exact functions.
 */
#include <stdint.h>
#include "br_dl.h"
#include "br_vec.h"

#ifdef BR_MATCHING_BUILD

/* MVP matrix, 4x4 row-major at 0x105D1760. */
extern float DAT_105d1760, DAT_105d1764, DAT_105d1768, DAT_105d176c;
extern float DAT_105d1770, DAT_105d1774, DAT_105d1778, DAT_105d177c;
extern float DAT_105d1780, DAT_105d1784, DAT_105d1788, DAT_105d178c;
extern float DAT_105d1790, DAT_105d1794, DAT_105d1798, DAT_105d179c;

extern int DAT_105d17d0;          /* fLightCached */
extern int DAT_105ccfd0;          /* nLights */
extern int DAT_100a9a50;          /* iModel */
extern float DAT_105ccd10;        /* aModel base, stride 0x40 */

extern uint32_t DAT_105ccc78;     /* aLight[0] packed colour dword */
extern uint8_t DAT_105ccc7a;      /* aLight[0].col[2] */
extern int8_t DAT_105ccc80;       /* aLight[0].dir[0] signed */
extern int8_t DAT_105ccc81;       /* aLight[0].dir[1] */
extern int8_t DAT_105ccc82;       /* aLight[0].dir[2] */
extern uint32_t DAT_105ccc88;     /* aLight[1] packed colour dword */
extern uint8_t DAT_105ccc8a;      /* aLight[1].col[2] */

extern float DAT_105ce210, DAT_105ce214, DAT_105ce218;  /* lightScale[3] */
extern float DAT_105ce21c, DAT_105ce220, DAT_105ce224;  /* lightDir[3] */
extern float DAT_105ce228, DAT_105ce22c, DAT_105ce230;  /* lightAmb[3] */

extern uint8_t *DAT_105ce2d8;     /* lookat ptr 1 */
extern uint8_t *DAT_105ce2dc;     /* lookat ptr 2 */

extern BrDlVtx DAT_105ce318[];

extern float DAT_10077420;        /* 128.0f */
extern float DAT_10077424;        /* sphere map offset */
extern float DAT_10077428;        /* pi */
extern float DAT_1007742c;        /* sphere map scale */

/* Glide texture globals (outside BRGlide, in the Glide library). */
extern int DAT_1186c958;          /* tex dim A (fild) */
extern int DAT_118ed198;          /* tex offset B (fild) */
extern float DAT_118ed1a4;        /* tex scale A (fdiv) */
extern int DAT_118ed1ac;          /* tex dim C (fild) */
extern int DAT_1186c950;          /* tex offset D (fild) */
extern float DAT_118ed1a8;        /* tex scale B (fdiv) */

typedef struct { float x, y, z, s, t, n0, n1, n2; } BrDlSrcVtxG;

extern void    FUN_100344D0(void *);
extern double  FUN_10074600(double);
extern void    FUN_10022AC0(const void *, void *);
extern int32_t FUN_10022120(void *);
extern void    FUN_10022070(void *, void *, float, float, float);

/* WHAT IT DOES: transforms a batch of vertices through the combined matrix,
 * generates sphere-map texture coordinates by rotating each normal into
 * world space and dotting with two view-direction vectors (one through asin
 * for latitude), then lights and projects each vertex.
 *
 * @t4-pass 0x10022BF0 1 2026-09-19 probes 13 bytes 1144 insns 344 regions 7 rows 179 census no  (5 compiler variants O2/Od/O2y/O2p/Odp + 8 expression-order variations: MVP x,y,z; texgen n0,n1,n2; dotX/dotY regroup; sphere-map step-by-step; light-dir regroup; colour cast unsigned; divide order.  Best O2y 799 diffs, texgen reorder 798, sphere steps 797.  All within 2 diffs of each other.  Residue is register allocation + x87 scheduling: 14 fstp-reg vs memory, 10 fmul-reg vs memory, 10 fld-reg vs memory, plus direct-vs-indirect call instruction class from separate-file callees.  No source lever.)
 * @t4-pass 0x10022BF0 2 2026-09-19 probes 13 bytes 1144 insns 344 regions 7 rows 179 census yes  (census of unpaired multiset: MISSING class is fld/fmul/fstp R (register operand) where recomp uses dword-ptr memory operand = TU-context register pressure; EXTRA class is fild-qword, fmul-st(N), cmp-R-R = different instruction encoding of same operations.  6 direct calls in orig vs 6 reloc calls in recomp = separate-file callee linkage, not a code difference.  Every divergent instruction is register-allocation or instruction-scheduling class.  A5 oracle: EQUIVALENT on 64 seeds, same return + globals + side effects.)
 */
/* @t3 0x10022BF0 2026-09-23 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1160/1307 insns 348/391 rows 108+65 regions 8 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation + x87 scheduling from separate-file
 * compilation (no TU neighbours).  All unpaired rows are instruction-
 * encoding differences (reg vs mem operand, direct vs reloc call).
 * A5 oracle: EQUIVALENT on 64 seeds.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10022BF0 glide BrDlVtxGenLin */
const uint8_t *BrDlVtxGenLin(const uint8_t *p)
{
    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            float *m;
            float dx, dy, dz;

            if (DAT_100a9a50 != 0)
                m = (float *)((char *)&DAT_105ccd10 + (DAT_100a9a50 << 6));
            else
                m = NULL;

            DAT_105ce210 = (float)(int)(DAT_105ccc78 & 0xff);
            DAT_105ce214 = (float)(int)((DAT_105ccc78 >> 8) & 0xff);
            DAT_105ce218 = (float)(int)(unsigned)DAT_105ccc7a;

            dx = (float)(int)DAT_105ccc80;
            dy = (float)(int)DAT_105ccc81;
            dz = (float)(int)DAT_105ccc82;

            DAT_105ce21c = ((m[1] * dy + m[0] * dx) + m[2] * dz) / DAT_10077420;
            DAT_105ce220 = ((m[5] * dy + m[4] * dx) + m[6] * dz) / DAT_10077420;
            DAT_105ce224 = ((m[9] * dy + m[8] * dx) + m[10] * dz) / DAT_10077420;

            FUN_100344D0(&DAT_105ce21c);

            DAT_105ce228 = (float)(int)(DAT_105ccc88 & 0xff);
            DAT_105ce22c = (float)(int)((DAT_105ccc88 >> 8) & 0xff);
            DAT_105ce230 = (float)(int)(unsigned)DAT_105ccc8a;
        }
        DAT_105d17d0 = 1;
    }

    {
        uint32_t w0 = *(const uint32_t *)p;
        const BrDlSrcVtxG *pSrc = *(const BrDlSrcVtxG **)(p + 4);
        int v0 = (w0 >> 16) & 0xFF;
        int n  = (w0 >> 10) & 0x3F;
        BrDlVtx *pV = &DAT_105ce318[v0];

        if (n > 0) {
            int i;
            for (i = n; i != 0; i--) {
                float *m;
                BrVec3 td;
                const uint8_t *look1, *look2;
                float lx0, lx1, lx2, ly0, ly1, ly2;
                float dotX, dotY, dotX_128, dotY_128;
                float asin_val;
                float lat;
                int32_t latBits;  /* the quotient's 32-bit image: rounds it through memory */
                float texDimA, texOffB, texDimC, texOffD;
                int32_t oc;

                pV->cx = DAT_105d1780 * pSrc->z + DAT_105d1770 * pSrc->y + pSrc->x * DAT_105d1760 + DAT_105d1790;
                pV->cy = DAT_105d1784 * pSrc->z + DAT_105d1774 * pSrc->y + pSrc->x * DAT_105d1764 + DAT_105d1794;
                pV->cz = DAT_105d1788 * pSrc->z + DAT_105d1778 * pSrc->y + pSrc->x * DAT_105d1768 + DAT_105d1798;
                pV->cw = DAT_105d178c * pSrc->z + DAT_105d177c * pSrc->y + pSrc->x * DAT_105d176c + DAT_105d179c;

                if (DAT_100a9a50 != 0)
                    m = (float *)((char *)&DAT_105ccd10 + (DAT_100a9a50 << 6));
                else
                    m = NULL;

                td.x = (m[0] * pSrc->n0 + m[8] * pSrc->n2) + m[4] * pSrc->n1;
                td.y = (m[1] * pSrc->n0 + m[9] * pSrc->n2) + m[5] * pSrc->n1;
                td.z = (m[2] * pSrc->n0 + m[10] * pSrc->n2) + m[6] * pSrc->n1;

                FUN_100344D0(&td);

                look1 = DAT_105ce2d8 + 8;
                look2 = DAT_105ce2dc + 8;

                lx0 = (float)(int)(int8_t)look1[0];
                lx2 = (float)(int)(int8_t)look1[2];
                ly0 = (float)(int)(int8_t)look2[0];
                ly1 = (float)(int)(int8_t)look2[1];
                lx1 = (float)(int)(int8_t)look1[1];
                ly2 = (float)(int)(int8_t)look2[2];

                dotX = (lx0 * td.x + lx2 * td.z) + lx1 * td.y;
                dotY = (ly0 * td.x + ly1 * td.y) + ly2 * td.z;

                dotX_128 = dotX / DAT_10077420;
                dotY_128 = dotY / DAT_10077420;

                asin_val = (float)FUN_10074600((double)dotX_128);

                texDimA = (float)DAT_1186c958;
                texOffB = (float)DAT_118ed198;
                pV->s = (dotY_128 * texDimA - DAT_10077424 - texOffB * DAT_1007742c) / DAT_118ed1a4;

                /* The latitude is its own FLOAT: the original rounds
                 * asin/pi through [esp+0x2c] (0x10023062 fstp / 0x10023066
                 * fld) before scaling by texDimC.  Folded into the product
                 * it stays at register precision and t comes out one ULP
                 * off -- the live oracle's first capture showed exactly that.
                 * VC5 forwards a float scalar in st(n) whatever the spelling
                 * (plain, cast, volatile, a local array, even under /Op);
                 * reading it back through its 32-bit image is the
                 * store-and-reload the original performs. */
                lat = asin_val / DAT_10077428;
                latBits = *(int32_t *)&lat;
                texDimC = (float)DAT_118ed1ac;
                texOffD = (float)DAT_1186c950;
                pV->t = (*(float *)&latBits * texDimC - DAT_10077424 - texOffD * DAT_1007742c) / DAT_118ed1a8;

                FUN_10022AC0(pSrc, &pV->f40);

                oc = FUN_10022120(&pV->f40);
                pV->outcode = oc;

                if (oc == 0) {
                    FUN_10022070(pV, &pV->f40, pV->n0, pV->n1, pV->n2);
                }

                pSrc++;
                pV++;
            }
        }
    }
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
