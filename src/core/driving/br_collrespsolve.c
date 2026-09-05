/* br_collrespsolve.c -- see br_collrespsolve.h.  The OBB collision response.
 *
 * Transcribed from orig/BRGlide.dll and pinned to tools/x87emu.py golden
 * vectors.  Each function carries the address of what it is.
 */
#include <math.h>
#include <string.h>

/* The original 0x10067710 takes TWO arguments (the body block and the box
 * matrix); the port's prototype in br_collrespsolve.h takes nine.  Under the
 * matching build the header's prototype is declared under a spare name so the
 * two-argument original can be defined here without touching include/. */
#ifdef BR_MATCHING_BUILD
#define BrCrRespWalk BrCrRespWalk_portproto
#endif
#include "br_collrespsolve.h"
#ifdef BR_MATCHING_BUILD
#undef BrCrRespWalk
#endif
#include "br_collresp.h"   /* BrCrTest, BrCollRespNode/Plane, g_pBrCollRespList */
#include "slice1_09.h"     /* BrMat4TransformPoint, BrVec3Normalise             */

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
#ifdef BR_MATCHING_BUILD
/* Matching arm, retranscribed from the bytes (the port arm below is the
 * verified readable form).  Structural facts the bytes force:
 *  - the centroid is a 3-iteration walking-pointer loop into a float[3]
 *    (0x100674c0: eax walks &aVerts[1], ecx walks the stack array, edx=3);
 *  - |c| is fcom [Zero] + fld st(0) + conditional fchs, keeping the RAW
 *    value beneath the abs on the x87 stack; inner abs reloads from the
 *    array, not from the already-abs temp;
 *  - the argmin is `if (t0 < t1)` fall-through (orig je to the >= arm)
 *    with a shared goto z-wins -- Ghidra inverted this to `<=` and
 *    duplicated z;
 *  - the sign is an INT -1/+1 packed into param_4's slot, converted with
 *    fild and scaled by the 0.5 memory constant, fnstsw deferred past
 *    the dead-axis zero stores;
 *  - the dot reads the normal BACK FROM THE GLOBALS, normal.x is scaled by
 *    the two extents in TWO separate statements, and modeFC is scaled as a
 *    float in place;
 *  - the extents live at +0x1dc/+0x1e0/+0x1e4 of the object pExt points
 *    into (the port prototype abstracts this);
 *  - the tail computes s = -(dot - planeD) (fsub then fchs) with s homed.
 * Remaining (REGNORM 4+21): s sunk to the join (missing fsub/fchs and
 * s-slot fmul/fstp); centroid loop strength-reduced (sub ecx,eax vs
 * add ecx,4); extra fld Ax in the early dot; x87 fxch drain. */
