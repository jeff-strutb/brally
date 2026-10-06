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
 * !! BOTH PARKED 2026-09-05 -- instruction-identical, register-blind
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
 *   What the probes proved (now in docs/brally/VC5-IDIOMS.md): under /O2 the
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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_41.h"   /* br_globals: its objects */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* name pieces: extern arrays, never literals (VC5 folds a literal's scan) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* "r"  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* "%d" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the empty string the labels reset to */

/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_br0AA010: which mode is being begun */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* RallySeason "loaded" latch */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_brAA28E8: TimeAttack "loaded" latch */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_br690A18 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the ghost save path buffer  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* the season save path buffer */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* g_br680738: loaded track index, <0 = bad */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* g_br68073F: loaded car index */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_brPACED34: the loaded 0x53-dword option block */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* g_aBrAA26F0: its in-game copy */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_br0B4050 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* g_brAA2A00, g_brAA2A08 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_br0BD3E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_br0B380C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* g_brAA289C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* season: race number */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* season: entrant count */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* season: class index */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* season: points total */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                /* season: packed 0-based positions */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* per-class 4 x u16 rows */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* per-class packed positions */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* label: race number */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* label: entrant count */

/* BrSub1003E680: prototype in br_funcs.h */
/* BrSub10071130: prototype in br_funcs.h */
/* FUN_100378c0: prototype in br_funcs.h */
/* BrOptSave: prototype in br_funcs.h */
/* BrSub1003E510: prototype in br_funcs.h */
/* BrRaceSettingsCommit: prototype in br_funcs.h */

/* WHAT IT DOES: starts a Rally Season from save slot n.  Blanks the two
 * season labels, builds "RallySeason<n>.brf" and gives up with 0 if it is
 * not there; otherwise records it as the season path, loads it (error box 7
 * on failure), copies the option block into play, publishes the race
 * number / entrant count / class, refreshes the option globals, sums the
 * class's four point columns, converts the packed finishing positions to
 * 1-based, and prints the race number and entrant count into the two
 * labels.  Reports 1. */
/* @t4-pass 0x1003B6D0 1 2026-09-07 probes 106 bytes 671 insns 216 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1003B6D0 2 2026-09-07 probes 107 bytes 671 insns 216 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t3 0x1003B6D0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 671/671 insns 216/216 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is scheduling/relocation (11 masked diffs at +0x9f, RAW/REGNORM
 * 0+0): a fold-order fork of the same shape as BrSaveResumeAutoSave below;
 * every instruction is the original's.  Passes 1-2 (crank.py, ledger above)
 * moved nothing at 671/216/1/0.  Do not reopen before the end-grind.
 */
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

    g_5C38 = 0;
    (*(int *)&g_brRaceRules.mode) = 0;
    BrSub1003E680();
    (*(int *)&DAT_105ccb68[23]) = 0;
    strcpy(g_aBrAA2518, g_aBr39B720);
    strcpy(DAT_10ac46a0, g_aBr39B720);
    strcpy(szPath, s_RallySeason_100acb00);
    _itoa(*pIdx, szNum, 10);
    strcat(szPath, szNum);
    strcat(szPath, s_brf_100acaf8);
    fp = fopen(szPath, DAT_100ac9c8);
    if (fp == NULL)
        return 0;
    fclose(fp);
    strcpy(DAT_117a6030, szPath);
    if (BrSaveLoad(4, 1) == 0)
        FUN_100378c0(7);
    p = (*(int * *)&g_aBrRaceCar[0].pEquip);
    memcpy(DAT_10ac5a48, p, 0x53 * 4);
    g_5C38 = 1;
    nRace = p[0];
    g_brPhase5BF8 = nRace;
    cnt = ((unsigned char *)p)[5];
    g_brIdx5C04 = cnt;
    g_brIdx5BFC = cnt;
    (*(char *)&DAT_10ac5c10) = ((char *)p)[4];
    (*(int *)&DAT_10ac5d60) = (*(int *)&g_aBrRaceCar[0].sz2ABC[20]);
    (*(int *)&DAT_100abdec) = (*(int *)&g_aBrRaceCar[0].sz2ABC[24]);
    (*(int *)&DAT_100abdf0) = (*(int *)&g_aBrRaceCar[0].sz2ABC[28]);
    g_brSel0ABDF4 = (*(int *)&g_aBrRaceCar[0].sz2ABC[32]);
    (*(int *)&g_i0AC65C) = (*(int *)&g_aBrRaceCar[0].sz2ABC[36]);
    g_5BF4 = 1;
    BrOptSave();
    BrSub1003E510();
    k = (*(char *)&DAT_10ac5c10);
    sum = 0;
    pw = &(*(unsigned short (*)[])&g_aBrAA270E)[k * 4];
    for (i = 0; i < 4; i++)
        sum += pw[i];
    (*(int *)&DAT_10ac5c1c) = sum;
    (*(int *)&g_brVal5A40) = (*(int (*)[])&g_aBrAA26F6)[k];
    for (i = 0; i < g_brIdx5BFC; i++)
        ((char *)&(*(int *)&g_brVal5A40))[i] += 1;
    (*(int *)&g_brVal40F8) = *(int *)pw;
    (*(int *)&DAT_10ac40fc) = *(int *)(pw + 2);
    sprintf(g_aBrAA2518, g_szBrFmt6B84, nRace + 1);
    sprintf(DAT_10ac46a0, g_szBrFmt6B84, cnt + 1);
    return 1;
}

