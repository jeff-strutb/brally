/* slice2_12.h -- decompiled from BRD3D.dll, addresses 0x100053F0-0x10008AA0.
 *
 * This packet is the ENCODE half of the network car-state codec whose DECODE
 * half a later pass already published in slice1_02.h, plus the parts of the
 * 16-slot player table that a later pass did not cover, plus a POD archive WRITER
 * (the read side is br_pod.h).
 *
 * Nothing here re-defines a type: BrCarState, BrNetSlot, BrNetState, the
 * BrFixUnpack* family and the BrNetMutexLock/Unlock hooks all come from
 * slice1_02.h; BrBitStream comes from slice1_09.h.
 *
 * WHY THE PAIRING MATTERS. Every quantiser below turned out to be the exact
 * inverse of one of another module's dequantisers -- including the two with
 * NEGATIVE scales, which a later pass flagged as suspicious. 0x100065E0 multiplies
 * by +32768 and subtracts from 0.5 (so the sign flips) while its partner
 * 0x10007280 multiplies by -1/32768; the flip cancels. That mutual
 * confirmation is the main evidence that both readings are right, so do not
 * "fix" the sign in either file without changing the other.
 */
#ifndef SLICE2_12_H
#define SLICE2_12_H

#include <stdint.h>
#include <stdio.h>

#include "slice1_02.h"          /* BrCarState, BrNetSlot, BrNetState, BrFix*  */
#include "slice1_09.h"          /* BrBitStream                                */

/* =====================================================================
 * External dependencies owned by other slices
 * ===================================================================== */

/* XSLICE 0x10073E70 */
/* The write twin of 0x10073C90 (BrBitStreamReadBits): emit the low nBits of
 * `value`, MSB-first, without aligning first. __thiscall in the original, the
 * object first here as everywhere else in slice1_09.h.
 * INTEGRATION: a later pass owns 0x10073B60-0x10073F50; this one falls in that
 * range but is not declared there yet. Rename to another module's export if it
 * lands under another name. */
extern void BrBitStreamWriteBits(BrBitStream *pBs, int32_t value, int32_t nBits);

/* XSLICE 0x1006F310 */
/* Consumes three consecutive floats and returns one float. Called twice by
 * 0x100054A0, once on a car-state field triple at +0x68 and once on the
 * position triple at +0x10, and the difference of the two results is added to
 * the predicted position's third axis -- so it behaves like "sample something
 * at this point", but nothing in this packet establishes what. Named for its
 * address on purpose. */
extern float BrProbe1006F310(const float av3[3]);

/* XSLICE 0x10004A10 -- slice1_02.h already declares this one as
 * BrNetSlotGetF02C(pNet, slot); listed here only because 0x10005F40 calls it
 * while already holding the same slot mutex. */

/* XSLICE 0x10008B90 */
/* thiscall on the stream object embedded at +4 of the POD writer. It fills
 * the 64-byte name field of a directory entry from the caller's string; the
 * length check and the _strupr that follow it are in 0x10008A00, so this is
 * NOT the uppercase-and-pad routine at 0x100085F0 (BrPodCleanupName). Most
 * likely a path -> basename copy, but that is not established. */
extern void BrPodWriterMakeName(void *pStream, const char *pszSrc, char *pszDst);

/* =====================================================================
 * 1. Car-state field clamps  (0x100058D0, 0x10005900, 0x10005930)
 * ===================================================================== */

/* All three clamp one float IN PLACE and take its address, and all three use
 * the same asymmetric test pair: the LOW bound is applied with "not >= lo",
 * which also catches NaN, while the HIGH bound is applied with "> hi", which
 * does not. A NaN therefore comes out as the LOW bound, never the high one.
 *
 * Their ranges line up exactly with the field ranges a later pass recorded for
 * f00..f0C, f10/f14 and f18 in BrCarState, which is what identifies them. */

