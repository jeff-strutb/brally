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
/* 5b. lighting                                                       */
/* ------------------------------------------------------------------ */

/* The bytes at BRGlide 0x100A9FF0, read out of the DLL: one libultra
 * `Lights1`, an 8-byte Ambient followed by a 16-byte Light.  The emitter at
 * 0x1000A88B sends +8 to G_MOVEMEM index 0x86 (slot 0, diffuse) and +0 to
 * index 0x88 (slot 1, ambient), which is gSPSetLights1's ordering exactly.
 * Using the SHIPPED numbers rather than invented ones means a sign error in
 * the direction shows up as a picture, not as an abstract failure. */
static const uint8_t g_aLights1[24] = {
    0x33, 0x33, 0x40, 0x00,  0x33, 0x33, 0x40, 0x00,            /* Ambient */
    0xEE, 0xEE, 0xCC, 0x00,  0xEE, 0xEE, 0xCC, 0x00,            /* Light   */
    0x54, 0x54, 0x54, 0x00
};
#define LIGHTS1_BASE 0x70000000u

/* The preamble the GAME emits and a shipped .rca does not carry: numlights,
 * the two light records, and the geometry mode with G_LIGHTING in it.
 * Measured: bb.rca and ce.rca contain zero 0xB6, 0xB7, 0xBC and 0x03
 * commands between them, so this state can only come from outside. */
static size_t light_preamble(uint8_t *p, uint32_t geo)
{
    put(p + 0x00, 0xBC000002u, 0x80000040u);        /* numlights, (w1>>5)&15 */
    put(p + 0x08, 0x03860010u, LIGHTS1_BASE + 8u);  /* slot 0: diffuse       */
    put(p + 0x10, 0x03880010u, LIGHTS1_BASE + 0u);  /* slot 1: ambient       */
    put(p + 0x18, 0xB7000000u, geo);                /* set geometry mode     */
    put(p + 0x20, 0xB8000000u, 0u);
    return 0x28;
}

#define GEO_LIT (BR_DL_GEO_ZBUFFER | BR_DL_GEO_SHADE | \
                 BR_DL_GEO_SHADING_SMOOTH | BR_DL_GEO_LIGHTING)

static void test_light_select(void)
{
    BrDl st;
    printf("lighting: which transform 0x1001FD70 installs\n");

    BrDlInit(&st, 64, 64);

    /* The seven rows of the table in PART 4 of br_dl.c, which is 0x1001FE0D
     * transcribed.  These are the ORIGINAL's addresses, so a future pass that
     * re-derives the selection has something exact to disagree with. */
    st.geoMode = BR_DL_GEO_ZBUFFER; st.fDecal = 0;
    check(BrDlVtxRoutine(&st) == 0x10021A20u && !BrDlIsLit(&st),
          "zbuffer, no lighting -> 0x10021A20, unlit");

    st.geoMode = GEO_LIT;
    check(BrDlVtxRoutine(&st) == 0x10021C70u && BrDlIsLit(&st),
          "zbuffer + lighting -> 0x10021C70, lit");

    st.fDecal = 1;
    check(BrDlVtxRoutine(&st) == 0x100221D0u && BrDlIsLit(&st),
          "...and the DECAL combiner row swaps in 0x100221D0");
    st.fDecal = 0;

    /* 0x1001FE48: texgen overrides the lighting choice, and TEXGEN_LINEAR
     * picks between the two.  Both still light. */
    st.geoMode = BR_DL_GEO_ZBUFFER | BR_DL_GEO_TEXTURE_GEN;
    check(BrDlVtxRoutine(&st) == 0x10022600u && BrDlIsLit(&st),
          "texgen -> 0x10022600 (lit) even with G_LIGHTING clear");
    st.geoMode |= BR_DL_GEO_TEXTURE_GEN_LIN;
    check(BrDlVtxRoutine(&st) == 0x10022BF0u,
          "texgen + linear -> 0x10022BF0");

    /* 0x1001FE99: without a z-buffer the choice is a different pair, and
     * texgen is not consulted at all. */
    st.geoMode = 0;
    check(BrDlVtxRoutine(&st) == 0x10023110u && !BrDlIsLit(&st),
          "no zbuffer, no lighting -> 0x10023110, unlit");
    st.geoMode = BR_DL_GEO_LIGHTING | BR_DL_GEO_TEXTURE_GEN;
    check(BrDlVtxRoutine(&st) == 0x10023360u && BrDlIsLit(&st),
          "no zbuffer + lighting -> 0x10023360, and texgen is ignored");
}

/* Drive one vertex whose normal is `n` through the machine and return the
 * colour the lit transform gave it.  The modelview is `pMtx` (16 floats, row
 * major) or identity when NULL. */
static void light_one(BrDl *pSt, const float *pMtx, const float *pN,
                      float *pOut, uint32_t geo)
{
    static uint8_t dl[64];
    static uint8_t verts[0x20];
    static uint8_t mtx[64];
    size_t n;
    int i;

    memset(mtx, 0, sizeof(mtx));
    if (pMtx) { for (i = 0; i < 16; ++i) wf(mtx + i * 4, pMtx[i]); }
    else      { wf(mtx + 0, 1.0f); wf(mtx + 5*4, 1.0f);
                wf(mtx + 10*4, 1.0f); wf(mtx + 15*4, 1.0f); }

    memset(verts, 0, sizeof(verts));
    wf(verts + 0x14, pN[0]); wf(verts + 0x18, pN[1]); wf(verts + 0x1C, pN[2]);

    memset(dl, 0, sizeof(dl));
    n = light_preamble(dl, geo);
    /* G_MTX load, modelview, no push -- so the light setup sees a real
     * matrix and the cache flag gets cleared by it, which is the ordering
     * the game uses too. */
    put(dl + n - 8, 0x01020000u | 0x20000u, 0x50000000u);
    put(dl + n + 0x00, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + n + 0x08, 0xB8000000u, 0u);

    BrDlInit(pSt, 64, 64);
    BrDlAddRegion(pSt, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));
    BrDlAddRegion(pSt, 0x50000000u, mtx, sizeof(mtx));
    BrDlAddRegion(pSt, 0x60000000u, verts, sizeof(verts));
    BrDlRun(pSt, dl, sizeof(dl));

    pOut[0] = pSt->aVtx[0].n0;
    pOut[1] = pSt->aVtx[0].n1;
    pOut[2] = pSt->aVtx[0].n2;
}

