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
#include "br_trkscene.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Stand-ins for slice2_17.o's other dependencies.                     */
/*                                                                    */
/* The track camera needs exactly ONE function from slice2_17.c --     */
/* BrMat4LookAt, the game's guLookAtF (0x100309A0) -- and linking the  */
/* object that holds it drags in nineteen cross-slice externs that     */
/* belong to the car table, the scratch ring and the frame draw, none  */
/* of which this test can reach.  build.sh's header records that a     */
/* test supplying its own stand-ins is the intended way out; these are */
/* the same nineteen test_slice2_17.c already defines, reduced to      */
/* nothing because nothing here calls them.  If one is ever REACHED    */
/* the count below is non-zero and the test says so.                   */
/* ------------------------------------------------------------------ */
static int g_s17Stub;

void BrStub10008B80(intptr_t a0, ...) { (void)a0; ++g_s17Stub; }
int  BrX10060E90(void) { ++g_s17Stub; return 0; }
void BrX100751D0(void *p) { (void)p; ++g_s17Stub; }
void BrX1002C2C0(void) { ++g_s17Stub; }
void BrX1003563A(int a0) { (void)a0; ++g_s17Stub; }
void BrX100397C0(void) { ++g_s17Stub; }
void BrX1002C500(void) { ++g_s17Stub; }
void BrX10034C66(void (*pfn)(void)) { (void)pfn; ++g_s17Stub; }
void BrX10075F10(void *p) { (void)p; ++g_s17Stub; }
void BrX100664C0(void *p) { (void)p; ++g_s17Stub; }
void BrX10072580(int a0) { (void)a0; ++g_s17Stub; }
void BrX10042AF0(void *p, int a1, int a2)
{ (void)p; (void)a1; (void)a2; ++g_s17Stub; }
void BrX10035BBA(const char *psz) { (void)psz; ++g_s17Stub; }
int  BrXAtExit(void (*pfn)(void)) { (void)pfn; ++g_s17Stub; return 0; }
static unsigned char g_s17Light[64];
static BrMat4        g_s17Mtx[8];
void *BrX10069530(void) { ++g_s17Stub; return g_s17Light; }
void *BrX10069490(void) { ++g_s17Stub; return &g_s17Mtx[0]; }
int   BrX10005DE0(void *p, unsigned char *a, unsigned char *b, unsigned char *c)
{ (void)p; *a = *b = *c = 0; ++g_s17Stub; return 0; }
void  BrX10076AE0(void *p, int a0) { (void)p; (void)a0; ++g_s17Stub; }
const char *BrX10005E70(void *p) { (void)p; ++g_s17Stub; return ""; }
void  BrX10068260(int i, uint32_t tag) { (void)i; (void)tag; ++g_s17Stub; }

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

#define GEO_W 640
#define GEO_H 640

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

/* Decode every texture the load-time pass registered and hand it to the
 * backend.  The addresses in the records are display-list addresses, so
 * they are resolved through the scene's image exactly as br_dl.c resolves a
 * G_VTX address -- no pointer is ever truncated into a 32-bit word. */
/* The decoded texels themselves, laid out as one sheet.  The whole point of
 * this exercise is to be able to LOOK at what the expander produced: a
 * shaded model can hide a texture that is scrambled, tiled at the wrong
 * rate, or reading the palette as texels. */
#define SHEET_W 512
#define SHEET_H 512

static uint8_t g_aSheet[SHEET_H][SHEET_W][3];
static uint32_t g_sheetX, g_sheetY, g_sheetRow;

static void sheet_reset(void)
{
    memset(g_aSheet, 0, sizeof(g_aSheet));
    g_sheetX = g_sheetY = g_sheetRow = 0;
}

static void sheet_add(const uint8_t *pRgba, uint32_t w, uint32_t h)
{
    uint32_t x, y;
    if (w > SHEET_W)
        return;
    if (g_sheetX + w + 2 > SHEET_W) {
        g_sheetX = 0;
        g_sheetY += g_sheetRow + 2;
        g_sheetRow = 0;
    }
    if (g_sheetY + h > SHEET_H)
        return;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const uint8_t *p = pRgba + ((size_t)y * w + x) * 4;
            uint8_t *q = g_aSheet[g_sheetY + y][g_sheetX + x];
            /* a checkerboard under the alpha, so a transparent texel is
             * visible AS transparent rather than as black */
            uint8_t bg = (uint8_t)((((x >> 3) ^ (y >> 3)) & 1) ? 96 : 48);
            q[0] = (uint8_t)((p[0] * p[3] + bg * (255 - p[3])) / 255);
            q[1] = (uint8_t)((p[1] * p[3] + bg * (255 - p[3])) / 255);
            q[2] = (uint8_t)((p[2] * p[3] + bg * (255 - p[3])) / 255);
        }
    if (h > g_sheetRow)
        g_sheetRow = h;
    g_sheetX += w + 2;
}

