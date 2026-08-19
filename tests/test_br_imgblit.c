/* test_br_imgblit.c -- Glide 0x1006C990, the full-screen .img blitter.
 *
 * WHAT IS WORTH ASSERTING HERE, and what deliberately is not.
 *
 * The one claim this module makes that could be wrong in a way nothing else
 * would catch is that 0x2AC7E58B is the Adler-32 of splash.img's payload.
 * That is checked against the real file rather than against a constant this
 * test also carries: the sum is COMPUTED by the module and compared to the
 * literal the original pushes.  If the seeding, the length, or the offset of
 * the payload were wrong, the numbers would not meet.
 *
 * The second is the format word's overlap -- it is read into the pixel
 * buffer and then overwritten.  Asserting only "hdr.fmt == 0x1555" would
 * pass for a module that read it into a local; the test also asserts the
 * buffer's first four bytes are the FIRST PIXEL afterwards, which only holds
 * if the second read really lands on top.
 *
 * NOT asserted, because the code cannot express them:
 *   - the `cdq ; sar 1` truncation.  Every value it can differ from `>> 1`
 *     on is negative, and every negative value is then clamped to 0, so the
 *     two divisions are indistinguishable through this function.  Saying so
 *     is better than an assertion that would pass either way.
 *   - the unordered-compare polarity.  The comparand is `(float)(int)`, so
 *     it is never NaN and the true side is unreachable.  The negated form is
 *     still written, because it costs nothing and the next reader of that
 *     line should not have to re-derive it.
 */
#include "br_imgblit.h"
#include "br_testdata.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ==================================================================== *
 * STAND-INS.
 *
 * The file helpers this module wires -- BrChkFReadOpen / BrChkFClose --
 * live in slice6_78.c, and the linker pulls that whole object in.  The rest
 * of it forwards to five other slices whose own closures cascade (slice4_52
 * wants the DirectPlay tags, slice5_63 wants br_data's CD globals, and so
 * on), so linking them would drag most of the tree behind a `fopen`.
 *
 * test_slice6_78.c has the same problem and solves it the way build.sh
 * documents: the test supplies its own stand-ins.  These are copied from
 * there so the two agree about the signatures.  NONE of them is on the path
 * under test -- reaching any is a defect, and the two that terminate in the
 * original terminate here too rather than letting an impossible path run on.
 * ==================================================================== */

uint8_t g_br4B0358;
int     g_br4B0348;

struct BrGfxWords;
void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1);
void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    (void)pOut; (void)a0; (void)b0; (void)c0; (void)d0;
    (void)Aa0; (void)Ab0; (void)Ac0; (void)Ad0;
    (void)a1; (void)b1; (void)c1; (void)d1;
    (void)Aa1; (void)Ab1; (void)Ac1; (void)Ad1;
}

void BrSub_10019260(void);
void BrSub_10019270(void);
void BrSub_100192F0(int size);
void BrSub_10019260(void) { }
void BrSub_10019270(void) { }
void BrSub_100192F0(int size) { (void)size; }

void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6);
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
}

int BrRandom(void);
int BrRandom(void) { return 0; }

void BrSwapU16x4Array(void *pv, int count);
void BrSwapU16x4Array(void *pv, int count) { (void)pv; (void)count; }

/* br_data.c owns these; the shipped value of g_brCdEnabled is 2. */
int g_brCdEnabled  = 2;
int g_brCdPlaying;
int g_brCdTrackCur;

/* ------------------------------------------------------------------ *
 * A recording set of hooks.
 * ------------------------------------------------------------------ */
static char     g_szLog[1024];
static int32_t  g_aTexArgs[BR_IMGBLIT_TEXARGS];
static int      g_cTexArgs;
static int32_t  g_hTexSeen;
static const void *g_pvDownload;
static int32_t  g_aClip[4];
static uint32_t g_aClear[3];
static int32_t  g_aCC[5];
static int32_t  g_aTC[7];
static BrDlVtx  g_aTri[4][3];   /* four triangles: two passes of two       */
static int      g_cTri;