static void test_light_math(void)
{
    BrDl st;
    float c[3], c2[3], nL[3], nBack[3], nPerp[3];
    int i;

    printf("lighting: the light record and the shading model\n");

    /* --- the record layout, asserted against the shipped bytes ------ */
    {
        static uint8_t dl[64];
        size_t n = light_preamble(dl, GEO_LIT);
        (void)n;
        BrDlInit(&st, 64, 64);
        BrDlAddRegion(&st, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));
        BrDlRun(&st, dl, sizeof(dl));

        check(st.nLights == 2,
              "G_MOVEWORD numlight: (0x80000040 >> 5) & 0xF == 2");
        /* G_MOVEMEM 0x86 -> slot 0 at +0x10 of the Lights1 blob. */
        check(st.aLight[BR_DL_LIGHT_DIFFUSE][BR_DL_LIGHT_COL + 0] == 0xEE &&
              st.aLight[BR_DL_LIGHT_DIFFUSE][BR_DL_LIGHT_COL + 2] == 0xCC &&
              st.aLight[BR_DL_LIGHT_DIFFUSE][BR_DL_LIGHT_DIR + 0] == 0x54,
              "index 0x86 loads slot 0: colour at +0, direction at +8");
        check(st.aLight[BR_DL_LIGHT_AMBIENT][BR_DL_LIGHT_COL + 0] == 0x33 &&
              st.aLight[BR_DL_LIGHT_AMBIENT][BR_DL_LIGHT_COL + 2] == 0x40,
              "index 0x88 loads slot 1: the ambient colour");
        check(st.fLightCached == 0,
              "and every light command leaves the cache flag clear");
    }

    /* --- the derived state ------------------------------------------ */
    {
        float n0[3] = { 0.0f, 0.0f, 1.0f };
        light_one(&st, NULL, n0, c, GEO_LIT);
        check(st.cLightSetup == 1, "the derived state is rebuilt exactly once");
        check(st.lightScale[0] == 238.0f && st.lightScale[2] == 204.0f,
              "the light colour is the raw bytes, 0..255 and NOT divided");
        check(st.lightAmb[0] == 51.0f && st.lightAmb[2] == 64.0f,
              "the ambient likewise");
        /* (84,84,84)/128 normalised is (1,1,1)/sqrt(3). */
        check(fabsf(st.lightDir[0] - 0.5773503f) < 1e-5f &&
              fabsf(st.lightDir[1] - st.lightDir[0]) < 1e-6f &&
              fabsf(st.lightDir[2] - st.lightDir[0]) < 1e-6f,
              "the direction is the signed bytes /128, normalised");
        for (i = 0; i < 3; ++i) nL[i] = st.lightDir[i];
    }

    /* --- the model ---------------------------------------------------- */
    /* n == L: t == 1, so the unclamped colour would be 289/289/268 and every
     * channel saturates.  That the CLAMP is a substitution of the literal
     * 255.0f rather than a min() is invisible here because the two agree;
     * what is visible is that nothing exceeds 255. */
    light_one(&st, NULL, nL, c, GEO_LIT);
    check(c[0] == 255.0f && c[1] == 255.0f && c[2] == 255.0f,
          "n parallel to L saturates: 238+51 and 204+64 both clamp to 255");

    /* A partially-lit normal, where the arithmetic is actually observable.
     * The expectation is computed from the dot product rather than written
     * down, so it tests the model and not a number. */
    {
        float v[3], t, e;
        int k, fOk = 1;
        v[0] = 0.6f; v[1] = 0.0f; v[2] = 0.8f;      /* unit */
        t = v[0]*nL[0] + v[1]*nL[1] + v[2]*nL[2];
        light_one(&st, NULL, v, c, GEO_LIT);
        for (k = 0; k < 3; ++k) {
            e = t * st.lightScale[k] + st.lightAmb[k];
            if (e > 255.0f) e = 255.0f;
            if (fabsf(c[k] - e) > 1e-3f) fOk = 0;
        }
        check(fOk && c[0] > 51.0f && c[0] < 255.0f,
              "a partly-lit normal gives t*colour + ambient, unsaturated");
    }

    /* The bound the whole model lives inside, and the one a sign error or a
     * backwards clamp breaks: every channel is between the ambient and the
     * lesser of 255 and ambient+colour. */
    {
        int k, fOk = 1;
        for (k = 0; k <= 12; ++k) {
            float a = (float)k / 12.0f * 6.2831853f, v[3], lo, hi;
            int j;
            v[0] = cosf(a); v[1] = sinf(a); v[2] = 0.0f;
            light_one(&st, NULL, v, c, GEO_LIT);
            for (j = 0; j < 3; ++j) {
                lo = st.lightAmb[j];
                hi = lo + st.lightScale[j];
                if (hi > 255.0f) hi = 255.0f;
                if (c[j] < lo - 1e-3f || c[j] > hi + 1e-3f) fOk = 0;
            }
        }
        check(fOk, "every channel stays within [ambient, min(255, amb+col)]");
    }

    /* n == -L: t < 0, ambient only -- and note this arm does NOT use the
     * clamp and does NOT use the scale. */
    for (i = 0; i < 3; ++i) nBack[i] = -nL[i];
    light_one(&st, NULL, nBack, c, GEO_LIT);
    check(c[0] == 51.0f && c[1] == 51.0f && c[2] == 64.0f,
          "n opposed to L gives the ambient exactly");
    check(st.cVtxLitAmbient == 1, "and takes the ambient-only arm");

    /* t == 0 exactly is the boundary, and the original's test is `!(t >= 0)`
     * -> lit, so zero must take the LIT arm and land on the ambient value by
     * arithmetic rather than by the early return. */
    nPerp[0] = 1.0f / sqrtf(2.0f); nPerp[1] = -nPerp[0]; nPerp[2] = 0.0f;
    light_one(&st, NULL, nPerp, c, GEO_LIT);
    check(fabsf(c[0] - 51.0f) < 1e-3f && fabsf(c[2] - 64.0f) < 1e-3f,
          "n perpendicular to L gives the ambient");
    check(st.cVtxLitAmbient == 0,
          "...through the LIT arm: t == 0 is inside, not outside");

    /* Monotone in n.L, which a sign error on either the dot or the direction
     * transform breaks. */
    {
        float prev = -1.0f;
        int fMono = 1, k;
        for (k = 0; k <= 8; ++k) {
            float a = (float)k / 8.0f;      /* lerp -L .. +L */
            float v[3];
            for (i = 0; i < 3; ++i) v[i] = nL[i] * (2.0f * a - 1.0f);
            light_one(&st, NULL, v, c, GEO_LIT);
            if (c[2] < prev - 1e-4f) fMono = 0;
            prev = c[2];
        }
        check(fMono, "the colour is non-decreasing as n turns towards L");
    }

    /* THE INVARIANT THAT PINS THE DIRECTION TRANSFORM, and the reason it is
     * worth a test of its own: the setup dots the light with the modelview's
     * ROWS, which under this file's row-vector convention is M applied as a
     * COLUMN-vector matrix -- i.e. M-transpose, i.e. M's inverse for a
     * rotation.  Reading it the other way round is the classic error and it
     * is invisible on axis-aligned geometry.
     *
     * The property: shading depends only on the WORLD-space normal.  So for
     * any rotation R, a model-space normal n under modelview R must shade
     * exactly as the world normal (n * R) does under the identity -- because
     *     n . (R applied to d)  ==  (n * R) . d.
     * Both sides are computed here; nothing is written down. */
    {
        float ang = 0.7f, ca = cosf(ang), sa = sinf(ang);
        float R[16] = { 0 };
        float nModel[3], nWorld[3];
        R[0] = ca;  R[1] = sa;                /* row-major, row-vector */
        R[4] = -sa; R[5] = ca;
        R[10] = 1.0f; R[15] = 1.0f;

        nModel[0] = 0.3f; nModel[1] = 0.5f; nModel[2] = 0.81f;
        nWorld[0] = nModel[0]*R[0] + nModel[1]*R[4] + nModel[2]*R[8];
        nWorld[1] = nModel[0]*R[1] + nModel[1]*R[5] + nModel[2]*R[9];
        nWorld[2] = nModel[0]*R[2] + nModel[1]*R[6] + nModel[2]*R[10];

        light_one(&st, NULL, nWorld, c, GEO_LIT);
        light_one(&st, R, nModel, c2, GEO_LIT);
        check(fabsf(c[0] - c2[0]) < 1e-2f && fabsf(c[1] - c2[1]) < 1e-2f &&
              fabsf(c[2] - c2[2]) < 1e-2f,
              "the light is pulled into MODEL space: n under M shades as "
              "(n*M) under the identity");
        /* And the transposed reading really is different, so the assertion
         * above has teeth: R is not symmetric, so n*R != n*R-transpose. */
        check(fabsf(nWorld[0] - (nModel[0]*R[0] + nModel[1]*R[1] +
                                 nModel[2]*R[2])) > 0.05f,
              "...and the two readings of R genuinely differ here");
    }

    /* Unlit geometry must still get the raw 1/128-scaled bytes: the whole
     * point of the two transforms is that only one of them touches r/g/b. */
    {
        float n0[3] = { 0.25f, -0.5f, 0.75f };
        light_one(&st, NULL, n0, c, BR_DL_GEO_ZBUFFER);
        check(c[0] == 0.25f && c[1] == -0.5f && c[2] == 0.75f,
              "with G_LIGHTING clear the trailing bytes pass through");
        check(st.cVtxLit == 0 && st.cLightSetup == 0,
              "and no light setup happens at all");
        check(BrDlColourScale(&st) == 1.0f,
              "BrDlColourScale reports the unlit convention");
    }
    {
        float n0[3] = { 0.0f, 0.0f, 1.0f };
        light_one(&st, NULL, n0, c, GEO_LIT);
        check(BrDlColourScale(&st) == 1.0f / 255.0f,
              "and the Glide 0..255 convention once a light ran");
    }
}

