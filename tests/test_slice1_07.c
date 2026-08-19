/* test_slice1_07.c -- behaviour/invariant tests for slice1_07.
 *
 * These assert properties the original demonstrably has (tiling of the grid
 * tables, the vertical flips, the colour-key predicate agreeing with the
 * blit, barycentric symmetry of the triangle test) rather than transcribing
 * expected byte values.
 */

#include "slice1_07.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

/* ------------------------------------------------------------------ */
/* 0x1005F800  grid tables                                             */
/* ------------------------------------------------------------------ */

static void CheckGrid(const BrRectI *p, int32_t count, int32_t cols,
                      int32_t w, int32_t h)
{
    int32_t i;

    for (i = 0; i < count; i++) {
        /* every cell has exactly the nominal size */
        CHECK(p[i].x1 - p[i].x0 == w);
        CHECK(p[i].y1 - p[i].y0 == h);
        /* origin is non-negative and on the grid */
        CHECK(p[i].x0 % w == 0);
        CHECK(p[i].y0 % h == 0);
        CHECK(p[i].x0 >= 0 && p[i].y0 >= 0);
        /* cells tile: right edge of one is the left edge of the next in row */
        if (i + 1 < count && ((i + 1) % cols) != 0) {
            CHECK(p[i].x1 == p[i + 1].x0);
            CHECK(p[i].y0 == p[i + 1].y0);
        }
        /* first cell of a row restarts at x=0 one row lower */
        if ((i % cols) == 0) {
            CHECK(p[i].x0 == 0);
            CHECK(p[i].y0 == (i / cols) * h);
        }
    }
}

static void TestGrids(void)
{
    BrRectI a[BR_GRID_A_COUNT], b[BR_GRID_B_COUNT];
    BrRectI c[BR_GRID_C_COUNT], d[BR_GRID_D_COUNT];

    BrRectTablesInit(a, b, c, d);

    CheckGrid(a, BR_GRID_A_COUNT, BR_GRID_A_COLS, BR_GRID_A_CELLW, BR_GRID_A_CELLH);
    CheckGrid(b, BR_GRID_B_COUNT, BR_GRID_B_COLS, BR_GRID_B_CELLW, BR_GRID_B_CELLH);
    CheckGrid(c, BR_GRID_C_COUNT, BR_GRID_C_COLS, BR_GRID_C_CELLW, BR_GRID_C_CELLH);
    CheckGrid(d, BR_GRID_D_COUNT, BR_GRID_D_COLS, BR_GRID_D_CELLW, BR_GRID_D_CELLH);

    /* Table A's 68 entries stop part-way through row 8 (68 = 8*8 + 4), so
     * the last entry is column 3 of row 8 -- the grid is truncated, not
     * padded out. */
    CHECK(a[67].x0 == 3 * 16);
    CHECK(a[67].y0 == 8 * 16);
    /* Tables B and D exactly fill their last row. */
    CHECK(b[BR_GRID_B_COUNT - 1].x0 == (BR_GRID_B_COLS - 1) * BR_GRID_B_CELLW);
    CHECK(d[BR_GRID_D_COUNT - 1].x0 == (BR_GRID_D_COLS - 1) * BR_GRID_D_CELLW);
}

/* ------------------------------------------------------------------ */
/* 0x1005FF30 / 0x10060280  clears                                     */
/* ------------------------------------------------------------------ */

static void TestClears(void)
{
    uint32_t a[BR_TABLE64_COUNT + 1], b[BR_TABLE64_COUNT + 1];
    uint32_t c[BR_TABLE64_COUNT + 1];
    BrDevSlot slot;
    int i, allZero;

    memset(a, 0xAB, sizeof a);
    memset(b, 0xAB, sizeof b);
    memset(c, 0xAB, sizeof c);
    BrTables64Clear(a, b, c);

    allZero = 1;
    for (i = 0; i < BR_TABLE64_COUNT; i++)
        if (a[i] || b[i] || c[i]) allZero = 0;
    CHECK(allZero);
    /* exactly 64 dwords each -- the guard element past the end survives */
    CHECK(a[BR_TABLE64_COUNT] == 0xABABABABu);
    CHECK(b[BR_TABLE64_COUNT] == 0xABABABABu);
    CHECK(c[BR_TABLE64_COUNT] == 0xABABABABu);

    memset(&slot, 0x5A, sizeof slot);
    BrDevSlotClear(&slot);
    CHECK(slot.f2C == 0 && slot.f30 == 0 && slot.f34 == 0 && slot.f38 == 0);
    CHECK(slot.f3C == 0 && slot.f40 == 0 && slot.f44 == 0 && slot.f48 == 0);
    CHECK(slot.f4C == 0);
    CHECK(slot.pIface == NULL);
    /* +0x00..+0x2B is deliberately left alone */
    for (i = 0; i < 0x2C; i++)
        CHECK(slot.head[i] == 0x5A);
}

