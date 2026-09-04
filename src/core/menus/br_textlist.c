/* br_textlist.c -- menus: the list of text rows, the widget behind the game's
 * scrolling menus and high-score tables. Its destructor body, the scalar
 * deleting destructor the compiler pairs with it, and the three-word stub
 * that sits between them in the original.
 *
 * Filed out of slice3_39.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  BrTextListInit (0x10054610)
 * and BrTextListSetBlob (0x10055020) are not byte-exact yet and stayed
 * behind in the slice.
 *
 * The original banner follows.
 *
 * slice3_39.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * Packet 0x1005AE70 - 0x100607B0.  See slice3_39.h for the layout notes and
 * the list of functions that were deliberately left out.
 *
 * Every arithmetic width here is deliberate: the original accumulates string
 * widths in 16 bits and stores 16 bits, and the range tests on a character
 * are done on the SIGN-EXTENDED byte.  Both are reproduced literally.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; matching needs thiscall.  Rename the cdecl
 * declaration so the definition below can wear a different convention. */
#define BrTextBoxDeleteDtor BrTextBoxDeleteDtor_cdecl
#define BrTextBoxMeasureA  BrTextBoxMeasureA_cdecl
#define BrTextBoxMeasureB  BrTextBoxMeasureB_cdecl
#endif
#ifdef BR_MATCHING_BUILD
#define BrTextBoxInit BrTextBoxInit_port
#include "slice3_39.h"
#undef BrTextBoxInit
#else
#include "slice3_39.h"
#endif
#ifdef BR_MATCHING_BUILD
#undef BrTextBoxDeleteDtor
#undef BrTextBoxMeasureA
#undef BrTextBoxMeasureB
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int operator_delete();
int __fastcall BrObj54710Dtor(void *pThis);
/* Lives in src/core/menus/br_textbox.c; BrObj54710Dtor still takes its
 * address as the vector destructor's element dtor. */
int __fastcall BrVtInit53EE0(int *param_1);
int __stdcall FUN_100746c0(int,int,int,int);
typedef int (*funcptr)();
extern funcptr PTR_FUN_10077720;
extern int * DAT_10ac66e8;
extern int * DAT_10ac6720;
extern int * DAT_10ac6730;
extern funcptr PTR_FUN_100776F0;
extern funcptr PTR_FUN_100776f0;

/* WHAT IT DOES: C++ scalar deleting destructor: run the destructor body (BrObj54710Dtor), then
 * operator delete if bit 0 of the flags is set. thiscall, spelled as __fastcall with an
 * unused EDX slot (BR_THISCALL1 idiom). */
/* @implements 0x100546F0 glide BrObj546F0DeleteDtor */

void * __fastcall BrObj546F0DeleteDtor(void *param_1,int _edx_unused,unsigned char param_2)
{
  BrObj54710Dtor(param_1);
  if ((param_2 & 1) != 0) {
    operator_delete(param_1);
  }
  return param_1;
}

/* WHAT IT DOES: C++ destructor body for the 0x10077720-vtable object: reset the vtable, then
 * run the CRT vector-destructor iterator (0x100746C0) over the 100 x 0x438-byte elements at
 * +0x2C with BrVtInit53EE0 as the element destructor. thiscall spelled as BR_THISCALL1. */
/* @implements 0x10054710 glide BrObj54710Dtor */

int __fastcall BrObj54710Dtor(void *param_1)

{
  *(int *)param_1 = (int)&PTR_FUN_10077720;
  FUN_100746c0((int)param_1 + 0x2c,0x438,100,(int)BrVtInit53EE0);
  return;
}

/* WHAT IT DOES: stdcall stub taking three words and returning 0. */
/* @implements 0x10054600 glide BrRet0Std3_10054600 */

int __stdcall BrRet0Std3_10054600(int _pad_0,int _pad_1,int _pad_2)
{
  return 0;
}

#endif /* BR_MATCHING_BUILD */
