#include "br_ui.h"
#include "br_phase.h"   /* BrPhase_, the canonical record */
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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <io.h>        /* _findfirst / _findnext -- 0x10055ED0 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* page object owning the RallySeason list */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* page object owning the TimeAttack list  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* RallySeason record-list base            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* TimeAttack record-list base             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the picked record index                 */

/* The four name pieces are extern arrays, not literals: the original SCANS
 * each with repne scasb, and VC5 folds a literal's length outright. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* "r" */

/* BrExt_10041A00: prototype in br_funcs.h */
/* BrExt_10042410: prototype in br_funcs.h */
/* BrFn1003E070: prototype in br_funcs.h */

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
    g_5D24 = pList;
    g_AB94 = n;
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

    g_brPAA29D0 = pList;
    g_AB94 = n;
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
        ((BrUiCtl_ *)DAT_10ac5d1c)->pOwner->aFlags[1] = 1;   /* ctl+0x2AE8 -> phase+0x70 */
    return 1;
}


/* 0x10055C50 -- store a display name into the season or time-attack record
 * named by a save key.
 *
 * WHAT IT DOES: the key is a save-file stem ("RallySeason3", "TimeAttack7");
 * skip the fixed prefix, atoi the trailing number, and copy the caller's
 * string over the name field of that slot's record. Which table is indexed
 * is a mode flag, not part of the key: the 0x10AC5BA0 flag picks the root
 * phase's +0xC0 (season) or +0xC4 (time attack) array. Records are 0x104
 * bytes with the name at +4, so VC5 scales the index as (n*64 + n) * 4 --
 * the *65 lands in each arm next to the atoi, the *4 and the +4 fold into
 * the lea after the arms merge.
 *
 * Two levers, both worth having in the dictionary:
 *
 *  - `strlen` of a string LITERAL is constant-folded by VC5 (the whole
 *    inline scan collapses to a `lea`). The original expands it as repne
 *    scasb over the string, so the prefixes cannot have been literals in
 *    this TU -- they are extern arrays whose contents the compiler cannot
 *    see. Spelling them as literals costs 37 bytes and two scans.
 *
 *  - Where the record ADDRESS is formed decides whether the +4 folds. The
 *    original has one `lea [tbl + n65*4 + 4]` after the arms merge, so
 *    each arm must end by producing the FINAL char* -- i.e. the arms say
 *    `pRec = tbl[atoi(..)].szName` and VC5 tail-merges the identical
 *    closing lea. Keeping table and index as two locals and indexing at
 *    the use hoists the *65 out of the arms; keeping a record POINTER in
 *    the arms leaves the +4 as its own `lea [R+4]` at the use. Both are
 *    3-instruction misses.
 *
 * __stdcall, `ret 8`, no receiver -- ecx is scratch from the first
 * instruction. Returns 1 unconditionally.
 */

typedef struct BrSaveRec55C50 {
    int  f00;               /* +0x000 */
    char szName[0x100];     /* +0x004 */
} BrSaveRec55C50;           /* 0x104 */

typedef struct BrRoot55C50 {
    char             pad[0xC0];
    BrSaveRec55C50  *pSeason;       /* +0xC0 */
    BrSaveRec55C50  *pTimeAttack;   /* +0xC4 */
} BrRoot55C50;

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10AC5BA0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10AC5C60 */

/* The prefixes are EXTERN arrays, not literals in this TU: VC5 folds
 * strlen() of a string literal to a constant and the original does not
 * fold it, so the original's source could not see the contents either. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "RallySeason" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "TimeAttack"  */

/* WHAT IT DOES: store a player-entered name into the right saved-game slot.
 * The slot number is parsed out of the END of the key string, and which
 * table it indexes -- season or time attack -- depends on the current mode.
 * Always reports success. */
/* @implements 0x10055C50 glide BrSaveSlotNameSet_10055C50 */
int __stdcall BrSaveSlotNameSet_10055C50(const char *pKey, const char *pName)
{
    char             szNum[32];
    char            *pRec;

    if (g_brGate5BA0 != 0) {
        strcpy(szNum, pKey + strlen(s_RallySeason_100acb00));
        pRec = (*(BrSaveRec55C50 * *)&((BrPhase_ *)((*(BrRoot55C50 * *)&g_2908)))->fC0)[atoi(szNum)].szName;
    } else {
        strcpy(szNum, pKey + strlen(s_TimeAttack_100acb14));
        pRec = (*(BrSaveRec55C50 * *)&((BrPhase_ *)((*(BrRoot55C50 * *)&g_2908)))->fC4)[atoi(szNum)].szName;
    }

    strcpy(pRec, pName);
    return 1;
}

/* WHAT IT DOES: count how many files match a wildcard pattern, using the
 * CRT's find-first/find-next walk and stopping after 100 entries. Returns
 * -1 when nothing matches at all; otherwise the number of matches AFTER the
 * first one (the first hit is not counted), which is what the saved-game
 * browser uses as the index of the last slot. */
/* @implements 0x10055ED0 glide BrFileCountMatching */
int __stdcall BrFileCountMatching(const char *pszPattern)
{
  struct _finddata_t fd;
  long h;
  int n;
  int i;

  h = _findfirst(pszPattern, &fd);
  if (h == -1) {
    return -1;
  }
  n = 0;
  for (i = 1; i < 100; i++) {
    if (_findnext(h, &fd) != 0) break;
    n++;
  }
  _findclose(h);
  return n;
}
