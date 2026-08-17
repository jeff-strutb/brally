/* slice8_83.c -- Boss Rally, packet 83.  See slice8_83.h for the map, the
 * demand ranking that chose these fourteen, and the signature adjudications.
 *
 * Reference binary: orig/BRGlide.dll.  Glide addresses are given wherever
 * they differ from the D3D ones the rest of port/ is named after.
 */

#include <string.h>

#include "slice8_83.h"

#include "br_crt.h"      /* BrFtolTrunc        -- 0x1007C8A0                */
#include "slice2_12.h"   /* BrNetSlotPredict, BrNetSlotGetF030              */
#include "slice2_25.h"   /* BrOptOpen296C/2970, BrGameObj, g_br0BD3E0       */
#include "slice3_42.h"   /* BrMat4FromCarState -- 0x100695D0                */
#include "slice3_44.h"   /* BrRbState, BrRbQuatDerivative -- 0x100742D0     */
#include "slice4_53.h"   /* g_pBrSlice4PhaseCtx                             */
#include "slice5_62.h"   /* BrSub100765E0      -- 0x100765E0                */

/* slice1_02.h's two, used by BrNetAnnounce and by every net adapter's callee. */
/* (declared there; nothing extra needed here)                                */

/* ==================================================================== */
/* Host bindings.  NULL/zero == the generated stub's old behaviour.      */
/* ==================================================================== */

BrNetState *g_pBrNetState83        = NULL;
void       *g_hBrNet1022AF34_83    = NULL;
int32_t     g_brLocalSlot83        = 0;
uint32_t    g_brNowTicks83         = 0;

BrHooks    *g_pBrHooks83           = NULL;

BrStartupState *g_pBrStartupState83 = NULL;
void           *g_pBrB4DF3083       = NULL;

const BrRcaFixup *g_pBrRcaFixup83   = NULL;

/* ==================================================================== */
/* Byte-wise access to the car record.                                   */
/*                                                                       */
/* slice2_15.h's BrCar is `uint8_t a0000[...]` with a couple of named     */
/* fields, and slice3_40.c reaches everything else by explicit byte       */
/* offset for the same reason: the eleven kilobytes do not form a         */
/* describable struct yet.  memcpy rather than a pointer cast keeps this  */
/* alignment- and aliasing-clean, which matters because several of these  */
/* offsets are not 4-aligned (+0x36D, +0x362, ...).                       */
/* ==================================================================== */

static uint32_t br83_ld32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void br83_st32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static float br83_ldf(const uint8_t *p)
{
    float f;
    memcpy(&f, p, sizeof f);
    return f;
}

static void br83_stf(uint8_t *p, float f)
{
    memcpy(p, &f, sizeof f);
}

/* The pointer at +0x29C0 -- slice3_40.h already records this offset as
 * "pointer to a dword of flags", and records that a host pointer there
 * covers +0x29C4 as well.  Same DEVIATION, spelled with memcpy. */
static uint32_t *br83_ldflagsptr(const uint8_t *p)
{
    uint32_t *q;
    memcpy(&q, p, sizeof q);
    return q;
}

/* Car-record offsets touched by 0x100607B0 / 0x10060A10.  Named by offset
 * because that is what is actually established. */
