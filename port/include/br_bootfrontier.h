/* br_bootfrontier.h -- the EDGE of the port, at the boot chain.
 *
 * WHAT THIS IS, AND WHAT IT DELIBERATELY IS NOT
 *
 * br_boot.c transcribes the game's entry point and top-level state machine.
 * Those functions call twelve others that are not transcribed yet. This file
 * declares them and counts the times each is reached.
 *
 * That is a frontier, not a placeholder, and this project has good reason to
 * insist on the distinction. A placeholder RETURNS A PLAUSIBLE VALUE so that
 * something visible happens -- it makes the tree look further along than it
 * is, and four of them in the phase system are exactly why "the menu does not
 * navigate" was investigated for a long time as though it were a bug in the
 * menu. A frontier entry does the opposite: it does nothing, it says so, and
 * a run prints what it hit.
 *
 * Concretely: no function here invents a return value that could be mistaken
 * for real behaviour. The two that must return something return the value
 * that is unambiguously "nothing happened" for their caller, and every reach
 * is counted and reported.
 *
 * WHY THE GLOBAL ACCESSORS EXIST
 *
 * State 4 reads five globals (0x10AC5C5C, 0x100ABAA0, 0x10B71A48..0x10B71A54)
 * that belong to modules br_boot.c does not own -- the renderer device, the
 * mode-change flag, and four values handed to 0x10063970. Rather than declare
 * storage for them here (which would create a SECOND definition racing the
 * real owner's, a mistake this tree has made before with 0x10AA2918), they
 * are reached through accessors that currently report the load-time value and
 * are the single place to re-point when the owning module lands.
 */
#ifndef BR_BOOTFRONTIER_H
#define BR_BOOTFRONTIER_H

#include <stdint.h>

/* ---- the twelve unported callees, by original Glide address -------- */

/* RallyMain's own chain, 0x1001CC00. */
void    BrBootFrontier_10007F10(void);
void    BrBootFrontier_10063860(void);
void    BrBootFrontier_1006D1A0(void);
void    BrBootFrontier_10007F40(const char *pszCmdLine);
void    BrBootFrontier_10063060(void);          /* __thiscall config load */
void    BrBootFrontier_10009C00(void);
void    BrBootBuildConfigPath(void);            /* the inlined strcpy+strcat */
const char *BrBootConfigPath(void);             /* 0x10B72F48, for tests */

void    BrBootFrontier_10032530(void);          /* state 0 */
void    BrBootFrontier_1006C290(int32_t set);   /* sfx bank select: 0=menu */
void    BrBootFrontier_10058AF0(void);

/* 0x1002E324 is NOT here: it is already ported as BrGameStepInvoke in
 * br_gamestep.c. It was briefly given a frontier entry, which declared a
 * transcribed function missing -- recorded because it is the recurring
 * failure in this project, not a one-off. */

/* TEST HOOK, and it exists to close a real hole rather than for convenience.
 *
 * 0x1001CDB0 calls the frame and THEN reads 0x100A98F8:
 *     call 0x1002E324
 *     mov  eax, [0x100A98F8]
 * so a frame that sets the quit flag takes effect on the SAME tick. With no
 * way to change the flag from inside the frame, a test cannot tell that order
 * from the reverse -- and a mutant that hoisted the read above the call passed
 * the whole suite. This hook is what makes that mutant die. */
void    BrBootFrontierSetFrameHook(void (*pfn)(void));

void    BrBootFrontier_10063970(int32_t a1, int32_t a2, int32_t a3,
                                int32_t a4, int32_t a5);   /* state 3 */
void    BrBootFrontier_1006C990(const char *pszImg, int32_t flags);
void    BrBootFrontier_100628B0(void);

void    BrBootFrontier_1006C460(void);          /* state 4 */
void    BrBootFrontier_10056260(void);
int32_t BrBootFrontier_1006E280(void);
void    BrBootFrontier_SetModeTail(void);       /* 0x1001CE9D.. */

/* ---- globals owned elsewhere -------------------------------------- */

int32_t BrBootGlobal_AC5C5C(void);   /* 0x10AC5C5C renderer device handle */
int32_t BrBootGlobal_ABAA0(void);    /* 0x100ABAA0 mode-change requested  */
int32_t BrBootGlobal_B71A48(void);   /* 0x10B71A48 } the four values      */
int32_t BrBootGlobal_B71A4C(void);   /* 0x10B71A4C } state 3 hands to     */
int32_t BrBootGlobal_B71A50(void);   /* 0x10B71A50 } 0x10063970           */
int32_t BrBootGlobal_B71A54(void);   /* 0x10B71A54 }                      */

void    BrBootSetModeGlobals(int32_t cx, int32_t cy);
void    BrBootSetAC6748(int32_t v);  /* 0x10AC6748 */

/* Install the real transcriptions for the three entries that now have them.
 * NULL leaves the entry counting and doing nothing, which is the honest edge.
 * The frontier deliberately takes no link dependency on those modules -- see
 * the banner in br_bootfrontier.c for why the direct call was reverted. */
void BrBootFrontierInstall(void (*pfn10007F10)(void),
                           void (*pfn10007F40)(const char *),
                           void (*pfn10063060)(void));

/* ---- the report --------------------------------------------------- */

/* Number of frontier entries. */
int         BrBootFrontierCount(void);
/* Name and hit count for entry i, in declaration order. */
const char *BrBootFrontierName(int i);
int32_t     BrBootFrontierHits(int i);
/* Print every entry that was reached, or say that none were. */
void        BrBootFrontierReport(void);
void        BrBootFrontierReset(void);

#endif /* BR_BOOTFRONTIER_H */
