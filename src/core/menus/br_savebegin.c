/* br_savebegin.c -- menus: start a game mode from a save slot the player has
 * just picked in a record list.
 *
 *   0x1003B6D0  RallySeason<n>.brf   (d3d twin 0x10042170)
 *   0x1003BDE0  TimeAttack<n>.grf    (d3d twin 0x10042880, the port body
 *                                      BrOptBeginTimeAttack in slice2_25.c)
 *
 * The companions of the probes in br_saveprobe.c: those decide whether slot
 * n already has a file, these load it.  Both assemble the slot's file name
 * in a stack buffer exactly as the probes do (4-byte itoa scratch under a
 * 260-byte path -- three digits at most, reproduced), copy the name into
 * the fixed save-path buffer, call the loader, and then fan the loaded
 * option words out into the globals the race setup reads.
 *
 * Shape notes, read off the original bytes:
 *  - the name pieces are extern arrays (the original scans them);
 *  - the 0x53-dword option block is copied with the strcpy/memcpy
 *    intrinsics (rep movsd), and in the RallySeason one the SAME pointer
 *    local is then read for the first dword and the two bytes at +4/+5;
 *  - in the RallySeason one the constant 1 pushed as the loader's second
 *    argument is the register VC5 then reuses for the two `= 1` stores;
 *  - the byte-position fix-up loop re-reads the count from the GLOBAL, not
 *    from the local that was just stored to it.
 *
 * ‼ BOTH PARKED 2026-09-05 -- instruction-identical, register-blind
 * multiset 0, one-file sweep tried all four flag sets (O2 is the best).
 *
 * 0x1003B6D0 RallySeason: 11 diff bytes = ONE FRAME SLOT.  The original's
 *   frame is [szNum F+0x10][race number F+0x14][szPath F+0x18]; ours always
 *   comes out [race number F+0x10][szNum F+0x14][szPath F+0x18].  Twenty
 *   spellings of the race-number local leave it at the bottom: int, int[1],
 *   char[4] via (int *), short[2], union, struct, block-scoped, declared
 *   first/last/between, initialised to 0, assigned from *pIdx before the
 *   itoa, memcpy(&n, p, 4), copied back from the global after the store,
 *   and re-reading the global at the sprintf (VC5 will not CSE an extern
 *   across the two calls; it WILL for a static, and then the slot vanishes).
 *   What the probes proved (now in docs/VC5-IDIOMS.md): under /O2 the
 *   frame is laid out top-down by class -- address-taken arrays first,
 *   largest highest -- and a scalar that is register-homed but spilled
 *   goes BELOW every array whatever its type or declaration position.  So
 *   the original's slot is NOT a spilled scalar of any spelling; something
 *   made it an address-taken object homed ABOVE the 4-byte itoa scratch
 *   (i.e. allocated before it), and nothing in the bytes takes its
 *   address.  Fresh idea needed, not another permutation.
 *
 * 0x1003BDE0 TimeAttack: +4 bytes, same 148 instructions.  The block after
 *   the `jge` guard is scheduled/allocated differently: the original keeps
 *   `movsx` of the car byte in EBX and the AF3CEC word in EAX across the
 *   rep movsd; ours swaps them (AF3CEC in EBX, the car index in EDX), and
 *   the eax short-form encodings account for the 4 bytes.  Dead: a signed
 *   char local for the track byte (needed anyway: `movsx edx,al`), named
 *   int locals for the car and class indices assigned after or before the
 *   guard, /Op, /Oy-.  Statement order is the port body's and matches the
 *   original's store order exactly.
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
extern char DAT_100ac9c8[];              /* "r"  */
extern char DAT_100a6b84[];              /* "%d" */
extern char DAT_10396f08[];              /* the empty string the labels reset to */