/* 0x100058D0  clamp to [-1, 1]      -- the four orientation components. */
void BrCarClampUnit(float *pv);
/* 0x10005900  clamp to [0, 2048]    -- the two unsigned position axes. */
void BrCarClampPosXY(float *pv);
/* 0x10005930  clamp to [-256, 256]  -- the signed third position axis. */
void BrCarClampPosZ(float *pv);

/* =====================================================================
 * 2. Quantisers -- float -> packed integer
 * ===================================================================== */

/* Same shape as another module's three: `clamp(floor(0.5 + scale*v))`, written by
 * the original as `0.5 - v*(-scale)`. Rounding is floor-of-(x+0.5), so halves
 * go UP, not away from zero. Every clamp here is on the INTEGER, after
 * __ftol, so the same latent sign-flip a later pass documented applies once the
 * scaled value leaves int32 range.
 *
 * The inverse of each is named in its comment. */

/* 0x100065A0  clamp(floor(0.5 - 128*v), -32, 31)      inverse of 0x10007250 */
int8_t BrFixPackS6Q7Neg(float v);
/* 0x100065E0  clamp(floor(0.5 - 32768*v), -32768, 32767)  inv. of 0x10007280 */
/* int16_t, not int32_t: the definition has always returned int16_t and the
 * clamp above is exactly int16_t's range, so this line was simply wrong. It
 * went unnoticed because NOTHING outside slice2_12.c calls this -- MSVC 5.0
 * accepted the mismatch, and clang rejects it, which is how it surfaced. */
int16_t BrFixPackS16Q15Neg(float v);
/* 0x10006620  clamp(floor(0.5 + v*256/361), 0, 255)   inverse of 0x100072A0.
 * The scale really is float(256/361) = 0.7091412544250488, the reciprocal of
 * another module's 1.41015625, so degrees round-trip through a byte. */
int32_t BrFixPackU8Angle(float v);
/* 0x10006660  clamp(floor(0.5 + (v-400)/120.63491821), 0, 63)  inv. 0x100072C0.
 * GOTCHA: the clamp is to 6 bits (0..63) even though the dequantiser masks a
 * full byte, so the pair is only symmetric over [400, 8000]. */
int8_t BrFixPackU8Range(float v);
/* 0x100066A0  4-level classifier, inverse of 0x100072E0:
 *     v < 128 -> 0,  v < 171 -> 1,  v < 213 -> 2,  else 3
 * The thresholds sit one above each of 0x100072E0's outputs (0, 170, 212),
 * which is what makes the round trip exact. NaN takes the first arm -> 0. */
int8_t BrFixPackLevel(float v);
/* 0x100067B0  clamp(floor(0.5 + 256*v), -32768, 32767)  inverse of 0x100073A0 */
int32_t BrFixPackS16Q8(float v);
/* 0x100067F0  clamp(floor(0.5 + 8*v), -128, 127)        inverse of 0x100073C0 */
int32_t BrFixPackS8Q3(float v);

/* =====================================================================
 * 3. Bitstream encoders  (0x100061A0 absolute, 0x10006830 delta)
 * ===================================================================== */

/* 0x100061A0  the encoder for 0x10006EC0 (BrCarStateDecode).
 *
 * Writes exactly 32 fields, in the order f00 f04 f08 f0C f10 f14 f18 f1C f20
 * f28 f2C f30 f34 f38 f3C f4C f50 f54 f58 f6C f70 f74 f78 f7C f80 f84 f88
 * f8C f90 f94 f98 f9C, for 8+8+8+8+17+17+15+16+16+5+5+5+4+4+4 +7*1 +24+6+2+2
 * +6*1 = 187 bits. f24, f40, f44, f48, f5C, f60, f64 and f68 are never sent,
 * which is the same set the decoder leaves alone.
 *
 * ARGUMENT ORDER: destination (the stream) first, exactly as in the original
 * -- arg1 is the bitstream and arg2 is the state. */
