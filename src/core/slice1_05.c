/* slice1_05.c -- Boss Rally (BRD3D.dll), a later pass, 0x1002B280..0x100360F0.
 * See slice1_05.h for the per-function notes and gotchas. */

#ifdef BR_MATCHING_BUILD
/* The originals of the vtx-cache cluster take no BrVtxCache parameter --
 * state is loose globals -- and BrVtxExpand/Insert/Resolve have different
 * arities. Hide the header's port prototypes behind renames so the
 * matching twins can define the real symbols with the original
 * signatures; other TUs keep calling with the port signatures (cdecl, so
 * the extra leading argument is harmless at run time). */
#define BrVtxExpand       BrVtxExpand_hdr
#define BrVtxCacheInsert  BrVtxCacheInsert_hdr
#define BrVtxCacheResolve BrVtxCacheResolve_hdr
#define BrSelLookup       BrSelLookup_hdr
#define BrPtrListAdd      BrPtrListAdd_hdr
#define BrF3DVtxFixup     BrF3DVtxFixup_hdr
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#else
#include "slice1_05.h"
#include "br_gamestep.h"   /* 0x10034C66/0x10034C73 == BRGlide 0x1002E317/0x1002E324 */
#endif

#include <stddef.h>

/* Decode a little-endian signed 16-bit value byte-wise. The x86 original
 * reads these with `movsx word ptr`; doing it by hand rather than by struct
 * overlay makes the result identical on a big-endian host too. */
static int br05_s16le(const unsigned char *p)
{
    unsigned v = (unsigned)p[0] | ((unsigned)p[1] << 8);
    return (v & 0x8000u) ? (int)v - 0x10000 : (int)v;
}

/* ================================================================== */
/* 1. N64 vertex cache                                                */
/* ================================================================== */

/* 0x1002BDD0 */
/* WHAT IT DOES: puts a batch of the game's original N64 vertices the right way
 * round for a PC. The console stored its numbers most-significant byte first,
 * so each of the six 16-bit fields in a vertex has its two bytes exchanged; the
 * colour or normal bytes at the end need no swapping and are left alone. */
/* @implements 0x1002BDD0 d3d BrVtxSwap */
#ifdef BR_MATCHING_BUILD
/* The original swaps each 16-bit field as a word store of a byte pack
 * (`xor edx,edx; mov dh/dl; mov [..],dx`), unrolled over the six fields.
 * The first two fields pack high-byte-first, the last four low-byte-first
 * -- a source operand-order fossil, preserved.
 * RESIDUE (parked): VC5 anchors the walked pointer at +2 (`add eax,2`
 * preheader, stores at -2..+8) from EVERY probed spelling -- pointer walk,
 * ((u16*)p)[k] scaled stores, short-pointer walk, indexed i*16 base,
 * do-while, statement reorder, union temp (spills), /O1 /Os /Op /Og-.
 * The original anchors at +0. One extra insn, +3 B, REGNORM 4+3; the
 * field-0 load-order flip is downstream of the same bias. */
void BrVtxSwap(void *pVerts, int count)
{
    unsigned char *p = (unsigned char *)pVerts;
    int i;

    for (i = 0; i < count; ++i) {
        ((unsigned short *)p)[0] = (unsigned short)((p[0] << 8) | p[1]);
        ((unsigned short *)p)[1] = (unsigned short)((p[2] << 8) | p[3]);
        ((unsigned short *)p)[2] = (unsigned short)(p[5] | (p[4] << 8));
        ((unsigned short *)p)[3] = (unsigned short)(p[7] | (p[6] << 8));
        ((unsigned short *)p)[4] = (unsigned short)(p[9] | (p[8] << 8));
        ((unsigned short *)p)[5] = (unsigned short)(p[11] | (p[10] << 8));
        p += BR_VTX_SRC_SIZE;
    }
}
#else
void BrVtxSwap(void *pVerts, int count)
{
    unsigned char *p = (unsigned char *)pVerts;
    int i;

    if (count <= 0)
        return;

    for (i = 0; i < count; ++i) {
        int k;
        /* Fully unrolled in the original over offsets 0x00,0x02,...,0x0A.
         * 0x0C..0x0F are deliberately left alone. */
        for (k = 0; k < 12; k += 2) {
            unsigned char t = p[k];
            p[k]     = p[k + 1];
            p[k + 1] = t;
        }
        p += BR_VTX_SRC_SIZE;
    }
}
#endif