static void test_light_cache(void)
{
    static uint8_t dl[128];
    static uint8_t mtx[64];
    static uint8_t verts[0x20];
    BrDl st;
    size_t n;
    int i;

    printf("lighting: what invalidates the derived state\n");

    memset(mtx, 0, sizeof(mtx));
    wf(mtx + 0, 1.0f); wf(mtx + 5*4, 1.0f); wf(mtx + 10*4, 1.0f);
    wf(mtx + 15*4, 1.0f);
    memset(verts, 0, sizeof(verts));
    wf(verts + 0x1C, 1.0f);

    /* Two G_VTX with nothing between them: one setup. */
    memset(dl, 0, sizeof(dl));
    n = light_preamble(dl, GEO_LIT);
    put(dl + n - 8, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + n + 0x00, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + n + 0x08, 0xB8000000u, 0u);
    BrDlInit(&st, 64, 64);
    BrDlAddRegion(&st, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
    BrDlRun(&st, dl, sizeof(dl));
    check(st.cVtxLit == 2 && st.cLightSetup == 1,
          "two G_VTX, one setup: 0x105D17D0 latches");

    /* A PROJECTION G_MTX between them must NOT invalidate: 0x10021080's two
     * projection arms jump past the store at 0x1002116E. */
    for (i = 0; i < 2; ++i) {
        uint32_t mode = i ? (0x10000u | 0x20000u)      /* projection, load */
                          : 0x20000u;                  /* modelview,  load */
        memset(dl, 0, sizeof(dl));
        n = light_preamble(dl, GEO_LIT);
        put(dl + n - 8, 0x04000000u | (1u << 10), 0x60000000u);
        put(dl + n + 0x00, 0x01000000u | mode, 0x50000000u);
        put(dl + n + 0x08, 0x04000000u | (1u << 10), 0x60000000u);
        put(dl + n + 0x10, 0xB8000000u, 0u);
        BrDlInit(&st, 64, 64);
        BrDlAddRegion(&st, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));
        BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
        BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
        BrDlRun(&st, dl, sizeof(dl));
        check(st.cLightSetup == (uint32_t)(i ? 1 : 2),
              i ? "a PROJECTION G_MTX does not invalidate the light cache"
                : "a MODELVIEW G_MTX LOAD does -- and used not to");
    }

    /* And a G_MOVEWORD LIGHTCOL does. */
    memset(dl, 0, sizeof(dl));
    n = light_preamble(dl, GEO_LIT);
    put(dl + n - 8, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + n + 0x00, 0xBC00000Au, 0x10203000u);   /* slot 0, triple at +0 */
    put(dl + n + 0x08, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + n + 0x10, 0xB8000000u, 0u);
    BrDlInit(&st, 64, 64);
    BrDlAddRegion(&st, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));
    BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
    BrDlRun(&st, dl, sizeof(dl));
    check(st.cLightSetup == 2 && st.lightScale[0] == 16.0f &&
          st.lightScale[1] == 32.0f && st.lightScale[2] == 48.0f,
          "G_MOVEWORD LIGHTCOL invalidates and the new colour takes effect");
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

    /* ---------------------------------------------------------------
     * BEFORE / AFTER: the same model, unlit and lit
     * ---------------------------------------------------------------
     * The point of the whole exercise, and the only assertion in this file
     * that is about a PICTURE.  Same geometry, same camera, same rasteriser;
     * the only difference is the five-command preamble the game emits and a
     * shipped .rca does not carry (measured: bb.rca and ce.rca contain zero
     * 0xB6/0xB7/0xBC/0x03 commands, so the light state cannot come from the
     * model).  The lights are the SHIPPED bytes at BRGlide 0x100A9FF0.
     *
     * The ramp is luminance, so a flat model prints as one character and a
     * shaded one prints as a gradient -- which is exactly the difference
     * being demonstrated.  There is no G_MTX in these runs, so the modelview
     * stays identity and the light direction is used as authored. */
    free(rgba);
    rgba = (uint8_t *)calloc(128u * 128u * 4u, 1);
    {
        static uint8_t pre[64];
        size_t cbPre = light_preamble(pre, GEO_LIT);
        int iPass;
        uint32_t aLevels[2] = { 0, 0 };
        uint32_t cLit = 0, cAmb = 0;
        int fInRange = 1;

        for (iPass = 0; iPass < 2; ++iPass) {
            uint8_t seen[16];
            uint32_t nLevel = 0;
            int ty, tx, k;

            BrDlInit(&st, 128, 128);
            BrDlAddRegion(&st, ARENA_BASE, ctx.pArena,
                          ctx.cArena * sizeof(float));
            BrDlAddRegion(&st, RCA_N64_BASE, ctx.pFile + RCA_FILE_BASE,
                          ctx.cbFile - RCA_FILE_BASE);
            BrDlAddRegion(&st, LIGHTS1_BASE, g_aLights1, sizeof(g_aLights1));

            memset(&st.combined, 0, sizeof(st.combined));
            st.combined.m[0][0] = 1.0f / maxAbs;
            st.combined.m[1][1] = 1.0f / maxAbs;
            st.combined.m[2][2] = 1.0f / maxAbs;
            st.combined.m[2][3] = 0.25f / maxAbs;
            st.combined.m[3][3] = 2.0f;
            BrDlSetViewport(&st, 48.0f, 64.0f, -48.0f, 64.0f);

            memset(rgba, 0, 128u * 128u * 4u);
            ras.pRgba = rgba; ras.cx = 128; ras.cy = 128; ras.cCovered = 0;
            BrDlAttachRaster(&st, &ras);

            if (iPass == 1)
                BrDlRun(&st, pre, cbPre);
            for (i = 0; i < nRuns; ++i)
                BrDlRun(&st, ctx.pFile + aStart[i], ctx.cbFile - aStart[i]);

            printf("  %s  (G_VTX handler %08X)\n",
                   iPass ? "AFTER  -- G_LIGHTING set, one directional light "
                           "+ ambient"
                         : "BEFORE -- as shipped: no light state at all",
                   BrDlVtxRoutine(&st));
            printf("    lit=%-5u ambient-only=%-5u setups=%u  px=%u\n",
                   st.cVtxLit, st.cVtxLitAmbient, st.cLightSetup,
                   ras.cCovered);

            /* 2x4 blocks, AVERAGED (a max would flatten the gradient into
             * one level and hide the very thing this is here to show), and
             * cropped to rows 16..47 because the model occupies the middle
             * of a 128px frame and blank rows are not evidence. */
            memset(seen, 0, sizeof(seen));
            for (ty = 0; ty < 32; ++ty) {
                char row[65];
                int fAny = 0;
                for (tx = 0; tx < 64; ++tx) {
                    int sum = 0, cov = 0, mx, my;
                    for (my = 0; my < 4; ++my)
                        for (mx = 0; mx < 2; ++mx) {
                            const uint8_t *q =
                                rgba + (((size_t)(ty * 4 + my) * 128u) +
                                        (size_t)(tx * 2 + mx)) * 4u;
                            if (!q[3]) continue;
                            sum += (77 * q[0] + 150 * q[1] + 29 * q[2]) >> 8;
                            cov++;
                        }
                    if (!cov) { row[tx] = ' '; continue; }
                    k = (sum / cov) * 9 / 255;
                    if (k > 9) k = 9;
                    if (k < 0) k = 0;
                    seen[k] = 1;
                    fAny = 1;
                    row[tx] = " .:-=+*#%@"[k];
                }
                row[64] = '\0';
                if (fAny)                     /* auto-crop: blank rows are
                                               * not evidence of anything */
                    printf("    |%s|\n", row);
            }

            /* And the render itself, so the comparison is not only ASCII.
             * Same naming scheme test_gfx uses. */
            {
                const char *pszLeaf = strrchr(pszPath, '/');
                char szOut[256];
                FILE *fo;
                pszLeaf = pszLeaf ? pszLeaf + 1 : pszPath;
                sprintf(szOut, "build/dl_%s.%s.ppm", pszLeaf,
                        iPass ? "lit" : "unlit");
                fo = fopen(szOut, "wb");
                if (fo) {
                    size_t q;
                    fprintf(fo, "P6\n128 128\n255\n");
                    for (q = 0; q < 128u * 128u; ++q)
                        fwrite(rgba + q * 4u, 1, 3, fo);
                    fclose(fo);
                    printf("    -> %s\n", szOut);
                }
            }
            for (k = 0; k < 10; ++k) nLevel += seen[k];
            aLevels[iPass] = nLevel;

            if (iPass == 1) {
                cLit = st.cVtxLit;
                cAmb = st.cVtxLitAmbient;
                /* Every colour the lit pass produced must be a value the
                 * shading model can produce.  A wrong sign, a missing
                 * ambient or a broken clamp all break this. */
                for (i = 0; i < BR_DL_VTX_COUNT; ++i) {
                    int j;
                    const float *pc = &st.aVtx[i].r;
                    if (st.aVtx[i].outcode != 0) continue;
                    for (j = 0; j < 3; ++j) {
                        float lo = st.lightAmb[j];
                        float hi = lo + st.lightScale[j];
                        if (hi > 255.0f) hi = 255.0f;
                        if (pc[j] < lo - 1e-2f || pc[j] > hi + 1e-2f)
                            fInRange = 0;
                    }
                }
            }
        }

        check(cLit > 0 && cAmb > 0,
              "the lit pass ran and some faces genuinely turn away from L");
        check(aLevels[1] > aLevels[0] && aLevels[1] >= 4,
              "the lit render has more distinct brightness levels: it is "
              "shaded, the unlit one is not");
        check(fInRange,
              "every lit colour lies in [ambient, min(255, ambient+colour)]");
    }
    free(rgba);

    free(aStart); free(marked);
    free(ctx.pArena); free(ctx.pFile);
}

