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
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

extern unsigned int FUN_10001000(unsigned int adler, const void *pv, unsigned int cb); /* 0x10001000 adler32 */
extern int   BrReplayGetSize(void);          /* 0x10063B50 */
extern void *BrReplayGetBuf(void);           /* 0x10063B40 */

extern char DAT_117a5f28[];                  /* the ghost save path        */
extern char DAT_1007b600[];                  /* "wb"                       */
extern char DAT_100b55a4[];                  /* "RGho"                     */
extern int  DAT_10077be4;                    /* the zero dword after it    */
extern int  DAT_10ac5c24;                    /* ghost option A             */
extern int  DAT_10ac5c20;                    /* ghost option B             */
extern char DAT_105bc8e0[];                  /* the 0x10-byte ghost header */
extern int  DAT_105bc8d8;                    /* bytes of header to write   */
extern int  DAT_10ac5d60, DAT_100abdec, DAT_100abdf0, DAT_100abdf4, DAT_100abdfc, DAT_100abdf8;
extern char DAT_10af3cf0[];                  /* the save's display name    */

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

    sum = FUN_10001000(0, 0, 0);
    sum = FUN_10001000(sum, &DAT_10ac5c24, 4);
    sum = FUN_10001000(sum, &DAT_10ac5c20, 4);
    sum = FUN_10001000(sum, DAT_105bc8e0, 0x10);
    sum = FUN_10001000(sum, BrReplayGetBuf(), BrReplayGetSize());
    cb  = BrReplayGetSize() + 0xc + DAT_105bc8d8;
    fp  = fopen(DAT_117a5f28, DAT_1007b600);
    if (fp == 0) {
        return 0;
    }
    if (fwrite(DAT_100b55a4, 1, 4, fp) != 4) {
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
    if (fwrite(&DAT_10ac5c24, 4, 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fwrite(&DAT_10ac5c20, 4, 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fwrite(DAT_105bc8e0, 1, DAT_105bc8d8, fp) != (unsigned int)DAT_105bc8d8) {
        fclose(fp);
        return 0;
    }
    if (fwrite(BrReplayGetBuf(), 1, BrReplayGetSize(), fp) != (unsigned int)BrReplayGetSize()) {
        fclose(fp);
        return 0;
    }
    fwrite(&DAT_10ac5d60, 4, 1, fp);
    fwrite(&DAT_100abdec, 4, 1, fp);
    fwrite(&DAT_100abdf0, 4, 1, fp);
    fwrite(&DAT_100abdf4, 4, 1, fp);
    fwrite(&DAT_100abdfc, 4, 1, fp);
    fwrite(&DAT_100abdf8, 4, 1, fp);
    if (fwrite(DAT_10af3cf0, 1, 0x80, fp) != 0x80) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

#endif /* BR_MATCHING_BUILD */
