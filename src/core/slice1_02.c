/* slice1_02.c -- BRD3D.dll 0x100049C0-0x100079E0. See slice1_02.h. */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_02.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* =====================================================================
 * Shared primitives the original reaches through the CRT
 * ===================================================================== */

/* The original sign-extends with `movsx`; spelled out here so the result does
 * not depend on implementation-defined narrowing conversions. */
static int32_t BrSext8(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFu);
    return (x & 0x80) ? x - 0x100 : x;
}

static int32_t BrSext16(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFFu);
    return (x & 0x8000) ? x - 0x10000 : x;
}

static int32_t BrSext24(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFFFFu);
    return (x & 0x800000) ? x - 0x1000000 : x;
}

/* =====================================================================
 * 2. Car-state packet
 * ===================================================================== */

/* BrCarStateLerp walks the struct as a flat float array, exactly as the
 * original walks the 0xA0-byte record. Catch any accidental padding here
 * rather than at run time. */
typedef char BrCarStateSizeCheck[
    (sizeof(BrCarState) == BR_CARSTATE_FLOATS * sizeof(float)) ? 1 : -1];

/* 0x1008F0CC = 128.0f and 0x1008F118 = 1.0f: the decoder uses BOTH as the
 * "true" value for different 1-bit fields, so they are not interchangeable. */
#define BR_ONE_128  128.0f
#define BR_ONE_1      1.0f

/* The delta code shared by f10, f14, f18 and f78 in 0x100073E0.
 *
 * `prev` is the reference value already re-quantised to the field's integer
 * form. The transmitted word carries a 2-bit code in its top bits and the new
 * low bits underneath. The code adjusts the retained high part by
 * {0, +step, +2*step, -step} -- note the fourth case is a SUBTRACT, not +3.
 *
 * GOTCHA: the original never re-masks after the add/subtract, so the high part
 * is free to carry out of `keepMask` (and, for the -step case starting from
 * zero, to wrap negative). That wraparound is load-bearing -- the s16 fields
 * rely on it to reach negative values -- so it is preserved, which is why this
 * is all unsigned arithmetic. */
static uint32_t BrCarStateDeltaMerge(uint32_t prev, uint32_t bits,
                                     uint32_t codeMask, uint32_t step,
                                     uint32_t keepMask, uint32_t lowMask)
{
    uint32_t code = bits & codeMask;
    uint32_t hi   = prev & keepMask;

    if (code == step)
        hi += step;
    else if (code == step * 2u)
        hi += step * 2u;
    else if (code != 0u)
        hi -= step;

    return hi | (bits & lowMask);
}

/* 0x10006EC0.  Field order is the bitstream order and must not be reordered.
 * The two arguments are (destination, reader); the original loads the reader
 * from the second stack slot into edi before anything else. */
/* WHAT IT DOES: reads one car's complete state out of a network packet and
 * fills in the car record: facing, position, speed-like values, wheel or
 * suspension figures, an angle (which fills four fields, twice raw and twice
 * offset by 35 degrees and wrapped), and a stack of on/off flags. Four
 * fields are deliberately left alone and one is zeroed without any bits
 * being read, so the caller must not assume the whole record was written. */
/* @implements 0x10006EC0 d3d BrCarStateDecode */
void BrCarStateDecode(BrCarState *pDst, BrBitReader *pReader)
{
    float angle, wrapped;

    /* Four s8s, each shifted into the high byte of a 16-bit word first
     * (`xor ecx,ecx / mov ch,al`), so the effective scale is 1/-128. */
    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));

    pDst->f10 = BrFixUnpackU32Q13(BrBitReaderRead(pReader, 17) << 7);
    pDst->f14 = BrFixUnpackU32Q13(BrBitReaderRead(pReader, 17) << 7);
    pDst->f18 = BrFixUnpackS16Q7((int32_t)(BrBitReaderRead(pReader, 15) << 1));

    pDst->f1C = BrFixUnpackS16Q8((int32_t)BrBitReaderRead(pReader, 16));
    pDst->f20 = BrFixUnpackS16Q8((int32_t)BrBitReaderRead(pReader, 16));

    pDst->f24 = 0.0f;                   /* stored, but no bits are consumed */

    /* `shl al,N` -- an 8-bit shift, so these stay inside one byte. */
    pDst->f28 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f2C = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f30 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 5) << 3) & 0xFFu));
    pDst->f34 = BrFixUnpackS8Q3((int32_t)((BrBitReaderRead(pReader, 4) << 4) & 0xFFu));
    pDst->f38 = BrFixUnpackS6Q7Neg((int32_t)((BrBitReaderRead(pReader, 4) << 2) & 0xFFu));

    /* One 4-bit angle feeds four fields: the raw value twice, then the same
     * value biased by +35 degrees and wrapped, twice. 0x1008F110 = -35.0f is
     * subtracted (hence the bias is positive) and 0x1008F114 = 360.0f. */
    angle = BrFixUnpackU8Angle((int32_t)((BrBitReaderRead(pReader, 4) << 4) & 0xFFu));
    pDst->f40 = angle;
    pDst->f3C = angle;
    wrapped = angle - (-35.0f);
    if (wrapped >= 360.0f)
        wrapped -= 360.0f;
    pDst->f48 = wrapped;
    pDst->f44 = wrapped;

    /* 1-bit flags widened through `fild qword`, i.e. plain 0.0f / 1.0f. */
    pDst->f4C = (float)BrBitReaderRead(pReader, 1);
    pDst->f50 = (float)BrBitReaderRead(pReader, 1);
    pDst->f54 = (float)BrBitReaderRead(pReader, 1);
    pDst->f58 = (float)BrBitReaderRead(pReader, 1);

    /* f5C, f60, f64, f68 are never written by this routine. */

    pDst->f6C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f70 = BrBitReaderRead(pReader, 1) ? BR_ONE_1   : 0.0f;
    pDst->f74 = BrBitReaderRead(pReader, 1) ? BR_ONE_1   : 0.0f;

    pDst->f78 = BrFixUnpackS24Q1(BrBitReaderRead(pReader, 24));
    pDst->f7C = BrFixUnpackU8Range((int32_t)BrBitReaderRead(pReader, 6));
    pDst->f80 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));
    pDst->f84 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));

    pDst->f88 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
}

