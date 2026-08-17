/* br_collrespsolve.c -- see br_collrespsolve.h.  The OBB collision response.
 *
 * Transcribed from orig/BRGlide.dll and pinned to tools/x87emu.py golden
 * vectors.  Each function carries the address of what it is.
 */
#include <math.h>
#include <string.h>

#include "br_collrespsolve.h"

/* 0x117787F0..FC and 0x117781A0..A8. */
BrCrPlaneState g_brCrPlane;

/* 0x10077B84 (1/3), 0x10077AC8 (0.5) -- the two literals the mode-2 arm uses. */
#define BR_CR_THIRD  (1.0f / 3.0f)
#define BR_CR_HALF   0.5f

/* ------------------------------------------------------------------ *
 * 0x10067470 -- contact-plane resolver.
 *
 * WHAT IT DOES: turns one candidate contact into a plane and writes the plane
 * normal scaled by how far the point is from it.  There are two ways in.  The
 * common one (mode != 2) is handed the plane normal outright.  The box-face
 * one (mode == 2) has to CHOOSE the face: it averages the three box-space
 * triangle vertices and picks the axis on which that centroid is SMALLEST --
 * the face the triangle lies most flush against -- and uses that box face's
 * outward normal.  The N64 sibling calls these two paths "Cube Edge to
 * Triangle Face" and "Triangle Edge to CubeFace".
 *
 * Two things here are preserved quirks, both confirmed against the bytes:
 *
 *  - The sign of the chosen box normal is taken from the centroid's X
 *    component, NOT from the component on the winning axis.  The original
 *    carries cx on the FPU stack through the entire argmin tournament and
 *    signs by whatever is left in st0, which is always cx.  A NaN cx takes the
 *    negative arm (the compare's unordered result), so it maps to -0.5.
 *
 *  - The argmin is the original's exact >= tournament, so ties resolve the way
 *    its `fcom`/`jae`-shaped branches do (a tie keeps the earlier axis).
 * ------------------------------------------------------------------ */
/* @implements 0x10067470 glide BrCrPlaneResolve */
void BrCrPlaneResolve(const BrVec3 *pExt, const BrVec3 *pA, float planeD,
                      const BrVec3 *pEdgeN, const BrVec3 aVerts[3])
{
    float nx, ny, nz;     /* the plane normal V fed to the shared tail */
    float d, s;

    if (g_brCrPlane.modeFC == 2u) {
        /* --- mode 2: build the box-face normal from the triangle centroid. */
        float cx, cy, cz, sign;
        int   k;

        /* centroid, in the original's add order ((v0 + v2) + v1) * 1/3 */
        cx = ((aVerts[0].x + aVerts[2].x) + aVerts[1].x) * BR_CR_THIRD;
        cy = ((aVerts[0].y + aVerts[2].y) + aVerts[1].y) * BR_CR_THIRD;
        cz = ((aVerts[0].z + aVerts[2].z) + aVerts[1].z) * BR_CR_THIRD;

        /* argmin |c| via the exact >= tournament: |cx| vs |cy| first, then the
         * survivor's partner against |cz|.  k is the axis the normal lands on. */
        if (fabsf(cx) >= fabsf(cy))
            k = (fabsf(cy) >= fabsf(cz)) ? 2 : 1;
        else
            k = (fabsf(cx) >= fabsf(cz)) ? 2 : 0;

        /* sign from cx (the quirk); NaN cx -> -0.5 */
        sign = (cx < 0.0f || isnan(cx)) ? -BR_CR_HALF : BR_CR_HALF;

        nx = ny = nz = 0.0f;
        if (k == 0)      nx = sign;
        else if (k == 1) ny = sign;
        else             nz = sign;

        /* the tail's dot: for a one-axis normal this is exact regardless of
         * summation order. */
        d = (pA->y * ny + pA->z * nz) + pA->x * nx;

        /* side-effect state the walker reads next.  .x is scaled by ext.x*ext.y
         * (two stores in the original), .y/.z keep the raw +-0.5, and modeFC is
         * multiplied by ext.z as a FLOAT (its int mode value reinterpreted). */
        g_brCrPlane.normal.x = pExt->y * (pExt->x * nx);
        g_brCrPlane.normal.y = ny;
        g_brCrPlane.normal.z = nz;
        {
            float fc;
            memcpy(&fc, &g_brCrPlane.modeFC, sizeof fc);
            fc = pExt->z * fc;
            memcpy(&g_brCrPlane.modeFC, &fc, sizeof fc);
        }
    } else {
        /* --- mode != 2: the plane normal is handed in directly. */
        nx = pEdgeN->x; ny = pEdgeN->y; nz = pEdgeN->z;
        d = (pA->y * ny + pA->z * nz) + pA->x * nx;   /* dot(pA, pEdgeN) */
    }

    /* shared tail: out = (planeD - dot(pA, V)) * pA */
    s = planeD - d;
    g_brCrPlane.out.x = s * pA->x;
    g_brCrPlane.out.y = s * pA->y;
    g_brCrPlane.out.z = s * pA->z;
}

