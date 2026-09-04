/* br_cammatrix.c -- drawing: the camera transforms a frame draws through.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_19.c, an address batch and not a module.  Three ways
 * of setting the camera up -- aimed at a target, fixed, and flat/orthographic
 * -- each ending in the display-list commands that install the result.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* Both display-list emitters below inline this in the original: take the
 * write cursor, advance it by 8 bytes, and fill the two words. */
static uint32_t *BrGfxTake2(void)
{
    uint32_t *p = g_BrGfxPtr;
    g_BrGfxPtr += 2;
    return p;
}

/* WHAT IT DOES: points the camera at what it is looking at and sets the lens,
 * then combines the two into the single transform everything in the world is
 * drawn through, and parks a copy of it where the renderer will find it.
 * Anything nearer than a fixed close distance, or further than the caller's
 * limit, is cut off. */
/* @implements 0x10033E83 d3d BrCamMatrixSetup */
/* @n64 0x8021B2F8 located */
#ifdef BR_MATCHING_BUILD
/* /Od: no locals at all -- the fovy chain is inline in the call (a named
 * local would cost a frame slot); pool alloc and matrix store direct. */
extern BrMat4 *BrSub_10069490(void);            /* glide 0x10062500 */
extern void BrGuMtxStore(const int pSrc[4][4], int pDst[4][4]);

void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5)
{
    BrMat4LookAt(&g_BrViewMat,
                 pCam->eye.x, pCam->eye.y, pCam->eye.z,
                 pCam->eye.x + pCam->fwd.x,
                 pCam->eye.y + pCam->fwd.y,
                 pCam->eye.z + pCam->fwd.z,
                 pCam->up.x, pCam->up.y, pCam->up.z);

    g_BrCamFar  = a3;
    g_BrCamNear = 0.8f;   /* the literal 0x3F4CCCCD */

    /* ((a2 * K518) * (a5 / a4)) * K51C -- note a5/a4 here but a4/a5 as the
     * aspect. Both are in the original. */
    BrMat4Perspective7(&g_BrProjMat, &g_BrPerspNorm,
                       a2 * g_BrK08F518 * (a5 / a4) * g_BrK08F51C,
                       a4 / a5, g_BrCamNear, g_BrCamFar, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMat, &g_BrCurMat);

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);
}
#else
void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5)
{
    float fovy;

    BrMat4LookAt(&g_BrViewMat,
                 pCam->eye.x, pCam->eye.y, pCam->eye.z,
                 pCam->eye.x + pCam->fwd.x,
                 pCam->eye.y + pCam->fwd.y,
                 pCam->eye.z + pCam->fwd.z,
                 pCam->up.x, pCam->up.y, pCam->up.z);

    g_BrCamFar  = a3;
    g_BrCamNear = 0.8f;   /* the literal 0x3F4CCCCD, stored to 0x106C3360 */

    /* ((a2 * K518) * (a5 / a4)) * K51C -- note a5/a4 here but a4/a5 as the
     * aspect two lines down. Both are in the original. */
    fovy = a2 * g_BrK08F518;
    fovy = fovy * (a5 / a4);
    fovy = fovy * g_BrK08F51C;

    BrMat4Perspective7(&g_BrProjMat, &g_BrPerspNorm,
                       fovy, a4 / a5, g_BrCamNear, g_BrCamFar, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMat, &g_BrCurMat);

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);   /* source first */
}
#endif

/* WHAT IT DOES: sets up a fixed camera looking straight at a flat scene at a
 * fixed distance -- what the menus and other flat screens are drawn through --
 * and issues the drawing commands that put that transform in force. The two
 * values it is passed are never looked at. */
/* @implements 0x10033F7E d3d BrCamMatrixSetupFixed */
/* @n64 0x8021B458 located */
#ifdef BR_MATCHING_BUILD
/* /Od TU: literal param self-assigns, the take-2 emit inlined per block
 * (own [ebp-N] slot each, globals re-read), the 0-arg pool alloc and the
 * matrix store called directly. Externs shared with BrCamMatrixSetup. */
