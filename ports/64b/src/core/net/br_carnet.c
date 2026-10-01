/* br_carnet.c -- net.
 *
 * The receiving end of a car's state on the wire: sending this car's own
 * state out, writing a received state into a car record, and the per-frame
 * prediction that keeps another player's car moving between packets.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 *
 * x87 NOTE.  The original is MSVC x87 code and the CRT leaves the precision
 * control at 53 bits, so an intermediate the original never spills carries
 * double precision, not float.  Wherever that is observable the intermediate
 * is a `double` here and the rounding to float happens exactly where the
 * original has an `fstp dword`.
 */

#include <string.h>

#include "slice3_40.h"

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */

/* Float constants, read out of BRD3D.dll .rdata rather than assumed. */
#define BR_K_08F7A8    0.0f            /* 0x1008F7A8 */
#define BR_K_08F7B0 (-1000.0f)         /* 0x1008F7B0 -- SUBTRACTED, so +1000 */

/* ==================================================================== */
/* 1. Network car-state apply / predict                                 */
/* ==================================================================== */

/* 0x100609E0 */
/* WHAT IT DOES: packs up this car's current state and sends it to the other
 * players. Whether the send succeeded is thrown away. */
/* @implements 0x100609E0 d3d BrCarNetSendState */
/* @n64 0x80261058 located */
void BrCarNetSendState(BrCar *pCar)
{
    BrCarState state;   /* the original's 0xA0-byte stack buffer */

    BrSub100607B0(&state, pCar);
    BrNetCarStateSend(&state);
    /* GOTCHA: BrNetCarStateSend's int result is discarded here. */
}

/* 0x10060A10 */
/* BrCarApplyState: the original function is BrCarGhostApply_10059A80 */

/* 0x10060CC0 */
/* WHAT IT DOES: brings one other player's car up to date from the network.
 * It does nothing for the local player's own car, and nothing at all when a
 * particular flag is set; otherwise it asks the networking code to predict
 * where that car should be by now, applies the answer, and rebuilds the
 * car's transform matrices so it can be drawn. */
/* @implements 0x10060CC0 d3d BrCarPredictRemote */
int32_t BrCarPredictRemote(BrCar *pCar, int32_t slot)
{
    BrCarState state;   /* the original's 0xA0-byte stack buffer */

    if (slot == BrSub10005D30()) {
        return 1;
    }
    if (BrG_6909B4 != 0) {
        return 1;
    }
    /* The LAST test is written in POSITIVE form -- `if (ok) { work; return 1; }
     * then `return 0;` -- and that is not cosmetic. Written as the guard
     * `if (!ok) return 0;` VC5 tail-merges the two `return 1`s above into one
     * shared exit and the function comes out 28 bytes short. In this form all
     * four exits are emitted in full, as the original has them. See
     * docs/VC5-IDIOMS.md, "the last test's polarity decides whether VC5
     * tail-merges the earlier returns". */
    if (BrNetSlotPredictOrig(&state, slot)) {
        BrCarApplyState(pCar, &state);
        BrCarBuildMatrices(pCar);
        return 1;
    }
    return 0;
}

/* ==================================================================== */
/* 2. Car record -> BrCarState (the sender's packer)                    */
/* ==================================================================== */

/* 0x10059820 (D3D 0x100607B0, shared body).  The port's BrCarRecordToState
 * in slice8_83.c is the same function written for the host; this is the
 * byte-exact one, and BrCarNetSendState above reaches it through the
 * BrSub100607B0 name that slice3_40.h declares. */
/* BrRacePosCopy: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 1.0f            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 0.0f            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* the lap-time sentinel */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                      /* 0x100BCBE8 lap count  */

/* Thirteen of the words are copied bit-for-bit by the original (`mov`, not
 * `fld/fstp`): the state slot is written as a raw dword. */
#define ST_RAW(p, fld)   (*(int32_t *)(void *)&(p)->fld)

/* WHAT IT DOES: pack a car's live state into the forty-float record that
 * goes on the wire: position and orientation from its matrix, the two
 * velocities, six raw words copied bit-for-bit, four counters and five
 * bytes widened to float, a flag bit and a sign test turned into 1.0/0.0,
 * the lap time (or the sentinel on the final lap), and the eight damage
 * bytes as floats. */
/* @implements 0x10059820 glide BrCarRecordToState */
void BrCarRecordToState(BrCarState *pDst, BrCar *pCar)
{
    int fNeg;

    BrRacePosCopy((int)pDst, (int)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x220))))));

    ST_RAW(pDst, f1C) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x1E8)))))));
    ST_RAW(pDst, f20) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x1EC)))))));
    ST_RAW(pDst, f24) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x1F0)))))));
    ST_RAW(pDst, f28) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x204)))))));
    ST_RAW(pDst, f2C) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x208)))))));
    ST_RAW(pDst, f30) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x20C)))))));
    ST_RAW(pDst, f34) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x338)))))));
    ST_RAW(pDst, f38) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x73C)))))));
    ST_RAW(pDst, f3C) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x544)))))));
    ST_RAW(pDst, f40) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x95C)))))));
    ST_RAW(pDst, f44) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x750)))))));
    ST_RAW(pDst, f48) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xB68)))))));

    pDst->f4C = (float)((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x524)))))));
    pDst->f50 = (float)((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x93C)))))));
    pDst->f54 = (float)((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x730)))))));
    pDst->f58 = (float)((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xB48)))))));

    pDst->f5C = (float)(int8_t)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x510)))))));
    pDst->f60 = (float)(int8_t)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x928)))))));
    pDst->f64 = (float)(int8_t)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x71C)))))));
    pDst->f68 = (float)(int8_t)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xB34)))))));
    pDst->f6C = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x36D)))))));

    pDst->f70 = (*(uint32_t *)((*(void *   *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x29C0))))))) & 0xC0000u)
                    ? DAT_1007776c : DAT_10077770;

    fNeg = ((*(float    *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xE68))))))) < DAT_10077770;
    pDst->f74 = (float)fNeg;

    pDst->f78 = (((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xFA8))))))) == g_brLapBound)
                    ? DAT_10077774 : ((*(float    *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xFF4)))))));
    ST_RAW(pDst, f7C) = ((*(int32_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0xE24)))))));

    pDst->f80 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x362)))))));
    pDst->f84 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x363)))))));
    pDst->f88 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x36C)))))));
    pDst->f8C = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x366)))))));
    pDst->f90 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x367)))))));
    pDst->f94 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x368)))))));
    pDst->f98 = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x369)))))));
    pDst->f9C = (float)((*(uint8_t  *)(((void *)((((uint8_t *)(void *)((pCar)))) + ((0x36A)))))));
}
