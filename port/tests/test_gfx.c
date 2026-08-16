/* test_gfx.c -- render real game artwork through the Metal backend, headless.
 *
 * Part 1 loads a retail .img, uploads it as a texture, draws it into an
 * offscreen target, reads it back and writes a PPM. Verifying the port by
 * looking at what it actually draws is the whole point of dropping
 * byte-matching.
 *
 * Part 2 does the same for GEOMETRY: it walks testdata/ce.rca's display
 * lists through br_dl.c and rasterises them TWICE from identical
 * interpreter state -- once through br_dl.c's own reference rasteriser and
 * once through the Metal pipeline-state backend in br_gfx3d.h -- and
 * compares the two images.
 *
 * WHY THE COMPARISON IS THE TEST AND THE PIXEL COUNT IS NOT: coverage counts
 * cannot tell a model from a smear (CONVENTIONS.md, "Tests"), and neither
 * rasteriser is the ground truth for the other in an absolute sense.  What
 * IS assertable is that two independent rasterisers fed the same vertices
 * agree: same triangle count, near-identical silhouette, and -- once the
 * z-buffer the software path does not have is switched off -- near-identical
 * colour.  A disagreement means one of them is wrong about the geometry,
 * which is exactly the failure this is here to catch.
 */
#include "br_img.h"
#include "br_testdata.h"
#include "br_gfx.h"
#include "br_gfx3d.h"
#include "br_dlscene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *pszWhat)
{
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", pszWhat);
    if (!cond)
        g_fail = 1;
}