/* ------------------------------------------------------------------ */
/* 0x10060030  error box                                               */
/* ------------------------------------------------------------------ */

static uint32_t     g_lookupId;
static void        *g_boxWnd;
static const char  *g_boxText;
static const char  *g_boxCaption;
static uint32_t     g_boxType;
static int          g_boxCalls;

static const char *TestLookup(uint32_t id)
{
    g_lookupId = id;
    return "CAPTION";
}

static void TestBox(void *hWnd, const char *pText, const char *pCaption,
                    uint32_t uType)
{
    g_boxWnd = hWnd;
    g_boxText = pText;
    g_boxCaption = pCaption;
    g_boxType = uType;
    g_boxCalls++;
}

static void TestErrorBox(void)
{
    int wnd = 0;

    /* no hooks installed: must not crash and must not invent a call */
    BrSetStringLookupFn(NULL);
    BrSetMessageBoxFn(NULL);
    BrErrorBox(&wnd, -2005530516, "boom");
    CHECK(g_boxCalls == 0);

    BrSetStringLookupFn(TestLookup);
    BrSetMessageBoxFn(TestBox);
    BrErrorBox(&wnd, -2005530516, "boom");

    CHECK(g_boxCalls == 1);
    CHECK(g_boxWnd == &wnd);
    CHECK(g_lookupId == BR_ERRSTR_MESSAGEBOX);
    /* caption comes from the string table, text from the caller -- the
     * inverse of how the call sites read */
    CHECK(g_boxText != NULL && strcmp(g_boxText, "boom") == 0);
    CHECK(g_boxCaption != NULL && strcmp(g_boxCaption, "CAPTION") == 0);
    CHECK(g_boxType == 0u);

    BrSetStringLookupFn(NULL);
    BrSetMessageBoxFn(NULL);
}

/* ------------------------------------------------------------------ */
/* 0x10060F00 / 0x10060EA0  24bpp -> RGBA                              */
/* ------------------------------------------------------------------ */

#define BMPW 3
#define BMPH 2
#define BMPPITCH (BMPW * 3 + 1)     /* deliberately padded */

static void MakeBgr(uint8_t *src)
{
    int x, y;
    memset(src, 0xEE, BMPPITCH * BMPH);      /* padding poison */
    for (y = 0; y < BMPH; y++)
        for (x = 0; x < BMPW; x++) {
            uint8_t *p = src + y * BMPPITCH + x * 3;
            p[0] = (uint8_t)(0x10 + y * 16 + x);   /* B */
            p[1] = (uint8_t)(0x40 + y * 16 + x);   /* G */
            p[2] = (uint8_t)(0x80 + y * 16 + x);   /* R */
        }
}

static void TestBgrConvert(void)
{
    uint8_t src[BMPPITCH * BMPH];
    uint8_t dst[BMPW * BMPH * 4 + 4];
    BrBitmap bmp;
    void *pOut;
    int x, y;

    MakeBgr(src);
    memset(dst, 0xCD, sizeof dst);
    BrBgr24ToRgbaFlip(dst, src, BMPW, BMPH, BMPPITCH);

    for (y = 0; y < BMPH; y++)
        for (x = 0; x < BMPW; x++) {
            /* destination row y comes from SOURCE row (h-1-y): flipped */
            const uint8_t *s = src + (BMPH - 1 - y) * BMPPITCH + x * 3;
            const uint8_t *d = dst + (y * BMPW + x) * 4;
            CHECK(d[0] == s[2]);       /* R <- src byte 2 */
            CHECK(d[1] == s[1]);       /* G */
            CHECK(d[2] == s[0]);       /* B <- src byte 0 */
            CHECK(d[3] == 0xFF);       /* alpha forced opaque */
        }
    /* writes exactly w*h*4 bytes: the guard dword is untouched, so the
     * padding in the source pitch was skipped rather than copied */
    CHECK(dst[BMPW * BMPH * 4] == 0xCD);

    /* height == 0 is an explicit early-out */
    memset(dst, 0xCD, sizeof dst);
    BrBgr24ToRgbaFlip(dst, src, BMPW, 0, BMPPITCH);
    CHECK(dst[0] == 0xCD);

    /* BrBmp24ToRgba rejects anything that is not 24bpp, and leaves the
     * width/height globals alone when it does */
    BrImgTintState.width = -1;
    BrImgTintState.height = -1;
    memset(&bmp, 0, sizeof bmp);
    bmp.bmWidth = BMPW;
    bmp.bmHeight = BMPH;
    bmp.bmWidthBytes = BMPPITCH;
    bmp.bmPlanes = 1;
    bmp.bmBitsPixel = 16;
    bmp.bmBits = src;
    CHECK(BrBmp24ToRgba(&bmp) == NULL);
    CHECK(BrImgTintState.width == -1 && BrImgTintState.height == -1);

    bmp.bmBitsPixel = 24;
    pOut = BrBmp24ToRgba(&bmp);
    CHECK(pOut != NULL);
    if (pOut) {
        /* same result as calling the converter directly */
        BrBgr24ToRgbaFlip(dst, src, BMPW, BMPH, BMPPITCH);
        CHECK(memcmp(pOut, dst, BMPW * BMPH * 4) == 0);
        free(pOut);
    }
    CHECK(BrImgTintState.width == BMPW);
    CHECK(BrImgTintState.height == BMPH);
}

