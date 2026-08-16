/* test_br_dl.c -- the display-list machine.
 *
 * Asserts properties of the original, not volume:
 *
 *   - the transformed-vertex record has the layout the Glide triangle
 *     handlers hand to grDrawTriangle (this one IS a hard number, because it
 *     is an ABI and getting it wrong is silent);
 *   - the dispatch table covers exactly the opcodes the original's table
 *     covers, and nothing else;
 *   - each handler advances the cursor by the amount the original does,
 *     including the three that are NOT eight bytes;
 *   - the combiner classifier is total and injective on the ten patterns
 *     0x1001E7A0 recognises;
 *   - real retail geometry walks cleanly: every triangle index lands inside
 *     the vertex array, every list ends at G_ENDDL, and the clip outcodes
 *     satisfy their defining inequalities.
 */
#include "br_dl.h"
#include "br_testdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------------ */
/* 1. layout                                                          */
/* ------------------------------------------------------------------ */
static void test_layout(void)
{
    printf("layout\n");
    /* 0x105CE318 stride 0x68; the first 0x3C is a two-TMU Glide GrVertex. */
    check(sizeof(BrDlVtx) == 104, "BrDlVtx is 0x68 bytes");
    check(offsetof(BrDlVtx, x)       == 0x00, "x   at 0x00");
    check(offsetof(BrDlVtx, r)       == 0x0C, "rgb at 0x0C");
    check(offsetof(BrDlVtx, oow)     == 0x20, "oow at 0x20");
    check(offsetof(BrDlVtx, tmu0)    == 0x24, "tmu0 at 0x24");
    check(offsetof(BrDlVtx, tmu1)    == 0x30, "tmu1 at 0x30");
    check(offsetof(BrDlVtx, outcode) == 0x3C, "outcode at 0x3C");
    check(offsetof(BrDlVtx, cx)      == 0x44, "clip xyz at 0x44");
    check(offsetof(BrDlVtx, s)       == 0x50, "s,t at 0x50");
    check(offsetof(BrDlVtx, cw)      == 0x58, "clip w at 0x58");
    check(offsetof(BrDlVtx, n0)      == 0x5C, "normal bytes at 0x5C");
}

/* ------------------------------------------------------------------ */
/* 2. the table                                                       */
/* ------------------------------------------------------------------ */
static void test_table(void)
{
    /* Read out of orig/BRGlide.dll at 0x100A9A58 and orig/BRD3D.dll at
     * 0x100A79F0. The two builds' sets are IDENTICAL -- that is the finding,
     * so both are covered by this one list. */
    static const unsigned aOps[] = {
        0x01, 0x03, 0x04, 0x06, 0xB1, 0xB6, 0xB7, 0xB8, 0xB9, 0xBC,
        0xBD, 0xBF, 0xDC, 0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xE4,
        0xED, 0xF2, 0xF6, 0xF7, 0xF8, 0xFA, 0xFB, 0xFC
    };
    unsigned i, op, n = 0;
    int fAll = 1, fNoExtra = 1;

    printf("dispatch table\n");
    for (i = 0; i < sizeof(aOps) / sizeof(aOps[0]); ++i)
        if (!BrDlIsHandled(aOps[i])) { fAll = 0; printf("    missing %02X\n", aOps[i]); }
    for (op = 0; op < 256; ++op)
        if (BrDlIsHandled(op)) {
            n++;
            for (i = 0; i < sizeof(aOps) / sizeof(aOps[0]); ++i)
                if (aOps[i] == op) break;
            if (i == sizeof(aOps) / sizeof(aOps[0]))
            { fNoExtra = 0; printf("    extra %02X\n", op); }
        }
    check(fAll, "every opcode the original handles is handled");
    check(fNoExtra && n == 28, "and no others: exactly 28");

    /* The ones a stock F3D consumer WOULD handle and this one does not.
     * They are skipped because texture setup happens at load time, not draw
     * time -- see br_dl.h. If a future pass "adds" them the fidelity claim
     * changes, so pin it. */
    check(!BrDlIsHandled(0xF5) && !BrDlIsHandled(0xF3) &&
          !BrDlIsHandled(0xF0) && !BrDlIsHandled(0xFD) &&
          !BrDlIsHandled(0xBB) && !BrDlIsHandled(0xBA),
          "SETTILE/LOADBLOCK/LOADTLUT/SETTIMG/TEXTURE/OTHERMODE_H are skipped");
}

