/* br_dlscene.h -- load a retail .rca and present its display lists to br_dl.
 *
 * NOT IN THE ORIGINAL, and deliberately so.  The game's own container index
 * -- whatever tells it where a car's display lists start -- has not been
 * located, so this module recovers the runs by signature the way
 * test_br_dl.c already did, and does it in ONE place so that the software
 * rasteriser and the Metal backend are fed byte-for-byte the same geometry.
 * That is the whole point: a comparison between two renderers is only
 * evidence if both were handed the same input.
 *
 * What IS from the original:
 *   - the .rca address mapping (file 0x8000 == N64 0x803C8000, CONVENTIONS.md)
 *   - BrDlPatch (0x10019040) does the byte-swap and the index halving
 *   - BrVtxCacheResolve (0x10018E10 Glide == 0x1002BD50 D3D) does the
 *     N64-Vtx -> eight-float expansion, called through the patch pass's
 *     resolve hook exactly as the Glide build calls it
 *
 * What is scaffolding, and is marked so at the site:
 *   - run discovery by scanning for a G_VTX whose address word is KSEG0
 *   - the view matrix.  These lists carry no G_MTX, so without one the
 *     combined matrix stays identity and every vertex falls outside the unit
 *     frustum.  BrDlSceneView builds a real perspective view with the game's
 *     own guPerspectiveF (0x10030930) so the model is seen, not squashed.
 */
#ifndef BR_DLSCENE_H
#define BR_DLSCENE_H

#include <stdint.h>
#include <stddef.h>

#include "br_dl.h"
#include "br_tex3d.h"

/* CONVENTIONS.md: ".rca: N64 struct at file 0x8000, N64 address 0x803C8000." */
#define BR_DLSCENE_FILE_BASE   0x8000u
#define BR_DLSCENE_N64_BASE    0x803C8000u
/* The expanded-vertex arena gets a 32-bit address of its own, because
 * br_dl.h resolves display-list addresses through a region table rather than
 * casting them to host pointers. */
#define BR_DLSCENE_ARENA_BASE  0x40000000u

#define BR_DLSCENE_MAX_RUNS    4096

struct BrVtxCache;

typedef struct BrDlScene {
    uint8_t  *pFile;
    size_t    cbFile;

    float    *pArena;         /* expanded 8-float vertex records */
    size_t    cArena;         /* floats used */
    size_t    cArenaMax;
    struct BrVtxCache *pCache;

    uint32_t  aRun[BR_DLSCENE_MAX_RUNS];   /* file offsets of each list */
    size_t    cRuns;

    float     bbMin[3], bbMax[3];          /* model bounds, arena space */
    uint32_t  cVerts;
    int       fEndOk;                      /* every run stopped at G_ENDDL */

    /* The load-time texture pass (br_tex3d.h, 0x10028820).  It runs over
     * each list immediately after BrDlPatch, exactly where the original
     * runs it -- the renderer vtable slot 0x118ED1DC is called from the
     * .rca fixup 0x10030770, not from the draw loop.  Without it a shipped
     * .rca contains no 0xDC at all and the whole texture path is dead. */
    BrTex3d   tex;
} BrDlScene;

/* Read the file, find every display-list run, patch each exactly once and
 * expand its vertices.  Returns 0 on success. */
int  BrDlSceneLoad(BrDlScene *pScene, const char *pszPath);
void BrDlSceneFree(BrDlScene *pScene);

/* Register the arena and the file image with pDl's address table. */
void BrDlSceneBind(const BrDlScene *pScene, BrDl *pDl);

/* Turn a 32-bit display-list address into a pointer into the loaded image,
 * or NULL.  The texture pass records texel and palette addresses in the
 * list's own 32-bit space (br_dl.h explains why the port keeps them there
 * rather than rewriting them to host pointers), so a caller that wants the
 * PIXELS needs this.  `*pcbAvail` receives how much of the image follows. */
const uint8_t *BrDlSceneResolve(const BrDlScene *pScene, uint32_t addr,
                                size_t *pcbAvail);

/* SCAFFOLDING.  Set pDl->combined to a perspective view that frames the
 * model, and the viewport to a cx-by-cy target.  Angles in degrees; yaw is
 * about the model's Y axis, pitch about X. */
void BrDlSceneView(const BrDlScene *pScene, BrDl *pDl,
                   float yawDeg, float pitchDeg, float fovDeg,
                   int32_t cx, int32_t cy);

/* Walk every run through pDl, in file order. */
void BrDlSceneRun(const BrDlScene *pScene, BrDl *pDl);

#endif /* BR_DLSCENE_H */