/* 0x1002BE30 */
/* WHAT IT DOES: unpacks a batch of the game's compact N64 vertices into the
 * floating-point positions, texture coordinates and normals the PC renderer
 * works with, appending them to a running buffer. It hands back where in that
 * buffer the batch started. */
/* @implements 0x1002BE30 d3d BrVtxExpand */
#ifdef BR_MATCHING_BUILD
/* Original: 2 args, state in globals. Each conversion is a direct
 * short/char load with an inline (float) cast -- one shared int home
 * slot, fild, fstp. Cursor and vertex count are re-read from the globals
 * at the loop tail (the fstps could alias them). */
extern float *DAT_100a751c;     /* output cursor       */
extern int    DAT_105b96f8;     /* running vertex count */
extern float  DAT_100773a0;     /* normal scale, 1/128  */

float *BrVtxExpand(const void *pVerts, int count)
{
    float *pStart = DAT_100a751c;
    const char *p;

    if (count > 0) {
        p = (const char *)pVerts;
        do {
            float *o = DAT_100a751c;
            o[0] = (float)*(const short *)p;
            p += 0x10;      /* advanced HERE in the original; the rest of
                             * the record is read back at negative offsets */
            o[1] = (float)*(const short *)(p - 0x0E);
            o[2] = (float)*(const short *)(p - 0x0C);
            /* offset 0x06 (the Vtx flag) is skipped */
            o[3] = (float)*(const short *)(p - 0x08);
            o[4] = (float)*(const short *)(p - 0x06);
            o[5] = (float)*(const signed char *)(p - 4) * DAT_100773a0;
            o[6] = (float)*(const signed char *)(p - 3) * DAT_100773a0;
            o[7] = (float)*(const signed char *)(p - 2) * DAT_100773a0;

            DAT_100a751c = DAT_100a751c + 8;
            DAT_105b96f8 = DAT_105b96f8 + 1;
        } while (--count != 0);
    }
    return pStart;
}
#else
float *BrVtxExpand(BrVtxCache *pCache, const void *pVerts, int count)
{
    const unsigned char *p = (const unsigned char *)pVerts;
    float *pStart = pCache->pCursor;
    float *pOut   = pStart;
    int i;

    /* The return value is captured before the loop, so a non-positive count
     * still returns the current cursor. */
    if (count <= 0)
        return pStart;

    for (i = 0; i < count; ++i) {
        pOut[0] = (float)br05_s16le(p + 0x00);
        pOut[1] = (float)br05_s16le(p + 0x02);
        pOut[2] = (float)br05_s16le(p + 0x04);
        /* offset 0x06 (the Vtx flag) is skipped */
        pOut[3] = (float)br05_s16le(p + 0x08);
        pOut[4] = (float)br05_s16le(p + 0x0A);
        pOut[5] = (float)(int)(signed char)p[0x0C] * BR_VTX_NORMAL_SCALE;
        pOut[6] = (float)(int)(signed char)p[0x0D] * BR_VTX_NORMAL_SCALE;
        pOut[7] = (float)(int)(signed char)p[0x0E] * BR_VTX_NORMAL_SCALE;
        /* offset 0x0F is never read */

        p    += BR_VTX_SRC_SIZE;
        pOut += BR_VTX_OUT_FLOATS;

        /* The original writes both globals back inside the loop body. */
        pCache->pCursor = pOut;
        pCache->nVerts += 1;
    }

    return pStart;
}
#endif

/* 0x1002BF00 */
/* WHAT IT DOES: notes that a particular batch of vertices has already been
 * unpacked, and where the unpacked copy lives, so the same batch does not have
 * to be converted twice. Once the note table is full, further batches are
 * silently not remembered -- they still work, they just get converted again
 * every time. */
/* @implements 0x1002BF00 d3d BrVtxCacheInsert */
#ifdef BR_MATCHING_BUILD
/* Original: 3 args; the entry table is a pinned global array at
 * 0x105B16F0 (base folded as a displacement), count at 0x105B76F4,
 * capacity 0x800. */
