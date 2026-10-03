/* br_sub69.c -- racing.
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
/* FUN_10069a80: prototype in br_funcs.h */

/* WHAT IT DOES: forward a parameter to FUN_10069A80 with a fixed first argument. */
/* @implements 0x10069DC0 glide BrSub69DC0 */

int BrSub69DC0(int param_1)

{
  return BrGhostLoad(&(*(int *)&DAT_117a5f28),param_1);
}

