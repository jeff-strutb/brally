/* br_vt55.c -- drawing.
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

extern funcptr PTR_FUN_10077750;

/* WHAT IT DOES: C++ scalar deleting destructor for the 0x10077750-vtable object: run the
 * destructor body, then operator delete if bit 0 of the flags is set. thiscall, spelled
 * as __fastcall with an unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10055A10 glide BrVt55A10DeleteDtor */

void * __fastcall BrVt55A10DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  BrVtInit55A30((int *)param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}


/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_10077750 (fastcall). */
/* @implements 0x10055A30 glide BrVtInit55A30 */

int __fastcall BrVtInit55A30(int *param_1)

{
  *param_1 = &PTR_FUN_10077750;
  return;
}

#endif /* BR_MATCHING_BUILD */