extern int  DAT_105b76f4;       /* nEntries */
extern void *DAT_105b16f0;      /* entry[0].pSrc  -- three interleaved   */
extern int   DAT_105b16f4;      /* entry[0].count -- pinned columns,     */
extern void *DAT_105b16f8;      /* entry[0].pOut  -- stride 12           */

void BrVtxCacheInsert(void *pSrc, int count, float *pOut)
{
    int n = DAT_105b76f4;

    if (n < 0x800) {
        /* byte-offset spelling: n*12 materialised once (lea/shl), each
         * column's own symbol as the displacement */
        *(void **)((char *)&DAT_105b16f0 + n * 12) = pSrc;
        *(int *)  ((char *)&DAT_105b16f4 + n * 12) = count;
        *(void **)((char *)&DAT_105b16f8 + n * 12) = pOut;
        DAT_105b76f4 = n + 1;
    }
}
#else
void BrVtxCacheInsert(BrVtxCache *pCache, void *pSrc, int count, float *pOut)
{
    int n = pCache->nEntries;

    if (n >= BR_VTX_CACHE_MAX)
        return;                     /* silently dropped -- see the header */

    pCache->aEntries[n].pSrc  = pSrc;
    pCache->aEntries[n].count = count;
    pCache->aEntries[n].pOut  = pOut;
    pCache->nEntries = n + 1;
}
#endif

/* 0x1002BD50 */
/* WHAT IT DOES: hands back the PC-ready version of a batch of the game's N64
 * vertices. If that batch has been seen before it returns the copy made last
 * time; otherwise it byte-swaps it, unpacks it, and remembers the result for
 * next time. This is the seam where console geometry becomes something the PC
 * can draw. */
/* @implements 0x1002BD50 d3d BrVtxCacheResolve */
#ifdef BR_MATCHING_BUILD
/* Original: 2 args, table in the same pinned globals. *ppVerts is
 * RE-READ before the expand and insert calls (recompute-per-site). */
void BrVtxCacheResolve(void **ppVerts, int count)
{
    void *pSrc = *ppVerts;
    const char *q;
    float *pOut;
    int n, i;

    if (pSrc == NULL)
        return;
    if (count == 0)                 /* not `count <= 0` */
        return;

    n = DAT_105b76f4;
    for (i = 0; i < n; ++i) {
        if (*(void **)((char *)&DAT_105b16f4 + i * 12 - 4) == pSrc &&
            *(int *)((char *)&DAT_105b16f4 + i * 12) == count) {
            *ppVerts = *(void **)((char *)&DAT_105b16f8 + i * 12);
            return;
        }
    }

    BrVtxSwap(pSrc, count);
    pOut = BrVtxExpand(*ppVerts, count);
    BrVtxCacheInsert(*ppVerts, count, pOut);
    *ppVerts = pOut;
}
#else
void BrVtxCacheResolve(BrVtxCache *pCache, void **ppVerts, int count)
{
    void *pSrc = *ppVerts;
    float *pOut;
    int i;

    if (pSrc == NULL)
        return;
    if (count == 0)                 /* not `count <= 0` -- see the header */
        return;

    for (i = 0; i < pCache->nEntries; ++i) {
        if (pCache->aEntries[i].pSrc == pSrc &&
            pCache->aEntries[i].count == count) {
            *ppVerts = pCache->aEntries[i].pOut;
            return;
        }
    }

    BrVtxSwap(pSrc, count);
    pOut = BrVtxExpand(pCache, pSrc, count);
    BrVtxCacheInsert(pCache, pSrc, count, pOut);
    *ppVerts = pOut;
}
#endif

/* ================================================================== */
/* 2. F3DEX display-list patching                                     */
/* ================================================================== */

/* 0x1002C150 */
/* WHAT IT DOES: repairs a "load these vertices" instruction in a piece of the
 * game's original console drawing script, turning the console's segmented
 * address into somewhere that actually exists in this process, and reports how
 * many vertices the instruction loads. */
/* @implements 0x1002C150 d3d BrF3DVtxFixup */
#ifdef BR_MATCHING_BUILD
/* Original: 1 arg, void. The segment base is the global, the fixup callee
 * is the 1-arg BrSegPtrFixup, and the vertex count is not returned -- it
 * goes straight into BrVtxCacheResolve(&w1, count). */
