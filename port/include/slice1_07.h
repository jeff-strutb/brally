/* slice1_07.h -- Boss Rally (BRD3D.dll) slice 1, agent 07.
 *
 * Range 0x1005F580 - 0x1006C740.  Three clusters live in here:
 *
 *   1. UV / rectangle grid table builders          (0x1005F800)
 *   2. A 32-bit image pipeline: Win32 24bpp BITMAP -> RGBA, a colour-keyed
 *      tint blit, and the predicate that decides whether an image needs it
 *      (0x10060EA0, 0x10060F00, 0x10061480, 0x100615B0)
 *   3. Table accessors and a point-in-triangle test
 *      (0x10060F70, 0x10060FB0, 0x1006C740)
 *
 * Everything here was recovered from the disassembly in
 * work/slice1/agent07.asm.  Field names that could not be justified are
 * positional (fNN = byte offset NN).
 *
 * NOTE 0x10069490 is NOT in this packet; it is already implemented as
 * BrPoolAlloc in br_pool.h and was not touched.
 */
#ifndef SLICE1_07_H
#define SLICE1_07_H

#include <stdint.h>
#include <stddef.h>

/* =====================================================================
 * 0x1005F800 -- rectangle grid tables
 *
 * The original builds four independent tables of 4-int records, each one a
 * regular grid of cells laid out left-to-right then top-to-bottom:
 *
 *     rec[i] = { col*cellW, row*cellH, col*cellW + cellW, row*cellH + cellH }
 *     col = i % cols, row = i / cols
 *
 * i.e. {left, top, right, bottom} -- an exclusive right/bottom edge, so
 * adjacent cells share an edge value and the rects are half-open.
 *
 * The four tables in the original (address, count, cols, cell size):
 *     0x10A9D180   68 entries   8 cols   16 x 16
 *     0x10A9D638   20 entries   5 cols   39 x 44
 *     0x10A9DA50   15 entries   5 cols  128 x128
 *     0x10A9DB40    9 entries   3 cols  128 x128
 * They are NOT contiguous with one another (there are gaps between them),
 * which is why this takes four separate pointers rather than one struct.
 *
 * Table A's 68 entries do not fill its last row (68 = 8*8 + 4); the grid is
 * simply truncated mid-row.  Preserved.
 * ===================================================================== */

typedef struct BrRectI { int32_t x0, y0, x1, y1; } BrRectI;

#define BR_GRID_A_COUNT 68
#define BR_GRID_A_COLS   8
#define BR_GRID_A_CELLW 16
#define BR_GRID_A_CELLH 16

#define BR_GRID_B_COUNT 20
#define BR_GRID_B_COLS   5
#define BR_GRID_B_CELLW 39
#define BR_GRID_B_CELLH 44

#define BR_GRID_C_COUNT 15
#define BR_GRID_C_COLS   5
#define BR_GRID_C_CELLW 128
#define BR_GRID_C_CELLH 128

#define BR_GRID_D_COUNT  9
#define BR_GRID_D_COLS   3
#define BR_GRID_D_CELLW 128
#define BR_GRID_D_CELLH 128

/* Fill `count` records of a `cols`-wide grid of cellW x cellH cells. */
void BrRectGridFill(BrRectI *pDst, int32_t count, int32_t cols,
                    int32_t cellW, int32_t cellH);

/* 0x1005F800  build all four tables, in the original's order. */
void BrRectTablesInit(BrRectI *pA, BrRectI *pB, BrRectI *pC, BrRectI *pD);

/* =====================================================================
 * 0x1005FF30 -- clear three 64-dword tables
 *
 * `rep stosd` with ecx = 0x40 over 0x10AA3288, 0x10AA2A80, 0x10AA2E88 (in
 * that order).  The three are not adjacent, so they are separate arrays.
 * ===================================================================== */
#define BR_TABLE64_COUNT 64
void BrTables64Clear(uint32_t *pA, uint32_t *pB, uint32_t *pC);

/* =====================================================================
 * 0x10060280 -- clear an object's tail fields
 *
 * thiscall.  Zeroes +0x50 FIRST and then +0x2C..+0x4C.  +0x00..+0x28 are
 * deliberately left alone.  +0x50 is a COM interface pointer: 0x100602E0
 * does `lea esi,[this+0x50]` and then calls through `**esi`.
 * ===================================================================== */
typedef struct BrDevSlot {
    uint8_t  head[0x2C];   /* +0x00..+0x2B -- untouched by the clear */
    int32_t  f2C, f30, f34, f38, f3C, f40, f44, f48, f4C;
    void    *pIface;       /* +0x50 */
} BrDevSlot;

void BrDevSlotClear(BrDevSlot *pSlot);

