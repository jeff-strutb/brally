/* br_pfx.c -- gamedata: the particle record pool.
 *
 * The free list every dust and spray particle is drawn from, the reset that
 * hands all of them back, and (Glide) the wheel spawner that draws from it.
 * Filed out of slice2_21.c sections 5 and 6.
 *
 * See slice2_21.h for the field offsets and the gotchas.
 */
#ifdef BR_MATCHING_BUILD
/* The header declares the port's pool/env-parameter forms; the originals
 * take no arguments (or the car alone) and reach the pool as globals. */
#define BrPfxReset      BrPfxReset_port
#define BrCarPfxSpawn   BrCarPfxSpawn_port
#define BrPfxUpdateB0   BrPfxUpdateB0_port
#define BrPfxUpdateB4AC BrPfxUpdateB4AC_port
#define BrPfxTick       BrPfxTick_port
#define BrPfxSaveState  BrPfxSaveState_port
#define BrCarSub9020    BrCarSub9020_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrPfxReset
#undef BrCarPfxSpawn
#undef BrPfxUpdateB0
#undef BrPfxUpdateB4AC
#undef BrPfxTick
#undef BrPfxSaveState
#undef BrCarSub9020
void BrPfxReset(void);
#endif

#include <string.h>

/* --------------------------------------------------------------------------
 * 5. Particle pool
 * -------------------------------------------------------------------------- */

/* 0x1003A4D0, free-list half. */
/* WHAT IT DOES: empties the particle pool -- puts every record back on the
 * free list and clears the three lists of particles in flight, so all the dust
 * and spray currently in the air vanishes. */
/* @implements 0x1003A4D0 d3d BrPfxReset */
/* @implements 0x10033B50 glide BrPfxReset */
#ifdef BR_MATCHING_BUILD
extern unsigned char DAT_10ac0c84[];   /* aRec[1].iNext, stride 0x20 */
extern unsigned char DAT_10ac2c64[];   /* loop end (exclusive) */
extern uint16_t DAT_10ac2c44;          /* aRec[255].iNext */
extern uint16_t DAT_10ac0c38;          /* iFree */
extern uint16_t DAT_10ac0c40;          /* iListB0 */
extern uint16_t DAT_10ac0c3c;          /* iListAC */
extern uint16_t DAT_10ac0c44;          /* iListB4 */
extern int32_t  DAT_100b2f04;          /* nCars */
extern int32_t  DAT_10af2264[];        /* car0 + 0x105C, stride 0x2B68 */
void BrPfxReset(void)
{
    int i = 1;
    unsigned char *p = DAT_10ac0c84;
    int32_t n;

    do {
        *(uint16_t *)p = (uint16_t)(i + 1);
        ++i;
        p += 0x20;
    } while ((int)p < (int)DAT_10ac2c64);

    n = DAT_100b2f04;
    DAT_10ac2c44 = 0;
    DAT_10ac0c38 = 1;
    if (n > 0) {
        int32_t *car = DAT_10af2264;
        do {
            *car = 0;
            car = (int32_t *)((unsigned char *)car + 0x2B68);
            --n;
        } while (n != 0);
    }
    DAT_10ac0c40 = 0;
    DAT_10ac0c3c = 0;
    DAT_10ac0c44 = 0;
}
#else
void BrPfxReset(BrPfxPool *pPool)
{
    int i;
    for (i = 1; i <= BR_PFX_RECS - 1; i++)
        pPool->aRec[i].iNext = (uint16_t)(i + 1);
    pPool->aRec[BR_PFX_RECS - 1].iNext = 0;   /* written after the loop */
    pPool->iFree    = 1;
    pPool->iListB0  = 0;
    pPool->iListAC  = 0;
    pPool->iListB4  = 0;
}
#endif

#ifdef BR_MATCHING_BUILD
extern float    g_fPfxDt;        /* 0x106E9D8C */
extern BrPfxRec g_aPfxRec[];     /* 0x10AC0C48 */
extern uint16_t g_iPfxFree;      /* 0x10AC0C38 */

#define CAR_B(p, o)   ((unsigned char *)(p) + (o))
#define CAR_F(p, o)   (*(float *)   CAR_B(p, o))
#define CAR_V(p, o)   ((BrVec3 *)   CAR_B(p, o))

/* 0x100335A0 */
/* Transcribed from the Glide bytes: __fastcall with the car in ecx (`mov
 * esi,ecx`), no stack arguments; dt (0x106E9D8C), the record array
 * (0x10AC0C48, 32-byte records, 1-based), the free head (0x10AC0C38, read as
 * a dword and masked) and the two list heads (0x10AC0C3C for surfaces 1-2,
 * 0x10AC0C44 for surface 3, moved as words) are globals. */
/* WHAT IT DOES: throws dust and spray up from a car's wheels. It only does
 * anything above about forty units of speed, and then only for wheels actually
 * touching a loose surface; the faster the car goes the more often each wheel
 * emits, and each new particle is flung backwards and outwards from the wheel,
 * with the two front wheels also thrown sideways. New particles are nudged part
 * of the way toward where that wheel emitted last time, so a spray follows the
 * wheel's path instead of appearing in a line of separate puffs. */
