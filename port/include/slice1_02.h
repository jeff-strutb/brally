/* slice1_02.h -- decompiled from BRD3D.dll, addresses 0x100049C0-0x100079E0.
 *
 * Three unrelated clusters happen to share this address range:
 *
 *   1. A family of scalar fixed-point <-> float codecs (0x100066E0-0x100073C0).
 *      Every one is a leaf: sign-extend or mask a bitfield, `fild`, multiply by
 *      a power of two. The scale constants were read out of .rdata, not guessed.
 *
 *   2. A network car-state packet codec built on top of those (0x10006EC0
 *      absolute, 0x100073E0 delta, 0x100079E0 interpolate).
 *
 *   3. A 16-slot mutex-guarded player table (0x100049C0-0x10005FE0).
 *
 * NAMING NOTE: none of the field meanings in BrCarState or BrNetSlot could be
 * established from the code alone, so every field is named by its byte offset
 * in the original. Do not rename one without evidence.
 */
#ifndef SLICE1_02_H
#define SLICE1_02_H

#include <stddef.h>
#include <stdint.h>

/* =====================================================================
 * External dependencies owned by other slices
 * ===================================================================== */

/* 0x10073C90 -- MSB-first bit reader, thiscall, owned by agent 09's slice.
 *
 * Layout recovered from the original for reference only; the type stays
 * opaque here so agent 09's definition is the single source of truth:
 *      +0x00  bit position inside the current byte (0..7)
 *      +0x04  index of the current byte
 *      +0x10  pointer to the byte buffer
 *
 * Semantics: consumes nBits from the top of the current byte downwards and
 * returns them right-aligned. nBits == 0 returns 0 and consumes nothing.
 *
 * INTEGRATION: rename this declaration to whatever agent 09 exports. */
typedef struct BrBitReader BrBitReader;
uint32_t BrBitReaderRead(BrBitReader *pReader, unsigned nBits);

/* 0x10003530 -- owned by agent 01's slice. Takes the formatted message that
 * 0x10005FE0 builds; its exact effect (chat line? HUD banner?) is not
 * established here. INTEGRATION: rename to agent 01's export. */
void BrNetAnnounce(const char *pszText);

/* DEVIATION: the original calls KERNEL32 WaitForSingleObject(h, INFINITE) and
 * ReleaseMutex(h) inline at every site. Ported behind these two hooks so the
 * module builds and can be tested without Win32. Supply real implementations
 * (or no-ops for a single-threaded build) when linking. */
void BrNetMutexLock(void *hMutex);
void BrNetMutexUnlock(void *hMutex);

/* =====================================================================
 * 1. Fixed-point codecs
 * ===================================================================== */

/* --- float -> packed integer ---------------------------------------------
 *
 * All three are `clamp(floor(0.5 + scale * v))`, i.e. round-half-UP, not
 * round-half-away-from-zero: -0.5 rounds to 0, +0.5 rounds to 1. The original
 * writes it as `0.5 - v * -scale` (an `fsubr` against a negated constant),
 * which is the same value. Same shape as BrPackNormalByte in br_vecd.h.
 *
 * GOTCHA: the clamps are applied at DIFFERENT points. 0x100066E0 clamps the
 * FLOAT before the int conversion; the other two clamp the INT afterwards.
 * Since __ftol keeps only the LOW DWORD of its 64-bit result, the sign of the
 * two integer-clamped routines flips underneath the clamp once the scaled
 * value leaves int32 range: BrFixPackS16Q7(1e9f) saturates to -32768, not
 * +32767. Their safe domains are |v| < 2^31/128 = 16777216 and
 * |v| < 2^31/2 = 1073741824 respectively; real inputs are nowhere near, so
 * this is latent in the original rather than reachable. */

/* 0x100066E0  clamp(floor(0.5 + 8192*v), 0, 0xFFFFFF)   -- unsigned Q13 in 24 bits */
int32_t BrFixPackU24Q13(float v);
/* 0x10006730  clamp(floor(0.5 + 2*v), -0x800000, 0x7FFFFF) -- signed Q1 in 24 bits */
int32_t BrFixPackS24Q1(float v);
/* 0x10006770  clamp(floor(0.5 + 128*v), -32768, 32767)  -- signed Q7 in 16 bits */
int32_t BrFixPackS16Q7(float v);