/* ------------------------------------------------------------------ */
/* 3. cursor advance                                                  */
/* ------------------------------------------------------------------ */
static void put(uint8_t *p, uint32_t w0, uint32_t w1)
{
    p[0] = (uint8_t)w0; p[1] = (uint8_t)(w0 >> 8);
    p[2] = (uint8_t)(w0 >> 16); p[3] = (uint8_t)(w0 >> 24);
    p[4] = (uint8_t)w1; p[5] = (uint8_t)(w1 >> 8);
    p[6] = (uint8_t)(w1 >> 16); p[7] = (uint8_t)(w1 >> 24);
}

static uint32_t g_rects;
static void on_rect(void *u, int fTex, int tile, int32_t a, int32_t b,
                    int32_t c, int32_t d)
{ (void)u; (void)fTex; (void)tile; (void)a; (void)b; (void)c; (void)d; g_rects++; }

static void test_advance(void)
{
    static uint8_t dl[256];
    BrDl st;
    size_t n;

    printf("cursor advance\n");

    /* 0xE4 is 24 bytes: three double-words, only the first of which is read.
     * Put two poisoned commands after it -- if the handler advanced by 8 the
     * walk would execute them and the count would be wrong. */
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0xE4000000u, 0x00000000u);
    put(dl + 0x08, 0xFFFFFFFFu, 0xFFFFFFFFu);   /* would be skipped as junk */
    put(dl + 0x10, 0xFFFFFFFFu, 0xFFFFFFFFu);
    put(dl + 0x18, 0xB8000000u, 0x00000000u);
    BrDlInit(&st, 320, 240);
    st.sink.pfnRect = on_rect;
    g_rects = 0;
    n = BrDlRun(&st, dl, sizeof(dl));
    check(n == 2 && g_rects == 1 && st.cUnhandled == 0,
          "0xE4 consumes 0x18 bytes");

    /* 0xDC advances by 8 * w1 -- br_font.c emits w1 == 1, but the handler
     * exists so one 0xDC can replace a run. */
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0xDC000001u, 0x00000003u);   /* skip three double-words */
    put(dl + 0x08, 0xFFFFFFFFu, 0xFFFFFFFFu);
    put(dl + 0x10, 0xFFFFFFFFu, 0xFFFFFFFFu);
    put(dl + 0x18, 0xB8000000u, 0x00000000u);
    BrDlInit(&st, 320, 240);
    n = BrDlRun(&st, dl, sizeof(dl));
    check(n == 2 && st.cUnhandled == 0 && st.hTexture == 1u,
          "0xDC consumes 8*w1 bytes and binds the low 24 bits");

    /* G_ENDDL with an empty stack ends the walk; with a return address it
     * resumes.  0x06 with a non-zero byte 2 is a JUMP, not a call. */
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0x06000000u, 0x40000010u);   /* call -> +0x10 */
    put(dl + 0x08, 0xB8000000u, 0x00000000u);   /* the return lands past here*/
    put(dl + 0x10, 0xB8000000u, 0x00000000u);   /* returns to +0x08 */
    BrDlInit(&st, 320, 240);
    BrDlAddRegion(&st, 0x40000000u, dl, sizeof(dl));
    n = BrDlRun(&st, dl, sizeof(dl));
    check(n == 3 && st.cDlCalls == 1 && st.sp == 0,
          "G_DL calls, G_ENDDL returns, stack balances");

    /* The 228 unhandled slots all advance eight bytes and do nothing. */
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0xF5000000u, 0u);
    put(dl + 0x08, 0xFD000000u, 0u);
    put(dl + 0x10, 0xBA000000u, 0u);
    put(dl + 0x18, 0xB8000000u, 0u);
    BrDlInit(&st, 320, 240);
    n = BrDlRun(&st, dl, sizeof(dl));
    check(n == 4 && st.cUnhandled == 3, "unhandled opcodes skip 8 bytes each");
}

