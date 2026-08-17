/* slice2_20.h -- Boss Rally .rca / track loader and big-endian fixup pass,
 * decompiled from BRD3D.dll 0x100370D0-0x10039020.
 *
 * WHAT THIS RANGE IS
 * ==================
 * Boss Rally's PC build loads the N64 asset files verbatim and then walks
 * them, byte-swapping every field in place and rebasing every embedded
 * pointer.  Everything here is that walk.  Two file families go through it:
 *
 *   .rca  car file.  0x100370D0 fixes it up.  The N64 struct it contains is
 *         at FILE OFFSET 0x8000 and its N64 address is 0x803C8000 -- that
 *         pair is what BrSegSetBases() is handed, and both retail cars in
 *         testdata/ have 0x803C.... words at 0x8010 onwards, which confirms
 *         it.  The first 0x8000 bytes are the "RCar" header plus name.
 *
 *   track header.  0x10038510 reads exactly 0x230 bytes and swaps them;
 *         0x10037E10 then walks everything they point at.
 *
 * TWO SWAP IDIOMS, ONE RESULT
 * ---------------------------
 * The original contains two different code shapes for "make this dword
 * host-order":
 *
 *   (a) four byte exchanges in place            -> BrSwap4()
 *   (b) assemble b0<<24|b1<<16|b2<<8|b3 in a
 *       register and store it back              -> BrRead32BE() + store
 *
 * They are numerically identical.  The distinction is still worth keeping
 * because only the (a) sites are ever followed by a BrSegFixup() -- i.e. (a)
 * marks a pointer field and (b) marks a scalar.  It is not a perfect rule
 * (plenty of (a) fields are never fixed up) but the converse holds: no (b)
 * field is ever fixed up.
 *
 * THAT CONVERSE IS A UNIVERSAL OVER EVERY SITE IN THE MODULE, AND IT WAS
 * SETTLED BY DECODING ALL OF THEM RATHER THAN BY SAMPLING.
 *
 * The two idioms are separable in the machine code.  (b) is a `shl reg,8`
 * twice with two `or`s and one `mov [field], reg`; (a) is two pairs of byte
 * moves swapping [f+0]<->[f+3] and [f+1]<->[f+2].  Counting `shl r32,8` over
 * the whole D3D range 0x100370D0..0x10039030 gives 30 instructions, i.e.
 * exactly 15 (b) sites, and there are 34 calls to BrSegFixup (0x1002B970).
 * Six functions hold all of them:
 *
 *      0x100370D0   3 (b)    7 fixups
 *      0x10038010   0        1
 *      0x10038250   0        1
 *      0x100382A0   0        4
 *      0x10038510   9       19
 *      0x10038B20   3        2
 *
 * BRGlide gives the identical table -- 0x10030770, 0x100316D0, 0x10031910,
 * 0x10031960, 0x10031B80, 0x10032190 with 3/7, 0/1, 0/1, 0/4, 9/19, 3/2 --
 * so the reference build agrees function for function.
 *
 * Resolving each fixup's argument back to the field it names (through the
 * stack slots in 0x10038510, and tracking the esp delta, because the
 * `mov reg,[esp+D]` sits BEFORE the `add esp,4` and naming the slot without
 * that correction shifts every answer by one -- CONVENTIONS.md, "a stack
 * displacement means nothing without the ESP it is relative to"):
 *
 *   0x100370D0  (b) at +0x8000 +0x8008 +0x8010
 *               fixed +0x8004 +0x800C +0x8014 +0x8018 +0x8094 +0x80BC +0x811C
 *   0x10038510  (b) at +0x00 +0x04 +0x08 +0x10 +0x18 +0x64 +0x7C +0x88 +0x160
 *               fixed +0x0C +0x1C +0x20 +0x24 +0x50 +0x54 +0x58 +0x5C +0x60
 *                     +0x68 +0x6C +0x70 +0x74 +0x78 +0x84 +0x8C +0x90 +0x94
 *
 * INTERSECTION EMPTY in every function, and the header's fields interleave
 * tightly enough that this is a real test rather than a lucky one: +0x64 is
 * (b) between two fixed neighbours +0x60 and +0x68, and +0x88 is (b) between
 * +0x84 and +0x8C.
 *
 * THE ONE APPARENT COUNTEREXAMPLE, AND WHY IT IS NOT ONE.  In 0x10038B20 the
 * expression [esi-5] is both a (b) store target (0x10038C6D) and a fixup
 * argument (0x10038BCE and 0x10038C2C).  Those are three arms of ONE SWITCH
 * on the record's kind byte [esi+3] (jump table at 0x10038C90, records of 12
 * bytes, `add esi,0xc`).  The arms at 0x10038BB3 and 0x10038C11 treat the
 * record's first dword as a pointer -- idiom (a), then fix it up; the arm at
 * 0x10038C51 treats the same four bytes as a scalar -- idiom (b), no fixup.
 * They are mutually exclusive, so no execution ever does both.
 *
 * The refinement worth carrying: the rule is per-SITE, not per-OFFSET.  Which
 * idiom a field gets is a property of the record's KIND, and reading "offset
 * X uses (b), so offset X is a scalar" across a switch would be wrong here.
 *
 * THE 32-BIT POINTER PROBLEM
 * --------------------------
 * BrSegFixup (br_seg.h) rewrites an embedded N64 address into a host address
 * *as a uint32_t, in place, inside the file image*.  On a 32-bit build that
 * value is directly dereferenceable.  On a 64-bit host it cannot be.  The
 * port therefore installs a 32-bit surrogate host base (BR_LOAD_BASE32) and
 * keeps the matching real host pointer next to it in BrLoadEnv; every
 * dereference goes through BrLoadResolve(), which also bounds-checks.  See
 * the DEVIATION notes in the .c file.
 *
 * Argument orders below are the original's, including where they disagree
 * with each other.
 */
