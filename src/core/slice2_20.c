/* slice2_20.c -- 0x100370D0-0x10039020 from BRD3D.dll.  See slice2_20.h.
 *
 * All field access here goes through memcpy-based helpers rather than casting
 * the file image to a struct pointer.  That is not defensive style, it is
 * required: the images are N64 data whose alignment the host cannot rely on,
 * and reading them by overlay would also make the byte order depend on the
 * host, which is exactly the bug this whole range exists to avoid.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

/* The original takes ONE argument: `mov esi,[esp+0x10]` after three
 * pushes is arg1, and `mov [esp+0x14],3` later homes a LOCAL in the
 * arg-1 slot -- there is no second parameter.  The port added cbFile
 * for a bounds check the original does not have.  Rename the header
 * prototype out of the way for this probe; the real fix is to drop
 * cbFile from slice2_20.h. */
#define BrRcaFixup BrRcaFixup_hdrdecl
#include "slice2_20.h"
#undef BrRcaFixup
#include "br_seg.h"
#include "br_bits.h"
#include "br_vec.h"

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* XSLICE 0x1002B9D0 */
/* Stores its argument to the global at 0x10675540.  Purpose unknown; called
 * once at the head of each fixup pass (0 for cars, 1 for tracks). */
extern void BrSegSetFlag(uint32_t v);

/* XSLICE 0x1002B9E0 */
/* Byte-swap n u16s in place.  n <= 0 is a no-op. */
extern void BrSwapU16Array(void *pv, int n);

/* XSLICE 0x1002BA00 */
/* Byte-swap n 8-byte records (four u16s each). */
extern void BrSwapRec8Array(void *pv, int n);

/* XSLICE 0x1002BA60 */
/* Byte-swap n Vec3s (stride 0x0C), i.e. n calls to BrSwapVec3. */
extern void BrSwapVec3Array(void *pv, int n);

/* XSLICE 0x1002BA80 */
/* Byte-swap and rebase n records of stride 0x24 (body at 0x1002BAA0). */
extern void BrSwapRec24Array(void *pv, int n);

/* XSLICE 0x1002BF40 */
/* Non-zero if pv is NULL or already in the registered display-list table at
 * 0x1067B550.  Callers use `== 0` to mean "not seen yet". */
extern int BrDlIsRegistered(const void *pv);

/* XSLICE 0x1002BF80 */
/* Register and byte-swap a display list. */
extern void BrDlRegister(void *pv);
extern void BrSegPtrFixup(uint32_t *p);

/* XSLICE 0x10074DC0 */
extern void BrSub10074DC0(int n);
/* XSLICE 0x10074E00 */
extern void BrSub10074E00(void);
/* XSLICE 0x1003445A */
extern void BrSub1003445A(void *pv);
/* XSLICE 0x10035BD1 */
extern void BrSub10035BD1(void);
/* XSLICE 0x10061010 */
extern void BrSub10061010(int iCar, int fPreview);
/* XSLICE 0x10037990 */
extern void BrSub10037990(const char *pszPath);

/* XSLICE 0x1003B170 */
/* One vector in, one float out.  Used here and at 0x10037B10 as `if (f != 0)
 * r = 1/f`, so almost certainly a length -- not verified in this packet. */
extern float BrVec3Len(const BrVec3 *pV);

/* XSLICE 0x1003BD50 */
extern int BrRand(void);

/* Checked stdio wrappers -- names and signatures as already declared by
 * slice1_01.h.  NOTE the FILE ** (not FILE *): the originals dereference it. */
/* XSLICE 0x10003170 */
extern void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile);
/* XSLICE 0x10003320 */
extern int BrChkFileExists(const char *pPath);
/* XSLICE 0x10002FE0 */
extern FILE **BrChkFReadOpen(const char *pPath);
/* XSLICE 0x10002F90 */
extern int BrChkFileSize(FILE **ppFile);
/* XSLICE 0x10003290 */
extern void BrChkFClose(FILE **ppFile);
/* XSLICE 0x1007C830 */
extern int BrSprintf(char *pDst, const char *pszFmt, ...);
/* XSLICE 0x10008CF0 */
extern void BrFatal(const char *pszMsg);

/* Backend dispatch, three function pointers in the DLL's data segment.  All
 * cdecl.  Handles are kept as uint32_t because the originals are 32-bit
 * values living inside the file image. */