void BrCarStateEncode(BrBitStream *pBs, const BrCarState *pSrc);

/* 0x10006830  the encoder for 0x100073E0 (BrCarStateDecodeDelta).
 *
 * 17 fields: f00..f0C absolute, then f10, f14, f18 and f78 delta-coded against
 * pRef, then f7C, f80, f84 and the six booleans f88..f9C absolute.
 *
 * The delta code is a 2-bit prefix above the transmitted low bits:
 *     0  high part unchanged
 *     1  high part is exactly one step above the reference
 *     2  reference is BELOW the current value by more than one step
 *     3  reference is at or ABOVE the current value
 * Note 2 and 3 are lossy: the decoder applies +2 steps and -1 step, so a jump
 * of three or more steps does not survive. That asymmetry is in the original.
 *
 * ARGUMENT ORDER: (stream, current, reference). The reference is arg3, the
 * LAST argument, while its decoder counterpart takes the reference SECOND. */
void BrCarStateEncodeDelta(BrBitStream *pBs, const BrCarState *pCur,
                           const BrCarState *pRef);

/* =====================================================================
 * 4. The 22-byte fixed record  (0x10006BD0 pack, 0x10007730 unpack)
 * ===================================================================== */

/* A second, bitstream-free wire form: 22 bytes with fields hand-packed into
 * the spare low bits of the quantised values. Two of the bytes are stolen
 * from the top of an earlier 32-bit store (b[0x0B] from the dword at 0x08 and
 * b[0x0F] from the dword at 0x0C), so the write ORDER matters -- the byte
 * store must come after the dword store, as it does in the original.
 *
 * ENDIANNESS: the original stores the multi-byte fields with plain x86 `mov`,
 * so the record is LITTLE-endian. That is worth flagging because everything
 * else this game serialises (the .rca payload, every BrBitStream accessor) is
 * big-endian. Handled byte-wise here, so the port does not depend on the
 * host's order. */
#define BR_CARPACKED_SIZE 22

typedef struct BrCarPacked { uint8_t b[BR_CARPACKED_SIZE]; } BrCarPacked;

/* 0x10006BD0  state -> record. Destination first, as in the original. */
void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc);

/* 0x10007730  record -> state. Destination first, as in the original.
 *
 * Writes 25 fields and leaves f1C, f20, f24, f28, f2C, f30 and f78 untouched,
 * so this form is lossier than the bitstream one -- callers must seed pDst.
 *
 * GOTCHA: the booleans do NOT all decode to 1.0f. f6C, f88, f8C, f90, f94,
 * f98 and f9C decode to 128.0f when set, while f70 and f74 decode to 1.0f.
 * The encoder only ever tests them against zero, so both values round-trip;
 * the asymmetry is real and reachable. */
void BrCarStateUnpack(BrCarState *pDst, const BrCarPacked *pSrc);

/* =====================================================================
 * 5. Player table accessors
 * ===================================================================== */

/* 0x10005470  count records whose first dword is non-zero.
 *
 * Stride 0x2B68 -- the entity/car record stride. The original's base was the
 * global at 0x10ACEDB0 and its count the global at 0x100B36FC; both are
 * parameters here. A count <= 0 yields 0 (the do/while body is guarded). */
uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords);

/* 0x10005D40 (and 0x10005D90, which is the identical routine over a different
 * pair of globals): pop the top of an int32 stack.
 *
 * GOTCHA: the top element lives AT *piTop, not at *piTop - 1, and the empty
 * sentinel is a NEGATIVE index -- the original tests `jl`, so -1 means empty
 * and index 0 still holds one element. Returns -1 when empty.
 *
 * 0x10005D40 works on the array at 0x10220D90 with the index at 0x10220DD4
 * (BrNetState::f10220DD4, whose array a later pass did not model);
 * 0x10005D90 works on pNet->a10221288 with pNet->f10221318 -- the stack
 * BrNetDropMatching pushes onto. Both take the matching mutex first. */
