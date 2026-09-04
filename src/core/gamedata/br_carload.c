/* br_carload.c -- gamedata: loading one car's .rca record off the disc.
 *
 * Builds "cars/" + the car's name + ".rca", reads the whole file into the
 * caller's buffer, checks the "RCar" magic and hands the record to the
 * fixup pass. Filed out of slice4_53.c.
 *
 * The portable reader for the same record -- the same 340 bytes at glide
 * 0x10030DE0 -- is br_cardata.c; see br_cardata.h for the trail from the
 * disc to body+0x1DC.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice4_53.h"
#include "slice2_20.h"      /* BrFileReadInto, BrRcaLoadCar */

#include <stdio.h>
#include <string.h>

/* 0x10037740 */
/* WHAT IT DOES: loads one car's model and data out of the game's .rca
 * archive into the given buffer. */
/* @implements 0x10037740 d3d BrSub10037740 */
#ifdef BR_MATCHING_BUILD
/* The original is the full path-building loader the port folded into
 * BrRcaLoadCar: special-buffer gate, per-arm hook call (cross-jumped),
 * strcpy/strcat path assembly, magic check via the imported memcmp, the
 * (bug-for-bug) self-referential failure sprintf into BrLogPrint, and the
 * flag save/restore around it all. */
extern int   DAT_10ac67a4;
extern int   DAT_10ac67c0;
extern int   DAT_100b8498;
extern char  DAT_100bcdd0;          /* the special car buffer */
extern char  DAT_100b7900[];        /* base path   */
extern char *DAT_100b7d00[];        /* per-car names */
extern char  DAT_100aa310[];        /* extension   */
extern char  DAT_100aa308[];        /* magic bytes */
extern char  DAT_100aa2f4[];        /* failure format */
extern int   FUN_1005a080(int idx, int flag);
extern void  BrSub10030770(void *pCar);   /* glide 0x10030770 */
extern void  BrLogPrint(const void *p);
/* The original calls the /MD import (FF 15) -- go through the import
 * slot explicitly; string.h's decl is not dllimport for the intrinsics. */
extern int (__cdecl *_imp__memcmp)(const void *, const void *, unsigned int);
#define BR_MEMCMP_IMP (*_imp__memcmp)

void BrSub10037740(void *pCar, void *pArg)
{
    int  saved;
    char szMsg[0x100];
    char szPath[0x400];
    int  idx = (int)pArg;

    DAT_10ac67a4 = idx;
    if (pCar != (void *)&DAT_100bcdd0) {
        saved = DAT_100b8498;
        if (DAT_100b8498 == 0)
            DAT_100b8498 = 1;
        FUN_1005a080(idx, 0);
    } else {
        FUN_1005a080(idx, 1);
    }

    DAT_10ac67c0 = 0;
    strcpy(szPath, DAT_100b7900);
    strcat(szPath, DAT_100b7d00[idx]);
    strcat(szPath, DAT_100aa310);

    BrFileReadInto(pCar, szPath, -1);

    if (BR_MEMCMP_IMP(pCar, DAT_100aa308, 4) != 0) {
        sprintf(szMsg, DAT_100aa2f4, szMsg);
        BrLogPrint(szMsg);
    }

    BrSub10030770(pCar);

    if (pCar != (void *)&DAT_100bcdd0)
        DAT_100b8498 = saved;
}
#else
/* WHAT IT DOES: the port spelling of the same load -- fetch car iCar's .rca
 * record into the caller's buffer. The path building, magic check and fixup
 * are folded into the portable reader instead of being spelled out here. */
/* @implements 0x10037740 d3d BrSub10037740 */
void BrSub10037740(void *pCar, void *pArg)
{
    /* DEVIATION: pArg is declared void* by slice2_19 but is an integer index
     * in the original.  DEVIATION: cbDest is slice2_20's port-only bound;
     * the original has none.  0x15F88 is the stride its only caller
     * (0x10035520) uses to compute pCar, so it is the true extent. */
    BrRcaLoadCar(pCar, (size_t)BR_RCA_CAR_STRIDE, (int)(intptr_t)pArg);
}
#endif
