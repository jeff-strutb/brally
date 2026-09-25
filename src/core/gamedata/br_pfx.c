/* br_pfx.c -- gamedata: the particle record pool.
 *
 * The free list every dust and spray particle is drawn from, the reset that
 * hands all of them back, and (Glide) the wheel spawner that draws from it.
 * Filed out of slice2_21.c sections 5 and 6.
 *
 * See slice2_21.h for the field offsets and the gotchas.
 */
#ifdef BR_MATCHING_BUILD
#define BrPfxReset      BrPfxReset_port
#define BrCarPfxSpawn   BrCarPfxSpawn_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrPfxReset
#undef BrCarPfxSpawn
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
#endif