int32_t BrNetStackPop(void *hMutex, int32_t *paStack, int32_t *piTop);

/* 0x10005E40  return pNet->a102212D0[i] under h1022AF28.
 * GOTCHA: no range check at all, and the index is signed. */
int32_t BrNetGetA102212D0(BrNetState *pNet, int32_t i);

/* 0x10005DE0  read four fields of one slot under that slot's mutex: the int32
 * at +0x030 is returned, and the three bytes at +0x034, +0x035 and +0x036 are
 * stored through the three out pointers.
 *
 * DEVIATION: a later pass models +0x034 as one int32 (BrNetSlot::f034), so the
 * three bytes are extracted from it as bits 0..7, 8..15 and 16..23 -- i.e.
 * the x86 memory order. On a big-endian host that is not the same layout.
 * These are the two fields BrNetReset deliberately does not clear. */
int32_t BrNetSlotGetF030(BrNetState *pNet, int32_t slot,
                         uint8_t *pb34, uint8_t *pb35, uint8_t *pb36);

/* 0x10005EE0  copy a NUL-terminated name into slot[i].f570 under that slot's
 * mutex. DEVIATION: the original is an inlined `rep movsd` strcpy with no
 * length limit; this truncates at BR_NET_SLOT_NAME - 1. */
void BrNetSlotSetName(BrNetState *pNet, int32_t slot, const char *pszName);

/* 0x10005F40  max(0, (slot[i].f02C & 0x3F) - 4).
 *
 * GOTCHA: it takes the slot mutex and then calls 0x10004A10, which takes the
 * SAME mutex again. That is only safe because Win32 mutexes are recursive --
 * a port that swaps in a plain non-recursive lock will deadlock here. */
int32_t BrNetSlotGetF02CBiased(BrNetState *pNet, int32_t slot);

/* 0x10005F90  slot[i].f974, floored at 0, under that slot's mutex. */
int32_t BrNetSlotGetF974(BrNetState *pNet, int32_t slot);

/* 0x10006090 / 0x100060C0  set / clear pNet->f10220DD0 under h1022131C. */
void BrNetSetF10220DD0(BrNetState *pNet);
void BrNetClearF10220DD0(BrNetState *pNet);

/* 0x10006160  if nowTicks >= pNet->f1022AF00, raise the flag.
 *
 * GOTCHA: the comparison is UNSIGNED (`jb`), and BrNetReset initialises
 * f1022AF00 to -1, so before something sets a real deadline the test reads as
 * "now >= 0xFFFFFFFF" and never fires. The -1 is a disarmed timer, not a past
 * one.
 *
 * The original read the clock from 0x10003460 (BrTicks30FromMs) and wrote the
 * global at 0x1022AF14, which a later pass did not model; both are parameters. */
void BrNetCheckDeadline(BrNetState *pNet, uint32_t nowTicks, int32_t *pfFlag);

/* =====================================================================
 * 6. Dead-reckoning  (0x100054A0)
 * ===================================================================== */