/* ------------------------------------------------------------------ */
/* 9. RDP state: the seven opcode readings that were wrong             */
/* ------------------------------------------------------------------ */
/* MUTATIONS KILLED (run under -fsanitize=address,undefined; each defect was
 * reinstated on its own, the suite rebuilt, and the red assertions recorded):
 *
 *   M1  route 0xE2 at the 0xED decode              -> 3 red
 *   M2  store the scissor's raw fields, no Y flip  -> 3 red
 *   M3a 0xE1 masks 0xFFF instead of sign-extending -> 3 red
 *   M3b drop the +1/-1 on the emitter window       -> 3 red
 *   M4  swap the untextured corners (ul from w0)   -> 4 red
 *   M5  scale 0xFA's prim colour by 1/255          -> 3 red
 *   M6  keep 0xF7 as a raw store                   -> 2 red
 *   M7  snap ties AWAY from zero instead of even   -> 3 red
 *   M8  lights-off fallback reads an unwritten
 *       lightOff[] instead of prim[]               -> 1 red
 *   M10 BrDlRun's bound back to `p < pEnd`         -> ASan
 *                                                     global-buffer-overflow
 *
 * (M9 is in test_slice2_16.c: dropping the +4 from G_SETTILESIZE's extent
 * turns five assertions red there.)
 *
 * Every one was detected.  The pre-existing quarter-pixel assertion in
 * test_synthetic is NOT in this table, and could not be: it checks that the
 * result lands on a quarter-pixel, which is true under both roundings. */