/* @implements 0x100335A0 glide BrCarPfxSpawn */
void __fastcall BrCarPfxSpawn(struct BrCar *pCar)
{
    extern int      BrRandom(void);      /* 0x100353D0                   */
    float rate, kFwd, kSide, s;
    int i;

    if (CAR_B(pCar, 0x36D)[0] == 0)
        return;
    if (CAR_F(pCar, 0x1030) <= 40.0f)    /* 0x100775B0 */
        return;

    rate = g_fPfxDt * 0.5f - CAR_F(pCar, 0x1030) * -0.0006600660271942616f;

    for (i = 0; i < 4; i++) {
        unsigned char *aW[4];
        unsigned char *pW;
        BrVec3 *pVel, *pPos, *pPrev;
        BrVec3 saved;
        unsigned iRec;
        float acc;

        acc = ((float)(int)CAR_B(pCar, 0x36D)[0] * 0.029999999329447746f) * rate
            + CAR_F(pCar, 0x106C + 4u * (unsigned)i);
        CAR_F(pCar, 0x106C + 4u * (unsigned)i) = acc;
        if (acc <= 0.75f)
            continue;

        aW[0] = CAR_B(pCar, 0x994);
        aW[1] = CAR_B(pCar, 0x57C);
        aW[2] = CAR_B(pCar, 0x370);
        aW[3] = CAR_B(pCar, 0x788);
        pW = aW[i];
        CAR_F(pCar, 0x106C + 4u * (unsigned)i) = 0.0f;

        if (*(int32_t *)(pW + 0x1B4) == 0)
            continue;
        if (*(signed char *)(pW + 0x1A0) <= 0 || *(signed char *)(pW + 0x1A0) > 3)
            continue;

        iRec = (unsigned)*(int32_t *)&DAT_10ac0c38 & 0xFFFFu;
        if (iRec == 0)
            continue;
        g_iPfxFree = g_aPfxRec[iRec].iNext;
        if (*(signed char *)(pW + 0x1A0) != 3) {
            g_aPfxRec[iRec].iNext = DAT_10ac0c3c;
            DAT_10ac0c3c = (uint16_t)iRec;
        } else {
            g_aPfxRec[iRec].iNext = DAT_10ac0c44;
            DAT_10ac0c44 = (uint16_t)iRec;
        }

        pVel = &g_aPfxRec[iRec].vel;
        BrVec3Scale(pVel, CAR_V(pCar, 0x1024), 0.20000000298023224f);
        kFwd = 0.25f;
        if (i >= 2)
            kFwd = 2.0f;
        BrVec3MulAddTo(pVel, CAR_V(pCar, 0x20), kFwd);
        if (i < 2) {
            kSide = -0.5f;
            if (i == 0)
                kSide = 0.5f;
            BrVec3MulAddTo(pVel, CAR_V(pCar, 0x10), kSide);
        }
        BrVec3SubFrom(pVel, CAR_V(pCar, 0x00));
        s = 1.0f - 50.0f / (CAR_F(pCar, 0x1030) - -50.0f);
        BrVec3ScaleBy(pVel, s);

        pPos = &g_aPfxRec[iRec].pos;
        BrVec3Sub(pPos, CAR_V(pCar, 0x70 + 0x40u * (unsigned)i), CAR_V(pCar, 0x00));
        g_aPfxRec[iRec].pos.z = g_aPfxRec[iRec].pos.z - CAR_F(pCar, 0x2994) * -0.5f;

        saved.x = pPos->x; saved.y = pPos->y; saved.z = pPos->z;
        pPrev = CAR_V(pCar, 0x107C + 12u * (unsigned)i);
        if (BrVec3DistSq(pPos, pPrev) < 256.0f) {
            float u = (float)(BrRandom() & 0xFFFF) * 1.5259021893143654e-05f;

            BrVec3Lerp(pPos, pPrev, pPos, u * u);
        }
        pPrev->x = saved.x;
        pPrev->y = saved.y;
        pPrev->z = saved.z;
        g_aPfxRec[iRec].age = 0.4000000059604645f;
        g_aPfxRec[iRec].f1E = 0x19;
        g_aPfxRec[iRec].f1F = (uint8_t)(int32_t)(16.0f - s * -167.3000030517578f);
    }
}

/* Glide match for BrPfxUpdateB0 - 0x10033880
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A200 d3d) and
 * takes `(BrPfxPool *, const BrPfxEnv *)`.  The original takes NOTHING - 
 * its call site at 0x10033BB0 pushes no arguments at all - because dt,
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
extern float    g_fPfxDt;       /* 0x106E9D8C */
extern BrVec3  g_vPfxDrift;    /* 0x104ADD40 */
extern BrPfxRec   g_aPfxRec[];    /* 0x10AC0C48 */
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