/* Where the texel and palette addresses in a BrTex3dRec resolve to.  A
 * function pointer because the two scene types answer the same question
 * about two different images -- a .rca at N64 0x803C8000 and a .TRK at N64
 * 0x80025C00 -- and the decode either side of it is identical. */
typedef const uint8_t *(*resolve_fn)(const void *pUser, uint32_t addr,
                                     size_t *pcb);

static const uint8_t *rca_resolve(const void *pUser, uint32_t addr, size_t *pcb)
{
    return BrDlSceneResolve((const BrDlScene *)pUser, addr, pcb);
}

static const uint8_t *trk_resolve(const void *pUser, uint32_t addr, size_t *pcb)
{
    return BrTrkSceneResolve((const BrTrkScene *)pUser, addr, pcb);
}

static uint32_t upload_textures(BrGfx *gfx, const BrTex3d *pT,
                                resolve_fn pfnResolve, const void *pUser,
                                int fPerTexture, uint32_t *pcUnsupported)
{
    uint32_t i, cMapped = 0, cBad = 0;
    static const char *aszRc[] =
        { "ok", "bad id", "format not transcribed", "source unresolved",
          "degenerate" };

    sheet_reset();

    printf("    textures: %u registered, %u binds planted over %u runs"
           " (%u multi-LOD)\n",
           pT->cRec, pT->cPlanted, pT->cRuns, pT->cMultiLod);

    for (i = 0; i < pT->cRec; i++) {
        const BrTex3dRec *pR = &pT->aRec[i];
        size_t cbTex = 0, cbPal = 0, cTexels;
        const uint8_t *pTexels = pfnResolve(pUser, pR->texelSrc, &cbTex);
        const uint8_t *pPal = (pR->palSrc != 0)
            ? pfnResolve(pUser, pR->palSrc, &cbPal) : NULL;
        uint16_t *pRaw;
        uint8_t  *pRgba;
        int rc;

        cTexels = (size_t)pR->w * (size_t)pR->h;
        pRaw  = (uint16_t *)malloc(cTexels * sizeof(uint16_t));
        pRgba = (uint8_t *)malloc(cTexels * 4u);
        if (pRaw == NULL || pRgba == NULL) { free(pRaw); free(pRgba); break; }

        rc = BrTex3dDecode(pT, i, pTexels, cbTex, pPal, cbPal, pRaw);
        /* A track has hundreds of records, so the per-texture dump is off
         * there -- but the ones that did NOT decode are the interesting
         * ones, and they are the pixels that come out flat white, so they
         * always get a line. */
        if (fPerTexture || rc != BR_TEX3D_OK)
            printf("      [%2u] %3dx%-3d fmt=%d siz=%d tlut=%s line=%d "
                   "clamp=%d,%d texels=%08X pal=%08X -> %s\n",
                   i, pR->w, pR->h, pR->tile.fmt, pR->tile.siz,
                   pR->palSrc ? "yes" : "no", pR->tile.line,
                   pR->clampS, pR->clampT, pR->texelSrc, pR->palSrc,
                   aszRc[(rc >= 0 && rc <= 4) ? rc : 1]);

        if (rc == BR_TEX3D_OK) {
            BrTexture h;
            BrTex3dToRgba8(pRaw, (uint32_t)cTexels, pRgba);
            sheet_add(pRgba, (uint32_t)pR->w, (uint32_t)pR->h);
            h = BrGfxCreateTexture(gfx, (uint32_t)pR->w, (uint32_t)pR->h, pRgba);
            if (h != 0) {
                float sS, sT;
                BrTex3dTexScaleNorm(pT, i, &sS, &sT);
                BrGfx3dMapTexture(gfx, i, h, sS, sT);
                cMapped++;
            }
        } else {
            cBad++;
        }
        free(pRaw);
        free(pRgba);
    }
    if (pcUnsupported != NULL)
        *pcUnsupported = cBad;
    return cMapped;
}

/* A 64x24 coverage thumbnail.  `mode` 0 = alpha (is anything there),
 * 1 = luminance ramp (what shape is it), because a track fills the frame and
 * a pure coverage map of a full frame is a solid block of '#'. */