extern int  DAT_100a9360;                /* g_br0AA010: which mode is being begun */
extern int  DAT_10ac5c38;                /* RallySeason "loaded" latch */
extern int  DAT_10ac5c40;                /* g_brAA28E8: TimeAttack "loaded" latch */
extern int  DAT_105ccbc4;                /* g_br690A18 */
extern char DAT_117a5f28[];              /* the ghost save path buffer  */
extern char DAT_117a6030[];              /* the season save path buffer */
extern signed char DAT_105bc8e0;         /* g_br680738: loaded track index, <0 = bad */
extern signed char DAT_105bc8e7;         /* g_br68073F: loaded car index */
extern int  DAT_10af3cd8, DAT_10af3cdc, DAT_10af3ce0, DAT_10af3ce4, DAT_10af3ce8, DAT_10af3cec;
extern int *DAT_10af2094;                /* g_brPACED34: the loaded 0x53-dword option block */
extern int  DAT_10ac5a48[];              /* g_aBrAA26F0: its in-game copy */
extern int  DAT_100abde8, DAT_100abdec, DAT_100abdf0, DAT_100abdf4, DAT_100abdf8, DAT_100abdfc;
extern int  DAT_100b3858;                /* g_br0B4050 */
extern int  DAT_10ac5d58, DAT_10ac5d60;  /* g_brAA2A00, g_brAA2A08 */
extern int  DAT_100bcbe8;                /* g_br0BD3E0 */
extern int  DAT_1007b320, DAT_1007b324, DAT_1007b328, DAT_1007b32c;
extern int  DAT_10226e7c, DAT_10226e80;
extern int  DAT_100b3014;                /* g_br0B380C */
extern int  DAT_100abc78[], DAT_100abc60[], DAT_100abbc0[], DAT_100abc40[], DAT_100abc50[], DAT_100abcb8[];
extern int  DAT_10ac5bf4;                /* g_brAA289C */
extern int  DAT_10ac5bf8;                /* season: race number */
extern int  DAT_10ac5bfc;                /* season: entrant count */
extern int  DAT_10ac5c04;
extern char DAT_10ac5c10;                /* season: class index */
extern int  DAT_10ac5c1c;                /* season: points total */
extern int  DAT_10ac5a40;                /* season: packed 0-based positions */
extern unsigned short DAT_10ac5a66[];    /* per-class 4 x u16 rows */
extern int  DAT_10ac5a4e[];              /* per-class packed positions */
extern int  DAT_10ac40f8, DAT_10ac40fc;
extern char DAT_10ac5870[];              /* label: race number */
extern char DAT_10ac46a0[];              /* label: entrant count */

extern void BrSub1003E680(void);         /* 0x10037C90 */
extern char BrSub10071130(int a, int b); /* 0x1006A080 the save loader */
extern void FUN_100378c0(int code);      /* 0x100378C0 error box */
extern void BrOptSave(void);             /* 0x10037920 */
extern void BrSub1003E510(void);         /* 0x10037B20 */
extern void BrRaceSettingsCommit(void);  /* 0x10058A30 */

/* WHAT IT DOES: starts a Rally Season from save slot n.  Blanks the two
 * season labels, builds "RallySeason<n>.brf" and gives up with 0 if it is
 * not there; otherwise records it as the season path, loads it (error box 7
 * on failure), copies the option block into play, publishes the race
 * number / entrant count / class, refreshes the option globals, sums the
 * class's four point columns, converts the packed finishing positions to
 * 1-based, and prints the race number and entrant count into the two
 * labels.  Reports 1. */