/* ------------------------------------------------------------------ */
/* 4. the combiner state model                                        */
/* ------------------------------------------------------------------ */
static void test_combine(void)
{
    static const uint32_t aPairs[][2] = {
        { 0xFCFFFFFFu, 0xFFFCF87Cu }, { 0xFCFFFFFFu, 0xFFFE793Cu },
        { 0xFC567EACu, 0xFFFFF3F9u }, { 0xFCFF97FFu, 0xFF2DFEFFu },
        { 0xFCFFFFFFu, 0xFFFDF2F9u }, { 0xFCFFFFFFu, 0xFFFF73B9u },
        { 0xFC127E08u, 0xF3FFF2F8u }, { 0xFC317E02u, 0x5FFEF3FAu },
        { 0xFC317E02u, 0x51FEF3FAu }, { 0xFC127FFFu, 0xFFFFF838u }
    };
    int seen[BR_DL_CC__COUNT];
    size_t i;
    int fDistinct = 1, fNoDefault = 1;

    printf("combiner\n");
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < sizeof(aPairs) / sizeof(aPairs[0]); ++i) {
        BrDlCombine id = BrDlClassifyCombine(aPairs[i][0], aPairs[i][1]);
        if (id == BR_DL_CC_DEFAULT) fNoDefault = 0;
        seen[id]++;
    }
    /* Nine rows, ten patterns: the DECAL row accepts two w1 values, which is
     * why it is the only id that may appear twice. */
    for (i = 1; i < BR_DL_CC__COUNT; ++i)
        if (seen[i] != ((BrDlCombine)i == BR_DL_CC_DECAL ? 2 : 1))
            fDistinct = 0;
    check(fNoDefault, "all ten recognised pairs classify");
    check(fDistinct, "and each to its own configuration");
    check(BrDlClassifyCombine(0xFC000000u, 0x00000000u) == BR_DL_CC_DEFAULT,
          "an unrecognised pair falls to the default");

    /* The one combine br_font.c emits must not land in the recognised set by
     * accident: BrRdpSetCombineLERP's output is a text combine, and the
     * text path shares this interpreter. */
    check(BrDlClassifyCombine(0xFC30B260u, 0x6041FEFFu) == BR_DL_CC_DEFAULT ||
          BrDlClassifyCombine(0xFC30B260u, 0x6041FEFFu) != BR_DL_CC_DECAL,
          "the text combine does not collide with the decal row");
}

/* ------------------------------------------------------------------ */
/* 5. synthetic geometry through the whole machine                    */
/* ------------------------------------------------------------------ */
static void wf(uint8_t *p, float f)
{ uint32_t v; memcpy(&v, &f, 4); p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
  p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }

static void test_synthetic(void)
{
    static uint8_t dl[128];
    static uint8_t verts[3 * 0x20];
    static uint8_t mtx[64];
    static uint8_t rgba[64 * 64 * 4];
    BrDl st;
    BrDlRaster ras;
    int i;

    printf("synthetic triangle\n");

    /* an identity modelview; the projection stays identity too, so clip
     * space is model space and w == 1. */
    memset(mtx, 0, sizeof(mtx));
    wf(mtx + 0 * 4, 1.0f); wf(mtx + 5 * 4, 1.0f);
    wf(mtx + 10 * 4, 1.0f); wf(mtx + 15 * 4, 1.0f);

    /* three vertices in the expanded 8-float form BrVtxExpand produces */
    memset(verts, 0, sizeof(verts));
    wf(verts + 0x00 + 0x00, -0.5f); wf(verts + 0x00 + 0x04, -0.5f);
    wf(verts + 0x20 + 0x00,  0.5f); wf(verts + 0x20 + 0x04, -0.5f);
    wf(verts + 0x40 + 0x00,  0.0f); wf(verts + 0x40 + 0x04,  0.5f);
    for (i = 0; i < 3; ++i) {
        wf(verts + i * 0x20 + 0x14, 1.0f);   /* r */
        wf(verts + i * 0x20 + 0x18, 1.0f);   /* g */
        wf(verts + i * 0x20 + 0x1C, 1.0f);   /* b */
    }

    memset(dl, 0, sizeof(dl));
    /* G_MTX: load, modelview, no push */
    put(dl + 0x00, 0x01020000u | 0x20000u, 0x50000000u);
    /* G_VTX: n = 3 at bits[15:10], v0 = 0 at bits[23:16] */
    put(dl + 0x08, 0x04000000u | (3u << 10), 0x60000000u);
    /* G_TRI1: indices already halved by the patch pass, so 0,1,2 */
    put(dl + 0x10, 0xBF000000u, 0x00000102u);
    put(dl + 0x18, 0xB8000000u, 0u);

    BrDlInit(&st, 64, 64);
    BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));

    memset(rgba, 0, sizeof(rgba));
    ras.pRgba = rgba; ras.cx = 64; ras.cy = 64; ras.cCovered = 0;
    BrDlAttachRaster(&st, &ras);

    BrDlRun(&st, dl, sizeof(dl));

    check(st.cVtxLoads == 1 && st.cVtxTransformed == 3,
          "G_VTX transformed three vertices");
    check(st.cTriIn == 1 && st.cTriDrawn == 1 && st.cTriRejected == 0 &&
          st.cTriClipped == 0, "G_TRI1 produced one unclipped triangle");
    /* w == 1 for every vertex, so 1/w must be exactly 1. */
    check(st.aVtx[0].oow == 1.0f && st.aVtx[2].oow == 1.0f,
          "oow is 1/w");
    /* Screen coordinates are quarter-pixel snapped -- an exact property of
     * the fmul 4 / fistp / fild / fmul 0.25 sequence at 0x10021BF9. */
    check(fabsf(st.aVtx[0].x * 4.0f - floorf(st.aVtx[0].x * 4.0f + 0.5f))
              < 1e-4f,
          "screen x lands on a quarter-pixel");
    check(ras.cCovered > 100, "the reference rasteriser filled the triangle");

    /* Push the same triangle outside the frustum and it must be REJECTED,
     * not clipped: all three share the same outcode bit. */
    wf(verts + 0x00 + 0x00, -5.0f);
    wf(verts + 0x20 + 0x00, -6.0f);
    wf(verts + 0x40 + 0x00, -7.0f);
    BrDlInit(&st, 64, 64);
    BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
    BrDlRun(&st, dl, sizeof(dl));
    check(st.cTriRejected == 1 && st.cTriDrawn == 0,
          "a wholly off-screen triangle is trivially rejected");
}

