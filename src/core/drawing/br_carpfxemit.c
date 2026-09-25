/* br_carpfxemit.c -- drawing: a car's particle emitter, one frame.
 *
 * BrCarSub9020 counts up the car's emit timer and, past a quarter second,
 * pops a node off the particle free list and starts a particle at the car
 * with a velocity built from the car's motion.
 *
 * Filed out of the address batch slice4_53.c; the preamble is slice4_53.c's,
 * carried whole (its #pragma function lines included; the forwarder bodies
 * between them stay in slice4_53.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port2
#include "slice4_53.h"
#undef BrCarSub9020
#else
#include "slice4_53.h"
#endif
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_17.h"      /* BrS17BankFlip, BrRenderCountersReset       */
#include "slice2_18.h"      /* BrGfx2C210, BrGfx31227 declarations        */
#include "slice2_19.h"      /* BrSub10002240, BrSub100088B0, BrSub10037740 */
#include "slice2_20.h"      /* BrPoolEmit, BrRcaLoadCar                   */
#ifdef BR_MATCHING_BUILD
#define BrCarSub9020 BrCarSub9020_port
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#undef BrCarSub9020
#else
#include "slice2_21.h"      /* BrSinF, BrSqrtF, BrCarSub9020              */
#endif
#include "slice2_22.h"      /* BrDPlayLink, BrDPlaySendTag4               */
#include "slice2_24.h"      /* BrStringById, BrMenuSub10044B90, ...       */

/* slice2_16.h cannot be included here: it defines a TYPE called BrRcaFixup
 * and slice2_20.h defines a FUNCTION of that name, so the two headers cannot
 * share a translation unit.  This is the one declaration needed from it, and
 * it is copied verbatim. */
/* XSLICE 0x1007CC00 */
extern void BrGbiStackOverflow(int code);

#ifdef _MSC_VER
#pragma function(sin)
#endif

#ifdef _MSC_VER
#pragma function(sqrt)
#endif

/* 0x10039020 */
/* WHAT IT DOES: runs a car's particle emitter for one frame: it counts up a
 * timer and, once a quarter of a second has gone by, takes one node off the
 * free list and starts a new particle at the car with a velocity based on
 * how the car is moving. If the pool is empty nothing is spawned. */
/* @t4-pass 0x100326A0 1 2026-09-24 probes 13 bytes 469 insns 124 regions 2 rows 0 census no  (hand: t placement/split, f2 reuse, copy spellings, byte-store casts and order) */
/* @t4-pass 0x100326A0 2 2026-09-24 probes 19 bytes 469 insns 124 regions 2 rows 0 census yes  (TU position sweep, every top-level slot) */
/* @t3 0x100326A0 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 469/469 insns 124/124 rows 0+0 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is x87/integer scheduling only (rows 0+0, same size): the 0.1f
 * product issues before the struct-copy loads in the original, and pop ebx
 * lands after the c66 byte store.  Dead probes: the two @t4-pass ledger
 * lines above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10039020 d3d BrCarSub9020 */
#ifdef BR_MATCHING_BUILD
/* The original is the full particle-spawn body the port folded into
 * BrPoolEmit: timer accumulate + threshold, free-slot word shuffle,
 * velocity build via the vec helpers, and the slot's colour/life fields
 * from the divided distance.
 * The constants are literals (the per-function .rdata pool at 0x1007752C
 * holds them in first-use order); the threshold store is a plain statement
 * ahead of the test (VC5 then emits fcom before fstp); the slot free-list
 * pop is three direct stores; the velocity copy is a BrVec3 struct copy.
 * RESIDUE (REGNORM 1+1, same size): two x87/integer schedule spots -- the
 * 0.1f product issues before the struct-copy loads in the original, and
 * pop ebx lands after the c66 byte store. */
extern float DAT_106e9d8c[];
extern int   DAT_10ac0c38;
extern unsigned short DAT_10ac0c40;
extern char  DAT_10ac0c48;      /* slot vec A column   */
extern char  DAT_10ac0c54;      /* slot vec B column   */
extern char  DAT_10ac0c60;      /* slot float column   */
extern char  DAT_10ac0c64;      /* slot word column    */
extern char  DAT_10ac0c66;      /* slot byte column    */
extern char  DAT_10ac0c67;      /* slot byte column    */
extern int   BrRandom(void);
extern void  BrSub10034560(void *pDst, void *pA, void *pB);
extern void  BrSub10034660(void *pDst, void *pA, void *pB, float s);
extern float BrSub100347F0(void *p);

void __fastcall BrCarSub9020(struct BrCar *pCar)
{
    char *p = (char *)pCar;
    float *pe24;
    BrVec3 local;
    float acc, f2, t, g;
    unsigned int idx;
    int off;

    pe24 = (float *)(p + 0xE24);
    acc = ((float)(BrRandom() & 0x1FFF) * 1.52587890625e-05f
           - *pe24 * -0.001f
           - (-1.0f)) * DAT_106e9d8c[0]
          + *(float *)(p + 0x105C);

    *(float *)(p + 0x105C) = acc;
    if (acc > 0.25f) {
        idx = (unsigned int)DAT_10ac0c38 & 0xFFFFu;
        if (idx != 0) {
            f2  = *pe24 * 0.001f;
            off = (int)idx << 5;
            *(int *)(p + 0x105C) = 0;

            *(unsigned short *)&DAT_10ac0c38 = *(unsigned short *)(&DAT_10ac0c64 + off);
            *(unsigned short *)(&DAT_10ac0c64 + off) = DAT_10ac0c40;
            DAT_10ac0c40 = (unsigned short)idx;

            BrVec3Scale((BrVec3 *)(void *)(&DAT_10ac0c54 + off),
                        (const BrVec3 *)(const void *)p,
                        -1.5f - f2);
            BrSub10034560(&local, p + 0xF0, p);
            BrSub10034660(&local, &local, p + 0x20, 0.2f);
            BrSub10034660(&local, &local, p + 0x10, 0.2f);

            g = (float)(BrRandom() & 0xFFFF) * 1.5259021893143654e-05f;
            BrSub10034560(&DAT_10ac0c48 + off, p + 0x1060, &local);
            BrSub10034660(&DAT_10ac0c48 + off, &local,
                          &DAT_10ac0c48 + off, g * g);

            *(BrVec3 *)(p + 0x1060) = local;

            t = f2 * 0.1f - (-1.0f);
            *(float *)(&DAT_10ac0c60 + off) = t * 0.15f;
            *(char *)(&DAT_10ac0c66 + off) =
                (char)(int)(1.0f / (BrSub100347F0(p + 0x1024) + t)
                            * 255.0f);
            *(unsigned char *)(&DAT_10ac0c67 + off) = 0xFF;
        }
    }
}
#else
void BrCarSub9020(struct BrCar *pCar)
{
    BrPoolEmit(pCar);
}
#endif