/* @implements 0x1003B6D0 glide BrSaveBeginRallySeason */
int BrSaveBeginRallySeason(int pList, int *pIdx)
{
    char  szNum[4];
    int   nRace;
    char  szPath[260];
    FILE *fp;
    int  *p;
    int   cnt;
    int   k;
    int   sum;
    int   i;
    unsigned short *pw;

    DAT_10ac5c38 = 0;
    DAT_100a9360 = 0;
    BrSub1003E680();
    DAT_105ccbc4 = 0;
    strcpy(DAT_10ac5870, DAT_10396f08);
    strcpy(DAT_10ac46a0, DAT_10396f08);
    strcpy(szPath, s_RallySeason_100acb00);
    _itoa(*pIdx, szNum, 10);
    strcat(szPath, szNum);
    strcat(szPath, s_brf_100acaf8);
    fp = fopen(szPath, DAT_100ac9c8);
    if (fp == NULL)
        return 0;
    fclose(fp);
    strcpy(DAT_117a6030, szPath);
    if (BrSub10071130(4, 1) == 0)
        FUN_100378c0(7);
    p = DAT_10af2094;
    memcpy(DAT_10ac5a48, p, 0x53 * 4);
    DAT_10ac5c38 = 1;
    nRace = p[0];
    DAT_10ac5bf8 = nRace;
    cnt = ((unsigned char *)p)[5];
    DAT_10ac5c04 = cnt;
    DAT_10ac5bfc = cnt;
    DAT_10ac5c10 = ((char *)p)[4];
    DAT_10ac5d60 = DAT_10af3cd8;
    DAT_100abdec = DAT_10af3cdc;
    DAT_100abdf0 = DAT_10af3ce0;
    DAT_100abdf4 = DAT_10af3ce4;
    DAT_100abdfc = DAT_10af3ce8;
    DAT_10ac5bf4 = 1;
    BrOptSave();
    BrSub1003E510();
    k = DAT_10ac5c10;
    sum = 0;
    pw = &DAT_10ac5a66[k * 4];
    for (i = 0; i < 4; i++)
        sum += pw[i];
    DAT_10ac5c1c = sum;
    DAT_10ac5a40 = DAT_10ac5a4e[k];
    for (i = 0; i < DAT_10ac5bfc; i++)
        ((char *)&DAT_10ac5a40)[i] += 1;
    DAT_10ac40f8 = *(int *)pw;
    DAT_10ac40fc = *(int *)(pw + 2);
    sprintf(DAT_10ac5870, DAT_100a6b84, nRace + 1);
    sprintf(DAT_10ac46a0, DAT_100a6b84, cnt + 1);
    return 1;
}

/* WHAT IT DOES: starts a Time Attack from save slot n.  Marks the mode,
 * clears the loaded latches, builds "TimeAttack<n>.grf", records it as the
 * ghost path and loads it; a negative loaded track index aborts with 0.
 * Otherwise it copies the loaded track / car / class / options into the
 * race globals (the option block by value), looks the per-index tables up,
 * commits the race settings and reports 1. */
/* @implements 0x1003BDE0 glide BrSaveBeginTimeAttack */
int BrSaveBeginTimeAttack(int pList, int *pIdx)
{
    char szNum[4];
    char szPath[260];
    int  iSel;
    signed char cTrack;

    DAT_100a9360 = 2;
    DAT_10ac5c40 = 0;
    DAT_105ccbc4 = 0;
    BrSub1003E680();
    strcpy(szPath, s_TimeAttack_100acb14);
    _itoa(*pIdx, szNum, 10);
    strcat(szPath, szNum);
    strcat(szPath, s_grf_100acb0c);
    strcpy(DAT_117a5f28, szPath);
    BrSub10071130(1, 1);
    cTrack = DAT_105bc8e0;
    if (cTrack < 0)
        return 0;
    iSel = cTrack;
    DAT_100abdf0 = DAT_10af3ce0;
    DAT_100abdec = DAT_10af3cdc;
    DAT_100abdfc = DAT_10af3ce8;
    DAT_100abdf4 = DAT_10af3ce4;
    DAT_100b3858 = 1;
    DAT_100abde8 = iSel;
    DAT_10ac5d58 = DAT_105bc8e7;
    DAT_10ac5d60 = DAT_10af3cd8;
    DAT_100abdf8 = DAT_10af3cec;
    DAT_100bcbe8 = DAT_10af3cec;
    memcpy(DAT_10ac5a48, DAT_10af2094, 0x53 * 4);
    DAT_100b3014 = DAT_100abc78[iSel];
    DAT_100bcbe8 = DAT_10af3cec;
    DAT_1007b320 = DAT_10af3ce8;
    DAT_10ac5c40 = 1;
    DAT_10226e7c = DAT_100abbc0[DAT_10af3ce4];
    DAT_10226e80 = DAT_100abc60[DAT_105bc8e7];
    DAT_1007b32c = DAT_100abc40[DAT_10af3cdc];
    DAT_1007b328 = DAT_100abc50[DAT_10af3ce0];
    DAT_1007b324 = DAT_100abcb8[DAT_10af3cd8];
    BrRaceSettingsCommit();
    DAT_10ac5bf4 = 1;
    return 1;
}

#endif /* BR_MATCHING_BUILD */