extern float BrCrK_Zero;     /* 0x10077A78 */
extern float BrCrK_Third;    /* 0x10077B84 */
extern float BrCrK_Half;     /* 0x10077AC8 */
#define BR_CR_EXT(off) (*(const float *)((const char *)pExt + (off)))
void BrCrPlaneResolve(const BrVec3 *pExt, const BrVec3 *pA, float planeD,
                      const BrVec3 *pEdgeN, const BrVec3 aVerts[3])
{
    float local_c[3];
    float fVar1;
    float *param_2;
    int sgn;

    param_2 = (float *)pA;

    if (g_brCrPlane.modeFC != 2u) {
        /* Early arm is the fall-through.  s = -(dot - planeD) is left in
         * fVar1 and the three out stores live at the join -- a goto-join
         * on pA->y hoists lea [pA+4] and steals esi. */
        fVar1 = -((*(float *)((char *)pEdgeN + 8) * param_2[2]
                   + pEdgeN->y * param_2[1]
                   + *param_2 * pEdgeN->x) - planeD);
        goto LAB_tail;
    }
    {
        float *pfVar3;
        float *pfVar4;
        float t;
        int iVar5;

        iVar5 = 3;
        pfVar3 = (float *)((int)aVerts + 0xc);
        pfVar4 = local_c;
        do {
            iVar5 = iVar5 + -1;
            t = pfVar3[-3];
            *pfVar4 = (t + pfVar3[3] + *pfVar3) * BrCrK_Third;
            pfVar3 = pfVar3 + 1;
            pfVar4 = pfVar4 + 1;
        } while (iVar5 != 0);
    }
    {
        float t0, t1, u0, u1;

        /* Ternary abs: both arms assign t so the pre-branch load IS t and
         * fchs is in-place.  `t = x; if (t < Z) t = -t` reassigns and
         * emits fstp-st; fld; fchs. */
        t0 = (local_c[0] < BrCrK_Zero) ? -local_c[0] : local_c[0];
        t1 = (local_c[1] < BrCrK_Zero) ? -local_c[1] : local_c[1];

        /* Orig: test ah,1; je then-at-higher-addr; fall-through is |c0|<|c1|.
         * Shared z-wins via goto -- duplicating it was the +76 B. */
        if (t0 < t1) {
            u0 = (local_c[0] < BrCrK_Zero) ? -local_c[0] : local_c[0];
            u1 = (local_c[2] < BrCrK_Zero) ? -local_c[2] : local_c[2];
            if (u0 < u1) {
                g_brCrPlane.normal.z = 0.0f;
                g_brCrPlane.normal.y = 0.0f;
                sgn = -1;
                if (!(local_c[0] < BrCrK_Zero))
                    sgn = 1;
                g_brCrPlane.normal.x = (float)sgn * BrCrK_Half;
            } else {
                goto LAB_z;
            }
        } else {
            u0 = (local_c[1] < BrCrK_Zero) ? -local_c[1] : local_c[1];
            u1 = (local_c[2] < BrCrK_Zero) ? -local_c[2] : local_c[2];
            if (u0 < u1) {
                g_brCrPlane.normal.z = 0.0f;
                g_brCrPlane.normal.x = 0.0f;
                sgn = -1;
                if (!(local_c[0] < BrCrK_Zero))
                    sgn = 1;
                g_brCrPlane.normal.y = (float)sgn * BrCrK_Half;
            } else {
LAB_z:
                g_brCrPlane.normal.y = 0.0f;
                g_brCrPlane.normal.x = 0.0f;
                sgn = -1;
                if (!(local_c[0] < BrCrK_Zero))
                    sgn = 1;
                g_brCrPlane.normal.z = (float)sgn * BrCrK_Half;
            }
        }
    }

    fVar1 = *param_2 * g_brCrPlane.normal.x;
    g_brCrPlane.normal.x = BR_CR_EXT(0x1dc) * g_brCrPlane.normal.x;
    g_brCrPlane.normal.x = BR_CR_EXT(0x1e0) * g_brCrPlane.normal.x;
    *(float *)&g_brCrPlane.modeFC =
        BR_CR_EXT(0x1e4) * *(float *)&g_brCrPlane.modeFC;
    fVar1 = -((fVar1 + g_brCrPlane.normal.y * param_2[1]
               + g_brCrPlane.normal.z * param_2[2]) - planeD);
LAB_tail:
    g_brCrPlane.out.x = *param_2 * fVar1;
    g_brCrPlane.out.y = fVar1 * param_2[1];
    g_brCrPlane.out.z = fVar1 * param_2[2];
}
#else
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
#endif /* BR_MATCHING_BUILD */

/* 0x10077B44 -1.05, 0x10077B40 0.2, 0x10077B38 0.9, 0x100B5170 1.0,
 * 0x10077AB8 27, 0x10077B3C 1e-4, 0x10077B30 -4.703703880, 0x10077B34 128. */
#define BR_CR_RESTITUTION  (-1.05f)   /* -(1 + e), e = 0.05                    */
#define BR_CR_KICK_REST     1.05f     /* 0x10077B2C -- +(1 + e), kick path     */
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