/* =====================================================================
 * 0x10060030 -- error message box
 *
 * cdecl(a1, a2, a3) -> MessageBoxA(hWnd = a1, lpText = a3,
 *                                  lpCaption = strings[0xAA], uType = 0).
 *
 * GOTCHA: a2 IS NEVER READ.  Every call site (0x10060312, 0x10060348,
 * 0x1006037C) passes the failing HRESULT there and it is silently dropped,
 * so the box never shows the error code.  Reproduced exactly.
 *
 * GOTCHA: the caption comes from the string table, the TEXT from the caller
 * -- the two are the opposite way round from what the call sites read like.
 *
 * The string lookup and the message box itself are Win32/global-table
 * dependent, so they are hooks here.  Default hooks do nothing.
 * ===================================================================== */
#define BR_ERRSTR_MESSAGEBOX 0xAAu   /* id used by 0x10060030 for the caption */

typedef const char *(*BrStringLookupFn)(uint32_t id);
typedef void (*BrMessageBoxFn)(void *hWnd, const char *pText,
                               const char *pCaption, uint32_t uType);

void BrSetStringLookupFn(BrStringLookupFn fn);
void BrSetMessageBoxFn(BrMessageBoxFn fn);

/* 0x10060030 */
void BrErrorBox(void *hWnd, int32_t unusedCode, const char *pText);

/* =====================================================================
 * The image pipeline
 *
 * Pixels are 4 bytes: [0]=R [1]=G [2]=B [3]=A, produced by
 * BrBgr24ToRgbaFlip below (which reads Windows BGR order and writes RGB).
 *
 * The "colour key" used by the tint pass is:  byte0 == 0 && byte1 == byte2
 * i.e. R == 0 and G == B.  Such a pixel carries a single grey level g = G,
 * and is replaced by (g*scaleR/255, g*scaleG/255, g*scaleB/255) with the
 * alpha byte copied through untouched.
 * ===================================================================== */

/* Globals at 0x10AA3440..0x10AA3468.  The gaps are real -- the three
 * multipliers live at 0x3440, 0x3448 and 0x345C, not at a uniform stride --
 * so the unused slots are kept as positional fields to preserve offsets.
 * width/height at 0x3464/0x3468 are written by BrBmp24ToRgba. */
typedef struct BrImgState {
    int32_t scaleR;   /* +0x00  0x10AA3440 -- multiplies dst byte 0 */
    int32_t f04;      /* +0x04  0x10AA3444 */
    int32_t scaleG;   /* +0x08  0x10AA3448 -- multiplies dst byte 1 */
    int32_t f0C;      /* +0x0C  0x10AA344C */
    int32_t f10;      /* +0x10  0x10AA3450 */
    int32_t f14;      /* +0x14  0x10AA3454 */
    int32_t f18;      /* +0x18  0x10AA3458 */
    int32_t scaleB;   /* +0x1C  0x10AA345C -- multiplies dst byte 2 */
    int32_t f20;      /* +0x20  0x10AA3460 */
    int32_t width;    /* +0x24  0x10AA3464 */
    int32_t height;   /* +0x28  0x10AA3468 */
} BrImgState;

extern BrImgState BrImgTintState;

/* Win32 BITMAP, as 0x10060EA0 and 0x100611A0 read it.
 * DEVIATION: bmBits sits at +0x14 in the 32-bit original; on a 64-bit host
 * this struct pads it to +0x18.  Nothing here overlays it on foreign
 * memory, so only the caller-filled path is used. */
typedef struct BrBitmap {
    int32_t  bmType;        /* +0x00 */
    int32_t  bmWidth;       /* +0x04 */
    int32_t  bmHeight;      /* +0x08 */
    int32_t  bmWidthBytes;  /* +0x0C */
    uint16_t bmPlanes;      /* +0x10 */
    uint16_t bmBitsPixel;   /* +0x12 */
    void    *bmBits;        /* +0x14 (32-bit original) */
} BrBitmap;

/* 0x10060F00  packed 24bpp BGR, bottom-up, srcPitch-strided
 *          -> tightly packed 32bpp RGBA, top-down.
 * Source scanning starts at the LAST row and walks backwards by srcPitch;
 * the destination cursor runs forwards without interruption, so the image
 * is vertically flipped.  Alpha is written as 0xFF for every pixel. */
void BrBgr24ToRgbaFlip(uint8_t *pDst, const uint8_t *pSrc,
                       int32_t width, int32_t height, int32_t srcPitch);

/* 0x10060EA0  convert a 24bpp BITMAP to a freshly malloc'd RGBA buffer.
 * Returns NULL if bmBitsPixel != 24 or if the allocation fails; only on
 * success are BrImgTintState.width/height updated.  Caller owns the
 * returned block (free()). */
void *BrBmp24ToRgba(const BrBitmap *pBmp);