#ifndef SLICE2_20_H
#define SLICE2_20_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "br_vec.h"

/* --------------------------------------------------------------------------
 * Endian helpers.  These are the two idioms described above, factored out.
 * -------------------------------------------------------------------------- */

/* Reverse exactly four bytes in place. */
void     BrSwap4(void *pv);
/* Reverse exactly two bytes in place. */
void     BrSwap2(void *pv);
/* Read four bytes big-endian.  Does not modify the source. */
uint32_t BrRead32BE(const void *pv);

/* --------------------------------------------------------------------------
 * The load environment.
 *
 * This one struct stands in for four DLL globals.  It exists only because a
 * 64-bit port cannot store host pointers in the u32 fields of the file image;
 * the original needed nothing but the two segment bases.
 * -------------------------------------------------------------------------- */

/* The surrogate 32-bit "host base" handed to BrSegSetBases.  Any value works
 * as long as it is above every N64 base in use and leaves room for the file;
 * fixed-up fields are only ever compared against, and resolved through, this
 * same constant. */
#define BR_LOAD_BASE32 0x40000000u

typedef struct BrLoadEnv {
    uint8_t *pImage;    /* host address that uBase32 stands for            */
    uint32_t uBase32;   /* the value BrSegSetBases was given as host base  */
    size_t   cbImage;   /* bytes valid at pImage; bounds-checks resolution */

    /* Was the global at 0x10690BEC: base for the texture/TLUT byte offsets
     * that 0x10038450 copies from.  The original computed it as
     * 0x106C8E78 + trackHeader[0]. */
    uint8_t *pTexBase;
    size_t   cbTexBase;

    /* Was the global at 0x106C7C64: an array parallel to the 0x24-stride
     * texture record table, same stride, read at +0x20 for the CI4/CI8 bit. */
    uint8_t *pTexFlags;
} BrLoadEnv;

extern BrLoadEnv g_BrLoad;

/* Map an already-fixed-up field value to a host pointer.  Returns NULL for 0
 * and for anything outside the image -- which is the same thing the original
 * hands its callers, because BrSegFixup zeroes unresolvable values. */
void *BrLoadResolve(uint32_t uFixedUp);

/* --------------------------------------------------------------------------
 * .rca (car)
 * -------------------------------------------------------------------------- */

