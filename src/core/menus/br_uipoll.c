/* br_uipoll.c -- menus: the per-row menu hooks the front end installs -- the
 * slide-to-setting hook at 0x1003E920 and the "ask the row's list where the
 * player moved to" poll family.
 *
 * Filed out of slice2_23.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice2_23.c -- BRD3D.dll 0x1003DC10-0x10040330, a later pass. See slice2_23.h.
 *
 * Constants below were read out of orig/BRD3D.dll rather than guessed:
 *   0x1008F660 == 8.0f, 0x1008F664 == -8.0f, the immediate 0x43020000 stored
 *   into item[0].F414 by 0x1003FA00 == 130.0f, the table at 0x100AB334 is 21
 *   records of 8 bytes whose second dword of the last record is exactly the
 *   global 0x100AB3D8, and the four record tables 0x10040040 searches hold
 *   120 / 134 / 134 / 10 records of 0x24 bytes.
 *
 * Two CRT routines the original calls are used directly instead of ported,
 * per the contract's "at or above 0x1007CC40 is statically linked MSVC CRT"
 * rule: 0x1008C000 is _itoa (called as _itoa(v, buf, 10)) and 0x1008C320 is
 * _stricmp -- CASE-INSENSITIVE, see br_stricmp_1008C320 below; this line used
 * to say strcmp and that was wrong at all three call sites in this file.
 * 0x1007C8A0 is __ftol and is reproduced locally as br23_ftol.
 *
 * Every string move in this range is an inlined `rep movsd` + `rep movsb`
 * over strlen+1 bytes, i.e. exactly strcpy, with no bound. strcpy is used.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_23.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * Matching build: direct globals and the thiscall bridge
 *
 * The port threads a BrUiGlobals pointer, but the original addresses every one
 * of these settings absolutely (`a1 f4 b3 0a 10` == `mov eax,[0x100AB3F4]`).
 * The addresses are the ones the BrUiGlobals field comments in slice2_23.h
 * already carry; the names follow the "XSLICE 0x..." convention used by
 * slice2_20.c.
 *
 * The nested widget at +0x3838 is dispatched __thiscall -- `this` in ecx and
 * the one int argument pushed and cleaned by the callee.  VC5's C compiler has
 * no __thiscall keyword; __fastcall puts the first REGISTER-ELIGIBLE argument
 * in ecx, and a struct is never register-eligible, so a 4-byte struct in
 * second position is forced back onto the stack.  Same trick, same reasoning
 * as BrSub10060260 in slice4_52.c -- see br_match.h.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
/* XSLICE 0x100AA010 */ extern int32_t  g_i0AA010;
/* XSLICE 0x10220B20 */ extern int32_t  g_i220B20;
/* XSLICE 0x100AB3D8 */ extern int32_t  g_i0AB3D8;
/* XSLICE 0x100AB3E0 */ extern void    *g_p0AB3E0;
/* XSLICE 0x100AB3F4 */ extern int32_t  g_i0AB3F4;
/* XSLICE 0x100AC65C */ extern int32_t  g_i0AC65C;
/* XSLICE 0x10AA2840 */ extern int32_t  g_iAA2840;
/* XSLICE 0x10AA2880 */ extern int32_t  g_iAA2880;
/* XSLICE 0x10AA28AC */ extern int32_t  g_iAA28AC;
/* XSLICE 0x10AA28D8 */ extern int32_t  g_iAA28D8;
/* XSLICE 0x10AA2A2C */ extern int32_t  g_iAA2A2C;
/* XSLICE 0x10AA2A30 */ extern int32_t  g_iAA2A30;
/* XSLICE 0x10AA2A34 */ extern int32_t  g_iAA2A34;
/* XSLICE 0x10B4E708 */ extern uint32_t g_uB4E708;
/* XSLICE 0x10B4E70C */ extern uint32_t g_uB4E70C;
/* XSLICE 0x10AA26E8 */ extern const int8_t  g_abAA26E8[];
/* XSLICE 0x10A9D068 */ extern const int16_t g_awA9D068[];
/* Glide addresses: 0x10037660 / 0x100376B0 / 0x100376E0 store these
 * absolutely (`xor eax,eax` / `mov eax,K` then `mov [DAT],r`). */
