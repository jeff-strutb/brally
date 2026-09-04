/* br_opt.c -- menus.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD


void BrOptSave(void)
{
    g_aBrB4E710[0]  = g_br0AC648;
    g_aBrB4E710[1]  = g_brAA2A00;
    g_aBrB4E710[2]  = g_brAA2A08;
    g_aBrB4E710[3]  = g_br0AC64C;
    g_aBrB4E710[4]  = g_br0AC650;
    g_aBrB4E710[5]  = g_br0AC654;
    g_aBrB4E710[6]  = g_brAA2A0C;
    g_aBrB4E710[7]  = g_br0AC658;
    g_aBrB4E710[8]  = g_brAA2A10;
    g_aBrB4E710[9]  = g_brAA2A14;
    g_aBrB4E710[10] = g_br0AC65C;
    g_aBrB4E710[11] = g_brAA2A18;
}

#endif /* BR_MATCHING_BUILD */