extern int32_t g_brSegN64Base;      /* 0x104B16E4 */

void BrF3DVtxFixup(BrGfxWords *pCmd)
{
    uint32_t base = (uint32_t)g_brSegN64Base;

    /* base ^ ((base ^ w1) & 0x00FFFFFF): take the low 24 bits from w1 and
     * the top 8 from the segment base. */
    pCmd->w1 = base ^ ((base ^ pCmd->w1) & 0x00FFFFFFu);

    BrSegPtrFixup(&pCmd->w1);
    BrVtxCacheResolve((void **)&pCmd->w1, (int)((pCmd->w0 >> 10) & 0x3Fu));
}
#else
unsigned BrF3DVtxFixup(const BrSegMap *pMap, BrGfxWords *pCmd)
{
    uint32_t base = pMap->n64Base;

    /* base ^ ((base ^ w1) & 0x00FFFFFF): take the low 24 bits from w1 and
     * the top 8 from the segment base. */
    pCmd->w1 = base ^ ((base ^ pCmd->w1) & 0x00FFFFFFu);

    BrSegFixup(pMap, &pCmd->w1);

    return (unsigned)((pCmd->w0 >> 10) & 0x3Fu);
}
#endif

/* 0x1002C190 */
/* WHAT IT DOES: repairs a draw-one-triangle instruction in the console drawing
 * script. The console counted its vertices in units of two bytes, so each of
 * the three vertex numbers is halved to become a plain index. */
/* @implements 0x1002C190 d3d BrF3DTri1Fixup */
void BrF3DTri1Fixup(void *pCmd)
{
    unsigned char *p = (unsigned char *)pCmd;

    /* The original touches 6, 5, 4 in that order; order is not observable. */
    p[6] = (unsigned char)(p[6] >> 1);
    p[5] = (unsigned char)(p[5] >> 1);
    p[4] = (unsigned char)(p[4] >> 1);
}

/* 0x1002C1B0 */
/* WHAT IT DOES: the same repair as its neighbour above, for the instruction
 * that draws two triangles at once -- six vertex numbers instead of three. */
/* @implements 0x1002C1B0 d3d BrF3DTri2Fixup */
void BrF3DTri2Fixup(void *pCmd)
{
    unsigned char *p = (unsigned char *)pCmd;

    p[2] = (unsigned char)(p[2] >> 1);
    p[1] = (unsigned char)(p[1] >> 1);
    p[0] = (unsigned char)(p[0] >> 1);
    p[6] = (unsigned char)(p[6] >> 1);
    p[5] = (unsigned char)(p[5] >> 1);
    p[4] = (unsigned char)(p[4] >> 1);
}

/* ================================================================== */
/* 3. RDP colour combiner                                             */
/* ================================================================== */

/* 0x1002FAF0 */
/* WHAT IT DOES: translates one named colour ingredient -- the texture, the
 * shading, a flat colour, and so on -- into the number the graphics hardware
 * uses for it. Plain zero and plain one are special-cased; everything else is
 * just an offset. */
/* @implements 0x1002FAF0 d3d BrRdpCCMux */
int BrRdpCCMux(int token)
{
    /* orig 0x1002FAF0: `sub eax, 0` / `dec eax` is MSVC's consecutive
     * switch on cases 0 and 1, not a pair of ifs. */
    switch (token) {
    case 0:
        return 31;              /* G_CCMUX_0 */
    case 1:
        return 6;               /* G_CCMUX_1 */
    default:
        return token - 1000;
    }
}

/* 0x1002FAC0 */
/* WHAT IT DOES: the same translation as its neighbour above, but for the
 * transparency channel, where the hardware numbers the ingredients differently
 * and one of them is not available at all. */
/* @implements 0x1002FAC0 d3d BrRdpACMux */
int BrRdpACMux(int token)
{
    /* orig 0x1001D150: `sub eax,0; je; dec; je; sub eax,0x3f4; je` is the
     * consecutive-case switch 0 / 1 / 1013, same shape as BrRdpCCMux. */
    switch (token) {
    case 0:
        return 7;               /* G_ACMUX_0 */
    case 1:
        return 6;               /* G_ACMUX_1 */
    case 1013:
        return 0;               /* LOD_FRACTION: 13 in colour, 0 in alpha */
    default:
        return token - 1000;
    }
}