/* XSLICE 0x118AA084 */
extern uint32_t (*g_pfn18AA084)(uint32_t hCtx, uint32_t hSrc, void *pDesc);
/* XSLICE 0x118AA0C4 */
extern void (*g_pfn18AA0C4)(void *pv);
/* XSLICE 0x118AA0C8 */
extern void (*g_pfn18AA0C8)(void *pRec, int flag);
/* XSLICE 0x118AA0CC */
extern void (*g_pfn18AA0CC)(void *pTable, int cRecords);

/* Plain globals. */
/* XSLICE 0x106C7C3C */ extern void    *g_p6C7C3C;
/* XSLICE 0x106C661C */ extern int      g_i6C661C;
/* XSLICE 0x106C6624 */ extern int      g_i6C6624;
/* XSLICE 0x100AC300 */ extern int      g_i0AC300;
/* XSLICE 0x104BBE08 */ extern int      g_i4BBE08;
/* XSLICE 0x100B8C90 */ extern int      g_i0B8C90;
/* XSLICE 0x10AA3444 */ extern int      g_i10AA3444;
/* XSLICE 0x10AA3460 */ extern int      g_i10AA3460;
/* XSLICE 0x100C12A0 */ extern uint8_t  g_ab0C12A0[];
/* XSLICE 0x100B84F8 */ extern const char *const g_apszCarFiles[];
/* XSLICE 0x100B80B8 */ extern const char *const g_apszTrackFiles[];
/* XSLICE 0x10220B20 */ extern uint32_t g_a220B20[0x46];

/* Particle-style pool, see slice2_20.h. */
/* XSLICE 0x10A99BB8 */ extern BrPoolNode g_aPoolNodes[];
/* XSLICE 0x10A99BA8 */ extern uint16_t   g_uPoolFree;
/* XSLICE 0x10A99BB0 */ extern uint16_t   g_uPoolHead;
/* XSLICE 0x106C2CFC */ extern float      g_f6C2CFC;


/* ==========================================================================
 * Endian / unaligned helpers
 * ==========================================================================
 *
 * BrSwap4 / BrSwap2 / BrRead32BE, the load environment g_BrLoad with
 * BrLoadResolve, BrRcaFixup (0x100370D0), BrFileReadInto, BrRcaLoadCar,
 * BrTrackLoadHandling, BrTrackHdrRead and BrGlTrackHdrRead (0x10031B80) now
 * live in src/core/gamedata/br_cartrackload.c.  The static helpers the
 * fixups below use are repeated here; the segment map is shared. */

static uint32_t BrRd32(const void *pv)
{
    uint32_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

static void BrWr32(void *pv, uint32_t v)
{
    memcpy(pv, &v, sizeof v);
}

static uint16_t BrRd16(const void *pv)
{
    uint16_t v;
    memcpy(&v, pv, sizeof v);
    return v;
}

static void BrWr16(void *pv, uint16_t v)
{
    memcpy(pv, &v, sizeof v);
}

/* Big-endian u32 at pv, written back as a host dword.  This is the second of
 * the two swap idioms in the original; numerically identical to BrSwap4. */
static void BrLoad32BE(void *pv)
{
    BrWr32(pv, BrRead32BE(pv));
}

/* Big-endian u16 at pv, written back as a host word. */
static void BrLoad16BE(void *pv)
{
    const uint8_t *p = (const uint8_t *)pv;
    BrWr16(pv, (uint16_t)(((uint16_t)p[0] << 8) | p[1]));
}

/* The segment map the original kept at 0x1057553C / 0x10575538; defined in
 * gamedata/br_cartrackload.c. */
extern BrSegMap s_seg;

/* Rebase the dword at pv in place, matching `push pv / call 0x1002B970`. */
static void BrFixupAt(void *pv)
{
    uint32_t v = BrRd32(pv);
    BrSegFixup(&s_seg, &v);
    BrWr32(pv, v);
}

/* The dword at pv, already rebased, as a host pointer. */
static void *BrPtrAt(const void *pv)
{
    return BrLoadResolve(BrRd32(pv));
}


/* ==========================================================================
 * 0x10038380 / 0x10038410 / 0x100382A0 / 0x10038250 -- geometry
 * ========================================================================== */

void BrSwapRec28(void *pvRec)
{
    uint8_t *p = (uint8_t *)pvRec;
    BrSwapVec3(p + 0x00);
    BrSwapVec3(p + 0x0C);
    BrSwapVec3(p + 0x18);
    BrSwap4   (p + 0x24);
}

void BrTrackFixupList84(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x84);
    int i;

    if (p == NULL)
        return;
    /* The count is re-read from the header every iteration in the original. */
    for (i = 0; i < (int)BrRd32(h + 0x88); ++i, p += 0x0C)
        BrSwapVec3(p);
}