static int write_ppm(const char *pszPath, const uint8_t *pRgba,
                     uint32_t w, uint32_t h)
{
    FILE *f = fopen(pszPath, "wb");
    uint32_t i;
    if (f == NULL)
        return 1;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (i = 0; i < w * h; i++)
        fwrite(pRgba + i * 4, 1, 3, f);
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Part 2: retail geometry, software vs Metal                         */
/* ------------------------------------------------------------------ */

#define GEO_W 256
#define GEO_H 256

static const char *g_aCombineName[BR_DL_CC__COUNT] = {
    "DEFAULT(tex*shade)", "SHADE[=texel]", "TEX[=shade]", "TEX_SHADE_C1",
    "TEX_SHADE_A", "TEX_SHADE_B", "TEX_SHADE_CW", "ENVMAP", "DECAL",
    "TEX_SHADE_C0"
};
static const char *g_aBlendName[BR_GFX3D_BLEND__COUNT] =
    { "opaque", "alpha", "add" };
static const char *g_aZName[BR_GFX3D_Z__COUNT] =
    { "less", "equal", "lequal", "always" };

/* Set up the interpreter for one render.  Called twice with the same
 * arguments, so the two sinks see byte-identical vertex streams. */
static float geo_env(const char *pszName, float dflt)
{
    const char *p = getenv(pszName);
    return (p != NULL && *p) ? (float)atof(p) : dflt;
}

static void geo_setup(BrDl *pDl, const BrDlScene *pScene)
{
    /* The view is scaffolding (these lists carry no G_MTX -- see
     * br_dlscene.h), so it is adjustable from the environment rather than
     * being a magic constant nobody can question:
     *     BR_GEO_YAW  BR_GEO_PITCH  BR_GEO_FOV */
    BrDlInit(pDl, GEO_W, GEO_H);
    BrDlSceneBind(pScene, pDl);
    BrDlSceneView(pScene, pDl,
                  geo_env("BR_GEO_YAW", 30.0f),
                  geo_env("BR_GEO_PITCH", 15.0f),
                  geo_env("BR_GEO_FOV", 40.0f), GEO_W, GEO_H);
}

static void thumb(const char *pszWhat, const uint8_t *pRgba)
{
    int ty, tx;
    printf("    %s\n", pszWhat);
    for (ty = 0; ty < 24; ty++) {
        char row[65];
        for (tx = 0; tx < 64; tx++) {
            int sy = ty * (GEO_H / 24), sx = tx * (GEO_W / 64), k, l, lit = 0;
            for (k = 0; k < GEO_H / 24 && !lit; k++)
                for (l = 0; l < GEO_W / 64 && !lit; l++) {
                    int px = sx + l, py = sy + k;
                    if (px < GEO_W && py < GEO_H &&
                        pRgba[((size_t)py * GEO_W + (size_t)px) * 4 + 3] > 127)
                        lit = 1;
                }
            row[tx] = (char)(lit ? '#' : '.');
        }
        row[64] = '\0';
        printf("    %s\n", row);
    }
}

static void test_geometry(const char *pszRca)
{
    BrDlScene scene;
    BrDl st;
    BrDlRaster ras;
    BrGfx *gfx;
    uint8_t *pSoft, *pHard, *pDepth;
    uint32_t softTri, hardTri, i, both = 0, either = 0, softOnly = 0, hardOnly = 0;
    double diff = 0.0;
    const BrGfx3dStats *pStats;

    printf("geometry: %s\n", pszRca);
    if (BrDlSceneLoad(&scene, pszRca) != 0) {
        check(0, "BrDlSceneLoad");
        return;
    }
    printf("    runs=%u verts=%u bbox=[%.0f %.0f %.0f]..[%.0f %.0f %.0f]\n",
           (unsigned)scene.cRuns, scene.cVerts,
           scene.bbMin[0], scene.bbMin[1], scene.bbMin[2],
           scene.bbMax[0], scene.bbMax[1], scene.bbMax[2]);
    check(scene.cRuns > 0 && scene.cVerts > 0, "scene loaded");
    check(scene.fEndOk, "every display-list run terminates at G_ENDDL");

    pSoft  = (uint8_t *)calloc((size_t)GEO_W * GEO_H * 4, 1);
    pHard  = (uint8_t *)calloc((size_t)GEO_W * GEO_H * 4, 1);
    pDepth = (uint8_t *)calloc((size_t)GEO_W * GEO_H * 4, 1);
    if (pSoft == NULL || pHard == NULL || pDepth == NULL) {
        check(0, "alloc");
        free(pSoft); free(pHard); free(pDepth);
        BrDlSceneFree(&scene);
        return;
    }

    /* --- 1. br_dl.c's reference rasteriser ------------------------- */
    geo_setup(&st, &scene);
    ras.pRgba = pSoft; ras.cx = GEO_W; ras.cy = GEO_H; ras.cCovered = 0;
    BrDlAttachRaster(&st, &ras);
    BrDlSceneRun(&scene, &st);
    softTri = st.cTriDrawn;
    printf("    software: tri=%u rejected=%u clipped=%u px=%u\n",
           st.cTriDrawn, st.cTriRejected, st.cTriClipped, ras.cCovered);
    check(st.cTriIn == st.cTriDrawn + st.cTriRejected + st.cTriClipped,
          "every triangle drawn, rejected or clipped -- none lost");

    gfx = BrGfxCreate(GEO_W, GEO_H);
    if (gfx == NULL) {
        printf("  [FAIL] BrGfxCreate: %s\n", BrGfxLastError());
        g_fail = 1;
        free(pSoft); free(pHard); free(pDepth);
        BrDlSceneFree(&scene);
        return;
    }
    check(BrGfx3dInit(gfx) == 0, "BrGfx3dInit: 30 pipeline + 8 depth states");

    /* --- 2. Metal, depth test off, to match the software path ------ */
    geo_setup(&st, &scene);
    BrGfx3dAttach(gfx, &st);
    BrGfx3dSetDepthTest(gfx, 0);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlSceneRun(&scene, &st);
    BrGfx3dEndFrame(gfx);
    check(BrGfxReadPixels(gfx, pHard) == 0, "BrGfxReadPixels (Metal geometry)");
    pStats = BrGfx3dGetStats(gfx);
    hardTri = pStats->cTri;
    printf("    metal:    tri=%u verts=%u draws=%u statechanges=%u "
           "combines=%u modes=%u rects=%u binds=%u\n",
           pStats->cTri, pStats->cVerts, pStats->cDraws,
           pStats->cStateChanges, pStats->cCombineCmds, pStats->cModeCmds,
           pStats->cRects, pStats->cBinds);
    for (i = 0; i < BR_DL_CC__COUNT; i++)
        if (pStats->aCombineUse[i])
            printf("      combiner %-20s %u tri\n",
                   g_aCombineName[i], pStats->aCombineUse[i]);
    for (i = 0; i < BR_GFX3D_BLEND__COUNT; i++)
        if (pStats->aBlendUse[i])
            printf("      blend    %-20s %u tri\n",
                   g_aBlendName[i], pStats->aBlendUse[i]);
    for (i = 0; i < BR_GFX3D_Z__COUNT; i++)
        if (pStats->aZUse[i])
            printf("      depth    %-20s %u tri\n",
                   g_aZName[i], pStats->aZUse[i]);
    /* The words the DATA carries, against the words 0x1001E7A0 and
     * 0x10021270 can recognise.  Printed, not asserted: which subset a given
     * .rca exercises is a property of the asset, and pinning it would be
     * exactly the kind of expectation CONVENTIONS.md warns about. */
    printf("      G_SETCOMBINE words present in this file:\n");
    for (i = 0; i < pStats->cSeenCombine; i++)
        printf("        %08X %08X -> %s\n",
               pStats->aSeenCombine[i][0], pStats->aSeenCombine[i][1],
               g_aCombineName[BrDlClassifyCombine(pStats->aSeenCombine[i][0],
                                                  pStats->aSeenCombine[i][1])]);
    printf("      render-mode words present (%u unrecognised of %u):\n",
           pStats->cModeUnrecognised, pStats->cModeCmds);
    for (i = 0; i < pStats->cSeenMode; i++)
        printf("        %08X\n", pStats->aSeenMode[i]);

    check(hardTri == softTri,
          "Metal and software received the same triangle count");

    /* --- 3. Metal with the z-buffer on: the honest render ---------- */
    geo_setup(&st, &scene);
    BrGfx3dAttach(gfx, &st);
    BrGfx3dSetDepthTest(gfx, 1);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlSceneRun(&scene, &st);
    BrGfx3dEndFrame(gfx);
    BrGfxReadPixels(gfx, pDepth);

    /* --- 4. compare ------------------------------------------------ */
    for (i = 0; i < (uint32_t)(GEO_W * GEO_H); i++) {
        int s = pSoft[i * 4 + 3] > 127, h = pHard[i * 4 + 3] > 127;
        if (s && h) {
            int k;
            both++;
            for (k = 0; k < 3; k++)
                diff += (double)abs((int)pSoft[i * 4 + k] - (int)pHard[i * 4 + k]);
        }
        if (s || h) either++;
        if (s && !h) softOnly++;
        if (h && !s) hardOnly++;
    }
    printf("    silhouette: union=%u intersection=%u soft-only=%u metal-only=%u"
           "  IoU=%.4f\n", either, both, softOnly, hardOnly,
           either ? (double)both / (double)either : 0.0);
    printf("    mean |RGB| difference over the intersection: %.3f / 255\n",
           both ? diff / (both * 3.0) : 0.0);

    thumb("software (br_dl.c reference rasteriser)", pSoft);
    thumb("Metal (depth test off, to match)", pHard);
    thumb("Metal (depth test on -- the real render)", pDepth);

    check(either > 2000, "the model covers a substantial area");
    /* Two independent rasterisers over the same vertices must agree on the
     * silhouette to within their fill-rule difference, which is a boundary
     * effect: it scales with the PERIMETER, not the area. */
    check(either > 0 && (double)both / (double)either > 0.95,
          "silhouettes agree (IoU > 0.95)");
    check(both > 0 && diff / (both * 3.0) < 4.0,
          "interior colours agree to within 4/255 on average");

    {
        char szOut[256];
        const char *pszTag = strrchr(pszRca, '/');
        pszTag = pszTag ? pszTag + 1 : pszRca;
        snprintf(szOut, sizeof(szOut), "build/gfx_%.4s_soft.ppm", pszTag);
        write_ppm(szOut, pSoft, GEO_W, GEO_H);
        snprintf(szOut, sizeof(szOut), "build/gfx_%.4s_metal.ppm", pszTag);
        write_ppm(szOut, pHard, GEO_W, GEO_H);
        snprintf(szOut, sizeof(szOut), "build/gfx_%.4s_metal_z.ppm", pszTag);
        write_ppm(szOut, pDepth, GEO_W, GEO_H);
        printf("    -> build/gfx_%.4s_{soft,metal,metal_z}.ppm\n", pszTag);
    }

    free(pSoft); free(pHard); free(pDepth);
    BrGfxDestroy(gfx);
    BrDlSceneFree(&scene);
}

/* ------------------------------------------------------------------ */
/* Part 3: every combiner row, exercised                              */
/*                                                                    */
/* The retail car models turn out to use exactly ONE of the ten rows   */
/* (see the report printed above), so nothing in part 2 can tell a     */
/* correctly wired pipeline array from one whose specialisations are   */
/* off by one -- every triangle would go through the same slot either  */
/* way. This draws one triangle per row into its own screen strip and  */
/* reads back the colour each row is defined to produce.               */
/* ------------------------------------------------------------------ */

#define CC_W 320
#define CC_H 64

static void put8(uint8_t *p, uint32_t w0, uint32_t w1)
{
    p[0] = (uint8_t)w0; p[1] = (uint8_t)(w0 >> 8);
    p[2] = (uint8_t)(w0 >> 16); p[3] = (uint8_t)(w0 >> 24);
    p[4] = (uint8_t)w1; p[5] = (uint8_t)(w1 >> 8);
    p[6] = (uint8_t)(w1 >> 16); p[7] = (uint8_t)(w1 >> 24);
}

static void test_combiner_rows(void)
{
    /* The ten pairs 0x1001E7A0 compares against, in its own order.  The
     * DECAL row's second accepted w1 is not repeated: it selects the same
     * configuration. */
    static const uint32_t aPair[BR_DL_CC__COUNT][2] = {
        { 0xFC000000u, 0x00000000u },   /* DEFAULT: deliberately no match  */
        { 0xFCFFFFFFu, 0xFFFCF87Cu },
        { 0xFCFFFFFFu, 0xFFFE793Cu },
        { 0xFC567EACu, 0xFFFFF3F9u },
        { 0xFCFF97FFu, 0xFF2DFEFFu },
        { 0xFCFFFFFFu, 0xFFFDF2F9u },
        { 0xFCFFFFFFu, 0xFFFF73B9u },
        { 0xFC127E08u, 0xF3FFF2F8u },
        { 0xFC317E02u, 0x5FFEF3FAu },
        { 0xFC127FFFu, 0xFFFFF838u }
    };
    /* What each row is DEFINED to produce here.  The unbound texture is a
     * 1x1 white texel, BrDlInit leaves the primitive colour white, and the
     * vertices below carry colour 0, which br_dl.c's [-1,1] -> [0,1] fold
     * turns into mid grey.  So:
     *   modulate rows  -> 1.0 * 0.5 = grey     (DEFAULT, DECAL)
     *   texture row    -> white
     *   shade row      -> grey
     *   constant rows  -> whatever that row's grConstantColorValue set  */
    static const int aExpect[BR_DL_CC__COUNT] =
        { 128, 255, 128, 0, 255, 255, 255, 255, 128, 0 };

    static uint8_t dl[8 * (2 + 2 * BR_DL_CC__COUNT)];
    static float arena[BR_DL_CC__COUNT * 3 * 8];
    BrDl st;
    BrGfx *gfx;
    uint8_t *px;
    int i, fColour = 1;
    const BrGfx3dStats *pStats;

    printf("combiner rows (synthetic)\n");

    for (i = 0; i < BR_DL_CC__COUNT; i++) {
        float x0 = -1.0f + 2.0f * (float)i / (float)BR_DL_CC__COUNT;
        float x1 = -1.0f + 2.0f * (float)(i + 1) / (float)BR_DL_CC__COUNT;
        float ax[3], ay[3];
        int k;
        ax[0] = x0; ay[0] = -0.9f;
        ax[1] = x1; ay[1] = -0.9f;
        ax[2] = x0; ay[2] =  0.9f;
        for (k = 0; k < 3; k++) {
            float *o = arena + (i * 3 + k) * 8;
            memset(o, 0, 8 * sizeof(float));
            o[0] = ax[k]; o[1] = ay[k]; o[2] = 0.0f;
            /* o[5..7] are the Vtx's trailing bytes; zero folds to mid grey */
        }
    }

    memset(dl, 0, sizeof(dl));
    /* G_VTX: n in bits[15:10], v0 in bits[23:16]. */
    put8(dl, 0x04000000u | ((uint32_t)(BR_DL_CC__COUNT * 3) << 10),
         BR_DLSCENE_ARENA_BASE);
    for (i = 0; i < BR_DL_CC__COUNT; i++) {
        put8(dl + 8 + i * 16, aPair[i][0], aPair[i][1]);
        put8(dl + 16 + i * 16, 0xBF000000u,
             ((uint32_t)(i * 3) << 16) | ((uint32_t)(i * 3 + 1) << 8) |
             (uint32_t)(i * 3 + 2));
    }
    put8(dl + 8 + BR_DL_CC__COUNT * 16, 0xB8000000u, 0u);

    gfx = BrGfxCreate(CC_W, CC_H);
    if (gfx == NULL) { check(0, "BrGfxCreate"); return; }

    /* BrDlInit already leaves `combined` identity and the viewport mapping
     * NDC onto the target, so clip space IS model space and w == 1. */
    BrDlInit(&st, CC_W, CC_H);
    BrDlAddRegion(&st, BR_DLSCENE_ARENA_BASE, arena, sizeof(arena));
    BrGfx3dAttach(gfx, &st);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlRun(&st, dl, sizeof(dl));
    BrGfx3dEndFrame(gfx);

    px = (uint8_t *)malloc((size_t)CC_W * CC_H * 4);
    if (px == NULL || BrGfxReadPixels(gfx, px) != 0) {
        check(0, "BrGfxReadPixels");
        free(px); BrGfxDestroy(gfx); return;
    }

    pStats = BrGfx3dGetStats(gfx);
    check(pStats->cTri == BR_DL_CC__COUNT, "ten triangles reached the sink");
    /* Every row is a different pipeline state, so every row must flush. */
    check(pStats->cDraws == BR_DL_CC__COUNT,
          "each combiner row forced its own draw call");

    for (i = 0; i < BR_DL_CC__COUNT; i++) {
        /* Inside the right triangle (x0,-0.9)-(x1,-0.9)-(x0,0.9): at the
         * vertical centre the interior runs from x0 to the strip midpoint. */
        int sx = (int)((-1.0f + 2.0f * ((float)i + 0.25f) /
                        (float)BR_DL_CC__COUNT + 1.0f) * 0.5f * CC_W);
        int sy = CC_H / 2;
        const uint8_t *q = px + ((size_t)sy * CC_W + (size_t)sx) * 4;
        int got = q[0];
        printf("      row %-20s -> %3d,%3d,%3d a=%3d (expected %3d)\n",
               g_aCombineName[i], q[0], q[1], q[2], q[3], aExpect[i]);
        if (abs(got - aExpect[i]) > 2 || q[3] < 250)
            fColour = 0;
    }
    check(fColour, "every combiner row produced the colour its Glide "
                   "argument tuple defines");

    free(px);
    BrGfxDestroy(gfx);
}

int main(int argc, char **argv)
{
    BR_REQUIRE_TESTDATA("testdata/splash.img", "gfx");
    const char *pszImg = (argc > 1) ? argv[1] : "testdata/splash.img";
    const char *pszOut = (argc > 2) ? argv[2] : "build/gfx_out.ppm";
    BrImage img;
    BrGfx *gfx;
    BrTexture tex;
    uint8_t *pOut;
    uint32_t W = 512, H = 384;
    uint32_t opaque = 0, i;

    printf("loading %s\n", pszImg);
    check(BrImgLoad(&img, pszImg) == 0, "BrImgLoad");
    if (g_fail)
        return 1;
    printf("  %ux%u\n", img.width, img.height);
    check(img.width > 0 && img.height > 0, "image has dimensions");

    /* Alpha must be strictly binary (these are 1-bit-alpha formats) and there
     * must be real opaque content. We cannot assert "mostly opaque": splash.img
     * is a transparency-keyed overlay at ~15% coverage, while loading.img is a
     * fully opaque background. Picking the wrong channel order collapses alpha
     * to all-0 or all-1, which both of these catch. */
    {
        uint32_t total = img.width * img.height, binary = 0;
        for (i = 0; i < total; i++) {
            uint8_t a = img.pixels[i * 4 + 3];
            if (a == 0 || a == 255) binary++;
            if (a == 255) opaque++;
        }
        printf("  opaque pixels: %u / %u\n", opaque, total);
        check(binary == total, "alpha is strictly binary");
        check(opaque > total / 20, "image has real opaque content");
        check(opaque > 0 && opaque <= total, "alpha not collapsed to a constant");
    }

    gfx = BrGfxCreate(W, H);
    if (gfx == NULL) {
        printf("  [FAIL] BrGfxCreate: %s\n", BrGfxLastError());
        return 1;
    }
    check(1, "BrGfxCreate (Metal device + runtime-compiled shaders)");

    tex = BrGfxCreateTexture(gfx, img.width, img.height, img.pixels);
    check(tex != 0, "BrGfxCreateTexture");

    BrGfxBeginFrame(gfx, 0.10f, 0.10f, 0.12f, 1.0f);
    BrGfxDrawTexture(gfx, tex, 16.0f, 16.0f, (float)img.width, (float)img.height);
    /* draw again, scaled, to exercise the sampler */
    BrGfxDrawTexture(gfx, tex, 300.0f, 200.0f, 128.0f, 128.0f);
    BrGfxEndFrame(gfx);

    pOut = (uint8_t *)malloc((size_t)W * H * 4);
    check(BrGfxReadPixels(gfx, pOut) == 0, "BrGfxReadPixels");

    /* the cleared background must survive where nothing was drawn */
    check(pOut[(H - 1) * W * 4 + 4] < 64, "clear colour present in empty area");

    check(write_ppm(pszOut, pOut, W, H) == 0, "wrote render output");
    printf("  -> %s\n", pszOut);

    free(pOut);
    BrGfxDestroy(gfx);
    BrImgFree(&img);

    test_combiner_rows();

    {
        FILE *probe = fopen("testdata/ce.rca", "rb");
        if (probe == NULL) {
            printf("  (testdata/ce.rca absent -- geometry section skipped)\n");
        } else {
            fclose(probe);
            test_geometry("testdata/ce.rca");
            /* A second retail model, because one asset cannot show which
             * part of the state model the DATA actually uses. */
            test_geometry("testdata/bb.rca");
        }
    }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