static void logs(const char *psz)
{
    size_t n = strlen(g_szLog);
    snprintf(g_szLog + n, sizeof g_szLog - n, "%s%s", (n != 0) ? "," : "", psz);
}

static void h_reset(void *pU)    { (void)pU; logs("reset"); }

static int32_t h_alloc(void *pU, const int32_t *pa)
{
    int i;
    (void)pU;
    for (i = 0; i < BR_IMGBLIT_TEXARGS; i++) {
        g_aTexArgs[i] = pa[i];
    }
    g_cTexArgs = BR_IMGBLIT_TEXARGS;
    logs("alloc");
    return 0x1234;
}

static void h_download(void *pU, int32_t h, const void *pv, int32_t lvl)
{
    (void)pU; (void)lvl;
    g_hTexSeen = h;
    g_pvDownload = pv;
    logs("download");
}
static void h_source(void *pU, int32_t h) { (void)pU; (void)h; logs("source"); }

static void h_texcombine(void *pU, int32_t a, int32_t b, int32_t c,
                         int32_t d, int32_t e, int32_t f, int32_t g)
{
    (void)pU;
    g_aTC[0] = a; g_aTC[1] = b; g_aTC[2] = c; g_aTC[3] = d;
    g_aTC[4] = e; g_aTC[5] = f; g_aTC[6] = g;
    logs("texcombine");
}
static void h_colcombine(void *pU, int32_t a, int32_t b, int32_t c,
                         int32_t d, int32_t e)
{
    (void)pU;
    g_aCC[0] = a; g_aCC[1] = b; g_aCC[2] = c; g_aCC[3] = d; g_aCC[4] = e;
    logs("colorcombine");
}
static void h_clip(void *pU, int32_t a, int32_t b, int32_t c, int32_t d)
{
    (void)pU;
    g_aClip[0] = a; g_aClip[1] = b; g_aClip[2] = c; g_aClip[3] = d;
    logs("clip");
}
static void h_clear(void *pU, uint32_t a, uint32_t b, uint32_t c)
{
    (void)pU;
    g_aClear[0] = a; g_aClear[1] = b; g_aClear[2] = c;
    logs("clear");
}
static void h_tri(void *pU, const BrDlVtx *pA, const BrDlVtx *pB,
                  const BrDlVtx *pC)
{
    (void)pU;
    if (g_cTri < 4) {
        g_aTri[g_cTri][0] = *pA;
        g_aTri[g_cTri][1] = *pB;
        g_aTri[g_cTri][2] = *pC;
    }
    g_cTri++;
    logs("tri");
}
static void h_present(void *pU) { (void)pU; logs("present"); }

static BrImgBlitOps g_ops;

static void arm(void)
{
    memset(&g_ops, 0, sizeof g_ops);
    g_ops.pfnTexReset      = h_reset;
    g_ops.pfnTexAlloc      = h_alloc;
    g_ops.pfnTexDownload   = h_download;
    g_ops.pfnTexSource     = h_source;
    g_ops.pfnTexCombine    = h_texcombine;
    g_ops.pfnColorCombine  = h_colcombine;
    g_ops.pfnClipWindow    = h_clip;
    g_ops.pfnBufferClear   = h_clear;
    g_ops.pfnDrawTriangle  = h_tri;
    g_ops.pfnPresent       = h_present;
    g_szLog[0] = '\0';
    g_cTri = 0;
    g_cTexArgs = 0;
    g_hTexSeen = 0;
    g_pvDownload = NULL;
    BrImgBlitResetForTest();
}

/* 131072 for the splash; the declared texture wants the same. */
static unsigned char g_abBuf[BR_IMGBLIT_TEX_BYTES];

/* ================================================================== *
 * 1.  0x2AC7E58B is splash.img's Adler-32.  The whole point.
 * ================================================================== */
