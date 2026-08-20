/* slice1_09.h -- decompiled from BRD3D.dll, address range 100734F0-10078CD0.
 *
 * Contents, by original address:
 *
 *   bit/byte stream     0x10073B60 0x10073BA0 0x10073BC0 0x10073BE0 0x10073C10
 *                       0x10073C40 0x10073C90 0x10073D40 0x10073D60 0x10073DC0
 *                       0x10073E10 0x10073F20
 *   float math          0x100741B0 0x10074250 0x100747C0
 *   entity offsets      0x10076AE0 0x10076C90
 *   misc                0x10073A10 (partial) 0x10074F70 0x10075100
 *
 * See slice1_09.c for the DEVIATION notes and the per-function derivations.
 */
#ifndef SLICE1_09_H
#define SLICE1_09_H

#include "br_match.h"   /* BR_THISCALL1 -- the bit-stream class is thiscall */

#include <stddef.h>

#include "br_vec.h"
#include "br_mat.h"

/* ------------------------------------------------------------------ */
/* Bit/byte stream                                                     */
/* ------------------------------------------------------------------ */
/* One class, used for both reading and writing, with two independent
 * cursors into the same buffer. The layout is pinned by five routines that
 * touch it (three of which are already in br_obj.h under positional names --
 * see the note below):
 *
 *   +0x00  read  bit position within the current byte, 0..7
 *   +0x04  read  byte position
 *   +0x08  write bit position within the current byte, 0..7
 *   +0x0C  write byte position
 *   +0x10  buffer base
 *
 * INTEGRATION NOTE: br_obj.h's "small object accessors" are members
 * of THIS class, not of three different structs:
 *   0x10073B40 BrObjInitInline   -> ctor for the inline-buffer flavour
 *                                   (pBuf = this + 0x14)
 *   0x10073B80 BrObjClear        -> reset both cursors, keep the buffer
 *   0x10073D20 BrObjConsumeFlag  -> align the READ cursor to a byte boundary
 *   0x10073F40                   -> bytes written = writeByte + (writeBit!=0)
 *   0x10073F50 BrObjGetF10       -> the buffer pointer
 * The "flag/count" and "f00..f10" readings are consistent with the bytes but
 * the fields are bit position / byte position / buffer.
 *
 * All of these are __thiscall in the original; the object is passed here as
 * the first argument.
 *
 * Every multi-byte accessor is BIG-ENDIAN, and every byte-granular accessor
 * first aligns its cursor to a byte boundary (discarding a partially
 * consumed byte). Reads are MSB-first.
 */
typedef struct BrBitStream {
    int            readBit;    /* +0x00 */
    int            readByte;   /* +0x04 */
    int            writeBit;   /* +0x08 */
    int            writeByte;  /* +0x0C */
    unsigned char *pBuf;       /* +0x10 */
} BrBitStream;

/* 0x10073B60  read cursor = 0, write bit = 0, write byte = nBytes, buf = pBuf.
 *
 * Argument order follows the original's pushes: the BUFFER is arg1 and the
 * length arg2, but the length lands in the WRITE byte cursor (+0x0C) while
 * the buffer lands at +0x10. That is what makes BrBitStreamAtEnd work on a
 * read-only stream: "end" means the read cursor has caught up with the write
 * cursor, and for a pre-filled buffer the write cursor is the payload size. */
void         BrBitStreamInit(BrBitStream *pBs, void *pBuf, int nBytes);

/* 0x10073F20  align the WRITE cursor: if writeBit != 0, zero it and step
 * writeByte. (The read-side twin is 0x10073D20 / BrObjConsumeFlag.) */
void         BR_THISCALL1 BrBitStreamAlignWrite(BrBitStream *pBs);

/* 0x10073BA0  align the read cursor, then advance it by n BYTES.
 * n is not range-checked. */
void         BrBitStreamSkipBytes(BrBitStream *pBs, int n);