/* 0x100370D0  Fix up a loaded .rca image in place.
 *
 * cbFile is a PORT ADDITION (the original knew no size); it only feeds the
 * bounds check in BrLoadResolve.
 *
 * Installs segment bases n64=0x803C8000 host=&file[0x8000], then walks the
 * N64 struct at file+0x8000:
 *
 *   +0x000 u32   scalar          (0x13B in ce.rca, 0x166-adjacent counts)
 *   +0x004 ptr
 *   +0x008 u32   scalar
 *   +0x00C ptr
 *   +0x010 u32   scalar          (0x20 = entry count of the +0x14 table)
 *   +0x014 ptr   -> table of [+0x010] records, stride 0x24
 *   +0x018 ptr   x30, each registered as a display list if not already
 *   +0x090 u32   byte-swapped but NOT rebased -- see the gotcha below
 *   +0x094 ptr   rebased but NOT byte-swapped -- same gotcha
 *   +0x098 u32   x6
 *   +0x0B0 u32   x3
 *   +0x0BC ptr   x9, each registered as a display list if not already
 *   +0x0E0 u32   x12 (three per group, four groups; swapped in place)
 *   +0x11A u8    index into the +0x014 table (render-mode patch)
 *   +0x11B u8    index into the +0x014 table (surface creation)
 *   +0x11C ptr
 *
 * GOTCHA: the +0x090 / +0x094 pair really is crossed in the original -- the
 * byte swap targets +0x090 and the BrSegFixup targets +0x094.  Every other
 * pointer site in the whole function swaps and rebases the SAME dword.  Both
 * retail cars have a live 0x803C.... pointer at +0x090 and zero at +0x094, so
 * in practice the pointer at +0x090 is left holding a raw N64 address and the
 * rebase is a no-op.  Reproduced as-is.
 */
void BrRcaFixup(void *pvFile, size_t cbFile);

/* 0x10037740  Build "cars/<name>.rca", read it, check the "RCar" magic and
 * run BrRcaFixup on it.  iCar indexes the filename table at 0x100B84F8.
 *
 * GOTCHA: pvDest is compared for IDENTITY against the static scratch buffer
 * at 0x100C12A0.  Loading into that buffer means "this is the preview/menu
 * car" and takes a different branch (arg 1 rather than 0 to 0x10061010, and
 * the 0x100B8C90 flag is left alone rather than saved-set-restored).
 *
 * DEVIATION: cbDest is a port addition -- the original takes two arguments.
 * It only feeds BrRcaFixup's bounds check. */
void BrRcaLoadCar(void *pvDest, size_t cbDest, int iCar);

/* The track handling-file extension, and it is a BUILD DIVERGENCE: Glide
 * 0x1003117B loads 0x100AA338 == ".hnt", D3D 0x10037AC9 loads 0x100AABA8 ==
 * ".hnd", and neither literal appears in the other image.  The disc ships
 * `desert.hnt` and `coast.hnt` and no `.hnd` at all, so Glide -- this
 * project's reference -- is also the build that agrees with the data.
 * Named rather than inlined so a test can assert on the same token the
 * implementation uses without the two drifting apart. */
#define BR_TRACK_HANDLING_EXT  ".hnt"

/* 0x10031140 (Glide) / 0x10037A90 (D3D)  Build "tracks/<name>", replace the
 * extension with BR_TRACK_HANDLING_EXT and hand it to 0x10037990 (Glide
 * 0x10031030).  iTrack indexes the table at 0x100B80B8 (Glide 0x100B78C0).
 *
 * NOTE the Glide number: 0x10031140 names THIS function in BRGlide.dll and
 * BrMat4Translate in BRD3D.dll.  A bare address is not self-describing here
 * (manifest.py defect 6), which is why the claim carries its build. */
void BrTrackLoadHandling(int iTrack);

/* 0x100378B0  Read a whole file into pvDest.
 *
 * cbMax < 0 means "the whole file" (the original queries the size and uses
 * that).  cbMax >= 0 is used as the byte count directly, with NO clamp to the
 * actual file size and no clamp against the destination -- callers pass 0x20
 * for palettes and -1 for everything else.
 *
 * A missing file is reported and then read from anyway. */