/* ------------------------------------------------------------------ */
/* 5b. the clipper                                                    */
/* ------------------------------------------------------------------ */
/* Asserts the two things a clipper can be right or wrong about, and no
 * volume: (a) every vertex it produces is inside every half-space, and
 * (b) the AREA it produces is the analytic area of the clipped polygon.
 * Area is order-independent, so it survives a different-but-equivalent
 * vertex sequence -- which matters, because Sutherland-Hodgman's output
 * order is a property of the plane order and this port keeps 0x1001EE70's.
 *
 * There is also a conservation law worth having: the 64-node pool at
 * 0x105CCFF0 is a free list, every plane routine and the output loop return
 * to it, and if any path leaks the list shortens.  Counting it is the only
 * cheap way to see that. */

/* slice1_03.c owns the pool and its free list -- br_dl.c only supplies the
 * storage.  BrClipPoolCount is that module's own not-in-the-original
 * accessor; declared by prototype rather than by including slice1_03.h, the
 * way br_dl.c declares BrMat4Mul. */
extern int BrClipPoolCount(void);
static int free_count(const BrDl *pDl) { (void)pDl; return BrClipPoolCount(); }

/* A validating sink: checks the defining half-space inequalities on every
 * vertex handed to pfnTri, accumulates signed clip-space area, then forwards
 * to whatever sink was installed underneath. */
typedef struct Watch {
    void (*pfnInner)(void *, const BrDlVtx *, const BrDlVtx *, const BrDlVtx *);
    void   *pInner;
    double  area;          /* sum of |clip-space triangle area|             */
    int     fInside;       /* every vertex satisfied every plane            */
    uint32_t cTri;
} Watch;

static int vtx_inside(const BrDlVtx *v)
{
    const float e = 1e-4f;
    return v->cw           >= -e && v->cz + v->cw >= -e &&
           v->cw - v->cz   >= -e && v->cw + v->cx >= -e &&
           v->cw - v->cx   >= -e && v->cy + v->cw >= -e &&
           v->cw - v->cy   >= -e;
}

static void watch_tri(void *u, const BrDlVtx *a, const BrDlVtx *b,
                      const BrDlVtx *c)
{
    Watch *w = (Watch *)u;
    double ar;
    if (!vtx_inside(a) || !vtx_inside(b) || !vtx_inside(c))
        w->fInside = 0;
    ar = ((double)b->cx - a->cx) * ((double)c->cy - a->cy) -
         ((double)b->cy - a->cy) * ((double)c->cx - a->cx);
    w->area += (ar < 0 ? -ar : ar) * 0.5;
    w->cTri++;
    if (w->pfnInner)
        w->pfnInner(w->pInner, a, b, c);
}

static void watch_install(BrDl *pDl, Watch *w)
{
    w->pfnInner = pDl->sink.pfnTri;
    w->pInner   = pDl->sink.pUser;
    w->area = 0.0; w->fInside = 1; w->cTri = 0;
    pDl->sink.pfnTri = watch_tri;
    pDl->sink.pUser  = w;
}