/* ------------------------------------------------------------------ */
/* 0x10061480 / 0x100615B0  colour-keyed tint                          */
/* ------------------------------------------------------------------ */

#define TW 2
#define TH 2
#define DSTW 4
#define DSTH 4

static void TestTint(void)
{
    uint8_t src[TW * TH * 4];
    uint8_t dst[DSTW * DSTH * 4];
    int x, y;

    /* row 0: one keyed pixel (R==0, G==B) and one plain one
     * row 1: two plain pixels */
    memset(src, 0, sizeof src);
    src[0] = 0;    src[1] = 100; src[2] = 100; src[3] = 0x77;   /* keyed */
    src[4] = 9;    src[5] = 20;  src[6] = 30;  src[7] = 0x11;
    src[8] = 1;    src[9] = 2;   src[10] = 2;  src[11] = 0x22;  /* R!=0 */
    src[12] = 0;   src[13] = 5;  src[14] = 6;  src[15] = 0x33;  /* G!=B */

    CHECK(BrImgHasKeyed(src, 0, TW, 0, TH) == 1);
    /* the predicate and the blit use the same key */
    CHECK(BrImgHasKeyed(src + 8, 0, TW, 0, 1) == 0);

    /* identity tint: a keyed pixel (0,g,g,a) becomes (g,g,g,a) */
    BrImgTintState.scaleR = 255;
    BrImgTintState.scaleG = 255;
    BrImgTintState.scaleB = 255;

    memset(dst, 0xCD, sizeof dst);
    CHECK(BrImgTintBlit(src, 0, TW, 0, TH, dst, DSTW, DSTH) == 1);

    for (y = 0; y < TH; y++)
        for (x = 0; x < TW; x++) {
            const uint8_t *s = src + (y * TW + x) * 4;
            /* source row y lands on destination row DSTH-y0-y-1 */
            const uint8_t *d = dst + ((DSTH - 0 - y - 1) * DSTW + 0 + x) * 4;
            if (s[0] == 0 && s[1] == s[2]) {
                CHECK(d[0] == s[1]);
                CHECK(d[1] == s[1]);
                CHECK(d[2] == s[1]);
            } else {
                CHECK(d[0] == s[0]);
                CHECK(d[1] == s[1]);
                CHECK(d[2] == s[2]);
            }
            CHECK(d[3] == s[3]);       /* alpha always copied through */
        }
    /* rows 0 and 1 of the destination were never touched */
    for (y = 0; y < 2; y++)
        for (x = 0; x < DSTW * 4; x++)
            CHECK(dst[y * DSTW * 4 + x] == 0xCD);

    /* the tinted result is no longer keyed (byte0 becomes g != 0), so a
     * second pass over it is a no-op -- the pass is self-terminating */
    CHECK(BrImgHasKeyed(dst + (DSTH - 1) * DSTW * 4, 0, 1, 0, 1) == 0);

    /* scaling: g * scale / 255, truncating */
    BrImgTintState.scaleR = 0;
    BrImgTintState.scaleG = 255;
    BrImgTintState.scaleB = 128;
    memset(dst, 0xCD, sizeof dst);
    BrImgTintBlit(src, 0, TW, 0, TH, dst, DSTW, DSTH);
    {
        const uint8_t *d = dst + ((DSTH - 1) * DSTW) * 4;
        CHECK(d[0] == 0);
        CHECK(d[1] == 100);
        CHECK(d[2] == (uint8_t)((100 * 128) / 255));
    }

    /* degenerate rects touch nothing and still report success */
    memset(dst, 0xCD, sizeof dst);
    CHECK(BrImgTintBlit(src, 0, TW, 2, 2, dst, DSTW, DSTH) == 1);
    CHECK(BrImgTintBlit(NULL, 0, TW, 0, TH, dst, DSTW, DSTH) == 1);
    CHECK(BrImgHasKeyed(NULL, 0, TW, 0, TH) == 0);
    CHECK(BrImgHasKeyed(src, 0, TW, 2, 2) == 0);
    for (x = 0; x < (int)sizeof dst; x++)
        CHECK(dst[x] == 0xCD);
}

