/* 0x10069A80 -- settings: read the ".GRF" Time Attack ghost file back in.
 * Twin of the season reader 0x100695C0 (src/core/settings/br_seasonload.c)
 * and the mirror of the ghost writer 0x10069DE0 (br_ghostsave.c); the ".GRF"
 * format is in include/br_save.h.
 *
 * C++ TU because the failure return is `mov al,[arg] / test al,al / setne al`
 * with NO zeroing of eax -- a C++ `bool`, exactly the residue that parks the
 * season reader in C.  Here the whole function returns bool: success is
 * `mov al,1`, failure the bare `setne al`, whose upper bytes are the 0xff...
 * of the `or eax,-1` that seeds the six error-reset stores.
 *
 * Shape notes from the bytes:
 *  - arg 1 (the path) is dead after fopen, so VC5 reuses its incoming slot as
 *    the running byte counter: the frame is `sub esp,8` (len + checksum) and
 *    the cursor lives up in the argument space -- reproduced by writing the
 *    counter back into the `path` parameter (parameter-as-cursor idiom);
 *  - `len < 0xc` is a FILE-VERSION test read twice: an old ghost (len 0)
 *    carries the length and the two option dwords in the file and checksums
 *    them; a new one (len >= 0xc) takes the length from `len`, skips the two
 *    option reads and installs the derived option instead;
 *  - the replay buffer pointer BrReplayGetBuf2() is re-called for the read and
 *    again for the checksum (never cached), as the writer does;
 *  - the player block pointer at 0x10af2094 is reloaded between the two bit
 *    ORs into +0xf2/+0xf0;
 *  - the tail (six option dwords then the 0x80-byte name) is located from the
 *    end: seek END, ftell, seek back 0x98 for the dwords and 0x80 for the name.
 *
 * PARKED 2026-09-06 at 848/828 B, 405 diffs (/O2 /GX /MD, first @ +0x18).  The
 * body is INSTRUCTION-COMPLETE and instruction-for-instruction identical to the
 * original (verified in a full side-by-side): every read, check, adler step,
 * the install block, the six tail freads, the name copy and the reset block are
 * present and in the original's order.  The whole residue is ONE coupled
 * allocation/layout wall, register-blind AND block-order-normalised = 0:
 *  (1) BLOCK LAYOUT.  The original lays the fail/reset block as the fall-through
 *      after the checksum test (`cmp; je install`) and the install block LAST;
 *      VC5 instead inverts to `cmp; jne closefail`, pulls the larger install
 *      block up as the fall-through and sinks the many-predecessor closefail
 *      merge to the end.  That reorders the whole back half, so nearly every
 *      byte past +0x1b0 differs and the fail branches grow short->near (the +20
 *      bytes).  This is the same wall the season reader 0x100695C0 is parked on.
 *  (2) REGISTER TIE.  fp and the enregistered version constant 0xc contend for
 *      {ebx, ebp}: the original gives fp=ebx / 0xc=ebp, VC5 here gives the
 *      reverse, which alone flips ~40 push/cmp bytes.  Coupled to (1): the
 *      install-inline layout changes fp's live range past the checksum.
 *
 * DEAD, do not re-run (each measured with tools/cpp_score.py, /O2 /GX /MD):
 *   - checksum branch as `if (checksum == sum) goto install;` with install
 *     labelled last: VC5 inverts to `jne closefail`, install stays inline (405);
 *   - the faithful nested-if shape (success falls through, one closefail at the
 *     bottom, Ghidra's exact control flow): BYTE-IDENTICAL to the flat form
 *     (405) -- VC5 canonicalises the two;
 *   - the first `len >= 0xc` test polarity: `if (len < 0xc)` first gives the
 *     WRONG `jae` (533); `if (len >= 0xc)` first is the original's `jb` (405),
 *     and is kept;
 *   - local declaration order (all 24 of fp/len/checksum orders tried): inert,
 *     405 every time -- the tie is with a compiler-generated constant temp, not
 *     a declared local, so [[declaration-order-tiebreak]] has no handle here.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

extern "C" {
unsigned int FUN_10001000(unsigned int adler, const void *pv, unsigned int cb); /* 0x10001000 adler32 */
char        *BrReplayGetBuf2(void);          /* 0x10063DA0 */
unsigned int BrReplayCountFromBytes(unsigned int cb); /* 0x10063DB0 */

