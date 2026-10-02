/* br_menuact.c -- menus.  See br_menuact.h. */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_ui.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include "br_menuact.h"

#include <stdint.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrExt_10043E70: prototype in br_funcs.h */
/* BrExt_10045BC0: prototype in br_funcs.h */
/* BrExt_100451E0: prototype in br_funcs.h */
/* BrExt_10046790: prototype in br_funcs.h */
/* BrExt_10046750: prototype in br_funcs.h */
/* BrExt_10046910: prototype in br_funcs.h */
/* BrExt_10046950: prototype in br_funcs.h */
/* BrExt_100469F0: prototype in br_funcs.h */
/* BrExt_10046A30: prototype in br_funcs.h */
/* BrExt_10046BB0: prototype in br_funcs.h */
/* BrExt_10043E70: the original function is Ctl3D3C0 */
/* BrExt_10045BC0: the original function is CtlF060 */
/* BrExt_100451E0: the original function is Ctl3E730 */
/* BrExt_10046790: the original function is BrMenuResetTrackStr */
/* BrExt_10046750: the original function is BrOpt6750 */
/* BrExt_10046910: the original function is BrOpt6910 */
/* BrExt_10046950: the original function is FUN_1003fda0 */
/* BrExt_100469F0: the original function is BrOpt69F0 */
/* BrExt_10046A30: the original function is FUN_1003fe80 */
/* BrExt_10046BB0: the original function is BrOpt6BB0 */

/* WHAT IT DOES: the first play-mode button.  Records "mode 0" and opens
 * the next screen.  Always reports success. */
/* @implements 0x10044010 d3d BrHook_10044010 */
/* @n64 0x80200128 located */
int BrHook_10044010(void *p)
{
    (*(uint32_t *)&DAT_10ac5bd4) = 0;
    Ctl3D3C0_fn(p);
    return 1;
}

/* WHAT IT DOES: a menu button was pressed.  Open the matching screen,
 * then point that screen's Back row at a leave routine so backing out
 * returns here.  Always reports success. */