static void thumb_wh(const char *pszWhat, const uint8_t *pRgba,
                     int w, int h, int mode)
{
    static const char kRamp[] = " .:-=+*#%@";
    int ty, tx;
    printf("    %s\n", pszWhat);
    for (ty = 0; ty < 24; ty++) {
        char row[65];
        for (tx = 0; tx < 64; tx++) {
            int sy = ty * (h / 24), sx = tx * (w / 64), k, l;
            int lit = 0; long sum = 0, n = 0;
            for (k = 0; k < h / 24; k++)
                for (l = 0; l < w / 64; l++) {
                    int px = sx + l, py = sy + k;
                    const uint8_t *p;
                    if (px >= w || py >= h)
                        continue;
                    p = pRgba + ((size_t)py * (size_t)w + (size_t)px) * 4;
                    if (p[3] > 127) lit = 1;
                    sum += (long)p[0] + p[1] + p[2];
                    n += 3;
                }
            if (mode == 0)
                row[tx] = (char)(lit ? '#' : '.');
            else {
                int v = n ? (int)(sum / n) : 0;
                row[tx] = kRamp[(v * 9) / 255];
            }
        }
        row[64] = '\0';
        printf("    %s\n", row);
    }
}

static void thumb(const char *pszWhat, const uint8_t *pRgba)
{
    thumb_wh(pszWhat, pRgba, GEO_W, GEO_H, 0);
}

static void test_geometry(const char *pszRca)
{
    BrDlScene scene;
    BrDl st;
    BrDlRaster ras;
    BrGfx *gfx;
    uint8_t *pSoft, *pHard, *pDepth, *pTexd;
    uint32_t softTri, hardTri, i, both = 0, either = 0, softOnly = 0, hardOnly = 0;
    uint32_t cMapped = 0, cUnsupported = 0, cBindsPlain = 0;
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
    pTexd  = (uint8_t *)calloc((size_t)GEO_W * GEO_H * 4, 1);
    if (pSoft == NULL || pHard == NULL || pDepth == NULL || pTexd == NULL) {
        check(0, "alloc");
        free(pSoft); free(pHard); free(pDepth); free(pTexd);
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
        free(pSoft); free(pHard); free(pDepth); free(pTexd);
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
    cBindsPlain = BrGfx3dGetStats(gfx)->cBinds;

    /* --- 3b. the same render with the TEXTURES BOUND ----------------
     * Deliberately after the three renders above, so every assertion that
     * compares the two rasterisers is comparing what it always compared.
     * The only thing that changes here is which pixels the fragment shader
     * samples: the geometry, the state machine and the batching are the
     * same walk of the same list. */
    cMapped = upload_textures(gfx, &scene.tex, rca_resolve, &scene, 1,
                              &cUnsupported);
    geo_setup(&st, &scene);
    BrGfx3dAttach(gfx, &st);
    BrGfx3dSetDepthTest(gfx, 1);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlSceneRun(&scene, &st);
    BrGfx3dEndFrame(gfx);
    BrGfxReadPixels(gfx, pTexd);

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

    /* The texture path, asserted on what it can only be true of if the
     * whole chain ran: a bind must have been PLANTED (nothing in a shipped
     * .rca carries one), a handle must have RESOLVED to pixels, and binding
     * it must have changed the picture without moving the silhouette. */
    {
        uint32_t litPlain = 0, litTex = 0, agree = 0, changed = 0, texOnly = 0;
        double texDiff = 0.0;
        for (i = 0; i < (uint32_t)(GEO_W * GEO_H); i++) {
            int a = pDepth[i * 4 + 3] > 127, b = pTexd[i * 4 + 3] > 127;
            if (a) litPlain++;
            if (b) litTex++;
            if (b && !a) texOnly++;
            if (a && b) {
                int k, d = 0;
                agree++;
                for (k = 0; k < 3; k++)
                    d += abs((int)pDepth[i * 4 + k] - (int)pTexd[i * 4 + k]);
                texDiff += d;
                if (d > 24) changed++;
            }
        }
        printf("    textured: mapped=%u unsupported=%u binds=%u  "
               "opaque(plain)=%u opaque(textured)=%u textured-only=%u  "
               "mean|RGB|delta=%.1f  pixels visibly changed=%u (%.1f%%)\n",
               cMapped, cUnsupported, cBindsPlain, litPlain, litTex, texOnly,
               agree ? texDiff / (agree * 3.0) : 0.0, changed,
               agree ? 100.0 * (double)changed / (double)agree : 0.0);
        thumb("Metal (textured)", pTexd);

        check(scene.tex.cPlanted > 0,
              "the load-time pass planted at least one 0xDC "
              "(a shipped .rca carries none)");
        check(cBindsPlain == scene.tex.cPlanted,
              "every planted 0xDC reached the sink exactly once");
        check(cMapped > 0, "at least one registered texture decoded to pixels");
        /* Binding a texture cannot ADD coverage.  An unbound handle samples
         * a 1x1 opaque white texel, so every texel a real texture supplies
         * has alpha <= that -- these are 1-bit-alpha formats and the
         * transparent texels are exactly the ones that drop out.  What must
         * never happen is a pixel becoming opaque that was not, which is
         * what a geometry or batching change would look like.
         *
         * This is deliberately NOT "the two silhouettes are equal": that
         * would be an expectation about the ARTWORK (whether any texel is
         * transparent), not a property of the code -- the trap
         * CONVENTIONS.md records under "Tests". */
        check(texOnly == 0,
              "binding a texture removes coverage or leaves it alone, "
              "never adds it");
        /* ... and it must actually SAMPLE it.  An unmapped handle reads a
         * 1x1 white texel, so if the mapping were inert this would be 0. */
        check(changed > 0,
              "binding the decoded textures changed the shaded pixels");
    }

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
        snprintf(szOut, sizeof(szOut), "build/gfx_%.4s_metal_tex.ppm", pszTag);
        write_ppm(szOut, pTexd, GEO_W, GEO_H);
        snprintf(szOut, sizeof(szOut), "build/gfx_%.4s_textures.ppm", pszTag);
        {
            FILE *f = fopen(szOut, "wb");
            if (f != NULL) {
                fprintf(f, "P6\n%u %u\n255\n", (unsigned)SHEET_W,
                        (unsigned)SHEET_H);
                fwrite(g_aSheet, 1, sizeof(g_aSheet), f);
                fclose(f);
            }
        }
        printf("    -> build/gfx_%.4s_{soft,metal,metal_z,metal_tex,"
               "textures}.ppm\n", pszTag);
    }

    free(pSoft); free(pHard); free(pDepth); free(pTexd);
    BrGfxDestroy(gfx);
    BrDlSceneFree(&scene);
}