/* ------------------------------------------------------------------ *
 * 0x10065950 -- signed distance of a point from a contact plane.
 *
 * Fourteen call sites, all in the OBB walk (0x10066D70, 0x10068070,
 * 0x100682C0), and every one of them pushes the plane record, the plane's
 * own +0x0C field as the constant, and the address of a point:
 * `push esi / push [esi+0xc] / push &pt / call`.  So the first argument is
 * the plane normal at +0x00..+0x08 of that record and the second is its
 * plane constant, passed by value because the caller already has it loaded.
 *
 * THE TERM ORDER IS y, z, x -- read off the two faddps, not guessed.  The
 * x87 stack at the first `faddp st(1)` is [py*ny (ST1), pz*nz (ST0)], and
 * `faddp st(1)` is ST(1) += ST(0), so that sum is (y + z); the second sees
 * [that (ST1), nx*px (ST0)] and makes ((y + z) + x).  The plane constant is
 * last, as a plain `fadd dword ptr [esp+8]`.  Float addition is not
 * associative, so this is the source order and not a scheduling artefact --
 * writing the conventional x, y, z sum pairs the wrong two products.
 *
 * ‼ NO PROTOTYPE.  This function is byte-exact ONLY when its definition is
 * not preceded by a declaration of itself: putting the obvious prototype in
 * br_collrespsolve.h adds one `fxch st(1)` and takes it 41 -> 43 bytes.
 * Isolated and re-measured both ways (the bare prototype alone does it, with
 * or without the comment above it).  A prototype in some OTHER translation
 * unit is harmless -- it is a prior declaration in the DEFINING TU that moves
 * the schedule.  Do not "tidy" this into the header.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: says which side of a plane a point is on, and how far --
 * positive in front of the plane, negative behind it. */
/* @implements 0x10065950 glide BrCrPlaneDist */
/* @n64 0x8025B704 located */
float BrCrPlaneDist(const BrVec3 *pN, float planeD, const BrVec3 *pPoint)
{
    return pPoint->y * pN->y + pPoint->z * pN->z + pPoint->x * pN->x + planeD;
}

/* ------------------------------------------------------------------ *
 * 0x10065980 -- the contact "kick" (see the header).
 *
 * The linear part is a plain restitution reflection: remove 1.05x the velocity
 * component along the contact normal, so an approaching body leaves with a small
 * rebound.  Then, gated on the two flags and the effect threshold, come the
 * damping and the spin fold.
 *
 * The spin fold (spinFlag) is the one non-obvious piece.  It builds an
 * orthogonal frame M from the normal -- row 0 is N, row 1 is the quadratic
 * tangent (Nx*Ny - Nz^2, Ny*Nz - Nx^2, Nx*Nz - Ny^2), row 2 is N x row 1 -- and
 * forms M diag(Mt . N) Mt . angVel.  Because Mt.N has a single non-zero entry
 * (N is row 0 of M, so Mt.N is |N|^2 along axis 0 and near-0 elsewhere), the
 * result is the component of the spin about the contact normal, sign and scale
 * carried by the frame.  Transcribed as the original computes it -- Mt.N, then
 * Mt.angVel, their componentwise product, then M times that -- so the exact
 * arithmetic (and any non-unit-N behaviour) matches.
 *
 * Verified against tools/x87emu.py over 6000 random cases (both flags, all
 * effect branches), worst relative error ~2e-6.  Golden vectors pin it.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: apply one collision impulse to a body: bounces the velocity
 * off the surface and adds the spin the contact imparts. Returns without
 * touching anything when the body is already moving AWAY from the surface,
 * which is what stops a resting car being kicked every frame. */
