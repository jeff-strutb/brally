/* br_trkscene.c -- see br_trkscene.h.  The .TRK counterpart of br_dlscene.c.
 *
 * The shape is deliberately br_dlscene.c's, so the two can be read side by
 * side: load the file, find the display lists, patch each exactly once,
 * expand its vertices into an arena, run the load-time texture pass, and
 * hand br_dl.c a region table instead of host pointers.  The differences are
 * all consequences of a .TRK being a SCENE rather than a bag of lists:
 *
 *   - the lists are not discovered by signature.  br_dlscene.c scans for a
 *     big-endian G_VTX because the .rca container index has never been
 *     located; the .TRK header says where its lists are (+0x50 and each
 *     instance's +0x44), and slice2_20.c already transcribes the original
 *     walking exactly those two.  So there is no scaffolding here.
 *   - each list is drawn under its instance's 4x4, which goes into the
 *     MODELVIEW slot.  The PROJECTION slot gets view*projection, already
 *     multiplied on the host -- that is the original's arrangement, not a
 *     simplification (see br_trkscene.h).
 */

#include "br_trkscene.h"
#include "slice1_05.h"      /* BrVtxCache, BrVtxCacheResolve (0x1002BD50) */
#include "slice2_17.h"      /* BrMat4LookAt (0x100309A0)                  */
/* BrMat4Translate is slice1_05.h's, 0x1002A7F0 Glide == 0x10031140 D3D. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0x100306C0 -- the full 4x4 multiply the G_MTX handler and the camera both
 * use.  Declared here rather than pulled in from slice1_05.h because that
 * header spells it with the same name and a different comment; one extern
 * keeps the two translation units agreeing. */
extern void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

#define BR_TRKSCENE_ARENA_SLACK 8u
#define BR_TRKSCENE_DRAW_GROW   256u

