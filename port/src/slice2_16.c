/* slice2_16.c -- Boss Rally (BRD3D.dll), a later pass, 0x1001CD60..0x1002BC90.
 * See slice2_16.h for the per-function notes and gotchas. */

#include "slice2_16.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Local helpers                                                      */
/* ------------------------------------------------------------------ */

/* 0x1007C8A0 __ftol: truncate toward zero, return the low dword of the
 * 64-bit result. DEVIATION: inputs outside the 64-bit range are undefined in
 * C, so they are turned into 0 rather than whatever the x87 indefinite
 * value would truncate to. */
static int32_t br16_ftol(double x)
{
    long long v;

    if (!(x > -9.2233720368547758e18 && x < 9.2233720368547758e18))
        return 0;
    v = (long long)x;
    return (int32_t)(uint32_t)((unsigned long long)v & 0xFFFFFFFFu);
}

/* The four scissor fields are 12-bit two's complement. */
static int32_t br16_sext12(uint32_t v)
{
    int32_t t = (int32_t)(v & 0xFFFu);
    /* The original compares against 0x800 and subtracts 0x1000 only then;
     * the store happens twice, which is invisible here. */
    if (t >= 0x800)
        t -= 0x1000;
    return t;
}

static uint32_t br16_bswap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

/* Byte-swap the u32 that starts at p, byte-wise so the host's own endianness
 * never enters into it. */
static void br16_swap_u32_at(uint8_t *p)
{
    uint8_t t;
    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void br16_swap_u16_at(uint8_t *p)
{
    uint8_t t = p[0];
    p[0] = p[1];
    p[1] = t;
}

/* Read/write a u32 byte-wise in the host's order. The .rca payload has
 * already been swapped by the time these are read back. */
static uint32_t br16_ld32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void br16_st32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static uint16_t br16_ld16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

/* Advance the fade emitter's cursor by one command and hand back the slot
 * that was reserved. Every emit site in 0x1002AF10 / 0x1002B340 is this. */
static BrGfxWords *br16_fade_alloc(BrFadeState *pSt)
{
    BrGfxWords *p = pSt->pCmd;
    pSt->pCmd = p + 1;
    return p;
}

/* ================================================================== */
/* 1. F3D command handlers                                            */
/* ================================================================== */

/* 0x1001CD60 */
BrGfxWords *BrGbiSet0A79E8(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f0A79E8 = pCmd->w1;
    return pCmd + 1;
}

/* 0x1001CD80 */
BrGfxWords *BrGbiSet4C5174(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f4C5174 = pCmd->w1;
    return pCmd + 1;
}

/* 0x1001CF30 */
BrGfxWords *BrGbiSetScissor(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrGbiScissor *p = &pSt->scissor;

    p->ulx = br16_sext12(pCmd->w0 >> 12);
    p->uly = br16_sext12(pCmd->w0);
    p->lrx = br16_sext12(pCmd->w1 >> 12);
    p->lry = br16_sext12(pCmd->w1);
    /* `sar` in the original: arithmetic, so a negative extent stays
     * negative rather than becoming enormous. */
    p->w = (p->lrx - p->ulx + 4) >> 2;
    p->h = (p->lry - p->uly + 4) >> 2;
    return pCmd + 1;
}

/* 0x1001E790 */
BrGfxWords *BrGbiClearGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->geo.prev = pSt->geo.cur;
    pSt->geo.cur  = pSt->geo.cur & ~pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}

/* 0x10020F20 */
BrGfxWords *BrGbiSetGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->geo.prev = pSt->geo.cur;
    pSt->geo.cur  = pSt->geo.cur | pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}

/* 0x10020D60 */
BrGfxWords *BrGbiDList(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrGbiDLStack *p = &pSt->dl;

    if ((pCmd->w0 & 0x00FF0000u) == 0) {
        /* GOTCHA: the guard tests the value the counter is ABOUT to take and
         * then stores anyway, so slot 9 is written and reported both. */
        if (p->n + 1 == BR_GBI_DL_STACK_MAX)
            BrGbiStackOverflow(1);
        p->ap[p->n] = pCmd + 1;
        p->n += 1;
    }
    /* DEVIATION: 32-bit branch target reinterpreted as a pointer. */
    return (BrGfxWords *)(uintptr_t)pCmd->w1;
}

/* 0x10020DA0 -- takes no argument in the original. */
BrGfxWords *BrGbiEndDList(BrGbiState *pSt)
{
    BrGbiDLStack *p = &pSt->dl;

    if (p->n == 0)
        return NULL;
    p->n -= 1;
    return p->ap[p->n];
}

BrMat4 *BrGbiMtxProj(BrGbiMtxState *pSt)
{
    return (BrMat4 *)(void *)&pSt->aWords[0];
}

BrMat4 *BrGbiMtxSlot(BrGbiMtxState *pSt, int index)
{
    return (BrMat4 *)(void *)&pSt->aWords[BR_GBI_MTX_STACK_OFF + index * 16];
}

/* The ring push used on both modelview paths: 10 wraps to 0 before the
 * increment, so `top` only ever takes 1..10. */
static void br16_mtx_push(BrGbiMtxState *pSt)
{
    if (pSt->top == 10)
        pSt->top = 0;
    pSt->top += 1;
}

/* top == 0 means "no modelview matrix"; the two sites that build a pointer
 * from it substitute NULL. */
static BrMat4 *br16_mtx_current(BrGbiMtxState *pSt)
{
    if (pSt->top == 0)
        return NULL;
    return BrGbiMtxSlot(pSt, pSt->top);
}

