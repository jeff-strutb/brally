/* WHAT IT DOES: reads one car's complete state out of a network packet and
 * fills in the car record: facing, position, speed-like values, wheel or
 * suspension figures, an angle (which fills four fields, twice raw and twice
 * offset by 35 degrees and wrapped), and a stack of on/off flags. Four
 * fields are deliberately left alone and one is zeroed without any bits
 * being read, so the caller must not assume the whole record was written. */
/* @implements 0x10007230 glide BrCarStateDecode
 * @cpp_symbol _BrCarStateDecode
 *
 * The C transcription (slice1_02.c, tagged at the D3D twin 0x10006EC0) is
 * shape-exact except for the reader's calling convention: the original
 * reads bits through a THISCALL member (`this` in ecx, ONE stack argument,
 * callee-cleaned) at each of the ~32 sites, where the C shim pushes the
 * reader and cleans the stack itself -- 32 surplus push/add-esp pairs
 * against 32 missing `mov ecx,r` (the fn.py multiset, 2026-09-12).  Same
 * family evidence as the encode pair (0x10006510 / 0x10006BA0 dossiers):
 * the calling TU was C++.  This file is that TU's shape: the reader is a
 * declared-not-defined native-thiscall class method, the unpack helpers
 * stay extern "C" cdecl, and the body is the slice1_02.c transcription
 * verbatim. */
#include <stdint.h>

class BrBitReader {
public:
    uint32_t ReadBits(unsigned nBits);   /* thiscall, declared only */
};

extern "C" {
float BrFixUnpackS16Q15Neg(int32_t v);
float BrFixUnpackU32Q13(uint32_t v);
float BrFixUnpackS16Q7(int32_t v);
float BrFixUnpackS16Q8(int32_t v);
float BrFixUnpackS8Q3(unsigned char v);   /* int32 in the C tree; byte here so the promoted push is unmasked */
float BrFixUnpackS6Q7Neg(unsigned char v);
float BrFixUnpackU8Angle(unsigned char v);
float BrFixUnpackS24Q1(uint32_t v);
float BrFixUnpackU8Range(int32_t v);
float BrFixUnpackLevel(int32_t v);
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

/* 0x1008F0CC = 128.0f and 0x1008F118 = 1.0f: both are used as the "true"
 * value, for different 1-bit fields. */
#define BR_ONE_128  128.0f
#define BR_ONE_1      1.0f

extern "C"
void BrCarStateDecode(BrCarState *pDst, BrBitReader *pReader)
{
    float angle, wrapped;
    uint8_t b;

    /* Four s8s, each shifted into the high byte of a 16-bit word first
     * (`xor ecx,ecx / mov ch,al`), so the effective scale is 1/-128. */
    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));

    pDst->f10 = BrFixUnpackU32Q13(pReader->ReadBits(17) << 7);
    pDst->f14 = BrFixUnpackU32Q13(pReader->ReadBits(17) << 7);
    pDst->f18 = BrFixUnpackS16Q7((int32_t)(pReader->ReadBits(15) << 1));

    pDst->f1C = BrFixUnpackS16Q8((int32_t)pReader->ReadBits(16));
    pDst->f20 = BrFixUnpackS16Q8((int32_t)pReader->ReadBits(16));

    pDst->f24 = 0.0f;                   /* stored, but no bits are consumed */

    /* `shl al,N` -- an 8-bit shift: a plain scalar uint8_t local shifted in
     * place keeps the op at byte width (byte-slot idiom, VC5-IDIOMS). */
    b = (uint8_t)pReader->ReadBits(5);
    b <<= 3;
    pDst->f28 = BrFixUnpackS8Q3(b);
    b = (uint8_t)pReader->ReadBits(5);
    b <<= 3;
    pDst->f2C = BrFixUnpackS8Q3(b);
    b = (uint8_t)pReader->ReadBits(5);
    b <<= 3;
    pDst->f30 = BrFixUnpackS8Q3(b);
    b = (uint8_t)pReader->ReadBits(4);
    b <<= 4;
    pDst->f34 = BrFixUnpackS8Q3(b);
    b = (uint8_t)pReader->ReadBits(4);
    b <<= 2;
    pDst->f38 = BrFixUnpackS6Q7Neg(b);

    /* One 4-bit angle feeds four fields: the raw value twice, then the same
     * value biased by +35 degrees and wrapped, twice. 0x1008F110 = -35.0f is
     * subtracted (hence the bias is positive) and 0x1008F114 = 360.0f. */
    b = (uint8_t)pReader->ReadBits(4);
    b <<= 4;
    angle = BrFixUnpackU8Angle(b);
    pDst->f40 = angle;
    pDst->f3C = angle;
    wrapped = angle - (-35.0f);
    if (wrapped >= 360.0f)
        wrapped -= 360.0f;
    pDst->f48 = wrapped;
    pDst->f44 = wrapped;

    /* 1-bit flags widened through `fild qword`, i.e. plain 0.0f / 1.0f. */
    pDst->f4C = (float)pReader->ReadBits(1);
    pDst->f50 = (float)pReader->ReadBits(1);
    pDst->f54 = (float)pReader->ReadBits(1);
    pDst->f58 = (float)pReader->ReadBits(1);

    /* f5C, f60, f64, f68 are never written by this routine. */

    pDst->f6C = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f70 = pReader->ReadBits(1) ? BR_ONE_1   : 0.0f;
    pDst->f74 = pReader->ReadBits(1) ? BR_ONE_1   : 0.0f;

    pDst->f78 = BrFixUnpackS24Q1(pReader->ReadBits(24));
    pDst->f7C = BrFixUnpackU8Range((int32_t)pReader->ReadBits(6));
    pDst->f80 = BrFixUnpackLevel((int32_t)pReader->ReadBits(2));
    pDst->f84 = BrFixUnpackLevel((int32_t)pReader->ReadBits(2));

    pDst->f88 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
}