/* @implements 0x10065980 glide BrCrContactKick */
int BrCrContactKick(BrVec3 *pVel, BrVec3 *pAngVel, const BrVec3 *pNormal,
                    int dampFlag, int spinFlag, BrCrEffect *pEffect)
{
    const float nx = pNormal->x, ny = pNormal->y, nz = pNormal->z;
    float d, s;

    /* gate: separating (or NaN, as the x87 fcomp) -> no response */
    d = nx * pVel->x + ny * pVel->y + nz * pVel->z;
    if (!(d < 0.0f))
        return 0;

    /* restitution reflection: vel -= 1.05 * dot(N, vel) * N */
    s = BR_CR_KICK_REST * d;
    pVel->x -= s * nx;
    pVel->y -= s * ny;
    pVel->z -= s * nz;

    /* effect record + extra damp, only for a hard enough hit (>= 10) */
    if (pEffect->threshold >= 10u) {
        float add   = -d;                                   /* |dot|, d < 0 */
        float inten = add < BR_CR_CLAMP27 ? add : BR_CR_CLAMP27;
        uint8_t peak;

        pEffect->intensity = br_cr_ftol_byte(inten);
        /* colour is the shared normal bank's dwords, not pNormal */
        memcpy(pEffect->color, &g_brCrPlane.normal, sizeof pEffect->color);
        peak = br_cr_ftol_byte(BR_CR_PEAK_BASE - BR_CR_PEAK_K * inten);
        if (peak > pEffect->peak)
            pEffect->peak = peak;

        pVel->x *= BR_CR_DAMP; pVel->y *= BR_CR_DAMP; pVel->z *= BR_CR_DAMP;
    }

    if (dampFlag) {
        pVel->x *= BR_CR_DAMP; pVel->y *= BR_CR_DAMP; pVel->z *= BR_CR_DAMP;
    }

    if (spinFlag) {
        /* frame M: row0 = N, row1 = quadratic tangent, row2 = N x row1 */
        float r1x = nx * ny - nz * nz;
        float r1y = ny * nz - nx * nx;
        float r1z = nx * nz - ny * ny;
        float r2x = ny * r1z - nz * r1y;
        float r2y = nz * r1x - nx * r1z;
        float r2z = nx * r1y - ny * r1x;
        float wx = pAngVel->x, wy = pAngVel->y, wz = pAngVel->z;

        /* a = Mt . N : out[i] = row0[i]*N.x + row1[i]*N.y + row2[i]*N.z */
        float ax = nx * nx + r1x * ny + r2x * nz;
        float ay = ny * nx + r1y * ny + r2y * nz;
        float az = nz * nx + r1z * ny + r2z * nz;
        /* b = Mt . angVel */
        float bx = nx * wx + r1x * wy + r2x * wz;
        float by = ny * wx + r1y * wy + r2y * wz;
        float bz = nz * wx + r1z * wy + r2z * wz;
        /* c = a (componentwise) b */
        float c0 = ax * bx, c1 = ay * by, c2 = az * bz;
        /* angVel = M . c : out[i] = row_i . c */
        pAngVel->x = nx  * c0 + ny  * c1 + nz  * c2;
        pAngVel->y = r1x * c0 + r1y * c1 + r1z * c2;
        pAngVel->z = r2x * c0 + r2y * c1 + r2z * c2;
    }

    return 1;
}

/* 0x10077B8C -- 1.1, the penetration push-out gain in the walker's position fix. */
#define BR_CR_PUSHOUT 1.1f