/* 0x10073BC0  align, then read one byte. */
unsigned char BR_THISCALL1 BrBitStreamReadU8(BrBitStream *pBs);
/* 0x10073BE0  align, then read 2 bytes big-endian. */
unsigned int  BR_THISCALL1 BrBitStreamReadU16(BrBitStream *pBs);
/* 0x10073C10  align, then read 3 bytes big-endian, zero-extended. */
unsigned int  BR_THISCALL1 BrBitStreamReadU24(BrBitStream *pBs);
/* 0x10073C40  align, then read 4 bytes big-endian.
 * The original sign-extends the FIRST byte (movsx) before shifting it left
 * 24 places, so the sign extension is discarded and the result is just the
 * 32-bit big-endian word -- but the natural return type is signed. */
int           BR_THISCALL1 BrBitStreamReadS32(BrBitStream *pBs);

/* 0x10073C90  read nBits bits MSB-first, WITHOUT aligning first.
 * Crosses byte boundaries; the accumulator is shifted left by the number of
 * bits taken each round, so the first bit read is the most significant bit
 * of the result. nBits == 0 returns 0 and consumes nothing. */
unsigned int  BrBitStreamReadBits(BrBitStream *pBs, int nBits);

/* 0x10073D40  1 once the read cursor has reached the write cursor.
 * A partially consumed byte counts as consumed: the test is
 *   readByte + (readBit != 0) >= writeByte     (signed compare). */
int           BR_THISCALL1 BrBitStreamAtEnd(const BrBitStream *pBs);

/* 0x10073D60  align the write cursor, then write the low byte of v. */
void          BrBitStreamWriteU8(BrBitStream *pBs, unsigned int v);
/* 0x10073DC0  align, then write bits 23..0 of v big-endian (3 bytes). */
void          BrBitStreamWriteU24(BrBitStream *pBs, unsigned int v);
/* 0x10073E10  align, then write v big-endian (4 bytes). */
void          BrBitStreamWriteU32(BrBitStream *pBs, unsigned int v);

/* ------------------------------------------------------------------ */
/* Float math                                                          */
/* ------------------------------------------------------------------ */

/* Four consecutive floats at +0x00/+0x04/+0x08/+0x0C. One caller's failure
 * path writes 1.0f to element 0 and 0.0f to the other three, which is a
 * quaternion identity, but which element is the scalar is not established --
 * so the fields are positional. Normalisation is symmetric anyway. */
typedef struct BrVec4 { float f00, f04, f08, f0C; } BrVec4;

/* 0x10074250  normalise a BrVec3 IN PLACE (no output parameter).
 *
 * v *= 1.0f / sqrtf(x*x + y*y + z*z), where the 1.0f is the constant at
 * 0x1008FC48 (verified = 0x3F800000 in the shipped DLL) and the square root
 * is the fsqrt wrapper at 0x10002250.
 *
 * WARNING: unlike BrVec3dNormalise (br_vecd.h) there is NO zero-length
 * guard. A zero vector yields NaNs. Do not add a guard "for safety" -- the
 * double-precision routine has one and this one does not, and that
 * difference is in the original. */
void BrVec3Normalise(BrVec3 *pV);

/* 0x100741B0  the same thing over four components, in place. Same missing
 * zero guard. */
void BrVec4Normalise(BrVec4 *pV);

/* 0x100747C0  out = v * M  --  the row-vector transform of a POINT:
 *
 *     out[i] = sum_j v[j] * m[j][i]  +  m[3][i]
 *
 * i.e. the upper 3x3 is applied TRANSPOSED relative to BrMat4MulVec3
 * (br_mat.h, 0x10074720) and identically to BrMat4MulVec3Transposed
 * (0x10074770), then the fourth ROW m[3][0..2] is added as the translation.
 * That is consistent with the row-vector convention BrMat4Frustum builds.
 *
 * Argument order is (out, matrix, vector) -- confirmed at the call sites
 * around 0x10061DA8, which pass a 0x40-byte matrix as arg2.
 *
 * ALIASING: out[0] is written before v[0] is read for the second and third
 * output components, so pOut must not alias pV. The original has the same
 * hazard; it is reproduced rather than fixed. */
void BrMat4TransformPoint(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV);

