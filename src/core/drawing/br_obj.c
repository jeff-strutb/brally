/* br_obj.c -- drawing.
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


/* ==========================================================================
 * 0x10008F90 (glide)  BrObjSelCycle
 * ========================================================================== */

/* The scene-DL selection state (see br_scenedl.c for the list's producer). */
extern int      DAT_10273308;       /* pending cycle step, consumed here    */
extern int      DAT_10396ea8;       /* current selected object index        */
extern int      DAT_106eed3c;       /* index count (wrap bound)             */
extern int      DAT_1035fb9c;       /* sorted-list entry count              */
extern uint16_t DAT_1035e710[];     /* sorted object index list             */

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

#endif /* BR_MATCHING_BUILD */