static void test_clip(void)
{
    static uint8_t dl[128];
    static uint8_t verts[3 * 0x20];
    static uint8_t mtx[64];
    BrDl st;
    Watch w;
    int i;

    printf("clipper (0x1001EE70 and its seven planes)\n");

    memset(mtx, 0, sizeof(mtx));
    wf(mtx + 0 * 4, 1.0f); wf(mtx + 5 * 4, 1.0f);
    wf(mtx + 10 * 4, 1.0f); wf(mtx + 15 * 4, 1.0f);

    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0x01020000u | 0x20000u, 0x50000000u);
    put(dl + 0x08, 0x04000000u | (3u << 10), 0x60000000u);
    put(dl + 0x10, 0xBF000000u, 0x00000102u);
    put(dl + 0x18, 0xB8000000u, 0u);

    /* An identity matrix makes clip space == model space with w == 1, so the
     * geometry below IS the clip-space geometry and the analytic answer can
     * be written down.  A = (-2, 0) is outside LEFT (cw + cx = -1); B and C
     * are inside every plane.  LEFT cuts the two edges at cx == -1. */
    memset(verts, 0, sizeof(verts));
    wf(verts + 0x00 + 0x00, -2.0f); wf(verts + 0x00 + 0x04,  0.0f);
    wf(verts + 0x20 + 0x00,  0.0f); wf(verts + 0x20 + 0x04, -0.5f);
    wf(verts + 0x40 + 0x00,  0.0f); wf(verts + 0x40 + 0x04,  0.5f);
    for (i = 0; i < 3; ++i) {
        wf(verts + i * 0x20 + 0x14, 1.0f);
        wf(verts + i * 0x20 + 0x18, 1.0f);
        wf(verts + i * 0x20 + 0x1C, 1.0f);
    }

    BrDlInit(&st, 64, 64);
    check(free_count(&st) == BR_DL_CLIP_POOL,
          "0x10023B10 threads all 64 pool nodes onto the free list");
    BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
    watch_install(&st, &w);
    BrDlRun(&st, dl, sizeof(dl));

    check(st.cTriIn == 1 && st.cTriClipped == 1 && st.cTriDrawn == 0 &&
          st.cTriRejected == 0,
          "a straddling triangle enters the clipper, not the drop path");
    /* Cutting one corner off a triangle gives a quad, and a quad fans to
     * two triangles.  Both numbers are properties of the geometry, not of
     * the implementation. */
    check(st.cClipVtxMax == 4, "one crossed plane turns 3 vertices into 4");
    check(st.cTriClipOut == 2 && w.cTri == 2,
          "the quad reaches the sink as two triangles");
    check(w.fInside, "every emitted vertex satisfies all seven half-spaces");
    /* The whole triangle is 1.0; the part with cx >= -1 is a trapezoid of
     * parallel sides 0.5 and 1.0 and width 1, i.e. 0.75. */
    check(fabs(w.area - 0.75) < 1e-5,
          "the clipped area is the analytic 0.75, not the original 1.0");
    check(st.cClipStarved == 0 && st.cClipOverflow == 0,
          "no pool starvation and no polygon wider than the frame");
    check(free_count(&st) == BR_DL_CLIP_POOL,
          "every borrowed clip vertex was returned to the pool");

    /* Straddle from the other side: the same triangle mirrored in x must
     * cross RIGHT instead of LEFT and give the same area.  This is the test
     * that catches an inverted plane -- a sign error makes one of the two
     * pass and the other reject the triangle outright. */
    wf(verts + 0x00 + 0x00,  2.0f);
    BrDlInit(&st, 64, 64);
    BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
    watch_install(&st, &w);
    BrDlRun(&st, dl, sizeof(dl));
    check(st.cTriClipped == 1 && st.cTriClipOut == 2 && w.fInside &&
          fabs(w.area - 0.75) < 1e-5,
          "mirrored in x it crosses RIGHT and gives the same 0.75");

    /* Behind the eye.  Pushing one vertex to w < 0 must engage the W plane
     * (0x1001F0D0, the seventh and last call) and NEAR, and must not produce
     * a vertex with a non-positive w -- which is the failure that shows up as
     * a triangle smeared across the screen. */
    {
        static uint8_t m2[64];
        memset(m2, 0, sizeof(m2));
        wf(m2 + 0 * 4, 1.0f); wf(m2 + 5 * 4, 1.0f);
        wf(m2 + 10 * 4, 1.0f);
        wf(m2 + 11 * 4, 1.0f);      /* w = z + 0.5 */
        wf(m2 + 15 * 4, 0.5f);

        memset(verts, 0, sizeof(verts));
        wf(verts + 0x00 + 0x00, 0.0f); wf(verts + 0x00 + 0x08, -1.0f);
        wf(verts + 0x20 + 0x00, 0.2f); wf(verts + 0x20 + 0x08,  0.2f);
        wf(verts + 0x40 + 0x00, -0.2f); wf(verts + 0x40 + 0x08, 0.2f);
        for (i = 0; i < 3; ++i) {
            wf(verts + i * 0x20 + 0x14, 1.0f);
            wf(verts + i * 0x20 + 0x18, 1.0f);
            wf(verts + i * 0x20 + 0x1C, 1.0f);
        }
        BrDlInit(&st, 64, 64);
        BrDlAddRegion(&st, 0x50000000u, m2, sizeof(m2));
        BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
        watch_install(&st, &w);
        BrDlRun(&st, dl, sizeof(dl));
        check(st.cTriClipped == 1 && w.cTri > 0 && w.fInside,
              "a vertex behind the eye is clipped, not projected");
        check(free_count(&st) == BR_DL_CLIP_POOL,
              "and the pool is still whole afterwards");
    }
}