/* @implements 0x100457A0 d3d BrHook_100457A0 */
int BrHook_100457A0(void *p)
{
    CtlF060_fn(p);
    g_brUipAA29F4->pfn08 = (BrUiCtlHookFn_)&BrMenuResetTrackStr;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045780 d3d BrHook_10045780 */
int BrHook_10045780(void *p)
{
    Ctl3E730_fn(p);
    g_brUipAA29C8->pfn08 = (BrUiCtlHookFn_)&BrOpt6750;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045800 d3d BrHook_10045800 */
int BrHook_10045800(void *p)
{
    Ctl3E730_fn(p);
    g_brUipAA29C8->pfn08 = (BrUiCtlHookFn_)&BrOpt6910;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045820 d3d BrHook_10045820 */
int BrHook_10045820(void *p)
{
    CtlF060_fn(p);
    g_brUipAA29F4->pfn08 = (BrUiCtlHookFn_)&FUN_1003fda0;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045840 d3d BrHook_10045840 */
int BrHook_10045840(void *p)
{
    Ctl3E730_fn(p);
    g_brUipAA29C8->pfn08 = (BrUiCtlHookFn_)&BrOpt69F0;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045860 d3d BrHook_10045860 */
int BrHook_10045860(void *p)
{
    CtlF060_fn(p);
    g_brUipAA29F4->pfn08 = (BrUiCtlHookFn_)&FUN_1003fe80;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x100458C0 d3d BrHook_100458C0 */
int BrHook_100458C0(void *p)
{
    Ctl3E730_fn(p);
    g_brUipAA29C8->pfn08 = (BrUiCtlHookFn_)&BrOpt6BB0;
    return 1;
}

/* CRT (strlen/strcpy/_stricmp) resolves via the FF 15 import table. */
#include <string.h>
/* ------------------------------------------------------------------ */
/* 0x100384C0                                                         */
/* ------------------------------------------------------------------ */

/* FUN_10038380: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: stores the car's display name if it changed, and clears the
 * "name is a default" bit when the name string is not empty. */
/* @implements 0x100384C0 glide BrCarNameCommit */
int BrCarNameCommit(BrUiCtl_ *param_1)
{
    char *s;

    Br85ItemApply((struct BrCtl85 *)param_1, 0);
    s = param_1->aText[0].sz;
    if (strlen(s) != 0) {
        DAT_10ac5d40->flags1C &= ~0x10u;
    }
    if (_stricmp(&(g_aBrA9CDF0[0]), s) != 0) {
        strcpy(&(g_aBrA9CDF0[0]), s);
        strcpy(&(DAT_10b71544[0]), &(g_aBrA9CDF0[0]));
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 0x1003AF30  SetStatusText                                          */
/* ------------------------------------------------------------------ */

/* thiscall with 4 stack args.  __fastcall puts `this` in ecx; the second
 * register-eligible arg is edx.  Passing param_1 (already live in edx as
 * the push temp) rather than literal 0 avoids `xor edx,edx`. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5C5C, the current phase object */

/* WHAT IT DOES: SetStatusText.  Looks up the control named by the root
 * page's status-line index (0x10AC4C58) on the current phase's first page
 * (0x10AC5C5C)->+0x14, and if that slot is occupied calls vtable +0x34
 * with (text, 1, 1, style 0x100AACF8). */
/* @implements 0x1003AF30 glide BrExt_100419D0 */
void BrExt_100419D0(const char *pszText)
{
    BrUiCtl_ *pCtl = g_brPAA29B8->aPages[0]->apCtl[DAT_10ac4c58];

    if (pCtl != NULL) {
        pCtl->pVtbl->f34(pCtl, pszText, 1, 1, &DAT_100aacf8);
    }
}


/* ------------------------------------------------------------------ */
/* 0x1003AF60                                                         */
/* ------------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: toggle a per-entry flag in the current list row and swap the
 * row's name with a saved one: turning it on stashes the existing name and
 * blanks the field, so the player can type a replacement. The undo half of
 * an in-place rename. */
/* @implements 0x1003AF60 glide BrExt_10041A00 */
int BrExt_10041A00(char * param_1)

{
  char *pcVar6;

  *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
  *(unsigned int *)(g_5D24 + 0x44c + g_AB94 * 0x438) =
       (unsigned int)(*(int *)(g_5D24 + 0x44c + g_AB94 * 0x438) == 0);
  g_5C30 = *(int *)(g_5D24 + 0x44c + g_AB94 * 0x438);
  if (g_5C30 != 0) {
    pcVar6 = (char *)(g_5D24 + g_AB94 * 0x438 + 0x35);
    strcpy(&(g_aBrA9D078[0]), pcVar6);
    strcpy(pcVar6, &(g_aBr39B720[0]));
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x1003B020                                                         */
/* ------------------------------------------------------------------ */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: copies the current track name into the selected driver's
 * slot, then copies the default name back over the working buffer. */
/* @implements 0x1003B020 glide BrMenuCopyTrackName */
int BrMenuCopyTrackName(char * param_1)
{
    *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
    g_5C3C = 0;
    if (g_5C30 != 0 && &(g_aBrA9D078[0]) != 0) {
        strcpy((char *)(g_5D24 + 0x35 + g_AB94 * 0x438),
               &(g_aBrA9D078[0]));
        strcpy(&(g_aBrA9D078[0]), &(g_aBr39B720[0]));
    }
    return 1;
}
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: latch the three pending menu values (two words and a byte) into their
 * current slots. Returns 1. */
/* @implements 0x10039F30 glide BrMenuLatchPending */

int BrMenuLatchPending(void)

{
  g_brPhase5BF8 = DAT_10ac5934;
  (*(char *)&DAT_10ac5c10) = DAT_10ac592c;
  g_brIdx5BFC = DAT_10ac5930;
  return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: menu option handler: set flag 5C50, request redraw, mark dirty. */
/* @implements 0x10040930 glide BrMenuOpt40930 */

int BrMenuOpt40930(void)

{
  DAT_10ac5c50 = 1;
  BrSub10072AF0(2,0x200020);
  g_track = 2;
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: menu option handler: set flag 5C54, request redraw, mark dirty. */
/* @implements 0x10040960 glide BrMenuOpt40960 */

int BrMenuOpt40960(void)

{
  DAT_10ac5c54 = 1;
  BrSub10072AF0(2,0x200020);
  g_track = 2;
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: menu option handler: set flag 5D98, request redraw, mark dirty. */
/* @implements 0x100409C0 glide BrMenuOpt409C0 */

int BrMenuOpt409C0(void)

{
  g_5D98 = 1;
  BrSub10072AF0(2,0x200020);
  g_track = 2;
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: menu option handler: set flag 5C4C, request redraw, mark dirty. */
/* @implements 0x100409F0 glide BrMenuOpt409F0 */

int BrMenuOpt409F0(void)

{
  DAT_10ac5c4c = 1;
  BrSub10072AF0(2,0x200020);
  g_track = 2;
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: the same in-place rename toggle as BrExt_10041A00, acting on
 * the list at 0x10AC5D28 instead. */
/* @implements 0x1003B970 glide BrExt_10042410 */
int BrExt_10042410(char * param_1)

{
  char *pcVar6;
  
  *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
  *(unsigned int *)(g_brPAA29D0 + 0x44c + g_AB94 * 0x438) =
       (unsigned int)(*(int *)(g_brPAA29D0 + 0x44c + g_AB94 * 0x438) == 0);
  g_5C30 = *(int *)(g_brPAA29D0 + 0x44c + g_AB94 * 0x438);
  if (g_5C30 != 0) {
    pcVar6 = (char *)(g_brPAA29D0 + g_AB94 * 0x438 + 0x35);
    strcpy(&(g_aBrA9D078[0]), pcVar6);
    strcpy(pcVar6, &(g_aBr39B720[0]));
  }
  return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* src dword */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* src byte  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* src byte -> widened */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* dst dword */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* dst dword (from byte) */

/* WHAT IT DOES: copy three menu values from their editing copies into their
 * live ones -- the commit step when a player accepts a page rather than
 * cancelling it. Always reports success. */
/* @implements 0x10039F60 glide FUN_10039f60 */
int FUN_10039f60(void)
{
  g_brPhase5BF8 = (DAT_10ac5a48[0]);
  (*(char *)&DAT_10ac5c10) = (g_aBrAA26F4[0]);
  g_brIdx5BFC = (*(unsigned char *)&g_aBrAA26F4[1]);
  return 1;
}