/* 0x100073E0.  Arguments are (destination, reference, reader): the original
 * loads edi from the third stack slot, ebx from the first and ebp from the
 * second. */
/* WHAT IT DOES: reads a car's state sent as a difference from an earlier
 * one. Facing comes through in full; position, height and one other field
 * arrive as low bits plus a two-bit hint that nudges the high part up one
 * step, up two, or down one. Everything the packet does not mention is left
 * as the caller had it, so the caller must seed the record from the
 * reference first. */
/* @implements 0x100073E0 d3d BrCarStateDecodeDelta */
void BrCarStateDecodeDelta(BrCarState *pDst, const BrCarState *pRef,
                           BrBitReader *pReader)
{
    uint32_t prev, bits;

    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((BrBitReaderRead(pReader, 8) & 0xFFu) << 8));

    /* f10/f14: re-quantise the reference to unsigned Q13-in-24, drop the low 7
     * bits (`shr esi,7`, a LOGICAL shift) to get a 17-bit value, then merge
     * 12 transmitted low bits under a 2-bit page code. */
    prev = (uint32_t)BrFixPackU24Q13(pRef->f10) >> 7;
    bits = BrBitReaderRead(pReader, 14);
    pDst->f10 = BrFixUnpackU32Q13(
        BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu) << 7);

    prev = (uint32_t)BrFixPackU24Q13(pRef->f14) >> 7;
    bits = BrBitReaderRead(pReader, 14);
    pDst->f14 = BrFixUnpackU32Q13(
        BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu) << 7);

    /* f18: signed Q7-in-16, then `sar ax,1` -- an ARITHMETIC shift on the low
     * 16 bits only -- giving a 15-bit signed value; 9 transmitted low bits. */
    {
        int32_t p16 = BrSext16((uint32_t)BrFixPackS16Q7(pRef->f18));
        prev = (uint32_t)(p16 >> 1);    /* arithmetic shift, matching `sar` */
        bits = BrBitReaderRead(pReader, 11);
        /* `lea eax,[esi+esi]` then a `movsx ax` inside 0x10007380, so only the
         * low 16 bits of the doubled value survive; masked here so the
         * conversion to int32_t is never implementation-defined. */
        pDst->f18 = BrFixUnpackS16Q7((int32_t)(
            (BrCarStateDeltaMerge(prev, bits, 0x600u, 0x200u, 0x7E00u, 0x1FFu)
             * 2u) & 0xFFFFu));
    }

    /* f78: signed Q1-in-24, kept whole; 7 transmitted low bits. */
    prev = (uint32_t)BrFixPackS24Q1(pRef->f78);
    bits = BrBitReaderRead(pReader, 9);
    pDst->f78 = BrFixUnpackS24Q1(
        BrCarStateDeltaMerge(prev, bits, 0x180u, 0x80u, 0xFFFF80u, 0x7Fu));

    pDst->f7C = BrFixUnpackU8Range((int32_t)BrBitReaderRead(pReader, 6));
    pDst->f80 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));
    pDst->f84 = BrFixUnpackLevel((int32_t)BrBitReaderRead(pReader, 2));

    pDst->f88 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = BrBitReaderRead(pReader, 1) ? BR_ONE_128 : 0.0f;
}

