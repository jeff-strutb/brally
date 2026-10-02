/* br_uiscreen.c -- menus: screen and element plumbing -- draw a numbered
 * picture at a position, the slide curve, element placement, the page and
 * screen deleting destructors, a screen's own behaviour slot, and two small
 * vtable/teardown callbacks.
 *
 * Filed out of slice3_32.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice3_32.c -- BRD3D.dll 0x10047930-0x1004A260, a later pass.
 *
 * See port/include/slice3_32.h for the model (three classes, the vtable
 * overlap that pins down every `this`, and the naming rationale).
 *
 * Not ported, and why -- also repeated in the report:
 *   0x100491B0 (2659 bytes)  SEH frame + a construction sequence whose object
 *                            layouts this packet does not establish.
 *   0x10049C20 (794 bytes)   ditto
 *   0x10049F40 (800 bytes)   ditto
 *   0x1004A260 (800 bytes)   ditto
 * 0x100484E0 sits inside the address range but was not in the packet listing,
 * so it is imported (BrSub100484E0) rather than guessed at.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_uispr.h"   /* br_globals: its objects */
#include <string.h>

/* --- DUPLICATE SYMBOL, not duplicate work (host link only) ---------------
 * 0x10048470 is declared by slice3_32.h over `BrUiPage` and by slice6_73.h
 * over `BrUiPage_`, under ONE name. The two structs are not the same shape --
 * slice3_32's has the pfn0C and +0x16 fields, and until br_uivt.c existed
 * slice6_73's did not -- so binding one model's caller to the other model's
 * body wrote every field from +0x00C onward at the wrong offset and ran eight
 * bytes past the end of what BR73_ALLOC asks for.
 *
 * br_uivt.c owns the BrUiPage_ body. Under BR_HOST_LINK this module's
 * BrUiPage layout body is renamed so both exist and each caller reaches the
 * one built for its own struct. This file's internal calls are renamed with
 * it, so the module stays self-consistent; its own test binary links this .o
 * alone, without BR_HOST_LINK, and is unaffected.
 *
 * This is a stopgap, exactly like the one at the top of slice6_73.c. The real
 * fix is to merge BrUiPage and BrUiPage_ the way br_phase.h merged the three
 * views of the phase object; the two are already field-for-field identical,
 * so the merge is mechanical and only the naming differs.
 * ------------------------------------------------------------------------ */
#ifdef BR_HOST_LINK
#define BrUiPageCtor_10048470 BrUiPageCtor_10048470_scr32
#endif

/* slice3_32.h adds a leading BrScrGlobals* that the original does not have.
 * Hide that prototype so the matching body can be the real __stdcall(code,x,y). */
#define BrUiDrawIndex_100479D0 BrUiDrawIndex_100479D0_port
/* Original is thiscall + one stack arg (`ret 4`); the port is cdecl. */
#define BrUiTweenCurve_10047CE0 BrUiTweenCurve_10047CE0_port
#define BrUiTweenBegin_10047CB0 BrUiTweenBegin_10047CB0_port
#define BrUiTweenReset_10047D10 BrUiTweenReset_10047D10_port
#define BrUiDrawCode_10047930   BrUiDrawCode_10047930_port
#define BrUiDrawCodeRect_10047980 BrUiDrawCodeRect_10047980_port
#define BrUiStepCode_10047A10   BrUiStepCode_10047A10_port
#define BrUiTweenStep_10047D30  BrUiTweenStep_10047D30_port
#define BrUiInit_10047FB0       BrUiInit_10047FB0_port
#define BrUiItemInit_10047EB0   BrUiItemInit_10047EB0_port
#include "slice3_32.h"
#undef BrUiDrawIndex_100479D0
#undef BrUiTweenCurve_10047CE0
#undef BrUiTweenBegin_10047CB0
#undef BrUiTweenReset_10047D10
#undef BrUiDrawCode_10047930
#undef BrUiDrawCodeRect_10047980
#undef BrUiStepCode_10047A10
#undef BrUiTweenStep_10047D30
#undef BrUiInit_10047FB0
#undef BrUiItemInit_10047EB0

/* WHAT IT DOES: draws a numbered picture at an outright screen position,
 * without any menu row being involved. It passes on the caller's number as the
 * picture's identity rather than the one filed in the table -- in the shipped
 * table those always agree. */
/* @implements 0x100479D0 d3d BrUiDrawIndex_100479D0 */
/* Original is __stdcall(code, x, y) and indexes the table at 0x100AB568 by
 * absolute address (`lea eax,[ecx+ecx*2]/shl eax,3` then
 * `[eax+0x100AB57C]` / `lea eax,[eax+0x100AB56C]`).  The port's extra
 * BrScrGlobals* is a pointer indirection the original does not have. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
int BR_STDCALL BrUiDrawIndex_100479D0(int32_t code, int32_t x, int32_t y)
{
    /* code * 24 bytes in the original: one 24-byte sprite record */
    BrSprFontDraw(x, y, code,
                  &g_aBrUiSprite[code].rect[0],
                  g_aBrUiSprite[code].fBlit);
    return 1;
}

