/* br_entstate.c -- driving: the entity state setters.
 *
 * Filed out of the address batch slice3_45.c, whose header block this
 * preamble is copied from.  These four write the rigid-body state (st, stA,
 * stB) the physics integrator reads, so they live with the physics rather
 * than with the entity's position (0x10076420, scene) or its artwork
 * (0x10076A00 / 0x10076A40, drawing), which sit either side of them in the
 * original packet.
 *
 * See slice3_45.h for the per-function derivations and gotchas.
 */
#include <string.h>

#include "br_match.h"
#ifdef BR_MATCHING_BUILD
/* The entity setters are thiscall with three stack floats; hide the
 * port's cdecl prototypes so the twins can carry the fastcall shape. */
#define BrEntSetMatrix      BrEntSetMatrix_port
#define BrEntSetVel         BrEntSetVel_port
#define BrEntSetAngVel      BrEntSetAngVel_port
#define BrEntSetOrientation BrEntSetOrientation_port
#include "slice3_45.h"
#undef BrEntSetMatrix
#undef BrEntSetVel
#undef BrEntSetAngVel
#undef BrEntSetOrientation
#else
#include <math.h>
#include "slice3_45.h"
#endif

/* ====================================================================== */
/* Constants read out of orig/BRD3D.dll .rdata (do not re-derive)          */
/* ====================================================================== */

/* 0x1008FCA8 = 0x3F000000, exactly 0.5. The quaternion half-angle factor. */
static const float kBrHalf = 0.5f;

/* ====================================================================== */
/* Entity state setters                                                    */
/* ====================================================================== */

/* Shared tail of 0x10076700 and 0x10076820: mirror st.quat into stB then
 * stA. Written out in the originals; identical instruction sequence in both. */
static void BrEntMirrorQuat(BrEnt *pE)
{
    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;

    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;
}

/* 0x10076700 */
/* WHAT IT DOES: sets an object's position and facing wholesale from a
 * ready-made transform, and works the facing back out into the form the
 * physics stores. Note that it does NOT update the physics' idea of where the
 * object is, only which way it is turned, so the rebuilt transform ends up
 * with the new rotation but the old position. */
