/* br_toggle.c -- menus.
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

extern int g_brAA28D8;

/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops. */
/* @implements 0x1003BFF0 glide BrToggleOnce_BFF0 */

int BrToggleOnce_BFF0(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}

extern int g_brAA28D8;


/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops (second instance). */
/* @implements 0x1003C050 glide BrToggleOnce_C050 */

int BrToggleOnce_C050(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