void BrTrackFixupNode(void *pvNode)
{
    uint8_t *p = (uint8_t *)pvNode;
    uint8_t *q;
    int i, cRec;

    BrSwap4(p + 0x00); BrFixupAt(p + 0x00);
    BrSwap4(p + 0x04); BrFixupAt(p + 0x04);
    BrSwap4(p + 0x08); BrFixupAt(p + 0x08);
    BrSwap4(p + 0x0C); BrFixupAt(p + 0x0C);

    BrLoad16BE(p + 0x14);
    BrLoad16BE(p + 0x16);

    BrSwapRec28(p + 0x18);

    /* GOTCHA: the original's guard here is an unsigned "< 0" that can never
     * fire, and the loop bound is <=, so this runs count+1 times even when
     * the count is zero. */
    cRec = BrRd16(p + 0x14);
    q = p + 0x40;
    for (i = 0; i <= cRec; ++i, q += 0x28)
        BrSwapRec28(q);
}

void BrTrackFixupList78(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x78);
    int i;

    if (p == NULL)
        return;
    for (i = 0; i < (int)BrRd32(h + 0x7C); ++i, p += 4) {
        void *pNode;
        BrSwap4(p);
        BrFixupAt(p);
        pNode = BrPtrAt(p);
        if (pNode != NULL)
            BrTrackFixupNode(pNode);
    }
}

/* 0x100316D0 BrTrackFixupRec54 now lives in src/core/gamedata/br_trackrec54.c. */

void BrTrackFixupList60(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *p = (uint8_t *)BrPtrAt(h + 0x60);
    int i;

    if (p == NULL)
        return;
    for (i = 0; i < (int)BrRd32(h + 0x64); ++i, p += 0x54)
        BrTrackFixupRec54(p);
}

/* 0x10038450 BrTexCopyRecords now lives in src/core/startup/br_track.c. */

/* ==========================================================================
 * 0x10037FA0 / 0x10037E10 -- track fixup
 * ========================================================================== */

void BrTrackF08FromMax(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    const uint8_t *pSeg = (const uint8_t *)BrPtrAt(h + 0x24);
    const uint8_t *pArr = (const uint8_t *)BrPtrAt(h + 0x20);
    int n = 0, best = 0, i;

    if (pSeg != NULL)
        n = BrRd16(pSeg + 0x2000);

    if (n > 1 && pArr != NULL) {
        for (i = 1; i < n; ++i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > best)
                best = v;
        }
    }
    /* Index 0 is skipped and best starts at 0, so this is never less than 1. */
    BrWr32(h + 8, (uint32_t)(best + 1));
}

void BrTrackFixup(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    uint8_t *pSeg;
    uint8_t *pArr;
    int n, maxA = 0, maxB = 0, i;

    BrSwapVec3Array (BrPtrAt(h + 0x14), (int)BrRd32(h + 0x10));
    BrSwapRec24Array(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));

    pSeg = (uint8_t *)BrPtrAt(h + 0x24);
    BrSwapU16Array(pSeg, 0x1001);
    n = (pSeg != NULL) ? BrRd16(pSeg + 0x2000) : 0;
    BrSwapU16Array(BrPtrAt(h + 0x20), n);

    /* max over [+0x20][n-1 .. 1], counting DOWN in the original. */
    pArr = (uint8_t *)BrPtrAt(h + 0x20);
    if (n - 1 >= 1 && pArr != NULL) {
        for (i = n - 1; i >= 1; --i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > maxA)
                maxA = v;
        }
    }

    BrSwapU16Array(BrPtrAt(h + 0x90), maxA + 1);

    pArr = (uint8_t *)BrPtrAt(h + 0x90);
    if (maxA >= 1 && pArr != NULL) {
        for (i = maxA; i >= 1; --i) {
            int v = BrRd16(pArr + (size_t)i * 2);
            if (v > maxB)
                maxB = v;
        }
    }

    /* Scan forward from maxB to the first zero entry; that index is the
     * number of u16s to swap in the table at +0x8C. */
    pArr = (uint8_t *)BrPtrAt(h + 0x8C);
    if (pArr != NULL) {
        i = maxB;
        while (BrRd16(pArr + (size_t)i * 2) != 0)
            ++i;
        BrSwapU16Array(BrPtrAt(h + 0x8C), i);
    }

    BrTrackF08FromMax(h);
    BrSwapRec8Array(BrPtrAt(h + 0x0C), (int)BrRd32(h + 0x08));

    BrDlRegister(BrPtrAt(h + 0x50));
    BrSub10074DC0(4);
    if (g_i4BBE08 == 3)
        BrTexCopyRecords(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));
    g_pfn18AA0C4(BrPtrAt(h + 0x50));

    BrTrackFixupList60(h);
    BrSub10074DC0(1);
    g_pfn18AA0CC(BrPtrAt(h + 0x1C), (int)BrRd32(h + 0x18));

    pSeg = (uint8_t *)BrPtrAt(h + 0x6C);
    BrSwapU16Array(pSeg, 0x1001);
    n = (pSeg != NULL) ? BrRd16(pSeg + 0x2000) : 0;
    BrSwapU16Array(BrPtrAt(h + 0x68), n);

    BrTrackFixupList78(h);
    BrTrackFixupList84(h);
}