extern int32_t  DAT_100b3014;          /* g0B380C */
extern int32_t  DAT_10226e80;          /* g22B350 */
extern int32_t  DAT_10226e7c;          /* g22B34C */
extern int32_t  DAT_1007b324;          /* g094354 */
extern int32_t  DAT_1007b32c;          /* g09435C */
extern int32_t  DAT_1007b328;          /* g094358 */
extern int32_t  DAT_10b71530;          /* gB4E1D0 */
extern void    *DAT_10b71534;          /* gB4E1D4 */
extern int32_t  DAT_1007b320;          /* g094350 */
extern unsigned char DAT_10b71290[];   /* g_B4DF30 */
extern int16_t  DAT_10ac5b38;          /* gAA27E0 */
extern int32_t  DAT_10ac58f0;          /* gAA2598 */
extern int16_t  DAT_10ac5b3a;          /* gAA27E2 */
extern int32_t  DAT_10ac40a0;          /* gA9D010 */

typedef struct { int32_t v; } BrUiSelArg;
typedef int32_t(__fastcall *BrUiSelOfferFn)(BrUiObj *pThis, BrUiSelArg a);
typedef void(__fastcall *BrUiSelCommitFn)(BrUiObj *pThis, BrUiSelArg a);
typedef void (__fastcall *BrUiThis0)(void *pThis);

/* BrUiLdPtr is an extern in slice2_23.h, so VC5 cannot inline it and emits a
 * real call where the original has a bare `mov edx,[eax+0x3838]`.  Dereference
 * directly instead. */
#define BR23_SEL_VTBL(pObj_) \
    (*(const BrUiWidgetVtbl *const *)((pObj_) + BR_UI_OFF_SEL))

/* `pVt->f20(pSel, v)` spelled as the original's thiscall. */
#define BR23_SEL_OFFER(pObj_, r_, v_)                                        \
    do {                                                                     \
        BrUiObj              *pSel_ = (pObj_) + BR_UI_OFF_SEL;               \
        const BrUiWidgetVtbl *pVt_  = BR23_SEL_VTBL(pObj_);                  \
        BrUiSelArg            a_;                                            \
        a_.v = (v_);                                                         \
        (r_) = ((BrUiSelOfferFn)pVt_->f20)(pSel_, a_);                       \
    } while (0)
#endif /* BR_MATCHING_BUILD */

/* Layout facts the original's arithmetic depends on. */
typedef char br23_assert_cfgrec[(sizeof(BrCfgRec) == 0x24) ? 1 : -1];
typedef char br23_assert_item[
    (BR_UI_ITEM_OFF_I420 < BR_UI_ITEM_STRIDE) ? 1 : -1];

/* WHAT IT DOES: slides a menu row sideways to a position worked out from
 * one of the settings, so the row's marker sits at the place that setting
 * corresponds to. */
/* @implements 0x10037F40 glide BrUiFn1003E920 */
/* @implements 0x1003E920 d3d BrUiFn1003E920 */
int32_t BrUiFn1003E920(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    /* Orig: lea 11*g+0x3D, fild, fstp [pObj+0x3c]. BrUiStF is an extern CALL. */
    int32_t v = g_i0AC65C * 11 + 0x3D;
    (void)pG;
    *(float *)(pObj + BR_UI_OFF_F3C) = (float)v;
    return 1;
#else
    /* lea ecx,[eax+eax*4] ; lea edx,[eax+ecx*2+0x3D]  ->  11*a + 61 */
    int32_t v = pG->g0AC65C * 11 + 0x3D;
    BrUiStF(pObj, BR_UI_OFF_F3C, (float)v);
    return 1;
#endif
}

/* ==========================================================================
 * The poll family
 *
 * br23_sel_offer / br23_poll_store are copied from slice2_23.c verbatim:
 * they are the port bodies' helpers and the slice still needs its own.
 * ========================================================================== */

/* `sel->f20(sel, v)` on the nested widget at +0x3838. */
static int32_t br23_sel_offer(BrUiObj *pObj, int32_t v)
{
    BrUiObj              *pSel = pObj + BR_UI_OFF_SEL;
    const BrUiWidgetVtbl *pVt  = (const BrUiWidgetVtbl *)
                                     BrUiLdPtr(pObj, BR_UI_OFF_SEL);
    return pVt->f20(pSel, v);
}

/* The bare "offer the global, keep the answer if it is not negative" body
 * shared by 0x1003EAE0 / 0x1003EB60 / 0x1003EB90 / 0x1003EC80 / 0x1003ED10 /
 * 0x1003EDF0. Returns the value the original leaves in eax. */
static int32_t br23_poll_store(BrUiObj *pObj, int32_t *pVal)
{
    int32_t r = br23_sel_offer(pObj, *pVal);
    if (r >= 0) {
        *pVal = r;
    }
    return r;
}

