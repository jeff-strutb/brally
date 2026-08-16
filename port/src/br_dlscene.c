/* br_dlscene.c -- see br_dlscene.h.  Scaffolding around br_dl.c, factored out
 * of test_br_dl.c so that two renderers can be handed identical geometry. */

#include "br_dlscene.h"
#include "slice1_05.h"      /* BrVtxCache, BrVtxCacheResolve (0x10018E10)   */
#include "br_mat.h"         /* BrMat4Perspective (0x10030930), BrMat4Mul    */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

/* The arena is sized against the file: an .rca cannot contain more than
 * cbFile/16 vertices, and each expands to eight floats. */
#define BR_DLSCENE_ARENA_SLACK 8u

static uint32_t br_dlscene_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* The Glide patch pass's resolve hook.  0x10019040 routes every G_VTX
 * through 0x10018E10 == BrVtxCacheResolve and stores the resulting HOST
 * pointer back into w1.  A host pointer does not fit in 32 bits here (see
 * br_dl.h), so the pointer is converted to an arena address instead; the
 * conversion is exact and the region table undoes it at draw time. */
static void br_dlscene_resolve(void *pUser, uint32_t *pw1, int n)
{
    BrDlScene *pS = (BrDlScene *)pUser;
    uint32_t addr = *pw1;
    size_t off;
    void *pv;

    if (addr < BR_DLSCENE_N64_BASE) { *pw1 = 0; return; }
    off = (size_t)BR_DLSCENE_FILE_BASE + (size_t)(addr - BR_DLSCENE_N64_BASE);
    if (n <= 0 || off + (size_t)n * 16u > pS->cbFile) { *pw1 = 0; return; }
    /* Refuse rather than overrun.  DEVIATION: the original has no bound. */
    if (pS->cArena + (size_t)n * 8u > pS->cArenaMax) { *pw1 = 0; return; }

    pv = pS->pFile + off;
    pS->pCache->pCursor = pS->pArena + pS->cArena;
    BrVtxCacheResolve(pS->pCache, &pv, n);
    /* A cache HIT leaves the cursor alone and returns an earlier block, so
     * the arena high-water mark is read back rather than added to. */
    pS->cArena = (size_t)(pS->pCache->pCursor - pS->pArena);
    *pw1 = BR_DLSCENE_ARENA_BASE +
           (uint32_t)(((const float *)pv - pS->pArena) * 4);
}

int BrDlSceneLoad(BrDlScene *pScene, const char *pszPath)
{
    FILE *f;
    long  cb;
    uint8_t *marked = NULL;
    size_t off, i;

    memset(pScene, 0, sizeof(*pScene));
    pScene->fEndOk = 1;

    f = fopen(pszPath, "rb");
    if (f == NULL)
        return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    cb = ftell(f);
    rewind(f);
    if (cb <= (long)BR_DLSCENE_FILE_BASE) { fclose(f); return 1; }

    pScene->cbFile    = (size_t)cb;
    pScene->pFile     = (uint8_t *)malloc(pScene->cbFile);
    pScene->cArenaMax = (pScene->cbFile / 16u + 1u) * BR_DLSCENE_ARENA_SLACK;
    pScene->pArena    = (float *)malloc(pScene->cArenaMax * sizeof(float));
    pScene->pCache    = (struct BrVtxCache *)calloc(1, sizeof(BrVtxCache));
    marked            = (uint8_t *)calloc(pScene->cbFile, 1);

    if (pScene->pFile == NULL || pScene->pArena == NULL ||
        pScene->pCache == NULL || marked == NULL ||
        fread(pScene->pFile, 1, pScene->cbFile, f) != pScene->cbFile) {
        fclose(f);
        free(marked);
        BrDlSceneFree(pScene);
        return 1;
    }
    fclose(f);

    ((BrVtxCache *)pScene->pCache)->pCursor = pScene->pArena;

    /* SCAFFOLDING: a run is entered at a big-endian G_VTX whose address word
     * is a KSEG0 pointer.  That is the only unambiguous entry signature
     * available without the container's own index, which has not been
     * located.  Each byte is marked once so a list is patched exactly once,
     * which is what BrDlPatch requires (it byte-swaps IN PLACE). */
    for (off = BR_DLSCENE_FILE_BASE; off + 8 <= pScene->cbFile; off += 8) {
        uint32_t w0, w1;
        size_t n, k;

        if (marked[off])
            continue;
        w0 = br_dlscene_be32(pScene->pFile + off);
        w1 = br_dlscene_be32(pScene->pFile + off + 4);
        if ((w0 >> 24) != 0x04u || (w1 >> 24) != 0x80u)
            continue;

        n = BrDlPatch(NULL, pScene->pFile + off, pScene->cbFile - off,
                      br_dlscene_resolve, pScene);
        if (n == 0)
            continue;
        for (k = 0; k < n * 8 && off + k < pScene->cbFile; ++k)
            marked[off + k] = 1;
        /* BrDlPatch stops at G_ENDDL; if it stopped because it ran out of
         * buffer instead, the last command is not one. */
        if (pScene->pFile[off + (n - 1) * 8 + 3] != 0xB8u)
            pScene->fEndOk = 0;
        if (pScene->cRuns < BR_DLSCENE_MAX_RUNS)
            pScene->aRun[pScene->cRuns++] = (uint32_t)off;
    }
    free(marked);

    pScene->cVerts = (uint32_t)(pScene->cArena / 8u);
    pScene->bbMin[0] = pScene->bbMin[1] = pScene->bbMin[2] =  1e30f;
    pScene->bbMax[0] = pScene->bbMax[1] = pScene->bbMax[2] = -1e30f;
    for (i = 0; i + 8 <= pScene->cArena; i += 8) {
        int a;
        for (a = 0; a < 3; ++a) {
            float v = pScene->pArena[i + (size_t)a];
            if (v < pScene->bbMin[a]) pScene->bbMin[a] = v;
            if (v > pScene->bbMax[a]) pScene->bbMax[a] = v;
        }
    }
    if (pScene->cVerts == 0) {
        for (i = 0; i < 3; ++i) { pScene->bbMin[i] = -1.0f; pScene->bbMax[i] = 1.0f; }
    }
    return (pScene->cRuns > 0) ? 0 : 1;
}

