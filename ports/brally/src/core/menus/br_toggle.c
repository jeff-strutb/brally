#include "br_ui.h"
/* br_toggle.c -- menus.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops. */
/* @implements 0x1003BFF0 glide BrToggleOnce_BFF0 */

int BrToggleOnce_BFF0(BrUiCtl_ *param_1)

{
  if (g_5C30 == 0) {
    g_5C30 = 1;
    (*(unsigned int *)&param_1->aText[0].f420) = (unsigned int)((*(int *)&param_1->aText[0].f420) == 0);
  }
  return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops (second instance). */
/* @implements 0x1003C050 glide BrToggleOnce_C050 */

int BrToggleOnce_C050(BrUiCtl_ *param_1)

{
  if (g_5C30 == 0) {
    g_5C30 = 1;
    (*(unsigned int *)&param_1->aText[0].f420) = (unsigned int)((*(int *)&param_1->aText[0].f420) == 0);
  }
  return 1;
}