/* ==========================================================================
 * 0x10038B20  BrTrackFixupCmds
 * ========================================================================== */

void BrTrackFixupCmds(void *pvHdr)
{
    uint8_t *h = (uint8_t *)pvHdr;
    int32_t  cCmds;
    int32_t  i;
    int32_t  cVerts = 0;      /* ebp: survives across records -- see tag 5 */

    BrLoad32BE(h + 0x224);
    cCmds = (int32_t)BrRd32(h + 0x224);
    if (cCmds <= 0)
        return;

    for (i = 0; i < cCmds; ++i) {
        uint8_t *pRec = h + 0x164 + (size_t)i * 0x0C;
        int      tag  = (int8_t)pRec[8];
        uint8_t *pVtx;
        int32_t  k;

        switch (tag) {
        case 0:
        case 1:
        case 2:
            BrSwap4(pRec + 0x00);
            BrSwap4(pRec + 0x04);
            break;

        case 4:
            BrSwap4(pRec + 0x00);
            BrFixupAt(pRec + 0x00);
            BrLoad32BE(pRec + 0x04);
            cVerts = (int32_t)BrRd32(pRec + 0x04);
            pVtx = (uint8_t *)BrPtrAt(pRec + 0x00);
            if (cVerts > 0 && pVtx != NULL) {
                for (k = 0; k < cVerts; ++k, pVtx += 0x0C)
                    BrSwapVec3(pVtx);
            }
            break;

        case 5:
            BrSwap4(pRec + 0x00);
            BrFixupAt(pRec + 0x00);
            pVtx = (uint8_t *)BrPtrAt(pRec + 0x00);
            /* GOTCHA: no count of its own -- reuses the last tag-4 count. */
            if (cVerts > 0 && pVtx != NULL) {
                for (k = 0; k < cVerts; ++k, pVtx += 0x0C)
                    BrSwapVec3(pVtx);
            }
            break;

        case 3:
        case 6:
        case 7:
            BrSwap4(pRec + 0x00);
            break;

        default:
            /* tag > 7 (the `ja` in the original is unsigned, so a negative
             * tag byte also lands here) -- nothing at all. */
            break;
        }

        /* The count is re-read from the header every iteration. */
        cCmds = (int32_t)BrRd32(h + 0x224);
    }
}

/* 0x10039000 BrInit220B20 now lives in src/core/startup/br_track.c. */

/* ==========================================================================
 * 0x10039020  BrPoolEmit
 *
 * Constants read from .rdata; the addresses are the original's operands.
 * ========================================================================== */

#define BR_K_08F564  1.52587890625e-05f      /* 0x37800000, == 1/65536      */
#define BR_K_08F568 -1.0000000474974513e-03f /* 0xBA83126F                  */
#define BR_K_08F56C -1.0f                    /* 0xBF800000                  */
#define BR_K_08F570  0.25f                   /* 0x3E800000                  */
#define BR_K_08F574  1.0000000474974513e-03f /* 0x3A83126F                  */
#define BR_K_08F578 -1.5f                    /* 0xBFC00000                  */
#define BR_K_08F57C  1.5259021893143654e-05f /* 0x37800080                  */
#define BR_K_08F580  0.10000000149011612f    /* 0x3DCCCCCD                  */
#define BR_K_08F584  0.15000000596046448f    /* 0x3E19999A                  */
#define BR_K_08F588  1.0f                    /* 0x3F800000                  */
#define BR_K_08F58C  255.0f                  /* 0x437F0000                  */
#define BR_K_MULADD  0.20000000298023224f    /* 0x3E4CCCCD, inline operand  */