/* --- packed integer -> float ---------------------------------------------
 *
 * Each takes the raw 32-bit word the caller pushed; the field width is applied
 * inside. Two of them carry a NEGATIVE scale -- that is not a transcription
 * slip, the constants at 0x1008F11C and 0x1008F120 really are negative, so the
 * encodings are sign-flipped relative to every other member of the family. */

/* 0x10007250  sign-extend 6 bits (bit 5 is the sign) then * -1/128.
 * GOTCHA: negative scale. 0x20 (= -32) decodes to +0.25f. */
float BrFixUnpackS6Q7Neg(int32_t v);
/* 0x10007280  sign-extend 16 bits then * -1/32768.
 * GOTCHA: negative scale. 0x8000 decodes to +1.0f.
 * Callers in this slice feed it (byte << 8), i.e. an s8 in the high half. */
float BrFixUnpackS16Q15Neg(int32_t v);
/* 0x100072A0  (v & 0xFF) * 1.41015625  -- degrees; 361/256 per unit */
float BrFixUnpackU8Angle(int32_t v);
/* 0x100072C0  400 + (v & 0xFF) * 120.63491821289062.
 * With the 6-bit field its callers use, the range is exactly [400, 8000]. */
float BrFixUnpackU8Range(int32_t v);
/* 0x100072E0  4-level table on the LOW BYTE: 0 -> 0, 1 -> 170, 2 -> 212,
 * anything else -> 255. Note the default arm, not a masked lookup. */
float BrFixUnpackLevel(int32_t v);
/* 0x10007310  v / 8192, with v read as UNSIGNED 32-bit.
 * GOTCHA: the original widens through `fild qword` with a zero high dword, so
 * a word with bit 31 set decodes to a large positive value, not a negative. */
float BrFixUnpackU32Q13(uint32_t v);
/* 0x10007340  sign-extend 24 bits (bit 23 is the sign) then * 0.5 */
float BrFixUnpackS24Q1(uint32_t v);
/* 0x10007380  sign-extend 16 bits then * 1/128 */
float BrFixUnpackS16Q7(int32_t v);
/* 0x100073A0  sign-extend 16 bits then * 1/256 */
float BrFixUnpackS16Q8(int32_t v);
/* 0x100073C0  sign-extend 8 bits then * 0.125 */
float BrFixUnpackS8Q3(int32_t v);

/* =====================================================================
 * 2. Car-state packet
 * ===================================================================== */

/* 0xA0 bytes, forty consecutive floats. The interpolator at 0x100079E0 walks
 * +0x10..+0x9C with a 4-byte stride, which is what fixes both the element type
 * and the size. Fields are named by offset because nothing in this slice
 * establishes their meaning; what IS visible:
 *
 *   f00..f0C   four s8-coded values, |x| <= 1 -- almost certainly a quaternion,
 *              since 0x100079E0 negates exactly these four when f00 of the two
 *              endpoints straddle zero (the double-cover fix).
 *   f10,f14    unsigned Q13, range [0, 2048)   -- two position axes
 *   f18        signed Q7,  range [-256, 256)   -- the third axis
 *   f24        always written as 0 by the decoder, never read
 *   f3C == f40 raw angle in degrees; f44 == f48 the same angle + 35 mod 360
 *   f5C..f68   never touched by anything in this slice
 *   f9C        copied verbatim (not interpolated) by 0x100079E0
 */
typedef struct BrCarState {
    float f00, f04, f08, f0C;
    float f10, f14, f18, f1C;
    float f20, f24, f28, f2C;
    float f30, f34, f38, f3C;
    float f40, f44, f48, f4C;
    float f50, f54, f58, f5C;
    float f60, f64, f68, f6C;
    float f70, f74, f78, f7C;
    float f80, f84, f88, f8C;
    float f90, f94, f98, f9C;
} BrCarState;