void BrDlSceneFree(BrDlScene *pScene)
{
    if (pScene == NULL)
        return;
    free(pScene->pFile);
    free(pScene->pArena);
    free(pScene->pCache);
    memset(pScene, 0, sizeof(*pScene));
}

void BrDlSceneBind(const BrDlScene *pScene, BrDl *pDl)
{
    BrDlAddRegion(pDl, BR_DLSCENE_ARENA_BASE, pScene->pArena,
                  pScene->cArena * sizeof(float));
    BrDlAddRegion(pDl, BR_DLSCENE_N64_BASE,
                  pScene->pFile + BR_DLSCENE_FILE_BASE,
                  pScene->cbFile - BR_DLSCENE_FILE_BASE);
}

void BrDlSceneView(const BrDlScene *pScene, BrDl *pDl,
                   float yawDeg, float pitchDeg, float fovDeg,
                   int32_t cx, int32_t cy)
{
    const float kDeg = 3.14159265358979f / 180.0f;
    float c[3], r = 0.0f;
    float cyaw, syaw, cpit, spit, dist, zn, zf;
    BrMat4 mv, proj;
    unsigned short perspNorm = 0;
    int a;

    for (a = 0; a < 3; ++a) {
        float half = (pScene->bbMax[a] - pScene->bbMin[a]) * 0.5f;
        c[a] = (pScene->bbMax[a] + pScene->bbMin[a]) * 0.5f;
        r += half * half;
    }
    r = sqrtf(r);
    if (!(r > 0.0f))
        r = 1.0f;

    cyaw = cosf(yawDeg * kDeg);   syaw = sinf(yawDeg * kDeg);
    cpit = cosf(pitchDeg * kDeg); spit = sinf(pitchDeg * kDeg);

    /* Row-vector convention (v' = v * M), so a row is the image of a basis
     * vector.  R = Ry * Rx, written out rather than multiplied so the sign
     * convention is visible. */
    memset(&mv, 0, sizeof(mv));
    mv.m[0][0] =  cyaw;         mv.m[0][1] = -syaw * spit; mv.m[0][2] = -syaw * cpit;
    mv.m[1][0] =  0.0f;         mv.m[1][1] =  cpit;        mv.m[1][2] = -spit;
    mv.m[2][0] =  syaw;         mv.m[2][1] =  cyaw * spit; mv.m[2][2] =  cyaw * cpit;
    mv.m[3][3] =  1.0f;

    /* Push the model's centre to (0, 0, -dist): BrMat4Perspective's layout
     * has -1 at [2][3], so w == -z and the camera looks down -Z. */
    dist = r * 2.8f;
    for (a = 0; a < 3; ++a)
        mv.m[3][a] = -(c[0] * mv.m[0][a] + c[1] * mv.m[1][a] + c[2] * mv.m[2][a]);
    mv.m[3][2] -= dist;

    zn = dist - r * 1.5f;
    if (!(zn > r * 0.01f))
        zn = r * 0.01f;
    zf = dist + r * 2.0f;
    BrMat4Perspective(&proj, &perspNorm, fovDeg,
                      (cy > 0) ? (float)cx / (float)cy : 1.0f, zn, zf);

    /* combined = model * projection, the same order 0x1002118A uses. */
    BrMat4Mul(&mv, &proj, &pDl->combined);

    BrDlSetViewport(pDl, (float)cx * 0.5f, (float)cx * 0.5f,
                    (float)cy * -0.5f, (float)cy * 0.5f);
}

void BrDlSceneRun(const BrDlScene *pScene, BrDl *pDl)
{
    size_t i;
    for (i = 0; i < pScene->cRuns; ++i)
        BrDlRun(pDl, pScene->pFile + pScene->aRun[i],
                pScene->cbFile - pScene->aRun[i]);
}
