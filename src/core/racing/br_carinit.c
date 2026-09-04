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

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#endif

#include "br_match.h"    /* BR_THISCALL1 */

/* .rdata constants, read out of the original rather than guessed. */
#define BR_K_08F9AC    0.137f          /* 0x1008F9AC */
#define BR_K_08F9B0 (-0.034f)          /* 0x1008F9B0 -- SUBTRACTED, so +0.034 */
#define BR_K_08F9B4    0.15f           /* 0x1008F9B4 */

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))
#define CAR_AT(c, off)   ((void *)(CAR_BYTES(c) + (off)))
#define CAR_U16(c, off)  (*(uint16_t *)CAR_AT(c, off))
#define CAR_I32(c, off)  (*(int32_t  *)CAR_AT(c, off))
#define CAR_U32(c, off)  (*(uint32_t *)CAR_AT(c, off))
#define CAR_F32(c, off)  (*(float    *)CAR_AT(c, off))

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

/* 0x10065630 */
/* WHAT IT DOES: resets a car's working tables at the start of a race: a set
 * of thresholds spaced evenly apart, several arrays of counters, and a block
 * of paired values seeded with a fixed pattern. It opens by computing four
 * floats and then immediately zeroes the very slots it just wrote -- a dead
 * store that is in the original and is kept. */
/* @implements 0x10065630 d3d BrCarInitTables */
/* Original is __thiscall (`fild [ecx+0x140]` / `ret`).  BR_THISCALL1 is the
 * single-arg fastcall spelling; the header stays cdecl. */
void BR_THISCALL1 BrCarInitTables(BrCar *pCar)
{
    /* float, not double: MSVC then emits `fmul/fsub dword` against .rdata.
     * On x87 the unspilled product still has 53-bit precision until fstp. */
    float v = CAR_I32(pCar, 0x140) * BR_K_08F9AC;
    float a1, a2, a3;
    int32_t *p;
    uint8_t *pW;
    uint8_t *pR;
    int i, j, k;
    int two;

    p = (int32_t *)CAR_AT(pCar, 0x10AC);
    a1 = v - BR_K_08F9B0;
    /* DEAD STORES -- see the header.  The loop below zeroes all four. */
    *(float *)(void *)p = v;

    /* Hoist both later walking pointers so they occupy registers across
     * the first loop, as the original's early lea edi/eax do. */
    pW = CAR_BYTES(pCar) + 0x2320;
    pR = CAR_BYTES(pCar) + 0x1120;

    a2 = a1 - BR_K_08F9B0;
    CAR_F32(pCar, 0x10B0) = a1;
    two = 2;
    a3 = a2 - BR_K_08F9B0;
    CAR_F32(pCar, 0x10B4) = a2;
    CAR_F32(pCar, 0x10B8) = a3;

    i = 0;
    do {
        /* named so fild/fmul stay on the x87 stack; ++i then ++p matches
         * the original `inc ebx` / `add edx,4` order. */
        float f = i * BR_K_08F9B4;
        p[4] = two;
        p[0] = 0;
        p[12] = 0;
        p[8] = 0;
        ++i;
        ++p;
        /* orig: add edx,4 then fstp [edx-0x44] (= -17 floats) */
        ((float *)(void *)p)[-17] = f;
    } while (i < 4);

    for (j = 0; j < 0x90; ++j) {
        *(uint16_t *)(void *)(pW + 4) = 0;
        *(uint16_t *)(void *)(pW + 2) = 0;
        *(uint16_t *)(void *)(pW + 0) = 0;

        *(int32_t *)(void *)(pR + 0x00) = 0;
        *(int32_t *)(void *)(pR + 0x04) = 0;
        *(int32_t *)(void *)(pR + 0x08) = 0;
        *(int32_t *)(void *)(pR + 0x14) = 0;
        /* orig `test dl,1` two-way branch.  Both arms write zero; the odd
         * arm reloads the dword just stored at +0x00 so MSVC cannot fold
         * the stores (mov ebx, esi / mov [eax+0x18], ebx vs esi). */
        if (j & 1) {
            *(int32_t *)(void *)(pR + 0x18) = *(int32_t *)(void *)(pR + 0x00);
        } else {
            *(int32_t *)(void *)(pR + 0x18) = 0;
        }
        *(int32_t *)(void *)(pR + 0x1C) = 0;
        /* +0x0C and +0x10 are deliberately left alone */
        pW += 6;
        pR += 0x20;
    }

    for (k = 0; k < 0x12; ++k) {
        CAR_U32(pCar, 0x2680 + 4 * k) = 0x00020002u;
    }
}

/* 0x10065710 */
/* WHAT IT DOES: clears a small block of a car's bookkeeping -- four numbers
 * and one half-sized one -- back to zero. */
/* @implements 0x10065710 d3d BrCarClear29C8 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall, no stack args: `xor eax,eax` then stores through ecx.
 * BR_THISCALL1 is __fastcall with one arg -- ecx = this, identical bytes. */
void BR_THISCALL1 BrCarClear29C8(BrCar *pCar)
#else
void BrCarClear29C8(BrCar *pCar)
#endif
{
    CAR_I32(pCar, 0x29C8) = 0;
    CAR_I32(pCar, 0x29CC) = 0;
    CAR_I32(pCar, 0x29D0) = 0;
    CAR_I32(pCar, 0x29D4) = 0;
    CAR_U16(pCar, 0x29D8) = 0;   /* a WORD, not a dword */
}
