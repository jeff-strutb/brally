/* br_trkscene.h -- present a retail .TRK's display lists to br_dl, and place
 * the camera the way the game places it.
 *
 * WHY THIS EXISTS
 * ----------------------------------------------------------------------
 * br_dlscene.h does this for a .rca (one car).  Nothing did it for a TRACK,
 * and a track is not a car with more triangles: a .rca is a bare pile of
 * display lists that carry no transform at all, while a .TRK is a SCENE --
 * an array of instances, each with its own 4x4 and its own list, plus one
 * root list -- and it is drawn through a projection matrix the game builds
 * on the host and hands to the RSP already multiplied.  So this module has
 * two jobs the .rca one does not: walk the instance array, and establish a
 * camera.
 *
 * WHERE THE LISTS ARE, READ OFF THE ORIGINAL AND NOT GUESSED
 * ----------------------------------------------------------------------
 * The .TRK fixup pass is 0x10037E10 (D3D) == 0x100314D0 (Glide), and
 * port/src/slice2_20.c already transcribes it.  Two -- and only two --
 * places in that function hand a pointer to BrDlRegister (== BrDlPatch,
 * 0x1002BF80 D3D == 0x10019040 Glide) and then to the backend's load-time
 * texture scan g_pfn18AA0C4 (== 0x10028820 == BrTex3dScan):
 *
 *   slice2_20.c:835  BrDlRegister(BrPtrAt(h + 0x50));      the ROOT list
 *                    g_pfn18AA0C4(BrPtrAt(h + 0x50));
 *   slice2_20.c:655  BrTrackFixupRec54: BrFixupAt(p + 0x44) -- "the only
 *                    rebased field" of a 0x54-byte instance record --
 *                    then BrDlRegister(pDl); g_pfn18AA0C4(pDl);
 *
 * So header +0x50 is ONE display list (not an array: nothing indexes off
 * it, nothing loops), and instance +0x44 is one display list per instance.
 * Everything else in the header is geometry for the collision grid, the
 * texture table (+0x1C, stride 0x24) or the u16 side tables.
 *
 * Measured on the shipped files, walking those roots to G_ENDDL:
 *
 *   race.trk    386 instances, 369 with a list, 369 distinct + the root
 *               9800 commands, 5873 triangles, 8949 G_VTX vertices in
 *               505 distinct vertex blocks, 151 distinct G_SETTIMG
 *   desert.trk  804 instances, 677 with a list, 677 distinct + the root
 *               22365 commands, 13064 triangles, 23081 vertices
 *
 * and the lists carry ONLY 0x04/0xB1/0xBF/0xB8 plus texture setup
 * (0xFD/0xE6/0xE8/0xF3/0xF5/0xF2).  No G_MTX, no G_SETCOMBINE, no
 * G_SETGEOMETRYMODE, no G_MOVEMEM, no nested G_DL.  Every piece of render
 * state therefore comes from OUTSIDE the file -- which is exactly why the
 * camera had to be found in the binary before a track could be drawn.
 *
 * THE CAMERA, AND WHICH HALF OF IT IS THE GAME'S
 * ----------------------------------------------------------------------
 * BrCamMatrixSetup (0x10033E83 D3D == 0x1002D534 Glide, ported in
 * port/src/slice2_19.c) is the whole of it, and the per-frame race render
 * 0x10014A30 calls it once per view:
 *
 *      0x10014C90  push [pCam + 0x40]      a2 = fov, RADIANS
 *      0x10014C8F  push [0x100AA8B0]       a3 = far  == 400.0f
 *      0x10014C88  push (float)view.w      a4
 *      0x10014C81  push (float)view.h      a5
 *      0x10014C92  call 0x10033E83
 *
 * and inside:
 *
 *      view  = guLookAtF(eye = pCam+0x30,
 *                        at  = pCam+0x30 + pCam+0x00,
 *                        up  = pCam+0x20)              0x100309A0
 *      fovy  = a2 * (4/3) * (a5/a4) * (180/pi)         degrees
 *      proj  = guPerspective(fovy, a4/a5, 0.8f, a3, 1.0f)   0x10030930
 *      combined = view * proj                          0x100306C0
 *
 * and `combined` is what goes into the RSP's PROJECTION slot as
 * G_MTX 0x01030040 (0x1001822D, in BrSceneSetupFrame).  THE RSP NEVER SEES
 * THE VIEW MATRIX SEPARATELY -- the modelview slot is per-object, which is
 * why an instance's 4x4 can go straight into it here.
 *
 * ERRATUM FOUND ON THE WAY, AND FIXED: slice2_19.c carried
 * g_BrK08F518 = g_BrK08F51C = 1.0f marked "ASSUMED".  They are 0x1008F518
 * and 0x1008F51C in BRD3D.dll and 0x100774E0 / 0x100774E4 in BRGlide.dll,
 * and both images read 1.3333334f and 57.2957764f -- 4/3 and 180/pi.  With
 * the assumed 1.0f the field of view came out about 0.75 DEGREES.
 *
 * WHAT IS THE HARNESS'S, SAID PLAINLY
 * ----------------------------------------------------------------------
 * The eye position, the look direction and the numeric fov are NOT from the
 * binary.  In the game they live in the chase-camera rig that hangs off the
 * car (slice2_11.h; the active basis is *(car + 0x2734), fov at +0x40) and
 * there is no car here.  BrTrkSceneStartPose supplies a pose from the track
 * header instead -- see its comment for how far that reading is supported.
 * The PIPELINE that turns a pose into a matrix is the game's, verbatim.
 */