static void test_checksum(void)
{
    BrImgBlitHdr hdr;
    uint32_t     sum = 0;
    int          rc;

    memset(g_abBuf, 0xCD, sizeof g_abBuf);
    rc = BrImgBlitLoad("testdata/splash.img", BR_IMGBLIT_SPLASH_ADLER,
                       g_abBuf, sizeof g_abBuf, &hdr, &sum);

    CHECK(rc == BR_IMGBLIT_OK);
    /* The LITERAL 0x2AC7E58B off the disassembly at 0x10032553, not the
     * header macro -- comparing a computed value against the same macro
     * the module used would pass whatever the macro said. */
    CHECK(sum == 0x2AC7E58Bu);
    CHECK(BR_IMGBLIT_SPLASH_ADLER == 0x2AC7E58Bu);
    CHECK(hdr.cx == 256);
    CHECK(hdr.cy == 256);
    CHECK(hdr.cbPixels == 256 * 256 * 2);
    /* width*height*2 + the 12-byte header is the whole file, which is what
     * makes "the payload runs to EOF" a statement about the format and not
     * about this one file. */
    CHECK((size_t)hdr.cbPixels + 12u == 131084u);

    /* The format word was seen... */
    CHECK(hdr.fmt == 0x1555u);
    /* ...and the payload landed on top of it, at buffer offset ZERO.  Stated
     * precisely, because the tempting reading is wrong: nothing at this
     * boundary can distinguish "read the format into the buffer" from "read
     * it into a local", since the payload overwrites it either way.  What
     * this DOES catch is a transcription that reads the payload at pb+4 to
     * "keep" the header -- the buffer's first four bytes would then still be
     * 0x00001555 and the picture would be shifted by two pixels. */
    CHECK(!(g_abBuf[0] == 0x55 && g_abBuf[1] == 0x15 &&
            g_abBuf[2] == 0x00 && g_abBuf[3] == 0x00));

    /* A wrong expectation must be rejected, and the computed sum must still
     * be reported -- the module does not stop computing when it disagrees. */
    sum = 0;
    rc = BrImgBlitLoad("testdata/splash.img", BR_IMGBLIT_SPLASH_ADLER ^ 1u,
                       g_abBuf, sizeof g_abBuf, &hdr, &sum);
    CHECK(rc == BR_IMGBLIT_BADSUM);
    CHECK(sum == BR_IMGBLIT_SPLASH_ADLER);

    /* Zero means "do not check", which is what state 3 passes.  It must not
     * mean "check against zero". */
    rc = BrImgBlitLoad("testdata/splash.img", 0u,
                       g_abBuf, sizeof g_abBuf, &hdr, &sum);
    CHECK(rc == BR_IMGBLIT_OK);
}

static void test_loading_img(void)
{
    BrImgBlitHdr hdr;
    uint32_t     sum = 0;
    int          rc;

    rc = BrImgBlitLoad("testdata/loading.img", 0u,
                       g_abBuf, sizeof g_abBuf, &hdr, &sum);
    CHECK(rc == BR_IMGBLIT_OK);
    CHECK(hdr.cx == 256);
    CHECK(hdr.cy == 200);
    CHECK(hdr.cbPixels == 256 * 200 * 2);
    CHECK(hdr.fmt == 0x1555u);

    /* The other file's sum, so the routine is not accidentally specific to
     * the splash.  State 3 passes 0 and never exercises this. */
    rc = BrImgBlitLoad("testdata/loading.img", 0x440E8E2Cu,
                       g_abBuf, sizeof g_abBuf, &hdr, &sum);
    CHECK(rc == BR_IMGBLIT_OK);
    CHECK(sum == 0x440E8E2Cu);
    CHECK(BR_IMGBLIT_LOADING_ADLER == 0x440E8E2Cu);

    /* The declared texture is 256x256 whatever the file is, so the download
     * over-reads by this much on the loading screen.  Recorded as an
     * assertion so the number cannot drift unnoticed. */
    CHECK(BR_IMGBLIT_TEX_BYTES - hdr.cbPixels == 28672);
}

/* ================================================================== *
 * 2.  Placement.  Half the arithmetic uses 256 and half uses the file.
 * ================================================================== */