/* Every number below is computed from the disassembly of orig/BRGlide.dll
 * and written down here BEFORE the code was changed, not read back out of
 * the port.  Each block names the instructions it rests on, so the next
 * reader can disagree with the binary rather than with this file.
 *
 * All of these are 640x480, which is what 0x100A7514 / 0x100A7518 hold in the
 * shipped image, so H == 480 throughout. */

static struct { int fTex, tile; int32_t ulx, uly, lrx, lry; int n; } g_rc;
static void on_rect2(void *u, int fTex, int tile, int32_t a, int32_t b,
                     int32_t c, int32_t d)
{
    (void)u;
    g_rc.fTex = fTex; g_rc.tile = tile;
    g_rc.ulx = a; g_rc.uly = b; g_rc.lrx = c; g_rc.lry = d;
    g_rc.n++;
}

/* Run one two-word command and stop.  The terminator goes at BOTH +8 and
 * +0x18 because 0xE4 is the one opcode here that advances 0x18 and would
 * otherwise step straight over a terminator at +8. */
static void one_cmd(BrDl *pSt, uint32_t w0, uint32_t w1)
{
    static uint8_t dl[64];
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, w0, w1);
    put(dl + 0x08, 0xB8000000u, 0u);
    put(dl + 0x18, 0xB8000000u, 0u);
    BrDlRun(pSt, dl, sizeof(dl));
}

