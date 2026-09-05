/* br_obj87.c -- gamedata.
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
#include <string.h>

#ifdef BR_MATCHING_BUILD

int operator_delete();
int __fastcall FUN_100087c0(void *pThis);

/* WHAT IT DOES: C++ scalar deleting destructor: run the destructor body (FUN_100087c0), then
 * operator delete if bit 0 of the flags is set. thiscall, spelled as __fastcall with an
 * unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x100087A0 glide BrObj87A0DeleteDtor */

void * __fastcall BrObj87A0DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  FUN_100087c0(param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}

/* The object's vtable, at 0x10077150.  Only its address is used here. */
extern void *BrObj87Vtbl;                       /* 0x10077150 */
extern void * __fastcall BrPodIdentity(void *pThis);   /* 0x10008D50 */

/* WHAT IT DOES: C++ constructor for the same object 0x100087A0 destroys --
 * runs the sub-object constructor at +4, installs the vtable, clears the
 * five scalar fields and zeroes the 1 KB buffer at +0x20.  Returns this,
 * as a C++ constructor does.  thiscall with no stack arguments, spelled as
 * __fastcall (the BR_THISCALL1 idiom).
 *
 * THE 16-BYTE BLOCK AT +8 IS A memset, NOT FOUR STORES.  The original holds
 * `lea ecx,[esi+8]` and writes [ecx], [ecx+4], [ecx+8], [ecx+0xc]; four plain
 * `*(int *)(p + N) = 0` statements fold the address back into esi (59 B, 40
 * diffs) and so does a named `int *q = (int *)(p + 8); q[0..3] = 0;` (33
 * diffs) -- VC5 rematerialises a pointer that is only ever a constant offset.
 * `memset(p + 8, 0, 0x10)` is what keeps the base in a register, and it is
 * byte-exact.  The 1 KB clear at +0x20 is the same call inlined as
 * `rep stosd`. */
/* @implements 0x10008760 glide BrObj87Ctor */
void * __fastcall BrObj87Ctor(void *pThis)
{
    unsigned char *p = (unsigned char *)pThis;
    BrPodIdentity(p + 4);
    *(void **)p = (void *)&BrObj87Vtbl;
    *(int *)(p + 0x18)  = 0;
    *(int *)(p + 0x1C)  = 0;
    *(int *)(p + 0x420) = 0;
    memset(p + 8, 0, 0x10);
    memset(p + 0x20, 0, 0x400);
    return pThis;
}

#endif /* BR_MATCHING_BUILD */