#define BR83_CAR_RB       0x01DCu   /* BrRbState, 0x44 bytes               */
#define BR83_CAR_VEL      0x01E8u   /* == RB + 0x0C                        */
#define BR83_CAR_QUAT     0x01F4u   /* == RB + 0x18                        */
#define BR83_CAR_ANGVEL   0x0204u   /* == RB + 0x28                        */
#define BR83_CAR_MAT      0x0220u   /* BrMat4                              */
#define BR83_CAR_RB_A     0x0278u   /* mirror of RB                        */
#define BR83_CAR_RB_B     0x02BCu   /* mirror of RB                        */
#define BR83_CAR_338      0x0338u
#define BR83_CAR_362      0x0362u
#define BR83_CAR_366      0x0366u
#define BR83_CAR_36D      0x036Du
#define BR83_CAR_510      0x0510u
#define BR83_CAR_524      0x0524u
#define BR83_CAR_544      0x0544u
#define BR83_CAR_71C      0x071Cu
#define BR83_CAR_730      0x0730u
#define BR83_CAR_73C      0x073Cu
#define BR83_CAR_750      0x0750u
#define BR83_CAR_928      0x0928u
#define BR83_CAR_93C      0x093Cu
#define BR83_CAR_95C      0x095Cu
#define BR83_CAR_B34      0x0B34u
#define BR83_CAR_B48      0x0B48u
#define BR83_CAR_B54      0x0B54u
#define BR83_CAR_B68      0x0B68u
#define BR83_CAR_E24      0x0E24u
#define BR83_CAR_E68      0x0E68u
#define BR83_CAR_FA8      0x0FA8u   /* int32 lap counter                   */
#define BR83_CAR_FF4      0x0FF4u   /* float best/target time              */
#define BR83_CAR_29C0     0x29C0u   /* uint32_t *flags                     */

#define BR83_RB_BYTES     0x44u     /* the original's `rep movsd` x 0x11    */

/* Read out of BOTH images and identical in both:
 *   Glide 0x1007776C/70/74/78, D3D 0x1008F7A4/A8/AC/B0. */
#define BR83_ONE       1.0f
#define BR83_ZERO      0.0f
#define BR83_SENTINEL  4188888.0f
#define BR83_TIMEBIAS  (-1000.0f)

/* The two flag bits at *(car+0x29C0).  0x100607B0 tests BOTH; 0x10060A10
 * writes only the low one.  Deliberately two constants, so the asymmetry
 * cannot be "tidied" into one. */
#define BR83_FLAG_TESTED  0x000C0000u
#define BR83_FLAG_WRITTEN 0x00040000u

/* ==================================================================== */
/* 0x100695A0 (Glide 0x10062610) -- matrix -> BrCarState                 */
/* ==================================================================== */

void BrCarStateFromMatrix83(BrCarState *pDst, const BrMat4 *pMat)
{
    /* Matrix FIRST: the wrapper receives (pDst, pMat) and forwards them the
     * other way round.  Traced through the two pushes at Glide 0x1006261A. */
    BrSub100765E0(pMat, (BrVec4 *)(void *)pDst);

    pDst->f10 = pMat->m[3][0];
    pDst->f14 = pMat->m[3][1];
    pDst->f18 = pMat->m[3][2];
}

/* ==================================================================== */
/* 0x100607B0 (Glide 0x10059820) -- car record -> BrCarState             */
/* ==================================================================== */