/* 0x100079E0.  0x1008F0C8 = 0.0f, 0x1008F148 = 10.0f, 0x1008F118 = 1.0f. */
/* WHAT IT DOES: blends between two car states -- the heart of the game's
 * smoothing between network updates. The blend factor may run up to ten
 * times past the second state, because the game is predicting ahead of the
 * last packet it received rather than merely filling in between two it has.
 * When the two facings point opposite ways it flips one of them first, so a
 * car does not spin the long way round; and the last field is copied
 * outright rather than blended. */
/* @implements 0x100079E0 d3d BrCarStateLerp */
void BrCarStateLerp(BrCarState *pDst, float t,
                    const BrCarState *pA, const BrCarState *pB)
{
    const float *a = (const float *)pA;
    const float *b = (const float *)pB;
    float       *d = (float *)pDst;
    int          i;
    int          negate;

    /* Clamp to [0, 10]. Both guards are written so that the x87 "unordered"
     * result takes the same branch a NaN takes here: NaN ends up at 0. */
    if (!(t > 0.0f))
        t = 0.0f;
    else if (!(t < 10.0f))
        t = 10.0f;

    /* Quaternion double-cover fix: only when the leading components have
     * opposite signs AND are at least 1.0 apart. Note this is a magnitude
     * threshold, not a dot-product sign test. */
    negate = (a[0] >= 0.0f && b[0] < 0.0f && (a[0] - b[0]) >= 1.0f)
          || (b[0] >= 0.0f && a[0] < 0.0f && (b[0] - a[0]) >= 1.0f);

    if (negate) {
        for (i = 0; i < 4; ++i)
            d[i] = (-b[i] - a[i]) * t + a[i];
    } else {
        for (i = 0; i < 4; ++i)
            d[i] = (b[i] - a[i]) * t + a[i];
    }

    /* The tail loop is a separate 36-iteration run over +0x10..+0x9C and is
     * NOT affected by the negation. */
    for (i = 4; i < BR_CARSTATE_FLOATS; ++i)
        d[i] = (b[i] - a[i]) * t + a[i];

    /* And then the last field is overwritten outright (a raw dword move in the
     * original), discarding the value the loop just produced. */
    pDst->f9C = pB->f9C;
}

/* =====================================================================
 * 3. Player slot table
 * ===================================================================== */

#ifdef BR_MATCHING_BUILD
/* The mutex pair is the raw Win32 import (FF 15), not the port's
 * BrNetMutexLock/Unlock wrappers -- the same lock idiom the rest of the
 * player-slot table uses. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern int DAT_10226a54;
extern int DAT_10226a28;
extern int DAT_10226a38;
extern unsigned char DAT_1021c9b0;

extern int FUN_1006ba60(int a, int b);
extern unsigned char *DAT_104abb20;
extern int DAT_104abb24;

/* WHAT IT DOES: release the pending input one-shot under the message mutex
 * -- silences its sound, frees the slot, and restores the default message
 * table and level if one was armed. */
/* @implements 0x10006460 glide FUN_10006460 */
/* Mutex-guarded release of one input one-shot: stop the pending sound and
 * reset its slot, and if the arm flag is set restore the default table
 * pointer (&DAT_1021c9b0) and its 3.0f level.  Same lock idiom as BrNetReset. */
void FUN_10006460(void)
{
    WaitForSingleObject((void *)DAT_10226a54, 0xffffffff);
    if (DAT_10226a28 >= 0) {
        FUN_1006ba60(DAT_10226a28, 0x200020);
        DAT_10226a28 = -1;
    }
    if (DAT_10226a38 != 0) {
        DAT_104abb20 = &DAT_1021c9b0;
        DAT_104abb24 = 0x40400000;
        DAT_10226a38 = 0;
    }
    ReleaseMutex((void *)DAT_10226a54);
}
#endif

/* 0x10004A10 */
/* WHAT IT DOES: reads a player slot's status word under that slot's lock. */
/* port-only body; Glide match is src/core/generated/0x10004D80.c */
int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    int32_t    value;

    BrNetMutexLock(p->hMutex);
    value = p->f02C;
    BrNetMutexUnlock(p->hMutex);
    return value;
}



/* 0x10005CF0 */
/* WHAT IT DOES: reads one identifying number out of a player slot under that
 * slot's lock. */
/* port-only body; Glide match is src/core/generated/0x10006060.c */
int32_t BrNetSlotGetF004(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    int32_t    value;

    BrNetMutexLock(p->hMutex);
    value = p->f004;
    BrNetMutexUnlock(p->hMutex);
    return value;
}

/* 0x10005E70 */
/* WHAT IT DOES: hands back a player's name, copied under that slot's lock
 * into a shared scratch buffer. Because the buffer is shared, the answer is
 * only good until the next call. The original copied with no length limit at
 * either end; this one is bounded. */
