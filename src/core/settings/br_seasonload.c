/* br_seasonload.c -- settings: read the ".BRF" season save back in, or take
 * the in-game option block instead, and install it on both players.
 *
 *   0x100695C0  877 B   the season reader (d3d 0x10070610, declared by the
 *                       port as BrSub10070610(mode, arg)); format in
 *                       include/br_save.h.
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
 * PARKED 2026-09-05 at 879/877 B (multiset: 1 push imm vs push reg, one
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
#ifdef BR_MATCHING_BUILD

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

/* WHAT IT DOES: loads a season save (or, in mode 0, the current in-game
 * options handed over on an open file) into the staging buffer, then either
 * installs it on both players and reads the file's trailing option words and
 * display name (mode 4), or installs it while preserving the other player's
 * five standing words (any other mode).  Returns 1 on success; when the file
 * cannot be opened or fails its magic/checksum checks it returns whether the
 * second argument was non-zero. */
/* @implements 0x100695C0 glide BrSeasonLoad */
char BrSeasonLoad(int mode, int arg)
{
    FILE *fp;

    /* The open block is the ELSE arm and never falls through -- every path
     * in it returns or `goto install`s -- so VC5 defers it past the epilogue
     * and reaches the install path by a backward `je`, as the original does.
     * As a then-arm with the mode-0 code as fallthrough, or as `goto open` to
     * a label after the return, it is laid inline instead. */
    if (mode == 0) {
        fp = (FILE *)arg;
        memcpy(DAT_117a6188, DAT_10ac5a48, 0x53 * 4);
    } else {
        unsigned int sum;

        fp = fopen(DAT_117a6030, DAT_1007b0e0);
        if (fp == NULL)
            return (char)arg != 0;
        if (fread(DAT_117a6188, 1, 4, fp) == 4
            && strncmp((char *)DAT_117a6188, DAT_100b559c, 4) == 0
            && fread(&sum, 1, 4, fp) == 4
            && fread(DAT_117a6188, 1, 0x200, fp) == 0x200
            && sum == FUN_10001000(FUN_10001000(0, 0, 0), DAT_117a6188, 0x200))
            goto install;
        fclose(fp);
        return (char)arg != 0;
    }
install:
    if (mode == 4) {
        long n;

        if (BrPairBufReset() == 0)
            return 0;
        if (DAT_10af2094[0].pBlock == NULL || DAT_10af4bfc == NULL)
            return 0;
        memcpy(DAT_10af2094[0].pBlock, DAT_117a6188, 0x53 * 4);
        memcpy(DAT_10af4bfc, DAT_117a6188, 0x53 * 4);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x94, 0);
        fread(&DAT_10af3cd8, 4, 1, fp);
        fread(&DAT_10af3cdc, 4, 1, fp);
        fread(&DAT_10af3ce0, 4, 1, fp);
        fread(&DAT_10af3ce4, 4, 1, fp);
        fread(&DAT_10af3ce8, 4, 1, fp);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x80, 0);
        fread(DAT_10af3cf0, 1, 0x80, fp);
        memcpy(DAT_10af6858, DAT_10af3cf0, 0x80);
    } else {
        int  save[5];
        int *p;

        p = DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock;
        save[0] = p[0x3e];
        save[1] = p[0x3f];
        save[2] = p[0x40];
        save[3] = p[0x41];
        save[4] = p[0x42];
        memcpy(DAT_10af2094[0].pBlock, DAT_117a6188, 0x53 * 4);
        memcpy(DAT_10af4bfc, DAT_117a6188, 0x53 * 4);
        DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock[0x3e] = save[0];
        DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock[0x3f] = save[1];
        DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock[0x40] = save[2];
        DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock[0x41] = save[3];
        DAT_10af2094[DAT_105ccbc4 ^ 1].pBlock[0x42] = save[4];
    }
    if (mode != 0)
        fclose(fp);
    return 1;
}

#endif /* BR_MATCHING_BUILD */
