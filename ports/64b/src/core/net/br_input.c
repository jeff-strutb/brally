/* br_input.c -- net.
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
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: return 1 if any input source (joystick, keyboard, or net) is active. */
/* @implements 0x10037720 glide BrInputAnyActive */

int BrInputAnyActive(void)

{
  if (((((g_act0 == 0) && ((*(int *)&g_act1) == 0)) && ((*(int *)&g_act2) == 0)) && ((*(int *)&g_act3) == 0))
     && ((g_5BB4 != 0 ||
         ((((*(int *)&g_BrDikEdge[28]) == 0 && ((*(int *)&g_BrDikEdge[156]) == 0)) &&
          (((*(int *)&g_BrDikEdge[203]) == 0 && ((*(int *)&g_BrDikEdge[205]) == 0)))))))) {
    return 0;
  }
  return 1;
}

