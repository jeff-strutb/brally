/* br_seasonload.c -- settings: read the ".BRF" season save back in, or take
 * the in-game option block instead, and install it on both players.
 *
 *   0x100695C0  877 B   the season reader (d3d 0x10070610, declared by the
 *                       port as BrSub10070610(mode, arg)); format in
 *                       src/brally/include/br_save.h.
 *
 * Two entry shapes share one install half:
 *   mode 0    the second argument is an OPEN FILE*; the staging buffer is
 *             filled from the in-game option block, nothing is read yet;
 *   mode != 0 the season path is opened, and magic + checksum + payload are
 *             read into the staging buffer; any failure closes the file and
 *             reports whether the second argument was non-zero.
 * Then mode 4 (a fresh season) resets the pair buffers, refuses if either
 * player block is missing, copies the staging buffer onto both players and
 * reads the tail of the file -- five option dwords 0x94 from the end and the
 * 0x80-byte display name 0x80 from the end -- into the option globals and
 * the name (also copied to the 0x10AF6858 mirror).  Any other mode keeps the
 * OTHER player's five standing words across the copy.  Modes other than 0
 * close the file.  Reports 1.
 *
 * Shape notes from the bytes:
 *  - the player blocks are two 0x2B68-byte objects at 0x10AF2094 whose first
 *    member is the payload pointer; `(flag ^ 1)` picks the other player and
 *    the original re-indexes it for EVERY one of the five stores (the two
 *    memcpys kill the pointer, so no local survives);
 *  - the checksum read in the open block and the standing-word copies in the
 *    install block are block-scoped and share frame slots;
 *  - fread's import address is loaded once, in the prologue, for both paths.
 *
 * (Matched 2026-10-03 in BrSeasonLoad_100695C0.cpp.)  PARKED 2026-09-05 at 879/877 B (multiset: 1 push imm vs push reg, one
 * epilogue merged, +1 xor).  Two residues, both pointing at the C++ front
 * end: (1) BLOCK LAYOUT -- the original lays the open block AFTER the
 * install path's epilogue and enters install by a backward `je`; every C
 * spelling lays it inline (plain if/else, open block as a never-falling
 * then-arm, trailing `goto install`, `goto open` to a label after the
 * return -- the last two also flip the guard to `je`).  (2) The failure
 * return is `mov al,[arg] / test al,al / setne al` with NO zeroing of eax --
 * a C++ `bool` return; C's `(char)arg != 0` and `? 1 : 0` both zero eax
 * first.  A bool-returning .cpp of the same body scores WORSE (653 diffs,
 * three layouts), so the C++ lane needs its own read of this one; the C
 * body here is instruction-complete and stays as the reference. */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

extern unsigned int FUN_10001000(unsigned int adler, const void *pv, unsigned int cb); /* 0x10001000 adler32 */
extern int BrPairBufReset(void);             /* 0x10037870 */

struct BrPlayerState {
    int  *pBlock;                            /* +0x000 the 0x53-dword option block */
    char  rest[0x2B68 - 4];
};
extern struct BrPlayerState DAT_10af2094[2];  /* 0x10AF2094 / 0x10AF4BFC */
extern int *DAT_10af4bfc;                     /* == DAT_10af2094[1].pBlock */
extern int  DAT_105ccbc4;                     /* which player is current   */
extern int  DAT_10ac5a48[];                   /* the in-game option block  */
extern int  DAT_117a6188[];                   /* the 0x200-byte staging buffer */
extern char DAT_117a6030[];                   /* the season save path      */
extern char DAT_1007b0e0[];                   /* "rb"                      */
extern char DAT_100b559c[];                   /* "RSea"                    */
extern int  DAT_10af3cd8, DAT_10af3cdc, DAT_10af3ce0, DAT_10af3ce4, DAT_10af3ce8;
extern char DAT_10af3cf0[];                   /* the save's display name   */
extern char DAT_10af6858[];                   /* its mirror                */

/* BrSeasonLoad (0x100695C0) is matched in the C++ lane, for its bool return:
 * src/brally/core/settings/BrSeasonLoad_100695C0.cpp.  The declarations above stay
 * for the Mac port spec, which supplies its own body. */