/* 0x10020DC0 */
BrGfxWords *BrGbiMatrix(BrGbiState *pSt, BrGfxWords *pCmd, const BrMat4 *pIn)
{
    BrGbiMtxState *pM = &pSt->mtx;
    uint32_t       w0 = pCmd->w0;

    if ((w0 & 0x10000u) != 0) {
        /* projection: G_MTX_PUSH is ignored on this path. */
        if ((w0 & 0x20000u) != 0)
            memcpy(BrGbiMtxProj(pM), pIn, 16 * sizeof(float));
        else
            BrMat4Mul(pIn, BrGbiMtxProj(pM), BrGbiMtxProj(pM));
    } else if ((w0 & 0x20000u) != 0) {
        /* modelview load */
        if ((w0 & 0x40000u) != 0)
            br16_mtx_push(pM);
        memcpy(BrGbiMtxSlot(pM, pM->top), pIn, 16 * sizeof(float));
        pM->f5180 = 0;
    } else {
        /* modelview multiply: tmp = new * current, then store. */
        BrMat4 tmp;
        BrMat4Mul(pIn, br16_mtx_current(pM), &tmp);
        if ((w0 & 0x40000u) != 0)
            br16_mtx_push(pM);
        memcpy(BrGbiMtxSlot(pM, pM->top), &tmp, 16 * sizeof(float));
        pM->f5180 = 0;
    }

    BrMat4Mul(br16_mtx_current(pM), BrGbiMtxProj(pM), &pM->combined);
    return pCmd + 1;
}

/* 0x10020EF0 */
BrGfxWords *BrGbiPopMatrix(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrGbiMtxState *pM = &pSt->mtx;

    if (pM->top != 0) {
        pM->top -= 1;
        if (pM->top == 0)
            pM->top = 10;
    }
    return pCmd + 1;
}

/* 0x10020F80 */
BrGfxWords *BrGbiSet4C1694(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f1694 = pCmd->w1;
    BrGbiCall10020FA0(pCmd->w1);
    return pCmd + 1;
}

/* 0x10020F50 */
BrGfxWords *BrGbiDispatch10020F50(BrGbiState *pSt, BrGfxWords *pCmd)
{
    int sel = (int8_t)((pCmd->w0 >> 16) & 0xFFu);

    if (sel == 0)
        return BrGbiCall100243D0(pCmd);
    if (sel == 3)
        return BrGbiSet4C1694(pSt, pCmd);
    return pCmd + 1;
}

/* 0x10021510 */
BrGfxWords *BrGbiTileRect(BrGbiState *pSt, BrGfxWords *pCmd)
{
    uint32_t w0 = pCmd->w0;
    uint32_t w1 = pCmd->w1;

    (void)pSt;
    BrGbiCall10021560((int)((w1 >> 12) & 0xFFFu),
                      (int)(w1 & 0xFFFu),
                      (int)((w0 >> 12) & 0xFFFu),
                      (int)(w0 & 0xFFFu),
                      (int)((w1 >> 24) & 7u));
    /* GOTCHA: three commands consumed, not one. */
    return pCmd + 3;
}

/* 0x10021B80 */
BrGfxWords *BrGbiTileRectS(BrGbiState *pSt, BrGfxWords *pCmd)
{
    uint32_t w0 = pCmd->w0;
    uint32_t w1 = pCmd->w1;

    (void)pSt;
    BrGbiCall10021560((int)(((w1 >> 12) & 0xFFFu) << 2),
                      (int)((w1 & 0xFFFu) << 2),
                      (int)(((w0 >> 12) & 0xFFFu) << 2),
                      (int)((w0 & 0xFFFu) << 2),
                      (int)((w1 >> 24) & 7u));
    return pCmd + 1;
}

/* 0x10022350 */
void BrGbiLightVertex(const BrGbiLightState *pSt, const float *pSrc, float *pDst)
{
    float t;
    int   i;

    if (pSt->numLights == 0) {
        pDst[7] = pSt->off[0];
        pDst[8] = pSt->off[1];
        pDst[9] = pSt->off[2];
        return;
    }

    /* Add order as emitted: (src[5]*dir[0] + src[6]*dir[1]) + src[7]*dir[2],
     * where src[5] is the float at +0x14. */
    t = (pSrc[5] * pSt->dir[0] + pSrc[6] * pSt->dir[1]) + pSrc[7] * pSt->dir[2];

    if (t < 0.0f) {           /* 0x1008F3C8 == 0.0f */
        pDst[7] = pSt->ambient[0];
        pDst[8] = pSt->ambient[1];
        pDst[9] = pSt->ambient[2];
        return;
    }

    for (i = 0; i < 3; ++i) {
        float v = t * pSt->scale[i] + pSt->ambient[i];
        /* The original substitutes the literal 1.0f (0x3F800000) rather than
         * the limit it compared against; the limit at 0x1008F3C4 is 1.0f. */
        pDst[7 + i] = (v <= 1.0f) ? v : 1.0f;
    }
}

/* 0x10022DC0 */
int BrGbiClipCodes(const float *pVert)
{
    float w = pVert[6];       /* +0x18 */
    int   f = 0;

    if (w < 0.0f)                    f |= 0x01;
    if (pVert[3] + w < 0.0f)         f |= 0x02;   /* +0x0C first */
    if (w - pVert[3] < 0.0f)         f |= 0x04;
    if (pVert[1] + w < 0.0f)         f |= 0x08;   /* then +0x04 */
    if (w - pVert[1] < 0.0f)         f |= 0x10;
    if (pVert[2] + w < 0.0f)         f |= 0x20;   /* then +0x08 */
    if (w - pVert[2] < 0.0f)         f |= 0x40;
    return f;
}

/* 0x10024240 */
BrGfxWords *BrGbiMoveMemMatrix(BrGbiState *pSt, BrGfxWords *pCmd,
                               const void *pSrc)
{
    memcpy(&pSt->mtx.combined, pSrc, 16 * sizeof(float));
    return pCmd + 1;
}

/* 0x10024150  G_MOVEMEM.
 *
 * Jump table recovered from the DLL (byte table 0x1002421C, targets
 * 0x100241E8):  0x80 -> 0x10024179, 0x82 -> 0x10024185,
 * 0x84 -> 0x10024193, 0x86/88/8A/8C/8E/90/92/94 -> 0x100241AE,
 * 0x9E -> 0x100241A2, everything else in 0x80..0x9E -> 0x100241E2. */
