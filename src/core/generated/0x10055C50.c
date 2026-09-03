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
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <string.h>

typedef struct BrSaveRec55C50 {
    int  f00;               /* +0x000 */
    char szName[0x100];     /* +0x004 */
} BrSaveRec55C50;           /* 0x104 */

typedef struct BrRoot55C50 {
    char             pad[0xC0];
    BrSaveRec55C50  *pSeason;       /* +0xC0 */
    BrSaveRec55C50  *pTimeAttack;   /* +0xC4 */
} BrRoot55C50;

extern int          g_brSeasonMode;     /* 0x10AC5BA0 */
extern BrRoot55C50 *g_brRootPhase;      /* 0x10AC5C60 */

/* The prefixes are EXTERN arrays, not literals in this TU: VC5 folds
 * strlen() of a string literal to a constant and the original does not
 * fold it, so the original's source could not see the contents either. */
extern char s_RallySeason_100acb00[];   /* "RallySeason" */
extern char s_TimeAttack_100acb14[];    /* "TimeAttack"  */

/* @implements 0x10055C50 glide BrSaveSlotNameSet_10055C50 */
int __stdcall BrSaveSlotNameSet_10055C50(const char *pKey, const char *pName)
{
    char             szNum[32];
    char            *pRec;

    if (g_brSeasonMode != 0) {
        strcpy(szNum, pKey + strlen(s_RallySeason_100acb00));
        pRec = g_brRootPhase->pSeason[atoi(szNum)].szName;
    } else {
        strcpy(szNum, pKey + strlen(s_TimeAttack_100acb14));
        pRec = g_brRootPhase->pTimeAttack[atoi(szNum)].szName;
    }

    strcpy(pRec, pName);
    return 1;
}

#endif /* BR_MATCHING_BUILD */