void BrCarRecordToState(BrCarState *pDst, void *pCar)
{
    uint8_t  *car = (uint8_t *)pCar;
    BrMat4    mat;
    uint32_t *pFlags;
    float     f;

    memcpy(&mat, car + BR83_CAR_MAT, sizeof mat);
    BrCarStateFromMatrix83(pDst, &mat);

    pDst->f1C = br83_ldf(car + BR83_CAR_VEL + 0);
    pDst->f20 = br83_ldf(car + BR83_CAR_VEL + 4);
    pDst->f24 = br83_ldf(car + BR83_CAR_VEL + 8);

    pDst->f28 = br83_ldf(car + BR83_CAR_ANGVEL + 0);
    pDst->f2C = br83_ldf(car + BR83_CAR_ANGVEL + 4);
    pDst->f30 = br83_ldf(car + BR83_CAR_ANGVEL + 8);

    /* +0x338..+0xB68 are copied as raw dwords by the original (`mov`), so
     * they are moved bit-for-bit here rather than through float. */
    memcpy(&pDst->f34, car + BR83_CAR_338, 4);
    memcpy(&pDst->f38, car + BR83_CAR_73C, 4);
    memcpy(&pDst->f3C, car + BR83_CAR_544, 4);
    memcpy(&pDst->f40, car + BR83_CAR_95C, 4);
    memcpy(&pDst->f44, car + BR83_CAR_750, 4);
    memcpy(&pDst->f48, car + BR83_CAR_B68, 4);

    /* `fild dword` -- signed int32 -> float. */
    pDst->f4C = (float)(int32_t)br83_ld32(car + BR83_CAR_524);
    pDst->f50 = (float)(int32_t)br83_ld32(car + BR83_CAR_93C);
    pDst->f54 = (float)(int32_t)br83_ld32(car + BR83_CAR_730);
    pDst->f58 = (float)(int32_t)br83_ld32(car + BR83_CAR_B48);

    /* `movsx` -- SIGNED bytes.  The eight below are not. */
    pDst->f5C = (float)(int8_t)car[BR83_CAR_510];
    pDst->f60 = (float)(int8_t)car[BR83_CAR_928];
    pDst->f64 = (float)(int8_t)car[BR83_CAR_71C];
    pDst->f68 = (float)(int8_t)car[BR83_CAR_B34];

    /* `xor edx,edx` then `mov dl,` -- UNSIGNED. */
    pDst->f6C = (float)(uint8_t)car[BR83_CAR_36D];

    pFlags = br83_ldflagsptr(car + BR83_CAR_29C0);
    pDst->f70 = (pFlags != NULL && (*pFlags & BR83_FLAG_TESTED) != 0)
                    ? BR83_ONE : BR83_ZERO;
    /* DEVIATION: the original has no null check on that pointer.  A NULL
     * here is a port-only state (the record is .bss until a track loads),
     * and dereferencing it would abort rather than reproduce anything. */

    /* `fcomp 0.0f` + `test ah,1` is C0, which is ALSO set for unordered, so
     * a NaN takes the 1.0f side.  Spelled negated for exactly that reason. */
    f = br83_ldf(car + BR83_CAR_E68);
    pDst->f74 = !(f >= BR83_ZERO) ? BR83_ONE : BR83_ZERO;

    /* On the final lap the sentinel replaces the time.  g_br0BD3E0 is the
     * lap count -- 0x100BD3E0 (D3D) / 0x100BCBE8 (Glide), one object. */
    pDst->f78 = ((int32_t)br83_ld32(car + BR83_CAR_FA8) == g_br0BD3E0)
                    ? BR83_SENTINEL
                    : br83_ldf(car + BR83_CAR_FF4);

    memcpy(&pDst->f7C, car + BR83_CAR_E24, 4);

    pDst->f80 = (float)(uint8_t)car[BR83_CAR_362 + 0];   /* 0x362 */
    pDst->f84 = (float)(uint8_t)car[BR83_CAR_362 + 1];   /* 0x363 */
    pDst->f88 = (float)(uint8_t)car[BR83_CAR_36D - 1];   /* 0x36C */
    pDst->f8C = (float)(uint8_t)car[BR83_CAR_366 + 0];   /* 0x366 */
    pDst->f90 = (float)(uint8_t)car[BR83_CAR_366 + 1];   /* 0x367 */
    pDst->f94 = (float)(uint8_t)car[BR83_CAR_366 + 2];   /* 0x368 */
    pDst->f98 = (float)(uint8_t)car[BR83_CAR_366 + 3];   /* 0x369 */
    pDst->f9C = (float)(uint8_t)car[BR83_CAR_366 + 4];   /* 0x36A */
}

void BrSub100607B0(BrCarState *pDst, void *pCar)
{
    /* Same address, second name.  slice3_40.h types the second parameter
     * `BrCar *` and slice3_42.h types it `void *`; C has no way to declare
     * both, so the wrapper exists and the caller's cast decides. */
    BrCarRecordToState(pDst, pCar);
}

/* ==================================================================== */
/* 0x10060A10 (Glide 0x10059A80) -- BrCarState -> car record             */
/* ==================================================================== */

