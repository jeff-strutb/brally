/* br_obj.h -- CORRECTED: these are NOT small object accessors.
 *
 * ERRATUM (found by cross-checking against 0x10073B60-0x10073F20): every
 * function in this header is a member of ONE class, a big-endian BIT STREAM.
 * The original reading here -- three unrelated "object accessor" helpers --
 * was wrong. Correct meanings:
 *
 *   BrObjInitInline   the payload begins at +0x14 because it is a bit stream
 *                     whose buffer is inline, NOT a small-buffer optimisation
 *   BrObjFlagCount    {flag, count} is really {readBit, readByte}
 *   BrObjConsumeFlag  ALIGNS THE READ CURSOR to a byte boundary; it is not a
 *                     "consume a flag and count it" operation
 *   BrObjClear        resets that stream's cursors
 *
 * The behaviour transcribed below is still correct -- the code does what the
 * original does -- but the NAMES mislead. See slice1_09.h for the full stream
 * API (read u8/u16BE/u24BE/s32BE/n-bits, write, at-end) which supersedes this
 * file. These should be merged into that class; kept for now only because
 * other modules already include this header.
 *
 * Original heading follows.
 *
 * br_obj.h -- small object accessors, decompiled from BRD3D.dll.
 *
 * Thiscall helpers on a common object header. Field names are positional;
 * their meaning is not established, so they are numbered by offset rather
 * than given speculative names.
 *
 *   0x10073B80  clear the first four dwords (+0x00..+0x0C)
 *   0x10073F50  return the dword at +0x10
 */
#ifndef BR_OBJ_H
#define BR_OBJ_H

#include "br_match.h"   /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

typedef struct BrObjHeader {
    int f00, f04, f08, f0C;
    int f10;
} BrObjHeader;

/* 0x10073B80  zero +0x00..+0x0C. Note +0x10 is deliberately NOT cleared.
 * thiscall in the original: `this` arrives in ecx and nothing is pushed. */
void BR_THISCALL1 BrObjClear(BrObjHeader *pObj);

/* 0x10073F50  return the field at +0x10. */
int  BrObjGetF10(const BrObjHeader *pObj);

/* 0x10073B40  init: zero +0x00..+0x0C, then point +0x10 at the INLINE buffer
 * that begins at +0x14. This is a small-buffer optimisation -- the object
 * starts out referring to storage inside itself, so the pointer must be
 * re-pointed on any copy or move, and must never be freed while it still
 * aims at +0x14. */
typedef struct BrObjInline {
    int   f00, f04, f08, f0C;
    void *pBuf;         /* +0x10 -- initially &inline[0] */
    char  inline_[1];   /* +0x14 */
} BrObjInline;
void BrObjInitInline(BrObjInline *pObj);

/* 0x10073D20  if +0x00 is set, clear it and increment +0x04. */
typedef struct BrObjFlagCount { int flag, count; } BrObjFlagCount;
void BrObjConsumeFlag(BrObjFlagCount *pObj);

#endif /* BR_OBJ_H */