/* ------------------------------------------------------------------ */
/* Part 2b: a TRACK                                                   */
/*                                                                    */
/* Everything above renders a .rca -- a car -- through a view matrix   */
/* br_dlscene.h openly calls scaffolding, because a .rca carries no    */
/* transform and the container index has never been found.  A .TRK is  */
/* the other case entirely: the file says where its display lists are, */
/* each list comes with an instance matrix, and the CAMERA that turns  */
/* those into a frame is a real function in the binary.  br_trkscene.h */
/* records where all three come from and, just as important, which     */
/* parts of the pose are still the harness's choice.                   */
/*                                                                    */
/* 640x480 and not 640x640: the fovy expression BrCamMatrixSetup uses  */
/* carries a (4/3)*(h/w) factor that is exactly 1 at 4:3, and 4:3 is   */
/* what BrViewportSetFull's 640/480 at 0x100A81C0/C4 says the game     */
/* renders at.  Rendering the game's camera at a made-up aspect ratio  */
/* would put a number in the picture that is nobody's.                 */
/* ------------------------------------------------------------------ */

#define TRK_W 640
#define TRK_H 480

static void trk_setup(BrDl *pDl, const BrTrkScene *pScene, BrTrkPose *pPose)
{
    BrDlInit(pDl, TRK_W, TRK_H);
    BrTrkSceneBind(pScene, pDl);
    BrTrkSceneSetState(pDl);
    /* HARNESS, and labelled as such at the definition: how far behind the
     * start line, how high above it, how far to pitch down, and the fov in
     * radians.  The fov FIELD is the game's (BrCamMatrixSetup's a2, pushed
     * from pCam+0x40 at 0x10014C90); this VALUE is not.  0.9 rad is 51.6
     * degrees vertical at 4:3. */
    BrTrkSceneStartPose(pScene, pPose,
                        geo_env("BR_TRK_BACK", 12.0f),
                        geo_env("BR_TRK_UP", 4.0f),
                        geo_env("BR_TRK_PITCH", 6.0f),
                        geo_env("BR_TRK_FOV", 0.9f));
    BrTrkSceneCamera(pDl, pPose, TRK_W, TRK_H);
}