void BrCarRecordFromState(void *pCar, const BrCarState *pSrc)
{
    uint8_t   *car = (uint8_t *)pCar;
    BrMat4     mat;
    BrRbState  rb;
    uint32_t  *pFlags;
    float      cur;
    int        fTakeTime;

    /* quaternion -> RB.quat (+0x18), translation -> RB.pos (+0x00) */
    memcpy(car + BR83_CAR_QUAT + 0x0, &pSrc->f00, 4);
    memcpy(car + BR83_CAR_QUAT + 0x4, &pSrc->f04, 4);
    memcpy(car + BR83_CAR_QUAT + 0x8, &pSrc->f08, 4);
    memcpy(car + BR83_CAR_QUAT + 0xC, &pSrc->f0C, 4);
    memcpy(car + BR83_CAR_RB   + 0x0, &pSrc->f10, 4);
    memcpy(car + BR83_CAR_RB   + 0x4, &pSrc->f14, 4);
    memcpy(car + BR83_CAR_RB   + 0x8, &pSrc->f18, 4);

    /* The matrix is rebuilt from the state, not from the record, and it is
     * rebuilt HERE -- before the velocities land -- because the original
     * calls 0x100695D0 at this point.  Order is preserved even though the
     * callee reads nothing the later stores touch. */
    BrMat4FromCarState(&mat, pSrc);
    memcpy(car + BR83_CAR_MAT, &mat, sizeof mat);

    memcpy(car + BR83_CAR_VEL    + 0, &pSrc->f1C, 4);
    memcpy(car + BR83_CAR_VEL    + 4, &pSrc->f20, 4);
    memcpy(car + BR83_CAR_VEL    + 8, &pSrc->f24, 4);
    memcpy(car + BR83_CAR_ANGVEL + 0, &pSrc->f28, 4);
    memcpy(car + BR83_CAR_ANGVEL + 4, &pSrc->f2C, 4);
    memcpy(car + BR83_CAR_ANGVEL + 8, &pSrc->f30, 4);

    memcpy(car + BR83_CAR_338, &pSrc->f34, 4);
    memcpy(car + BR83_CAR_73C, &pSrc->f38, 4);
    memcpy(car + BR83_CAR_B54, &pSrc->f38, 4);   /* the SAME field, twice */
    memcpy(car + BR83_CAR_544, &pSrc->f3C, 4);
    memcpy(car + BR83_CAR_95C, &pSrc->f40, 4);
    memcpy(car + BR83_CAR_750, &pSrc->f44, 4);
    memcpy(car + BR83_CAR_B68, &pSrc->f48, 4);

    /* __ftol: truncates toward zero, and an out-of-range value yields 0
     * rather than 0x80000000 (README, 0x1007C8A0). */
    br83_st32(car + BR83_CAR_524, (uint32_t)BrFtolTrunc(pSrc->f4C));
    br83_st32(car + BR83_CAR_93C, (uint32_t)BrFtolTrunc(pSrc->f50));
    br83_st32(car + BR83_CAR_730, (uint32_t)BrFtolTrunc(pSrc->f54));
    br83_st32(car + BR83_CAR_B48, (uint32_t)BrFtolTrunc(pSrc->f58));

    /* `mov byte[..], al` -- the low byte of the ftol result, so the
     * signed/unsigned split the reader has does not exist on this side. */
    car[BR83_CAR_510] = (uint8_t)BrFtolTrunc(pSrc->f5C);
    car[BR83_CAR_928] = (uint8_t)BrFtolTrunc(pSrc->f60);
    car[BR83_CAR_71C] = (uint8_t)BrFtolTrunc(pSrc->f64);
    car[BR83_CAR_B34] = (uint8_t)BrFtolTrunc(pSrc->f68);
    car[BR83_CAR_36D] = (uint8_t)BrFtolTrunc(pSrc->f6C);

    /* `fcomp 0.0f` + `test ah,0x40` is C3, set for EQUAL and for UNORDERED.
     * The set-bit arm is the fall-through, i.e. strictly-ordered-and-unequal,
     * which is `(<0) || (>0)` -- and false for NaN.  Writing this as
     * `f70 != 0.0f` would put NaN on the wrong side. */
    pFlags = br83_ldflagsptr(car + BR83_CAR_29C0);
    if (pFlags != NULL) {           /* DEVIATION: see the reader, above */
        if (pSrc->f70 < BR83_ZERO || pSrc->f70 > BR83_ZERO)
            *pFlags |= BR83_FLAG_WRITTEN;
        else
            *pFlags &= ~BR83_FLAG_WRITTEN;
    }

    /* Same C3 test, and note the polarity: SET means -1.0f. */
    br83_stf(car + BR83_CAR_E68,
             (pSrc->f74 < BR83_ZERO || pSrc->f74 > BR83_ZERO)
                 ? -BR83_ONE : BR83_ONE);

    cur = br83_ldf(car + BR83_CAR_FF4);
    if (!(cur > BR83_ZERO)) {
        fTakeTime = 1;                       /* no time yet -- or NaN */
    } else {
        /* `test ah,0x41` is C0|C3: take the new time unless ours+1000 is
         * STRICTLY better.  Negated so unordered takes the assign side. */
        fTakeTime = !((cur - BR83_TIMEBIAS) > pSrc->f78);
    }
    if (fTakeTime)
        memcpy(car + BR83_CAR_FF4, &pSrc->f78, 4);

    memcpy(car + BR83_CAR_E24, &pSrc->f7C, 4);

    car[BR83_CAR_362 + 0] = (uint8_t)BrFtolTrunc(pSrc->f80);   /* 0x362 */
    car[BR83_CAR_362 + 1] = (uint8_t)BrFtolTrunc(pSrc->f84);   /* 0x363 */
    car[BR83_CAR_36D - 1] = (uint8_t)BrFtolTrunc(pSrc->f88);   /* 0x36C */
    car[BR83_CAR_366 + 0] = (uint8_t)BrFtolTrunc(pSrc->f8C);   /* 0x366 */
    car[BR83_CAR_366 + 1] = (uint8_t)BrFtolTrunc(pSrc->f90);   /* 0x367 */
    car[BR83_CAR_366 + 2] = (uint8_t)BrFtolTrunc(pSrc->f94);   /* 0x368 */
    car[BR83_CAR_366 + 3] = (uint8_t)BrFtolTrunc(pSrc->f98);   /* 0x369 */
    car[BR83_CAR_366 + 4] = (uint8_t)BrFtolTrunc(pSrc->f9C);   /* 0x36A */

    /* qDot = 0.5 * (0, angVel) x quat, in place, then two mirrors. */
    memcpy(&rb, car + BR83_CAR_RB, sizeof rb);
    BrRbQuatDerivative(&rb);
    memcpy(car + BR83_CAR_RB, &rb, sizeof rb);

    memcpy(car + BR83_CAR_RB_A, car + BR83_CAR_RB, BR83_RB_BYTES);
    memcpy(car + BR83_CAR_RB_B, car + BR83_CAR_RB, BR83_RB_BYTES);
}