/* WHAT IT DOES: says how far along its slide an element should be after a
 * given number of milliseconds. The distance grows with the square of the time,
 * so the element starts slowly and speeds up rather than moving evenly. */
/* @implements 0x10047CE0 d3d BrUiTweenCurve_10047CE0 */
/* thiscall + one stack arg (`ret 4`). Sequential `x *= 0.5f; x *= 1e-3f`
 * keeps two `fmul dword [const]` -- a single expression folds them. */
typedef struct BrUiTwCurve {
    unsigned char pad[0x3824];
    float twrate;
} BrUiTwCurve;

float __fastcall BrUiTweenCurve_10047CE0(BrUiTwCurve *p, int n)
{
    float x;
    n = n * n;
    x = (float)n;
    x *= p->twrate;
    x *= 0.5f;
    x *= 1.0e-3f;
    return x;
}

/* WHAT IT DOES: places a menu element: tells it which screen owns it, where it
 * sits, three separate sets of behaviour flags, and which picture it starts
 * with -- that last one being written into two fields at once, the live picture
 * and the one to fall back to. */
/* @implements 0x10047FB0 d3d BrUiInit_10047FB0 */
/* Orig is thiscall / ret 0x20. BR_THISCALL1 is 1-arg only; a dummy edx
 * slot keeps pPhase on the stack (no xor edx,edx: the param is unused). */
void __fastcall BrUiInit_10047FB0(BrUiObj *pObj,
                                   BrPhaseFull *pPhase, float f3C, float f40,
                                   uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                                   uint32_t n2968, int16_t wCode)
{
    unsigned char *p = (unsigned char *)pObj;
    *(BrPhaseFull **)(void *)(p + 0x2AE8) = pPhase;
    *(uint32_t *)(void *)(p + 0x1C) |= nOr1C;
    *(uint32_t *)(void *)(p + 0x24) |= nOr24;
    *(uint32_t *)(void *)(p + 0x28) |= nOr28;
    *(uint32_t *)(void *)(p + 0x2968) = n2968;
    /* Mention f3C first so it hoists into edx; the wCode load clobbers
     * eax, so f40 (eax) stores first and f3C (edx) after: orig order. */
    *(float *)(void *)(p + 0x3C) = f3C;
    *(float *)(void *)(p + 0x40) = f40;
    *(uint16_t *)(void *)(p + 0x2A40) = (uint16_t)wCode;
    *(uint16_t *)(void *)(p + 0x1E20C) = (uint16_t)wCode;
}

/* WHAT IT DOES: tears a page down, and frees its memory too if the caller asks
 * for that. It hands the page's address back afterwards even when it has just
 * been freed -- that is what a C++ deleting destructor compiles to. */
/* @implements 0x100484C0 d3d BrUiPageDelete_100484C0 */
/* C++ scalar deleting destructor: thiscall via __fastcall with unused EDX
 * (BR_THISCALL1 idiom, as BrVt55A10DeleteDtor); byte-typed flags give the
 * `test byte [esp+8],1`; the dtor body is thiscall too (ECX copy-prop). */
/* FUN_10041930: prototype in br_funcs.h */
void *__fastcall BrUiPageDelete_100484C0(BrUiPage *pThis,
                                         unsigned char nFlags)
{
    BrVtInit41930(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}

/* WHAT IT DOES: tears a menu screen down and frees it if asked, returning its
 * address either way -- the standard C++ deleting destructor shape. */
/* @implements 0x10048850 d3d BrPhaseDelete_10048850 */
/* FUN_10041cc0: prototype in br_funcs.h */
void *__fastcall BrPhaseDelete_10048850(BrPhaseFull *pThis,
                                        unsigned char nFlags)
{
    BrPhaseDtor_10048870(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}

/* WHAT IT DOES: calls one particular slot of a screen's own behaviour table
 * and always reports success. Nothing in this packet fills that slot, so what
 * the call actually does depends entirely on the screen. */
/* @implements 0x10041D00 glide BrPhaseFn_100488B0 */
int BR_THISCALL1 BrPhaseFn_100488B0(BrPhaseFull *pThis)
{
    /* vtable +0x20 -- a slot no function in this packet implements. */
    pThis->pVtbl->f20(pThis);
    return 1;
}

/* -- Ghidra-matched functions --------------------------- */
/* operator_delete: prototype in br_funcs.h */
/* FUN_10040d10: prototype in br_funcs.h */
typedef int (*funcptr)();
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100014e0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_100776C0 (fastcall). */
/* @implements 0x10041930 glide BrVtInit41930 */
/* @n64 0x8021C6E4 located */

int __fastcall BrVtInit41930(const void **param_1)

{
  *param_1 = &PTR_FUN_100776c0;
  return;
}

/* WHAT IT DOES: menu teardown callback: release a resource and invoke the cleanup funcptr. */
/* @implements 0x10041DB0 glide BrMenuCallback41DB0 */

int BrMenuCallback41DB0(void)

{
  BrGlideLfbWrite(DAT_10ac5d84);
                    
                    
  (*(*(funcptr *)&BrGlFlipHook))();
  return;
}