BrGfxWords *BrGbiMoveMem(BrGbiState *pSt, BrGfxWords *pCmd, const void *pSrc)
{
    uint32_t idx = (pCmd->w0 >> 16) & 0xFFu;

    if (idx < 0x80u || idx > 0x9Eu)
        return pCmd + 1;

    switch (idx) {
    case 0x80:
        return BrGbiCall10024260(pCmd);
    case 0x82:
        pSt->f1698 = pCmd->w1;
        return pCmd + 1;
    case 0x84:
        pSt->f169C = pCmd->w1;
        return pCmd + 1;
    case 0x9E:
        return BrGbiMoveMemMatrix(pSt, pCmd, pSrc);
    case 0x86: case 0x88: case 0x8A: case 0x8C:
    case 0x8E: case 0x90: case 0x92: case 0x94: {
        /* dst = 0x104BBE38 + ((idx - 0x86) >> 1) * 16, length = w0 & 0xFFFF.
         * DEVIATION: the length comes straight out of the command and the
         * original does not check it; it is clamped to the record here so a
         * malformed list cannot walk off the light array. */
        size_t   slot = (size_t)((idx - 0x86u) >> 1);
        size_t   len  = (size_t)(pCmd->w0 & 0xFFFFu);
        size_t   room = sizeof pSt->lights.aRaw - slot * BR_GBI_LIGHT_SIZE;
        if (len > room)
            len = room;
        memcpy(&pSt->lights.aRaw[slot * BR_GBI_LIGHT_SIZE], pSrc, len);
        pSt->mtx.f5180 = 0;
        return pCmd + 1;
    }
    default:
        return pCmd + 1;
    }
}

/* 0x100242F0  G_MOVEWORD.
 *
 * Jump table recovered from the DLL (byte table 0x100243C0, targets
 * 0x100243AC): index 0x02 -> 0x1002431A, 0x0A -> 0x1002432D; 0x08 and 0x0E
 * reach the table but land on the default arm; everything outside 0x02..0x0E
 * is rejected by the range check. The index is the SIGN-EXTENDED low byte of
 * w0, so 0x80..0xFF go to the default too. */
BrGfxWords *BrGbiMoveWord(BrGbiState *pSt, BrGfxWords *pCmd)
{
    int      sel = (int8_t)(pCmd->w0 & 0xFFu);
    uint32_t off;
    size_t   slot;
    uint8_t *p;

    if ((uint32_t)(sel - 2) > 0xCu)
        return pCmd + 1;

    if (sel == 0x02) {
        pSt->light.numLights = (int32_t)((pCmd->w1 >> 5) & 0xFu);
        return pCmd + 1;
    }
    if (sel != 0x0A)
        return pCmd + 1;

    off  = (pCmd->w0 >> 8) & 0xFFFFu;
    slot = (size_t)(off >> 5);
    /* DEVIATION: `off` is a full 16 bits and the original scales it straight
     * into the light array with no bound. Out-of-range slots are dropped. */
    if (slot >= BR_GBI_LIGHT_SLOTS)
        return pCmd + 1;

    p = &pSt->lights.aRaw[slot * BR_GBI_LIGHT_SIZE];
    /* Low nibble 0 writes the record's first three bytes, anything else its
     * bytes 4..6. The original tests `off & 0xF` -- not `== 4`. */
    if ((off & 0xFu) == 0) {
        p[0] = (uint8_t)(pCmd->w1 >> 24);
        p[1] = (uint8_t)(pCmd->w1 >> 16);
        p[2] = (uint8_t)(pCmd->w1 >> 8);
    } else {
        p[4] = (uint8_t)(pCmd->w1 >> 24);
        p[5] = (uint8_t)(pCmd->w1 >> 16);
        p[6] = (uint8_t)(pCmd->w1 >> 8);
    }
    pSt->mtx.f5180 = 0;
    return pCmd + 1;
}

/* 0x10024A90 */
void BrGbiRun(const BrGbiHandler *apTable, BrGfxWords *pCmd)
{
    while (pCmd != NULL)
        pCmd = apTable[(pCmd->w0 >> 24) & 0xFFu](pCmd);
}

/* ================================================================== */
/* 2. Texture-load scanning pass                                      */
/* ================================================================== */

const void *BrGbiTexScanData(BrGbiTexScan *pSt, uint32_t addr)
{
    if (pSt->pfnData != NULL)
        return pSt->pfnData(pSt->pUser, addr);
    return (const void *)(uintptr_t)addr;
}

/* 0x10029E60 */
void BrGbiTexScanMark(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    if (pSt->pRunEnd == NULL)
        pSt->pRunEnd = pCmd;
}

/* 0x10029410 */
void BrGbiTexScanFlush(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    int id;

    if (pSt->state == 0)
        return;
    if (pSt->pRunEnd == NULL)
        pSt->pRunEnd = pCmd;

    id = BrGbiCall10029470(pSt->aStage);
    if (id != -1) {
        BrGfxWords *pRun = pSt->pRunStart;
        pRun->w0 = ((uint32_t)id & 0x00FFFFFFu) | 0xDC000000u;
        /* Length in 8-byte commands: the original does the pointer
         * subtraction and an arithmetic shift right by 3. */
        pRun->w1 = (uint32_t)(int32_t)(pSt->pRunEnd - pSt->pRunStart);
    }
    pSt->state = 0;
}

/* 0x10029E80  G_TEXTURE */
void BrGbiTexScanTexture(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    pSt->f5553E8 = (int32_t)((pCmd->w0 >> 8)  & 7u);
    pSt->f5553E0 = (int32_t)((pCmd->w0 >> 11) & 7u);
}

/* 0x10029EB0  G_SETTIMG */
void BrGbiTexScanSetImg(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    int32_t s = pSt->state;

    if (s != 0 && s != 3 && s != 6)
        return;

    pSt->timgSiz  = (int32_t)((pCmd->w0 >> 19) & 3u);
    pSt->timgAddr = pCmd->w1;
    pSt->srcSeen  = 0;
    if (s == 0) {
        pSt->pRunStart = pCmd;
        pSt->pRunEnd   = NULL;
    }
    pSt->state = 1;
}