/* ==================================================================== */
/* 0x10003530 (Glide 0x100038A0) -- BrNetAnnounce                        */
/* ==================================================================== */

char    g_aBrAnnounce83[BR83_ANNOUNCE_MAX];   /* 0x1021C9B0 */
int32_t g_brAnnouncePending83;                /* 0x10226A38 */
void   *g_hBrAnnounce83;                      /* 0x10226A54 */

void BrNetAnnounce(const char *pszText)
{
    size_t n;

    BrNetMutexLock(g_hBrAnnounce83);

    /* DEVIATION: the original's copy is an unbounded inlined strcpy into a
     * fixed global.  Bounded here, at both ends, exactly as slice1_02.c
     * bounds the identical idiom in BrNetSlotName.  Behaviour is unchanged
     * for any message that fits. */
    for (n = 0; n < sizeof g_aBrAnnounce83 - 1 && pszText[n] != '\0'; ++n)
        g_aBrAnnounce83[n] = pszText[n];
    g_aBrAnnounce83[n] = '\0';

    g_brAnnouncePending83 = 1;

    BrNetMutexUnlock(g_hBrAnnounce83);
}

/* ==================================================================== */
/* Adapters                                                              */
/* ==================================================================== */

int BrNetSlotPredictOrig(BrCarState *pDst, int32_t slot)
{
    if (g_pBrNetState83 == NULL)
        return 0;                      /* == the retired stub's answer */

    return BrNetSlotPredict(pDst, slot, g_pBrNetState83,
                            g_hBrNet1022AF34_83, g_brLocalSlot83,
                            g_brNowTicks83);
}