#define BR_CARSTATE_FLOATS 40

/* 0x10006EC0  decode a full car state from the bitstream.
 * Reads 32 fields in a fixed order; leaves f5C, f60, f64 and f68 untouched and
 * stores a literal 0 into f24 without consuming any bits. */
void BrCarStateDecode(BrCarState *pDst, BrBitReader *pReader);

/* 0x100073E0  decode a DELTA car state against a reference.
 *
 * Only f00..f0C, f10, f14, f18, f78, f7C, f80, f84 and f88..f9C are written;
 * every other field of pDst is left alone, so callers must seed pDst from the
 * reference first if they want a complete state.
 *
 * f10, f14, f18 and f78 are delta-coded: the reference value is re-quantised
 * with the matching BrFixPack* routine, its low bits are replaced by the
 * transmitted ones, and a 2-bit code adjusts the high part by
 * {0, +1 step, +2 steps, -1 step}. See BrCarStateDeltaMerge in the .c. */
void BrCarStateDecodeDelta(BrCarState *pDst, const BrCarState *pRef,
                           BrBitReader *pReader);

/* 0x100079E0  pDst = lerp(pA, pB, t) over all forty floats.
 *
 * ARGUMENT ORDER: destination, then the SCALAR, then the two endpoints. t sits
 * between the output and the inputs, which is unlike anything in br_vec.h.
 *
 * t is clamped to [0, 10] -- extrapolation up to 10x is deliberately allowed
 * (this is dead-reckoning between network packets). NaN clamps to 0.
 *
 * If f00 of the two endpoints have opposite signs AND differ by at least 1.0,
 * the first four components are interpolated toward -pB instead of +pB.
 *
 * f9C is finally overwritten with pB->f9C, discarding its interpolated value.
 *
 * Aliasing: pDst may alias pA or pB (each element is read before it is
 * written), which the original relies on. */
void BrCarStateLerp(BrCarState *pDst, float t,
                    const BrCarState *pA, const BrCarState *pB);

/* =====================================================================
 * 3. Player slot table
 * ===================================================================== */

#define BR_NET_SLOTS      16       /* 0x9780 / 0x978 */
#define BR_NET_SLOT_NAME  0x404    /* +0x570 .. +0x973 */

/* One 0x978-byte record from the array at 0x10221328.
 *
 * PORTABILITY: hMutex is a pointer, so on a 64-bit host this struct is larger
 * than the original's 0x978 and the offsets below no longer hold. The project
 * targets portable source rather than a byte-identical layout; the offsets are
 * kept in the names so the mapping back to the binary stays checkable. */
typedef struct BrNetSlot {
    void    *hMutex;          /* +0x000  guards this record */
    int32_t  f004;            /* +0x004  matched against the argument of 0x10005FE0 */
    int32_t  f008;            /* +0x008 */
    int32_t  f00C[8];         /* +0x00C..+0x028 */
    int32_t  f02C;            /* +0x02C  low 6 bits are flags (see 0x10005FE0) */
    int32_t  f030, f034;      /* +0x030,+0x034 -- NOT cleared by BrNetReset */
    int32_t  f038[8];         /* +0x038..+0x054 */
    uint8_t  f058[0x558-0x58];/* +0x058..+0x557 -- never touched in this slice */
    int32_t  f558, f55C;
    int32_t  f560;            /* +0x560  reset to -1, not 0 */
    int32_t  f564, f568, f56C;
    char     f570[BR_NET_SLOT_NAME];  /* +0x570  NUL-terminated player name */
    int32_t  f974;
} BrNetSlot;

/* Everything 0x10005960 touches. In the original these are loose globals
 * scattered from 0x10220CEC to 0x106909D8, so they are named by their original
 * address rather than grouped into anything meaningful. Each `h...` is a
 * mutex handle guarding the fields listed under it. */