/* WHAT IT DOES: asks the row's list which entry the player has moved to and
 * remembers it as the current selection, leaving the selection alone if the
 * list has no answer. Several near-identical hooks follow, differing only
 * in which setting they store into. */
/* @implements 0x1003EAE0 d3d BrUiPoll1003EAE0 */
int32_t BrUiPoll1003EAE0(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    BR23_SEL_OFFER(pObj, r, g_i0AB3F4);
    if (r >= 0) {
        g_i0AB3F4 = r;
    }
#else
    (void)br23_poll_store(pObj, &pG->g0AB3F4);
#endif
    return 1;
}

/* WHAT IT DOES: asks the list which entry the player has moved to and then
 * throws the answer away -- there is no store-back at all, so this hook only
 * has whatever effect the asking itself has. */
/* @implements 0x1003EBC0 d3d BrUiPoll1003EBC0 */
int32_t BrUiPoll1003EBC0(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    /* The answer is thrown away -- there is no store-back here. */
    BR23_SEL_OFFER(pObj, r, g_iAA2880);
    (void)r;
#else
    (void)br23_sel_offer(pObj, pG->gAA2880);
#endif
    return 1;
}

/* WHAT IT DOES: the same ask-and-remember, storing into yet another
 * setting. */
/* @implements 0x100382A0 glide BrUiPoll1003EC80 */
/* @implements 0x1003EC80 d3d BrUiPoll1003EC80 */
int32_t BrUiPoll1003EC80(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    BR23_SEL_OFFER(pObj, r, g_iAA2840);
    if (r >= 0)
        g_iAA2840 = r;
    return 1;
#else
    (void)br23_poll_store(pObj, &pG->gAA2840);
    return 1;
#endif
}

/* WHAT IT DOES: the same ask-and-remember, storing into yet another
 * setting. */
/* @implements 0x10038320 glide BrUiPoll1003EDF0 */
/* @implements 0x1003EDF0 d3d BrUiPoll1003EDF0 */
int32_t BrUiPoll1003EDF0(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    BR23_SEL_OFFER(pObj, r, g_iAA2A30);
    if (r >= 0)
        g_iAA2A30 = r;
    return 1;
#else
    (void)br23_poll_store(pObj, &pG->gAA2A30);
    return 1;
#endif
}

/* WHAT IT DOES: the same, storing into the setting that tracks which entry
 * of a per-player table is current. */
/* @implements 0x10038150 glide BrUiPoll1003EB60 */
/* @implements 0x1003EB60 d3d BrUiPoll1003EB60 */
int32_t BrUiPoll1003EB60(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    BR23_SEL_OFFER(pObj, r, g_iAA28AC);
    if (r >= 0)
        g_iAA28AC = r;
    return 1;
#else
    (void)br23_poll_store(pObj, &pG->gAA28AC);
    return 1;
#endif
}

/* WHAT IT DOES: the same, storing into a different setting again. */
/* @implements 0x10038180 glide BrUiPoll1003EB90 */
/* @implements 0x1003EB90 d3d BrUiPoll1003EB90 */
int32_t BrUiPoll1003EB90(BrUiObj *pObj, BrUiGlobals *pG)
{
#ifdef BR_MATCHING_BUILD
    int32_t r;
    (void)pG;
    BR23_SEL_OFFER(pObj, r, g_iAA2880);
    if (r >= 0)
        g_iAA2880 = r;
    return 1;
#else
    (void)br23_poll_store(pObj, &pG->gAA2880);
    return 1;
#endif
}

int FUN_1003fac0(int);
extern int DAT_10ac5d44;
extern int DAT_10ac5d88;
extern int DAT_10b71a48;
extern int DAT_10b71a4c;
extern int DAT_10b71a50;
extern int DAT_10b71a54;
extern int _DAT_10ac5bbc;

/* WHAT IT DOES: read the four corner values of the currently selected table
 * row, following the row pointer stored for that index. The accessor the
 * menu drawing uses to lay a row out. */
/* @implements 0x100382D0 glide FUN_100382d0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100382d0(int param_1)

{
  int *puVar1;
  int a;
  int b;
  int c;
  int d;
  int idx;

  idx = DAT_10ac5d88;
  puVar1 = *(int **)(DAT_10ac5d44 + 0x1de48 + idx * 8);
  b = puVar1[1];
  c = puVar1[2];
  d = puVar1[3];
  a = *puVar1;
  _DAT_10ac5bbc = idx;
  DAT_10b71a48 = a;
  DAT_10b71a4c = b;
  DAT_10b71a50 = c;
  DAT_10b71a54 = d;
  FUN_1003fac0(param_1);
  return 0;
}
