/* br_optrestore.c -- settings: republishing the saved option block.
 *
 * BrSub1003E3A0 selects the data table for the chosen entry, copies its name
 * for the menus, sets four on/off states, and copies the saved twelve-dword
 * settings block back out into the individual globals the game reads.
 *
 * Filed out of the address batch slice6_72.c, whose preamble is carried
 * verbatim below.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice3_42.h"   /* br_globals: its objects */
#include <string.h>

#include "slice6_72.h"

/* ==========================================================================
 * 0x1003E3A0
 * ========================================================================== */
/* WHAT IT DOES: makes a set of options actually take effect. It picks the data
 * table that goes with the selected entry, copies its name into the buffer the
 * menus display, notes four on/off states, and then republishes a saved block
 * of a dozen settings out into the individual globals the rest of the game
 * reads. One setting cannot hold the value 1 and is quietly promoted to 2 on
 * the way through. */
/* @implements 0x1003E3A0 d3d BrSub1003E3A0 */
/* Glide arm, hand-transcribed from 0x100379B0: loose globals throughout; the
 * record pointer is a switch on the selector (default first, as the original
 * lays the `dec/je` chain out); the final OR goes through a named temp, which
 * is what puts the global's load in eax and the setting in ecx. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10008d60: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrSub1003E3A0(void)
{
    int v;

    v = (*(int (*)[])&g_aBrAC520)[g_brKind5D64];
    (*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ = v;
    switch (v) {
    case 1:  (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[])&g_BrCtrlCfg) + 0xa8; break;
    case 2:  (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[])&g_BrCtrlCfg) + 0x150; break;
    case 3:  (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[])&g_BrCtrlCfg) + 0x1f8; break;
    default: (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[])&g_BrCtrlCfg); break;
    }
    g_brSel5D74 = DAT_10b71540 == 0;
    (*(int *)&DAT_10ac5d78) = DAT_10b71538 == 0;
    g_brSel5D7C = (*(int *)&g_BrDrawReflectEnable) == 0;
    g_brSel5D80 = DAT_10b71b00 == 0;
    strcpy(g_aBrA9CDF0, DAT_10b71544);
    BrPodNop();
    g_brIdx0ABDE8 = (g_aBrB4E710[0]);
    DAT_10ac5d58 = (*(int *)&g_aBrB4E710[1]);
    (*(int *)&DAT_10ac5d60) = (*(int *)&g_aBrB4E710[2]);
    (*(int *)&DAT_100abdec) = (*(int *)&g_aBrB4E710[3]);
    (*(int *)&DAT_100abdf0) = (*(int *)&g_aBrB4E710[4]);
    g_brSel0ABDF4 = (*(int *)&g_aBrB4E710[5]);
    g_brKind5D64 = (*(int *)&g_aBrB4E710[6]);
    if ((*(int *)&g_aBrB4E710[6]) == 1)
        g_brKind5D64 = 2;
    /* statement order decides which of edx/ecx each setting lands in */
    DAT_100abdf8 = (*(int *)&g_aBrB4E710[7]);
    DAT_100aab84 |= (unsigned short)(*(int *)&g_aBrB4E710[8]);
    DAT_10ac5d6c = (*(int *)&g_aBrB4E710[9]);
    (*(int *)&g_i0AC65C) = (*(int *)&g_aBrB4E710[10]);
    DAT_10ac5d70 = (*(int *)&g_aBrB4E710[11]);
    DAT_10ac5d68 = (*(int *)&g_aBrB4E710[8]);
    {
        int t = DAT_100aab8c;
        t |= (*(int *)&g_aBrB4E710[9]);
        DAT_100aab8c = t;
    }
}
