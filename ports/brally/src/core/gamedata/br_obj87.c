/* br_obj87.c -- gamedata.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_vtables.h"
#include "br_podarc.h"
#include <stdint.h>
#include <string.h>


/* operator_delete: prototype in br_funcs.h */
/* BrObj87Dtor: prototype in br_funcs.h */

/* WHAT IT DOES: C++ scalar deleting destructor: run the destructor body (BrObj87Dtor), then
 * operator delete if bit 0 of the flags is set. thiscall, spelled as __fastcall with an
 * unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x100087A0 glide BrObj87A0DeleteDtor */

void * __fastcall BrObj87A0DeleteDtor(void *param_1,unsigned char param_2)
{
  BrObj87Dtor(param_1);
  if ((param_2 & 1) != 0) {
    BrOperatorDelete(param_1);
  }
  return param_1;
}

/* The object's vtable, at 0x10077150.  Only its address is used here. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x10077150 */
/* BrPodIdentity: prototype in br_funcs.h */
/* BrSub10008D60: prototype in br_funcs.h */

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
    BrPodArc *p = (BrPodArc *)pThis;
    BrPodIdentity((unsigned char *)&p->sub04);
    p->pVtbl = g_brVtbl_10077150;
    p->aEntries = 0;
    p->pFile = 0;
    p->cbDir = 0;
    memset(p->magic, 0, 0x10);         /* magic, cEntries, offDir */
    memset(p->szName, 0, 0x400);
    return pThis;
}

/* WHAT IT DOES: C++ destructor body for the same object -- puts the class's
 * own vtable back (as a destructor does before tearing down members) and
 * destroys the sub-object at +4.  thiscall, spelled as __fastcall. */
/* @implements 0x100087C0 glide BrObj87Dtor */
void __fastcall BrObj87Dtor(void *pThis)
{
    BrPodArc *p = (BrPodArc *)pThis;
    p->pVtbl = g_brVtbl_10077150;
    BrPodNop();
}