/* The port-only bound in BrDlRun, and the position it used to miss.
 *
 * The original walks `while (p) p = table[p[3]](p)` with no bound at all, so
 * every byte of this is a DEVIATION -- but a deviation that reads off the end
 * of the buffer is not a deviation, it is a defect, and this is the case that
 * exercised it: three no-op commands in a 24-byte list leave the cursor on
 * pEnd exactly.  `p < pEnd` is false there, so the guard was skipped and
 * `p[3]` read a fourth byte past the array.  ASan reported it as a
 * global-buffer-overflow; without ASan it read whatever followed in .bss and
 * the suite merely looked flaky. */
static void test_walk_bound(void)
{
    static uint8_t dl[24];
    BrDl st;
    size_t n;

    printf("BrDlRun's bound stops ON the end, not one byte past it\n");

    /* Three unhandled opcodes, no G_ENDDL: 3 * 8 == 24 == sizeof(dl), so the
     * third handler returns exactly pEnd. */
    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0xF5000000u, 0u);
    put(dl + 0x08, 0xF5000000u, 0u);
    put(dl + 0x10, 0xF5000000u, 0u);
    BrDlInit(&st, 640, 480);
    n = BrDlRun(&st, dl, sizeof(dl));
    check(n == 3 && st.cUnhandled == 3,
          "a list that ends exactly on pEnd runs its last command and stops");

    /* And a list with a trailing fragment shorter than one command still
     * stops before reading it -- the case the bound was written for. */
    {
        static uint8_t dl2[20];
        memset(dl2, 0, sizeof(dl2));
        put(dl2 + 0x00, 0xF5000000u, 0u);
        put(dl2 + 0x08, 0xF5000000u, 0u);
        BrDlInit(&st, 640, 480);
        n = BrDlRun(&st, dl2, sizeof(dl2));
        check(n == 2, "a four-byte tail is not decoded as a command");
    }
}

static void test_scissor_ops(void)
{
    BrDl st;

    printf("0xE2 / 0xED: two conventions, one clip window\n");

    /* w0 and w1 are chosen so the two decodes cannot coincide.
     *   w0 = 0x0002C051, w1 = 0x001400F0
     * 0xE2 (0x1001EBC0, `shr 0xC / and 0xFFF` and `and 0xFFF`):
     *     ulx = 0x02C = 44   uly = 0x051 = 81
     *     lrx = 0x140 = 320  lry = 0x0F0 = 240
     * 0xED (0x1001EB50, `shr 0xE / and 0x3FF` and `shr 2 / and 0x3FF`):
     *     ulx = 11   uly = 20   lrx = 80   lry = 60
     * and both then store minX=ulx, minY=H-lry, maxX=lrx, maxY=H-uly. */
    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xE202C051u, 0x001400F0u);
    check(st.scisMinX == 44 && st.scisMaxX == 320,
          "0xE2 reads the X pair as plain 12-bit integers");
    check(st.scisMinY == 480 - 240 && st.scisMaxY == 480 - 81,
          "0xE2 flips Y against 0x100A7518: minY = H-lry, maxY = H-uly");

    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xED02C051u, 0x001400F0u);
    check(st.scisMinX == 11 && st.scisMaxX == 80,
          "0xED reads the SAME words as 10.2 -- a different answer");
    check(st.scisMinY == 480 - 60 && st.scisMaxY == 480 - 20,
          "0xED flips Y the same way");

    /* The property the un-flipped version broke, stated on its own: the
     * window 0x1001E380 clamps against must have min below max.  With the raw
     * fields stored, uly < lry makes scisMinY the LARGER of the two. */
    check(st.scisMinY < st.scisMaxY,
          "and the window comes out with min below max, not inverted");

    /* The two opcodes must not be the same host function.  Feeding both the
     * same words and getting the same window would mean one decode is
     * serving two slots -- which is exactly the defect. */
    {
        BrDl a, b;
        BrDlInit(&a, 640, 480); one_cmd(&a, 0xE202C051u, 0x001400F0u);
        BrDlInit(&b, 640, 480); one_cmd(&b, 0xED02C051u, 0x001400F0u);
        check(a.scisMinX != b.scisMinX && a.scisMaxX != b.scisMaxX &&
              a.scisMinY != b.scisMinY && a.scisMaxY != b.scisMaxY,
              "0xE2 and 0xED disagree on all four corners: two functions");
    }
}

