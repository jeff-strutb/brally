/* br_savename.c -- menus: finish (or cancel) the in-place rename of a save
 * slot the player has just typed a name into.
 *
 *   0x1003B350  RallySeason<n>.brf   (d3d twin 0x10041DF0)
 *   0x1003BAC0  TimeAttack<n>.grf    (d3d twin 0x10042560)
 *
 * The third member of the probe (br_saveprobe.c) / begin (br_savebegin.c)
 * trio for the two record lists.  Called with the edit's result code: -1
 * means the edit was cancelled, so the name that was set aside in the
 * 0x10AC4100 buffer goes back onto the record; anything else commits --
 * the slot's file name is assembled and recorded as the save path, the
 * typed name is copied into the descriptor's 0x104-byte entry for the slot
 * and into the save header's name field, the save is written, and the
 * list's "dirty" latch is set.  Both report 1; the RallySeason one refuses
 * with 0 while no season is active.
 *
 * Shape notes, read off the original bytes:
 *  - the record index is the GLOBAL 0x100AAB94, read at each use: VC5 keeps
 *    one load across the strcpy intrinsics but reloads after every call, so
 *    spelling a local would collapse reads the original keeps;
 *  - the cancelled arm tests the ADDRESS of the set-aside buffer against
 *    zero (`mov eax,offset / test / je`), as br_uinameedit.c also notes --
 *    reproduced, not corrected;
 *  - the record's label is at +0x35 of record n (the +0x2C sub-object's
 *    +0x09 label) and that address is null-tested before use, as in the
 *    probes;
 *  - the RallySeason one wipes three ranges of a fresh save header with
 *    memset when both the class and count bytes are zero, re-reading the
 *    header pointer for each.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* name pieces: extern arrays, never literals (VC5 folds a literal's scan) */
extern char s_RallySeason_100acb00[];
extern char s_brf_100acaf8[];
extern char s_TimeAttack_100acb14[];
extern char s_grf_100acb0c[];

extern int  DAT_100aab94;                /* the picked record index          */
extern int  DAT_10ac5bf4;                /* g_brAA289C: a season is active   */
extern char DAT_10ac4100[];              /* the name set aside for a cancel  */
extern char DAT_117a5f28[];              /* the ghost save path buffer       */
extern char DAT_117a6030[];              /* the season save path buffer      */
extern int  DAT_10ac5c60;                /* g_brRootPhase: owns the descriptors at +0xC0/+0xC4 */

/* The loaded-save staging block.  The header pointer (0x10AF2094) and the
 * display name (0x10AF3CF0) are declared as ONE object on purpose: the
 * original reloads the header pointer only AFTER the strcpy into the name
 * has finished (`rep movsb / mov edx,[pHdr]`), i.e. VC5 treated the copy as
 * able to alias the pointer.  Two separate externs let it hoist the reload
 * into the strcpy's tail; a separate struct for the name alone does not
 * block it either.  Only members that are used are named; the extent is the
 * distance between the two addresses, not a claim about what lies between. */
struct BrSaveStage {
    int  *pHdr;               /* 0x10AF2094  the loaded 0x200-byte payload */
    char  pad[0x1C5C - 4];
    char  szName[0x80];       /* 0x10AF3CF0  the save's display name */
};
extern struct BrSaveStage DAT_10af2094;
extern int  DAT_10ac5c3c;                /* RallySeason list dirty           */
extern int  DAT_10ac5c44;                /* TimeAttack list dirty            */

extern void BrMenuSub100709A0(void);     /* 0x10069930 write the season save */
extern void FUN_10069de0(void);          /* 0x10069DE0 write the ghost save  */

/* WHAT IT DOES: finish renaming Rally Season slot n.  Refuses with 0 while
 * no season is active.  A result of -1 (cancelled) puts the set-aside name
 * back on the record; otherwise the typed name is committed: the slot's
 * file name becomes the season path, the name goes into the descriptor's
 * entry and the save header, a header whose class and count are both zero
 * has its three tables wiped, the save is written and the list is marked
 * dirty.  Reports 1. */
/* @implements 0x1003B350 glide BrSaveNameCommitRallySeason */
int BrSaveNameCommitRallySeason(int pList, int code)
{
    char szNum[4];
    char szPath[260];

    if (DAT_10ac5bf4 == 0)
        return 0;
    /* Cancelled arm as the `if`, commit as the `else`: the lone if/else is
     * laid failure-first (`jne` to the commit), which is how the original
     * has the -1 arm inline and the commit as the jump target. */
    if (code == -1) {
        if (DAT_10ac4100 != NULL)
            strcpy((char *)pList + DAT_100aab94 * 0x438 + 0x35, DAT_10ac4100);
    } else {
        if ((char *)pList + DAT_100aab94 * 0x438 + 0x35 != NULL) {
            strcpy(szPath, s_RallySeason_100acb00);
            _itoa(DAT_100aab94, szNum, 10);
            strcat(szPath, szNum);
            strcat(szPath, s_brf_100acaf8);
            strcpy(DAT_117a6030, szPath);
            strcpy((char *)*(int *)(DAT_10ac5c60 + 0xc0) + DAT_100aab94 * 0x104 + 4,
                   (char *)pList + DAT_100aab94 * 0x438 + 0x35);
            strcpy(DAT_10af2094.szName, (char *)pList + DAT_100aab94 * 0x438 + 0x35);
            if (((char *)DAT_10af2094.pHdr)[4] == 0 && ((char *)DAT_10af2094.pHdr)[5] == 0) {
                memset((char *)DAT_10af2094.pHdr + 6, 0, 6 * 4);
                memset((char *)DAT_10af2094.pHdr + 0x1e, 0, 12 * 4);
                memset((char *)DAT_10af2094.pHdr + 0x50, 0, 24 * 4);
            }
            BrMenuSub100709A0();
            DAT_10ac5c3c = 1;
        }
    }
    return 1;
}

/* WHAT IT DOES: finish renaming Time Attack slot n -- the same commit as
 * the RallySeason one without the active-season guard or the header wipe:
 * -1 restores the set-aside name; otherwise the slot's file name becomes
 * the ghost path, the typed name goes into the descriptor entry and the
 * save header, the ghost save is written and the list is marked dirty.
 * Reports 1. */
/* @implements 0x1003BAC0 glide BrSaveNameCommitTimeAttack */
int BrSaveNameCommitTimeAttack(int pList, int code)
{
    char szNum[4];
    char szPath[260];

    if (code == -1) {
        if (DAT_10ac4100 != NULL)
            strcpy((char *)pList + DAT_100aab94 * 0x438 + 0x35, DAT_10ac4100);
    } else {
        if ((char *)pList + DAT_100aab94 * 0x438 + 0x35 != NULL) {
            strcpy(szPath, s_TimeAttack_100acb14);
            _itoa(DAT_100aab94, szNum, 10);
            strcat(szPath, szNum);
            strcat(szPath, s_grf_100acb0c);
            strcpy(DAT_117a5f28, szPath);
            strcpy((char *)*(int *)(DAT_10ac5c60 + 0xc4) + DAT_100aab94 * 0x104 + 4,
                   (char *)pList + DAT_100aab94 * 0x438 + 0x35);
            strcpy(DAT_10af2094.szName, (char *)pList + DAT_100aab94 * 0x438 + 0x35);
            FUN_10069de0();
            DAT_10ac5c44 = 1;
        }
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