void BrCamMatrixSetupFixed(float a1, float a2)
{
    a1 = a1;
    a2 = a2;

    BrMat4LookAt(&g_BrViewMat,
                 512.0f, 384.0f, 1000.0f,
                 512.0f, 384.0f,    0.0f,
                   0.0f,   1.0f,    0.0f);

    BrMat4Perspective7(&g_BrProjMatFixed, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMatFixed, &g_BrCurMat);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0xBC00000Eu;
        p_[1] = g_BrPerspNorm;
    }

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0x01030040u;
        p_[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
    }
}
#else
void BrCamMatrixSetupFixed(float a1, float a2)
{
    uint32_t *pCmd;

    (void)a1;
    (void)a2;

    BrMat4LookAt(&g_BrViewMat,
                 512.0f, 384.0f, 1000.0f,
                 512.0f, 384.0f,    0.0f,
                   0.0f,   1.0f,    0.0f);

    BrMat4Perspective7(&g_BrProjMatFixed, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrProjMatFixed, &g_BrCurMat);

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;      /* zero-extended from the u16 */

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}
#endif

/* WHAT IT DOES: sets up flat drawing with no perspective at all, mapping a
 * rectangle of the given width and height onto the screen with the origin at
 * one corner, and issues the commands that put it in force. Depth is thrown
 * away entirely, so nothing drawn this way can be in front of or behind
 * anything else. */
/* @implements 0x1003407D d3d BrCamMatrixSetupOrtho */
#ifdef BR_MATCHING_BUILD
/* Same /Od TU and same four idioms as BrCamMatrixSetupFixed above -- literal
 * param self-assigns, the take-2 emit inlined per block with its own [ebp-N]
 * slot and the cursor re-read, the 0-arg pool alloc, and the matrix store
 * called directly. Every other function between 0x1002C0F3 and 0x1002E13B is
 * already byte-exact under /Od; this one was written in the /O2 shape, which
 * is the whole 19-instruction gap. */
void BrCamMatrixSetupOrtho(float w, float h)
{
    w = w;
    h = h;

    g_BrCurMat.m[0][0] = g_BrK08F514 / w;
    g_BrCurMat.m[0][1] = 0.0f;
    g_BrCurMat.m[0][2] = 0.0f;
    g_BrCurMat.m[0][3] = 0.0f;
    g_BrCurMat.m[1][0] = 0.0f;
    g_BrCurMat.m[1][1] = g_BrK08F514 / h;
    g_BrCurMat.m[1][2] = 0.0f;
    g_BrCurMat.m[1][3] = 0.0f;
    g_BrCurMat.m[2][0] = 0.0f;
    g_BrCurMat.m[2][1] = 0.0f;
    g_BrCurMat.m[2][2] = 0.0f;   /* explicit; z is discarded, not passed on */
    g_BrCurMat.m[2][3] = 0.0f;
    g_BrCurMat.m[3][0] = -1.0f;
    g_BrCurMat.m[3][1] = -1.0f;
    g_BrCurMat.m[3][2] = 0.0f;
    g_BrCurMat.m[3][3] = 1.0f;

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0xBC00000Eu;
        p_[1] = g_BrPerspNorm;
    }

    g_BrMtxSlot = BrSub_10069490();
    BrGuMtxStore((const int (*)[4])&g_BrCurMat, (int (*)[4])g_BrMtxSlot);

    {
        uint32_t *p_ = g_BrGfxPtr;
        g_BrGfxPtr += 2;
        p_[0] = 0x01030040u;
        p_[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
    }
}
#else
void BrCamMatrixSetupOrtho(float w, float h)
{
    uint32_t *pCmd;

    g_BrCurMat.m[0][0] = g_BrK08F514 / w;
    g_BrCurMat.m[0][1] = 0.0f;
    g_BrCurMat.m[0][2] = 0.0f;
    g_BrCurMat.m[0][3] = 0.0f;
    g_BrCurMat.m[1][0] = 0.0f;
    g_BrCurMat.m[1][1] = g_BrK08F514 / h;
    g_BrCurMat.m[1][2] = 0.0f;
    g_BrCurMat.m[1][3] = 0.0f;
    g_BrCurMat.m[2][0] = 0.0f;
    g_BrCurMat.m[2][1] = 0.0f;
    g_BrCurMat.m[2][2] = 0.0f;   /* explicit; z is discarded, not passed on */
    g_BrCurMat.m[2][3] = 0.0f;
    g_BrCurMat.m[3][0] = -1.0f;
    g_BrCurMat.m[3][1] = -1.0f;
    g_BrCurMat.m[3][2] = 0.0f;
    g_BrCurMat.m[3][3] = 1.0f;

    pCmd = BrGfxTake2();
    pCmd[0] = 0xBC00000Eu;
    pCmd[1] = g_BrPerspNorm;

    g_BrMtxSlot = BrPoolAlloc(g_BrPool);
    BrMat4Copy(&g_BrCurMat, (BrMat4 *)g_BrMtxSlot);

    pCmd = BrGfxTake2();
    pCmd[0] = 0x01030040u;
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;
}
#endif