/* 0x100054A0  predict one remote car's state from the two newest samples held
 * in its slot. Returns 1 when pDst was produced, 0 when it was not.
 *
 * THE SLOT'S SAMPLE RING. This function is what shows that BrNetSlot's
 * +0x058..+0x557 -- which slice1_02.h records as "never touched in this
 * slice" -- is eight BrCarState records of 0xA0 bytes each, indexed in
 * parallel with f00C[] (a sequence number per sample) and f038[] (non-zero
 * when that sample is occupied).
 *
 * Behaviour:
 *   - a slot index equal to localSlot skips the prediction entirely but still
 *     falls into the shared tail, so pDst is clamped and normalised on the way
 *     out and the return is 1. It is NOT a no-op;
 *   - fewer than 2 samples (f558 < 2) sets pDst->f7C to 400.0f and returns 0.
 *     400.0f is the bottom of BrFixUnpackU8Range's [400, 8000] window, so the
 *     "no data" state is the minimum of that field, not zero;
 *   - otherwise it picks the two highest sequence numbers among the occupied
 *     samples and extrapolates:
 *         t = (min(now - seq[best], 6) + (seq[best] - seq[second])) / dt
 *     and calls BrCarStateLerp(pDst, t, second, best). t is >= 1 by
 *     construction, so this is extrapolation, not interpolation; the +6 cap
 *     is what bounds how far it may run ahead.
 *
 * GOTCHA: both argmax scans start with index 0 and a running maximum of 0,
 * and compare UNSIGNED. A slot whose only occupied sample has sequence 0 is
 * therefore indistinguishable from "nothing found", and index 0 is the
 * fallback answer in both scans -- so best and second can come out equal when
 * fewer than two samples carry a non-zero sequence, making dt zero.
 *
 * GOTCHA: when dt is zero and the chosen sample is the same one as last time,
 * the whole prediction is skipped and record[f560] is copied verbatim -- but
 * only on that one path. The other two paths divide by dt regardless, so a
 * zero dt there produces an infinity in t. Preserved.
 *
 * On every path that produces a state, the four orientation components are
 * clamped to [-1, 1] and then normalised (0x100741B0), EXCEPT that a sum of
 * exactly 0.0f replaces them with (1, 0, 0, 0); the position is then clamped
 * with BrCarClampPosXY / BrCarClampPosZ.
 *
 * GOTCHA: the "is it all zero" test sums f00 + f04 + f0C + f08 -- in that
 * order, third and fourth swapped. Preserved because float addition is not
 * associative.
 *
 * The original took only (pDst, slot); pNet, hGlobal, localSlot and nowTicks
 * were the globals 0x10221328, 0x1022AF34, 0x10094294 and 0x10003460. */
int BrNetSlotPredict(BrCarState *pDst, int32_t slot, BrNetState *pNet,
                     void *hGlobal, int32_t localSlot, uint32_t nowTicks);

/* =====================================================================
 * 7. 76-byte-key record cache  (0x10008670, 0x10008970)
 * ===================================================================== */

/* Records are 0x4C bytes; only the 16 dwords at +0x0C..+0x48 take part in the
 * comparison, so +0x00..+0x08 are payload the search ignores. */
typedef struct BrKeyCacheEntry {
    int32_t f00, f04, f08;      /* +0x00..+0x08  not compared */
    int32_t aKey[16];           /* +0x0C..+0x48  compared as a unit */
} BrKeyCacheEntry;

typedef struct BrKeyCache {
    void            *pVtbl;     /* +0x000 */
    int32_t          f004;
    int32_t          f008, f00C;
    int32_t          cEntries;  /* +0x010  the search bound */
    int32_t          f014;
    BrKeyCacheEntry *aEntries;  /* +0x018  released with operator delete */
    FILE            *pFile;     /* +0x01C  released with fclose */
    int32_t          a020[256]; /* +0x020..+0x41C */
    int32_t          f420;
} BrKeyCache;

/* 0x10008670  linear search for the entry whose 16-dword key matches; returns
 * the index, or -1 when cEntries is 0 or nothing matches.
 *
 * DEVIATION: the original builds the key by calling its own second virtual
 * method -- `(*this->vtbl[1])(this, arg, &key)` -- and takes the argument to
 * that method, not the key. Modelling the vtable would mean inventing an
 * interface this packet gives no name for, so the already-built key is passed
 * in instead. The search itself is transcribed unchanged. */
int32_t BrKeyCacheFind(const BrKeyCache *pCache, const int32_t aKey[16]);

