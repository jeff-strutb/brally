/* br_carinit.c -- racing: setting a car up for a race and for a frame.
 *
 * Rebuilding the transforms that place a car and its parts in the world, and
 * the per-race reset of a car's working tables. Filed out of slice3_40.c
 * section 5.
 *
 * See slice3_40.h for the offset maps and every GOTCHA.
 *
 * x87 NOTE: the original is MSVC x87 code and the CRT leaves the precision
 * control at 53 bits, so an intermediate the original never spills carries
 * double precision, not float.
 */

#include <string.h>

#include "slice3_40.h"

#include "br_match.h"    /* BR_THISCALL1 */

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))
#define CAR_AT(c, off)   ((void *)(CAR_BYTES(c) + (off)))

/* 0x10061BE0 */
/* WHAT IT DOES: rebuilds the four transform matrices that place a car and
 * its parts in the world, from the physics state of each. Called after
 * anything moves the car -- the physics step, or a network update. */
/* @implements 0x10061BE0 d3d BrCarBuildMatrices */
/* @implements 0x1005AC60 glide BrCarBuildMatrices */
void BrCarBuildMatrices(BrCar *pCar)
{
    uint8_t *pSub;

    BrSub1006F4A0(CAR_AT(pCar, 0x164));

    /* Orig unrolls the four sub-object pointers at +0x168..+0x174. */
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 0);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 1);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 2);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
    pSub = (uint8_t *)BR_CAR_SUBPTR(pCar, 3);
    BrRbBuildMatrix((BrMat4 *)(void *)(pSub + BR_CARSUB_MAT),
                    (const BrRbState *)(void *)(pSub + BR_CARSUB_RB));
}