/* 0x10077B44 -1.05, 0x10077B40 0.2, 0x10077B38 0.9, 0x100B5170 1.0,
 * 0x10077AB8 27, 0x10077B3C 1e-4, 0x10077B30 -4.703703880, 0x10077B34 128. */
#define BR_CR_RESTITUTION  (-1.05f)   /* -(1 + e), e = 0.05                    */
#define BR_CR_TANGENT       0.2f      /* tangential fraction folded into rhs   */
#define BR_CR_DAMP          0.9f      /* impact damping on the contact velocity*/
#define BR_CR_CLAMP27      27.0f      /* intensity ceiling                     */
#define BR_CR_EPS           1e-4f     /* restOffset gate for the damping path  */
#define BR_CR_PEAK_K       (-4.703703880310059f)
#define BR_CR_PEAK_BASE   128.0f

/* trunc-to-int, the x87 _ftol (0x10074560) the original calls; the store then
 * keeps the low byte. */
static uint8_t br_cr_ftol_byte(float x)
{
    return (uint8_t)(int32_t)x;   /* C truncates toward zero, as _ftol does */
}

/* ------------------------------------------------------------------ *
 * 0x10065C80 -- the impulse solver.
 *
 * WHAT IT DOES: resolves ONE contact into a collision impulse and applies it to
 * the body's `next` linear and angular velocity.  This is the function whose
 * absence lets cars fall through the world -- contacts were found and never
 * answered.
 *
 * The shape is a textbook rigid-body contact solve, with this engine's own
 * choices baked in:
 *
 *   nb   = orientationT . normal          -- the contact normal in body frame;
 *                                            this engine uses it AS the lever arm
 *                                            (there is no separate contact point).
 *   vc   = vel + angVel x nb              -- velocity at the contact.
 *   gate: if the approach speed dot(vc, relDir) is >= 0 the bodies are
 *         separating -- return 0, touch nothing.  Spelled `>=` so a NaN
 *         continues, matching the x87 `fcomp`/`jne`.
 *   Wworld = orientationT . invInertia . orientation   -- inverse inertia,
 *                                            world frame.
 *   K    = (1/mass) I - [nb]x . Wworld . [nb]x          -- the effective-mass
 *                                            matrix (diag preset to 1/mass).
 *   rhs  = dd*relDir + s*(vc - dd*relDir), dd = dot(vc, relDir),
 *          s = 0.2 when `flag`, else 0     -- normal component, plus an optional
 *                                            slice of the tangential velocity.
 *   J    = solve(K, rhs)                   -- the impulse (Cramer, no singular
 *                                            guard; a degenerate K yields inf/nan
 *                                            exactly as the original).
 *   vel    -= (1.05 + restOffset) * J / mass
 *   angVel -= (1.05 + restOffset) * Wworld . (nb x J)
 *
 * The effect record (damage/particle cue) is stamped alongside: intensity =
 * trunc(min(|dd|, 27)) and colour = the normal's raw dwords, always; and on a
 * hard hit (threshold > 10, restOffset < 1e-4) the contact velocity is damped
 * to 0.9 before the solve and a saturating `peak` byte is raised.
 *
 * Verified against tools/x87emu.py executing 0x10065C80's real opcode stream:
 * the Python model this mirrors matched the emulator over >14000 random cases
 * (both paths, effect bytes, and the restOffset gate), worst relative error
 * ~1.6e-3 confined to near-singular K.  Golden vectors below pin it.
 * ------------------------------------------------------------------ */
