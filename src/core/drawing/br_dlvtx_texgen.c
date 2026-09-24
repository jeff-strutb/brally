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
#include <stddef.h>
typedef struct BrVec3 { float x, y, z; } BrVec3;
typedef struct BrDlVtx {
    float   x, y, z;
    float   r, g, b;
    float   ooz;
    float   a;
    float   oow;
    float   tmu0[3];
    float   tmu1[3];
    int32_t outcode;
    float   f40;
    float   cx, cy, cz;
    float   s, t;
    float   cw;
    float   n0, n1, n2;
} BrDlVtx;

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
extern BrDlMtx DAT_105ccd50[];    /* model matrix stack, 1-based: [top - 1] */

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

extern float DAT_105cd9f8;
extern float DAT_10077410;
extern float DAT_10077428;
extern int32_t     DAT_105ce310;
extern int DAT_118ec988;
extern int DAT_1186c954;
extern uint8_t DAT_105ccc8a;
extern float DAT_10077408;
extern float DAT_1007742c;
extern float       DAT_105cd9f8;
extern uint32_t DAT_105ccc88;
extern float DAT_105ccd48;
extern float DAT_105ccfdc;
extern float DAT_10077404;
extern float       DAT_105ccfdc;
extern float       DAT_105cd9fc;
extern float       DAT_105ccd48;
extern float DAT_105cd9fc;
/* T2 2026-09-24 (hand transcription from the asm), compiled like the rest of
 * its original TU with /O2 /Op: 50 of 362 instructions still differ
 * (difflib count).  Spellings that each moved a whole class (all measured):
 *  - the lights are N64 Light records read as BYTES: VC5 merges col[0]/col[1]
 *    into one dword load + `and`/`mov dl,ah`, exactly as the original;
 *  - x87 operand roles follow VC5's operand-kind ladder, not source order:
 *    the z/y MVP columns are absolute-address derefs (they take the fld side
 *    over the vertex fields), the x/w columns stay extern symbols;
 *  - the normal pointer is re-derived from the source pointer at the top of
 *    each pass (`pn = &pSrc->n1`), never advanced by hand: VC5 strength-
 *    reduces it into the original's esi = ebx+0x18 induction and sets it up
 *    AFTER the zero-trip test, as the original does;
 *  - pSrc is advanced before the helper's output-vertex pointer (tail order
 *    ebx, esi, pVc, as in the original);
 *  - the output vertex pointer is formed BEFORE the vertex count is
 *    extracted: that keeps the count in eax and the command pointer in esi;
 *  - the model-matrix stack is 1-BASED, read as [top - 1] from 0x105CCD50:
 *    VC5 folds the -0x40 into the displacement and emits the original's
 *    `lea reg,[reg+0x105ccd10]` (every zero-based spelling gives `add`);
 *  - the look-vector bytes are INLINE (float)(int) casts: under /Op each is
 *    rounded through one scratch slot and consumed from memory, as in the
 *    original; named float temps get reloaded instead;
 *  - the x dot product is computed BEFORE the s store: VC5 will not hoist the
 *    look1 byte loads above a store through the vertex pointer;
 *  - the file carries its own minimal BrDlVtx typedef instead of br_dl.h and
 *    declares the TU's other light/clip globals: the symbol table in front
 *    of the function decides x87 operand-role ties.  Setup order, groupings
 *    and declaration order were found by a hill-climb over those choices.
 * Open (x87 scheduling / operand role only; every row pairs):
 *  - the transform rows: the original issues the x product LAST (column
 *    loads z, y, then the vertex x); ours issues it second.  Dead: all six
 *    term orders, row/association grouping (8 forms), x as deref / cast /
 *    indexed, x column absolute / array / extern position (65 positions);
 *  - m[0]*n0: the original loads m[0]; ours loads n0.  Dead: struct matrix
 *    pointer, const pointer, `*m`;
 *  - the light setup's three extra fxch (direction fild issued before the
 *    last colour store): dead under all 720 statement orders;
 *  - the texgen spill slots for dotX and the t-scale cast swap (0x2c/0x30).
 * @t4-pass 0x10022600 1 2026-09-24 probes 733 bytes 1206 insns 359 regions 10 rows 11 census no  (light setup: all 720 orders of the three direction and three colour statements; transform rows: 8 grouping/operand-kind forms of the x term; matrix/vertex pointer types: struct matrix pointer, const, `*m`, cast and indexed x; nothing moved)
 * @t4-pass 0x10022600 2 2026-09-24 probes 272 bytes 1206 insns 359 regions 10 rows 11 census yes  (census: every row is an x87 operand-role or issue-slot pair plus the dotX/t-scale spill-slot swap, which is what declaration order decides -- moved each of the 17 locals to every other slot; nothing moved) */