/* 0x1002F900 */
/* WHAT IT DOES: builds the single instruction that tells the graphics hardware
 * how to mix its ingredients together to get a pixel's colour -- which of the
 * texture, the lighting, the flat colours and so on go into each of the two
 * mixing stages, for colour and for transparency alike. Sixteen choices are
 * squeezed into two words, and the instruction's own identifying byte falls out
 * of the packing rather than being written in. */
/* @implements 0x1002F900 d3d BrRdpSetCombineLERP */
void BrRdpSetCombineLERP(BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    /* Orig converts every argument first (a0..d0 live in ebx/esi/edi/ebp
     * across the remaining calls; the rest overwrite their own stack slots)
     * and only then packs. Interleaved convert-and-shift is 40 bytes short
     * and never pushes ebp. */
    a0  = BrRdpCCMux(a0);
    b0  = BrRdpCCMux(b0);
    c0  = BrRdpCCMux(c0);
    d0  = BrRdpCCMux(d0);
    Aa0 = BrRdpACMux(Aa0);
    Ab0 = BrRdpACMux(Ab0);
    Ac0 = BrRdpACMux(Ac0);
    Ad0 = BrRdpACMux(Ad0);
    a1  = BrRdpCCMux(a1);
    b1  = BrRdpCCMux(b1);
    c1  = BrRdpCCMux(c1);
    d1  = BrRdpCCMux(d1);
    Aa1 = BrRdpACMux(Aa1);
    Ab1 = BrRdpACMux(Ab1);
    Ac1 = BrRdpACMux(Ac1);
    Ad1 = BrRdpACMux(Ad1);

    /* The 0xFC command byte comes out of this ones-fill after the shifts
     * total exactly 20 bits; it is never OR'd in explicitly. b0 is the one
     * field not masked; the 28-bit total shift discards the excess. */
    pOut->w0 = ((((((a0 & 0x0F) | 0xFFFFFFC0) << 5 | (c0 & 0x1F)) << 3
                  | (Aa0 & 7)) << 3 | (Ac0 & 7)) << 4 | (a1 & 0x0F)) << 5
               | (c1 & 0x1F);
    pOut->w1 = ((((((((b0 << 4 | (b1 & 0x0F)) << 3 | (Aa1 & 7)) << 3
                     | (Ac1 & 7)) << 3 | (d0 & 7)) << 3 | (Ab0 & 7)) << 3
                   | (Ad0 & 7)) << 3 | (d1 & 7)) << 3 | (Ab1 & 7)) << 3
               | (Ad1 & 7);
}

/* ================================================================== */
/* 4. 4x4 matrix helpers                                              */
/* ================================================================== */

/* 0x100306C0 */
/* WHAT IT DOES: multiplies two 4x4 transforms together, which is how the game
 * combines a rotation with a position, or an object's placing with the camera.
 * If the answer is being written back over one of the inputs it works through a
 * scratch copy -- and, oddly, adds the four products up in a different order on
 * that path, so the two routes can disagree in the last bit or two. */
/* @implements 0x100306C0 d3d BrMat4Mul */
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{
    BrMat4 tmp;
    int i, j;

    if (pA == NULL || pB == NULL)
        return;
    /* pOut is deliberately NOT checked -- see the header. */

    /* TWO separate loop nests, not one nest with a flag: the original
     * branches once (both compares jump into the scratch path, the direct
     * path is the fallthrough) and each path carries its own rolled 4x4
     * loops with a different summation order. */
    /* Each element is ONE expression (a named `s` accumulator costs
     * fadd-without-pop + a discard at the loop tail).  The written pair
     * order is REVERSED from the evaluated one, and the two nests' pair
     * spellings are COUPLED through the optimizer -- all four combinations
     * measured; this one is the minimum.
     * RESIDUE (2+2 regnorm, 24 masked B, T3a): hoisted-operand-load order
     * inside the aliased nest (which b-row load is hoisted first) --
     * identical op counts, operand-source only; the playbook's documented
     * scheduling wall class. */
    if (pA != pOut && pB != pOut) {
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j) {
                /* 0x100306FD evaluates ((a2*b2 + a3*b3) + a0*b0) + a1*b1 */
                pOut->m[i][j] = (pA->m[i][3] * pB->m[3][j]
                                 + pA->m[i][2] * pB->m[2][j]
                                 + pA->m[i][0] * pB->m[0][j])
                                + pA->m[i][1] * pB->m[1][j];
            }
        }
        return;
    }

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            /* 0x10030753 evaluates ((a3*b3 + a1*b1) + a0*b0) + a2*b2 */
            tmp.m[i][j] = (pA->m[i][1] * pB->m[1][j]
                           + pA->m[i][3] * pB->m[3][j]
                           + pA->m[i][0] * pB->m[0][j])
                          + pA->m[i][2] * pB->m[2][j];
        }
    }
    *pOut = tmp;                /* `rep movsd` of 16 dwords in the original */
}