static void test_place(void)
{
    float x0, y0, x1, y1;

    /* The splash at the mode br_boot.h installs. */
    BrImgBlitPlace(256, 256, 640, 480, &x0, &y0, &x1, &y1);
    CHECK(x0 == 192.0f);
    CHECK(y0 == 112.0f);
    CHECK(x1 == 447.0f);
    CHECK(y1 == 367.0f);
    /* A square image on a 640x480 screen IS centred, which is why the
     * loading screen's asymmetry below looks like a bug and is not. */
    CHECK((x0 + x1 + 1.0f) / 2.0f == 320.0f);
    CHECK((y0 + y1 + 1.0f) / 2.0f == 240.0f);

    /* The loading screen: 256x200.  y0 still comes from the literal 256, so
     * the quad is 200 tall starting at 112 and its centre is 212, not 240.
     * Centring on the file would have given y0 = 140. */
    BrImgBlitPlace(256, 200, 640, 480, &x0, &y0, &x1, &y1);
    CHECK(y0 == 112.0f);
    CHECK(y1 == 311.0f);
    CHECK((y0 + y1 + 1.0f) / 2.0f == 212.0f);
    CHECK(y0 != 140.0f);

    /* The near corner is centred on the LITERAL 256, in BOTH axes.  Every
     * shipped .img is 256 wide, so an x centred on the file's own width
     * would be indistinguishable on real data -- this is the synthetic case
     * that separates them, and it exists because mutation testing showed the
     * assertion above could not fail on that substitution. */
    BrImgBlitPlace(200, 120, 640, 480, &x0, &y0, &x1, &y1);
    CHECK(x0 == 192.0f);            /* 220.0f if it centred on cx == 200 */
    CHECK(y0 == 112.0f);            /* 180.0f if it centred on cy == 120 */
    CHECK(x1 == 391.0f);            /* the FAR corner does use the file  */
    CHECK(y1 == 231.0f);

    /* The clamp.  A mode narrower or shorter than 256 would put the near
     * corner off-screen; the original pins it at zero and lets the far
     * corner run past the edge. */
    BrImgBlitPlace(256, 256, 200, 100, &x0, &y0, &x1, &y1);
    CHECK(x0 == 0.0f);
    CHECK(y0 == 0.0f);
    CHECK(x1 == 255.0f);
    CHECK(y1 == 255.0f);

    /* Exactly 256 is the boundary and must NOT be clamped away from its own
     * value -- 0 is both the computed and the clamped answer here, so the
     * assertion that bites is the one just above. */
    BrImgBlitPlace(256, 256, 256, 256, &x0, &y0, &x1, &y1);
    CHECK(x0 == 0.0f);
    CHECK(x1 == 255.0f);
}

/* ================================================================== *
 * 3.  The quad.  Invariants rather than a transcription of the table.
 * ================================================================== */