/* ------------------------------------------------------------------ */
/* 0x10060F70 / 0x10060FB0  record accessors                           */
/* ------------------------------------------------------------------ */

static void TestRec10(void)
{
    static BrRec10 table[2 * BR_REC10_COLS];
    int32_t r, j;
    int32_t w5, w6, w7, w8;

    for (r = 0; r < 2 * BR_REC10_COLS; r++)
        for (j = 0; j < BR_REC10_DWORDS; j++)
            table[r].dw[j] = r * 1000 + j;

    /* [a][b] addressing: 30 records per `a` */
    CHECK(BrRec10Get(table, 0, 0, 0) == 0);
    CHECK(BrRec10Get(table, 0, 7, 3) == 7 * 1000 + 3);
    CHECK(BrRec10Get(table, 1, 0, 9) == 30 * 1000 + 9);
    CHECK(BrRec10Get(table, 1, 29, 1) == 59 * 1000 + 1);

    /* the side-effect global always gets dword 4 of the same record */
    (void)BrRec10Get(table, 1, 2, 0);
    CHECK(BrRec10LastDw4 == 32 * 1000 + 4);

    w5 = w6 = w7 = w8 = -1;
    BrRec10Get4(table, 1, 2, &w5, &w6, &w7, &w8);
    CHECK(w5 == 32 * 1000 + 5);
    CHECK(w6 == 32 * 1000 + 6);
    CHECK(w7 == 32 * 1000 + 7);
    CHECK(w8 == 32 * 1000 + 8);

    /* both routines agree on which record they address */
    CHECK(BrRec10Get(table, 1, 2, 5) == w5);
}

/* ------------------------------------------------------------------ */
/* 0x1006ABA0 / 0x1006AE00  24-byte records                            */
/* ------------------------------------------------------------------ */

static void TestRec24(void)
{
    uint32_t n;

    for (n = 0; n < 5000; n += 137) {
        BrG_B502E8 = n;
        BrRec24SetCount(BrRec24TotalBytes());
        CHECK(BrG_B502EC == n);            /* (n*24)/24 round-trips */
    }
    /* truncation, and the divide is UNSIGNED */
    BrRec24SetCount(23);  CHECK(BrG_B502EC == 0);
    BrRec24SetCount(47);  CHECK(BrG_B502EC == 1);
    BrRec24SetCount(0xFFFFFFFFu);
    CHECK(BrG_B502EC == 0xFFFFFFFFu / 24u);
}

/* ------------------------------------------------------------------ */
/* 0x1006C740  point in triangle                                       */
/* ------------------------------------------------------------------ */

static int16_t Inside(const float *n, const float *a, const float *b,
                      const float *c, const float *p)
{
    BrTri t;
    t.n[0] = n[0]; t.n[1] = n[1]; t.n[2] = n[2];
    t.f0C = 0.0f;
    t.pA = a; t.pB = b; t.pC = c;
    return BrTriContainsPoint2D(&t, p);
}