/* 0x10031140 */
void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz)
{
    pM->m[0][0] = 1.0f; pM->m[0][1] = 0.0f; pM->m[0][2] = 0.0f; pM->m[0][3] = 0.0f;
    pM->m[1][0] = 0.0f; pM->m[1][1] = 1.0f; pM->m[1][2] = 0.0f; pM->m[1][3] = 0.0f;
    pM->m[2][0] = 0.0f; pM->m[2][1] = 0.0f; pM->m[2][2] = 1.0f; pM->m[2][3] = 0.0f;
    pM->m[3][0] = tx;   pM->m[3][1] = ty;   pM->m[3][2] = tz;   pM->m[3][3] = 1.0f;
}

/* ================================================================== */
/* 5. Assorted setters, lists and lookups                             */
/* ================================================================== */

/* 0x1002B280 */
/* WHAT IT DOES: points both halves of a pair of cursors at the same place,
 * which is how a buffer gets rewound to its start. What the buffer holds is not
 * established here. */
/* @implements 0x1002B280 d3d BrCursorPairSet */
#ifdef BR_MATCHING_BUILD
void *g_brCursor575510;   /* 0x10575510 */
void *g_brCursor575518;   /* 0x10575518 */

void BrCursorPairSet(void *pv)
{
    /* CLOSE, NOT MATCHING -- 16 bytes against the original's 18.  The one
     * argument and the two absolute stores are now right; the original also
     * keeps a second live copy (`mov ecx, eax`) and stores one register to
     * each global, where MSVC here reuses eax for both.  A chained assignment
     * (a = b = pv) does not produce the copy either -- register-allocation
     * class, not source shape. */
    g_brCursor575510 = pv;
    g_brCursor575518 = pv;
}
#else
void BrCursorPairSet(BrCursorPair *pPair, void *pv)
{
    pPair->f10 = pv;
    pPair->f18 = pv;
}
#endif

/* 0x1002C1F0 */
/* WHAT IT DOES: appends one more entry to a fixed-length list. Once the list
 * is full further entries are dropped without a word and without an error. */
/* @implements 0x1002C1F0 d3d BrPtrListAdd */
#ifdef BR_MATCHING_BUILD
/* Original: 1 arg; count and array are pinned globals (0x105B76F0 /
 * 0x105B76F8), scaled-index store. */
extern int   DAT_105b76f0;
extern void *DAT_105b76f8[];

void BrPtrListAdd(void *pv)
{
    int n = DAT_105b76f0;

    if (n < 0x800) {
        DAT_105b76f8[n] = pv;       /* silently dropped when full */
        DAT_105b76f0 = n + 1;
    }
}
#else
void BrPtrListAdd(BrPtrList *pList, void *pv)
{
    int n = pList->n;

    if (n >= BR_PTRLIST_MAX)
        return;                     /* silently dropped */

    pList->ap[n] = pv;
    pList->n = n + 1;
}
#endif

/* 0x1002F460 */
/* WHAT IT DOES: purpose unclear. Observably it reads a pair of numbers out of
 * a table using two selector bytes as a row and column, and if one flag bit is
 * set it rotates the first of the pair by half a turn of twelve -- six becomes
 * zero, zero becomes six -- which has the shape of a mirroring or opposite-
 * direction rule. The second number is looked up fresh so the rotation cannot
 * affect it. What the table describes is not established here. */
