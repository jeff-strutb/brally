/* WHAT IT DOES: write a car's state as a difference from a previously sent
 * one, so a routine update costs far fewer bits. The facing goes out in full,
 * but position, height and one other field send only their low bits plus a
 * two-bit hint about how far the high part moved. That hint is deliberately
 * coarse: a jump of three or more steps cannot be reconstructed, a limitation
 * of the original format, not of this transcription. */
/* @implements 0x10006BA0 glide BrCarStateEncodeDelta
 * @cpp_symbol _BrCarStateEncodeDelta
 *
 * BYTE-EXACT under VC5 /O2 /GX /MD (2026-09-25).  Hand-transcribed from the
 * Glide bytes; C++ because the writer 0x1006D0B0 is a thiscall member (`this`
 * in ecx, `ret 8`).  The earlier verdict that this family is a VC4.2 object
 * is REFUTED: the whole TU (tu_006: Encode, the ten packers, this) is VC5,
 * like its byte-exact decode siblings.  Three source facts carry it:
 *
 *   - The writer's value parameter is `unsigned int` (ReadBits 0x1006CED0
 *     returns unsigned int too).  Converting a promoted `short >> 8` to an
 *     unsigned parameter is what makes VC5 shift in AX (`sar ax,8`) and then
 *     `movsx` -- WITHOUT a named 16-bit temporary, so the argument stays a
 *     pure expression and `push 8` goes out before the pack call.  An int
 *     parameter widens the shift; a `short` parameter drops the movsx; any
 *     short assignment (`q = ...`) is a side effect and pushes nBits late.
 *     ref/cur are `uint32_t` for the same reason (f18's `sar ax,1; movsx`).
 *   - `ref` is declared before `cur`: that is what makes the f10/f14 xor copy
 *     cur and xor ref in, as the original does.
 *   - The `|` operand roles (which of code/cur is the or's destination) are
 *     NOT set by source order -- VC5 canonicalises them, and the tie-break
 *     depends on the compiler's heap layout, i.e. on how much the TU declared
 *     before this function.  With the four headers below (math.h is certain:
 *     the packers in this TU call floor) f10/f14 are code-first and f18/f78
 *     cur-first, as in the original.  Measured with N dummy prototypes in
 *     their place: N=200..248 and 328..376 match, 0..192 leave f18's
 *     cur/code colouring swapped.  ‼ Adding or removing declarations above
 *     the function can flip this; re-score after any preamble edit.
 * The six tail bits are plain `!= 0.0f` arguments: VC5 emits the original's
 * `fcomp [pool 0.0]; fnstsw; test ah,0x40; jne; mov 1; jmp; xor` diamond at
 * each, and tail-duplicates the last call and epilogue itself.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

class BrBitStream {
public:
    void WriteBits(unsigned int value, unsigned int nBits);   /* 0x1006D0B0, declared only */
};

extern "C" {
short BrFixPackS16Q15Neg(float);   /* 0x10006950 */
int   BrFixPackU24Q13(float);      /* 0x10006A50 */
short BrFixPackS16Q7(float);       /* 0x10006AE0 */
int   BrFixPackS24Q1(float);       /* 0x10006AA0 */
int   BrFixPackU8Range(float);     /* 0x100069D0 */
int   BrFixPackLevel(float);       /* 0x10006A10 */
}

struct BrCarState {
    float f00, f04, f08, f0C;
    float f10, f14, f18, f1C, f20;
    float f24;
    float f28, f2C, f30, f34, f38, f3C;
    float f40, f44, f48;
    float f4C, f50, f54, f58;
    float f5C, f60, f64, f68;
    float f6C, f70, f74, f78, f7C, f80, f84;
    float f88, f8C, f90, f94, f98, f9C;
};

/* The two-bit movement hint for the masked high part: 0 = unchanged, one
 * step = exactly one step up, two steps = further up, three steps = at or
 * below the reference.  Returned pre-shifted into the field's code bits. */
static __inline int32_t BrCarStateDeltaCode(uint32_t cur, uint32_t ref,
                                            uint32_t hiMask, uint32_t step)
{
    uint32_t code;
    uint32_t hiRef, hiCur;

    if (((cur ^ ref) & hiMask) == 0) {
        code = 0;
    } else {
        hiRef = ref & hiMask;
        hiCur = cur & hiMask;
        if (hiRef + step == hiCur)
            code = step;
        else
            code = (hiRef < hiCur) ? 2 * step : 3 * step;
    }
    return (int32_t)(code & (3 * step));
}

extern "C"
void BrCarStateEncodeDelta(BrBitStream *pBs, const BrCarState *pCur, const BrCarState *pRef)
{
    uint32_t ref, cur;

    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f00) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f04) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f08) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f0C) >> 8, 8);

    /* f10, f14: 17-bit quantities, 12 bits sent plus a code on 0x1F000. */
    ref = ((uint32_t)BrFixPackU24Q13(pRef->f10) >> 7);
    cur = ((uint32_t)BrFixPackU24Q13(pCur->f10) >> 7);
    pBs->WriteBits(BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);
    ref = ((uint32_t)BrFixPackU24Q13(pRef->f14) >> 7);
    cur = ((uint32_t)BrFixPackU24Q13(pCur->f14) >> 7);
    pBs->WriteBits(BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);

    /* f18: 15-bit signed quantity, 9 bits sent plus a code on 0x7E00. */
    ref = BrFixPackS16Q7(pRef->f18) >> 1;
    cur = BrFixPackS16Q7(pCur->f18) >> 1;
    pBs->WriteBits((cur & 0x1FF) | BrCarStateDeltaCode(cur, ref, 0x7E00, 0x200), 11);

    /* f78: 24-bit signed quantity, 7 bits sent plus a code on 0xFFFF80. */
    ref = BrFixPackS24Q1(pRef->f78);
    cur = BrFixPackS24Q1(pCur->f78);
    pBs->WriteBits((cur & 0x7F) | BrCarStateDeltaCode(cur, ref, 0xFFFF80, 0x80), 9);

    pBs->WriteBits((uint8_t)BrFixPackU8Range(pCur->f7C), 6);
    pBs->WriteBits((uint8_t)BrFixPackLevel(pCur->f80), 2);
    pBs->WriteBits((uint8_t)BrFixPackLevel(pCur->f84), 2);

    /* NaN compares equal to zero here (C3 covers unordered), as in the
     * original. */
    pBs->WriteBits(pCur->f88 != 0.0f, 1);
    pBs->WriteBits(pCur->f8C != 0.0f, 1);
    pBs->WriteBits(pCur->f90 != 0.0f, 1);
    pBs->WriteBits(pCur->f94 != 0.0f, 1);
    pBs->WriteBits(pCur->f98 != 0.0f, 1);
    pBs->WriteBits(pCur->f9C != 0.0f, 1);
}