/* 0x10029F10  G_LOADTLUT */
void BrGbiTexScanLoadTlut(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                          const void *pSrc)
{
    int32_t  ds, dt;
    uint32_t len;

    if (pSt->state != 1)
        return;

    ds = (int32_t)(pCmd->w1 & 0xFFFu) - (int32_t)(pCmd->w0 & 0xFFFu);
    dt = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
         (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    len = (uint32_t)((ds + 1) * (dt + 1)) << 1;

    pSt->srcSeen = pSt->timgAddr;
    /* DEVIATION: the destination is a caller-owned buffer and the length is
     * data-driven; the original does not check it and neither does this. */
    memcpy(pSt->pTlutDst, pSrc, len);
    pSt->state = 7;
}

/* 0x10029F80  G_RDPLOADSYNC */
void BrGbiTexScanLoadSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 1)
        pSt->state = 2;
}

/* 0x10029FA0  G_LOADBLOCK */
void BrGbiTexScanLoadBlock(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                           const void *pSrc)
{
    int32_t  d;
    uint32_t len, copy;

    if (pSt->state != 2)
        return;

    d = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
        (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    len = (uint32_t)(d + d + 2);

    pSt->stageSrc = pSt->timgAddr;
    pSt->stageLen = len;              /* the unclamped request, as published */

    /* DEVIATION: clamped to the staging buffer. */
    copy = len;
    if (copy > (uint32_t)BR_GBI_STAGE_SIZE)
        copy = (uint32_t)BR_GBI_STAGE_SIZE;
    memcpy(pSt->aStage, pSrc, copy);
    pSt->state = 3;
}

/* 0x1002A000  G_RDPPIPESYNC */
void BrGbiTexScanPipeSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 7)
        pSt->state = 8;
}

/* 0x1002A020  G_RDPTILESYNC */
void BrGbiTexScanTileSync(BrGbiTexScan *pSt)
{
    if (pSt->state == 3 || pSt->state == 7)
        pSt->state = 4;
}

/* 0x1002A040  G_SETTILE */
void BrGbiTexScanSetTile(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    int32_t    tile = (int32_t)((w1 >> 24) & 7u);
    BrGbiTile *p = &pSt->aTiles[tile];

    p->fmt     = (int32_t)((w0 >> 21) & 7u);
    p->siz     = (int32_t)((w0 >> 19) & 3u);
    p->line    = (int32_t)(((w0 >> 9) & 0x1FFu) << 3);
    p->tmem    = (int32_t)(w0 & 0x1FFu);
    p->mirrorS = (int32_t)((w1 >> 8)  & 1u);
    p->clampS  = (int32_t)((w1 >> 9)  & 1u);
    p->mirrorT = (int32_t)((w1 >> 18) & 1u);
    p->clampT  = (int32_t)((w1 >> 19) & 1u);
    p->maskS   = (int32_t)((w1 >> 4)  & 0xFu);
    p->maskT   = (int32_t)((w1 >> 14) & 0xFu);
    p->shiftS  = (int32_t)(w1 & 0xFu);
    p->shiftT  = (int32_t)((w1 >> 10) & 0xFu);

    /* maxTile is forced (not maximised) when a load is in flight. */
    if (pSt->state == 3 || pSt->state == 4 || pSt->state == 7 ||
        tile > pSt->maxTile)
        pSt->maxTile = tile;
    pSt->state = 5;
}

/* 0x1002A140  G_SETTILESIZE */
void BrGbiTexScanSetTileSize(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    BrGbiTile *p = &pSt->aTiles[(w1 >> 24) & 7u];

    p->uls = (int32_t)((w0 >> 12) & 0xFFFu);
    p->ult = (int32_t)(w0 & 0xFFFu);
    pSt->state = 6;
    p->lrs = (int32_t)((w1 >> 12) & 0xFFFu);
    p->lrt = (int32_t)(w1 & 0xFFFu);
}

/* 0x1002A1A0  G_SETOTHERMODE_L */
void BrGbiTexScanOtherModeL(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t v;

    if ((pCmd->w0 & 0xFF00u) != 0x300u)
        return;

    v = pCmd->w1;
    if (v == 0x504F50u || v == 0xC184240u || v == 0x504240u || v == 0) {
        pSt->f575414 = 0;
        return;
    }
    /* `test ah,0x18` -- bits 11 and 12 of w1. */
    if ((v & 0x1800u) == 0) {
        pSt->f575414 = 0;
        return;
    }
    pSt->f575414 = (int32_t)((v >> 16) & 1u);
}

/* 0x1002A250 */
void BrGbiTexScanOtherModeH0E(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t v = pCmd->w1;

    if (v == 0)
        return;
    if (v == 0x8000u)
        pSt->f5553DC = 0;
    else if (v == 0xC000u)
        pSt->f5553DC = 3;
}

/* 0x1002A210  G_SETOTHERMODE_H */
void BrGbiTexScanOtherModeH(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t sel = pCmd->w0 & 0xFF00u;

    if (sel == 0x0E00u) {
        BrGbiTexScanOtherModeH0E(pSt, pCmd);
        return;
    }
    if (sel == 0x1100u)
        pSt->f5544C = (pCmd->w1 == 0x40000u) ? 1 : 0;
}

/* 0x100290E0
 *
 * Opcode table recovered from the DLL (byte table 0x10029308, targets
 * 0x100292BC). The mapping is exactly F3D:
 *   0x04, 0xB1, 0xBF -> flush     0xB8 -> stop
 *   0xB9 -> othermode_l then mark 0xBA -> othermode_h
 *   0xBB -> texture               0xE6 -> loadsync
 *   0xE7 -> pipesync              0xE8 -> tilesync
 *   0xF0 -> loadtlut              0xF2 -> settilesize
 *   0xF3 -> loadblock             0xF5 -> settile
 *   0xFA -> prim colour           0xFB -> env colour
 *   0xFC -> combine probe         everything else -> mark
 *
 * DEVIATION: G_LOADTLUT and G_LOADBLOCK need the bytes their w1 points at.
 * The original dereferences the already-segment-fixed address; here the
 * walker goes through BrGbiTexScanData so a 64-bit host can supply them. */