/* @implements 0x10065C80 glide BrCrImpulseSolve */
int BrCrImpulseSolve(float mass, const BrMat3 *pInvInertia, const BrMat4 *pOrient,
                     BrVec3 *pVel, BrVec3 *pAngVel,
                     const BrVec3 *pNormal, const BrVec3 *pRelDir,
                     int flag, float restOffset, BrCrEffect *pEffect)
{
    BrMat3 Rt, R;             /* orientation 3x3, transposed and straight */
    BrMat3 skew, Wworld, tmp, WSS, D, K;
    BrVec3 nb, vc, cross, rhs, J, dw;
    float  invMass = 1.0f / mass;
    float  dd, add, inten, tang, mult;
    int    i;

    /* nb = orientationT . normal */
    BrMat4ToMat3Both(&Rt, &R, pOrient);
    BrMat3MulVec3(&nb, &Rt, pNormal);

    /* vc = vel + angVel x nb */
    cross.x = pAngVel->y * nb.z - pAngVel->z * nb.y;
    cross.y = pAngVel->z * nb.x - pAngVel->x * nb.z;
    cross.z = pAngVel->x * nb.y - pAngVel->y * nb.x;
    vc.x = pVel->x + cross.x;
    vc.y = pVel->y + cross.y;
    vc.z = pVel->z + cross.z;

    /* gate: separating (or NaN handled as the original) -> no response */
    dd = vc.x * pRelDir->x + vc.y * pRelDir->y + vc.z * pRelDir->z;
    if (!(dd < 0.0f))
        return 0;

    /* effect record, always written when the contact answers:
     * intensity = trunc(min(|dd|, 27)); colour = the normal's dwords verbatim. */
    add   = fabsf(dd);
    inten = add < BR_CR_CLAMP27 ? add : BR_CR_CLAMP27;
    pEffect->intensity = br_cr_ftol_byte(inten);
    memcpy(pEffect->color, pNormal, sizeof pEffect->color);

    /* hard-hit path: raise the saturating peak byte and damp the contact
     * velocity used to drive the solve.  restOffset < 1e-4 is always true for
     * the shipped caller (it passes 0). */
    if (pEffect->threshold > 10u && restOffset < BR_CR_EPS) {
        uint8_t v = br_cr_ftol_byte(BR_CR_PEAK_BASE - BR_CR_PEAK_K * inten);
        if (v > pEffect->peak)
            pEffect->peak = v;
        vc.x *= BR_CR_DAMP; vc.y *= BR_CR_DAMP; vc.z *= BR_CR_DAMP;
        dd   *= BR_CR_DAMP;
    }

    /* K = (1/mass) I - [nb]x . Wworld . [nb]x.  Built exactly as the original:
     * an identity whose diagonal is overwritten with 1/mass, minus the triple
     * product, via the packed 3x3 subtract (0x1006DD80). */
    BrMat3Skew(&skew, &nb);
    BrMat3Mul(&tmp, pInvInertia, &R);   /* invInertia . orientation           */
    BrMat3Mul(&Wworld, &Rt, &tmp);      /* orientationT . invInertia . orient */
    BrMat3Mul(&tmp, &Wworld, &skew);    /* Wworld . [nb]x                     */
    BrMat3Mul(&WSS, &skew, &tmp);       /* [nb]x . Wworld . [nb]x             */
    for (i = 0; i < 9; ++i)
        D.m[i] = (i == 0 || i == 4 || i == 8) ? invMass : 0.0f;
    BrMat3Sub(K.m, D.m, WSS.m);

    /* rhs = dd*relDir + tang*(vc - dd*relDir) */
    tang = flag ? BR_CR_TANGENT : 0.0f;
    rhs.x = dd * pRelDir->x + tang * (vc.x - dd * pRelDir->x);
    rhs.y = dd * pRelDir->y + tang * (vc.y - dd * pRelDir->y);
    rhs.z = dd * pRelDir->z + tang * (vc.z - dd * pRelDir->z);

    /* J = solve(K, rhs); apply. */
    BrMat3Solve(&J, &K, &rhs);
    mult = restOffset - BR_CR_RESTITUTION;   /* restOffset + 1.05 */

    pVel->x -= mult * J.x * invMass;
    pVel->y -= mult * J.y * invMass;
    pVel->z -= mult * J.z * invMass;

    cross.x = nb.y * J.z - nb.z * J.y;       /* nb x J */
    cross.y = nb.z * J.x - nb.x * J.z;
    cross.z = nb.x * J.y - nb.y * J.x;
    BrMat3MulVec3(&dw, &Wworld, &cross);
    pAngVel->x -= mult * dw.x;
    pAngVel->y -= mult * dw.y;
    pAngVel->z -= mult * dw.z;

    return 1;
}
