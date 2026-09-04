/* br_sndslot.c -- audio.
 *
 * The bank-and-slot accessors: given a bank index and a slot index they look
 * the sound's handle out of the 0x12-dword-per-bank table and set its volume,
 * its panning, or stop it.  Each reports success when the sound system is not
 * running, so callers need no guard of their own.
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

#ifdef BR_MATCHING_BUILD

int FUN_1006b790(int, int);
extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;

/* WHAT IT DOES: the same bank-and-slot lookup, setting a sound's panning
 * rather than its volume. */
/* @implements 0x1006BAA0 glide FUN_1006baa0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006baa0(int param_1,int param_2,int param_3)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    return FUN_1006b790(DAT_100b55f8[param_2 + param_1 * 0x12], param_3) != 0;
  }
  return 1;
}


extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
int BrSndBufSetVolume(int, int);
int FUN_1006b6e0(int, int, int);

/* WHAT IT DOES: set a sound's volume from a float, for the banks whose slots
 * hold a PAIR of handles -- it doubles the slot index and truncates the level
 * to an integer before handing both to FUN_1006b6e0 below. The doubling is
 * the same stride FUN_1006bb10 uses; the single-handle callers reach
 * FUN_1006b6e0 directly. */
/* @implements 0x1006B6C0 glide BrSndSetVolumePairF */

int BrSndSetVolumePairF(int param_1,int param_2,float param_3)

{
  return FUN_1006b6e0(param_1,param_2 * 2,(int)param_3);
}

/* WHAT IT DOES: set the volume of one sound in a two-dimensional bank-and-
 * slot table. Reports success without doing anything when the sound system
 * is not running, so callers need no guard of their own -- the same shape as
 * its two siblings below. */
/* @implements 0x1006B6E0 glide FUN_1006b6e0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006b6e0(int param_1,int param_2,int param_3)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    return BrSndBufSetVolume(DAT_100b55f8[param_2 + param_1 * 0x12], param_3) != 0;
  }
  return 1;
}


int FUN_1006b4c0(int);
extern int DAT_100b55f8[];
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;

/* WHAT IT DOES: the same bank-and-slot lookup, stopping a sound. GOTCHA: its
 * table stride is DIFFERENT from its two siblings -- the second index is
 * doubled -- so this addresses a pair of handles per slot where they address
 * one. */
/* @implements 0x1006BB10 glide FUN_1006bb10 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1006bb10(int param_1,int param_2)

{
  int iVar1;

  if ((((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) &&
     (iVar1 = DAT_100b55f8[param_1 * 0x12 + param_2 * 2], iVar1 != 0)) {
    return FUN_1006b4c0(iVar1) == 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