void BrGbiTexScanRun(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    if (pCmd == NULL)
        return;

    pSt->f5544C    = 0;
    pSt->maxTile   = 0;
    pSt->f575448   = 0;
    pSt->state     = 0;
    pSt->pRunEnd   = NULL;

    for (;;) {
        uint32_t op = (pCmd->w0 >> 24) & 0xFFu;

        switch (op) {
        case 0xB8:                       /* G_ENDDL */
            return;

        case 0x04: case 0xB1: case 0xBF: /* G_VTX, G_TRI2, G_TRI1 */
            BrGbiTexScanFlush(pSt, pCmd);
            break;
        case 0xB9:                       /* G_SETOTHERMODE_L */
            BrGbiTexScanOtherModeL(pSt, pCmd);
            BrGbiTexScanMark(pSt, pCmd);
            break;
        case 0xBA:
            BrGbiTexScanOtherModeH(pSt, pCmd);
            break;
        case 0xBB:
            BrGbiTexScanTexture(pSt, pCmd);
            break;
        case 0xE6:
            BrGbiTexScanLoadSync(pSt);
            break;
        case 0xE7:
            BrGbiTexScanPipeSync(pSt);
            break;
        case 0xE8:
            BrGbiTexScanTileSync(pSt);
            break;
        case 0xF0:
            BrGbiTexScanLoadTlut(pSt, pCmd,
                                 BrGbiTexScanData(pSt, pSt->timgAddr));
            break;
        case 0xF2:
            BrGbiTexScanSetTileSize(pSt, pCmd);
            break;
        case 0xF3:
            BrGbiTexScanLoadBlock(pSt, pCmd,
                                  BrGbiTexScanData(pSt, pSt->timgAddr));
            break;
        case 0xF5:
            BrGbiTexScanSetTile(pSt, pCmd);
            break;
        case 0xFD:
            BrGbiTexScanSetImg(pSt, pCmd);
            break;

        case 0xFA:                       /* inline block 0x100291FA */
            pSt->prim[0] = (uint8_t)(pCmd->w1 >> 24);
            pSt->prim[1] = (uint8_t)(pCmd->w1 >> 16);
            pSt->prim[2] = (uint8_t)(pCmd->w1 >> 8);
            pSt->prim[3] = (uint8_t)(pCmd->w1);
            pSt->f575444 = 1;
            break;
        case 0xFB:                       /* inline block 0x10029233 */
            pSt->env[0] = (uint8_t)(pCmd->w1 >> 24);
            pSt->env[1] = (uint8_t)(pCmd->w1 >> 16);
            pSt->env[2] = (uint8_t)(pCmd->w1 >> 8);
            pSt->env[3] = (uint8_t)(pCmd->w1);
            pSt->f575440 = 1;
            break;
        case 0xFC:                       /* inline block 0x1002926D */
            /* One specific G_SETCOMBINE is recognised; every other combine
             * clears the flag. */
            pSt->f575448 = (pCmd->w0 == 0xFC50FE04u &&
                            pCmd->w1 == 0x3FFDF3F8u) ? 1 : 0;
            break;

        default:
            BrGbiTexScanMark(pSt, pCmd);
            break;
        }
        pCmd += 1;
    }
}

/* ================================================================== */
/* 2b. Texture upload thunks                                          */
/* ================================================================== */

/* 0x10027C00 */
int BrGbiSizeShift(int n)
{
    if (n <= 1)    return 0;
    if (n <= 2)    return 1;
    if (n <= 4)    return 2;
    if (n <= 8)    return 3;
    if (n <= 0x10) return 4;
    if (n <= 0x20) return 5;
    if (n <= 0x40) return 6;
    if (n <= 0x80) return 7;
    return 8;
}

/* 0x10028C70 */
int BrGbiTexelsPerWord(int siz)
{
    switch (siz) {
    case 0:  return 16;
    case 1:  return 8;
    case 2:  return 4;
    default: return 2;
    }
}

/* 0x10028BF0 */
void BrGbiBlit(BrGbiBlitFn pfn,
               uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
               uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8,
               uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12,
               uintptr_t a13, uintptr_t a14)
{
    int32_t   rounded = (int32_t)(1 << BrGbiSizeShift((int)a3));
    int32_t   pitch   = (rounded / BrGbiTexelsPerWord((int)a5)) * 8;

    pfn(a1, a2, a3, a4, (uintptr_t)(intptr_t)pitch,
        a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

/* 0x1002A280 */
void BrGbiTexCreate(BrGbiTexCreateFn pfn, BrGbiTexRec *pRec, uintptr_t a2)
{
    uint32_t flags, sel, fmt, siz;

    if (pRec->pTex == NULL)
        return;
    flags = pRec->flags;
    if ((flags & 0x100000u) != 0)
        return;

    sel = flags & 0x0F000000u;
    if (sel == 0x01000000u) {
        fmt = 0; siz = 2;
    } else if (sel == 0x04000000u) {
        fmt = 1; siz = 4;
    } else {
        fmt = 2; siz = 0;
    }

    pRec->pTex = pfn(pRec->pTex, pRec->f04,
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->w)),
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->h)),
                     fmt, siz,
                     (flags >> 31) & 1u, (flags >> 30) & 1u,
                     (flags >> 29) & 1u, (flags >> 28) & 1u,
                     0u, 0u, 1u, a2);
}

/* 0x1002A740 */
void BrGbiSolidTexBuild(BrGbiTexCreateFn pfn, BrGbiSolidTex *pSt)
{
    uint8_t fill = (pSt->mode == 2 || pSt->mode == 3) ? 0x20u : 0x80u;
    int     i;

    for (i = 0; i < 16; ++i)
        pSt->aTexels[i] = fill;

    pSt->pTex = pfn(pSt->aTexels, 0u, 4u, 4u, 1u, 4u,
                    0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u);
}

/* ================================================================== */
/* 3. Screen wipe / fade                                              */
/* ================================================================== */

/* 0x1002AEA0 */
int BrFadeRelease(BrFadeState *pSt)
{
    pSt->refCount -= 1;
    if (pSt->refCount == 0)
        pSt->pfnRelease();
    return 1;
}

/* 0x1002AEC0 */
void BrFadeLatch(BrFadeState *pSt)
{
    pSt->pos      = pSt->srcC0;
    pSt->f5754FC  = pSt->srcC4;
}