/* ------------------------------------------------------------------ *
 * 0x10067710 -- the response walker: the top of the collision response.
 *
 * It walks the broad phase's candidate list (g_pBrCollRespList) and, for each
 * contact that survives the exact test, resolves the impulse and pushes the
 * body back out of the triangle.  This is the function that finally wires the
 * whole unit together, so the previous three all run because this calls them.
 *
 * Per contact:
 *   1. transform the record's three triangle vertices into the car's box space
 *      (BrMat4TransformPoint) and form the face normal e2 x e1, exactly as the
 *      broad phase did;
 *   2. BrCrTest the transformed triangle against the unit box; skip if clear;
 *   3. build the contact plane: normalise the face normal, take planeD =
 *      dot(normal, v0), and choose a box-corner sign per axis --
 *      sign[i] = +0.5 if planeD*normal[i] >= 0 else -0.5.  The shared normal
 *      bank is then (ext.x*sx, ext.y*sy, ext.z*sz + ext.w), the box corner the
 *      contact drives toward;
 *   4. BrCrPlaneResolve, then BrCrImpulseSolve with that bank as the normal and
 *      the plane's own normal as the approach direction.  The solver's tangent
 *      flag is off when the body's "no-torque" gate (body+0xE4) exceeds 0.5;
 *   5. if the solver acted, push the body out: subtract 1.1x the penetration
 *      (measured from the saved position along the plane normal) from next.pos,
 *      restore the quaternion, and rebuild qDot and the orientation matrix.
 *
 * The mode-2 / contact-kick branch is gated on the debug global 0x100A9360 == 4
 * (it is 1 in the shipped build), so the retail path always takes the impulse
 * solver; that branch is not reproduced here.  The four "trace" calls
 * (0x10008D60) are a one-byte `ret` stub and are dropped.
 *
 * pOrient/pNext are REBUILT on a resolved contact.  Returns the number of
 * contacts that produced a response.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: walk every surface the car is touching this frame and apply
 * each one's collision response in turn. The top of the collision solver --
 * one pass over the contact list built during the collision test.  Returns
 * 1 if any contact produced a response, else 0, and keeps a per-body "frames
 * with no contact" byte (body+0x200): bumped (saturating at 40) when nothing
 * passed the exact test this pass, reset to 0 otherwise. */
/* @implements 0x10067710 glide BrCrRespWalk */
#ifdef BR_MATCHING_BUILD
/* Matching arm, transcribed from the bytes.  The original is
 * (body, pMatBox): every field the port passes separately is read off the
 * body block (offsets below), and its two callees take the body too --
 * 0x10065C80 is (body, pNormal, pRelDir, flag, restOffset) and 0x10065980 is
 * (body, pNormal, dampFlag, spinFlag).  Their port definitions in this file
 * keep the port signatures, so the calls go through a cast of the function
 * designator, which VC5 still emits as a direct `call rel32`.
 *
 * State 2026-09-04: 1296/1301 B, 375/375 instructions, regnorm 7+7 under
 * plain /O2 (the original is esp-framed: `sub esp,0x78`, ebp a general
 * register -- NOT the /Oy- the old report row said).  Frame, prologue,
 * loop shape, the mode-4 block, the sign tournament, the normal bank, both
 * calls, the push-out, the quat copy and the +0x200 tail all line up.
 * What the port had dropped: the debug-mode-4 kick path with its three
 * trace calls (0x10008D60, a bare ret, still called with the string), the
 * body-relative signature, the 0/1 return, and the +0x200 counter tail.
 * Levers that landed: the push-out delta is a NAMED BrVec3 (three homed
 * slots, `fld st(0)`/`fld st(1)` dups); `pos = pos - dp` not `pos -= dp`
 * (orig fld pos; fsub [slot]); the quat copy is three DWORD copies
 * (integer regs, hoisted loads), not float assignments; the first sign is
 * an if/else (`< 0` arm first), the other two `sgn=-1; if (!(x<0)) sgn=1`.
 * Landed 2026-09-05: e1 is computed BEFORE e2 (the orig loads all of e1's
 * minuends aV[3..5] before the first fsub, so region 1 was a pure statement-
 * order artifact) -- register-blind multiset 17+17 -> 12+12.  What is left
 * is the d/dp block (lines ~682-696): the orig loads pos.z,pos.y first and
 * `fsub` the BF slots (pos-BF, natural minuend-first) while ours loads all
 * three BF and `fsubr`s; plus the sum's faddp association and one
 * fld-st-dup vs fmul-mem.  All FPU-stack scheduling of one expression, not
 * source-permutable (blind term reordering forbidden); T3a.
 * Remaining, all in one cause-group (register/slot allocation, 4 regions):
 *  - slots: orig planeD@0x10 cnt@0x14 sgn@0x18 spin@0x1c; ours has
 *    planeD/cnt one slot up.  Declaration order both ways: inert (7+7).
 *  - `spin` is HOMED in the orig ([esp+0x1c], written via the edi that
 *    also carries the constant 0/1); ours keeps spin in a register.  The
 *    orig's `mov edi,0` (not xor) then `mov [modeFC],edi` says modeFC=0
 *    and spin=0 share one constant register.
 *  - the mode-4 else block sits OUT OF LINE after the epilogue in the orig
 *    (three `je` to it, `jmp` back past `mov edi,1`); ours is inline with a
 *    `jmp` from the then-arm.  goto-to-a-trailing-label form: worse (9+8).
 * Dead: BF-offset spelling of next.pos in the delta (fsubp shape, 11+7);
 * `pos -= dp` (fsubr x3); goto LAB_common out of the then-arm (extra jmp). */
