/* br_saveprobe.c -- menus: "does this save slot already have a file?" probes
 * for the two record lists that own an in-place name edit.
 *
 *   0x1003B580  RallySeason<n>.brf   (list base 0x10AC5D24, page 0x10AC5D18)
 *   0x1003BCA0  TimeAttack<n>.grf    (list base 0x10AC5D28, page 0x10AC5D1C)
 *
 * Both are the "pick slot n" callbacks of a record list: they publish the
 * list base and the picked index, assemble the slot's save-file name in a
 * stack buffer and fopen it.  A file that opens means the slot is taken, so
 * the page's sub-object gets its +0x70 "confirm overwrite" word set; a slot
 * with no file goes straight into the rename toggle (0x1003AF60 /
 * 0x1003B970) so the player names it.  Either way they report 1.
 *
 * Shape notes, all read off the original bytes:
 *  - the frame is 0x108 = a 4-byte itoa scratch UNDER a 260-byte path, so
 *    the number may not exceed three digits; reproduced, not fixed.
 *  - the address of the record's label (+0x35 = the +0x2C sub-object's
 *    +0x09 label) is formed and null-tested but never read; the name is
 *    built from the literal, the index and the extension only.
 *  - the "no file" flag is ONE variable: zeroed as an initialiser (it is
 *    live across the early return in the RallySeason one), set to 1 in the
 *    fopen-failed arm, tested once after the probe.  strcpy/strcat are the
 *    /O2 intrinsics (repne scasb + rep movs); _itoa/fopen/fclose are /MD
 *    imports.
 *  - the four name pieces are extern arrays, not literals: VC5 folds a
 *    literal's length and emits unrolled movs, the original scans.
 *  - the tail is if/else + ONE return; the second epilogue in the bytes
 *    is VC5's return-duplication (see the note in the RallySeason body).
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern int DAT_10ac5d18;    /* page object owning the RallySeason list */
extern int DAT_10ac5d1c;    /* page object owning the TimeAttack list  */
extern int DAT_10ac5d24;    /* RallySeason record-list base            */
extern int DAT_10ac5d28;    /* TimeAttack record-list base             */
extern int DAT_100aab94;    /* the picked record index                 */

/* The four name pieces are extern arrays, not literals: the original SCANS
 * each with repne scasb, and VC5 folds a literal's length outright. */
extern char s_RallySeason_100acb00[];
extern char s_brf_100acaf8[];
extern char s_TimeAttack_100acb14[];
extern char s_grf_100acb0c[];
extern char DAT_100ac9c8[];        /* "r" */

extern int  BrExt_10041A00(int pPage);   /* 0x1003AF60 rename toggle, RallySeason */
extern int  BrExt_10042410(int pPage);   /* 0x1003B970 rename toggle, TimeAttack  */
extern void BrFn1003E070(void);          /* 0x10037710 refresh input edges        */

/* WHAT IT DOES: the RallySeason list's "pick slot n" callback.  Rejects a
 * negative index with 0; otherwise publishes the list and the index, builds
 * "RallySeason<n>.brf", refreshes the input edges, and either flags the
 * page's sub-object for an overwrite confirm (file exists) or starts the
 * in-place rename of the empty slot.  Reports 1. */
/* @implements 0x1003B580 glide BrSaveProbeRallySeason */
int BrSaveProbeRallySeason(int pList, int *pIdx)
{
    char  szNum[4];
    char  szPath[260];
    int   missing = 0;
    int   n = *pIdx;
    FILE *fp;

    if (n < 0)
        return 0;
    /* One shared `return 1` after the if/else, NOT a return in each arm: the
     * two epilogues in the bytes are VC5's return-duplication.  A written
     * mid-return is "semantically redundant" (VC5-IDIOMS) and pins all four
     * callee-saved pushes in the prologue; without it edi/esi/ebp sink past
     * this early-out exactly as the original has them (+3 pops otherwise). */
    DAT_10ac5d24 = pList;
    DAT_100aab94 = n;
    if ((char *)pList + n * 0x438 + 0x35 != NULL) {
        strcpy(szPath, s_RallySeason_100acb00);
        _itoa(n, szNum, 10);
        strcat(szPath, szNum);
        strcat(szPath, s_brf_100acaf8);
    }
    fp = fopen(szPath, DAT_100ac9c8);
    if (fp != NULL)
        fclose(fp);
    else
        missing = 1;
    BrFn1003E070();
    if (missing)
        BrExt_10041A00(DAT_10ac5d18);
    else
        *(int *)(*(int *)(DAT_10ac5d18 + 0x2ae8) + 0x70) = 1;
    return 1;
}

/* WHAT IT DOES: the TimeAttack list's "pick slot n" callback -- the same
 * probe as the RallySeason one without the negative-index guard or the
 * input refresh: publishes the list and the index, builds
 * "TimeAttack<n>.grf", and either flags the page's sub-object for an
 * overwrite confirm (file exists) or starts the in-place rename of the
 * empty slot.  Reports 1. */
/* @implements 0x1003BCA0 glide BrSaveProbeTimeAttack */
int BrSaveProbeTimeAttack(int pList, int *pIdx)
{
    char  szNum[4];
    char  szPath[260];
    int   missing = 0;
    int   n = *pIdx;
    FILE *fp;

    DAT_10ac5d28 = pList;
    DAT_100aab94 = n;
    if ((char *)pList + n * 0x438 + 0x35 != NULL) {
        strcpy(szPath, s_TimeAttack_100acb14);
        _itoa(n, szNum, 10);
        strcat(szPath, szNum);
        strcat(szPath, s_grf_100acb0c);
    }
    fp = fopen(szPath, DAT_100ac9c8);
    if (fp != NULL)
        fclose(fp);
    else
        missing = 1;
    if (missing)
        BrExt_10042410(DAT_10ac5d1c);
    else
        *(int *)(*(int *)(DAT_10ac5d1c + 0x2ae8) + 0x70) = 1;
    return 1;
}

#endif /* BR_MATCHING_BUILD */
