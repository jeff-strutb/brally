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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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

#ifdef BR_MATCHING_BUILD
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
#endif
#include "slice3_32.h"
#ifdef BR_MATCHING_BUILD
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
#endif

/* WHAT IT DOES: draws a numbered picture at an outright screen position,
 * without any menu row being involved. It passes on the caller's number as the
 * picture's identity rather than the one filed in the table -- in the shipped
 * table those always agree. */
/* @implements 0x100479D0 d3d BrUiDrawIndex_100479D0 */
#ifdef BR_MATCHING_BUILD
/* Original is __stdcall(code, x, y) and indexes the table at 0x100AB568 by
 * absolute address (`lea eax,[ecx+ecx*2]/shl eax,3` then
 * `[eax+0x100AB57C]` / `lea eax,[eax+0x100AB56C]`).  The port's extra
 * BrScrGlobals* is a pointer indirection the original does not have. */
extern unsigned char g_aBrUiSprite[];
int BR_STDCALL BrUiDrawIndex_100479D0(int32_t code, int32_t x, int32_t y)
{
    /* Spell *24 as *3 then <<3 so VC5 emits `lea`/`shl` rather than
     * a scaled-index `[r*8+table]` operand. */
    uint32_t off = (uint32_t)code * 3u;
    off <<= 3;
    BrSub1005F5A0(x, y, code,
                  g_aBrUiSprite + off + 4u,
                  *(int32_t *)(g_aBrUiSprite + off + 0x14u));
    return 1;
}
#else
int BrUiDrawIndex_100479D0(const BrScrGlobals *pG, int32_t code,
                           int32_t x, int32_t y)
{
    const BrScrRectEnt *pEnt = &pG->aAB568[code];

    /* GOTCHA: the id handed on is the CALLER'S index, not pEnt->f00. In the
     * shipped table the two are equal for every entry. */
    BrSub1005F5A0(x, y, code, &pEnt->rc, pEnt->f14);
    return 1;
}
#endif

/* WHAT IT DOES: says how far along its slide an element should be after a
 * given number of milliseconds. The distance grows with the square of the time,
 * so the element starts slowly and speeds up rather than moving evenly. */
/* @implements 0x10047CE0 d3d BrUiTweenCurve_10047CE0 */
#ifdef BR_MATCHING_BUILD
/* thiscall + one stack arg (`ret 4`). Sequential `x *= 0.5f; x *= 1e-3f`
 * keeps two `fmul dword [const]` -- a single expression folds them. */
typedef struct BrUiTwCurve {
    unsigned char pad[0x3824];
    float twrate;
} BrUiTwCurve;

float __fastcall BrUiTweenCurve_10047CE0(BrUiTwCurve *p, int _edx, int n)
{
    float x;
    n = n * n;
    x = (float)n;
    x *= p->twrate;
    x *= 0.5f;
    x *= 1.0e-3f;
    return x;
}
#else
float BrUiTweenCurve_10047CE0(const BrUiObj *pObj, int32_t n)
{
    /* imul n, n -- a 32-bit signed multiply that WRAPS; done in unsigned so
     * the port has no signed-overflow UB. */
    uint32_t uSq = (uint32_t)n * (uint32_t)n;
    int32_t  nSq = (uSq & 0x80000000u) ? -(int32_t)(0u - uSq) : (int32_t)uSq;

    /* 0x1008F678 = 0.5f and 0x1008F67C = 0.001f, read out of .rdata. */
    double d = (double)nSq;
    d *= (double)BrScrLdF(pObj, BR_SCR_UI_TWRATE);
    d *= (double)0.5f;
    d *= (double)1.0e-3f;

    /* The original leaves the product in st(0) at the x87's working
     * precision, which is 53-bit (CRT control word 0x027F -- CONVENTIONS.md)
     * and not the 80-bit this note used to claim. The `double` chain above is
     * therefore an exact model of it. The one remaining departure is the
     * narrowing to float on return, which is real: the caller sees st(0)
     * unrounded. */
    return (float)d;
}
#endif

/* WHAT IT DOES: places a menu element: tells it which screen owns it, where it
 * sits, three separate sets of behaviour flags, and which picture it starts
 * with -- that last one being written into two fields at once, the live picture
 * and the one to fall back to. */
/* @implements 0x10047FB0 d3d BrUiInit_10047FB0 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall / ret 0x20. BR_THISCALL1 is 1-arg only; a dummy edx
 * slot keeps pPhase on the stack (no xor edx,edx — the param is unused). */