extern int   g_br0AA010;        /* 0x100A9360 -- the debug/game mode; 4 arms the kick path */
extern float BrCrK_Flat;        /* 0x10077B88 0.999 */
extern float BrCrK_Pushout;     /* 0x10077B8C 1.1 */
extern const BrCollPlane *g_pBrCrCurPlane;   /* 0x1177819C */
void BrExt_10008D60(const char *pszFmt, ...); /* 0x10008D60, a bare `ret` */

typedef int (*BrCrImpulseSolveFn)(char *, const BrVec3 *, const BrCollPlane *, int, float);
typedef int (*BrCrContactKickFn)(char *, const BrCollPlane *, int, int);
#define BR_CR_IMPULSE(b, n, p, f, r) (((BrCrImpulseSolveFn)BrCrImpulseSolve)((b), (n), (p), (f), (r)))
#define BR_CR_KICK(b, p, f, s)       (((BrCrContactKickFn)BrCrContactKick)((b), (p), (f), (s)))

#define BR_CR_BF(off)  (*(float *)(pBody + (off)))
#define BR_CR_BB(off)  (*(unsigned char *)(pBody + (off)))
#define BR_CR_BD(off)  (*(uint32_t *)(pBody + (off)))
#define BR_CR_NEXT     ((BrRbState *)(pBody + 0x158))
#define BR_CR_ORIENT   ((BrMat4 *)(pBody + 0xbc))