/* @implements 0x10076700 d3d BrEntSetMatrix */
/* @n64 0x80220150 located */
/* Thiscall with ONE stack argument (`mov ebx,ecx` then `[esp+4]`, `ret 4`),
 * spelled the way the rest of this file's entity setters are.  The
 * quaternion mirror is written out here rather than calling
 * BrEntMirrorQuat: VC5 does not inline the static helper, and the original
 * has the eight dword copies in line. */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetMatrix(BrEnt *pE, int _edx_unused, const BrMat4 *pSrc)
{
    (void)_edx_unused;

    /* `rep movsd` of 16 dwords. */
    memcpy(&pE->mat0, pSrc, sizeof(BrMat4));

    BrSub100765E0(pSrc, &pE->st.quat);

    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;

    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#else
void BrEntSetMatrix(BrEnt *pE, const BrMat4 *pSrc)
{
    /* `rep movsd` of 16 dwords. */
    memcpy(&pE->mat0, pSrc, sizeof(BrMat4));

    BrSub100765E0(pSrc, &pE->st.quat);
    BrEntMirrorQuat(pE);

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
#endif

/* 0x100767A0 */
/* WHAT IT DOES: tells an object how fast and in which direction it is
 * travelling, writing it into all four places the game keeps that figure so
 * they agree. Nothing else about the object is disturbed. */
/* @implements 0x100767A0 d3d BrEntSetVel */
/* @n64 0x802201C8 located */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetVel(BrEnt *pE, int _edx_unused, float x, float y,
                            float z)
{
    (void)_edx_unused;

    pE->st.vel.x = x;
    pE->st.vel.y = y;
    pE->st.vel.z = z;

    pE->stB.vel.x = x;
    pE->stB.vel.y = y;
    pE->stB.vel.z = z;

    pE->stA.vel.x = x;
    pE->stA.vel.y = y;
    pE->stA.vel.z = z;

    pE->f1024[0] = x;
    pE->f1024[1] = y;
    pE->f1024[2] = z;
}
#else
void BrEntSetVel(BrEnt *pE, float x, float y, float z)
{
    pE->st.vel.x = x;
    pE->st.vel.y = y;
    pE->st.vel.z = z;

    pE->stB.vel.x = x;
    pE->stB.vel.y = y;
    pE->stB.vel.z = z;

    pE->stA.vel.x = x;
    pE->stA.vel.y = y;
    pE->stA.vel.z = z;

    pE->f1024[0] = x;
    pE->f1024[1] = y;
    pE->f1024[2] = z;
}
#endif

/* 0x10076820 */
/* WHAT IT DOES: turns an object by three angles about its three axes. It
 * ADDS the rotation to however the object was already facing rather than
 * replacing it, and unlike the other setters here it leaves the object's
 * drawing transform stale until something else rebuilds it. */
/* @implements 0x10076820 d3d BrEntSetOrientation */
#ifdef BR_MATCHING_BUILD
/* thiscall + three stack floats; sin/cos are the float-arg tree wrappers
 * (sin FIRST per axis), quat built fresh each axis with immediate zeros. */
extern float BrSinF(float a);      /* glide 0x10002560 */
extern float BrCosF(float a);      /* glide 0x100023E0 */

void __fastcall BrEntSetOrientation(BrEnt *pE, int _edx_unused,
                                    float a1, float a2, float a3)
{
    float h1 = a1 * kBrHalf;
    float h2 = a2 * kBrHalf;
    float h3 = a3 * kBrHalf;
    BrVec4 q;

    (void)_edx_unused;

    {
        float sn = BrSinF(h1);
        q.f00 = BrCosF(h1);
        q.f04 = 0.0f;
        q.f08 = 0.0f;
        q.f0C = sn;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    {
        float sn = BrSinF(h2);
        q.f00 = BrCosF(h2);
        q.f04 = 0.0f;
        q.f08 = sn;
        q.f0C = 0.0f;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    {
        float sn = BrSinF(h3);
        q.f00 = BrCosF(h3);
        q.f04 = sn;
        q.f08 = 0.0f;
        q.f0C = 0.0f;
    }
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    BrVec4Normalise(&pE->st.quat);

    pE->stB.quat.f00 = pE->st.quat.f00;
    pE->stB.quat.f04 = pE->st.quat.f04;
    pE->stB.quat.f08 = pE->st.quat.f08;
    pE->stB.quat.f0C = pE->st.quat.f0C;
    pE->stA.quat.f00 = pE->st.quat.f00;
    pE->stA.quat.f04 = pE->st.quat.f04;
    pE->stA.quat.f08 = pE->st.quat.f08;
    pE->stA.quat.f0C = pE->st.quat.f0C;
}
#else
/* The port twin of 0x10076820; the tag above the #ifdef covers both arms. */
void BrEntSetOrientation(BrEnt *pE, float a1, float a2, float a3)
{
    /* All three half-angles are formed up front, before any call. */
    float h1 = a1 * kBrHalf;
    float h2 = a2 * kBrHalf;
    float h3 = a3 * kBrHalf;
    BrVec4 q;

    /* Z: (cos, 0, 0, sin) */
    q.f0C = sinf(h1);
    q.f00 = cosf(h1);
    q.f04 = 0.0f;
    q.f08 = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    /* Y: (cos, 0, sin, 0) */
    q.f08 = sinf(h2);
    q.f00 = cosf(h2);
    q.f04 = 0.0f;
    q.f0C = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    /* X: (cos, sin, 0, 0) */
    q.f04 = sinf(h3);
    q.f00 = cosf(h3);
    q.f08 = 0.0f;
    q.f0C = 0.0f;
    BrSub10074090(&pE->st.quat, &pE->st.quat, &q);

    BrVec4Normalise(&pE->st.quat);
    BrEntMirrorQuat(pE);
    /* No BrRbBuildMatrix here -- see the header. */
}
#endif

/* 0x100769A0 */
/* WHAT IT DOES: tells an object how fast it is spinning, writing it into all
 * three places the game keeps that figure so they agree. */
/* @implements 0x100769A0 d3d BrEntSetAngVel */
/* @n64 0x80220358 located */
#ifdef BR_MATCHING_BUILD
void __fastcall BrEntSetAngVel(BrEnt *pE, int _edx_unused, float x, float y,
                               float z)
{
    (void)_edx_unused;

    pE->st.angVel.x = x;
    pE->st.angVel.y = y;
    pE->st.angVel.z = z;

    pE->stB.angVel.x = x;
    pE->stB.angVel.y = y;
    pE->stB.angVel.z = z;

    pE->stA.angVel.x = x;
    pE->stA.angVel.y = y;
    pE->stA.angVel.z = z;
}
#else
void BrEntSetAngVel(BrEnt *pE, float x, float y, float z)
{
    pE->st.angVel.x = x;
    pE->st.angVel.y = y;
    pE->st.angVel.z = z;

    pE->stB.angVel.x = x;
    pE->stB.angVel.y = y;
    pE->stB.angVel.z = z;

    pE->stA.angVel.x = x;
    pE->stA.angVel.y = y;
    pE->stA.angVel.z = z;
}
#endif
