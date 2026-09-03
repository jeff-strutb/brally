/* Glide match for BrPfxUpdateB4AC — 0x100339C0
 *
 * Third member of the particle-step family, after 0x10033BB0 BrPfxTick
 * and 0x10033880 BrPfxUpdateB0.  The port body in src/core/slice2_21.c
 * (tagged 0x1003A340 d3d) already had the no-argument globals form; what
 * it still lacked were the two levers that closed BrPfxUpdateB0:
 *
 *  1. the record index chain is RESPELLED in every statement --
 *     `g_aPfxRec[iRec].field`, never a hoisted `PfxRec *p`;
 *  2. a redundant outer paren pair around the left group of each
 *     position sum, `(prod*dt + drift) + pos`, which is what puts the
 *     drift `fadd` before the position `fadd`.  Permuting the summands
 *     does nothing -- VC5 canonicalises commutative float addition.
 *
 * The rest is the family's shared shape: head read as a DWORD and masked
 * with the link ADDRESS planted as an immediate (two assignments per
 * arm, not one `*piLink` read), `k` and `scale` carried on the x87 stack
 * across the __ftol call -- hence the raw `(int32_t)` cast rather than
 * the BrFtolTrunc helper -- and the free inlined against the free head.
 * This one walks TWO lists (B4 then AC) and adds the gravity term and
 * the second free condition.
 *
 * Both levers landed here: the three position sums, the fade, the
 * gravity term, the inlined free and the whole pass loop are
 * instruction-for-instruction the original's.  (The `fadd [R]` /
 * `fstp [R]` rows the regnorm multiset still reports are a SCORER
 * ARTEFACT, not a gap: a member at record offset 0 has a zero reloc
 * addend, so capstone prints `[esi]` where the original, whose
 * displacement is the resolved absolute, prints `[esi+0x10AC0C48]`.)
 *
 * PARKED at -2 bytes / -1 instruction, register-blind 2+3.  The whole
 * residue is ONE instruction: after `and r,0xFFFF` on the merged head the
 * original emits a redundant `test r,r` before the loop-entry `je`; our
 * cl fuses the two and branches on the AND's flags.  Its sibling
 * 0x10033880 fuses them in the ORIGINAL too (there the `and` is
 * separated from the `je` by two unrelated instructions), so this is an
 * emitter peephole, not a source shape.
 * DEAD PROBES -- none of these move it off 2+3:
 *   guard shape: `while (iRec != 0)`, `if (iRec != 0) do {...} while`,
 *     the mask hoisted out of the two arms vs applied inside each (the
 *     latter costs a second `and`)
 *   mask spelling: `iRec &= 0xFFFFu`, `iRec = head & 0xFFFFu` through a
 *     separate `head` local, `iRec = (unsigned short)iRec`
 *   index type: `int iRec` with a signed mask is WORSE (4+5)
 *   flags: /O2 (best), /Od, /O2 /Oy-, /O2 /Op
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
extern int32_t  g_iPfxHeadB4;   /* 0x10AC0C44 -- dword read, low word is
                                 * the head */
extern int32_t  g_iPfxHeadAC;   /* 0x10AC0C3C */
extern uint16_t g_iPfxFree;     /* 0x10AC0C38 */
extern const float kPfx0_7;     /* 0x100775D4  0.7     */
extern const float kPfxRecip;   /* 0x100775C4  1/65280 */
extern const float kPfxNeg0_8;  /* 0x100775C8  -0.8    */
extern const float kPfxCell;    /* 0x100775D0  0.03125 */
extern const float kPfx19_62;   /* 0x100775D8  19.62   */
extern const float kPfx102;     /* 0x100775DC  102.0   */
extern const float kPfxNeg30;   /* 0x100775E0  -30.0   */

/* @implements 0x100339C0 glide BrPfxUpdateB4AC */
void BrPfxUpdateB4AC(void)
{
    float k = g_fPfxDt * kPfx0_7;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        uint16_t *piLink;
        unsigned iRec;
        int iNext;

        if (pass != 0) {
            iRec   = (unsigned)g_iPfxHeadAC;
            piLink = (uint16_t *)&g_iPfxHeadAC;
        } else {
            iRec   = (unsigned)g_iPfxHeadB4;
            piLink = (uint16_t *)&g_iPfxHeadB4;
        }
        iRec &= 0xFFFFu;

        while (iRec != 0) {
            float scale;

            iNext = g_aPfxRec[iRec].iNext;
            g_aPfxRec[iRec].age = k + g_aPfxRec[iRec].age;
            scale = (float)((int)g_aPfxRec[iRec].f1F * (int)g_aPfxRec[iRec].f1E)
                  * kPfxRecip;

            g_aPfxRec[iRec].pos.x = ((g_aPfxRec[iRec].vel.x * scale) * g_fPfxDt
                                  + g_vPfxDrift.x) + g_aPfxRec[iRec].pos.x;
            g_aPfxRec[iRec].pos.y = ((g_aPfxRec[iRec].vel.y * scale) * g_fPfxDt
                                  + g_vPfxDrift.y) + g_aPfxRec[iRec].pos.y;
            g_aPfxRec[iRec].pos.z = ((scale * g_aPfxRec[iRec].vel.z - kPfxNeg0_8)
                                  * g_fPfxDt + g_vPfxDrift.z)
                                  + g_aPfxRec[iRec].pos.z;

            g_aPfxRec[iRec].vel.z = g_aPfxRec[iRec].vel.z
                                  - g_fPfxDt * kPfx19_62;

            g_aPfxRec[iRec].f1E =
                (uint8_t)(int32_t)(kPfx102 / g_aPfxRec[iRec].age);

            if (scale < kPfxCell || g_aPfxRec[iRec].vel.z < kPfxNeg30) {
                *piLink = g_aPfxRec[iRec].iNext;
                g_aPfxRec[iRec].iNext = g_iPfxFree;
                g_iPfxFree = (uint16_t)iRec;
            } else {
                piLink = &g_aPfxRec[iRec].iNext;
            }

            iRec = iNext;
        }
    }
}

#endif /* BR_MATCHING_BUILD */