static void TestTri(void)
{
    static const float nz[3] = { 0.0f, 0.0f, 1.0f };
    static const float ny[3] = { 0.0f, 1.0f, 0.0f };
    static const float ones[3] = { 1.0f, 1.0f, 1.0f };
    static const float A[3] = { 0.0f, 0.0f, 0.0f };
    static const float B[3] = { 1.0f, 0.0f, 0.0f };
    static const float C[3] = { 0.0f, 1.0f, 0.0f };

    float p[3];
    int i;

    /* the three vertices are inside (u,v hit 0 and 1 exactly) */
    CHECK(Inside(nz, A, B, C, A) == 1);
    CHECK(Inside(nz, A, B, C, B) == 1);
    CHECK(Inside(nz, A, B, C, C) == 1);

    /* centroid and an edge midpoint are inside; the test is edge-inclusive */
    p[0] = 1.0f / 3.0f; p[1] = 1.0f / 3.0f; p[2] = 0.0f;
    CHECK(Inside(nz, A, B, C, p) == 1);
    p[0] = 0.5f; p[1] = 0.5f; p[2] = 0.0f;      /* on the hypotenuse */
    CHECK(Inside(nz, A, B, C, p) == 1);
    p[0] = 0.5f; p[1] = 0.0f; p[2] = 0.0f;      /* on edge AB */
    CHECK(Inside(nz, A, B, C, p) == 1);

    /* outside on each side */
    p[0] = 0.9f; p[1] = 0.9f; p[2] = 0.0f; CHECK(Inside(nz, A, B, C, p) == 0);
    p[0] = -0.1f; p[1] = 0.5f; p[2] = 0.0f; CHECK(Inside(nz, A, B, C, p) == 0);
    p[0] = 0.5f; p[1] = -0.1f; p[2] = 0.0f; CHECK(Inside(nz, A, B, C, p) == 0);

    /* Swapping B and C swaps the roles of u and v; the accepted region is
     * symmetric in them, so containment must be unchanged.  This also
     * exercises the OTHER branch: with (A,C,B) the second vertex has
     * B[i1] == A[i1], which takes the bx == 0 special case. */
    for (i = 0; i < 40; i++) {
        p[0] = -0.5f + (float)i * 0.05f;
        p[1] = 0.7f - (float)i * 0.03f;
        p[2] = 0.0f;
        CHECK(Inside(nz, A, B, C, p) == Inside(nz, A, C, B, p));
    }

    /* The dominant axis of the normal is PROJECTED OUT: the component along
     * it is never looked at, so this is a 2D test, not 3D containment.
     * A point far off the plane still reports inside. */
    p[0] = 0.25f; p[1] = 0.25f; p[2] = 999.0f;
    CHECK(Inside(nz, A, B, C, p) == 1);

    /* A triangle whose normal is dominant in Y: axis 1 is dropped, so the
     * test runs in (z,x).  Vertices and the centroid must still be inside. */
    {
        static const float A2[3] = { 0.0f, 5.0f, 0.0f };
        static const float B2[3] = { 1.0f, 5.0f, 0.0f };
        static const float C2[3] = { 0.0f, 5.0f, 1.0f };
        CHECK(Inside(ny, A2, B2, C2, A2) == 1);
        CHECK(Inside(ny, A2, B2, C2, B2) == 1);
        CHECK(Inside(ny, A2, B2, C2, C2) == 1);
        p[0] = 0.25f; p[1] = 5.0f; p[2] = 0.25f;
        CHECK(Inside(ny, A2, B2, C2, p) == 1);
        p[0] = 0.9f; p[1] = 5.0f; p[2] = 0.9f;
        CHECK(Inside(ny, A2, B2, C2, p) == 0);
    }

    /* |nx| == |ny| == |nz| picks axis 2 (ties go to the later axis), which
     * for these vertices is the same projection as nz above. */
    p[0] = 0.25f; p[1] = 0.25f; p[2] = 0.0f;
    CHECK(Inside(ones, A, B, C, p) == Inside(nz, A, B, C, p));

    /* A NaN coordinate must fail closed: every comparison in the original
     * branches on the x87 flags, where unordered takes the failing edge. */
    p[0] = 0.0f / 0.0f; p[1] = 0.25f; p[2] = 0.0f;
    CHECK(Inside(nz, A, B, C, p) == 0);

    /* A degenerate (zero-area) triangle divides by zero; the result must
     * still be a clean 0/1, never a trap or a stray value. */
    {
        static const float D2[3] = { 0.0f, 0.0f, 0.0f };
        int16_t r;
        p[0] = 0.5f; p[1] = 0.5f; p[2] = 0.0f;
        r = Inside(nz, A, D2, D2, p);
        CHECK(r == 0 || r == 1);
    }
}

int main(void)
{
    TestGrids();
    TestClears();
    TestErrorBox();
    TestBgrConvert();
    TestTint();
    TestRec10();
    TestRec24();
    TestTri();

    if (g_fails == 0) {
        printf("test_slice1_07: all checks passed\n");
        return 0;
    }
    printf("test_slice1_07: %d FAILURES\n", g_fails);
    return 1;
}
