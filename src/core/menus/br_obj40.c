/* br_obj40.c -- menus.
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

int operator_delete();
int __fastcall FUN_10040d10(void *pThis);

/* WHAT IT DOES: C++ scalar deleting destructor: run the destructor body (FUN_10040d10), then
 * operator delete if bit 0 of the flags is set. thiscall, spelled as __fastcall with an
 * unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x10040CF0 glide BrObj40CF0DeleteDtor */

void * __fastcall BrObj40CF0DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  FUN_10040d10(param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}

#endif /* BR_MATCHING_BUILD */