void BrFileReadInto(void *pvDest, const char *pszPath, int cbMax);

/* --------------------------------------------------------------------------
 * track
 * -------------------------------------------------------------------------- */

/* 0x10038510  Read the 0x230-byte track header into pvHdr and byte-swap it.
 *
 * Layout recovered from which swap idiom is used and which fields are then
 * rebased by the tail of the same function:
 *
 *   scalars (idiom b): +0x00 +0x04 +0x08 +0x10 +0x18 +0x64 +0x7C +0x88 +0x160
 *   pointers (swapped and rebased):
 *       +0x0C +0x14 +0x1C +0x20 +0x24 +0x50 +0x54 +0x58 +0x5C +0x60
 *       +0x68 +0x6C +0x70 +0x74 +0x78 +0x84 +0x8C +0x90 +0x94
 *   swapped, never rebased: +0x28..+0x4C, +0x98..+0x15F (ten 0x14-byte rows)
 *
 * GOTCHA: +0x80 is skipped entirely -- the swaps run +0x7C then +0x84.  It is
 * the only hole in +0x00..+0x164.
 *
 * The nineteen rebases all happen in one run at the very end, after every
 * swap, in ascending field order.
 *
 * +0x164..+0x223 is sixteen 12-byte command records and +0x224 is their
 * count; those are NOT touched here, they are BrTrackFixupCmds's job. */
void BrTrackHdrRead(void *pvHdr, FILE **ppFile);

/* 0x10037E10  Walk everything the swapped header points at. */
void BrTrackFixup(void *pvHdr);

/* 0x10037FA0  hdr[+0x08] = 1 + max(u16 array at [+0x20], indices 1 .. n-1),
 * where n is the u16 stored at [+0x24] + 0x2000.
 *
 * GOTCHA: index 0 is deliberately skipped and the running maximum starts at
 * 0, so the result is always >= 1 even for an all-zero table. */
void BrTrackF08FromMax(void *pvHdr);

/* 0x10037FE0  For each of hdr[+0x64] records of stride 0x54 at hdr[+0x60],
 * run BrTrackFixupRec54. */
void BrTrackFixupList60(void *pvHdr);

/* 0x10038010  Fix up one 0x54-byte record:
 *   +0x00..+0x47  eighteen dwords, byte-swapped
 *   +0x44         the only one rebased; points at a display list
 *   +0x48..+0x53  six u16s, byte-swapped
 * then registers the display list and hands it to the backend. */
void BrTrackFixupRec54(void *pvRec);

/* 0x10038250  For each of hdr[+0x7C] dwords at hdr[+0x78]: swap, rebase, and
 * run BrTrackFixupNode on the target. */
void BrTrackFixupList78(void *pvHdr);

/* 0x100382A0  Fix up one geometry node:
 *   +0x00 +0x04 +0x08 +0x0C  pointers, swapped and rebased
 *   +0x14 +0x16              u16s, byte-swapped; +0x14 is a count
 *   +0x18                    one 0x28-byte record
 *   +0x40 + i*0x28           count+1 more, i = 0 .. count INCLUSIVE
 *
 * GOTCHA: the loop bound is `<=`, so it processes count+1 records after the
 * one at +0x18 -- count+2 in total.
 *
 * GOTCHA: the guard in front of that loop is `cmp word[+0x14], di / jb` with
 * di == 0, i.e. an unsigned "< 0" test that can never be taken.  The loop
 * always runs at least once even when the count is zero. */
void BrTrackFixupNode(void *pvNode);

/* 0x10038380  Byte-swap one 0x28-byte record: three Vec3s at +0x00, +0x0C,
 * +0x18 and a single dword at +0x24. */
void BrSwapRec28(void *pvRec);

/* 0x10038410  Byte-swap hdr[+0x88] Vec3s at hdr[+0x84]. */
void BrTrackFixupList84(void *pvHdr);