/* port-only body; Glide match is src/core/generated/0x100061E0.c */
char *BrNetSlotName(BrNetState *pNet, int32_t slot)
{
    BrNetSlot *p = &pNet->aSlots[slot];
    size_t     n;

    BrNetMutexLock(p->hMutex);
    /* DEVIATION: the original is an inlined `repne scasb` + `rep movsd/movsb`
     * strcpy into a fixed global with no length limit at either end. Bounded
     * here, and the source is bounded too in case the record is not
     * terminated. Behaviour is identical for any name that fits. */
    for (n = 0; n < sizeof p->f570 && p->f570[n] != '\0'; ++n)
        ;
    if (n >= sizeof pNet->aNameScratch)
        n = sizeof pNet->aNameScratch - 1;
    memcpy(pNet->aNameScratch, p->f570, n);
    pNet->aNameScratch[n] = '\0';
    BrNetMutexUnlock(p->hMutex);

    return pNet->aNameScratch;
}

/* 0x10005FE0 */
/* WHAT IT DOES: drops every player whose identifier matches the one given --
 * the disconnect path. For each match it puts the slot number on the free
 * list, clears the slot's status, and announces "<name> left the game" to
 * the other players. The announcement really does begin with a literal
 * percent-fifteen, which is a colour code in the game's message text. */
/* port-only body; Glide match is src/core/generated/0x10006350.c -- the
 * original takes only the key and reaches every slot through the globals,
 * which this signature cannot express. */
void BrNetDropMatching(BrNetState *pNet, int32_t key)
{
    char    szMsg[0x400];       /* the original's stack buffer, same size */
    int32_t i;

    for (i = 0; i < BR_NET_SLOTS; ++i) {
        if (BrNetSlotGetF004(pNet, i) != key)
            continue;
        /* `test al,0x3f` -- any of the low six flag bits. */
        if ((BrNetSlotGetF02C(pNet, i) & 0x3F) == 0)
            continue;

        BrNetMutexLock(pNet->h1022AF30);
        /* Pre-increment then store, so the index starts at 0 given the -1
         * BrNetReset leaves behind.
         *
         * DEVIATION: the original has no bound on this push. It cannot
         * overflow in one pass (16 slots, 16 entries) but repeated calls
         * without a reset would walk off the end of the array; guarded. */
        if (pNet->f10221318 + 1 < (int32_t)(sizeof pNet->a10221288 /
                                            sizeof pNet->a10221288[0])) {
            pNet->f10221318 += 1;
            pNet->a10221288[pNet->f10221318] = i;
        }
        BrNetMutexUnlock(pNet->h1022AF30);

        BrNetSlotSetF02C(pNet, i, 0);

        /* DEVIATION: sprintf -> snprintf. The '%%' is in the original literal
         * at 0x10094338, so the rendered text really does begin with "%15". */
        snprintf(szMsg, sizeof szMsg, "%%15%s left the game.",
                 pNet->aSlots[i].f570);
        BrNetAnnounce(szMsg);
    }
}

/* =====================================================================
 * 4. Palette fetch
 * ===================================================================== */

/* 0x100049C0.  Records are 3 bytes (`ecx + ecx*2 + base`) and the copy is
 * straight: source +0 -> dest +0, +1 -> +1, +2 -> +2 (no channel swap). The
 * original writes the last byte first; order is immaterial. */
/* WHAT IT DOES: reads one colour out of a palette: three bytes at the
 * entry's position, copied straight through with no channel reordering. */
/* @implements 0x100049C0 d3d BrPalFetch */
#ifdef BR_MATCHING_BUILD
/* The original takes no arguments: index is 0x10094294, table is
 * 0x100B37D0, dest is 0x10AD0854.  The port signature is the header's.
 * volatile on the index stops VC5 CSEing the three loads into one lea. */
extern volatile int32_t g_br094294; /* 0x10094294 */
extern uint8_t g_aBr0B37D0[];       /* 0x100B37D0 */
extern uint8_t g_brAD0854[3];       /* 0x10AD0854 */

void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    int i0, i1, i2, b0, b1, b2;

    i0 = g_br094294;
    i1 = g_br094294;
    b2 = g_aBr0B37D0[i0 * 3 + 2];
    i2 = g_br094294;
    b1 = g_aBr0B37D0[i1 * 3 + 1];
    b0 = g_aBr0B37D0[i2 * 3];
    g_brAD0854[2] = (uint8_t)b2;
    g_brAD0854[1] = (uint8_t)b1;
    g_brAD0854[0] = (uint8_t)b0;
}
#else
void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    const uint8_t *p = pTable + (ptrdiff_t)index * 3;

    aOut[2] = p[2];
    aOut[1] = p[1];
    aOut[0] = p[0];
}
#endif


