/* slice1_09.c -- decompiled from BRD3D.dll, range 100734F0-10078CD0.
 * See slice1_09.h for the recovered layouts and the argument-order notes.
 *
 * Skipped functions and the reason for each are listed at the bottom of this
 * file so the information does not get lost.
 */
#include "slice1_09.h"

#include <math.h>
#include <stddef.h>

/* ================================================================== */
/* Bit/byte stream                                                     */
/* ================================================================== */

/* 0x10073D20 -- align the READ cursor.
 *
 * This is the routine already exported as BrObjConsumeFlag() in br_obj.h
 * (its "flag" is readBit and its "count" is readByte). It is reproduced as a
 * static here for one reason only: so this translation unit links without
 * br_obj.o. At integration the two should be collapsed into one.
 * DEVIATION: duplicate of an existing symbol, kept private (static). */
static void BrBitStreamAlignRead(BrBitStream *pBs)
{
    if (pBs->readBit != 0) {
        pBs->readBit = 0;
        pBs->readByte++;
    }
}

/* 0x10073F20 */
void BrBitStreamAlignWrite(BrBitStream *pBs)
{
    if (pBs->writeBit != 0) {
        pBs->writeBit = 0;
        pBs->writeByte++;
    }
}

/* 0x10073B60  __thiscall(ecx=this), ret 8.
 * Note the original zeroes +0x08 and +0x00/+0x04 and then stores arg2 into
 * +0x0C and arg1 into +0x10 -- the LENGTH goes into the write byte cursor. */
void BrBitStreamInit(BrBitStream *pBs, void *pBuf, int nBytes)
{
    pBs->writeBit  = 0;
    pBs->readBit   = 0;
    pBs->readByte  = 0;
    pBs->writeByte = nBytes;
    pBs->pBuf      = (unsigned char *)pBuf;
}

/* 0x10073BA0  __thiscall, ret 4 */
void BrBitStreamSkipBytes(BrBitStream *pBs, int n)
{
    BrBitStreamAlignRead(pBs);
    pBs->readByte += n;
}

/* 0x10073BC0  __thiscall.
 * The original returns the byte in AL only; the upper 24 bits of EAX are
 * left holding the top of the buffer pointer. Callers that used it as an
 * int would therefore see garbage, so it is typed as a byte here.
 * DEVIATION: return narrowed from the register-width EAX to unsigned char. */
unsigned char BrBitStreamReadU8(BrBitStream *pBs)
{
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    pBs->readByte = i + 1;
    return pBs->pBuf[i];
}

