/* br_pfx.c -- gamedata: the particle record pool.
 *
 * The free list every dust and spray particle is drawn from, and the reset
 * that hands all of them back. Filed out of slice2_21.c section 5.
 *
 * See slice2_21.h for the field offsets and the gotchas.
 */
#ifdef BR_MATCHING_BUILD
#define BrPfxReset      BrPfxReset_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrPfxReset
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
