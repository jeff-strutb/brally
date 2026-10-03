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
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#include "br_mat.h"   /* br_globals: its objects */
#include "slice2_19.h"
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 0x10AC0810 */
/* BrSub100088B0: prototype in br_funcs.h */
/* BrSegSetBases: prototype in br_funcs.h */
#undef BrRgbSinkSet

#include <string.h>
#include <math.h>
#include "br_camwide.h"

/* Both display-list emitters below inline this in the original: take the
 * write cursor, advance it by 8 bytes, and fill the two words. */
/* (port-only BrGfxTake2 removed) */


/* WHAT IT DOES: points the camera at what it is looking at and sets the lens,
 * then combines the two into the single transform everything in the world is
 * drawn through, and parks a copy of it where the renderer will find it.
 * Anything nearer than a fixed close distance, or further than the caller's
 * limit, is cut off. */
/* @implements 0x10033E83 d3d BrCamMatrixSetup */
/* @n64 0x8021B2F8 located */
/* /Od: no locals at all -- the fovy chain is inline in the call (a named
 * local would cost a frame slot); pool alloc and matrix store direct. */
/* BrSub_10069490: prototype in br_funcs.h */
/* BrGuMtxStore: prototype in br_funcs.h */

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

/* Port: the lens for a race view stretched over a window wider (or taller)
 * than the game's shape (br_camwide.h, as the wasm lane's native/aspect.m).
 * The game makes fovy = angle * K518 * (h / w) * K51C and aspect = w / h
 * from the view's rectangle; the view is given the rectangle it fills on the
 * target and the angle that keeps fovy (Hor+), or the fovy that keeps the
 * horizontal angle (Vert+). */
void BrCamMatrixSetupWide(const BrCamBasis *pCam, float angle, float far_, float w, float h)
{
    float kx, ky;
    double k, fovy, fovy2, w2, h2;
    plat_view_scale(&kx, &ky);
    if ((kx <= 1.0f && ky <= 1.0f) || w <= 0 || h <= 0) {
        BrCamMatrixSetup(pCam, angle, far_, w, h);
        return;
    }
    k = (double)g_BrK08F518 * g_BrK08F51C;
    fovy = angle * k * (h / w);
    w2 = w * kx;
    h2 = h * ky;
    fovy2 = ky > 1.0f ? 2.0 * atan(ky * tan(fovy * M_PI / 360.0)) * 180.0 / M_PI : fovy;
    BrCamMatrixSetup(pCam, (float)(fovy2 / (k * (h2 / w2))), far_, (float)w2, (float)h2);
}

/* WHAT IT DOES: sets up a fixed camera looking straight at a flat scene at a
 * fixed distance -- what the menus and other flat screens are drawn through --
 * and issues the drawing commands that put that transform in force. The two
 * values it is passed are never looked at. */
/* @implements 0x10033F7E d3d BrCamMatrixSetupFixed */
/* @n64 0x8021B458 located */
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

    BrMat4Perspective7(&g_BrDrawScale, &g_BrPerspNorm,
                       45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f);

    BrMat4Mul(&g_BrViewMat, &g_BrDrawScale, &g_BrCurMat);

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
        p_[1] = br_addr32(g_BrMtxSlot);
    }
}

/* WHAT IT DOES: sets up flat drawing with no perspective at all, mapping a
 * rectangle of the given width and height onto the screen with the origin at
 * one corner, and issues the commands that put it in force. Depth is thrown
 * away entirely, so nothing drawn this way can be in front of or behind
 * anything else. */
/* @implements 0x1003407D d3d BrCamMatrixSetupOrtho */
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
        p_[1] = br_addr32(g_BrMtxSlot);
    }
}