static void test_fill_rects(void)
{
    BrDl st;

    printf("0xE1 / 0xF6: which word is which corner, and the sign\n");

    /* 0xE1, 0x1001E720.  `shl 20 / sar 20` on all four fields, so a 12-bit
     * field of 0xFF8 is -8 and NOT 4088.  w0 carries the LOWER-RIGHT pair and
     * w1 the UPPER-LEFT: the pushes at 0x1001E752..0x1001E759 are
     * (H - w1.lo, w0.hi + 1, H - w0.lo - 1, w1.hi), read last-first as
     *     0x1001E380(ulx, H - lry - 1, lrx + 1, H - uly). */
    BrDlInit(&st, 640, 480);
    st.sink.pfnRect = on_rect2;
    g_rc.n = 0;
    one_cmd(&st, 0xE1064032u, 0x00FF8FFCu);
    check(g_rc.n == 1 && g_rc.fTex == 0, "0xE1 is one untextured rectangle");
    check(g_rc.ulx == -8 && g_rc.uly == -4,
          "0xE1 takes the UPPER-LEFT from w1, sign-extended: (-8, -4)");
    check(g_rc.lrx == 100 && g_rc.lry == 50,
          "...and the LOWER-RIGHT from w0: (100, 50)");
    check(st.rectMinX == -8 && st.rectMaxX == 101,
          "the emitter gets ulx and lrx+1 -- `inc edi` at 0x1001E74E");
    check(st.rectMinY == 480 - 50 - 1 && st.rectMaxY == 480 + 4,
          "and H-lry-1 / H-uly -- `dec edx` at 0x1001E753");

    /* 0xF6, 0x1001E320.  Same shape, but `sar 0x16` plus `and 0x3FF`, so the
     * sign extension is masked straight off again: unsigned 10.2.  Corners
     * lr = (100, 50) and ul = (10, 20) in pixels. */
    BrDlInit(&st, 640, 480);
    st.sink.pfnRect = on_rect2;
    g_rc.n = 0;
    one_cmd(&st, 0xF61900C8u, 0x00028050u);
    check(g_rc.ulx == 10 && g_rc.uly == 20,
          "0xF6 takes the UPPER-LEFT from w1 too: (10, 20)");
    check(g_rc.lrx == 100 && g_rc.lry == 50,
          "...and the LOWER-RIGHT from w0: (100, 50)");
    check(st.rectMinX == 10 && st.rectMinY == 480 - 50 - 1 &&
          st.rectMaxX == 101 && st.rectMaxY == 480 - 20,
          "0xF6 hands 0x1001E380 the same four expressions as 0xE1");

    /* THE CROSS-CHECK THAT WOULD HAVE CAUGHT IT.  0xE4 (0x10021570) reads its
     * first two words with the same corner assignment as 0xF6 -- its pushes
     * into 0x100215C0 are (tile, w0.lo, w0.hi, w1.lo, w1.hi), i.e. w1 first.
     * Feed the two the same eight bytes and the four corners must match. */
    {
        int32_t f6[4];
        BrDlInit(&st, 640, 480); st.sink.pfnRect = on_rect2; g_rc.n = 0;
        one_cmd(&st, 0xF61900C8u, 0x00028050u);
        f6[0] = g_rc.ulx; f6[1] = g_rc.uly; f6[2] = g_rc.lrx; f6[3] = g_rc.lry;

        BrDlInit(&st, 640, 480); st.sink.pfnRect = on_rect2; g_rc.n = 0;
        one_cmd(&st, 0xE41900C8u, 0x00028050u);
        check(g_rc.fTex == 1 && g_rc.ulx == f6[0] && g_rc.uly == f6[1] &&
              g_rc.lrx == f6[2] && g_rc.lry == f6[3],
              "0xF6 and 0xE4 assign the same eight bytes to the same corners");
    }

    /* 0xE3 (0x100219D0) is the integer form of the same call: it shifts each
     * field LEFT two on the way in, so the pixel corners come out identical
     * to 0xE4's when the fields are a quarter as large. */
    BrDlInit(&st, 640, 480);
    st.sink.pfnRect = on_rect2;
    g_rc.n = 0;
    one_cmd(&st, 0xE3064032u, 0x0000A014u);
    check(g_rc.ulx == 10 && g_rc.uly == 20 &&
          g_rc.lrx == 100 && g_rc.lry == 50,
          "0xE3 multiplies by four and lands on the same pixels");
}

static void test_colours(void)
{
    BrDl st;

    printf("0xFA / 0xFB / 0xF7: three colour payloads, three decodes\n");

    /* 0xFA, 0x1001EA80: four `fild qword ; fstp dword`, and there is no
     * `fmul` in the whole 138 bytes.  0..255.
     * 0xFB, 0x1001E930: the same four, each followed by
     * `fmul [0x10077400]`, and 0x10077400 is 0x3B808081 == 1/255.  0..1. */
    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xFA000000u, 0x8040C0FFu);
    check(st.prim[0] == 128.0f && st.prim[1] == 64.0f &&
          st.prim[2] == 192.0f && st.prim[3] == 255.0f,
          "0xFA stores the four bytes UNSCALED: 0..255");

    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xFB000000u, 0x8040C0FFu);
    /* Written as a MULTIPLY by the constant, not as a divide by 255: the
     * original is `fmul [0x10077400]` and 0x3B808081 is the float nearest
     * 1/255, so `128.0f * k` and `128.0f / 255.0f` differ in the last bit.
     * Asserting the divide form fails, and the code is the right one. */
    check(st.env[0] == 128.0f * (1.0f / 255.0f) &&
          st.env[1] == 64.0f * (1.0f / 255.0f) &&
          st.env[2] == 192.0f * (1.0f / 255.0f) && st.env[3] == 1.0f,
          "0xFB multiplies each by 1/255: 0..1");

    /* Stated as the relation, because that is what one shared unpack
     * destroyed: the same word through the two opcodes differs by exactly
     * the factor 255. */
    {
        BrDl a, b;
        BrDlInit(&a, 640, 480); one_cmd(&a, 0xFA000000u, 0x8040C0FFu);
        BrDlInit(&b, 640, 480); one_cmd(&b, 0xFB000000u, 0x8040C0FFu);
        check(fabsf(a.prim[0] - b.env[0] * 255.0f) < 1e-3f &&
              fabsf(a.prim[2] - b.env[2] * 255.0f) < 1e-3f &&
              a.prim[0] != b.env[0],
              "prim is env times 255 -- the two handlers are not one handler");
    }

    /* The consequence that pins WHICH of the two is right, independently of
     * the fmul: 0x10022AC0's numlights==0 arm (0x10022BCC) copies 0x105D17A4,
     * 0x105D17B4 and 0x105CE2D0 -- 0xFA's own first three destinations --
     * into the vertex colour, in a pipeline whose ceiling (0x10077418) is
     * 255.0f.  A 0..1 prim would make that arm 255 times too dark, and the
     * port had no writer for it at all, so it was black. */
    {
        static uint8_t dl[64];
        static uint8_t mtx[64];
        static uint8_t verts[0x20];

        memset(mtx, 0, sizeof(mtx));
        wf(mtx + 0, 1.0f); wf(mtx + 5*4, 1.0f);
        wf(mtx + 10*4, 1.0f); wf(mtx + 15*4, 1.0f);
        memset(verts, 0, sizeof(verts));
        wf(verts + 0x1C, 1.0f);                  /* a unit normal */

        memset(dl, 0, sizeof(dl));
        put(dl + 0x00, 0xB7000000u, GEO_LIT);            /* G_SETGEOMETRYMODE */
        put(dl + 0x08, 0xFA000000u, 0x8040C0FFu);        /* prim colour       */
        put(dl + 0x10, 0x01020000u | 0x20000u, 0x50000000u);
        put(dl + 0x18, 0x04000000u | (1u << 10), 0x60000000u);
        put(dl + 0x20, 0xB8000000u, 0u);

        BrDlInit(&st, 640, 480);
        BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
        BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
        BrDlRun(&st, dl, sizeof(dl));

        check(st.nLights == 0 && st.cVtxLitOff == 1,
              "G_LIGHTING with no G_MOVEWORD numlights takes the fallback");
        check(st.aVtx[0].n0 == 128.0f && st.aVtx[0].n1 == 64.0f &&
              st.aVtx[0].n2 == 192.0f,
              "and the fallback colour IS the prim colour -- one object");
    }

    /* 0xF7, 0x1001E9F0.  Not a store: three bitfield merges plus a bit
     * spread.  Low half 0xFAAB is R=31, G=10, B=21, A=1, and the widening is
     * (v << 3) | (v >> 2):
     *     R = 248 | 7 = 255      G = 80 | 2 = 82
     *     B = 168 | 5 = 173      A = 255  (bit 0 spread, not 0/1) */
    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xF7000000u, 0x0000FAABu);
    check(st.fillR == 255 && st.fillG == 82 && st.fillB == 173,
          "0xF7 widens RGBA5551 as (v << 3) | (v >> 2)");
    check(st.fillA == 255, "and spreads bit 0 to 0 or 255, never 0 or 1");

    BrDlInit(&st, 640, 480);
    one_cmd(&st, 0xF7000000u, 0x0000FAAAu);     /* alpha bit clear */
    check(st.fillA == 0, "alpha bit clear gives 0");

    /* A 32-bit fill colour is two RGBA5551 pixels and this build reads one.
     * Every mask in the handler lands inside bits 15:0, so the high half must
     * make no difference at all. */
    {
        BrDl a, b;
        BrDlInit(&a, 640, 480); one_cmd(&a, 0xF7000000u, 0x0000FAABu);
        BrDlInit(&b, 640, 480); one_cmd(&b, 0xF7000000u, 0xFFFFFAABu);
        check(a.fillR == b.fillR && a.fillG == b.fillG &&
              a.fillB == b.fillB && a.fillA == b.fillA,
              "the high halfword of w1 is not read");
    }
}