/* 0x10038450  Copy texture data and TLUTs out of the image for the entries of
 * a 0x24-stride record table that ask for it.  Called as
 * (hdr[+0x1C], hdr[+0x18]) -- table first, count second, same as the swap
 * helpers at 0x1002BA00-0x1002BA80.
 *
 * Per record, all of these must hold or the record is skipped:
 *   rec[+0x00] != 0, rec[+0x20] & 0x100000, desc = rec[+0x08],
 *   u16 desc[+0x02] == 2, s32 desc[+0x08] == -1, (rec[+0x20] & 0x3FFFF) != 0
 * then (rec[+0x20] & 0x3FFFF) bytes are copied from pTexBase + desc[+0x0C]
 * to rec[+0x00], and if rec[+0x04] != 0 a palette of 0x20 or 0x200 bytes is
 * copied from pTexBase + desc[+0x10] to rec[+0x04].
 *
 * GOTCHA: the palette size is chosen from a DIFFERENT array -- the parallel
 * table at 0x106C7C64, same index, same stride, field +0x20.  0x20 bytes
 * (16 entries, CI4) when that field's nibble at 0xF000000 is exactly
 * 0x1000000, otherwise 0x200 (256 entries, CI8).  testdata/skytexdesert.lut4
 * is 32 bytes, which is the CI4 case. */
void BrTexCopyRecords(void *pvTable, int cRecords);

/* 0x10038B20  Walk the hdr[+0x224] command records at hdr[+0x164], stride
 * 0x0C, dispatching on the tag byte at record +0x08.
 *
 * The jump table is at 0x10038C90 and reads:
 *   tag 0,1,2 -> swap the dwords at +0x00 and +0x04
 *   tag 3,6,7 -> swap the dword at +0x00 only
 *   tag 4     -> swap+rebase +0x00, read +0x04 big-endian as a Vec3 COUNT,
 *                then byte-swap that many Vec3s at the target
 *   tag 5     -> swap+rebase +0x00, then byte-swap Vec3s at the target using
 *                the count LEFT OVER FROM THE LAST tag-4 RECORD
 *   tag > 7   -> ignored
 *
 * GOTCHA: that carry-over is real and is the whole point of tag 5 -- the
 * count lives in a register (ebp) that survives across loop iterations and
 * tag 5 never loads one of its own.  A tag-5 record before any tag-4 record
 * uses ebp = 0 and does nothing. */
void BrTrackFixupCmds(void *pvHdr);

/* --------------------------------------------------------------------------
 * misc
 * -------------------------------------------------------------------------- */

/* 0x10039000  Zero the 0x118-byte block at 0x10220B20, set its first dword to
 * 8, then call 0x10035BD1. */
void BrInit220B20(void);

/* 0x10039020  thiscall.  Accumulates a timer on the caller's object and, when
 * it crosses 0.25, pops one node off a free list and initialises it.
 *
 * The pool is three DLL globals: a u16 free-list head at 0x10A99BA8, a u16
 * live-list head at 0x10A99BB0, and a 0x20-stride array at 0x10A99BB8.  Index
 * 0 is the null sentinel -- a free head of 0 means "pool empty" and the
 * function returns without spawning.
 *
 * The node fields are named positionally; "particle" is inferred only from
 * the shape (position, a velocity built from the object's motion plus 20% of
 * two other vectors, a size, and a byte forced to 0xFF next to a byte holding
 * a 0..255 quantity derived from a reciprocal distance).
 */
typedef struct BrPoolNode {
    BrVec3   v00;    /* +0x00  (0x10A99BB8) */
    BrVec3   v0C;    /* +0x0C  (0x10A99BC4) */
    float    f18;    /* +0x18  (0x10A99BD0) */
    uint16_t uNext;  /* +0x1C  (0x10A99BD4) intrusive list link */
    uint8_t  b1E;    /* +0x1E  (0x10A99BD6) */
    uint8_t  b1F;    /* +0x1F  (0x10A99BD7) always set to 0xFF */
} BrPoolNode;

void BrPoolEmit(void *pvThis);

#endif /* SLICE2_20_H */
