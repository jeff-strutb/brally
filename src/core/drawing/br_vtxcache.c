/* br_vtxcache.c -- drawing: turning the console's geometry into the PC's.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_05.c, an address batch and not a module.  Two things
 * that belong together sit here: the cache that unpacks a batch of N64
 * vertices once and hands the same copy back afterwards, and the repairs
 * made to the console display-list instructions that reference those
 * batches.  0x1002C1F0's fixed-length pointer list is contiguous with the
 * fixups in the original and comes with them.
 *
 * See slice1_05.h for the per-function notes and gotchas.
 *
 * ‼ br_gamestep.h IS LOAD-BEARING and must not be dropped as an unused
 * include.  Nothing here calls into it, but without it 0x10019210 loses its
 * match (12 differing bytes, and the best variant slides from /O2 to
 * /O2 /Oy-).  br_rdpmode.c carries the same warning for the same reason.
 */
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
#define BrPtrListAdd      BrPtrListAdd_hdr
#define BrF3DVtxFixup     BrF3DVtxFixup_hdr
#define BrVtxSwap         BrVtxSwap_hdr     /* defined on the vertex struct */
#include "slice1_05.h"
#include "br_gamestep.h"
#include "slice2_16.h"    /* g_brRca67B548/54C, for 0x10018A30 */
#define BrPtrListContains BrPtrListContains_port
#include "slice2_17.h"    /* BrPtrList, for 0x10019000 */
#undef BrPtrListContains
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#undef BrVtxSwap
#else
#include "slice1_05.h"
#include "br_gamestep.h"
#include "slice2_16.h"
#include "slice2_17.h"
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
/* One 16-byte source vertex, walked as the struct-pointer PARAMETER (a local
 * cast from void* is not enough): that is what anchors
 * VC5's pointer at +0 (a byte pointer stepped by 16 anchors it at +2).  The
 * first two fields are a byte pack (`xor edx,edx; mov dh; mov dl`); the last
 * four are a byte swap of the 16-bit VALUE -- `(u8)(s >> 8) | ((u8)s << 8)`
 * -- which VC5 turns into byte loads in the other order (dl first).  Both
 * spellings are needed; operand order alone is canonicalised. */
typedef struct BrVtxSrc16 { unsigned char b[16]; } BrVtxSrc16;