static void test_quad(void)
{
    BrDlVtx av[4];
    int     i;
    float   x0 = 192.0f, y0 = 112.0f, x1 = 447.0f, y1 = 311.0f;
    int     seen[4];

    memset(av, 0, sizeof av);
    BrImgBlitQuad(av, 256, 200, x0, y0, x1, y1);

    memset(seen, 0, sizeof seen);
    for (i = 0; i < 4; i++) {
        int fRight = (av[i].x == x1);
        int fTop   = (av[i].y == y0);

        /* Each vertex sits on one of the two x and one of the two y. */
        CHECK(av[i].x == x0 || av[i].x == x1);
        CHECK(av[i].y == y0 || av[i].y == y1);

        /* s tracks x: the right edge samples column cx-1. */
        CHECK(av[i].tmu0[0] == (fRight ? 255.0f : 0.0f));
        /* t runs OPPOSITE to y: the TOP of the quad samples row cy-1.  This
         * is the one relationship a plausible-looking transcription gets
         * backwards, and it is the reason the pair is asserted per vertex
         * rather than as four literal rows. */
        CHECK(av[i].tmu0[1] == (fTop ? 199.0f : 0.0f));

        CHECK(av[i].r == 255.0f && av[i].g == 255.0f && av[i].b == 255.0f);
        CHECK(av[i].a == 255.0f);
        CHECK(av[i].oow == 1.0f);

        seen[(fRight ? 1 : 0) + (fTop ? 2 : 0)] = 1;
    }
    /* All four corners are present exactly once -- no duplicate, no missing
     * corner, which a swapped pair of stores would produce. */
    CHECK(seen[0] && seen[1] && seen[2] && seen[3]);

    /* And the original's own slot order, because the two grDrawTriangle
     * calls index these positions directly. */
    CHECK(av[0].x == x1 && av[0].y == y1);
    CHECK(av[1].x == x0 && av[1].y == y0);
    CHECK(av[2].x == x0 && av[2].y == y1);
    CHECK(av[3].x == x1 && av[3].y == y0);

    /* The fields the original never writes stay untouched. */
    CHECK(av[0].z == 0.0f && av[0].ooz == 0.0f);
    CHECK(av[0].tmu1[0] == 0.0f);

    /* A one-pixel image gives extents of 0, not -1 wrapped: the `dec` is on
     * a signed int and the `fild` that follows is signed too. */
    BrImgBlitQuad(av, 1, 1, 0.0f, 0.0f, 0.0f, 0.0f);
    CHECK(av[3].tmu0[0] == 0.0f && av[3].tmu0[1] == 0.0f);
}

/* ================================================================== *
 * 4.  The whole function, against the real splash.
 * ================================================================== */
static void test_fullscreen(void)
{
    static const char szExpect[] =
        "reset,alloc,download,source,texcombine,colorcombine,"
        "clip,clear,tri,tri,present,"
        "clip,clear,tri,tri,present,"
        "reset";
    int i;

    arm();
    BrImgBlitFullScreen("testdata/splash.img", BR_IMGBLIT_SPLASH_ADLER,
                        g_abBuf, sizeof g_abBuf, 640, 480, &g_ops);

    /* The order, the double pass, and both texture-manager wipes in one
     * comparison.  Moving any call, or dropping either `reset`, changes it. */
    CHECK(strcmp(g_szLog, szExpect) == 0);
    if (strcmp(g_szLog, szExpect) != 0) {
        printf("     log was: %s\n", g_szLog);
    }
    CHECK(g_cTri == 4);

    /* The handle the allocator returned is the one downloaded to. */
    CHECK(g_hTexSeen == 0x1234);
    CHECK(g_pvDownload == (const void *)g_abBuf);

    /* The fifteen literals, and the four GrTexInfo fields inside them. */
    CHECK(g_cTexArgs == BR_IMGBLIT_TEXARGS);
    CHECK(g_aTexArgs[BR_IMGBLIT_TEXARG_FORMAT]   == 0xB);  /* ARGB_1555   */
    CHECK(g_aTexArgs[BR_IMGBLIT_TEXARG_ASPECT]   == 3);    /* 1x1         */
    CHECK(g_aTexArgs[BR_IMGBLIT_TEXARG_SMALLLOD] == 0);
    CHECK(g_aTexArgs[BR_IMGBLIT_TEXARG_LARGELOD] == 0);
    CHECK(g_aTexArgs[BR_IMGBLIT_TEXARG_EVENODD]  == 3);
    CHECK(g_aTexArgs[2] == 0x80 && g_aTexArgs[3] == 0x20);
    CHECK(g_aTexArgs[5] == 2);

    /* grColorCombine(SCALE_OTHER, ONE, ITERATED, TEXTURE, 0). */
    CHECK(g_aCC[0] == 3 && g_aCC[1] == 8 && g_aCC[2] == 1 &&
          g_aCC[3] == 1 && g_aCC[4] == 0);
    CHECK(g_aTC[0] == 0 && g_aTC[1] == 1 && g_aTC[2] == 0 && g_aTC[3] == 1);

    /* The clip window is the whole mode, and the clear takes depth 0xFFFF
     * and not 0 -- a Glide depth clear of 0 would be the near plane. */
    CHECK(g_aClip[0] == 0 && g_aClip[1] == 0 &&
          g_aClip[2] == 640 && g_aClip[3] == 480);
    CHECK(g_aClear[2] == 0xFFFFu);

    /* The two passes must draw IDENTICAL geometry -- that is what makes the
     * splash survive the flip.  Comparing pass 2 against pass 1 also
     * cross-checks the second block's different stack displacements. */
    for (i = 0; i < 3; i++) {
        CHECK(g_aTri[2][i].x == g_aTri[0][i].x);
        CHECK(g_aTri[2][i].y == g_aTri[0][i].y);
        CHECK(g_aTri[3][i].x == g_aTri[1][i].x);
        CHECK(g_aTri[3][i].y == g_aTri[1][i].y);
    }

    /* The two triangles share the edge (v0, v1) and between them name all
     * four corners -- i.e. they tile the quad rather than overlap it. */
    CHECK(g_aTri[0][1].x == g_aTri[1][0].x && g_aTri[0][1].y == g_aTri[1][0].y);
    CHECK(g_aTri[0][2].x == g_aTri[1][2].x && g_aTri[0][2].y == g_aTri[1][2].y);
    CHECK(g_aTri[0][0].x == 192.0f && g_aTri[0][0].y == 367.0f); /* v2 */
    CHECK(g_aTri[1][1].x == 447.0f && g_aTri[1][1].y == 112.0f); /* v3 */

    /* Nothing was skipped in a fully wired run. */
    for (i = 0; i < BR_IMGBLIT_NSTEPS; i++) {
        CHECK(BrImgBlitSkipped((BrImgBlitStep)i) == 0);
    }
}