/* ------------------------------------------------------------------ */
/* 6. retail geometry                                                 */
/* ------------------------------------------------------------------ */

/* The .rca maps file 0x8000 to N64 0x803C8000 (CONVENTIONS.md), and the
 * segment fixup at 0x100189E0 is that identity plus a base. The port keeps
 * the 32-bit address and resolves through the region table, so the mapping
 * is registered rather than baked into the words. */
#define RCA_FILE_BASE  0x8000u
#define RCA_N64_BASE   0x803C8000u
#define ARENA_BASE     0x40000000u

extern void  BrVtxSwap(void *pVerts, int count);

typedef struct Ctx {
    uint8_t *pFile;
    size_t   cbFile;
    float   *pArena;
    size_t   cArena;      /* floats used */
    size_t   cArenaMax;
} Ctx;

/* Stands in for the Glide patch pass's call to 0x10018E10: swap the N64 Vtx
 * in place and expand it to the eight floats the transform reads.  This is
 * BrVtxExpand's arithmetic (0x10018EF0 == 0x1002BE30, SHARED) written out
 * here rather than called, because BrVtxCache is 0x800 entries and the test
 * only needs the conversion. */
static void resolve_vtx(void *pUser, uint32_t *pw1, int n)
{
    Ctx *c = (Ctx *)pUser;
    uint32_t addr = *pw1;
    size_t off;
    const uint8_t *pv;
    int i;

    if (addr < RCA_N64_BASE) { *pw1 = 0; return; }
    off = RCA_FILE_BASE + (size_t)(addr - RCA_N64_BASE);
    if (n <= 0 || off + (size_t)n * 16u > c->cbFile) { *pw1 = 0; return; }
    if (c->cArena + (size_t)n * 8u > c->cArenaMax)   { *pw1 = 0; return; }

    pv = c->pFile + off;
    BrVtxSwap(c->pFile + off, n);      /* the original swaps IN PLACE */

    {
        size_t start = c->cArena;
        for (i = 0; i < n; ++i) {
            const uint8_t *q = pv + (size_t)i * 16u;
            float *o = c->pArena + c->cArena;
            /* little-endian after the swap */
            #define S16(k) ((float)(int16_t)(uint16_t)((uint32_t)q[k] | ((uint32_t)q[(k)+1] << 8)))
            o[0] = S16(0x00); o[1] = S16(0x02); o[2] = S16(0x04);
            o[3] = S16(0x08); o[4] = S16(0x0A);
            #undef S16
            o[5] = (float)(int)(signed char)q[0x0C] * 0.0078125f;
            o[6] = (float)(int)(signed char)q[0x0D] * 0.0078125f;
            o[7] = (float)(int)(signed char)q[0x0E] * 0.0078125f;
            c->cArena += 8;
        }
        *pw1 = ARENA_BASE + (uint32_t)(start * 4u);
    }
}

