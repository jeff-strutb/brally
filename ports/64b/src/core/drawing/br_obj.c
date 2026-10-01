/* br_obj.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_13.h"   /* br_globals: its objects */
#include <stdint.h>



/* ==========================================================================
 * 0x10008F90 (glide)  BrObjSelCycle
 * ========================================================================== */

/* The scene-DL selection state (see br_scenedl.c for the list's producer). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* pending cycle step, consumed here    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* current selected object index        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* index count (wrap bound)             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* sorted-list entry count              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* sorted object index list             */

/* WHAT IT DOES: applies a pending selection step, wrapping at both ends,
 * and keeps stepping until it lands on index 0 or on an index present in
 * the sorted object list; then clears the pending step. */
/* @implements 0x10008F90 glide BrObjSelCycle */
void BrObjSelCycle(void)
{
    int       i;
    uint16_t *p;

    if (DAT_10273308 != 0) {
        for (;;) {
            DAT_10396ea8 = DAT_10396ea8 + DAT_10273308;
            if (DAT_10396ea8 >= DAT_106eed3c) {
                DAT_10396ea8 = 0;
            }
            if (DAT_10396ea8 < 0) {
                DAT_10396ea8 = DAT_106eed3c - 1;
            }
            if (DAT_10396ea8 == 0) break;
            i = 0;
            if (0 < DAT_1035fb9c) {
                p = DAT_1035e710;
                do {
                    if (DAT_10396ea8 == *p) goto LAB_selDone;
                    i = i + 1;
                    p = p + 1;
                } while (i < DAT_1035fb9c);
            }
        }
LAB_selDone: ;
        DAT_10273308 = 0;
    }
}