extern char  DAT_1007b0e0[];                 /* "rb"                       */
extern char  DAT_117a6188[];                 /* the staging buffer         */
extern char  DAT_100b55a4[];                 /* "RGho"                     */
extern int   DAT_10ac5c24;                   /* ghost option A             */
extern int   DAT_10ac5c20;                   /* ghost option B (float; reset as int) */
extern float DAT_10077bec;                   /* option B's scale           */
extern unsigned char DAT_105bc8e0[0x10];     /* the 0x10-byte ghost header */
extern int   DAT_105bc8e4, DAT_105bc8e8, DAT_105bc8ec;  /* its tail dwords */
extern int   DAT_105bc8d8;                   /* header byte count          */
extern char *DAT_10af2094;                   /* player 0's block pointer   */
extern int   DAT_10af3cd8, DAT_10af3cdc, DAT_10af3ce0, DAT_10af3ce4, DAT_10af3ce8, DAT_10af3cec;
extern char  DAT_10af3cf0[];                 /* the save's display name    */
extern char  DAT_10af6858[];                 /* its mirror                 */
}

/* WHAT IT DOES: loads a Time Attack ghost save named by `path` -- validates
 * the "RGho" magic, reads the length and (for an old-format file) the two
 * option dwords, the ghost header and the replay buffer, and checks the whole
 * lot against the file's adler32.  On success it installs the header size, the
 * replay frame count, the derived option B (new-format only), sets two flag
 * bits in the player block from the header, reads the trailing six option
 * dwords and the 0x80-byte display name (both located from the end of the
 * file), mirrors the name and returns true.  On any failure it resets the
 * option and header globals to their empty state and returns whether the
 * second argument's low byte was non-zero. */
/* @implements 0x10069A80 glide BrGhostLoad
 * @cpp_kind free
 * @cpp_symbol ?BrGhostLoad@@YA_NPADH@Z */
bool BrGhostLoad(char *path, int arg)
{
    FILE        *fp;
    unsigned int len;
    unsigned int checksum;

    fp = fopen(path, DAT_1007b0e0);
    if (fp == 0)
        goto fail;

    if (fread(DAT_117a6188, 1, 4, fp) != 4)
        goto closefail;
    if (strncmp(DAT_117a6188, DAT_100b55a4, 4) != 0)
        goto closefail;
    if (fread(&len, 4, 1, fp) != 1)
        goto closefail;

    if (len >= 0xc) {
        path = (char *)len;
    } else {
        if (len != 0 || fread(&path, 1, 4, fp) != 4)
            goto closefail;
    }
    path -= 4;
    if (fread(&checksum, 1, 4, fp) != 4)
        goto closefail;
    if (len < 0xc) {
        path -= 4;
        if (fread(&DAT_10ac5c24, 1, 4, fp) != 4)
            goto closefail;
        path -= 4;
        if (fread(&DAT_10ac5c20, 1, 4, fp) != 4)
            goto closefail;
    }
    path -= 0x10;
    if (fread(DAT_105bc8e0, 1, 0x10, fp) != 0x10)
        goto closefail;
    if (fread(BrReplayGetBuf2(), 1, (size_t)path, fp) != (size_t)path)
        goto closefail;
    {
        unsigned int sum = FUN_10001000(0, 0, 0);
        if (len < 0xc) {
            sum = FUN_10001000(sum, &DAT_10ac5c24, 4);
            sum = FUN_10001000(sum, &DAT_10ac5c20, 4);
        }
        sum = FUN_10001000(sum, DAT_105bc8e0, 0x10);
        sum = FUN_10001000(sum, BrReplayGetBuf2(), (size_t)path);
        if (checksum == sum)
            goto install;
    }
closefail:
    fclose(fp);
fail:
    DAT_10ac5c20 = 0;
    *(int *)DAT_105bc8e0 = -1;
    DAT_10ac5c24 = 0;
    DAT_105bc8e4 = -1;
    DAT_105bc8e8 = -1;
    DAT_105bc8ec = -1;
    return (char)arg != 0;
install:
    {
        long         n;
        unsigned int count;

        DAT_105bc8d8 = 0x10;
        count = BrReplayCountFromBytes((size_t)path);
        if (len >= 0xc) {
            DAT_10ac5c24 = 0;
            *(float *)&DAT_10ac5c20 = (float)((int)count - 0xcc) * DAT_10077bec;
        }
        *(unsigned short *)(DAT_10af2094 + 0xf2) |= (unsigned short)(1 << DAT_105bc8e0[0]);
        *(unsigned short *)(DAT_10af2094 + 0xf0) |= (unsigned short)(1 << DAT_105bc8e0[1]);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x98, 0);
        fread(&DAT_10af3cd8, 4, 1, fp);
        fread(&DAT_10af3cdc, 4, 1, fp);
        fread(&DAT_10af3ce0, 4, 1, fp);
        fread(&DAT_10af3ce4, 4, 1, fp);
        fread(&DAT_10af3ce8, 4, 1, fp);
        fread(&DAT_10af3cec, 4, 1, fp);
        fseek(fp, 0, 2);
        n = ftell(fp);
        fseek(fp, n - 0x80, 0);
        fread(DAT_10af3cf0, 1, 0x80, fp);
        memcpy(DAT_10af6858, DAT_10af3cf0, 0x80);
        fclose(fp);
        return true;
    }
}

#endif /* BR_MATCHING_BUILD */