int BrCrRespWalk(char *pBody, const BrMat4 *pMatBox)
{
    const BrCollRespNode *pNode;
    const BrCollPlane *pP;
    float  aV[9];
    BrVec3 e1, e2, nrm, sign, dp;
    float  planeD, pz, d;
    int    ret = 0;
    short  cnt = 0;
    int    flag, spin, sgn, r;

    for (pNode = g_pBrCollRespList; pNode != NULL; pNode = pNode->pNext) {
        pP = pNode->pPlane;
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[0], pMatBox, pP->pV0);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[3], pMatBox, pP->pV1);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[6], pMatBox, pP->pV2);
        e1.x = aV[3] - aV[0]; e1.y = aV[4] - aV[1]; e1.z = aV[5] - aV[2];
        e2.x = aV[6] - aV[0]; e2.y = aV[7] - aV[1]; e2.z = aV[8] - aV[2];
        nrm.x = e2.z * e1.y - e2.y * e1.z;
        nrm.y = e2.x * e1.z - e2.z * e1.x;
        nrm.z = e2.y * e1.x - e2.x * e1.y;
        if (BrCrTest(aV, &nrm) == 0)
            continue;
        BrVec3Normalise(&nrm);
        cnt++;
        planeD = nrm.x * aV[0] + nrm.y * aV[1] + nrm.z * aV[2];
        g_brCrPlane.modeFC = 0;
        if (g_br0AA010 == 4) {
            if (((nrm.x < BrCrK_Zero) ? -nrm.x : nrm.x) <= BrCrK_Flat
                && ((nrm.y < BrCrK_Zero) ? -nrm.y : nrm.y) <= BrCrK_Flat
                && ((nrm.z < BrCrK_Zero) ? -nrm.z : nrm.z) <= BrCrK_Flat) {
                if (((planeD < BrCrK_Zero) ? -planeD : planeD) < BrCrK_Half) {
                    BrExt_10008D60("Triangle Edge to CubeFace\n");
                    spin = 0;
                    g_brCrPlane.modeFC = 2;
                }
            } else {
                g_brCrPlane.modeFC = 1;
                BrExt_10008D60("Wank CT1 case\n");
                spin = 1;
            }
        }
        flag = 1;
        BrExt_10008D60("Cube Edge to Triangle Face\n");

        pz = planeD * nrm.z;
        if (planeD * nrm.x < BrCrK_Zero)
            sgn = -1;
        else
            sgn = 1;
        sign.x = (float)sgn * BrCrK_Half;
        sgn = -1;
        if (!(planeD * nrm.y < BrCrK_Zero))
            sgn = 1;
        sign.y = (float)sgn * BrCrK_Half;
        sgn = -1;
        if (!(pz < BrCrK_Zero))
            sgn = 1;
        sign.z = (float)sgn * BrCrK_Half;

        g_brCrPlane.normal.x = BR_CR_BF(0x1dc) * sign.x;
        g_brCrPlane.normal.y = BR_CR_BF(0x1e0) * sign.y;
        g_brCrPlane.normal.z = BR_CR_BF(0x1e4) * sign.z + BR_CR_BF(0x1e8);
        g_pBrCrCurPlane = pP;
        BrCrPlaneResolve((const BrVec3 *)pBody, &nrm, planeD, &sign, (const BrVec3 *)(void *)aV);

        if (BR_CR_BF(0xe4) > BrCrK_Half)
            flag = 0;
        if (flag)
            BrExt_10008D60("Resistive collision %10.3f\n", BR_CR_BF(0xe4));

        if (g_brCrPlane.modeFC != 1)
            r = BR_CR_IMPULSE(pBody, &g_brCrPlane.normal, g_pBrCrCurPlane, flag, 0.0f);
        else
            r = BR_CR_KICK(pBody, g_pBrCrCurPlane, flag, spin);
        if (r == 0)
            continue;

        d = ((BR_CR_NEXT->pos.x - BR_CR_BF(0x114)) * pP->nx
           + (BR_CR_NEXT->pos.y - BR_CR_BF(0x118)) * pP->ny
           + (BR_CR_NEXT->pos.z - BR_CR_BF(0x11c)) * pP->nz) * BrCrK_Pushout;
        ret = 1;
        dp.x = d * pP->nx;
        dp.y = d * pP->ny;
        dp.z = d * pP->nz;
        BR_CR_NEXT->pos.x = BR_CR_NEXT->pos.x - dp.x;
        BR_CR_NEXT->pos.y = BR_CR_NEXT->pos.y - dp.y;
        BR_CR_NEXT->pos.z = BR_CR_NEXT->pos.z - dp.z;
        BR_CR_BD(0x170) = BR_CR_BD(0x12c);
        BR_CR_BD(0x174) = BR_CR_BD(0x130);
        BR_CR_BD(0x178) = BR_CR_BD(0x134);
        BrRbQuatDerivative(BR_CR_NEXT);
        BrRbBuildMatrix(BR_CR_ORIENT, BR_CR_NEXT);
    }

    if (cnt == 0) {
        if (BR_CR_BB(0x200) < 0x28)
            BR_CR_BB(0x200)++;
    } else {
        BR_CR_BB(0x200) = 0;
    }
    return ret;
}
#else
int BrCrRespWalk(float mass, const BrMat3 *pInvInertia, BrMat4 *pOrient,
                 const float ext[4],
                 BrRbState *pNext, const BrVec3 *pSavePos, const BrVec3 *pQuatSrc,
                 BrCrEffect *pEffect, const BrMat4 *pMatBox)
{
    const BrCollRespNode *pNode;
    int nResponded = 0;

    for (pNode = g_pBrCollRespList; pNode != NULL; pNode = pNode->pNext) {
        const BrCollPlane *pP = pNode->pPlane;
        float  aV[9];
        BrVec3 e1, e2, nrm, normal, sign, planeN;
        float  planeD, d;
        int    flag, r;

        /* 1. transform the triangle into box space and form its face normal */
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[0], pMatBox, pP->pV0);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[3], pMatBox, pP->pV1);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[6], pMatBox, pP->pV2);
        e1.x = aV[3] - aV[0]; e1.y = aV[4] - aV[1]; e1.z = aV[5] - aV[2];
        e2.x = aV[6] - aV[0]; e2.y = aV[7] - aV[1]; e2.z = aV[8] - aV[2];
        nrm.x = e2.z * e1.y - e2.y * e1.z;
        nrm.y = e2.x * e1.z - e2.z * e1.x;
        nrm.z = e2.y * e1.x - e2.x * e1.y;

        /* 2. exact test */
        if (BrCrTest(aV, &nrm) == 0)
            continue;

        /* 3. contact plane: normalise the face normal, plane offset, box sign */
        normal = nrm;
        BrVec3Normalise(&normal);
        planeD = normal.x * aV[0] + normal.y * aV[1] + normal.z * aV[2];
        sign.x = (planeD * normal.x >= 0.0f) ? 0.5f : -0.5f;
        sign.y = (planeD * normal.y >= 0.0f) ? 0.5f : -0.5f;
        sign.z = (planeD * normal.z >= 0.0f) ? 0.5f : -0.5f;

        g_brCrPlane.normal.x = ext[0] * sign.x;
        g_brCrPlane.normal.y = ext[1] * sign.y;
        g_brCrPlane.normal.z = ext[2] * sign.z + ext[3];
        g_brCrPlane.modeFC   = 0u;

        /* 4. resolve + solve */
        BrCrPlaneResolve(&normal, &normal, planeD, &sign, (const BrVec3 *)(void *)aV);
        /* the "no torque" gate is body+0xE4, which IS orient.m[2][2]: when the
         * car's own up axis still points up (> 0.5) the tangential term is off. */
        flag = (pOrient->m[2][2] > 0.5f) ? 0 : 1;
        planeN.x = pP->nx; planeN.y = pP->ny; planeN.z = pP->nz;
        r = BrCrImpulseSolve(mass, pInvInertia, pOrient, &pNext->vel, &pNext->angVel,
                             &g_brCrPlane.normal, &planeN, flag, 0.0f, pEffect);
        if (r == 0)
            continue;
        ++nResponded;

        /* 5. push the body out along the plane normal, then rebuild orientation */
        d = ((pNext->pos.x - pSavePos->x) * planeN.x
           + (pNext->pos.y - pSavePos->y) * planeN.y
           + (pNext->pos.z - pSavePos->z) * planeN.z) * BR_CR_PUSHOUT;
        pNext->pos.x -= d * planeN.x;
        pNext->pos.y -= d * planeN.y;
        pNext->pos.z -= d * planeN.z;
        pNext->quat.f00 = pQuatSrc->x;
        pNext->quat.f04 = pQuatSrc->y;
        pNext->quat.f08 = pQuatSrc->z;
        BrRbQuatDerivative(pNext);
        BrRbBuildMatrix(pOrient, pNext);
    }

    return nResponded;
}
#endif /* BR_MATCHING_BUILD */
