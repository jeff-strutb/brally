/* WHAT IT DOES: loads a season save (or, in mode 0, the current in-game
 * options handed over on an open file) into the staging buffer, then either
 * installs it on both players and reads the file's trailing option words and
 * display name (mode 4), or installs it while preserving the other player's
 * five standing words (any other mode).  Returns true on success; when the
 * file cannot be opened or fails its magic/checksum checks it returns whether
 * the second argument was non-zero. */
/* @implements 0x100695C0 glide BrSeasonLoad
 * @cpp_kind free
 * @cpp_symbol _BrSeasonLoad
 *
 * The C++ lane, for its bool: `return (char)arg ? true : false;` is the
 * original's `mov al,[arg] / test al,al / setne al` with no zeroing of eax
 * (C, and a C++ `!= 0`, zero eax first).  extern "C", so the C callers in
 * br_saveload.c link to it unchanged.  The open block's shape decides the
 * layout:
 *  - every failed read closes the file in its own block and jumps to one
 *    shared return; VC5 hoists the next read's `push ebx` (fp) above the
 *    branch where the two successors start alike, then merges the tails;
 *  - the block ends in an unconditional `goto install`, so VC5 lays it
 *    after the install path's epilogue and re-enters by a backward je;
 *  - the empty adler32 seed is computed before the outer call's arguments.
 */
/* @t4-pass 0x100695C0 1 2026-09-07 probes 61 bytes 871 insns 288 regions 3 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x100695C0 2 2026-09-07 probes 61 bytes 871 insns 288 regions 3 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x100695C0 3 2026-09-13 probes 61 bytes 879 insns 280 regions 2 rows 13 census yes  (tools/crank.py) */
/* @t4-pass 0x100695C0 4 2026-09-20 probes 12 bytes 879 insns 280 regions 2 rows 13 census yes  (failure-return respelling arg&0xff?1:0 scores worse 3+10->6+13; C `(char)arg!=0` stays) */
/* @t4-pass 0x100695C0 5 2026-09-20 probes 10 bytes 879 insns 280 regions 2 rows 13 census no   (baseline reconfirm; the two residues are C++-front-end block layout + bool-return, per header) */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

extern "C" {
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

bool BrSeasonLoad(int mode, int arg)
{
    FILE *fp;

    if (mode == 0) {
        fp = (FILE *)arg;
        memcpy(DAT_117a6188, DAT_10ac5a48, 0x53 * 4);
    } else {
        unsigned int sum;
        unsigned int crc;

        fp = fopen(DAT_117a6030, DAT_1007b0e0);
        if (fp == NULL)
            return (char)arg ? true : false;
        if (fread(DAT_117a6188, 1, 4, fp) != 4) {
            fclose(fp);
            goto fail2;
        }
        if (strncmp((char *)DAT_117a6188, DAT_100b559c, 4) != 0) {
            fclose(fp);
            goto fail2;
        }
        if (fread(&sum, 1, 4, fp) != 4) {
            fclose(fp);
            goto fail2;
        }
        if (fread(DAT_117a6188, 1, 0x200, fp) != 0x200) {
            fclose(fp);
            goto fail2;
        }
        crc = FUN_10001000(0, 0, 0);
        if (sum != FUN_10001000(crc, DAT_117a6188, 0x200)) {
            fclose(fp);
            goto fail2;
        }
        goto install;
    fail2:
        return (char)arg ? true : false;
    }
install:
    if (mode == 4) {
        long n;

        if (BrPairBufReset() == 0)
            return false;
        if (DAT_10af2094[0].pBlock == NULL || DAT_10af4bfc == NULL)
            return false;
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
    return true;
}
}