/* @implements 0x1002F460 d3d BrSelLookup */
#ifdef BR_MATCHING_BUILD
/* Original: no parameters. The input record comes through a pointer
 * global, the table is two interleaved pinned byte columns (0x100B3028 /
 * 0x100B3029), and the results are globals. A shared unsigned-char temp
 * carries first the table byte (edx, copied into the int a) and then the
 * flag byte (dl reloaded without re-zeroing). */
typedef struct BrSelInM {
    unsigned char f00;
    unsigned char pad[3];
    unsigned char f04;
    unsigned char f05;
} BrSelInM;

extern BrSelInM     *DAT_10af2094;
extern unsigned char DAT_100b3028[];
extern unsigned char DAT_100b3029[];
extern int           DAT_100b3014;
extern int           DAT_104b15e8;

void BrSelLookup(void)
{
    BrSelInM *p = DAT_10af2094;
    int idx = p->f04 * 12 + p->f05;
    unsigned char t;
    int a;

    t = DAT_100b3028[idx * 2];
    a = t;
    DAT_100b3014 = a;

    t = p->f00;
    if (t & 1) {
        if (a < 6)
            a += 6;
        else
            a -= 6;
        DAT_100b3014 = a;
    }

    /* recomputed, so the fold above cannot leak into the second lookup */
    idx = p->f04 * 12 + p->f05;
    DAT_104b15e8 = DAT_100b3029[idx * 2];
}
#else
void BrSelLookup(const BrSelInput *pIn, const unsigned char (*aTable)[2],
                 int *pOutA, int *pOutB)
{
    int idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    int a   = (int)aTable[idx][0];

    *pOutA = a;

    if (pIn->f00 & 1) {
        a = (a >= 6) ? (a - 6) : (a + 6);
        *pOutA = a;
    }

    /* Recomputed from f04/f05 in the original, so the fold above cannot
     * leak into the second lookup. */
    idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    *pOutB = (int)aTable[idx][1];
}
#endif

/* 0x10034C32 */
void BrHookNopA(void)
{
}

/* 0x10034C37 */
void BrHookSetA(BrHooks *pH, void *pv)
{
    pH->pfA = pv;
}

/* 0x10034C44 */
void BrHookSetB(BrHooks *pH, void *pv)
{
    pH->pfB = pv;
}

/* 0x10034C66 -- ONE BODY, br_gamestep.c's (BrGameStepSet), which carries
 * BRGlide's 0x1002E317 for it.
 *
 * `pH` IS NOT USED, AND WAS NEVER USED BY THE ORIGINAL.  0x10034C66 is
 *     push ebp / mov ebp,esp / mov eax,[ebp+8] / mov [0x106C0964],eax / ret
 * -- one cdecl argument written to a fixed global.  There is no `this`: the
 * BrHooks struct is a port-side gathering of six unrelated globals, and a
 * note elsewhere in the tree explaining this pair as __thiscall with the
 * `this` dropped was reading a struct that does not exist in the game.  The
 * parameter is kept only so the existing call sites need no change. */
void BrHookSetC(BrHooks *pH, void (*pfn)(void))
{
    (void)pH;
    BrGameStepSet(pfn);
}

/* 0x10034C73 -- ONE BODY, br_gamestep.c's (BrGameStepInvoke), which carries
 * BRGlide's 0x1002E324 for it.  `pH` is unused for the same reason as the
 * setter above: the original is `call dword ptr [0x106C0964]` and takes no
 * argument at all.
 *
 * DEVIATION, inherited from the surviving body and stated here because this
 * declaration used to promise the opposite: br_gamestep.c tests the slot for
 * NULL and returns 0, where the original calls through it and faults.  The
 * host harness needs to be able to report "nothing installed" rather than
 * die; the fault is the only behaviour lost. */
/* WHAT IT DOES: runs one frame of whatever the game is currently doing --
 * the race, or the front end -- by calling the routine installed in the
 * single slot that names the current activity. */
void BrHookCallC(const BrHooks *pH)
{
    (void)pH;
    (void)BrGameStepInvoke();
}

/* 0x10034C83 */
void BrHookNopB(void)
{
}

/* 0x10034C88 */
/* WHAT IT DOES: purpose unclear. Observably it raises one flag and hands back
 * the value of a counter, ignoring the pointer it is passed -- the routine the
 * original called with it does not read it either. */