/* WHAT IT DOES: starts a Time Attack from save slot n.  Marks the mode,
 * clears the loaded latches, builds "TimeAttack<n>.grf", records it as the
 * ghost path and loads it; a negative loaded track index aborts with 0.
 * Otherwise it copies the loaded track / car / class / options into the
 * race globals (the option block by value), looks the per-index tables up,
 * commits the race settings and reports 1. */
/* @t4-pass 0x1003BDE0 1 2026-09-07 probes 150 bytes 532 insns 148 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1003BDE0 2 2026-09-07 probes 150 bytes 531 insns 148 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1003BDE0 3 2026-09-07 probes 150 bytes 531 insns 148 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1003BDE0 4 2026-09-09 probes 10 bytes 532 insns 148 regions 1 rows 0 census no  (hand, fn.py variants: literal/order/decl/index spellings, all inert or worse) */
/* @t4-pass 0x1003BDE0 5 2026-09-09 probes 10 bytes 532 insns 148 regions 1 rows 0 census yes  (hand, fn.py variants: amp/nested/expression forms, store-order swaps, all inert; corpus query at +0x10) */
/* @t3 0x1003BDE0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 532/528 insns 148/148 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 5 zero-movement 4 5
 * residue is register colouring/encoding only: identical register-blind
 * multiset (rows 0+0), insn-exact, +4 B of encoding shadow, 1 masked
 * region.  Three crank census passes (150 probes each) at the prior
 * numbers plus two hand passes at the current ones.
 * Do not reopen before the end-grind. */
