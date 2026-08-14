/* slice1_07.c -- Boss Rally (BRD3D.dll) slice 1, a later pass.
 * Range 0x1005F580 - 0x1006C740.  See slice1_07.h for the interfaces and
 * work/slice1/agent07.asm for the disassembly this was recovered from.
 */

#include "slice1_07.h"

#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* 0x1005F800  rectangle grid tables                                     */
/* ===================================================================== */

/* The original does col/row with the standard compiler idioms for signed
 * division (abs/xor for %8, magic-multiply for /5 and /3), which are exactly
 * C's truncating semantics.  The loop counter is never negative, so plain
 * / and % reproduce it. */
void BrRectGridFill(BrRectI *pDst, int32_t count, int32_t cols,
                    int32_t cellW, int32_t cellH)
{
    int32_t i;

    for (i = 0; i < count; i++) {
        int32_t x = (i % cols) * cellW;
        int32_t y = (i / cols) * cellH;
        pDst[i].x0 = x;
        pDst[i].y0 = y;
        pDst[i].x1 = x + cellW;
        pDst[i].y1 = y + cellH;
    }
}

void BrRectTablesInit(BrRectI *pA, BrRectI *pB, BrRectI *pC, BrRectI *pD)
{
    BrRectGridFill(pA, BR_GRID_A_COUNT, BR_GRID_A_COLS,
                   BR_GRID_A_CELLW, BR_GRID_A_CELLH);
    BrRectGridFill(pB, BR_GRID_B_COUNT, BR_GRID_B_COLS,
                   BR_GRID_B_CELLW, BR_GRID_B_CELLH);
    BrRectGridFill(pC, BR_GRID_C_COUNT, BR_GRID_C_COLS,
                   BR_GRID_C_CELLW, BR_GRID_C_CELLH);
    BrRectGridFill(pD, BR_GRID_D_COUNT, BR_GRID_D_COLS,
                   BR_GRID_D_CELLW, BR_GRID_D_CELLH);
}

/* ===================================================================== */
/* 0x1005FF30  clear three 64-dword tables                               */
/* ===================================================================== */