/* Glide match for BrPfxUpdateB4AC - 0x100339C0
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
extern float    g_fPfxDt;       /* 0x106E9D8C */
extern BrVec3  g_vPfxDrift;    /* 0x104ADD40 */
extern BrPfxRec   g_aPfxRec[];    /* 0x10AC0C48 */
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

/* WHAT IT DOES: advance two more particle lists by a frame, in the same way
 * BrPfxUpdateB0 handles its own -- ages, moves and fades each particle. The
 * two passes are two separate lists sharing one loop. */
/* @t4-pass 0x100339C0 1 2026-09-07 probes 116 bytes 396 insns 96 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100339C0 2 2026-09-07 probes 121 bytes 396 insns 96 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t3 0x100339C0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 396/398 insns 96/97 rows 1+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is allocation/scheduling: 1+0 classified rows, 2 masked regions, 2 B short;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (project rule 12). */
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

/* Glide match for BrPfxTick - 0x10033BB0
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A530 d3d) and
 * carries the aggregate parameters `(pPool, pEnv, pFxEnv, pTick, pSeed)`
 * the port introduced.  The original takes NO arguments at all: the pool,
 * the two mode words, the driver count and the driver-slot table are
 * globals, and the three per-car helpers are __fastcall on the car
 * pointer alone (`mov ecx,[esi]` / `call`).  That parameter list is the
 * whole reason the port body could never converge - see the
 * port-safety/globals-struct class in docs/VC5-IDIOMS.md.
 *
 * Shape notes, read off the original:
 *  - the driver table at 0x10AF0858 has a 0x80-byte stride with the car
 *    pointer at +0; VC5 strength-reduces the subscript into the `add
 *    esi,0x80` pointer walk.
 *  - the loop bound `DAT_100b2f00` is RE-READ at the bottom of every
 *    iteration, which is what a plain `i < global` for-loop emits once a
 *    call in the body can clobber it.
 *  - the car pointer is re-loaded for the second call in the first two
 *    loops (the call clobbers ecx), so each arm spells the slot read out
 *    rather than caching it in a local.
 *  - the two early `return`s give loops 1 and 2 their own
 *    `pop edi/pop esi/ret`; all three `jle` exits share loop 3's.
 */
typedef struct {
    void *pCar;                 /* +0x00 */
    char  pad[0x7C];
} BrPfxDriverSlot;              /* 0x80 */

extern int DAT_10ac2c48;        /* pool-initialised flag */
extern int DAT_106ed6b0;        /* mode: age the B0 family */
extern int DAT_106ed6ac;        /* mode word */
extern int DAT_106ed6b4;        /* mode flag */
extern int DAT_100b2f00;        /* driver count */
extern BrPfxDriverSlot DAT_10af0858[];

void BrPfxUpdateB0(void);
void BrPfxUpdateB4AC(void);
void __fastcall BrCarSub9020(struct BrCar *pCar);   /* 0x10039020, thiscall */
void __fastcall BrCarPfxSpawn(struct BrCar *pCar);

/* WHAT IT DOES: run the particle system for one frame -- initialises it on
 * the very first call, then updates whichever particle lists the current
 * mode uses and gives every active car a chance to throw up its own wheel
 * effects. */
/* @implements 0x10033BB0 glide BrPfxTick */
void BrPfxTick(void)
{
    int i;

    if (DAT_10ac2c48 == 0) {
        BrPfxReset();
        DAT_10ac2c48 = 1;
    }

    if (DAT_106ed6b0 != 0) {
        BrPfxUpdateB0();
        for (i = 0; i < DAT_100b2f00; i++) {
            if (DAT_10af0858[i].pCar != 0) {
                BrCarSub9020(DAT_10af0858[i].pCar);
                BrCarWheelFx(DAT_10af0858[i].pCar);
            }
        }
        return;
    }

    if (DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
        BrPfxUpdateB4AC();
        for (i = 0; i < DAT_100b2f00; i++) {
            if (DAT_10af0858[i].pCar != 0) {
                BrCarPfxSpawn(DAT_10af0858[i].pCar);
                BrCarWheelFx(DAT_10af0858[i].pCar);
            }
        }
        return;
    }

    for (i = 0; i < DAT_100b2f00; i++) {
        if (DAT_10af0858[i].pCar != 0)
            BrCarWheelFx(DAT_10af0858[i].pCar);
    }
}

/* 0x10033C90 */
/* WHAT IT DOES: copy the entire particle system state into a caller's buffer
 * -- the four list heads in the order free, B0, AC, B4 (not their address
 * order), then the whole 8 KB record pool.  Used to snapshot particles so
 * they survive something that would otherwise reset them. */
/* @implements 0x10033C90 glide BrPfxSaveState */
void BrPfxSaveState(short *pOut)
{
    pOut[0] = (short)DAT_10ac0c38;
    pOut[1] = DAT_10ac0c40;
    pOut[2] = (short)DAT_10ac0c3c;
    pOut[3] = (short)DAT_10ac0c44;
    memcpy(pOut + 4, g_aPfxRec, 8192);
}
#endif
