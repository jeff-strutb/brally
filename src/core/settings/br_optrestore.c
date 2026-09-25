/* br_optrestore.c -- settings: republishing the saved option block.
 *
 * BrSub1003E3A0 selects the data table for the chosen entry, copies its name
 * for the menus, sets four on/off states, and copies the saved twelve-dword
 * settings block back out into the individual globals the game reads.
 *
 * Filed out of the address batch slice6_72.c, whose preamble is carried
 * verbatim below.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
#ifdef BR_MATCHING_BUILD
/* Glide arm, hand-transcribed from 0x100379B0: loose globals throughout; the
 * record pointer is a switch on the selector (default first, as the original
 * lays the `dec/je` chain out); the final OR goes through a named temp, which
 * is what puts the global's load in eax and the setting in ecx. */
extern int DAT_10ac5d64, DAT_100abcc0[], DAT_10b71530;
extern void *DAT_10b71534;
extern unsigned char DAT_10b71290[];
extern int DAT_10b71540, DAT_10b71538, DAT_10b7153c, DAT_10b71b00;
extern int DAT_10ac5d74, DAT_10ac5d78, DAT_10ac5d7c, DAT_10ac5d80;
extern char DAT_10ac3e80[], DAT_10b71544[];
int FUN_10008d60();
extern int DAT_10b71a70, DAT_10b71a74, DAT_10b71a78, DAT_10b71a7c, DAT_10b71a80, DAT_10b71a84, DAT_10b71a88;
extern int DAT_10b71a8c, DAT_10b71a90, DAT_10b71a94, DAT_10b71a98, DAT_10b71a9c;
extern int DAT_100abde8, DAT_10ac5d58, DAT_10ac5d60, DAT_100abdec, DAT_100abdf0, DAT_100abdf4;
extern int DAT_100abdf8, DAT_100abdfc, DAT_10ac5d68, DAT_10ac5d6c, DAT_10ac5d70, DAT_100aab8c;
extern unsigned short DAT_100aab84;
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
#else
void BrSub1003E3A0(void)
{
    Br72Env *pE = g_pBr72Env;
    int32_t  n;

    /* DEVIATION (memory safety): the original indexes 0x100AC520 with
     * 0x10AA2A0C and does not range-check it.  The port does; the shipped
     * selector never leaves 0..3. */
    n = 0;
    if (pE->aAC520 != NULL &&
        pE->nAA2A0C >= 0 && pE->nAA2A0C < pE->cAC520) {
        n = pE->aAC520[pE->nAA2A0C];
    }
    pE->nB4E1D0 = n;

    /* 0x1003E3B3 -- a dec-chain, i.e. 1/2/3 with everything else defaulting. */
    if (n == 1) {
        pE->pB4E1D4 = pE->pB4DFD8;
    } else if (n == 2) {
        pE->pB4E1D4 = pE->pB4E080;
    } else if (n == 3) {
        pE->pB4E1D4 = pE->pB4E128;
    } else {
        pE->pB4E1D4 = pE->pB4DF30;
    }

    /* 0x1003E3EA -- four "is zero" flags.  The original loads all four values
     * first and interleaves the tests; the results are independent. */
    pE->nAA2A1C = (pE->nB4E1E0 == 0);
    pE->nAA2A20 = (pE->nB4E1D8 == 0);
    pE->nAA2A24 = (pE->nB4E1DC == 0);
    pE->nAA2A28 = (pE->nB4E7A0 == 0);

    /* 0x1003E435 -- strlen + rep movs of len+1 bytes: a plain strcpy, and as
     * unbounded as the original.
     * DEVIATION (memory safety): bounded to the destination's room. */
    if (pE->pszB4E1E4 != NULL) {
        size_t cb = strlen(pE->pszB4E1E4) + 1u;
        if (cb > sizeof(pE->szA9CDF0)) {
            cb = sizeof(pE->szA9CDF0);
        }
        memcpy(pE->szA9CDF0, pE->pszB4E1E4, cb);
        pE->szA9CDF0[sizeof(pE->szA9CDF0) - 1u] = '\0';
    }

    if (pE->pfn1003E2C0 != NULL) {
        pE->pfn1003E2C0();
    }

    /* 0x1003E45F -- republish the twelve-dword config block. */
    pE->n0AC648 = pE->cfgB4E710.nB4E710;
    pE->nAA2A00 = pE->cfgB4E710.nB4E714;
    pE->nAA2A08 = pE->cfgB4E710.nB4E718;
    pE->n0AC64C = pE->cfgB4E710.nB4E71C;
    pE->n0AC650 = pE->cfgB4E710.nB4E720;
    pE->n0AC654 = pE->cfgB4E710.nB4E724;

    pE->nAA2A0C = pE->cfgB4E710.nB4E728;
    if (pE->cfgB4E710.nB4E728 == 1) {
        /* GOTCHA: 1 is not representable -- it is promoted to 2 here. */
        pE->nAA2A0C = 2;
    }

    pE->n0AC658 = pE->cfgB4E710.nB4E72C;
    pE->n0AC65C = pE->cfgB4E710.nB4E738;
    /* GOTCHA: a WORD-wide OR of the LOW HALF of a dword. */
    pE->w0AB3E4 = (uint16_t)(pE->w0AB3E4 | (uint16_t)pE->cfgB4E710.nB4E730);
    pE->nAA2A10 = pE->cfgB4E710.nB4E730;
    pE->n0AB3EC = pE->n0AB3EC | pE->cfgB4E710.nB4E734;
    pE->nAA2A14 = pE->cfgB4E710.nB4E734;
    pE->nAA2A18 = pE->cfgB4E710.nB4E73C;
}
#endif
