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
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))
#define CAR_AT(c, off)   ((void *)(CAR_BYTES(c) + (off)))
#define CAR_U8(c, off)   (*(uint8_t  *)CAR_AT(c, off))
#define CAR_I32(c, off)  (*(int32_t  *)CAR_AT(c, off))
#define CAR_F32(c, off)  (*(float    *)CAR_AT(c, off))
#define CAR_PTR(c, off)  (*(void *   *)CAR_AT(c, off))

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
void BrCarApplyState(BrCar *pCar, const BrCarState *pState)
{
    /* --- the BrRbState at pCar+0x1DC ------------------------------- */
    /* quaternion, scalar first (slice3_44 pins the ordering) */
    CAR_F32(pCar, 0x1F4) = pState->f00;
    CAR_F32(pCar, 0x1F8) = pState->f04;
    CAR_F32(pCar, 0x1FC) = pState->f08;
    CAR_F32(pCar, 0x200) = pState->f0C;
    /* position */
    CAR_F32(pCar, 0x1DC) = pState->f10;
    CAR_F32(pCar, 0x1E0) = pState->f14;
    CAR_F32(pCar, 0x1E4) = pState->f18;

    /* The original makes this call HERE, between the f18 and f1C stores,
     * not before or after the block.  Order preserved in case it reads
     * what has been written so far. */
    BrSub100695D0(CAR_AT(pCar, 0x220), pState);

    /* velocity */
    CAR_F32(pCar, 0x1E8) = pState->f1C;
    CAR_F32(pCar, 0x1EC) = pState->f20;
    CAR_F32(pCar, 0x1F0) = pState->f24;
    /* angular velocity */
    CAR_F32(pCar, 0x204) = pState->f28;
    CAR_F32(pCar, 0x208) = pState->f2C;
    CAR_F32(pCar, 0x20C) = pState->f30;

    /* --- seven scattered dwords, copied verbatim ------------------- */
    CAR_F32(pCar, 0x338) = pState->f34;
    CAR_F32(pCar, 0x73C) = pState->f38;
    CAR_F32(pCar, 0xB54) = pState->f38;   /* GOTCHA: f38 stored twice */
    CAR_F32(pCar, 0x544) = pState->f3C;
    CAR_F32(pCar, 0x95C) = pState->f40;
    CAR_F32(pCar, 0x750) = pState->f44;
    CAR_F32(pCar, 0xB68) = pState->f48;

    /* --- four truncated to int32 ----------------------------------- */
    CAR_I32(pCar, 0x524) = BrFtolTrunc(pState->f4C);
    CAR_I32(pCar, 0x93C) = BrFtolTrunc(pState->f50);
    CAR_I32(pCar, 0x730) = BrFtolTrunc(pState->f54);
    CAR_I32(pCar, 0xB48) = BrFtolTrunc(pState->f58);

    /* --- five truncated and narrowed to a byte (the original keeps
     *     only AL, so the wrap is a truncation of the low dword) ----- */
    CAR_U8(pCar, 0x510) = (uint8_t)BrFtolTrunc(pState->f5C);
    CAR_U8(pCar, 0x928) = (uint8_t)BrFtolTrunc(pState->f60);
    CAR_U8(pCar, 0x71C) = (uint8_t)BrFtolTrunc(pState->f64);
    CAR_U8(pCar, 0xB34) = (uint8_t)BrFtolTrunc(pState->f68);
    CAR_U8(pCar, 0x36D) = (uint8_t)BrFtolTrunc(pState->f6C);

    /* --- two "== 0.0f" booleans ------------------------------------ */
    /* Both are `fcomp 0.0f` + `test ah,0x40`, i.e. the C3 bit alone.  An
     * unordered compare sets C3 as well, so a NaN takes the EQUAL branch.
     * Written as "not (< 0 or > 0)" so that holds in C too. */
    {
        uint32_t *pFlags = (uint32_t *)CAR_PTR(pCar, 0x29C0);
        uint32_t  v = *pFlags;
        if (pState->f70 < BR_K_08F7A8 || pState->f70 > BR_K_08F7A8) {
            v |= 0x00040000u;
        } else {
            v &= 0xFFFBFFFFu;     /* equal, or NaN -> CLEAR bit 0x40000 */
        }
        *pFlags = v;
    }
    CAR_F32(pCar, 0xE68) =
        (pState->f74 < BR_K_08F7A8 || pState->f74 > BR_K_08F7A8)
            ? -1.0f : 1.0f;

    /* --- the pCar+0xFF4 guard -------------------------------------- */
    {
        float f = CAR_F32(pCar, 0xFF4);
        int   take;
        if (!(f > BR_K_08F7A8)) {
            /* `test ah,0x41` after fcomp: equal-or-less, NaN included */
            take = 1;
        } else {
            /* the extra 1000 is held in an x87 register, never stored.
             * `test ah,0x41` again folds unordered in with less-or-equal,
             * so a NaN here does NOT take the overwrite. */
            double biased = (double)f - (double)BR_K_08F7B0;
            take = (biased > (double)pState->f78);
        }
        if (take) {
            CAR_F32(pCar, 0xFF4) = pState->f78;
        }
    }

    CAR_F32(pCar, 0xE24) = pState->f7C;

    /* --- eight more truncated bytes -------------------------------- */
    CAR_U8(pCar, 0x362) = (uint8_t)BrFtolTrunc(pState->f80);
    CAR_U8(pCar, 0x363) = (uint8_t)BrFtolTrunc(pState->f84);
    CAR_U8(pCar, 0x36C) = (uint8_t)BrFtolTrunc(pState->f88);
    CAR_U8(pCar, 0x366) = (uint8_t)BrFtolTrunc(pState->f8C);
    CAR_U8(pCar, 0x367) = (uint8_t)BrFtolTrunc(pState->f90);
    CAR_U8(pCar, 0x368) = (uint8_t)BrFtolTrunc(pState->f94);
    CAR_U8(pCar, 0x369) = (uint8_t)BrFtolTrunc(pState->f98);
    CAR_U8(pCar, 0x36A) = (uint8_t)BrFtolTrunc(pState->f9C);

    /* --- close the rigid-body state and snapshot it twice ---------- */
    BrRbQuatDerivative((BrRbState *)CAR_AT(pCar, 0x1DC));
    memcpy(CAR_AT(pCar, 0x278), CAR_AT(pCar, 0x1DC), 0x44);
    memcpy(CAR_AT(pCar, 0x2BC), CAR_AT(pCar, 0x1DC), 0x44);
}

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