/* The 0x3EB / 0x3E8 / 0 token soup both emit paths hand to
 * BrRdpSetCombineLERP; spelled out once so the two call sites stay readable
 * and identical to the original's push order. */
static void br16_combine(BrGfxWords *pOut, int t13, int t9, int t5, int t1)
{
    BrRdpSetCombineLERP(pOut,
                        0, 0, 0, t1,
                        0, 0, 0, t5,
                        0, 0, 0, t9,
                        0, 0, 0, t13);
}

/* 0x1002AF10 */
void BrFadeDrawSprite(BrFadeState *pSt, const uint32_t *pRecs, float alpha)
{
    BrGfxWords     *p;
    BrGfxWords     *pE1;
    const uint32_t *pRec;
    uint32_t        lo, hi;

    /* NEGATED, and that is the whole point -- found by the equivalence audit.
     *
     *   1002AF14  fcomp dword ptr [0x1008F414]   ; 0.1f
     *   1002AF1A  fnstsw ax
     *   1002AF1C  test  ah, 1                    ; C0
     *   1002AF1F  jne   0x1002B120               ; -> ret
     *
     * `fcomp` sets C0 for LESS-THAN *and* for UNORDERED, so the original
     * returns on NaN and emits nothing. `alpha < 0.1f` is FALSE for NaN, so
     * the port fell through and emitted all ten display-list commands --
     * including a G_SETPRIMCOLOR whose alpha byte came from __ftol(NaN).
     *
     * NaN is reachable here: `over` is a duration and a zero `over` yields an
     * infinity that propagates into alpha (see slice2_16.h).
     *
     * This idiom was already understood in this very file -- BrFadeSetTarget
     * below uses `!(to < pSt->value)` for exactly this reason. It simply was
     * not applied here.
     *
     * The SECOND compare needs no negation: `test ah,0x41 / jne` fires only
     * when C0 and C3 are both clear, i.e. strictly greater AND ordered, and
     * `alpha > 0.7f` is already false for NaN. */
    if (!(alpha >= 0.1f))         /* 0x1008F414 */
        return;
    if (alpha > 0.7f)             /* 0x1008F418; the original overwrites the
                                   * incoming argument slot */
        alpha = 0.7f;

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA001402u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xB900031Du; p->w1 = 0x00504340u;
    br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt);
    p->w0 = 0xFA000000u;                      /* G_SETPRIMCOLOR */
    p->w1 = (uint32_t)br16_ftol((double)(alpha * 255.0f)) | 0xFFFFFF00u;

    p = br16_fade_alloc(pSt); p->w0 = 0xBA000602u; p->w1 = 0x000000C0u;

    pE1  = br16_fade_alloc(pSt);
    pRec = pRecs + (size_t)pSt->rectIdx * BR_FADE_RECT_DWORDS;

    lo = (pRec[3] + pRec[1]) & 0xFFFu;
    hi = ((pRec[2] + pRec[0]) << 12) & 0xFFF000u;
    pE1->w0 = 0xE1000000u | hi | lo;
    pE1->w1 = ((pRec[0] & 0xFFFu) << 12) | (pRec[1] & 0xFFFu);

    br16_combine(br16_fade_alloc(pSt), 0x3E8, 0x3E8, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA000602u; p->w1 = pSt->otherModeH;
}

/* 0x1002B130 */
void BrFadeSetTarget(BrFadeState *pSt, float to, float over)
{
    pSt->kick = 1;

    if (!(to < pSt->value) && to != 0.0f) {   /* 0x1008F410 == 0.0f */
        pSt->target = to;
        pSt->rate   = 1.0f / over;            /* 0x1008F420 == 1.0f */
        return;
    }

    /* GOTCHA: the guard is value != 1.0f, not == 1.0f -- the `jne` after
     * `test ah,0x40` leaves the bounce path only when the two are UNequal.
     * 0x1008F420 == 1.0f, 0x1008F428 == 0.0 (a double). */
    if (pSt->value != 1.0f && !(pSt->rate <= 0.0)) {
        pSt->bounce = 1;
        return;
    }
    pSt->target = to;
    pSt->rate   = -1.0f / over;               /* 0x1008F430 == -1.0f */
}

/* 0x1002B1C0 */
void BrFadeSetTargetA(BrFadeState *pSt, float to, float over)
{
    pSt->kickA = 1;
    pSt->tgtA  = to;

    if (!(to - pSt->curA < 0.0f) && to != 0.0f)
        pSt->rateA = 1.0f / over;
    else
        pSt->rateA = -1.0f / over;
}

/* 0x1002B220 */
void BrFadeSetTargetB(BrFadeState *pSt, float to, float over)
{
    pSt->kickB = 1;
    pSt->tgtB  = to;

    if (!(to - pSt->curB < 0.0f) && to != 0.0f)
        pSt->rateB = 1.0f / over;
    else
        pSt->rateB = -1.0f / over;
}

/* 0x1002B2A0 */
int BrFadeIsClosing(const BrFadeState *pSt)
{
    if (pSt->rate < 0.0f)
        return 1;
    return (pSt->bounce != 0) ? 1 : 0;
}

/* 0x1002B2D0 */
int BrFadeIsSettled(const BrFadeState *pSt)
{
    if (pSt->value != pSt->target)
        return 0;
    return (pSt->bounce != 0) ? 0 : 1;
}

/* 0x1002B300 */
int BrFadeIsShut(const BrFadeState *pSt)
{
    if (!(pSt->rate < 0.0f))
        return 0;
    if (pSt->value != 0.0f)
        return 0;
    return (pSt->bounce != 0) ? 0 : 1;
}

/* The 0xE1 command both bar-emitting arms build. */
static uint32_t br16_bar_w0(int32_t top, int32_t width, int32_t shift)
{
    uint32_t a = (uint32_t)((uint32_t)top << shift);
    uint32_t b = (uint32_t)((uint32_t)width << shift);

    a = (uint32_t)(a + 0xFFFFFu);
    a = (a << 12) & 0xFFF000u;
    b = (b - 1u) & 0xFFFu;
    return 0xE1000000u | a | b;
}

