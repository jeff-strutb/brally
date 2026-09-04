/* br_chain.c -- menus.
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

void BrOperatorDelete(void *p);

/* WHAT IT DOES: thiscall recursive teardown of the +0x10 chain: free the
 * child's own chain first, delete the child, clear the link. */
/* @implements 0x10058C90 glide BrChainFreeRec_10058C90 */

void __fastcall BrChainFreeRec_10058C90(int param_1)
{
  int pvVar1;

  pvVar1 = *(int *)(param_1 + 0x10);
  if (pvVar1 != 0) {
    BrChainFreeRec_10058C90(pvVar1);
    BrOperatorDelete((void *)pvVar1);
    *(int *)(param_1 + 0x10) = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
