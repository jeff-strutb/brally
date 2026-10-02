/* br_ghostsave.c -- settings: write the ".GRF" Time Attack ghost file.
 *
 *   0x10069DE0  649 B   the ghost writer (twin of the season writer
 *                       0x10069930 in src/core/generated/, format in
 *                       include/br_save.h: magic "RGho", a zero dword, the
 *                       payload length, the adler32, two option dwords, the
 *                       0x10-byte ghost header, the replay buffer, six loose
 *                       option dwords and the 0x80-byte display name).
 *
 * Every write after the header is checked against its byte count except the
 * six option dwords, exactly as the season writer leaves them unchecked; any
 * short write closes the file and reports failure.  The checksum is
 * adler32 seeded the way the original asks for it -- `adler32(0, NULL, 0)`
 * -- then run over the two option dwords, the 0x10-byte ghost header and
 * the replay buffer, in that order.
 *
 * Shape notes from the bytes:
 *  - the checksum and the length are the two frame dwords, the checksum
 *    below the length;
 *  - the replay write's byte count and its check are BOTH fresh calls to
 *    BrReplayGetSize (the original never caches it), and the argument list
 *    is evaluated right to left, size before buffer;
 *  - fwrite is called through one register (its import address is loaded
 *    once, after fopen succeeds).
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdio.h>

/* FUN_10001000: prototype in br_funcs.h */
/* BrReplayGetSize: prototype in br_funcs.h */
/* BrReplayGetBuf: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* the ghost save path        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* "wb"                       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* "RGho"                     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* the zero dword after it    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* ghost option A             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* ghost option B             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* the 0x10-byte ghost header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                    /* bytes of header to write   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* the save's display name    */

/* WHAT IT DOES: writes the Time Attack ghost file named by the ghost path:
 * magic, a zero dword, the total payload length, an adler32 over the two
 * option dwords + ghost header + replay buffer, then those pieces, six race
 * option dwords and the 0x80-byte display name.  Reports 1 on success and 0
 * -- closing the file -- as soon as the file cannot be opened or a checked
 * write comes up short. */
/* @implements 0x10069DE0 glide BrGhostSave */
char BrGhostSave(void)
{
    unsigned int sum;
    int          cb;
    FILE        *fp;

    sum = BrAdler32(0, 0, 0);
    sum = BrAdler32(sum, &(*(int *)&g_brTime5C24), 4);
    sum = BrAdler32(sum, &(*(int *)&g_brTime5C20), 4);
    sum = BrAdler32(sum, (*(char (*)[])&g_aBrRaceBeginRec), 0x10);
    sum = BrAdler32(sum, BrReplayGetBuf(), BrReplayGetSize());
    cb  = BrReplayGetSize() + 0xc + (*(int *)&g_brRace5BC8D8);
    fp  = fopen(DAT_117a5f28, DAT_1007b600);
    if (fp == 0) {
        return 0;
    }
    if (fwrite((*(char (*)[])&DAT_100b51e4[960]), 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&DAT_10077be4, 4, 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&cb, 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&sum, 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&(*(int *)&g_brTime5C24), 4, 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&(*(int *)&g_brTime5C20), 4, 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fwrite((*(char (*)[])&g_aBrRaceBeginRec), 1, (*(int *)&g_brRace5BC8D8), fp) != (unsigned int)(*(int *)&g_brRace5BC8D8)) {
        fclose(fp);
        return 0;
    }
    if (fwrite(BrReplayGetBuf(), 1, BrReplayGetSize(), fp) != (unsigned int)BrReplayGetSize()) {
        fclose(fp);
        return 0;
    }
    fwrite(&(*(int *)&DAT_10ac5d60), 4, 1, fp);
    fwrite(&(*(int *)&DAT_100abdec), 4, 1, fp);
    fwrite(&(*(int *)&DAT_100abdf0), 4, 1, fp);
    fwrite(&g_brSel0ABDF4, 4, 1, fp);
    fwrite(&(*(int *)&g_i0AC65C), 4, 1, fp);
    fwrite(&DAT_100abdf8, 4, 1, fp);
    if (fwrite((*(char (*)[])((char *)&g_aBrRaceCar + 0x2AE8)) /* BR_LP64_BYTE_VIEW */, 1, 0x80, fp) != 0x80) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