/* ================================================================== *
 * 5.  An unwired run counts every call site and invents nothing.
 * ================================================================== */
static void test_unwired(void)
{
    BrImgBlitOps ops;

    memset(&ops, 0, sizeof ops);
    BrImgBlitResetForTest();
    BrImgBlitFullScreen("testdata/splash.img", BR_IMGBLIT_SPLASH_ADLER,
                        g_abBuf, sizeof g_abBuf, 640, 480, &ops);

    /* The counts ARE the call structure: two wipes, one of each setup call,
     * two clip/clear/present (one per buffer) and four triangles. */
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_TEXRESET)     == 2);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_TEXALLOC)     == 1);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_TEXDOWNLOAD)  == 1);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_TEXSOURCE)    == 1);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_TEXCOMBINE)   == 1);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_COLORCOMBINE) == 1);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_CLIPWINDOW)   == 2);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_BUFFERCLEAR)  == 2);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_DRAWTRIANGLE) == 4);
    CHECK(BrImgBlitSkipped(BR_IMGBLIT_PRESENT)      == 2);
}

/* ================================================================== *
 * 6.  The buffer bound.  DEVIATION territory -- the original has none.
 * ================================================================== */
static void test_bound(void)
{
    BrImgBlitHdr hdr;
    int          rc;

    /* loading.img needs 102400; a 4096-byte buffer must be refused rather
     * than written past.  The original would have written 102400 bytes. */
    rc = BrImgBlitLoad("testdata/loading.img", 0u, g_abBuf, 4096u,
                       &hdr, NULL);
    CHECK(rc == BR_IMGBLIT_TOOBIG);
    /* The header is still reported, because it was read before the refusal. */
    CHECK(hdr.cx == 256 && hdr.cy == 200);
}

int main(void)
{
    BR_REQUIRE_TESTDATA("testdata/splash.img", "test_br_imgblit");
    BR_REQUIRE_TESTDATA("testdata/loading.img", "test_br_imgblit");

    test_checksum();
    test_loading_img();
    test_place();
    test_quad();
    test_fullscreen();
    test_unwired();
    test_bound();

    /* tools/regress.sh matches the COUNT, never the word, so the last line
     * carries it in both cases. */
    printf("test_br_imgblit: %d failures\n", g_fails);
    return (g_fails == 0) ? 0 : 1;
}