/* @t3 0x10022600 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1206/1212 insns 359/362 rows 7+4 regions 10 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is x87 scheduling and operand role only (see the Open list above);
 * every row pairs.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* WHAT IT DOES: transforms a batch of vertices through the combined matrix,
 * generates texture coordinates by rotating each normal into world space and
 * projecting it on the two view-direction vectors (a straight linear map to
 * the current texture's size), then lights and projects each vertex. */
/* @implements 0x10022600 glide BrDlVtxGen */
const uint8_t *BrDlVtxGen(const uint8_t *p)
{
    int v0;
    int n;
    BrDlVtx *pVc;
    const uint8_t *look1, *look2;
    int i;
    float *pf;
    const BrDlSrcVtxT *pSrc;
    float *m;
    BrVec3 td;
    BrDlVtx *pV;
    float dx, dy, dz;
    const float *pn;
    int off;
    float dotX_128;
    uint32_t w0;
    float texDimA, texOffB, texDimC, texOffD;
    int32_t oc;

    if (!DAT_105d17d0) {
        if (DAT_105ccfd0 != 0) {
            m = DAT_100a9a50 ? DAT_105ccd50[DAT_100a9a50 - 1].m : NULL;
            dx = (float)DAT_105ccc78[0].dir[0];
            dy = (float)DAT_105ccc78[0].dir[1];
            dz = (float)DAT_105ccc78[0].dir[2];
            DAT_105ce210 = (float)DAT_105ccc78[0].col[0];
            DAT_105ce214 = (float)DAT_105ccc78[0].col[1];
            DAT_105ce218 = (float)DAT_105ccc78[0].col[2];
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
    pV = &DAT_105ce318[v0];
    n  = (w0 >> 10) & 0x3F;

    pVc = pV;
    for (i = 0; i < n; i++) {
        pn = &pSrc->n1;
        pV[i].cx = DAT_105d1780 * pn[-4] + DAT_105d1770 * pn[-5] + pSrc->x * DAT_105d1760 + DAT_105d1790;
        pV[i].cy = DAT_105d1784 * pn[-4] + DAT_105d1774 * pn[-5] + pSrc->x * DAT_105d1764 + DAT_105d1794;
        pV[i].cz = DAT_105d1788 * pn[-4] + DAT_105d1778 * pn[-5] + pSrc->x * DAT_105d1768 + DAT_105d1798;
        pV[i].cw = DAT_105d178c * pn[-4] + DAT_105d177c * pn[-5] + pSrc->x * DAT_105d176c + DAT_105d179c;

        if (DAT_100a9a50 != 0) {
            off = (DAT_100a9a50 - 1) << 6;
            m = (float *)((char *)DAT_105ccd50 + off);
        } else
            m = NULL;

        td.x = (m[0] * pn[-1] + m[8] * pn[1]) + m[4] * pn[0];
        td.y = (m[1] * pn[-1] + m[9] * pn[1]) + m[5] * pn[0];
        td.z = (m[2] * pn[-1] + m[10] * pn[1]) + m[6] * pn[0];

        FUN_100344D0(&td);

        pf = &pV[i].f40;
        look1 = DAT_105ce2d8 + 8;
        look2 = DAT_105ce2dc + 8;

        dotX_128 = ((td.x * (float)(signed char)look1[0] + (float)(signed char)look1[2] * td.z) + (float)(signed char)look1[1] * td.y) / DAT_10077420;
        pV[i].s = (((td.x * (float)(signed char)look2[0] + (float)(signed char)look2[2] * td.z) + (float)(signed char)look2[1] * td.y) / DAT_10077420
                   * (float)DAT_1186c958 - DAT_10077424 - (float)DAT_118ed198) / DAT_118ed1a4;
        pV[i].t = (dotX_128 * (float)DAT_118ed1ac - DAT_10077424 - (float)DAT_1186c950) / DAT_118ed1a8;

        FUN_10022AC0(pSrc, pf);

        oc = FUN_10022120(pf);
        pV[i].outcode = oc;

        if (oc == 0) {
            FUN_10022070(pVc, pf, pV[i].n0, pV[i].n1, pV[i].n2);
        }
        pSrc++;
        pVc++;
    }
    return p + 8;
}

#endif /* BR_MATCHING_BUILD */