/* 0x10008970  release both buffers and reset the object.
 *
 * GOTCHA: +0x1C is closed with fclose (0x1007CD50) while +0x18 goes to
 * operator delete (0x1007DE40) -- one is a FILE, the other is memory, and the
 * two are adjacent fields. Neither pointer is checked beyond != 0. The clear
 * starts at +0x008, so the vtable slot and +0x004 survive the reset. */
void BrKeyCacheReset(BrKeyCache *pCache);

/* =====================================================================
 * 8. POD archive writer  (0x100089C0, 0x10008A00, 0x10008AA0)
 * ===================================================================== */

/* The write side of br_pod.h, and it confirms that format independently: a
 * 16-byte header of "POD" + u32 0x1F4 + u32 count + u32 directory offset, then
 * the payloads, then a directory of 76-byte entries.
 *
 * NEW FACT for br_pod.h: the entry field at +0x08 that br_pod.h records as an
 * unknown u32 is really TWO BYTES, +0x08 and +0x09, taken from two separate
 * arguments of Add. +0x0A and +0x0B are never written at all.
 *
 * GOTCHA: the original builds the header in a 16-byte stack local and sets
 * only bytes 0..2 to 'P','O','D' -- the fourth byte of the magic is never
 * assigned, so it is whatever was left on the stack. Retail archives happen
 * to carry 0 there. This port writes 0 explicitly. */
/* 4096, NOT 1024. The old value read `0x13000` as a BYTE count; `rep stosd`
 * counts DWORDS.
 *
 *   100089DD  mov ecx, 0x13000
 *   100089E4  mov edi, 0x1022B358
 *   100089EC  rep stosd            ; 0x13000 DWORDS = 0x4C000 bytes
 *
 * 0x4C000 / 76 = 4096. The image layout confirms it independently:
 * 0x1022B358 + 0x4C000 = 0x10277358, which is exactly the cEntries global the
 * very next instruction writes. The buffer ends where the count begins, so no
 * other reading fits.
 *
 * This was not latent. BrPodWriteAdd carries a bound the ORIGINAL DOES NOT
 * HAVE, so the undersized constant turned into silent truncation of any
 * archive past 1024 members -- a sizing error converted into quiet data loss
 * by a defensive guard. Found by the equivalence audit; untestable by
 * construction, since nothing in this tree writes an archive that large. */
#define BR_POD_WRITER_MAX 4096      /* 0x13000 DWORDS / 76 bytes per entry */
#define BR_POD_WRITER_MAGIC_EXTRA 0x1F4

typedef struct BrPodWriteEntry {
    uint32_t offData;               /* +0x00 */
    uint32_t cbData;                /* +0x04 */
    uint8_t  b08, b09, b0A, b0B;    /* +0x08..+0x0B; only b08/b09 are written */
    char     szName[64];            /* +0x0C..+0x4B */
} BrPodWriteEntry;

typedef struct BrPodWriter {
    FILE            *pFile;
    uint32_t         cEntries;
    BrPodWriteEntry  aEntries[BR_POD_WRITER_MAX];
} BrPodWriter;

/* 0x100089C0  open and seek past the 16-byte header, then clear the whole
 * directory. Returns 0 on success.
 * DEVIATION: the original opens through a stream object embedded at +4 and
 * seeks with 0x1007C910; FILE * and fseek here. */
int  BrPodWriteOpen(BrPodWriter *pW, const char *pszPath);

/* 0x10008A00  append one member. Records ftell() as the offset, writes the
 * payload, and fills in the directory entry.
 *
 * GOTCHA: a name longer than 64 characters is REPORTED and then written
 * anyway -- the same report-and-continue habit br_pod.h notes on the read
 * side. The name is uppercased in place after it is copied. */
void BrPodWriteAdd(BrPodWriter *pW, const char *pszName,
                   const void *pvData, uint32_t cbData,
                   uint8_t b08, uint8_t b09);

/* 0x10008AA0  write the directory, rewind, write the header, close. */
void BrPodWriteClose(BrPodWriter *pW);

#endif /* SLICE2_12_H */
