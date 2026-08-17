/* slice1_05.c -- Boss Rally (BRD3D.dll), a later pass, 0x1002B280..0x100360F0.
 * See slice1_05.h for the per-function notes and gotchas. */

#include "slice1_05.h"
#include "br_gamestep.h"   /* 0x10034C66/0x10034C73 == BRGlide 0x1002E317/0x1002E324 */

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

/* 0x1002BE30 */
/* WHAT IT DOES: unpacks a batch of the game's compact N64 vertices into the
 * floating-point positions, texture coordinates and normals the PC renderer
 * works with, appending them to a running buffer. It hands back where in that
 * buffer the batch started. */
/* @implements 0x1002BE30 d3d BrVtxExpand */
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

/* 0x1002BF00 */
/* WHAT IT DOES: notes that a particular batch of vertices has already been
 * unpacked, and where the unpacked copy lives, so the same batch does not have
 * to be converted twice. Once the note table is full, further batches are
 * silently not remembered -- they still work, they just get converted again
 * every time. */
/* @implements 0x1002BF00 d3d BrVtxCacheInsert */
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

/* 0x1002BD50 */
/* WHAT IT DOES: hands back the PC-ready version of a batch of the game's N64
 * vertices. If that batch has been seen before it returns the copy made last
 * time; otherwise it byte-swaps it, unpacks it, and remembers the result for
 * next time. This is the seam where console geometry becomes something the PC
 * can draw. */
/* @implements 0x1002BD50 d3d BrVtxCacheResolve */
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

/* ================================================================== */
/* 2. F3DEX display-list patching                                     */
/* ================================================================== */

/* 0x1002C150 */
/* WHAT IT DOES: repairs a "load these vertices" instruction in a piece of the
 * game's original console drawing script, turning the console's segmented
 * address into somewhere that actually exists in this process, and reports how
 * many vertices the instruction loads. */
/* @implements 0x1002C150 d3d BrF3DVtxFixup */
unsigned BrF3DVtxFixup(const BrSegMap *pMap, BrGfxWords *pCmd)
{
    uint32_t base = pMap->n64Base;

    /* base ^ ((base ^ w1) & 0x00FFFFFF): take the low 24 bits from w1 and
     * the top 8 from the segment base. */
    pCmd->w1 = base ^ ((base ^ pCmd->w1) & 0x00FFFFFFu);

    BrSegFixup(pMap, &pCmd->w1);

    return (unsigned)((pCmd->w0 >> 10) & 0x3Fu);
}

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
    if (token == 0)
        return 31;              /* G_CCMUX_0 */
    if (token == 1)
        return 6;               /* G_CCMUX_1 */
    return token - 1000;
}

/* 0x1002FAC0 */
/* WHAT IT DOES: the same translation as its neighbour above, but for the
 * transparency channel, where the hardware numbers the ingredients differently
 * and one of them is not available at all. */
/* @implements 0x1002FAC0 d3d BrRdpACMux */
int BrRdpACMux(int token)
{
    if (token == 0)
        return 7;               /* G_ACMUX_0 */
    if (token == 1)
        return 6;               /* G_ACMUX_1 */
    if (token == 1013)
        return 0;               /* LOD_FRACTION: 13 in colour, 0 in alpha */
    return token - 1000;
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
    uint32_t w0, w1;

    /* The 0xFC command byte comes out of this ones-fill after the shifts
     * below total exactly 20 bits; it is never OR'd in explicitly. */
    w0  = ((uint32_t)BrRdpCCMux(a0) & 0x0Fu) | 0xFFFFFFC0u;
    w0  = (w0 << 5) | ((uint32_t)BrRdpCCMux(c0)  & 0x1Fu);
    w0  = (w0 << 3) | ((uint32_t)BrRdpACMux(Aa0) & 0x07u);
    w0  = (w0 << 3) | ((uint32_t)BrRdpACMux(Ac0) & 0x07u);
    w0  = (w0 << 4) | ((uint32_t)BrRdpCCMux(a1)  & 0x0Fu);
    w0  = (w0 << 5) | ((uint32_t)BrRdpCCMux(c1)  & 0x1Fu);

    /* b0 is the one field the original does not mask; the 28-bit total
     * shift is what discards the excess. */
    w1  = (uint32_t)BrRdpCCMux(b0);
    w1  = (w1 << 4) | ((uint32_t)BrRdpCCMux(b1)  & 0x0Fu);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Aa1) & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Ac1) & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpCCMux(d0)  & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Ab0) & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Ad0) & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpCCMux(d1)  & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Ab1) & 0x07u);
    w1  = (w1 << 3) | ((uint32_t)BrRdpACMux(Ad1) & 0x07u);

    pOut->w0 = w0;
    pOut->w1 = w1;
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
    BrMat4 *pDst;
    int aliased, i, j;

    if (pA == NULL || pB == NULL)
        return;
    /* pOut is deliberately NOT checked -- see the header. */

    aliased = (pOut == pA || pOut == pB);
    pDst = aliased ? &tmp : pOut;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            float s;
            if (aliased) {
                /* 0x10030753: ((a3*b3 + a1*b1) + a0*b0) + a2*b2 */
                s = pA->m[i][3] * pB->m[3][j] + pA->m[i][1] * pB->m[1][j];
                s = s + pA->m[i][0] * pB->m[0][j];
                s = s + pA->m[i][2] * pB->m[2][j];
            } else {
                /* 0x100306FD: ((a2*b2 + a3*b3) + a0*b0) + a1*b1 */
                s = pA->m[i][2] * pB->m[2][j] + pA->m[i][3] * pB->m[3][j];
                s = s + pA->m[i][0] * pB->m[0][j];
                s = s + pA->m[i][1] * pB->m[1][j];
            }
            pDst->m[i][j] = s;
        }
    }

    if (aliased)
        *pOut = tmp;            /* `rep movsd` of 16 dwords in the original */
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
void BrCursorPairSet(BrCursorPair *pPair, void *pv)
{
    pPair->f10 = pv;
    pPair->f18 = pv;
}