void BrTables64Clear(uint32_t *pA, uint32_t *pB, uint32_t *pC)
{
    memset(pA, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(pB, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(pC, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
}

/* ===================================================================== */
/* 0x10060280  clear an object's tail fields                             */
/* ===================================================================== */

void BrDevSlotClear(BrDevSlot *pSlot)
{
    /* Original store order: +0x50 first, then +0x2C..+0x4C ascending. */
    pSlot->pIface = NULL;
    pSlot->f2C = 0;
    pSlot->f30 = 0;
    pSlot->f34 = 0;
    pSlot->f38 = 0;
    pSlot->f3C = 0;
    pSlot->f40 = 0;
    pSlot->f44 = 0;
    pSlot->f48 = 0;
    pSlot->f4C = 0;
}

/* ===================================================================== */
/* 0x10060030  error message box                                         */
/* ===================================================================== */

static BrStringLookupFn s_pfnStringLookup = NULL;
static BrMessageBoxFn   s_pfnMessageBox   = NULL;

void BrSetStringLookupFn(BrStringLookupFn fn) { s_pfnStringLookup = fn; }
void BrSetMessageBoxFn(BrMessageBoxFn fn)     { s_pfnMessageBox = fn; }

void BrErrorBox(void *hWnd, int32_t unusedCode, const char *pText)
{
    const char *pCaption;

    /* The second argument really is dead in the original -- the value is
     * pushed by every caller and never loaded.  Kept in the signature so
     * call sites transfer unchanged. */
    (void)unusedCode;

    /* DEVIATION: the original calls 0x10074030 with a single argument (the
     * string id 0xAA) and then MessageBoxA.  Both are replaced by hooks so
     * this file stays free of Win32 and of the string table's globals.
     *
     * GOTCHA for whoever owns 0x10074030: br_bits.h declares it as
     * BrHandleLookup(apTable, handle), i.e. two arguments.  Both call sites
     * in this range disagree -- 0x100602E0 does `push 0xAC / call / add
     * esp,4`, exactly one argument.  At 0x10060030 the apparent second push
     * is really the uType=0 for MessageBoxA pushed early and left on the
     * stack (only 4 bytes are cleaned after the call).  So the routine takes
     * one argument: a string id. */
    pCaption = s_pfnStringLookup ? s_pfnStringLookup(BR_ERRSTR_MESSAGEBOX)
                                 : NULL;

    if (s_pfnMessageBox)
        s_pfnMessageBox(hWnd, pText, pCaption, 0u);
}

/* ===================================================================== */
/* 0x10060F00  24bpp BGR bottom-up -> 32bpp RGBA top-down                */
/* ===================================================================== */

void BrBgr24ToRgbaFlip(uint8_t *pDst, const uint8_t *pSrc,
                       int32_t width, int32_t height, int32_t srcPitch)
{
    const uint8_t *pRow;
    uint8_t *d;
    int32_t x, y;

    if (height == 0)
        return;                       /* explicit guard in the original */

    pRow = pSrc + (ptrdiff_t)(height - 1) * srcPitch;
    d = pDst;

    /* DEVIATION: the original's outer loop is `dec ecx / jne`, a do-while on
     * the row count.  A negative `height` therefore runs ~2^32 iterations and
     * scribbles far past the destination.  This is a signed `<` compare
     * instead.  Behaviour is identical for height >= 0, which is all the one
     * caller (0x10060EA0) can produce: it allocates width*height*4 first and
     * bails when the allocation fails, and a negative height makes that
     * allocation fail. */
    for (y = 0; y < height; y++) {
        const uint8_t *s = pRow;
        for (x = 0; x < width; x++) {
            uint8_t b = s[0];
            uint8_t g = s[1];
            uint8_t r = s[2];
            s += 3;
            *d++ = r;
            *d++ = g;
            *d++ = b;
            *d++ = 0xFF;
        }
        pRow -= srcPitch;
    }
}

/* ===================================================================== */
/* 0x10060EA0  24bpp BITMAP -> malloc'd RGBA                             */
/* ===================================================================== */

BrImgState BrImgTintState;

void *BrBmp24ToRgba(const BrBitmap *pBmp)
{
    uint32_t bytes;
    uint8_t *pOut;

    if (pBmp->bmBitsPixel != 24)
        return NULL;                  /* `cmp word [esi+0x12],0x18` */

    /* The original computes height*width*4 as a wrapping 32-bit product;
     * done in uint32_t here so overflow stays defined. */
    bytes = (uint32_t)pBmp->bmHeight * (uint32_t)pBmp->bmWidth * 4u;

    pOut = (uint8_t *)malloc(bytes);
    if (pOut == NULL)
        return NULL;                  /* globals are NOT updated on failure */

    BrBgr24ToRgbaFlip(pOut, (const uint8_t *)pBmp->bmBits,
                      pBmp->bmWidth, pBmp->bmHeight, pBmp->bmWidthBytes);

    BrImgTintState.width  = pBmp->bmWidth;
    BrImgTintState.height = pBmp->bmHeight;
    return pOut;
}

/* ===================================================================== */
/* 0x10061480 / 0x100615B0  colour-keyed tint                            */
/* ===================================================================== */

/* The key: byte0 == 0 and byte1 == byte2, i.e. R == 0 and G == B. */
static int BrPixIsKeyed(const uint8_t *p)
{
    return p[0] == 0 && p[1] == p[2];
}

/* g * scale / 255.  The original multiplies with a wrapping 32-bit `imul`
 * and then divides by 255 via the 0x80808081 magic sequence, which truncates
 * toward zero exactly like C's `/`.  The product is computed through
 * uint32_t so the wrap stays defined; the store is an 8-bit truncation. */
static uint8_t BrTintChannel(int32_t g, int32_t scale)
{
    int32_t n = (int32_t)((uint32_t)g * (uint32_t)scale);
    return (uint8_t)(n / 255);
}

int32_t BrImgTintBlit(const uint8_t *pSrc, int32_t x0, int32_t x1,
                      int32_t y0, int32_t y1, uint8_t *pDst,
                      int32_t dstPitchPixels, int32_t dstHeight)
{
    const uint8_t *pRow;
    int32_t w, h, x, y;

    if (pSrc == NULL)
        return 1;

    w = x1 - x0;
    h = y1 - y0;
    if (h <= 0)
        return 1;

    pRow = pSrc;
    for (y = 0; y < h; y++) {
        if (w > 0) {
            /* dst row index counts DOWN as the source row counts up:
             *   row = dstHeight - y0 - y - 1 */
            const uint8_t *s = pRow;
            uint8_t *d = pDst + 4 * ((ptrdiff_t)dstPitchPixels
                                     * (dstHeight - y0 - y - 1) + x0);
            for (x = 0; x < w; x++) {
                /* The original copies the whole 32-bit pixel first and only
                 * then overwrites bytes 0..2 for keyed pixels, so the alpha
                 * byte always survives. */
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = s[3];
                if (BrPixIsKeyed(s)) {
                    int32_t g = s[1];
                    d[0] = BrTintChannel(g, BrImgTintState.scaleR);
                    d[1] = BrTintChannel(g, BrImgTintState.scaleG);
                    d[2] = BrTintChannel(g, BrImgTintState.scaleB);
                }
                s += 4;
                d += 4;
            }
        }
        pRow += (ptrdiff_t)w * 4;     /* source rows are w pixels wide */
    }
    return 1;                         /* the original has no failure path */
}

int32_t BrImgHasKeyed(const uint8_t *pPixels, int32_t x0, int32_t x1,
                      int32_t y0, int32_t y1)
{
    const uint8_t *pRow;
    int32_t w, h, x, y;

    if (pPixels == NULL)
        return 0;

    w = x1 - x0;
    h = y1 - y0;
    if (h <= 0)
        return 0;

    pRow = pPixels;
    for (y = 0; y < h; y++) {
        const uint8_t *s = pRow;
        for (x = 0; x < w; x++) {
            if (BrPixIsKeyed(s))
                return 1;
            s += 4;
        }
        pRow += (ptrdiff_t)w * 4;
    }
    return 0;
}

/* ===================================================================== */
/* 0x10060F70 / 0x10060FB0  40-byte record accessors                     */
/* ===================================================================== */

int32_t BrRec10LastDw4;

int32_t BrRec10Get(const BrRec10 *pTable, int32_t a, int32_t b, int32_t index)
{
    /* Address arithmetic reproduced flat, exactly as the original's
     * lea chain builds it: base + 1200*a + 40*b, then + 4*index. */
    const int32_t *pRec = (const int32_t *)pTable
                        + ((ptrdiff_t)a * BR_REC10_COLS + b) * BR_REC10_DWORDS;

    BrRec10LastDw4 = pRec[4];         /* 0x100ADFE0 = base + 0x10 */
    return pRec[index];               /* no bounds check -- see header */
}

void BrRec10Get4(const BrRec10 *pTable, int32_t a, int32_t b,
                 int32_t *pDw5, int32_t *pDw6, int32_t *pDw7, int32_t *pDw8)
{
    const int32_t *pRec = (const int32_t *)pTable
                        + ((ptrdiff_t)a * BR_REC10_COLS + b) * BR_REC10_DWORDS;

    *pDw5 = pRec[5];                  /* 0x100ADFE4 */
    *pDw6 = pRec[6];                  /* 0x100ADFE8 */
    *pDw7 = pRec[7];                  /* 0x100ADFEC */
    *pDw8 = pRec[8];                  /* 0x100ADFF0 */
}

/* ===================================================================== */
/* 0x1006ABA0 / 0x1006AE00  24-byte record arithmetic                    */
/* ===================================================================== */

uint32_t BrG_B502E8;
uint32_t BrG_B502EC;

uint32_t BrRec24TotalBytes(void)
{
    return BrG_B502E8 * 24u;          /* lea eax,[eax+eax*2] ; shl eax,3 */
}

void BrRec24SetCount(uint32_t bytes)
{
    BrG_B502EC = bytes / 24u;         /* mul 0xAAAAAAAB ; shr edx,4 */
}

/* ===================================================================== */
/* 0x1006C740  point in triangle                                         */
/* ===================================================================== */

/* fld x ; fcomp 0.0 ; test ah,1 ; fchs -- C0 means "less than or
 * unordered", so a NaN gets its sign flipped, which changes nothing. */
static float BrAbsF(float v)
{
    return v < 0.0f ? -v : v;
}

int16_t BrTriContainsPoint2D(const BrTri *pTri, const float *pPoint)
{
    float nx = BrAbsF(pTri->n[0]);
    float ny = BrAbsF(pTri->n[1]);
    float nz = BrAbsF(pTri->n[2]);
    int32_t k, i1, i2;
    float px, py, bx, by, cx, cy, u, v;

    /* Drop the dominant axis.  Comparisons are strict `>`, so ties go to the
     * later axis: |nx| == |ny| == |nz| selects 2, not 0. */
    if (nx > ny)
        k = (nx > nz) ? 0 : 2;
    else
        k = (ny > nz) ? 1 : 2;

    i1 = (k + 1) % 3;
    i2 = (k + 2) % 3;

    px = pPoint[i1]   - pTri->pA[i1];
    py = pPoint[i2]   - pTri->pA[i2];
    bx = pTri->pB[i1] - pTri->pA[i1];
    by = pTri->pB[i2] - pTri->pA[i2];
    cx = pTri->pC[i1] - pTri->pA[i1];
    cy = pTri->pC[i2] - pTri->pA[i2];

    /* Solve  px = u*cx + v*bx ,  py = u*cy + v*by  for (u,v):
     * u weights C and v weights B.  Note the argument order of the cross
     * terms -- getting them the other way round negates u silently.
     *
     * The bx == 0 test is `fcomp ; test ah,0x40`, i.e. the C3 flag, which
     * x87 also sets for unordered.  `!(bx != 0)` reproduces that: it is true
     * for 0.0, -0.0 and NaN alike. */
    if (!(bx != 0.0f)) {
        u = px / cx;
        if (!(u >= 0.0f)) return 0;
        if (!(u <= 1.0f)) return 0;
        v = (py - u * cy) / by;
    } else {
        u = (py * bx - by * px) / (cy * bx - by * cx);
        if (!(u >= 0.0f)) return 0;
        if (!(u <= 1.0f)) return 0;
        v = (px - u * cx) / bx;
    }

    /* Every test is written negated on purpose: the original branches on the
     * x87 condition codes, where "unordered" takes the failing edge.  Plain
     * `if (u < 0) return 0;` would let a NaN through. */
    if (!(v >= 0.0f)) return 0;
    if (!((u + v) <= 1.0f)) return 0;

    /* DEVIATION: the original keeps the numerator, denominator and v in
     * 80-bit x87 registers (only u is round-tripped through a float32 slot).
     * These are float32 throughout, so results can differ in the last ulp
     * for points sitting exactly on an edge. */
    return 1;
}
