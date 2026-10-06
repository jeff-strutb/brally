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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdio.h>
#include <string.h>

/* FUN_10001000: prototype in br_funcs.h */
/* BrPairBufReset: prototype in br_funcs.h */

/* BrPlayerState: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10AF2094 / 0x10AF4BFC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                     /* == DAT_10af2094[1].pBlock */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                     /* which player is current   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* the in-game option block  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* the 0x200-byte staging buffer */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* the season save path      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* "rb"                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* "RSea"                    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* the save's display name   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                   /* its mirror                */

/* WHAT IT DOES: loads a season save (or, in mode 0, the current in-game
 * options handed over on an open file) into the staging buffer, then either
 * installs it on both players and reads the file's trailing option words and
 * display name (mode 4), or installs it while preserving the other player's
 * five standing words (any other mode).  Returns 1 on success; when the file
 * cannot be opened or fails its magic/checksum checks it returns whether the
 * second argument was non-zero. */
/* @t4-pass 0x100695C0 1 2026-09-07 probes 61 bytes 871 insns 288 regions 3 rows 5 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100695C0 2 2026-09-07 probes 61 bytes 871 insns 288 regions 3 rows 5 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100695C0 3 2026-09-13 probes 61 bytes 879 insns 280 regions 2 rows 13 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100695C0 4 2026-09-20 probes 12 bytes 879 insns 280 regions 2 rows 13 census yes  (failure-return respelling arg&0xff?1:0 scores worse 3+10->6+13; C `(char)arg!=0` stays) */
/* @t4-pass 0x100695C0 5 2026-09-20 probes 10 bytes 879 insns 280 regions 2 rows 13 census no   (baseline reconfirm; the two residues are C++-front-end block layout + bool-return, per header) */
/* @t3 0x100695C0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 879/877 insns 280/287 rows 10+3 regions 2 oracle EQUIVALENT
 * @t3-effort passes 5 zero-movement 4 5
 * Residue is C++-front-end codegen only: the original lays the open block
 * after the install epilogue and enters by a backward je (every C spelling
 * lays it inline), and the failure path is a C++ bool return with no eax
 * zeroing where C zeros eax first (a .cpp of this body scores worse).  A5
 * oracle EQUIVALENT is the completeness proof (rule 12).  Do not reopen
 * before the end-grind. */
/* @implements 0x100695C0 glide BrSeasonLoad */
char BrSeasonLoad(int mode, intptr_t arg)
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
            && strncmp((char *)DAT_117a6188, (*(char (*)[])&DAT_100b51e4[952]), 4) == 0
            && fread(&sum, 1, 4, fp) == 4
            && fread(DAT_117a6188, 1, 0x200, fp) == 0x200
            && sum == BrAdler32(BrAdler32(0, 0, 0), DAT_117a6188, 0x200))
            goto install;
        fclose(fp);
        return (char)arg != 0;
    }
install:
    if (mode == 4) {
        long n;

        if (BrPairBufReset() == 0)
            return 0;
        if ((*(int * *)&g_aBrRaceCar[0].pEquip) == NULL || (*(int * *)&g_aBrRaceCar[1].pEquip) == NULL)
            return 0;
        memcpy((*(int * *)&g_aBrRaceCar[0].pEquip), DAT_117a6188, 0x53 * 4);
        memcpy((*(int * *)&g_aBrRaceCar[1].pEquip), DAT_117a6188, 0x53 * 4);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x94, 0);
        fread(&(*(int *)&g_aBrRaceCar[0].sz2ABC[20]), 4, 1, fp);
        fread(&(*(int *)&g_aBrRaceCar[0].sz2ABC[24]), 4, 1, fp);
        fread(&(*(int *)&g_aBrRaceCar[0].sz2ABC[28]), 4, 1, fp);
        fread(&(*(int *)&g_aBrRaceCar[0].sz2ABC[32]), 4, 1, fp);
        fread(&(*(int *)&g_aBrRaceCar[0].sz2ABC[36]), 4, 1, fp);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x80, 0);
        fread((*(char (*)[])&g_aBrRaceCar[0].sz2ABC[44]), 1, 0x80, fp);
        memcpy((*(char (*)[])&g_aBrRaceCar[1].sz2ABC[44]), (*(char (*)[])&g_aBrRaceCar[0].sz2ABC[44]), 0x80);
    } else {
        int  save[5];
        int *p;

        p = (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip);
        save[0] = p[0x3e];
        save[1] = p[0x3f];
        save[2] = p[0x40];
        save[3] = p[0x41];
        save[4] = p[0x42];
        memcpy((*(int * *)&g_aBrRaceCar[0].pEquip), DAT_117a6188, 0x53 * 4);
        memcpy((*(int * *)&g_aBrRaceCar[1].pEquip), DAT_117a6188, 0x53 * 4);
        (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip)[0x3e] = save[0];
        (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip)[0x3f] = save[1];
        (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip)[0x40] = save[2];
        (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip)[0x41] = save[3];
        (*(int * *)&g_aBrRaceCar[((*(int *)&DAT_105ccb68[23]) ^ 1)].pEquip)[0x42] = save[4];
    }
    if (mode != 0)
        fclose(fp);
    return 1;
}