static void test_track(const char *pszTrk)
{
    BrTrkScene scene;
    BrTrkPose  pose;
    BrDl st;
    BrDlRaster ras;
    BrGfx *gfx;
    uint8_t *pSoft, *pHard, *pTexd;
    uint32_t softTri, softClipOut, i, both = 0, either = 0;
    uint32_t cMapped = 0, cUnsupported = 0;
    double diff = 0.0;
    const BrGfx3dStats *pStats;

    printf("track: %s\n", pszTrk);
    if (BrTrkSceneLoad(&scene, pszTrk) != 0) {
        check(0, "BrTrkSceneLoad");
        return;
    }
    printf("    instances=%u lists=%u draws=%u commands=%u verts=%u bad=%u\n",
           BrTrackInstanceCount(&scene.track), scene.cLists, scene.cDraw,
           scene.cCommands, scene.cVerts, scene.cListsBad);
    printf("    collision-mesh bounds [%.0f %.0f %.0f]..[%.0f %.0f %.0f]\n",
           scene.bbMin.x, scene.bbMin.y, scene.bbMin.z,
           scene.bbMax.x, scene.bbMax.y, scene.bbMax.z);
    check(scene.cLists > 1, "more than one display list found "
                            "(the +0x50 root and the instance array)");
    check(scene.fEndOk, "every display-list run terminates at G_ENDDL");
    check(scene.cListsBad == 0, "every list pointer resolved inside the image");

    pSoft = (uint8_t *)calloc((size_t)TRK_W * TRK_H * 4, 1);
    pHard = (uint8_t *)calloc((size_t)TRK_W * TRK_H * 4, 1);
    pTexd = (uint8_t *)calloc((size_t)TRK_W * TRK_H * 4, 1);
    if (pSoft == NULL || pHard == NULL || pTexd == NULL) {
        check(0, "alloc");
        free(pSoft); free(pHard); free(pTexd);
        BrTrkSceneFree(&scene);
        return;
    }

    /* --- 1. br_dl.c's reference rasteriser ------------------------- */
    trk_setup(&st, &scene, &pose);
    printf("    camera: eye (%.1f %.1f %.1f) fwd (%.3f %.3f %.3f) up (0 0 1)"
           " fov %.3f rad, near 0.8, far %.0f\n",
           pose.eye.x, pose.eye.y, pose.eye.z,
           pose.fwd.x, pose.fwd.y, pose.fwd.z, pose.fovRad, pose.farClip);
    ras.pRgba = pSoft; ras.cx = TRK_W; ras.cy = TRK_H; ras.cCovered = 0;
    BrDlAttachRaster(&st, &ras);
    BrTrkSceneRun(&scene, &st, &pose);
    softTri = st.cTriDrawn;
    softClipOut = st.cTriClipOut;
    printf("    software: in=%u drawn=%u rejected=%u clipped=%u->%u px=%u"
           "  lit=%u (ambient-only %u)\n",
           st.cTriIn, st.cTriDrawn, st.cTriRejected, st.cTriClipped,
           st.cTriClipOut, ras.cCovered, st.cVtxLit, st.cVtxLitAmbient);
    check(st.cTriIn == st.cTriDrawn + st.cTriRejected + st.cTriClipped,
          "every triangle drawn, rejected or clipped -- none lost");
    check(st.cVtxLit > 0,
          "the LIGHTING geometry-mode bit selected a lighting transform");

    gfx = BrGfxCreate(TRK_W, TRK_H);
    if (gfx == NULL) {
        printf("  [FAIL] BrGfxCreate: %s\n", BrGfxLastError());
        g_fail = 1;
        free(pSoft); free(pHard); free(pTexd);
        BrTrkSceneFree(&scene);
        return;
    }
    BrGfx3dInit(gfx);

    /* --- 2. Metal, depth test off, to match the software path ------ */
    trk_setup(&st, &scene, &pose);
    BrGfx3dAttach(gfx, &st);
    BrGfx3dSetDepthTest(gfx, 0);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrTrkSceneRun(&scene, &st, &pose);
    BrGfx3dEndFrame(gfx);
    check(BrGfxReadPixels(gfx, pHard) == 0, "BrGfxReadPixels (Metal track)");
    pStats = BrGfx3dGetStats(gfx);
    printf("    metal:    tri=%u verts=%u draws=%u statechanges=%u binds=%u\n",
           pStats->cTri, pStats->cVerts, pStats->cDraws,
           pStats->cStateChanges, pStats->cBinds);
    /* The two counters are not the same quantity and a track is the first
     * asset where the difference shows: br_dl.h defines cTriDrawn as
     * triangles that went to the sink WHOLE, with the ones the clipper cut up
     * counted separately in cTriClipOut.  A .rca framed to fit clips nothing,
     * so the two happened to be equal in part 2; a track fills the frame and
     * clips against every plane.  What must hold is that the sink saw the
     * same total both times. */
    check(pStats->cTri == softTri + softClipOut,
          "Metal and software received the same triangle count "
          "(whole + clipper output)");

    /* --- 3. the honest render: z-buffer on, textures bound --------- */
    cMapped = upload_textures(gfx, &scene.tex, trk_resolve, &scene, 0,
                              &cUnsupported);
    trk_setup(&st, &scene, &pose);
    BrGfx3dAttach(gfx, &st);
    BrGfx3dSetDepthTest(gfx, 1);
    BrGfx3dBeginFrame(gfx, 0.0f, 0.0f, 0.0f, 0.0f);
    BrTrkSceneRun(&scene, &st, &pose);
    BrGfx3dEndFrame(gfx);
    BrGfxReadPixels(gfx, pTexd);
    printf("    textures: %u of %u decoded and mapped (%u unsupported)\n",
           cMapped, scene.tex.cRec, cUnsupported);

    /* --- 4. compare ------------------------------------------------ */
    for (i = 0; i < (uint32_t)(TRK_W * TRK_H); i++) {
        int s = pSoft[i * 4 + 3] > 127, h = pHard[i * 4 + 3] > 127;
        if (s && h) {
            int k;
            both++;
            for (k = 0; k < 3; k++)
                diff += (double)abs((int)pSoft[i * 4 + k] -
                                    (int)pHard[i * 4 + k]);
        }
        if (s || h) either++;
    }
    printf("    silhouette: union=%u intersection=%u IoU=%.4f  "
           "mean |RGB| difference %.3f / 255\n",
           either, both, either ? (double)both / (double)either : 0.0,
           both ? diff / (both * 3.0) : 0.0);

    thumb_wh("software (br_dl.c reference rasteriser, no z)", pSoft,
             TRK_W, TRK_H, 0);
    thumb_wh("Metal (z-buffer on, textured) -- luminance", pTexd,
             TRK_W, TRK_H, 1);

    check(softTri > 100, "the track produced a substantial triangle count");
    check(either > (uint32_t)(TRK_W * TRK_H) / 20,
          "the track covers a substantial area of the frame");
    check(either > 0 && (double)both / (double)either > 0.95,
          "silhouettes agree (IoU > 0.95)");
    /* Both renders here are UNTEXTURED -- the Metal one runs before the
     * textures are uploaded, so an unmapped handle samples a 1x1 white texel
     * and what is left is the iterated colour alone.  That makes the whole
     * lighting chain comparable between the two rasterisers, which is the
     * one thing a picture cannot check by itself: 8008 lit vertices, two
     * independent interpolators, one colour convention. */
    check(both > 0 && diff / (both * 3.0) < 8.0,
          "lit interior colours agree to within 8/255 on average");

    {
        char szOut[256];
        const char *pszTag = strrchr(pszTrk, '/');
        pszTag = pszTag ? pszTag + 1 : pszTrk;
        snprintf(szOut, sizeof(szOut), "build/trk_%.6s_soft.ppm", pszTag);
        write_ppm(szOut, pSoft, TRK_W, TRK_H);
        snprintf(szOut, sizeof(szOut), "build/trk_%.6s_metal.ppm", pszTag);
        write_ppm(szOut, pHard, TRK_W, TRK_H);
        snprintf(szOut, sizeof(szOut), "build/trk_%.6s_metal_tex.ppm", pszTag);
        write_ppm(szOut, pTexd, TRK_W, TRK_H);
        printf("    -> build/trk_%.6s_{soft,metal,metal_tex}.ppm\n", pszTag);
    }

    free(pSoft); free(pHard); free(pTexd);
    BrGfxDestroy(gfx);
    BrTrkSceneFree(&scene);
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

    {
        FILE *probe = fopen("testdata/tracks/race.trk", "rb");
        if (probe == NULL) {
            printf("  (testdata/tracks/race.trk absent -- track section"
                   " skipped)\n");
        } else {
            fclose(probe);
            test_track("testdata/tracks/race.trk");
            test_track("testdata/tracks/desert.trk");
        }
    }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
