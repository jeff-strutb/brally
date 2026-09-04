/* Glide match for BrPfxUpdateB0 — 0x10033880
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A200 d3d) and
 * takes `(BrPfxPool *, const BrPfxEnv *)`.  The original takes NOTHING —
 * its call site at 0x10033BB0 pushes no arguments at all — because dt,
 * the ambient drift, the 32-byte record array and the list heads are
 * globals and the free is inlined against the free head.  Same
 * globals-struct-parameter blocker as its sibling BrPfxUpdateB4AC, whose
 * already-converted body in slice2_21.c is the template for this one.
 *
 * Shape, straight off the original:
 *  - the head is read as a DWORD and masked (`mov ebp,[0x10AC0C40]` /
 *    `and ebp,0xFFFF`), and the link ADDRESS is planted in its stack slot
 *    as an immediate, so head value and head address are two assignments,
 *    not one `*piLink` read.
 *  - `k` and `scale` ride the x87 stack across the __ftol call, which is
 *    why the fade uses the raw `(int32_t)` cast rather than the
 *    BrFtolTrunc helper: a real call would spill them.
 *  - no gravity term here (the B4AC family's `vel.z -= dt*19.62` and its
 *    second free condition are absent), and the fade divides by the
 *    SQUARE of the age.
 *
 * Two levers took this from 245 diffs to byte-exact, in this order:
 *  1. Spell `g_aPfxRec[iRec].field` in EVERY statement. Hoisting the
 *     record pointer into a local (`PfxRec *p = &g_aPfxRec[iRec]`, which
 *     is what the already-converted BrPfxUpdateB4AC body still does)
 *     collapses the index chain into a base register and costs 19 bytes
 *     and ten instructions: 48+21 regnorm -> 18+8. Same "rebuild the
 *     index chain in every statement" rule as the slots class.
 *  2. A REDUNDANT OUTER PAREN PAIR around the left group of the three
 *     position sums: `(prod*dt + drift) + pos`. Without it VC5 emits
 *     `fadd pos` before `fadd drift` on y and z (but NOT on x, which
 *     computes `scale` inline and so meets the adds at a different x87
 *     depth). Permuting the summands does nothing -- VC5 canonicalises
 *     commutative float addition -- but the paren pair moves the
 *     schedule; see docs/VC5-IDIOMS.md.
 */
#ifdef BR_MATCHING_BUILD

#include <stdint.h>

typedef struct {
    float    x, y, z;
} PfxVec3;

typedef struct {
    PfxVec3  pos;               /* +0x00 */
    PfxVec3  vel;               /* +0x0C */
    float    age;               /* +0x18 */
    uint16_t iNext;             /* +0x1C */
    uint8_t  f1E;               /* +0x1E */
    uint8_t  f1F;               /* +0x1F */
} PfxRec;                       /* 0x20 */

extern float    g_fPfxDt;       /* 0x106E9D8C */
extern PfxVec3  g_vPfxDrift;    /* 0x104ADD40 */
extern PfxRec   g_aPfxRec[];    /* 0x10AC0C48 */
extern int32_t  g_iPfxHeadB0;   /* 0x10AC0C40 -- dword read, low word is
                                 * the head */
extern uint16_t g_iPfxFree;     /* 0x10AC0C38 */
extern const float kPfx0_3;     /* 0x1007758C  0.3     */
extern const float kPfxRecip;   /* 0x100775C4  1/65280 */
extern const float kPfxNeg0_8;  /* 0x100775C8  -0.8    */
extern const float kPfx5_7375;  /* 0x100775CC  5.7375  */
extern const float kPfxCell;    /* 0x100775D0  0.03125 */

/* WHAT IT DOES: advance one whole list of particles by a frame: ages each
 * one, moves it by its own velocity plus the global drift, applies a
 * downward pull, and fades its size. This is the per-frame physics for dust,
 * smoke and spray. */
/* @implements 0x10033880 glide BrPfxUpdateB0 */
void BrPfxUpdateB0(void)
{
    float k = g_fPfxDt * kPfx0_3;
    uint16_t *piLink;
    unsigned iRec;
    int iNext;

    iRec   = (unsigned)g_iPfxHeadB0 & 0xFFFFu;
    piLink = (uint16_t *)&g_iPfxHeadB0;

    while (iRec != 0) {
        float scale;

        iNext = g_aPfxRec[iRec].iNext;
        g_aPfxRec[iRec].age = k + g_aPfxRec[iRec].age;
        scale = (float)((int)g_aPfxRec[iRec].f1F * (int)g_aPfxRec[iRec].f1E)
              * kPfxRecip;

        g_aPfxRec[iRec].pos.x = (g_aPfxRec[iRec].vel.x * scale) * g_fPfxDt
                              + g_vPfxDrift.x + g_aPfxRec[iRec].pos.x;
        g_aPfxRec[iRec].pos.y = ((g_aPfxRec[iRec].vel.y * scale) * g_fPfxDt
                              + g_vPfxDrift.y) + g_aPfxRec[iRec].pos.y;
        g_aPfxRec[iRec].pos.z = ((scale * g_aPfxRec[iRec].vel.z - kPfxNeg0_8)
                              * g_fPfxDt + g_vPfxDrift.z) + g_aPfxRec[iRec].pos.z;

        g_aPfxRec[iRec].f1E = (uint8_t)(int32_t)
            (kPfx5_7375 / (g_aPfxRec[iRec].age * g_aPfxRec[iRec].age));

        if (scale < kPfxCell) {
            *piLink = g_aPfxRec[iRec].iNext;
            g_aPfxRec[iRec].iNext = g_iPfxFree;
            g_iPfxFree = (uint16_t)iRec;
        } else {
            piLink = &g_aPfxRec[iRec].iNext;
        }

        iRec = iNext;
    }
}

#endif /* BR_MATCHING_BUILD */
