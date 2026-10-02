/* br_pfx.c -- gamedata: the particle record pool.
 *
 * The free list every dust and spray particle is drawn from, the reset that
 * hands all of them back, and (Glide) the wheel spawner that draws from it.
 * Filed out of slice2_21.c sections 5 and 6.
 *
 * See slice2_21.h for the field offsets and the gotchas.
 */
/* The header declares the port's pool/env-parameter forms; the originals
 * take no arguments (or the car alone) and reach the pool as globals. */
#define BrPfxReset      BrPfxReset_port
#define BrCarPfxSpawn   BrCarPfxSpawn_port
#define BrPfxUpdateB0   BrPfxUpdateB0_port
#define BrPfxUpdateB4AC BrPfxUpdateB4AC_port
#define BrPfxTick       BrPfxTick_port
#define BrPfxSaveState  BrPfxSaveState_port
#define BrCarSub9020    BrCarSub9020_port
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_21.h"
#include "br_cartypes.h"
#include "slice3_41.h"
#undef BrPfxReset
#undef BrCarPfxSpawn
#undef BrPfxUpdateB0
#undef BrPfxUpdateB4AC
#undef BrPfxTick
#undef BrPfxSaveState
#undef BrCarSub9020
/* BrPfxReset: prototype in br_funcs.h */

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* aRec[1].iNext, stride 0x20 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* loop end (exclusive) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* aRec[255].iNext */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* iFree */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* iListB0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* iListAC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* iListB4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* nCars */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* car0 + 0x105C, stride 0x2B68 */
void BrPfxReset(void)
{
    int i = 1;
    unsigned char *p = (*(unsigned char (*)[])((char *)&g_aPfxRec + 0x3C));
    int32_t n;

    do {
        *(uint16_t *)p = (uint16_t)(i + 1);
        ++i;
        p += 0x20;
    } while ((int)p < (int)(*(unsigned char (*)[])&g_BrVisColLo[1]));

    n = g_BrCarCount;
    (*(uint16_t *)((char *)&g_aPfxRec + 0x1FFC)) = 0;
    g_iPfxFree = 1;
    if (n > 0) {
        int32_t *car = (*(int32_t (*)[])&g_aBrRaceCar[0].f105C);
        do {
            *car = 0;
            car = (int32_t *)((unsigned char *)car + 0x2B68);
            --n;
        } while (n != 0);
    }
    (*(uint16_t *)&g_iPfxHeadB0) = 0;
    (*(uint16_t *)&g_iPfxHeadAC) = 0;
    (*(uint16_t *)&g_iPfxHeadB4) = 0;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x106E9D8C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC0C48 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AC0C38 */


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
void __fastcall BrCarPfxSpawn(struct BrDriverCar *pCar)
{
/* BrRandom: prototype in br_funcs.h */
    float rate, kFwd, kSide, s;
    int i;

    if (((unsigned char *)&pCar->aBody[0].f0209)[0] == 0)
        return;
    if (((pCar->f1030)) <= 40.0f)    /* 0x100775B0 */
        return;

    rate = g_brRaceFlyStep * 0.5f - ((pCar->f1030)) * -0.0006600660271942616f;

    for (i = 0; i < 4; i++) {
        unsigned char *aW[4];
        BrCarBody *pW;
        BrVec3 *pVel, *pPos, *pPrev;
        BrVec3 saved;
        unsigned iRec;
        float acc;

        acc = ((float)(int)((unsigned char *)&pCar->aBody[0].f0209)[0] * 0.029999999329447746f) * rate
            + ((*(float *)   ((unsigned char *)&pCar->f106C[0] + 4u * (unsigned)i)));
        ((*(float *)   ((unsigned char *)&pCar->f106C[0] + 4u * (unsigned)i))) = acc;
        if (acc <= 0.75f)
            continue;

        aW[0] = ((unsigned char *)&pCar->aBody[4]);
        aW[1] = ((unsigned char *)&pCar->aBody[2]);
        aW[2] = ((unsigned char *)&pCar->aBody[1]);
        aW[3] = ((unsigned char *)&pCar->aBody[3]);
        pW = aW[i];
        ((*(float *)   ((unsigned char *)&pCar->f106C[0] + 4u * (unsigned)i))) = 0.0f;

        if (*(int32_t *)&pW->rb.f1B4 == 0)
            continue;
        if (*(signed char *)&pW->rb.f01A0 <= 0 || *(signed char *)&pW->rb.f01A0 > 3)
            continue;

        iRec = (unsigned)*(int32_t *)&g_iPfxFree & 0xFFFFu;
        if (iRec == 0)
            continue;
        g_iPfxFree = g_aPfxRec[iRec].iNext;
        if (*(signed char *)&pW->rb.f01A0 != 3) {
            g_aPfxRec[iRec].iNext = (*(uint16_t *)&g_iPfxHeadAC);
            (*(uint16_t *)&g_iPfxHeadAC) = (uint16_t)iRec;
        } else {
            g_aPfxRec[iRec].iNext = (*(uint16_t *)&g_iPfxHeadB4);
            (*(uint16_t *)&g_iPfxHeadB4) = (uint16_t)iRec;
        }

        pVel = &g_aPfxRec[iRec].vel;
        BrVec3Scale(pVel, ((&pCar->f1024)), 0.20000000298023224f);
        kFwd = 0.25f;
        if (i >= 2)
            kFwd = 2.0f;
        BrVec3MulAddTo(pVel, ((&pCar->up)), kFwd);
        if (i < 2) {
            kSide = -0.5f;
            if (i == 0)
                kSide = 0.5f;
            BrVec3MulAddTo(pVel, ((&pCar->right)), kSide);
        }
        BrVec3SubFrom(pVel, (((BrVec3 *)   ((unsigned char *)&pCar->fwd))));
        s = 1.0f - 50.0f / (((pCar->f1030)) - -50.0f);
        BrVec3ScaleBy(pVel, s);

        pPos = &g_aPfxRec[iRec].pos;
        BrVec3Sub(pPos, (((BrVec3 *)   ((unsigned char *)&pCar->aWheel[i].m[3][0]))), (((BrVec3 *)   ((unsigned char *)&pCar->fwd))));
        g_aPfxRec[iRec].pos.z = g_aPfxRec[iRec].pos.z - ((pCar->fHitDist)) * -0.5f;

        saved.x = pPos->x; saved.y = pPos->y; saved.z = pPos->z;
        pPrev = (((BrVec3 *)   ((unsigned char *)&pCar->f107C + 12u * (unsigned)i)));
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

/* Glide match for BrPfxUpdateB0: 0x10033880
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A200 d3d) and
 * takes `(BrPfxPool *, const BrPfxEnv *)`.  The original takes NOTHING:
 * its call site at 0x10033BB0 pushes no arguments at all, because dt,
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x106E9D8C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x104ADD40 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC0C48 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC0C40 -- dword read, low word is

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC0C38 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100775C4  1/65280 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100775C8  -0.8    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100775CC  5.7375  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100775D0  0.03125 */

/* WHAT IT DOES: advance one whole list of particles by a frame: ages each
 * one, moves it by its own velocity plus the global drift, applies a
 * downward pull, and fades its size. This is the per-frame physics for dust,
 * smoke and spray. */
/* @implements 0x10033880 glide BrPfxUpdateB0 */
void BrPfxUpdateB0(void)
{
    /* A literal, not an extern const: VC5 orders `fld var; fmul const` only
     * for a literal (the original's 0.3f sits at 0x1007758C).  With an extern
     * it swapped the two operand addresses -- the product is the same, but the
     * relocations named the wrong object at each site. */
    float k = g_brRaceFlyStep * 0.3f;
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

        g_aPfxRec[iRec].pos.x = (g_aPfxRec[iRec].vel.x * scale) * g_brRaceFlyStep
                              + g_vPfxDrift.x + g_aPfxRec[iRec].pos.x;
        g_aPfxRec[iRec].pos.y = ((g_aPfxRec[iRec].vel.y * scale) * g_brRaceFlyStep
                              + g_vPfxDrift.y) + g_aPfxRec[iRec].pos.y;
        g_aPfxRec[iRec].pos.z = ((scale * g_aPfxRec[iRec].vel.z - kPfxNeg0_8)
                              * g_brRaceFlyStep + g_vPfxDrift.z) + g_aPfxRec[iRec].pos.z;

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

/* Glide match for BrPfxUpdateB4AC: 0x100339C0
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x106E9D8C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x104ADD40 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10AC0C48 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC0C44 -- dword read, low word is

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC0C3C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC0C38 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100775D4  0.7     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100775C4  1/65280 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100775C8  -0.8    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100775D0  0.03125 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100775D8  19.62   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x100775DC  102.0   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100775E0  -30.0   */

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
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x100339C0 glide BrPfxUpdateB4AC */
void BrPfxUpdateB4AC(void)
{
    float k = g_brRaceFlyStep * kPfx0_7;
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

            g_aPfxRec[iRec].pos.x = ((g_aPfxRec[iRec].vel.x * scale) * g_brRaceFlyStep
                                  + g_vPfxDrift.x) + g_aPfxRec[iRec].pos.x;
            g_aPfxRec[iRec].pos.y = ((g_aPfxRec[iRec].vel.y * scale) * g_brRaceFlyStep
                                  + g_vPfxDrift.y) + g_aPfxRec[iRec].pos.y;
            g_aPfxRec[iRec].pos.z = ((scale * g_aPfxRec[iRec].vel.z - kPfxNeg0_8)
                                  * g_brRaceFlyStep + g_vPfxDrift.z)
                                  + g_aPfxRec[iRec].pos.z;

            g_aPfxRec[iRec].vel.z = g_aPfxRec[iRec].vel.z
                                  - g_brRaceFlyStep * kPfx19_62;

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

/* Glide match for BrPfxTick: 0x10033BB0
 *
 * The port body lives in src/core/slice2_21.c (tagged 0x1003A530 d3d) and
 * carries the aggregate parameters `(pPool, pEnv, pFxEnv, pTick, pSeed)`
 * the port introduced.  The original takes NO arguments at all: the pool,
 * the two mode words, the driver count and the driver-slot table are
 * globals, and the three per-car helpers are __fastcall on the car
 * pointer alone (`mov ecx,[esi]` / `call`).  That parameter list is the
 * whole reason the port body could never converge: see the
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* pool-initialised flag */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* mode: age the B0 family */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* mode word */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* mode flag */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* driver count */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrPfxUpdateB0: prototype in br_funcs.h */
/* BrPfxUpdateB4AC: prototype in br_funcs.h */
/* BrCarSub9020: prototype in br_funcs.h */
/* BrCarPfxSpawn: prototype in br_funcs.h */

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

    if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0) {
        BrPfxUpdateB0();
        for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) {
            if (g_aBrRaceDriver[i].pCar != 0) {
                BrCarSub9020(g_aBrRaceDriver[i].pCar);
                BrCarWheelFx(g_aBrRaceDriver[i].pCar);
            }
        }
        return;
    }

    if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) == 0 && (*(int *)((char *)&g_aBrEntRecs + 0x84)) == 0) {
        BrPfxUpdateB4AC();
        for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) {
            if (g_aBrRaceDriver[i].pCar != 0) {
                BrCarPfxSpawn(g_aBrRaceDriver[i].pCar);
                BrCarWheelFx(g_aBrRaceDriver[i].pCar);
            }
        }
        return;
    }

    for (i = 0; i < (*(int *)&g_brRaceNDriver); i++) {
        if (g_aBrRaceDriver[i].pCar != 0)
            BrCarWheelFx(g_aBrRaceDriver[i].pCar);
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
    pOut[0] = (short)g_iPfxFree;
    pOut[1] = (*(uint16_t *)&g_iPfxHeadB0);
    pOut[2] = (short)(*(uint16_t *)&g_iPfxHeadAC);
    pOut[3] = (short)(*(uint16_t *)&g_iPfxHeadB4);
    memcpy(pOut + 4, g_aPfxRec, 8192);
}