void __fastcall BrUiInit_10047FB0(BrUiObj *pObj, void *_edx,
                                   BrPhaseFull *pPhase, float f3C, float f40,
                                   uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                                   uint32_t n2968, int16_t wCode)
{
    unsigned char *p = (unsigned char *)pObj;

    (void)_edx;
    *(BrPhaseFull **)(void *)(p + 0x2AE8) = pPhase;
    *(uint32_t *)(void *)(p + 0x1C) |= nOr1C;
    *(uint32_t *)(void *)(p + 0x24) |= nOr24;
    *(uint32_t *)(void *)(p + 0x28) |= nOr28;
    *(uint32_t *)(void *)(p + 0x2968) = n2968;
    /* Mention f3C first so it hoists into edx; the wCode load clobbers
     * eax, so f40 (eax) stores first and f3C (edx) after — orig order. */
    *(float *)(void *)(p + 0x3C) = f3C;
    *(float *)(void *)(p + 0x40) = f40;
    *(uint16_t *)(void *)(p + 0x2A40) = (uint16_t)wCode;
    *(uint16_t *)(void *)(p + 0x1E20C) = (uint16_t)wCode;
}
#else
void BrUiInit_10047FB0(BrUiObj *pObj, BrPhaseFull *pPhase,
                       float f3C, float f40,
                       uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                       uint32_t n2968, int16_t wCode)
{
    BrScrStSlot(pObj, BR_SCR_SLOT_PHASE, pPhase);
    BrScrSt32(pObj, BR_UI_OFF_FLAGS, BrScrLd32(pObj, BR_UI_OFF_FLAGS) | nOr1C);
    BrScrSt32(pObj, BR_SCR_UI_FLAGS24,
              BrScrLd32(pObj, BR_SCR_UI_FLAGS24) | nOr24);
    BrScrSt32(pObj, BR_SCR_UI_FLAGS28,
              BrScrLd32(pObj, BR_SCR_UI_FLAGS28) | nOr28);
    BrScrSt32(pObj, BR_SCR_UI_F2968, n2968);
    BrScrStF(pObj, BR_UI_OFF_F40, f40);
    BrScrStF(pObj, BR_UI_OFF_F3C, f3C);
    /* GOTCHA: one word, two destinations. */
    BrScrSt16(pObj, BR_UI_OFF_W2A40, (uint16_t)wCode);
    BrScrSt16(pObj, BR_UI_OFF_W1E20C, (uint16_t)wCode);
}
#endif

/* WHAT IT DOES: tears a page down, and frees its memory too if the caller asks
 * for that. It hands the page's address back afterwards even when it has just
 * been freed -- that is what a C++ deleting destructor compiles to. */
/* @implements 0x100484C0 d3d BrUiPageDelete_100484C0 */
#ifdef BR_MATCHING_BUILD
/* C++ scalar deleting destructor: thiscall via __fastcall with unused EDX
 * (BR_THISCALL1 idiom, as BrVt55A10DeleteDtor); byte-typed flags give the
 * `test byte [esp+8],1`; the dtor body is thiscall too (ECX copy-prop). */
void __fastcall FUN_10041930(void *pThis);
void *__fastcall BrUiPageDelete_100484C0(BrUiPage *pThis, int _edx_unused,
                                         unsigned char nFlags)
{
    FUN_10041930(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}
#else
void *BrUiPageDelete_100484C0(BrUiPage *pThis, int32_t nFlags)
{
    BrSub100484E0(pThis);
    if (nFlags & 1)
        BrOperatorDelete(pThis);
    /* The MSVC scalar deleting destructor returns `this` even after freeing
     * it. Preserved; callers in this range discard it. */
    return pThis;
}
#endif

/* WHAT IT DOES: tears a menu screen down and frees it if asked, returning its
 * address either way -- the standard C++ deleting destructor shape. */
/* @implements 0x10048850 d3d BrPhaseDelete_10048850 */
#ifdef BR_MATCHING_BUILD
void __fastcall FUN_10041cc0(void *pThis);
void *__fastcall BrPhaseDelete_10048850(BrPhaseFull *pThis, int _edx_unused,
                                        unsigned char nFlags)
{
    FUN_10041cc0(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}
#else
void *BrPhaseDelete_10048850(BrPhaseFull *pThis, int32_t nFlags)
{
    BrPhaseDtor_10048870(pThis);
    if (nFlags & 1)
        BrOperatorDelete(pThis);
    return pThis;
}
#endif

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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int operator_delete();
int __fastcall FUN_10040d10(void *pThis);
typedef int (*funcptr)();
extern funcptr DAT_106b7ab8;
extern int DAT_10ac5d84;
int FUN_100014e0();
extern funcptr PTR_FUN_100776C0;
extern funcptr PTR_FUN_100776c0;

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_100776C0 (fastcall). */
/* @implements 0x10041930 glide BrVtInit41930 */
/* @n64 0x8021C6E4 located */

int __fastcall BrVtInit41930(int *param_1)

{
  *param_1 = &PTR_FUN_100776c0;
  return;
}

/* WHAT IT DOES: menu teardown callback: release a resource and invoke the cleanup funcptr. */
/* @implements 0x10041DB0 glide BrMenuCallback41DB0 */

int BrMenuCallback41DB0(void)

{
  FUN_100014e0(DAT_10ac5d84);
                    
                    
  (*DAT_106b7ab8)();
  return;
}


#endif /* BR_MATCHING_BUILD */