void BrVtxSwap(BrVtxSrc16 *v, int count)
{
    int i;

    for (i = 0; i < count; ++i, ++v) {
        unsigned short *s = (unsigned short *)v;

        *(unsigned short *)&v->b[0] = (unsigned short)((v->b[0] << 8) | v->b[1]);
        *(unsigned short *)&v->b[2] = (unsigned short)((v->b[2] << 8) | v->b[3]);
        s[2] = (unsigned short)((unsigned char)(s[2] >> 8) | ((unsigned char)s[2] << 8));
        s[3] = (unsigned short)((unsigned char)(s[3] >> 8) | ((unsigned char)s[3] << 8));
        s[4] = (unsigned short)((unsigned char)(s[4] >> 8) | ((unsigned char)s[4] << 8));
        s[5] = (unsigned short)((unsigned char)(s[5] >> 8) | ((unsigned char)s[5] << 8));
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
/* @t4-pass 0x10018EF0 1 2026-09-10 probes 60 bytes 203 insns 55 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018EF0 2 2026-09-10 probes 60 bytes 203 insns 55 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018EF0 3 2026-09-10 probes 228 bytes 203 insns 55 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018EF0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 203/203 insns 55/55 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * residue after tools/crank.py: 228 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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

/* 0x1002B9C0 */
/* WHAT IT DOES: empties the vertex cache and the pointer list, so the next
 * batch of loaded geometry starts from nothing. */
/* @implements 0x1002B9C0 d3d BrRcaResetCounts */
/* @implements 0x10018A30 glide BrRcaResetCounts */
#ifdef BR_MATCHING_BUILD
void BrRcaResetCounts(void)
{
    g_brRca67B54C = 0;
    g_brRca67B548 = 0;
}
#else
void BrRcaResetCounts(BrVtxCache *pCache, BrPtrList *pList)
{
    pCache->nEntries = 0;
    pList->n = 0;
}
#endif

/* 0x1002BF40 */
/* WHAT IT DOES: asks whether something is already in a list. A null thing is
 * reported as present without the list being looked at, which is how callers
 * get "nothing to do" for free. */
/* @implements 0x1002BF40 d3d BrPtrListContains */
#ifdef BR_MATCHING_BUILD
/* The original takes only pv and reads the list at two absolute addresses
 * (count 0x1067B548, array 0x1067B550).  The port passes the list in. */
extern int   g_br67B548;
extern void *g_br67B550[];

int BrPtrListContains(const void *pv)
{
    int i;
    int n;

    if (pv == NULL)
        return 1;                    /* NULL short-circuits to "present" */

    n = g_br67B548;
    for (i = 0; i < n; ++i)
        if (g_br67B550[i] == pv)
            return 1;

    return 0;
}
#else
int BrPtrListContains(const BrPtrList *pList, const void *pv)
{
    int i;

    if (pv == NULL)
        return 1;                    /* NULL short-circuits to "present" */

    if (pList->n <= 0)
        return 0;

    for (i = 0; i < pList->n; ++i)
        if (pList->ap[i] == pv)
            return 1;

    return 0;
}
#endif

/* 0x1002BF80 (d3d) == 0x10019040 (glide) */
/* WHAT IT DOES: walks a console display list from the top, and for every
 * instruction byte-swaps its two words from the console's big-endian order
 * into the host's before handing them to the per-instruction repair. It
 * registers the list, then loops: swap w0/w1 in place, look at the command
 * byte (the top byte of w0), and fix up vertex-load, one-triangle,
 * two-triangle and segment-pointer instructions; the end instruction (0xb8)
 * stops the walk. A null list does nothing. */
/* @t4-pass 0x10019040 1 2026-09-07 probes 54 bytes 482 insns 138 regions 5 rows 74 census yes  (tools/crank.py) */
/* @t4-pass 0x10019040 2 2026-09-07 probes 54 bytes 482 insns 138 regions 5 rows 74 census yes  (tools/crank.py) */
/* @implements 0x10019040 glide BrF3DListFixup */
#ifdef BR_MATCHING_BUILD
/* BrPtrListAdd and the four fixups are the file's own definitions above /
 * the header's; BrSegPtrFixup is the 1-arg segment repair BrF3DVtxFixup also
 * calls. The two words are read byte-by-byte big-endian, stored back, and
 * dispatched on the command byte through the original's jump table (fn.py
 * reports the object's appended index+jump tables as extra bytes; the
 * original's live outside its 182 code bytes).
 *
 * The loop is a `goto`: VC5 strength-reduces a for/while loop's `p + 4`
 * (the w1 pointer) into a second induction variable (`add edi,8` in every
 * arm), where a goto loop recomputes `lea edi,[esi+4]` each pass as the
 * original does.  RESIDUE 2 insns: the first byte pack loads b1 before b0
 * (`mov al` / `mov ah` swapped); every spelling of that pack probed
 * (pairwise, nested, compound, u16-value swap, struct view, declaration and
 * store order) gives the same order. */
void BrF3DListFixup(BrGfxWords *pCmd)
{
    unsigned char *p;
    uint32_t a, b;

    if (pCmd == NULL)
        return;

    BrPtrListAdd(pCmd);
next:
    p = (unsigned char *)pCmd;
    a = p[0];
    a = (a << 8) | p[1];
    a = (a << 8) | p[2];
    a = (a << 8) | p[3];
    pCmd->w0 = a;
    b = p[4];
    b = (b << 8) | p[5];
    b = (b << 8) | p[6];
    b = (b << 8) | p[7];
    pCmd->w1 = b;
    switch ((unsigned short)(((uint32_t)((int32_t)a >> 24)) & 0xffu)) {
    case 0x04: BrF3DVtxFixup(pCmd); break;
    case 0xb1: BrF3DTri2Fixup(pCmd); break;
    case 0xb8: return;
    case 0xbf: BrF3DTri1Fixup(pCmd); break;
    case 0xfd: BrSegPtrFixup(&pCmd->w1); break;
    }
    pCmd += 1;
    goto next;
}
#endif
