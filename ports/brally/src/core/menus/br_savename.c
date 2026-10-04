#include "br_ui.h"
#include "slice1_06.h"
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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* name pieces: extern arrays, never literals (VC5 folds a literal's scan) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* the picked record index          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_brAA289C: a season is active   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the name set aside for a cancel  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the ghost save path buffer       */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the season save path buffer      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_brRootPhase: owns the descriptors at +0xC0/+0xC4 */

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* RallySeason list dirty           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* TimeAttack list dirty            */

/* BrMenuSub100709A0: prototype in br_funcs.h */
/* FUN_10069de0: prototype in br_funcs.h */

/* WHAT IT DOES: finish renaming Rally Season slot n.  Refuses with 0 while
 * no season is active.  A result of -1 (cancelled) puts the set-aside name
 * back on the record; otherwise the typed name is committed: the slot's
 * file name becomes the season path, the name goes into the descriptor's
 * entry and the save header, a header whose class and count are both zero
 * has its three tables wiped, the save is written and the list is marked
 * dirty.  Reports 1. */
/* @implements 0x1003B350 glide BrSaveNameCommitRallySeason */
int BrSaveNameCommitRallySeason(void *pList, int code)
{
    char szNum[4];
    char szPath[260];

    if (g_5BF4 == 0)
        return 0;
    /* Cancelled arm as the `if`, commit as the `else`: the lone if/else is
     * laid failure-first (`jne` to the commit), which is how the original
     * has the -1 arm inline and the commit as the jump target. */
    if (code == -1) {
        if (g_aBrA9D078 != NULL)
            strcpy(((BrTextList *)pList)->aItems[g_AB94].sz, g_aBrA9D078);
    } else {
        if (((BrTextList *)pList)->aItems[g_AB94].sz != NULL) {
            strcpy(szPath, s_RallySeason_100acb00);
            _itoa(g_AB94, szNum, 10);
            strcat(szPath, szNum);
            strcat(szPath, s_brf_100acaf8);
            strcpy(DAT_117a6030, szPath);
            strcpy(((BrNameList *)((BrPhase_ *)g_2908)->fC0)->asz[g_AB94],
                   ((BrTextList *)pList)->aItems[g_AB94].sz);
            strcpy((*(char (*)[128])&g_aBrRaceCar[0].sz2ABC[44]), ((BrTextList *)pList)->aItems[g_AB94].sz);
            if (((char *)(*(int * *)&g_aBrRaceCar[0].pEquip))[4] == 0 && ((char *)(*(int * *)&g_aBrRaceCar[0].pEquip))[5] == 0) {
                memset((char *)(*(int * *)&g_aBrRaceCar[0].pEquip) + 6, 0, 6 * 4);
                memset((char *)(*(int * *)&g_aBrRaceCar[0].pEquip) + 0x1e, 0, 12 * 4);
                memset((char *)(*(int * *)&g_aBrRaceCar[0].pEquip) + 0x50, 0, 24 * 4);
            }
            BrMenuSub100709A0();
            g_5C3C = 1;
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
int BrSaveNameCommitTimeAttack(void *pList, int code)
{
    char szNum[4];
    char szPath[260];

    if (code == -1) {
        if (g_aBrA9D078 != NULL)
            strcpy(((BrTextList *)pList)->aItems[g_AB94].sz, g_aBrA9D078);
    } else {
        if (((BrTextList *)pList)->aItems[g_AB94].sz != NULL) {
            strcpy(szPath, s_TimeAttack_100acb14);
            _itoa(g_AB94, szNum, 10);
            strcat(szPath, szNum);
            strcat(szPath, s_grf_100acb0c);
            strcpy(DAT_117a5f28, szPath);
            strcpy(((BrNameList *)((BrPhase_ *)g_2908)->fC4)->asz[g_AB94],
                   ((BrTextList *)pList)->aItems[g_AB94].sz);
            strcpy((*(char (*)[128])&g_aBrRaceCar[0].sz2ABC[44]), ((BrTextList *)pList)->aItems[g_AB94].sz);
            BrGhostSave();
            (*(int *)&g_brUinAA28EC) = 1;
        }
    }
    return 1;
}