static void run_rca(const char *pszPath)
{
    FILE *f = fopen(pszPath, "rb");
    long  cb;
    Ctx   ctx;
    uint8_t *marked;
    size_t off, nRuns = 0;
    size_t *aStart;
    BrDl st;
    BrDlRaster ras;
    uint8_t *rgba;
    float maxAbs = 1.0f;
    size_t i;
    int fEndOk = 1;
    int iCam;
    uint32_t cClipBase = 0;

    printf("%s\n", pszPath);
    if (!f) { check(0, "open"); return; }
    fseek(f, 0, SEEK_END); cb = ftell(f); rewind(f);
    ctx.cbFile = (size_t)cb;
    ctx.pFile  = (uint8_t *)malloc(ctx.cbFile);
    ctx.cArenaMax = 1u << 18;
    ctx.pArena = (float *)malloc(ctx.cArenaMax * sizeof(float));
    ctx.cArena = 0;
    marked = (uint8_t *)calloc(ctx.cbFile, 1);
    aStart = (size_t *)malloc(sizeof(size_t) * 4096);
    if (!ctx.pFile || !ctx.pArena || !marked || !aStart ||
        fread(ctx.pFile, 1, ctx.cbFile, f) != ctx.cbFile) {
        check(0, "read"); fclose(f); return;
    }
    fclose(f);

    /* Find each display-list run and patch it exactly once. A run is entered
     * at a big-endian G_VTX whose address word is a KSEG0 pointer -- the only
     * unambiguous entry signature available without the container's own
     * index, which has NOT been located (see the report). */
    for (off = RCA_FILE_BASE; off + 8 <= ctx.cbFile; off += 8) {
        uint32_t w0, w1;
        if (marked[off]) continue;
        w0 = ((uint32_t)ctx.pFile[off] << 24) | ((uint32_t)ctx.pFile[off+1] << 16) |
             ((uint32_t)ctx.pFile[off+2] << 8) | ctx.pFile[off+3];
        w1 = ((uint32_t)ctx.pFile[off+4] << 24) | ((uint32_t)ctx.pFile[off+5] << 16) |
             ((uint32_t)ctx.pFile[off+6] << 8) | ctx.pFile[off+7];
        if ((w0 >> 24) != 0x04u || (w1 >> 24) != 0x80u) continue;
        {
            size_t n = BrDlPatch(NULL, ctx.pFile + off, ctx.cbFile - off,
                                 resolve_vtx, &ctx);
            size_t k;
            if (n == 0) continue;
            for (k = 0; k < n * 8 && off + k < ctx.cbFile; ++k)
                marked[off + k] = 1;
            /* BrDlPatch stops at G_ENDDL; if it stopped because it ran out
             * of buffer the last command is not one. */
            if (ctx.pFile[off + (n - 1) * 8 + 3] != 0xB8u) fEndOk = 0;
            if (nRuns < 4096) aStart[nRuns++] = off;
        }
    }
    check(nRuns > 0, "found display-list runs");
    check(fEndOk, "every run terminates at G_ENDDL");
    check(ctx.cArena > 0, "G_VTX addresses resolved and expanded");

    for (i = 0; i < ctx.cArena; i += 8) {
        float a = fabsf(ctx.pArena[i]);
        float b = fabsf(ctx.pArena[i + 1]);
        float c = fabsf(ctx.pArena[i + 2]);
        if (a > maxAbs) maxAbs = a;
        if (b > maxAbs) maxAbs = b;
        if (c > maxAbs) maxAbs = c;
    }

    /* Three cameras over the same geometry.  The first is the one this test
     * has always used, and it is exactly the reason the clip path had never
     * executed: both retail models fit entirely inside it, so `clip` was 0 on
     * every run and the branch was dead.  The other two MOVE THE CAMERA so the
     * model genuinely straddles a plane -- which is the only way to exercise
     * a clipper, and the whole point of running them.
     *
     * All three are TEST SCAFFOLDING, not the original: these runs carry no
     * G_MTX, so the combined matrix would otherwise stay identity and every
     * vertex would land outside a unit frustum. */
    rgba = (uint8_t *)calloc(128u * 128u * 4u, 1);

    for (iCam = 0; iCam < 3; ++iCam) {
        static const char *const aszCam[3] = {
            "centred  (model wholly inside -- the historical case)",
            "panned   (translated right until it crosses RIGHT/LEFT)",
            "close-up (w scaled down until it crosses NEAR and W)"
        };
        Watch w;

        BrDlInit(&st, 128, 128);
        BrDlAddRegion(&st, ARENA_BASE, ctx.pArena, ctx.cArena * sizeof(float));
        BrDlAddRegion(&st, RCA_N64_BASE, ctx.pFile + RCA_FILE_BASE,
                      ctx.cbFile - RCA_FILE_BASE);

        memset(&st.combined, 0, sizeof(st.combined));
        st.combined.m[0][0] = 1.0f / maxAbs;
        st.combined.m[1][1] = 1.0f / maxAbs;
        st.combined.m[2][2] = 1.0f / maxAbs;
        st.combined.m[2][3] = 0.25f / maxAbs;
        st.combined.m[3][3] = 2.0f;
        if (iCam == 1) {
            /* Row 3 is the translation row (row-vector convention), so this
             * slides clip-space x by +1.6 against a half-width of w == 2. */
            st.combined.m[3][0] = 1.6f;
        } else if (iCam == 2) {
            /* Shrink the constant part of w so the model's own z drives it
             * through zero: cw = 0.35 + z/maxAbs. */
            st.combined.m[2][3] = 1.0f / maxAbs;
            st.combined.m[3][3] = 0.35f;
        }
        BrDlSetViewport(&st, 48.0f, 64.0f, -48.0f, 64.0f);

        memset(rgba, 0, 128u * 128u * 4u);
        ras.pRgba = rgba; ras.cx = 128; ras.cy = 128; ras.cCovered = 0;
        BrDlAttachRaster(&st, &ras);
        watch_install(&st, &w);

        for (i = 0; i < nRuns; ++i)
            BrDlRun(&st, ctx.pFile + aStart[i], ctx.cbFile - aStart[i]);

        printf("  camera %d: %s\n", iCam, aszCam[iCam]);
        printf("    runs=%-4u cmds=%-6u vtxload=%-4u verts=%-5u tri=%-5u "
               "drawn=%-5u rej=%-5u clip=%-5u clipout=%-5u killed=%-5u "
               "wmax=%u px=%u\n",
               (unsigned)nRuns, st.cCommands, st.cVtxLoads, st.cVtxTransformed,
               st.cTriIn, st.cTriDrawn, st.cTriRejected, st.cTriClipped,
               st.cTriClipOut, st.cTriClipKilled, st.cClipVtxMax,
               ras.cCovered);

        /* A thumbnail, printed rather than asserted. Coverage counts cannot
         * tell a model from a smear; a silhouette can, and it costs twelve
         * lines.  With a moved camera it is also the only cheap check that
         * the clipper produced a COHERENT shape and not confetti. */
        {
            int ty, tx;
            for (ty = 0; ty < 16; ++ty) {
                char row[41];
                for (tx = 0; tx < 40; ++tx) {
                    int sy = ty * 8, sx = tx * 3, k, l, lit = 0;
                    for (k = 0; k < 8 && !lit; ++k)
                        for (l = 0; l < 3 && !lit; ++l) {
                            int px = sx + l, py = sy + k;
                            if (px < 128 && py < 128 &&
                                rgba[((size_t)py * 128u + (size_t)px) * 4u + 3])
                                lit = 1;
                        }
                    row[tx] = (char)(lit ? '#' : '.');
                }
                row[40] = '\0';
                printf("    %s\n", row);
            }
        }

        /* Every triangle must have been accounted for exactly once.  This is
         * why cTriClipped keeps its old meaning -- "entered the clipper" --
         * and the clipper's own output is counted separately. */
        check(st.cTriIn == st.cTriDrawn + st.cTriRejected + st.cTriClipped,
              "every triangle is drawn, rejected or clipped -- none lost");
        check(st.cTriIn > 0 && ras.cCovered > 0,
              "retail geometry reaches the sink");
        /* The invariant that matters whether or not anything was clipped. */
        check(w.fInside,
              "every vertex handed to the sink is inside all seven planes");
        check(free_count(&st) == BR_DL_CLIP_POOL,
              "the 64-node clip pool is intact after the whole model");
        check(st.cClipStarved == 0, "the pool never ran dry");

        if (iCam == 0) {
            check(st.cTriClipped == 0 && st.cTriDrawn == st.cTriIn,
                  "centred: nothing is clipped -- this is the dead case");
            cClipBase = st.cTriClipped;
            /* An index outside the 32-entry array would mean the TRI byte
             * positions or the patch pass's halving is wrong.  Only sound on
             * the centred camera: the other two deliberately push vertices
             * out, so cVtxTransformed legitimately drops. */
            check(st.cVtxTransformed >= st.cVtxLoads * 3,
                  "vertex counts plausible");
            check(ras.cCovered > 500, "the model rasterises to a solid image");
        } else {
            check(st.cTriClipped > cClipBase && st.cTriClipOut > 0,
                  "moved camera: the clip path RAN and produced geometry");
            check(ras.cCovered > 500,
                  "and the result is still a solid model, not a smear");
        }

        /* The defining property of the outcodes, checked on the surviving
         * set -- it holds under every camera. */
        {
            int fOk = 1;
            for (i = 0; i < BR_DL_VTX_COUNT; ++i) {
                const BrDlVtx *v = &st.aVtx[i];
                if (v->outcode != 0) continue;
                if (!(v->cw > 0.0f) ||
                    fabsf(v->cx) > v->cw + 1e-3f ||
                    fabsf(v->cy) > v->cw + 1e-3f) fOk = 0;
            }
            check(fOk, "outcode 0 implies the vertex is inside the frustum");
        }
    }

    free(rgba); free(aStart); free(marked);
    free(ctx.pArena); free(ctx.pFile);
}

int main(void)
{
    test_layout();
    test_table();
    test_advance();
    test_combine();
    test_synthetic();
    test_clip();

    /* Not BR_REQUIRE_TESTDATA at the top: the five suites above need no
     * assets and must still run and still report on a fresh clone. */
    {
        FILE *probe = fopen("testdata/bb.rca", "rb");
        if (probe == NULL) {
            printf("  (testdata/bb.rca absent -- retail-geometry section "
                   "skipped; see README, 'Asset policy')\n");
        } else {
            fclose(probe);
            run_rca("testdata/bb.rca");
            run_rca("testdata/ce.rca");
        }
    }

    printf(g_fail ? "\n1 failures\n" : "\n0 failures\n");
    return g_fail;
}