/* ------------------------------------------------------------------ */
/* Entity array offsets                                                */
/* ------------------------------------------------------------------ */
/* Two routines operate on an object of unknown layout that lives in a global
 * array based at 0x10ACDEA8. The element size is 0x2B68 = 11112 bytes,
 * established twice and independently:
 *   - 0x10076C90 divides (this - 0x10ACDEA8) by 11112 using the magic
 *     multiply 0x5E5D422B >> 44;
 *   - callers at 0x1002C65D / 0x1002C6A4 pass 0x10ACDEA8 and 0x10AD0A10,
 *     which differ by exactly 0x2B68.
 * Only four fields are touched, so no struct is invented: the object is
 * passed as void * and the fields are reached by byte offset. */
#define BR_ENTITY_STRIDE      11112   /* 0x2B68 */
#define BR_ENTITY_AUX_STRIDE  348     /* 0x15C  -- stride of the array at
                                       *          0x106C6678 */
#define BR_ENTITY_OFF_MATRIX  0x26D4  /* BrMat4 */
#define BR_ENTITY_OFF_INDEX   0x29A8  /* int */
#define BR_ENTITY_OFF_BANK    0x29B4  /* int */
#define BR_ENTITY_OFF_AUX     0x29C0  /* pointer into the 0x106C6678 array */

/* 0x10076AE0  split a signed index into (bank, index-within-bank) at 16:
 *   index >= 16  ->  bank = 1, stored index = index - 16
 *   index <  16  ->  bank = 0, stored index = index      (negatives included)
 * The comparison is SIGNED, so a negative index takes the bank-0 path and is
 * stored unchanged. */
void BrEntitySetIndex(void *pEntity, int index);

/* 0x10076C90  recover the entity's own index from its address and point
 * +0x29C0 at the matching element of the auxiliary array, then set the
 * BrMat4 at +0x26D4 to identity.
 *
 * The original hardcodes the two array bases (0x10ACDEA8 and 0x106C6678);
 * they are parameters here. Passing pEntity itself as pEntityArrayBase
 * therefore selects element 0 of the auxiliary array. */
void BrEntityBindAux(void *pEntity, void *pEntityArrayBase,
                     void *pAuxArrayBase);

/* ------------------------------------------------------------------ */
/* Misc                                                                */
/* ------------------------------------------------------------------ */

/* 0x10073A10 (PARTIAL -- see slice1_09.c).
 * Fill 16 RGBA8888 texels with white and a linear alpha ramp:
 *   texel[i] = { 0xFF, 0xFF, 0xFF, (i << 4) | i }
 * so alpha runs 0x00, 0x11, ... 0xFF. pOut must have room for 64 bytes. */
void BrAlphaRampBuild(unsigned char *pOut);

/* 0x10074F70  a 256-entry ring of (int,int) pairs with a write cursor and no
 * read cursor: past 256 pushes it wraps and silently overwrites. */
#define BR_PAIR_RING_SLOTS 256
typedef struct BrPairRingItem { int a, b; } BrPairRingItem;
typedef struct BrPairRing {
    int            write;                          /* 0x118A9878 */
    BrPairRingItem aItems[BR_PAIR_RING_SLOTS];     /* 0x118A9880 */
} BrPairRing;

/* Push one pair. Argument order follows the original: the value pushed LAST
 * by the caller (arg1) becomes .a and the first-pushed (arg2) becomes .b. */
void BrPairRingPush(BrPairRing *pRing, int a, int b);

/* 0x10075100  derive the frame counter from a millisecond timestamp. */
typedef struct BrTimeState {
    unsigned int ms;      /* 0x118AB118 -- the raw millisecond input */
    unsigned int f12C;    /* 0x118AB12C -- unconditionally zeroed here */
    unsigned int tick30;  /* 0x118AB134 */
} BrTimeState;

/* tick30 = (ms % 100) / 33 + 3 * (ms / 100).
 *
 * That is 30 ticks per second, but it is NOT floor(ms * 30 / 1000): inside
 * each 100 ms block the sub-ticks land at 0/33/66/99 ms, so ms=99 already
 * yields 3 where a true 30 Hz counter would yield 2. Non-decreasing, and
 * exactly 30 per second at every whole second. Reproduced as-is.
 *
 * All arithmetic is UNSIGNED in the original (div/mul, not idiv/imul). */
void BrTimeUpdate(BrTimeState *pState, unsigned int ms);

#endif /* SLICE1_09_H */