/* 0x1007C8A0 is __ftol: truncate toward zero, low dword before any clamp.
 * Only the low byte of the result is used at the one call site here. */
static int32_t BrFtol(float f)
{
    return (int32_t)f;
}

static float BrFldAt(const void *pv)
{
    float f;
    memcpy(&f, pv, sizeof f);
    return f;
}

static void BrFstAt(void *pv, float f)
{
    memcpy(pv, &f, sizeof f);
}

void BrPoolEmit(void *pvThis)
{
    uint8_t *pThis = (uint8_t *)pvThis;
    float    fE24  = BrFldAt(pThis + 0x0E24);
    float    f105C;
    float    fTmp1;
    float    fT;
    float    fU;
    BrVec3   vTmp;
    uint16_t idx;
    BrPoolNode *pNode;

    /* timer += ((rand & 0x1FFF)/65536 - fE24 * -0.001 - -1.0) * dt */
    f105C = BrFldAt(pThis + 0x105C)
          + (((float)(BrRand() & 0x1FFF) * BR_K_08F564 - fE24 * BR_K_08F568)
             - BR_K_08F56C) * g_f6C2CFC;
    BrFstAt(pThis + 0x105C, f105C);

    if (!(f105C > BR_K_08F570))
        return;                       /* fcom + test ah,0x41: <= or unordered */

    idx = g_uPoolFree;
    if (idx == 0)
        return;                       /* pool empty; index 0 is the sentinel */

    fTmp1 = fE24 * BR_K_08F574;
    BrFstAt(pThis + 0x105C, 0.0f);

    pNode = &g_aPoolNodes[idx];

    /* Unlink from the free list, link onto the live list. */
    {
        uint16_t uOldHead = g_uPoolHead;
        g_uPoolHead = idx;
        g_uPoolFree = pNode->uNext;
        pNode->uNext = uOldHead;
    }

    BrVec3Scale(&pNode->v0C, (const BrVec3 *)(const void *)pThis,
                BR_K_08F578 - fTmp1);

    BrVec3Sub(&vTmp, (const BrVec3 *)(const void *)(pThis + 0x00F0),
                     (const BrVec3 *)(const void *)pThis);
    BrVec3MulAdd(&vTmp, &vTmp,
                 (const BrVec3 *)(const void *)(pThis + 0x0020), BR_K_MULADD);
    BrVec3MulAdd(&vTmp, &vTmp,
                 (const BrVec3 *)(const void *)(pThis + 0x0010), BR_K_MULADD);

    fT = (float)(BrRand() & 0xFFFF) * BR_K_08F57C;

    BrVec3Sub(&pNode->v00,
              (const BrVec3 *)(const void *)(pThis + 0x1060), &vTmp);
    BrVec3MulAdd(&pNode->v00, &vTmp, &pNode->v00, fT * fT);

    fU = fTmp1 * BR_K_08F580 - BR_K_08F56C;

    /* The three dwords are copied as raw words in the original, but they are
     * the same BrVec3 that was just built. */
    BrFstAt(pThis + 0x1060 + 0, vTmp.x);
    BrFstAt(pThis + 0x1060 + 4, vTmp.y);
    BrFstAt(pThis + 0x1060 + 8, vTmp.z);

    pNode->f18 = fU * BR_K_08F584;
    pNode->b1E = (uint8_t)BrFtol(
        BR_K_08F588 / (BrVec3Len((const BrVec3 *)(const void *)(pThis + 0x1024))
                       + fU) * BR_K_08F58C);
    pNode->b1F = 0xFF;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
/* 0x10031660 BrTrackSetF08FromMax and 0x100316A0 BrTrackFixupAllRec54 now
 * live in src/core/startup/br_track.c. */

/* 0x10031960 -- swaps and fixes one record's three segment pointers. */
int BrTrackFixupSegRec();


#endif /* BR_MATCHING_BUILD */
