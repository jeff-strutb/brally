/* br_chain.c -- menus.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>


/* BrOperatorDelete: prototype in br_funcs.h */

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


/* Hand-matched from disassembly: 0x10058C70
 * fastcall: pointer arrives in ecx, five consecutive dwords zeroed, ret. */

/* WHAT IT DOES: the constructor of the bounds-tree Node (0x14 bytes) that
 * Ctl58D40::Rebuild (0x10058D40) news once per table row: all five fields
 * zeroed, among them the child link at +0x10 that BrChainFreeRec_10058C90
 * above walks.  A C++ constructor, written as __fastcall (this in ecx). */
/* @implements 0x10058C70 glide FUN_10058c70 */
int *__fastcall FUN_10058c70(int *p)
{
  p[0] = 0;
  p[1] = 0;
  p[2] = 0;
  p[3] = 0;
  p[4] = 0;
  return p;
}