#ifndef BR_TRKSCENE_H
#define BR_TRKSCENE_H

#include <stdint.h>
#include <stddef.h>

#include "br_dl.h"
#include "br_mat.h"
#include "br_tex3d.h"
#include "br_track.h"

/* The expanded-vertex arena gets a 32-bit address of its own, for the same
 * reason br_dlscene.h gives: br_dl.c resolves display-list addresses through
 * a region table rather than casting them to host pointers.  The image
 * itself answers to BR_TRK_N64_BASE, so this must not overlap it. */
#define BR_TRKSCENE_ARENA_BASE 0x40000000u

/* Header fields this module reads that br_track.h does not name, because
 * nothing had a use for them.  +0x50 is the display-list root (see above);
 * the rest are established in BrTrkSceneStartPose's comment. */
#define BR_TRK_H_DLROOT    0x50u
#define BR_TRK_H_WORLDMINZ 0x38u
#define BR_TRK_H_WORLDMAXZ 0x3Cu
#define BR_TRK_H_STARTX    0x40u
#define BR_TRK_H_STARTY    0x44u
#define BR_TRK_H_STARTZ    0x48u
#define BR_TRK_H_STARTHDG  0x4Cu

/* The instance record's display-list pointer.  slice2_20.c:655 calls +0x44
 * "the only rebased field" of the 0x54-byte record, which is what makes it
 * a pointer at all. */
#define BR_TRK_INST_DLOFF  0x44u

/* One thing to draw: a modelview and a list. */
typedef struct BrTrkDraw {
    BrMat4   model;      /* the instance 4x4; identity for the root list  */
    uint32_t listOff;    /* file offset of the patched list               */
    int32_t  iInstance;  /* -1 for the +0x50 root                         */
} BrTrkDraw;

struct BrVtxCache;

typedef struct BrTrkScene {
    BrTrack   track;

    float    *pArena;          /* expanded 8-float vertex records */
    size_t    cArena, cArenaMax;
    struct BrVtxCache *pCache;

    BrTrkDraw *aDraw;
    uint32_t   cDraw, cDrawMax;

    uint32_t  cLists;          /* distinct lists patched            */
    uint32_t  cListsBad;       /* pointers that did not resolve     */
    uint32_t  cCommands;       /* commands BrDlPatch touched        */
    int       fEndOk;          /* every list stopped at G_ENDDL     */
    uint32_t  cVerts;          /* distinct expanded vertices        */

    BrTex3d   tex;             /* the load-time texture pass */

    /* Bounds of the COLLISION mesh (header +0x14), i.e. the drivable
     * surface, in world units.  BrTrackBounds' output, kept here so a
     * caller framing a camera does not have to re-walk it. */
    BrVec3    bbMin, bbMax;
} BrTrkScene;

/* A camera pose in the shape BrCamMatrixSetup wants.  Deliberately the
 * same three vectors and the same fov unit (RADIANS) as slice2_19.h's
 * BrCamBasis, so a caller that later has a real BrCamBasis can hand its
 * fields straight over. */
typedef struct BrTrkPose {
    BrVec3 eye;
    BrVec3 fwd;
    BrVec3 up;
    float  fovRad;
    float  farClip;
} BrTrkPose;

/* Open the file, walk +0x50 and every instance's +0x44, patch each distinct
 * list exactly once, run the load-time texture pass over it and expand its
 * vertices.  Returns 0 on success. */
int  BrTrkSceneLoad(BrTrkScene *pScene, const char *pszPath);
void BrTrkSceneFree(BrTrkScene *pScene);

/* Register the image and the arena with pDl's address table. */
void BrTrkSceneBind(const BrTrkScene *pScene, BrDl *pDl);

/* Turn a 32-bit display-list address into a pointer into the image, or
 * NULL.  `*pcbAvail` receives how much of the image follows. */
const uint8_t *BrTrkSceneResolve(const BrTrkScene *pScene, uint32_t addr,
                                 size_t *pcbAvail);

/* The render state the track lists do NOT carry and BrSceneSetupFrame does.
 * See br_trkscene.c for the command-by-command derivation. */
void BrTrkSceneSetState(BrDl *pDl);

/* BrCamMatrixSetup's body, minus its last two lines (the pool allocation and
 * the copy into it, which only exist so the RSP can be handed an address).
 * Sets pDl->proj to view*projection and the viewport to a cx-by-cy target.
 *
 * `cx`/`cy` are the view rectangle 0x10014C81/0x10014C88 pass as a4/a5. */
void BrTrkSceneCamera(BrDl *pDl, const BrTrkPose *pPose,
                      int32_t cx, int32_t cy);

/* A pose from the track header.  READ THE COMMENT AT THE DEFINITION before
 * quoting this as the game's camera -- the POSITION is the header's, the
 * height offset, the pitch and the fov are the harness's. */
void BrTrkSceneStartPose(const BrTrkScene *pScene, BrTrkPose *pPose,
                         float back, float up, float pitchDeg, float fovRad);

/* Draw every instance, in the order the original registers them: the +0x50
 * root first, then the instance array in index order.
 *
 * `pPose` is the pose BrTrkSceneCamera was given.  It is needed because the
 * +0x50 root is a SKY DOME and BrSceneSetupFrame draws the sky under a
 * modelview translated to the eye -- see the comment at the definition,
 * which also says what part of that is NOT established.  Pass NULL to draw
 * the root at identity instead, i.e. wherever the file puts it. */
void BrTrkSceneRun(const BrTrkScene *pScene, BrDl *pDl,
                   const BrTrkPose *pPose);

#endif /* BR_TRKSCENE_H */
