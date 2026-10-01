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

    v = DAT_100abcc0[DAT_10ac5d64];
    DAT_10b71530 = v;
    switch (v) {
    case 1:  DAT_10b71534 = DAT_10b71290 + 0xa8; break;
    case 2:  DAT_10b71534 = DAT_10b71290 + 0x150; break;
    case 3:  DAT_10b71534 = DAT_10b71290 + 0x1f8; break;
    default: DAT_10b71534 = DAT_10b71290; break;
    }
    DAT_10ac5d74 = DAT_10b71540 == 0;
    DAT_10ac5d78 = DAT_10b71538 == 0;
    DAT_10ac5d7c = DAT_10b7153c == 0;
    DAT_10ac5d80 = DAT_10b71b00 == 0;
    strcpy(DAT_10ac3e80, DAT_10b71544);
    FUN_10008d60();
    DAT_100abde8 = DAT_10b71a70;
    DAT_10ac5d58 = DAT_10b71a74;
    DAT_10ac5d60 = DAT_10b71a78;
    DAT_100abdec = DAT_10b71a7c;
    DAT_100abdf0 = DAT_10b71a80;
    DAT_100abdf4 = DAT_10b71a84;
    DAT_10ac5d64 = DAT_10b71a88;
    if (DAT_10b71a88 == 1)
        DAT_10ac5d64 = 2;
    /* statement order decides which of edx/ecx each setting lands in */
    DAT_100abdf8 = DAT_10b71a8c;
    DAT_100aab84 |= (unsigned short)DAT_10b71a90;
    DAT_10ac5d6c = DAT_10b71a94;
    DAT_100abdfc = DAT_10b71a98;
    DAT_10ac5d70 = DAT_10b71a9c;
    DAT_10ac5d68 = DAT_10b71a90;
    {
        int t = DAT_100aab8c;
        t |= DAT_10b71a94;
        DAT_100aab8c = t;
    }
}