/* @implements 0x1003BDE0 glide BrSaveBeginTimeAttack */
int BrSaveBeginTimeAttack(int pList, int *pIdx)
{
    char szNum[4];
    char szPath[260];
    int  iSel;
    signed char cTrack;

    (*(int *)&g_brRaceRules.mode) = 2;
    DAT_10ac5c40 = 0;
    (*(int *)&DAT_105ccb68[23]) = 0;
    BrSub1003E680();
    strcpy(szPath, s_TimeAttack_100acb14);
    _itoa(*pIdx, szNum, 10);
    strcat(szPath, szNum);
    strcat(szPath, s_grf_100acb0c);
    strcpy(DAT_117a5f28, szPath);
    BrSaveLoad(1, 1);
    cTrack = (g_aBrRaceBeginRec[0]);
    if (cTrack < 0)
        return 0;
    iSel = cTrack;
    (*(int *)&DAT_100abdf0) = (*(int *)&g_aBrRaceCar[0].sz2ABC[28]);
    (*(int *)&DAT_100abdec) = (*(int *)&g_aBrRaceCar[0].sz2ABC[24]);
    (*(int *)&g_i0AC65C) = (*(int *)&g_aBrRaceCar[0].sz2ABC[36]);
    g_brSel0ABDF4 = (*(int *)&g_aBrRaceCar[0].sz2ABC[32]);
    (*(int *)&g_brRaceNEntrant) = 1;
    g_brIdx0ABDE8 = iSel;
    DAT_10ac5d58 = (*(signed char *)&g_aBrRaceBeginRec[7]);
    (*(int *)&DAT_10ac5d60) = (*(int *)&g_aBrRaceCar[0].sz2ABC[20]);
    DAT_100abdf8 = (*(int *)&g_aBrRaceCar[0].sz2ABC[40]);
    g_CBE8 = (*(int *)&g_aBrRaceCar[0].sz2ABC[40]);
    memcpy(DAT_10ac5a48, (*(int * *)&g_aBrRaceCar[0].pEquip), 0x53 * 4);
    (*(int *)&g_Br0B380C) = (*(int (*)[])&g_aBrAC4D8)[iSel];
    g_CBE8 = (*(int *)&g_aBrRaceCar[0].sz2ABC[40]);
    g_7b320 = (*(int *)&g_aBrRaceCar[0].sz2ABC[36]);
    DAT_10ac5c40 = 1;
    g_226e7c = (*(int (*)[])&g_aBrAC420)[(*(int *)&g_aBrRaceCar[0].sz2ABC[32])];
    g_226e80 = (*(int (*)[])&g_aBrAC4C0)[(*(signed char *)&g_aBrRaceBeginRec[7])];
    g_7b32c = (*(int (*)[])&g_aBrAC4A0)[(*(int *)&g_aBrRaceCar[0].sz2ABC[24])];
    g_7b328 = (*(int (*)[])&g_aBrAC4B0)[(*(int *)&g_aBrRaceCar[0].sz2ABC[28])];
    g_7b324 = (*(int (*)[])&g_aBrAC518)[(*(int *)&g_aBrRaceCar[0].sz2ABC[20])];
    BrRaceSettingsCommit();
    g_5BF4 = 1;
    return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* "AutoSave.brf" */

/* WHAT IT DOES: continues the auto-saved Rally Season.  Blanks the two
 * season labels, looks for AutoSave.brf and silently does nothing when it
 * is not there; otherwise the same load-and-publish sequence as picking a
 * named season slot (see BrSaveBeginRallySeason above): record it as the
 * season path, load it (error box 7 on failure), copy the option block into
 * play, publish race number / entrant count / class, refresh the option
 * globals, total the class's points, make the packed positions 1-based and
 * print the two labels.  Returns nothing.
 *
 * PARKED 2026-09-05 at 536/536 B, 7 diff bytes, multiset 0: the original
 * loads BOTH row dwords (`mov eax,[edi] / mov ecx,[edi+4]`) and the sprintf
 * import before `inc edx` and the first store; ours stores the first dword
 * before loading the second.  The identical tail text is byte-exact in
 * BrSaveBeginRallySeason above, so the difference is this function's
 * context (void, whole body nested under the fopen test), not the
 * statement.  Dead: two int temps for the pair; an `int *` view of the row;
 * indexed `*(int *)&DAT_10ac5a66[k*4]` loads (recomputes the row, -3
 * insns); `if (fp == NULL) return;` instead of the nested block. */
/* @t4-pass 0x1003B130 1 2026-09-07 probes 86 bytes 536 insns 162 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1003B130 2 2026-09-07 probes 87 bytes 536 insns 162 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t3 0x1003B130 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 536/536 insns 162/162 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is scheduling (7 masked diffs at +0x1d7, RAW/REGNORM 0+0): the
 * original folds an import load before the `inc edx`/first store where this
 * build stores the first dword before loading the second.  The identical tail
 * is byte-exact in BrSaveBeginRallySeason above, so it is this function's
 * context (void body nested under the fopen test), not the statement; the
 * dead-probe list is in the RESIDUE comment above.  Passes 1-2 (crank.py,
 * ledger above) moved nothing at 536/162/1/0.  Do not reopen before the
 * end-grind. */
/* @implements 0x1003B130 glide BrSaveResumeAutoSave */
void BrSaveResumeAutoSave(void)
{
    char  szPath[260];
    int   nRace;
    FILE *fp;
    int  *p;
    int   cnt;
    int   k;
    int   sum;
    int   i;
    unsigned short *pw;

    (*(int *)&g_brRaceRules.mode) = 0;
    BrSub1003E680();
    (*(int *)&DAT_105ccb68[23]) = 0;
    strcpy(g_aBrAA2518, g_aBr39B720);
    strcpy(DAT_10ac46a0, g_aBr39B720);
    strcpy(szPath, s_AutoSave_brf);
    fp = fopen(szPath, DAT_100ac9c8);
    if (fp != NULL) {
        fclose(fp);
        strcpy(DAT_117a6030, szPath);
        if (BrSaveLoad(4, 1) == 0)
            FUN_100378c0(7);
        p = (*(int * *)&g_aBrRaceCar[0].pEquip);
        memcpy(DAT_10ac5a48, p, 0x53 * 4);
        g_5C38 = 1;
        nRace = p[0];
        g_brPhase5BF8 = nRace;
        cnt = ((unsigned char *)p)[5];
        g_brIdx5C04 = cnt;
        g_brIdx5BFC = cnt;
        (*(char *)&DAT_10ac5c10) = ((char *)p)[4];
        (*(int *)&DAT_10ac5d60) = (*(int *)&g_aBrRaceCar[0].sz2ABC[20]);
        (*(int *)&DAT_100abdec) = (*(int *)&g_aBrRaceCar[0].sz2ABC[24]);
        (*(int *)&DAT_100abdf0) = (*(int *)&g_aBrRaceCar[0].sz2ABC[28]);
        g_brSel0ABDF4 = (*(int *)&g_aBrRaceCar[0].sz2ABC[32]);
        (*(int *)&g_i0AC65C) = (*(int *)&g_aBrRaceCar[0].sz2ABC[36]);
        g_5BF4 = 1;
        BrOptSave();
        BrSub1003E510();
        k = (*(char *)&DAT_10ac5c10);
        sum = 0;
        pw = &(*(unsigned short (*)[])&g_aBrAA270E)[k * 4];
        for (i = 0; i < 4; i++)
            sum += pw[i];
        (*(int *)&DAT_10ac5c1c) = sum;
        (*(int *)&g_brVal5A40) = (*(int (*)[])&g_aBrAA26F6)[k];
        for (i = 0; i < g_brIdx5BFC; i++)
            ((char *)&(*(int *)&g_brVal5A40))[i] += 1;
        (*(int *)&g_brVal40F8) = *(int *)pw;
        (*(int *)&DAT_10ac40fc) = *(int *)(pw + 2);
        sprintf(g_aBrAA2518, g_szBrFmt6B84, nRace + 1);
        sprintf(DAT_10ac46a0, g_szBrFmt6B84, cnt + 1);
    }
}