/* 0x10061480  copy a w x h RGBA rect into a bottom-up destination surface,
 * tinting colour-keyed pixels on the way through.
 *
 *   w = x1 - x0, h = y1 - y0
 *   source is TIGHTLY PACKED at w pixels per row (stride w*4), NOT x1*4
 *   destination row for source row y is (dstHeight - y0 - y - 1),
 *   destination column origin is x0
 *
 * So the destination is bottom-up (a DIB) while the source is top-down.
 * Always returns 1, including for a NULL source or a non-positive height --
 * the return value carries no failure information. */
int32_t BrImgTintBlit(const uint8_t *pSrc, int32_t x0, int32_t x1,
                      int32_t y0, int32_t y1, uint8_t *pDst,
                      int32_t dstPitchPixels, int32_t dstHeight);

/* 0x100615B0  1 if any pixel in the rect matches the colour key, else 0.
 * Same tightly-packed (x1-x0)-wide source addressing as BrImgTintBlit, so
 * the two agree about which pixels the blit will alter. */
int32_t BrImgHasKeyed(const uint8_t *pPixels, int32_t x0, int32_t x1,
                      int32_t y0, int32_t y1);

/* =====================================================================
 * 0x10060F70 / 0x10060FB0 -- accessors over a table of 40-byte records
 *
 * Both index the same array at 0x100ADFD0 as table[a][b], with 30 records
 * per `a` row (stride 1200 bytes) and a 40-byte (10 dword) record.
 * The meaning of the dwords is not established, so they are numbered.
 * ===================================================================== */
#define BR_REC10_DWORDS 10
#define BR_REC10_COLS   30

typedef struct BrRec10 { int32_t dw[BR_REC10_DWORDS]; } BrRec10;

/* 0x100B2AD0 -- side-effect output of BrRec10Get (kept as a global so the
 * function keeps the original's signature). */
extern int32_t BrRec10LastDw4;

/* 0x10060F70  sets BrRec10LastDw4 = table[a][b].dw[4] and returns
 * table[a][b].dw[index].
 * GOTCHA: `index` is NOT bounds-checked in the original and may run past
 * dw[9] into the following record.  Preserved. */
int32_t BrRec10Get(const BrRec10 *pTable, int32_t a, int32_t b, int32_t index);

/* 0x10060FB0  reads dwords 5,6,7,8 of table[a][b] into four out params.
 * Note the out params come after the indices, in that order. */
void BrRec10Get4(const BrRec10 *pTable, int32_t a, int32_t b,
                 int32_t *pDw5, int32_t *pDw6, int32_t *pDw7, int32_t *pDw8);

/* =====================================================================
 * 0x1006ABA0 / 0x1006AE00 -- 24-byte record count/size conversions
 *
 * 0x1006ABA0 returns g_B502E8 * 24 (lea+shl).
 * 0x1006AE00 stores bytes/24 into g_B502EC.  The divide is UNSIGNED
 * (`mul 0xAAAAAAAB` then `shr 4`), so a "negative" byte count is treated as
 * a huge positive one.
 * The two touch different globals (0x10B502E8 vs 0x10B502EC), so they are
 * not a getter/setter pair on one variable despite the matching factor.
 * ===================================================================== */
extern uint32_t BrG_B502E8;
extern uint32_t BrG_B502EC;

uint32_t BrRec24TotalBytes(void);            /* 0x1006ABA0 */
void     BrRec24SetCount(uint32_t bytes);    /* 0x1006AE00 */

/* =====================================================================
 * 0x1006C740 -- point in triangle
 *
 * The triangle is described by a plane normal plus three POINTERS to its
 * vertices (the vertices themselves live elsewhere).  The normal is used
 * only to pick the axis to project out: the axis with the largest |n|
 * component is dropped and the test runs in the remaining two.
 *
 * Barycentric convention (matters if you read the intermediates):
 *   u weights C, v weights B, and the inside test is
 *   u >= 0 && u <= 1 && v >= 0 && u + v <= 1.
 *
 * Returns 1 inside (edges inclusive), 0 outside.
 * GOTCHA: the original returns in AX only (`mov ax,bx`), leaving the high
 * 16 bits of EAX holding leftovers -- hence the 16-bit return type here.
 * Do not widen it to int and assume the top half was ever set.
 * ===================================================================== */
typedef struct BrTri {
    float        n[3];   /* +0x00 plane normal (magnitude irrelevant) */
    float        f0C;    /* +0x0C unused by this routine */
    const float *pA;     /* +0x10 */
    const float *pB;     /* +0x14 */
    const float *pC;     /* +0x18 */
} BrTri;

/* RENAMED during integration: was BrTriContainsPoint, which collided with a
 * DIFFERENT function of that name in slice1_06.h (0x1003B940, barycentric,
 * takes pt + 3 vertices). This one is 0x1006C740 and is a 2D test -- the
 * dominant axis is projected out and never re-examined -- hence the 2D suffix.
 * Two real functions, one name; renamed rather than merged. */
int16_t BrTriContainsPoint2D(const BrTri *pTri, const float *pPoint);

#endif /* SLICE1_07_H */