static void test_quarter_snap(void)
{
    static uint8_t dl[64];
    static uint8_t mtx[64];
    static uint8_t verts[0x20];
    BrDl st;
    int i;

    /* Three viewport translations and the value each must snap to.  The
     * vertex sits at the origin with an identity matrix and the viewport
     * SCALE at zero, so the screen coordinate is exactly the translation and
     * nothing else can move it.
     *
     * 0x100220C6: `fmul 4.0 / fistp [0x105CE310] / fild / fmul 0.25`.  fistp
     * rounds to nearest with TIES TO EVEN under the startup control word, and
     * nothing in 0x10022070 or its callers touches that word.  So a value
     * whose quadrupled form is an exact half goes to the EVEN neighbour:
     *
     *    0.125 -> 0.5  -> 0 -> 0.00      (+/-0.5 and truncate gives 0.25)
     *    0.625 -> 2.5  -> 2 -> 0.50      (...gives 0.75)
     *   -0.125 -> -0.5 -> 0 -> 0.00      (...gives -0.25)
     *    0.200 -> 0.8  -> 1 -> 0.25      the CONTROL: not a tie, both agree
     */
    static const float aIn[4]  = { 0.125f, 0.625f, -0.125f, 0.2f };
    static const float aOut[4] = { 0.00f,  0.50f,   0.00f,  0.25f };

    printf("the quarter-pixel snap rounds ties to EVEN\n");

    memset(mtx, 0, sizeof(mtx));
    wf(mtx + 0, 1.0f); wf(mtx + 5*4, 1.0f);
    wf(mtx + 10*4, 1.0f); wf(mtx + 15*4, 1.0f);
    memset(verts, 0, sizeof(verts));

    memset(dl, 0, sizeof(dl));
    put(dl + 0x00, 0x01020000u | 0x20000u, 0x50000000u);
    put(dl + 0x08, 0x04000000u | (1u << 10), 0x60000000u);
    put(dl + 0x10, 0xB8000000u, 0u);

    for (i = 0; i < 4; ++i) {
        char sz[96];
        BrDlInit(&st, 640, 480);
        BrDlAddRegion(&st, 0x50000000u, mtx, sizeof(mtx));
        BrDlAddRegion(&st, 0x60000000u, verts, sizeof(verts));
        BrDlSetViewport(&st, 0.0f, aIn[i], 0.0f, aIn[i]);
        BrDlRun(&st, dl, sizeof(dl));
        {
            float away = (float)((int32_t)(aIn[i] * 4.0f +
                          (aIn[i] >= 0.0f ? 0.5f : -0.5f))) * 0.25f;
            sprintf(sz, "%.3f snaps to %.2f%s", (double)aIn[i],
                    (double)aOut[i],
                    away == aOut[i] ? "  (not a tie: both roundings agree)"
                                    : " -- ties-away would give another value");
        }
        check(st.cVtxTransformed == 1 &&
              st.aVtx[0].x == aOut[i] && st.aVtx[0].y == aOut[i], sz);
    }
}

int main(void)
{
    test_layout();
    test_table();
    test_advance();
    test_combine();
    test_synthetic();
    test_clip();
    test_light_select();
    test_light_math();
    test_light_cache();
    test_walk_bound();
    test_scissor_ops();
    test_fill_rects();
    test_colours();
    test_quarter_snap();

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