/* The argument is an INDEX (see slice8_83.h).  Bounded, because the one
 * caller in port/ currently types it `void *`. */
static int br83_slot_of(void *pOwner, int32_t *pSlot)
{
    intptr_t v = (intptr_t)pOwner;

    if (g_pBrNetState83 == NULL)
        return 0;
    if (v < 0 || v >= (intptr_t)BR_NET_SLOTS)
        return 0;

    *pSlot = (int32_t)v;
    return 1;
}

int BrX10005DE0(void *pOwner, unsigned char *pb0,
                unsigned char *pb1, unsigned char *pb2)
{
    int32_t slot;

    if (!br83_slot_of(pOwner, &slot))
        return 0;

    return (int)BrNetSlotGetF030(g_pBrNetState83, slot,
                                 (uint8_t *)pb0, (uint8_t *)pb1,
                                 (uint8_t *)pb2);
}

const char *BrX10005E70(void *pOwner)
{
    int32_t slot;

    if (!br83_slot_of(pOwner, &slot))
        return "";     /* never NULL: the one caller strcpy()s the result */

    return BrNetSlotName(g_pBrNetState83, slot);
}

void BrSub10005FE0(uint32_t idPlayer)
{
    if (g_pBrNetState83 == NULL)
        return;

    BrNetDropMatching(g_pBrNetState83, (int32_t)idPlayer);
}

void BrX10034C66(void (*pfn)(void))
{
    if (g_pBrHooks83 == NULL)
        return;

    BrHookSetC(g_pBrHooks83, pfn);
}

void BrSwapRec24Array(void *pv, int n)
{
    if (g_pBrRcaFixup83 == NULL)
        return;

    BrRcaFixupArray(g_pBrRcaFixup83, pv, n);
}

void BrExt_1003DFC0(void)
{
    if (g_pBrStartupState83 == NULL)
        return;

    BrUiFn1003DFC0(g_pBrStartupState83, g_pBrB4DF3083);
}

void BrExt_10043260(void *pArg)
{
    (void)BrOptOpen296C((BrGameObj *)pArg);   /* argument unread downstream */
}

/* WHAT IT DOES: opens one particular options screen on behalf of a menu row. */
/* @implements 0x10043330 d3d BrExt_10043330 */
void BrExt_10043330(void *pArg)
{
    (void)BrOptOpen2970((BrGameObj *)pArg);
}

/* RETURN VALUE: 0. 0x10044970 ends `xor eax, eax` at 0x10044A1E, and the
 * +0x08 slot this is stored into is TESTED by 0x10048180 -- see br_phase.h.
 * The frontier guard also returns 0, which is the same answer the original
 * gives; there is no path here that can return anything else. */
int32_t BrOptFn10044970(void *pEntity)
{
    if (g_pBrSlice4PhaseCtx == NULL)
        return 0;

    (void)BrPhaseLeave_10044970(g_pBrSlice4PhaseCtx, pEntity);
    return 0;
}