/* 0x1002C1F0 */
/* WHAT IT DOES: appends one more entry to a fixed-length list. Once the list
 * is full further entries are dropped without a word and without an error. */
/* @implements 0x1002C1F0 d3d BrPtrListAdd */
void BrPtrListAdd(BrPtrList *pList, void *pv)
{
    int n = pList->n;

    if (n >= BR_PTRLIST_MAX)
        return;                     /* silently dropped */

    pList->ap[n] = pv;
    pList->n = n + 1;
}

/* 0x1002F460 */
/* WHAT IT DOES: purpose unclear. Observably it reads a pair of numbers out of
 * a table using two selector bytes as a row and column, and if one flag bit is
 * set it rotates the first of the pair by half a turn of twelve -- six becomes
 * zero, zero becomes six -- which has the shape of a mirroring or opposite-
 * direction rule. The second number is looked up fresh so the rotation cannot
 * affect it. What the table describes is not established here. */
/* @implements 0x1002F460 d3d BrSelLookup */
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
uint32_t BrHookTakeA(BrHooks *pH, const void *pSrc)
{
    (void)pSrc;                 /* the callee 0x100378A0 ignores it */
    pH->f7C44 = 1;
    return pH->g0938;
}

/* 0x10034CA8 */
/* WHAT IT DOES: the twin of the routine above -- same raised flag, same
 * ignored argument, but it returns a different counter. Purpose equally
 * unclear. */
/* @implements 0x10034CA8 d3d BrHookTakeB */
uint32_t BrHookTakeB(BrHooks *pH, const void *pSrc)
{
    (void)pSrc;
    pH->f7C44 = 1;
    return pH->g3300;
}

/* ================================================================== */
/* 6. Peer table                                                      */
/* ================================================================== */

/* 0x10036030 */
/* WHAT IT DOES: finds which slot in the network player table belongs to a
 * given player. If that player is not there yet it gives back the first free
 * slot instead, so the same call both looks up and allocates; if the table is
 * full it reports failure. The local player is always slot zero. */
/* @implements 0x10036030 d3d BrPeerFind */
int BrPeerFind(const BrPeer *aPeers, uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((aPeers[i].f2C & BR_PEER_STATE_MASK) != 0u &&
            aPeers[i].f04 == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((aPeers[i].f2C & BR_PEER_STATE_MASK) == 0u)
            return i;
    }

    return -1;
}

/* 0x10035FE0 */
/* WHAT IT DOES: prepares one entity for use -- clears its state, works out its
 * own number from where it sits in the array, and links it to the matching
 * record in the parallel table so the two can find each other later. */
/* @implements 0x10035FE0 d3d BrEntInit */
void BrEntInit(BrEnt *pEnt, BrEnt *pBase, BrEntRec *pRecs)
{
    long idx;

    /* Written in this order by the original: +0x30, +0x2C, +0x44. */
    pEnt->f30 = 0;
    pEnt->f2C = 0;
    pEnt->f44 = 0;

    idx = (long)(pEnt - pBase);
    pEnt->f154 = (int32_t)idx;
    pEnt->f158 = &pRecs[idx];
}