static uint32_t br_trk_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* A header field br_track.c relocated: it is already a FILE OFFSET.  A field
 * read straight out of the image (an instance's +0x44) is still a raw N64
 * address, because br_track.c's relocation pass only covers the header. */
static uint32_t br_trk_n64_to_off(const BrTrkScene *pScene, uint32_t n64)
{
    if (n64 == 0 || n64 < BR_TRK_N64_BASE)
        return 0;
    if ((n64 - BR_TRK_N64_BASE) >= pScene->track.cbImage)
        return 0;
    return n64 - BR_TRK_N64_BASE;
}

/* ---------------------------------------------------------------------
 * The patch pass's resolve hook
 * ---------------------------------------------------------------------
 * Identical in role to br_dlscene.c's: 0x10019040 routes every G_VTX through
 * BrVtxCacheResolve and stores the resulting HOST pointer back into w1; a
 * host pointer does not fit in 32 bits, so the pointer becomes an arena
 * address and the region table undoes it at draw time.
 *
 * MEASURED, and it is what makes the cache safe here: across race.trk's 370
 * lists there are 8949 G_VTX vertices in 505 distinct (address, count)
 * pairs, and NO address appears twice under two different counts.  The cache
 * key is that pair, so every repeat is a hit and no vertex block is ever
 * byte-swapped twice -- which is the one way BrVtxCacheResolve can corrupt
 * data (slice1_05.h says so).  505 is also comfortably under the 0x800 entry
 * table, so the cache never fills and never starts missing. */
static void br_trkscene_resolve(void *pUser, uint32_t *pw1, int n)
{
    BrTrkScene *pS = (BrTrkScene *)pUser;
    uint32_t off = br_trk_n64_to_off(pS, *pw1);
    void *pv;

    if (off == 0 || n <= 0 ||
        off + (size_t)n * 16u > pS->track.cbImage) { *pw1 = 0; return; }
    /* Refuse rather than overrun.  DEVIATION: the original has no bound. */
    if (pS->cArena + (size_t)n * 8u > pS->cArenaMax) { *pw1 = 0; return; }

    pv = pS->track.pbImage + off;
    pS->pCache->pCursor = pS->pArena + pS->cArena;
    BrVtxCacheResolve(pS->pCache, &pv, n);
    /* A cache HIT leaves the cursor alone and returns an earlier block, so
     * the high-water mark is read back rather than added to. */
    pS->cArena = (size_t)(pS->pCache->pCursor - pS->pArena);
    *pw1 = BR_TRKSCENE_ARENA_BASE +
           (uint32_t)(((const float *)pv - pS->pArena) * 4);
}

/* ---------------------------------------------------------------------
 * List registration -- BrDlRegister + g_pfn18AA0C4, in that order
 * --------------------------------------------------------------------- */

/* Returns the list's file offset, or 0.  `pMarked` makes a list that two
 * instances share get patched once: BrDlPatch byte-swaps IN PLACE, so a
 * second pass over the same bytes would swap them back. */
static uint32_t br_trkscene_add_list(BrTrkScene *pScene, uint8_t *pMarked,
                                     uint32_t off)
{
    size_t n, k;

    if (off == 0 || off + 8 > pScene->track.cbImage)
        return 0;
    if (pMarked[off])
        return off;                     /* already patched -- reuse it */

    n = BrDlPatch(NULL, pScene->track.pbImage + off,
                  pScene->track.cbImage - off,
                  br_trkscene_resolve, pScene);
    if (n == 0)
        return 0;

    /* THE LOAD-TIME TEXTURE PASS, in the place the original runs it: the
     * fixup calls BrDlRegister and then g_pfn18AA0C4 == 0x10028820 on the
     * same pointer, back to back (slice2_20.c:835 and :661). */
    BrTex3dScan(&pScene->tex, pScene->track.pbImage + off, n * 8u);

    for (k = 0; k < n * 8 && off + k < pScene->track.cbImage; ++k)
        pMarked[off + k] = 1;
    if (pScene->track.pbImage[off + (n - 1) * 8 + 3] != 0xB8u)
        pScene->fEndOk = 0;
    pScene->cLists++;
    pScene->cCommands += (uint32_t)n;
    return off;
}

static int br_trkscene_add_draw(BrTrkScene *pScene, const BrMat4 *pM,
                                uint32_t listOff, int32_t iInst)
{
    if (pScene->cDraw == pScene->cDrawMax) {
        uint32_t cNew = pScene->cDrawMax + BR_TRKSCENE_DRAW_GROW;
        BrTrkDraw *p = (BrTrkDraw *)realloc(pScene->aDraw,
                                            (size_t)cNew * sizeof(BrTrkDraw));
        if (p == NULL)
            return 1;
        pScene->aDraw = p;
        pScene->cDrawMax = cNew;
    }
    pScene->aDraw[pScene->cDraw].model = *pM;
    pScene->aDraw[pScene->cDraw].listOff = listOff;
    pScene->aDraw[pScene->cDraw].iInstance = iInst;
    pScene->cDraw++;
    return 0;
}

int BrTrkSceneLoad(BrTrkScene *pScene, const char *pszPath)
{
    uint8_t *pMarked = NULL;
    uint32_t cInst, i, instOff, rootOff;
    BrMat4   ident;

    memset(pScene, 0, sizeof(*pScene));
    pScene->fEndOk = 1;

    if (BrTrackOpen(&pScene->track, pszPath) != 0)
        return 1;

    pScene->cArenaMax = ((size_t)pScene->track.cbImage / 16u + 1u)
                        * BR_TRKSCENE_ARENA_SLACK;
    pScene->pArena = (float *)malloc(pScene->cArenaMax * sizeof(float));
    pScene->pCache = (struct BrVtxCache *)calloc(1, sizeof(BrVtxCache));
    pMarked        = (uint8_t *)calloc(pScene->track.cbImage, 1);
    if (pScene->pArena == NULL || pScene->pCache == NULL || pMarked == NULL) {
        free(pMarked);
        BrTrkSceneFree(pScene);
        return 1;
    }
    ((BrVtxCache *)pScene->pCache)->pCursor = pScene->pArena;

    BrMat4Identity(&ident);

    /* 1. THE ROOT, header +0x50.  br_track.c has already relocated it, so it
     *    is a file offset rather than an N64 address.  The original does this
     *    one FIRST, before the instance array (slice2_20.c:835 vs :837). */
    rootOff = BrTrackHdrU32(&pScene->track, BR_TRK_H_DLROOT);
    if (rootOff != 0 && rootOff < pScene->track.cbImage) {
        uint32_t o = br_trkscene_add_list(pScene, pMarked, rootOff);
        if (o != 0)
            br_trkscene_add_draw(pScene, &ident, o, -1);
        else
            pScene->cListsBad++;
    } else if (rootOff != 0) {
        pScene->cListsBad++;
    }

    /* 2. THE INSTANCE ARRAY, header +0x60 / +0x64.  Each 0x54-byte record is
     *    a 4x4 followed by the scale the load pass wrote at +0x40 and, at
     *    +0x44, the record's display list. */
    instOff = BrTrackHdrU32(&pScene->track, BR_TRK_H_INSTANCES);
    cInst   = BrTrackInstanceCount(&pScene->track);
    if (instOff != 0 &&
        (uint64_t)instOff + (uint64_t)cInst * BR_TRK_INSTANCE_STRIDE
            <= pScene->track.cbImage) {
        for (i = 0; i < cInst; ++i) {
            const uint8_t *pRec = pScene->track.pbImage
                                + instOff + (size_t)i * BR_TRK_INSTANCE_STRIDE;
            uint32_t off = br_trk_n64_to_off(pScene,
                               br_trk_be32(pRec + BR_TRK_INST_DLOFF));
            BrMat4 m;

            if (off == 0)
                continue;               /* no list: 17 of race.trk's 386 */
            off = br_trkscene_add_list(pScene, pMarked, off);
            if (off == 0) { pScene->cListsBad++; continue; }
            /* br_track.c decodes the matrix big-endian out of the image and
             * is the only thing that knows the record layout; use it rather
             * than a second decoder that could drift. */
            if (BrTrackInstance(&pScene->track, i, &m, NULL, NULL) != 0)
                continue;
            br_trkscene_add_draw(pScene, &m, off, (int32_t)i);
        }
    }

    free(pMarked);
    pScene->cVerts = (uint32_t)(pScene->cArena / 8u);

    if (BrTrackBounds(&pScene->track, &pScene->bbMin, &pScene->bbMax) != 0) {
        pScene->bbMin.x = pScene->bbMin.y = pScene->bbMin.z = 0.0f;
        pScene->bbMax.x = pScene->bbMax.y = pScene->bbMax.z = 1.0f;
    }

    return (pScene->cDraw > 0) ? 0 : 1;
}

void BrTrkSceneFree(BrTrkScene *pScene)
{
    if (pScene == NULL)
        return;
    BrTrackClose(&pScene->track);
    free(pScene->pArena);
    free(pScene->pCache);
    free(pScene->aDraw);
    BrTex3dFree(&pScene->tex);
    memset(pScene, 0, sizeof(*pScene));
}

void BrTrkSceneBind(const BrTrkScene *pScene, BrDl *pDl)
{
    BrDlAddRegion(pDl, BR_TRKSCENE_ARENA_BASE, pScene->pArena,
                  pScene->cArena * sizeof(float));
    BrDlAddRegion(pDl, BR_TRK_N64_BASE, pScene->track.pbImage,
                  pScene->track.cbImage);
}

const uint8_t *BrTrkSceneResolve(const BrTrkScene *pScene, uint32_t addr,
                                 size_t *pcbAvail)
{
    uint32_t off;

    if (pcbAvail != NULL)
        *pcbAvail = 0;
    if (pScene == NULL || pScene->track.pbImage == NULL)
        return NULL;
    off = br_trk_n64_to_off(pScene, addr);
    if (off == 0)
        return NULL;
    if (pcbAvail != NULL)
        *pcbAvail = pScene->track.cbImage - off;
    return pScene->track.pbImage + off;
}

/* ---------------------------------------------------------------------
 * The state the track lists do not carry
 * ---------------------------------------------------------------------
 * A .TRK display list contains only G_VTX, G_TRI1, G_TRI2, G_ENDDL and
 * texture setup -- measured, and stated in br_trkscene.h.  Every other
 * register is left where the frame set-up put it, and the frame set-up is
 * BrSceneSetupFrame (0x100180B0 D3D == 0x10015630 Glide), already ported in
 * port/src/slice2_15.c.  Its last three geometry-mode commands are:
 *
 *      BrGfxEmit(0xB7000000, bitsB7);   0x2000 == CULL_BACK, normally
 *      BrGfxEmit(0xB6000000, bitsB6);   0x1000 == CULL_FRONT, cleared
 *      BrGfxEmit(0xB6000000, 0x000C0000);   clear TEXTURE_GEN | _LINEAR
 *      BrGfxEmit(0xB7000000, 0x00020205);   set ZBUFFER|SHADE|SMOOTH|LIGHTING
 *
 * (0xB7 sets bits, 0xB6 clears them; bitsB7/bitsB6 swap when the rear-view
 * mirror flag differs, which is the g_6C3364 != g_6C1174 test.)  So a track
 * is drawn ZBUFFERed, smooth-shaded and LIT, with back faces culled.
 *
 * The lit path matters and is not a stylistic choice: with G_LIGHTING the
 * G_VTX handler is swapped for a lighting transform (br_dl.h, PART 4) that
 * reads the Vtx's trailing three bytes as a NORMAL.  MEASURED on the shipped
 * files: all 8949 of race.trk's vertices and 23077 of desert.trk's 23081
 * have |(b12,b13,b14)| == 127 +/- 6, i.e. they are unit normals, not vertex
 * colours.  Clearing LIGHTING would feed those bytes to the rasteriser as a
 * colour and the track would come out in garbage hues.
 *
 * The lights are the one Lights1 blob in the image, 0x100A9FF0, whose bytes
 * br_dl.h already records:  a warm white directional (238,238,204) along
 * (+84,+84,+84) and a dim cool ambient (51,51,64).  The G_MOVEMEM pair puts
 * the LIGHT (0x100A9FF8) in slot 0 and the AMBIENT (0x100A9FF0) in slot 1,
 * and G_MOVEWORD numlight w1 == 0x80000040 gives (w1 >> 5) & 0xF == 2.
 *
 * NOTE, so nobody reads more into the cull bits than is there: neither
 * br_dl.c nor the Metal backend acts on CULL_FRONT/CULL_BACK today.  They
 * are set because the original sets them, and because the depth buffer makes
 * the image correct without them. */
void BrTrkSceneSetState(BrDl *pDl)
{
    static const uint8_t kLight[16] =
        { 0xEE, 0xEE, 0xCC, 0x00, 0xEE, 0xEE, 0xCC, 0x00,
          0x54, 0x54, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t kAmbient[16] =
        { 0x33, 0x33, 0x40, 0x00, 0x33, 0x33, 0x40, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    pDl->geoModePrev = pDl->geoMode;
    pDl->geoMode = BR_DL_GEO_ZBUFFER | BR_DL_GEO_SHADE
                 | BR_DL_GEO_SHADING_SMOOTH | BR_DL_GEO_LIGHTING
                 | BR_DL_GEO_CULL_BACK;

    memcpy(pDl->aLight[BR_DL_LIGHT_DIFFUSE], kLight, sizeof(kLight));
    memcpy(pDl->aLight[BR_DL_LIGHT_AMBIENT], kAmbient, sizeof(kAmbient));
    pDl->nLights = 2;
    pDl->fLightCached = 0;
}

/* ---------------------------------------------------------------------
 * The camera
 * ---------------------------------------------------------------------
 * BrCamMatrixSetup (0x10033E83 D3D == 0x1002D534 Glide, ported at
 * port/src/slice2_19.c:144) with its last two lines dropped -- those
 * allocate a 64-byte pool slot and copy the matrix into it so the RSP can be
 * given an address, and there is no RSP here.  Everything above that is
 * reproduced call for call, including the two constants:
 *
 *      fovy   = fovRad * (4/3) * (cy/cx) * (180/pi)      DEGREES
 *      aspect = cx / cy
 *      near   = 0.8f                        literal 0x3F4CCCCD
 *      far    = pPose->farClip              0x100AA8B0 == 400.0f in the race
 *
 * -- note the a5/a4 in the fovy line against the a4/a5 in the aspect line.
 * Both are in the original; at 4:3 the (4/3)*(cy/cx) factor is exactly 1, so
 * fovy degrees == fovRad * 180/pi and the horizontal fov is what is held
 * fixed on other aspect ratios.
 *
 * The viewport is the one 0x10032C38 (BrViewportSetFull) emits for a 640x480
 * target: vscale = {1280, 960, 511, 0}, vtrans = {1280, 960, 511, 0}, i.e.
 * the N64 10.2 form of (cx/2, cy/2) scale and (cx/2, cy/2) translate.  The Y
 * scale is negated here because the N64 viewport's Y runs the other way,
 * which is the same sign BrDlInit installs. */
void BrTrkSceneCamera(BrDl *pDl, const BrTrkPose *pPose,
                      int32_t cx, int32_t cy)
{
    /* 0x1008F518 / 0x1008F51C in BRD3D.dll == 0x100774E0 / 0x100774E4 in
     * BRGlide.dll.  Both images: 1.3333334f and 57.2957764f. */
    const float k4_3 = 1.3333333730697632f;
    const float kRadToDeg = 57.2957763671875f;
    BrMat4 view, proj;
    unsigned short perspNorm = 0;
    float fovy, a4 = (float)cx, a5 = (float)cy;

    BrMat4LookAt(&view,
                 pPose->eye.x, pPose->eye.y, pPose->eye.z,
                 pPose->eye.x + pPose->fwd.x,
                 pPose->eye.y + pPose->fwd.y,
                 pPose->eye.z + pPose->fwd.z,
                 pPose->up.x, pPose->up.y, pPose->up.z);

    fovy = pPose->fovRad * k4_3;
    fovy = fovy * (a5 / a4);
    fovy = fovy * kRadToDeg;

    BrMat4Perspective(&proj, &perspNorm, fovy, a4 / a5, 0.8f, pPose->farClip);

    BrMat4Mul(&view, &proj, &pDl->proj);

    BrDlSetViewport(pDl, (float)cx * 0.5f, (float)cx * 0.5f,
                    (float)cy * -0.5f, (float)cy * 0.5f);
}

/* ---------------------------------------------------------------------
 * A pose from the track header -- HOW FAR THIS IS SUPPORTED
 * ---------------------------------------------------------------------
 * WHAT IS ESTABLISHED: header +0x40/+0x44/+0x48 is a point ON THE DRIVABLE
 * SURFACE.  Test: the nearest vertex of the collision mesh (header +0x14,
 * the array br_track.c's BrTrackVertex reads) to that point is 6.85 units
 * away in race.trk and 5.45 in desert.trk, on tracks whose collision mesh
 * spans roughly 900 x 940 x 119 and 1950 x 1790 x 250 units.  A junk triple
 * does not land on the road twice.  Corroboration from the two fields just
 * before it: +0x38/+0x3C hold -5.43/113.69 in race.trk against a measured
 * collision-mesh Z range of -5.4/113.7, so that pair is the world's Z
 * extent, which also settles that Z IS UP in track space (X and Y run 0..2048,
 * which is the 64x64 collision grid the header's +0x20/+0x24 tables index).
 *
 * WHAT IS A READING, NOT A PROOF: that +0x4C is a HEADING IN RADIANS.  It
 * holds 1.5602 in race.trk and 1.7522 in desert.trk -- both close to pi/2,
 * both in range for an angle and out of range for most other things -- and
 * the field sits immediately after the position in a header whose other
 * float groups are plainly (x, y, x, y, 30.0) gate records.  No reader for it
 * has been found in either build.  If the rendered image faces along the road
 * that is evidence for it; if it faces across the road, this is the field to
 * doubt first.
 *
 * WHAT IS PURELY THE HARNESS: `back`, `up`, `pitchDeg` and `fovRad`.  In the
 * game the eye comes from the chase-camera rig (slice2_11.h: the active
 * BrCamBasis is *(car + 0x2734), its fov is at +0x40) and there is no car in
 * a still frame.  The fov FIELD is established -- 0x10014C90 pushes
 * [pCam + 0x40] as BrCamMatrixSetup's a2, in radians -- but its runtime VALUE
 * is not: no initialiser for it was found, so the number a caller passes here
 * is the harness's choice and must be labelled as such. */
void BrTrkSceneStartPose(const BrTrkScene *pScene, BrTrkPose *pPose,
                         float back, float up, float pitchDeg, float fovRad)
{
    const float kDeg = 3.14159265358979f / 180.0f;
    float hdg, cp, sp;
    union { uint32_t u; float f; } cv;

    cv.u = BrTrackHdrU32(&pScene->track, BR_TRK_H_STARTHDG); hdg = cv.f;
    cp = cosf(pitchDeg * kDeg);
    sp = sinf(pitchDeg * kDeg);

    pPose->fwd.x = cosf(hdg) * cp;
    pPose->fwd.y = sinf(hdg) * cp;
    pPose->fwd.z = -sp;

    cv.u = BrTrackHdrU32(&pScene->track, BR_TRK_H_STARTX);
    pPose->eye.x = cv.f - cosf(hdg) * back;
    cv.u = BrTrackHdrU32(&pScene->track, BR_TRK_H_STARTY);
    pPose->eye.y = cv.f - sinf(hdg) * back;
    cv.u = BrTrackHdrU32(&pScene->track, BR_TRK_H_STARTZ);
    pPose->eye.z = cv.f + up;

    pPose->up.x = 0.0f;
    pPose->up.y = 0.0f;
    pPose->up.z = 1.0f;

    pPose->fovRad  = fovRad;
    /* 0x100AA8B0, the far plane the single-view call site passes. */
    pPose->farClip = 400.0f;
}

/* ---------------------------------------------------------------------
 * The +0x50 root is the SKY, and this is the evidence
 * ---------------------------------------------------------------------
 * The root list is not part of the track surface: its vertices are a small
 * shell around the ORIGIN -- |v| between 24.8 and 75.7 in race.trk, 6.0 and
 * 25.5 in desert.trk, with 73% / 84% of them at z >= 0 -- while every
 * instanced piece sits somewhere in the 2048 x 2048 world.  Drawn at
 * identity it lands in the corner of the map and is never in frame.
 *
 * BrSceneSetupFrame (slice2_15.c, 0x100180B0 D3D == 0x10015630 Glide) draws
 * exactly one thing between its G_MTX pair and its G_POPMTX, and it draws it
 * under a modelview that is a pure TRANSLATION TO THE CAMERA:
 *
 *      BrSub_10031140(&mtx, cam->eye.x, cam->eye.y, cam->eye.z * 0.99f)
 *      ... 0x01040040 G_MTX modelview MUL|PUSH with that matrix ...
 *      0xB6 clear 0x000F0205   ZBUFFER|SHADE|SMOOTH|FOG|LIGHTING|TEXGEN(2)
 *      0x06 G_DL  <the sky list>
 *      0xB7 set   0x00020205   ZBUFFER|SHADE|SMOOTH|LIGHTING, restored
 *      0xBD G_POPMTX
 *
 * -- which is the textbook skybox: centred on the eye, no depth, no lights.
 * Applied to the +0x50 root the picture becomes a cloudy sky with a distant
 * hill horizon, sitting behind the track instead of in front of it.  That is
 * the confirmation; the geometry alone only said "small shell".
 *
 * WHAT IS **NOT** ESTABLISHED, and must not be read into this: that the PC
 * builds ever draw it.  BrSceneSetupFrame takes the sky path only when
 * g_scene.f6C7C98 (0x106C7C98 D3D == 0x106EED28 Glide) is non-zero, and a
 * byte search of both images' .text finds only TWO references to that
 * dword -- 0x1001560C inside BrSceneUsePlainClear and 0x10015A8F inside
 * BrSceneSetupFrame -- and both are READS.  Nothing writes it, so
 * BrSceneUsePlainClear always returns 1 and the PC builds always take the
 * flat-fill branch instead.  The sky dome is the N64 build's, shipped in the
 * .TRK and left unused; drawing it here is the harness choosing to. */
void BrTrkSceneRun(const BrTrkScene *pScene, BrDl *pDl, const BrTrkPose *pPose)
{
    uint32_t i;
    uint32_t geoTrack = pDl->geoMode;
    /* BrSceneSetupFrame clears 0x000F0205 for its sky -- ZBUFFER, SHADE,
     * SHADING_SMOOTH, FOG, LIGHTING and both TEXTURE_GEN bits.  ONLY THE
     * ZBUFFER BIT IS TAKEN HERE, and the departure is deliberate:
     *
     *  - ZBUFFER is what makes a skybox a skybox, and nothing else in the
     *    interpreter can express "behind everything".  Kept.
     *  - LIGHTING is kept because the bits that clear it belong to a
     *    DIFFERENT LIST.  g_scene.f6C7C98 is the list BrSceneSetupFrame
     *    draws, and this build never sets it (see the comment above); the
     *    +0x50 root is a .TRK payload that the .TRK fixup registers through
     *    exactly the same BrDlRegister + texture-scan pair as every instance
     *    list, so the state that fits it is the state its siblings get.  It
     *    also has the same DATA: all 941 of race.trk's root vertices carry
     *    unit normals in their trailing three bytes, like the other 8008.
     *    Clearing LIGHTING sends those normals to the colour slots instead
     *    (br_dl.h says the unlit transform copies them verbatim), and the
     *    sky comes out tinted magenta by its own surface normals -- which is
     *    an artefact of the port's colour convention, not a rendering the
     *    original would produce either.
     *
     * There is no ground truth to match here, because the PC builds do not
     * draw a sky at all.  This is the harness choosing the reading that puts
     * the file's own data on screen; it is labelled rather than hidden. */
    uint32_t geoSky = geoTrack & ~BR_DL_GEO_ZBUFFER;

    for (i = 0; i < pScene->cDraw; ++i) {
        const BrTrkDraw *pD = &pScene->aDraw[i];
        BrMat4 sky;

        if (pD->iInstance < 0 && pPose != NULL) {
            BrMat4Translate(&sky, pPose->eye.x, pPose->eye.y,
                            pPose->eye.z * 0.99f);   /* 0x100772FC == 0.99f */
            pDl->geoMode = geoSky;
            pDl->iModel = 1;
            pDl->aModel[1] = sky;
            pDl->fLightCached = 0;
            BrMat4Mul(&pDl->aModel[1], &pDl->proj, &pDl->combined);
            BrDlRun(pDl, pScene->track.pbImage + pD->listOff,
                    pScene->track.cbImage - pD->listOff);
            pDl->geoMode = geoTrack;                 /* 0xB7 0x00020205 */
            continue;
        }

        /* What the G_MTX handler does for a MODELVIEW LOAD: install the
         * matrix in the current slot, invalidate the derived light state
         * (0x1002116E writes 0 to 0x105D17D0), and recombine.  iModel is
         * held at 1 for the same reason BrDlInit holds it there -- 0 is the
         * "no modelview" sentinel and the lighting transform reads the
         * modelview to bring the light direction into model space. */
        pDl->iModel = 1;
        pDl->aModel[1] = pD->model;
        pDl->fLightCached = 0;
        BrMat4Mul(&pDl->aModel[1], &pDl->proj, &pDl->combined);

        BrDlRun(pDl, pScene->track.pbImage + pD->listOff,
                pScene->track.cbImage - pD->listOff);
    }
}