/* @implements 0x10034C88 d3d BrHookTakeA */
#ifdef BR_MATCHING_BUILD
/* Literal form: 3-arg call into 0x10030F40 (which ignores its arguments and
 * just raises the one-shot flag), then return the destination global.  The
 * copy therefore never happens in practice; the shape is the original's. */
int FUN_10030f40();
extern uint32_t DAT_106e79c8;
uint32_t BrHookTakeA(BrHooks *pH, const void *pSrc)
{
    FUN_10030f40(&DAT_106e79c8, (char *)pH + 4, 4);
    return DAT_106e79c8;
}
#else
uint32_t BrHookTakeA(BrHooks *pH, const void *pSrc)
{
    (void)pSrc;                 /* the callee 0x100378A0 ignores it */
    pH->f7C44 = 1;
    return pH->g0938;
}
#endif

/* 0x10034CA8 */
/* WHAT IT DOES: the twin of the routine above -- same raised flag, same
 * ignored argument, but it returns a different counter. Purpose equally
 * unclear. */
/* @implements 0x10034CA8 d3d BrHookTakeB */
#ifdef BR_MATCHING_BUILD
extern uint32_t DAT_106ea390;
uint32_t BrHookTakeB(BrHooks *pH, const void *pSrc)
{
    FUN_10030f40(&DAT_106ea390, pH, 4);
    return DAT_106ea390;
}
#else
uint32_t BrHookTakeB(BrHooks *pH, const void *pSrc)
{
    (void)pSrc;
    pH->f7C44 = 1;
    return pH->g3300;
}
#endif

/* ================================================================== */
/* 6. Peer table                                                      */
/* ================================================================== */

/* 0x10036030 */
/* WHAT IT DOES: finds which slot in the network player table belongs to a
 * given player. If that player is not there yet it gives back the first free
 * slot instead, so the same call both looks up and allocates; if the table is
 * full it reports failure. The local player is always slot zero. */
BrPeer g_aBrPeers[BR_PEER_COUNT];   /* Glide 0x117A9B88; loop 1 starts at [1] */

#ifdef BR_MATCHING_BUILD
/* The original probes every record under that record's own Win32 mutex:
 * WaitForSingleObject(h, INFINITE), read f04/f2C, ReleaseMutex(h) -- through
 * the import table (the Wait import is CSEd into ebp, Release stays a
 * memory call).  Each probe's verdict is computed between the reads and the
 * release, then tested after it. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

/* @implements 0x10036030 d3d BrPeerFind */
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t idv, st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        idv = p->f04;
        st  = p->f2C;
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((st & BR_PEER_STATE_MASK) >= 1u && idv == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        /* Dword load, byte-width AND (`and bl,0x3f`), then the neg/sbb/inc
         * boolean OVERWRITES st in the same register (dword sbb), crossing
         * the Release call; the byte cast in the if gives the original's
         * `test bl,bl`.
         * RESIDUE (1+0 regnorm, +1 insn): the original births the load in
         * ebx and computes in place; ours computes in eax and copies to
         * ebx before the call.  Probed and failed: uint8_t st (byte load),
         * split byte local (extra byte move), separate int bFree (same). */
        st = p->f2C;
        st = (uint32_t)(((uint8_t)st & BR_PEER_STATE_MASK) == 0u);
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((uint8_t)st)
            return i;
    }

    return -1;
}
#else
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) != 0u &&
            g_aBrPeers[i].f04 == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) == 0u)
            return i;
    }

    return -1;
}
#endif

/* 0x10035FE0 */
/* WHAT IT DOES: prepares one entity for use -- clears its state, works out its
 * own number from where it sits in the array, and links it to the matching
 * record in the parallel table so the two can find each other later. */
BrEnt    g_aBrEnts[16];      /* 0x106ED708 */
BrEntRec g_aBrEntRecs[16];   /* 0x106ED630 */

/* @implements 0x10035FE0 d3d BrEntInit */
void __fastcall BrEntInit(BrEnt *pEnt)
{
    long idx;

    /* Written in this order by the original: +0x30, +0x2C, +0x44. */
    pEnt->f30 = 0;
    pEnt->f2C = 0;
    pEnt->f44 = 0;

    idx = (long)(pEnt - g_aBrEnts);
    pEnt->f154 = (int32_t)idx;
    pEnt->f158 = &g_aBrEntRecs[idx];
}