/* 0x1002B340 */
void BrFadeDrawBars(BrFadeState *pSt)
{
    BrGfxWords *p;

    if (pSt->value == 1.0f)            /* 0x1008F420 */
        return;

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA001402u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xB900031Du; p->w1 = 0x0F0A4000u;
    br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt);
    p->w0 = 0xE2000000u;
    {
        /* fild followed straight by __ftol: a round trip the compiler left
         * in, so this really is just the shifted integer. */
        uint32_t x = (uint32_t)br16_ftol((double)(pSt->width << pSt->shift))
                     & 0xFFFu;
        uint32_t y = (uint32_t)br16_ftol((double)(pSt->span  << pSt->shift))
                     & 0xFFFu;
        p->w1 = x | (y << 12);
    }

    p = br16_fade_alloc(pSt); p->w0 = 0xFA00FFFFu; p->w1 = 0;

    if (pSt->pos2 != 0) {
        /* Dead store in the original: the value read out of aPos2 with the
         * parity inverted is written to a stack local nothing reads. */
        (void)pSt->aPos2[pSt->parity ^ 1];

        if (BrFadeIsShut(pSt))
            pSt->bars = 3;

        p = br16_fade_alloc(pSt);
        p->w0 = 0xB900031Du; p->w1 = 0x00504340u;
        br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

        p = br16_fade_alloc(pSt); p->w0 = 0xFA000000u; p->w1 = 0xFFu;

        p = br16_fade_alloc(pSt);
        p->w1 = 0;
        p->w0 = br16_bar_w0(pSt->pos2, pSt->width, pSt->shift);
    }

    if (pSt->pos < pSt->span) {
        if (pSt->bars != 0) {
            p = br16_fade_alloc(pSt);
            pSt->bars -= 1;
            p->w0 = br16_bar_w0(pSt->span, pSt->width, pSt->shift);
            p->w1 = (uint32_t)(((uint32_t)pSt->pos << pSt->shift) & 0xFFFu)
                    << 12;
        }
    } else if (pSt->value == 0.0f && pSt->bars != 0) {   /* 0x1008F410 */
        p = br16_fade_alloc(pSt);
        pSt->bars -= 1;
        p->w0 = br16_bar_w0(pSt->span, pSt->width, pSt->shift);
        /* The original shifts a zero and masks it: always 0. */
        p->w1 = 0;
    }

    p = br16_fade_alloc(pSt); p->w1 = 0; p->w0 = 0xE7000000u;
}

/* One ramp step. Shared by the two ramp arms of 0x1002B670, which are
 * identical instruction for instruction.
 *
 * GOTCHA: unlike the wipe, a ramp that lands exactly on its target is left
 * alone -- the forward arm tests `cur <= tgt` where the wipe tests
 * `value < target`. */
static void br16_ramp_step(float *pCur, float tgt, float rate, float dt,
                           int32_t *pKick, uint8_t *pOut)
{
    if (*pKick != 0) {
        *pKick = 0;
    } else if (*pCur != tgt) {
        int forward = !(rate < 0.0f);
        *pCur = rate * dt + *pCur;
        if (forward) {
            if (!(*pCur <= tgt))
                *pCur = tgt;
        } else {
            if (*pCur < tgt)
                *pCur = tgt;
        }
    }
    *pOut = (uint8_t)br16_ftol((double)*pCur * 255.0);  /* 0x1008F438 */
}

/* 0x1002B670 */
void BrFadeTick(BrFadeState *pSt)
{
    if (pSt->kick != 0) {
        pSt->kick = 0;
    } else if (pSt->value != pSt->target) {
        int forward = !(pSt->rate < 0.0f);

        pSt->value = pSt->rate * pSt->dt + pSt->value;
        if (forward) {
            /* Overshoot INCLUDES equality here -- that is what lets the
             * bounce fire when the wipe lands exactly on its target. */
            if (!(pSt->value < pSt->target)) {
                pSt->value = pSt->target;
                if (pSt->bounce != 0) {
                    pSt->rate   = -pSt->rate;
                    pSt->target = 0.0f;
                    pSt->bounce = 0;
                }
            }
        } else {
            if (pSt->value < pSt->target)
                pSt->value = pSt->target;
        }
    }

    pSt->aPos2[pSt->parity] = pSt->pos2;
    pSt->aPos[pSt->parity]  = pSt->pos;
    pSt->f57550C            = 0;
    pSt->f5754FC            = pSt->width;

    if (pSt->rate > 0.0f) {
        int32_t v;
        pSt->pos2 = 0;
        v = br16_ftol((double)pSt->span * (double)pSt->value);
        pSt->pos = (v + 3) & ~3;
    } else if (pSt->rate < 0.0f) {
        int32_t v = br16_ftol((double)pSt->span * (double)pSt->value);
        int32_t step = ((pSt->span - v - pSt->pos2) + 3) & ~3;
        pSt->pos  += step;
        pSt->pos2 += step;
        if (pSt->pos > pSt->span)
            pSt->pos = pSt->span;
    } else {
        pSt->pos2 = 0;
        pSt->pos  = pSt->span;
    }

    br16_ramp_step(&pSt->curA, pSt->tgtA, pSt->rateA, pSt->dt,
                   &pSt->kickA, &pSt->outA);
    br16_ramp_step(&pSt->curB, pSt->tgtB, pSt->rateB, pSt->dt,
                   &pSt->kickB, &pSt->outB);
}

/* ================================================================== */
/* 4. .rca byte-swap and fixup helpers                                */
/* ================================================================== */

/* 0x1002B930 */
void BrCopy8Words(void *pDst, const void *pSrc)
{
    memcpy(pDst, pSrc, 8 * sizeof(uint32_t));
}

/* 0x1002B9C0 */
void BrRcaResetCounts(BrVtxCache *pCache, BrPtrList *pList)
{
    pCache->nEntries = 0;
    pList->n = 0;
}

/* 0x1002B9E0 */
void BrSwapU16Array(void *pv, int count)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;

    if (count <= 0)
        return;
    for (i = 0; i < count; ++i) {
        br16_swap_u16_at(p);
        p += 2;
    }
}