/* 0x10073BE0  big-endian u16; DH gets byte 0, DL byte 1, EDX pre-zeroed. */
unsigned int BrBitStreamReadU16(BrBitStream *pBs)
{
    const unsigned char *p;
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    p = pBs->pBuf + i;
    pBs->readByte = i + 2;
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

/* 0x10073C10  big-endian u24, zero-extended. */
unsigned int BrBitStreamReadU24(BrBitStream *pBs)
{
    const unsigned char *p;
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    p = pBs->pBuf + i;
    pBs->readByte = i + 3;
    return ((unsigned int)p[0] << 16)
         | ((unsigned int)p[1] << 8)
         |  (unsigned int)p[2];
}

/* 0x10073C40  big-endian 32-bit.
 * The original builds the value as ((((s8)p[0] << 8 | p[1]) << 8 | p[2]) << 8
 * | p[3]); the movsx on p[0] is shifted out entirely, so the result is the
 * plain big-endian word. Built here through unsigned to avoid signed
 * overflow, then converted -- the bit pattern is identical.
 * DEVIATION: unsigned intermediate, because a signed left shift into the
 * sign bit is undefined in C99. */
int BrBitStreamReadS32(BrBitStream *pBs)
{
    const unsigned char *p;
    unsigned int v;
    int i;
    BrBitStreamAlignRead(pBs);
    i = pBs->readByte;
    p = pBs->pBuf + i;
    pBs->readByte = i + 4;
    v = ((unsigned int)p[0] << 24)
      | ((unsigned int)p[1] << 16)
      | ((unsigned int)p[2] << 8)
      |  (unsigned int)p[3];
    return (int)v;
}

/* 0x10073C90  __thiscall, ret 4.
 *
 * Per round it takes  take = min(8 - readBit, remaining)  bits out of the
 * current byte, from the position  shift = 8 - readBit - take, and appends
 * them to the accumulator low end after shifting the accumulator left by
 * `take`. So the stream is MSB-first.
 *
 * The original sign-extends the source byte before masking; the mask never
 * reaches above bit 7, so that has no effect and is not reproduced.
 *
 * The original also keeps a running total of bits consumed in a stack local
 * ([esp+0x10]) that nothing ever reads -- a dead store, omitted.
 *
 * DEVIATION: nBits < 0 is not reproduced. The original would shift by
 * (nBits & 31) and return garbage after a single round; in C that is
 * undefined, so this function simply must not be called with nBits < 0.
 * nBits > 32 loses the high bits in both versions. */
unsigned int BrBitStreamReadBits(BrBitStream *pBs, int nBits)
{
    unsigned int acc = 0;
    int remaining = nBits;

    if (remaining == 0)
        return 0;

    do {
        int avail = 8 - pBs->readBit;
        int take, shift;
        int byteIndex;
        unsigned int mask, v;

        if (avail > remaining) {
            take  = remaining;
            shift = avail - remaining;
        } else {
            take  = avail;
            shift = 0;
        }

        byteIndex = pBs->readByte;
        mask = ((1u << take) - 1u) << shift;
        v    = (mask & (unsigned int)pBs->pBuf[byteIndex]) >> shift;

        pBs->readBit += take;
        acc = (acc << take) | v;

        if (pBs->readBit >= 8) {
            pBs->readBit  = 0;
            pBs->readByte = byteIndex + 1;
        }

        remaining -= take;
    } while (remaining != 0);

    return acc;
}

/* 0x10073D40  __thiscall, no stack args. Signed compare (setge). */
int BrBitStreamAtEnd(const BrBitStream *pBs)
{
    int pos = pBs->readByte;
    if (pBs->readBit != 0)
        pos++;
    return pos >= pBs->writeByte ? 1 : 0;
}

/* 0x10073D60  __thiscall, ret 4. Only the low byte of the argument is used. */
void BrBitStreamWriteU8(BrBitStream *pBs, unsigned int v)
{
    BrBitStreamAlignWrite(pBs);
    pBs->pBuf[pBs->writeByte] = (unsigned char)v;
    pBs->writeByte++;
}

/* 0x10073DC0  big-endian 24-bit. */
void BrBitStreamWriteU24(BrBitStream *pBs, unsigned int v)
{
    int w;
    BrBitStreamAlignWrite(pBs);
    w = pBs->writeByte;
    pBs->pBuf[w]     = (unsigned char)(v >> 16);
    pBs->pBuf[w + 1] = (unsigned char)(v >> 8);
    pBs->pBuf[w + 2] = (unsigned char)v;
    pBs->writeByte = w + 3;
}

/* 0x10073E10  big-endian 32-bit. */
void BrBitStreamWriteU32(BrBitStream *pBs, unsigned int v)
{
    int w;
    BrBitStreamAlignWrite(pBs);
    w = pBs->writeByte;
    pBs->pBuf[w]     = (unsigned char)(v >> 24);
    pBs->pBuf[w + 1] = (unsigned char)(v >> 16);
    pBs->pBuf[w + 2] = (unsigned char)(v >> 8);
    pBs->pBuf[w + 3] = (unsigned char)v;
    pBs->writeByte = w + 4;
}

/* ================================================================== */
/* Float math                                                          */
/* ================================================================== */

/* 0x10074250.
 * x87 order recovered by tracing the fxch chain: the sum is built as
 * (f04^2 + f08^2) + f00^2, then  k = 1.0f / sqrtf(sum)  (fdivr against the
 * 1.0f at 0x1008FC48), then each component is multiplied by k. The
 * association is preserved below even though it cannot change the result by
 * more than rounding. */
void BrVec3Normalise(BrVec3 *pV)
{
    float sum = (pV->y * pV->y + pV->z * pV->z) + pV->x * pV->x;
    float k   = 1.0f / sqrtf(sum);
    pV->x *= k;
    pV->y *= k;
    pV->z *= k;
}

/* 0x100741B0. Same shape over four components; sum is
 * ((f04^2 + f08^2) + f0C^2) + f00^2. */
void BrVec4Normalise(BrVec4 *pV)
{
    float sum = ((pV->f04 * pV->f04 + pV->f08 * pV->f08)
                 + pV->f0C * pV->f0C) + pV->f00 * pV->f00;
    float k   = 1.0f / sqrtf(sum);
    pV->f00 *= k;
    pV->f04 *= k;
    pV->f08 *= k;
    pV->f0C *= k;
}

/* 0x100747C0.
 * Written out longhand rather than with temporaries so that the write order
 * matches the original exactly: each output component is zeroed and fully
 * accumulated before the next one begins, and the translation row is added
 * to all three only afterwards. That ordering is observable when pOut
 * aliases pV. */
void BrMat4TransformPoint(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    pOut->x = 0.0f;
    pOut->x = pV->x * pM->m[0][0] + pOut->x;
    pOut->x = pV->y * pM->m[1][0] + pOut->x;
    pOut->x = pV->z * pM->m[2][0] + pOut->x;

    pOut->y = 0.0f;
    pOut->y = pV->x * pM->m[0][1] + pOut->y;
    pOut->y = pV->y * pM->m[1][1] + pOut->y;
    pOut->y = pV->z * pM->m[2][1] + pOut->y;

    pOut->z = 0.0f;
    pOut->z = pV->x * pM->m[0][2] + pOut->z;
    pOut->z = pV->y * pM->m[1][2] + pOut->z;
    pOut->z = pV->z * pM->m[2][2] + pOut->z;

    pOut->x = pM->m[3][0] + pOut->x;
    pOut->y = pM->m[3][1] + pOut->y;
    pOut->z = pM->m[3][2] + pOut->z;
}

/* ================================================================== */
/* Entity array offsets                                                */
/* ================================================================== */

/* 0x100307D0 -- BrMat4Identity (br_mat.h). Reproduced as a static for the
 * same reason as BrBitStreamAlignRead above: 0x10076C90 calls it and this
 * translation unit must link on its own.
 * DEVIATION: duplicate of an existing symbol, kept private (static). */
static void BrMat4IdentityLocal(BrMat4 *pM)
{
    int r, c;
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            pM->m[r][c] = (r == c) ? 1.0f : 0.0f;
}

/* 0x10076AE0  __thiscall, ret 4. `cmp eax,0x10 / jl` -- signed. */
void BrEntitySetIndex(void *pEntity, int index)
{
    unsigned char *p = (unsigned char *)pEntity;
    int *pIndex = (int *)(void *)(p + BR_ENTITY_OFF_INDEX);
    int *pBank  = (int *)(void *)(p + BR_ENTITY_OFF_BANK);

    if (index >= 16) {
        *pBank  = 1;
        *pIndex = index - 16;
    } else {
        *pBank  = 0;
        *pIndex = index;
    }
}

/* 0x10076C90  __thiscall.
 *
 * The original is  idx = (this - 0x10ACDEA8) / 0x2B68  performed with the
 * magic multiply 0x5E5D422B followed by `sar edx,12` and the usual
 * shr/add sign fix -- i.e. plain signed division truncating toward zero.
 * The 348-byte scale is assembled as 348 = ((idx*8 - idx)*4 + idx)*3*4
 * through three LEAs.
 *
 * DEVIATION: the two array bases are parameters instead of the hardcoded
 * 0x10ACDEA8 / 0x106C6678, and the aux pointer is stored as a host pointer
 * (8 bytes on a 64-bit build) where the original stored a 32-bit value. The
 * two strides below are the ORIGINAL 32-bit strides and are not adjusted --
 * they are the sizes of the game's own structures, not of anything declared
 * here. */
void BrEntityBindAux(void *pEntity, void *pEntityArrayBase,
                     void *pAuxArrayBase)
{
    unsigned char *p    = (unsigned char *)pEntity;
    unsigned char *pAux = (unsigned char *)pAuxArrayBase;
    ptrdiff_t      idx  = (p - (unsigned char *)pEntityArrayBase)
                          / BR_ENTITY_STRIDE;
    void **ppAux = (void **)(void *)(p + BR_ENTITY_OFF_AUX);

    *ppAux = pAux + idx * BR_ENTITY_AUX_STRIDE;
    BrMat4IdentityLocal((BrMat4 *)(void *)(p + BR_ENTITY_OFF_MATRIX));
}

/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x10073A10 (PARTIAL).
 *
 * The full original does three things: two calls through the import pointer
 * at 0x118AA0B0 with fourteen constant arguments (an unidentified 0x40x0x40
 * surface/texture creation), then this table build, then a call to
 * sub_100098A0(dst=0x11829118, src=0x11829330, size=0x40, format=2) whose
 * byte return is divided by 16. Only the table build is portable and
 * identifiable, so only it is ported; the rest is reported as skipped.
 *
 * The loop bound in the original is `cmp eax, 0x11829371 / jl` against a
 * cursor that starts at base+1 and steps 4, giving exactly 16 iterations
 * over a 0x40-byte table at 0x11829330. */
void BrAlphaRampBuild(unsigned char *pOut)
{
    int i;
    for (i = 0; i < 16; ++i) {
        pOut[i * 4 + 0] = 0xFF;
        pOut[i * 4 + 1] = 0xFF;
        pOut[i * 4 + 2] = 0xFF;
        pOut[i * 4 + 3] = (unsigned char)((i << 4) | i);
    }
}

/* 0x10074F70.
 *
 * DEVIATION: the original brackets the whole body with
 * WaitForSingleObject(g_18AA0A0, INFINITE) / ReleaseMutex(g_18AA0A0). The
 * mutex is dropped here -- callers must serialise. The ring itself is
 * otherwise verbatim, including the fact that the write index is stored
 * back before the bounds test and then overwritten with 0 when it reached
 * 0x100. There is no read cursor and no fullness check anywhere in the
 * original: entry 0 is simply overwritten on the 257th push. */
void BrPairRingPush(BrPairRing *pRing, int a, int b)
{
    int i = pRing->write;

    pRing->aItems[i].a = a;
    pRing->aItems[i].b = b;

    i++;
    pRing->write = i;
    if (i >= BR_PAIR_RING_SLOTS)
        pRing->write = 0;
}

/* 0x10075100.
 *
 * The original calls the platform timer at 0x10075020 for `ms` and then
 * does the arithmetic below with `div esi` (esi = 100) and two unsigned
 * magic multiplies: 0x51EB851F >> 37 is ms/100 and 0x3E0F83E1 >> 35 is
 * (ms % 100) / 33.
 *
 * DEVIATION: `ms` is a parameter instead of a call into
 * QueryPerformanceCounter / timeGetTime (0x10075020, skipped -- see below).
 * Everything else is verbatim, including the order of the three stores. */
void BrTimeUpdate(BrTimeState *pState, unsigned int ms)
{
    pState->f12C   = 0;
    pState->ms     = ms;
    pState->tick30 = (ms % 100u) / 33u + 3u * (ms / 100u);
}

/* ==================================================================
 * SKIPPED, with reasons
 * ==================================================================
 *
 * Already implemented elsewhere (not re-done):
 *   0x10073B40 0x10073B80 0x10073D20 0x10073F40 0x10073F50  br_obj.h
 *   0x10074030  BrHandleLookup (br_bits.h)
 *   0x10074720 0x10074770                                   br_mat.h
 *
 * Windows / COM / platform-only, nothing portable inside:
 *   0x100734F0  tears down the g_1828F48 object and clears a 0x48-stride
 *               table plus a 60-byte block; pure global bookkeeping around
 *               two calls into an unrecovered class.
 *   0x10073560  DirectSound-family init: GlobalAlloc/GlobalLock a 0x12-byte
 *               descriptor, fill it (0x5622, 0x15888, 4, 0x10, 0),
 *               CoCreateInstance, then five vtable calls with the usual
 *               release-on-failure ladder. Nothing to port.
 *   0x10073950  one call through the import at 0x118AA0B0 with 14 constant
 *               arguments (0x40 x 0x40, format 4). Callee unidentified.
 *   0x100739E0  the same call with a different source and all-zero flags.
 *   0x100770F0  COM/DirectSound init behind a +1 refcount guard; three
 *               vtable calls, each with its own bail-out.
 *   0x10078CD0  SEH frame, MessageBoxA on failure, two more vtable calls.
 *   0x10075020  QueryPerformanceFrequency / QueryPerformanceCounter with a
 *               64-bit multiply/divide pair (0x1007ED20, 0x1007FD10) and a
 *               timeGetTime fallback. The portable core is
 *               ms = (ticks * 1000 + 500) / freq  -- note the +500, i.e.
 *               round-to-nearest, not truncation -- minus a base captured on
 *               the first call. Not ported because it is entirely a wrapper
 *               over two Win32 clocks.
 *   0x10076CE0 0x10076E90 0x10076ED0 0x10076FA0  RIFF/WAVE loading built
 *               entirely on WINMM's mmio* API. Error codes, for whoever
 *               reimplements them: 0xE000 out of memory, 0xE100 open
 *               failed, 0xE101 malformed/short RIFF, 0xE102 short read of
 *               the fmt chunk, 0xE103 mmio buffer exhausted mid-copy.
 *               0x10076CE0 additionally hardcodes the assumption that a
 *               PCM (wFormatTag == 1) header has no cbSize field and reads
 *               only 16 bytes for it.
 *
 * Layout not established well enough to port:
 *   0x100770C0  zeroes 14 dwords at 0x118ABD38, zeroes 0x118ABAD4 and sets
 *               0x118ABD80 to 1. The three globals are 0x2A4 apart and
 *               0x48 apart respectively with nothing to tie them into one
 *               structure, so any struct here would be invented.
 */
