/* br_car.c -- startup.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>



/* 0x10035520 */
/* WHAT IT DOES: fills in one of the game's car slots. Beware the flag it is
 * given: a non-zero flag means DO NOT load, in which case it only writes a
 * note to the log; a zero flag is the one that actually loads the car. Either
 * way the slot ends up pointing at the caller's data. */
/* @implements 0x10035520 d3d BrCarSlotLoad */
/* Three args; the car-record table 0x100BCDD0 and the slot-pointer array
 * 0x106ED5E8 are globals, and the record address is recomputed for the
 * second call (/Od compiles each expression literally -- no pCar local). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100BCDD0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                          /* 0x106ED5E8 */

void BrCarSlotLoad(int i, void *pArg, int flag)
{
    /* GOTCHA: flag != 0 means "do not load", not "load". */
    if (flag == 0)
        BrSub10037740((unsigned char *)g_hudSpriteTable + i * 0x15F88, pArg);
    else
        BrLogPrint("LoadCar()");

    BrSub1003551B();
    g_apBr6ED5E8[i] = pArg;
}