/* 0x1002BA20 -- fully unrolled in the original over offsets 0,2,4,6. */
void BrSwapU16x4(void *pv)
{
    uint8_t *p = (uint8_t *)pv;

    br16_swap_u16_at(p + 0);
    br16_swap_u16_at(p + 2);
    br16_swap_u16_at(p + 4);
    br16_swap_u16_at(p + 6);
}

/* 0x1002BA00 */
void BrSwapU16x4Array(void *pv, int count)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;

    if (count <= 0)
        return;
    for (i = 0; i < count; ++i) {
        BrSwapU16x4(p);
        p += 8;
    }
}

/* 0x1002BA60 */
void BrSwapVec3Array(void *pv, int count)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;

    if (count <= 0)
        return;
    for (i = 0; i < count; ++i) {
        BrSwapVec3(p);
        p += 12;
    }
}

/* 0x1002BC90 */
void BrRcaSwapMesh(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    uint32_t i;

    if (p == NULL)
        return;

    br16_swap_u16_at(p + 2);
    br16_swap_u32_at(p + 4);

    /* The count is re-read from +0x02 on every iteration, exactly as the
     * original does. */
    for (i = 0; i < (uint32_t)br16_ld16(p + 2); ++i) {
        uint8_t *e = p + 8 + i * 12;
        br16_swap_u32_at(e + 0);
        br16_swap_u32_at(e + 4);
        br16_swap_u32_at(e + 8);
    }
}

/* 0x1002BAA0 */
void BrRcaFixupRecord(const BrRcaFixup *pCtx, void *pRec)
{
    uint8_t *r = (uint8_t *)pRec;
    uint32_t flags;
    uint32_t len;
    uint8_t *pDst;
    uint8_t *pSrc;

    /* Byte-swap and rebase the two embedded pointers. */
    br16_swap_u32_at(r + 0x00);
    BrSegFixup(pCtx->pSeg, (uint32_t *)(void *)(r + 0x00));
    br16_swap_u32_at(r + 0x04);
    BrSegFixup(pCtx->pSeg, (uint32_t *)(void *)(r + 0x04));

    br16_swap_u32_at(r + 0x08);
    br16_swap_u16_at(r + 0x0C);
    br16_swap_u16_at(r + 0x0E);
    br16_swap_u16_at(r + 0x10);
    br16_swap_u16_at(r + 0x12);
    br16_swap_u16_at(r + 0x14);
    br16_swap_u16_at(r + 0x16);
    /* 0x18..0x1F are deliberately left alone. */

    /* The original stages +0x20 on the stack and reassembles it from four
     * unaligned byte reads; the net effect is a plain byte reversal. */
    flags = br16_bswap32(br16_ld32(r + 0x20));
    br16_st32(r + 0x20, flags);

    if (pCtx->enable == 0) {
        BrGbiCall10075330((void *)(uintptr_t)br16_ld32(r + 0x04));
        return;
    }

    /* Length of the SECOND payload: 0x20 when bits[27:24] are exactly 1,
     * 0x200 otherwise (an neg/sbb equality test, not a comparison). */
    len = ((flags & 0x0F000000u) == 0x01000000u) ? 0x20u : 0x200u;

    if ((flags & 0x00100000u) != 0) {
        uint8_t  *pMesh;
        uint32_t  off0, off1;
        int       entry = 0;

        BrSegFixup(pCtx->pSeg, (uint32_t *)(void *)(r + 0x08));
        pMesh = (uint8_t *)pCtx->pfnResolve(pCtx->pUser, br16_ld32(r + 0x08));
        BrRcaSwapMesh(pMesh);
        /* DEVIATION: the original dereferences the mesh header immediately
         * after 0x1002BC90 returns, without repeating that routine's own
         * null check, so a null header faults. Here it falls straight
         * through to the release call. */
        if (pMesh == NULL) {
            BrGbiCall10075330((void *)(uintptr_t)br16_ld32(r + 0x04));
            return;
        }

        if (br16_ld16(pMesh + 2) == 2 && br16_ld32(pMesh + 8) == 0xFFFFFFFFu)
            entry = 1;

        /* off0 = entry[e].dword1 at (e+1)*12, off1 = entry[e].dword2 at
         * e*12 + 0x10 -- the entries themselves start at +0x08, stride 12. */
        off0 = br16_ld32(pMesh + (size_t)(entry + 1) * 12);
        off1 = br16_ld32(pMesh + 0x10 + (size_t)entry * 12);

        if (entry == 0) {
            uint32_t n = flags & 0x0003FFFFu;
            if (n != 0 && off0 != 0xFFFFFFFFu) {
                /* GOTCHA: the destination taken from +0x00 is NOT null
                 * checked here, unlike the one at +0x04 below. */
                pDst = (uint8_t *)pCtx->pfnResolve(pCtx->pUser,
                                                   br16_ld32(r + 0x00));
                if (pDst != NULL)
                    memcpy(pDst, pCtx->pBlob + off0, n);
            }
        }

        pDst = (uint8_t *)pCtx->pfnResolve(pCtx->pUser, br16_ld32(r + 0x04));
        if (pDst == NULL || off1 == 0xFFFFFFFFu) {
            BrGbiCall10075330(pDst);
            return;
        }
        pSrc = pCtx->pBlob + off1;
    } else {
        /* +0x08 is a 12-bit index scaled by 32 rather than a pointer. */
        uint32_t idx = br16_ld32(r + 0x08) & 0xFFFu;

        pDst = (uint8_t *)pCtx->pfnResolve(pCtx->pUser, br16_ld32(r + 0x04));
        if (pDst == NULL) {
            BrGbiCall10075330(pDst);
            return;
        }
        pSrc = pCtx->pBlob + (idx << 5);
    }

    memcpy(pDst, pSrc, len);
    BrGbiCall10075330(pDst);
}

/* 0x1002BA80 */
void BrRcaFixupArray(const BrRcaFixup *pCtx, void *pv, int count)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;

    if (count <= 0)
        return;
    for (i = 0; i < count; ++i) {
        BrRcaFixupRecord(pCtx, p);
        p += BR_RCA_REC_SIZE;
    }
}
