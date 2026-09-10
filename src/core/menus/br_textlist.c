/* br_textlist.c -- menus: the list of text rows, the widget behind the game's
 * scrolling menus and high-score tables. Its destructor body, the scalar
 * deleting destructor the compiler pairs with it, and the three-word stub
 * that sits between them in the original.
 *
 * Filed out of slice3_39.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  BrTextListSetBlob
 * (0x10055020) went byte-exact on 2026-09-10 once it was declared thiscall
 * and came over then; BrTextListInit (0x10054610) is not byte-exact yet and
 * stayed behind in the slice.
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

/* =====================================================================
 * 0x1005C200 -- store an opaque blob against an item slot
 * ===================================================================== */

/* WHAT IT DOES: attaches a lump of arbitrary data to one row of a list --
 * whatever the menu wants to remember alongside the visible text. Passing -1
 * means the row just added. Beware a real bug that is kept: the memory is
 * only allocated the first time a slot is used, so storing a bigger lump
 * into a slot that already has a smaller one writes past the end of it.
 *
 * THISCALL, and that was the whole residue: `mov ebp,ecx` at +0x0B takes
 * pList out of ecx and the function ends `ret 0xc`, three dwords cleaned by
 * the callee.  Spelled cdecl it read pList off the stack like the other
 * three, which cost the register the original keeps the list in and left a
 * `lea` where the original re-indexes.  Wrapping every argument after `this`
 * (br_match.h: __fastcall SKIPS a struct when handing out registers, so a
 * bare second argument would take edx and the epilogue would clean 8) makes
 * it byte-exact. */
/* @implements 0x1005C200 d3d BrTextListSetBlob */
int32_t BR_THISCALL1 BrTextListSetBlob(BrTextList *pList, BrBlobSrcArg pSrc,
                                       BrBlobSizeArg size, BrBlobIndexArg index)
{
    BrTextBlob *pSlot;
    int32_t     i = index.v;

    if (i == -1) {
        i = (int32_t)(uint16_t)pList->count - 1;
        if (i < 0) {
            i = 0;
        }
    }

    pSlot = &pList->aBlobs[i];

    if (pSlot->p == NULL) {
        pSlot->p = BrOperatorNew(size.v);
    }
    /* GOTCHA: no realloc on a size increase -- the original copies `size`
     * bytes into whatever the first call allocated. */
    memcpy(pSlot->p, pSrc.v, size.v);
    pSlot->size = size.v;

    return 1;
}