typedef struct BrNetState {
    BrNetSlot aSlots[BR_NET_SLOTS];   /* 0x10221328 */

    void    *h1022AF24;               /* guards f1022AEF8, f1022AF08, f10220E80 */
    int32_t  f1022AEF8;               /* reset to -1 */
    int32_t  f1022AF08;
    uint8_t  f10220E80;

    void    *h1022AF28;               /* guards a102212D0 */
    int32_t  a102212D0[16];

    void    *h1022AF2C;               /* guards f10220DD4 */
    int32_t  f10220DD4;               /* reset to -1 */

    void    *h1022AF30;               /* guards f10221318 + a10221288 */
    int32_t  f10221318;               /* top index of a10221288; -1 when empty */
    int32_t  a10221288[16];

    void    *h10221324;               /* guards f1022AAA8 */
    int32_t  f1022AAA8;

    void    *h1022AF04;               /* guards f1022AAF4 */
    int32_t  f1022AAF4;

    void    *h10220DDC;               /* guards f10221314 */
    int32_t  f10221314;

    void    *h1022131C;               /* guards f10220DD0 */
    int32_t  f10220DD0;

    void    *h10220CEC;               /* guards f1022AF00 */
    int32_t  f1022AF00;               /* reset to -1 */

    /* reset without taking any lock */
    int32_t  f10220DD8;
    int32_t  f1022AF3C;               /* reset to -1 */
    int32_t  a1022AAB0[16];           /* 0x1022AAB0..0x1022AAEF */
    int32_t  f1022AF20;
    int32_t  f106909D8;

    char     aNameScratch[BR_NET_SLOT_NAME];  /* 0x1022AAF8 */
} BrNetState;

/* 0x10005960  reset every slot and every global above. Always returns 1.
 *
 * GOTCHA: f004, f030 and f034 of each slot are deliberately NOT cleared -- the
 * original's two `rep stosd` runs skip over them. Four fields reset to -1
 * rather than 0 (slot f560, f1022AEF8, f10220DD4, f10221318, f1022AF00,
 * f1022AF3C); -1 is the "empty" sentinel throughout this subsystem, same
 * convention as BR_SLOT_EMPTY in br_slots.h. */
int BrNetReset(BrNetState *pNet);

/* 0x10004A10 / 0x10004A50  get/set slot[i].f02C under the slot's mutex.
 * GOTCHA: the setter's value is the SECOND argument even though the original
 * reads it from a stack slot that looks like the first. */
int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot);
void    BrNetSlotSetF02C(BrNetState *pNet, int32_t slot, int32_t value);

/* 0x10005CF0  get slot[i].f004 under the slot's mutex. */
int32_t BrNetSlotGetF004(BrNetState *pNet, int32_t slot);

/* 0x10005E70  copy slot[i].f570 into the SHARED scratch buffer and return it.
 * GOTCHA: not reentrant and not per-slot -- every call clobbers the string the
 * previous call returned. */
char *BrNetSlotName(BrNetState *pNet, int32_t slot);

/* 0x10005FE0  for every slot whose f004 equals `key` and whose f02C has any of
 * its low 6 bits set: push the slot index onto a10221288, zero its f02C, and
 * announce "%15<name> left the game." through BrNetAnnounce.
 *
 * The literal in .rdata is "%%15%s left the game.", so the rendered text keeps
 * a single leading '%' -- presumably a colour escape for the text renderer. */
void BrNetDropMatching(BrNetState *pNet, int32_t key);

/* =====================================================================
 * 4. Palette fetch
 * ===================================================================== */

/* 0x100049C0  copy one 3-byte record out of a table of 3-byte records.
 *
 * In the original both the table (0x100B37D0) and the destination
 * (0x10AD0854) are fixed globals and the index is the global at 0x10094294;
 * they are parameters here. The table really is RGB triples -- entry 5 is
 * 255,255,255 and entry 7 is 0,0,0.
 *
 * GOTCHA: the index global is initialised to -1 in the image, and the original
 * does no range check, so calling this before something sets the index reads
 * three bytes from in front of the table. Preserved: the index is signed. */
void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3]);

#endif /* SLICE1_02_H */
