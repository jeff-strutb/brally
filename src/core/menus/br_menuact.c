/* br_menuact.c -- menus.  See br_menuact.h. */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_menuact.h"

#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern uint32_t g_AA287C;
extern uint32_t *g_AA29F4;
extern uint32_t *g_AA29C8;
void BrExt_10043E70(void *);
void BrExt_10045BC0(void *);
void BrExt_100451E0(void *);
void BrExt_10046790(void);
void BrExt_10046750(void);
void BrExt_10046910(void);
void BrExt_10046950(void);
void BrExt_100469F0(void);
void BrExt_10046A30(void);
void BrExt_10046BB0(void);
void BrExt_10043E70(void *p) { (void)p; }
void BrExt_10045BC0(void *p) { (void)p; }
void BrExt_100451E0(void *p) { (void)p; }
void BrExt_10046790(void) {}
void BrExt_10046750(void) {}
void BrExt_10046910(void) {}
void BrExt_10046950(void) {}
void BrExt_100469F0(void) {}
void BrExt_10046A30(void) {}
void BrExt_10046BB0(void) {}
#else
uint32_t  g_AA287C;
uint32_t *g_AA29F4;
uint32_t *g_AA29C8;
void BrExt_10043E70(void *p);
void BrExt_10045BC0(void *p);
void BrExt_100451E0(void *p);
void BrExt_10046790(void);
void BrExt_10046750(void);
void BrExt_10046910(void);
void BrExt_10046950(void);
void BrExt_100469F0(void);
void BrExt_10046A30(void);
void BrExt_10046BB0(void);
#endif

/* WHAT IT DOES: the first play-mode button.  Records "mode 0" and opens
 * the next screen.  Always reports success. */
/* @implements 0x10044010 d3d BrHook_10044010 */
/* @n64 0x80200128 located */
int BrHook_10044010(void *p)
{
    g_AA287C = 0;
    BrExt_10043E70(p);
    return 1;
}

/* WHAT IT DOES: a menu button was pressed.  Open the matching screen,
 * then point that screen's Back row at a leave routine so backing out
 * returns here.  Always reports success. */
/* @implements 0x100457A0 d3d BrHook_100457A0 */
int BrHook_100457A0(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046790;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045780 d3d BrHook_10045780 */
int BrHook_10045780(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046750;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045800 d3d BrHook_10045800 */
int BrHook_10045800(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046910;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045820 d3d BrHook_10045820 */
int BrHook_10045820(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046950;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045840 d3d BrHook_10045840 */
int BrHook_10045840(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_100469F0;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x10045860 d3d BrHook_10045860 */
int BrHook_10045860(void *p)
{
    BrExt_10045BC0(p);
    g_AA29F4[2] = (uint32_t)(uintptr_t)&BrExt_10046A30;
    return 1;
}

/* WHAT IT DOES: another menu button of the same family (open + wire Back). */
/* @implements 0x100458C0 d3d BrHook_100458C0 */
int BrHook_100458C0(void *p)
{
    BrExt_100451E0(p);
    g_AA29C8[2] = (uint32_t)(uintptr_t)&BrExt_10046BB0;
    return 1;
}

#ifdef BR_MATCHING_BUILD
/* CRT (strlen/strcpy/_stricmp) resolves via the FF 15 import table. */
#include <string.h>
/* ------------------------------------------------------------------ */
/* 0x100384C0                                                         */
/* ------------------------------------------------------------------ */

int FUN_10038380(int, int);
extern char DAT_10ac3e80;
extern char DAT_10b71544;
extern int DAT_10ac5d40;

/* WHAT IT DOES: stores the car's display name if it changed, and clears the
 * "name is a default" bit when the name string is not empty. */
/* @implements 0x100384C0 glide BrCarNameCommit */
int BrCarNameCommit(int param_1)
{
    char *s;

    FUN_10038380(param_1, 0);
    s = (char *)(param_1 + 0x2b65);
    if (strlen(s) != 0) {
        *(unsigned int *)(DAT_10ac5d40 + 0x1c) &= ~0x10u;
    }
    if (_stricmp(&DAT_10ac3e80, s) != 0) {
        strcpy(&DAT_10ac3e80, s);
        strcpy(&DAT_10b71544, &DAT_10ac3e80);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 0x1003AF30  SetStatusText                                          */
/* ------------------------------------------------------------------ */

/* thiscall with 4 stack args.  __fastcall puts `this` in ecx; the second
 * register-eligible arg is edx.  Passing param_1 (already live in edx as
 * the push temp) rather than literal 0 avoids `xor edx,edx`. */
typedef int (__fastcall *BrCtlF34)(void *this, int _edx_unused, int, int, int, int);
extern int DAT_10ac4c58;
extern int DAT_100aacf8;
extern int DAT_10ac5c5c;   /* 0x10AC5C5C, the current phase object */

/* WHAT IT DOES: SetStatusText.  Looks up the control named by the root
 * page's status-line index (0x10AC4C58) on the current phase's first page
 * (0x10AC5C5C)->+0x14, and if that slot is occupied calls vtable +0x34
 * with (text, 1, 1, style 0x100AACF8). */
/* @implements 0x1003AF30 glide BrExt_100419D0 */
void BrExt_100419D0(int param_1)
{
    int *piVar1;

    piVar1 = *(int **)(*(int *)(DAT_10ac5c5c + 0x14) + 0x18 + DAT_10ac4c58 * 4);
    if (piVar1 != (int *)0x0) {
        (*(BrCtlF34 *)(*(int *)(piVar1) + 52))(piVar1, param_1, param_1, 1, 1, &DAT_100aacf8);
    }
    return;
}


/* ------------------------------------------------------------------ */
/* 0x1003AF60                                                         */
/* ------------------------------------------------------------------ */

extern char DAT_10396f08;
extern char DAT_10ac4100;
extern int DAT_10ac5d24;
extern int g_brAA28D8;
extern int g_i0AB3F4;

/* WHAT IT DOES: toggle a per-entry flag in the current list row and swap the
 * row's name with a saved one: turning it on stashes the existing name and
 * blanks the field, so the player can type a replacement. The undo half of
 * an in-place rename. */
/* @implements 0x1003AF60 glide BrExt_10041A00 */
int BrExt_10041A00(int param_1)

{
  char *pcVar6;

  *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
  *(unsigned int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438) =
       (unsigned int)(*(int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438) == 0);
  g_brAA28D8 = *(int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438);
  if (g_brAA28D8 != 0) {
    pcVar6 = (char *)(DAT_10ac5d24 + g_i0AB3F4 * 0x438 + 0x35);
    strcpy(&DAT_10ac4100, pcVar6);
    strcpy(pcVar6, &DAT_10396f08);
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x1003B020                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10ac5c30;
extern int DAT_10ac5c3c;
extern char DAT_10ac4100;
extern int DAT_10ac5d24;
extern int DAT_100aab94;
extern char DAT_10396f08;

/* WHAT IT DOES: copies the current track name into the selected driver's
 * slot, then copies the default name back over the working buffer. */
/* @implements 0x1003B020 glide BrMenuCopyTrackName */
int BrMenuCopyTrackName(int param_1)
{
    *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
    DAT_10ac5c3c = 0;
    if (DAT_10ac5c30 != 0 && &DAT_10ac4100 != 0) {
        strcpy((char *)(DAT_10ac5d24 + 0x35 + DAT_100aab94 * 0x438),
               &DAT_10ac4100);
        strcpy(&DAT_10ac4100, &DAT_10396f08);
    }
    return 1;
}
extern char DAT_10ac592c;
extern int DAT_10ac5930;
extern int DAT_10ac5934;
extern int DAT_10ac5bf8;
extern char DAT_10ac5c10;
extern int g_brAA28A4;

/* WHAT IT DOES: latch the three pending menu values (two words and a byte) into their
 * current slots. Returns 1. */
/* @implements 0x10039F30 glide BrMenuLatchPending */

int BrMenuLatchPending(void)

{
  DAT_10ac5bf8 = DAT_10ac5934;
  DAT_10ac5c10 = DAT_10ac592c;
  g_brAA28A4 = DAT_10ac5930;
  return 1;
}

extern int DAT_10ac5c50;
extern int g_brAA2854;

/* WHAT IT DOES: menu option handler: set flag 5C50, request redraw, mark dirty. */
/* @implements 0x10040930 glide BrMenuOpt40930 */

int BrMenuOpt40930(void)

{
  DAT_10ac5c50 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}

extern int DAT_10ac5c54;
extern int g_brAA2854;


/* WHAT IT DOES: menu option handler: set flag 5C54, request redraw, mark dirty. */
/* @implements 0x10040960 glide BrMenuOpt40960 */

int BrMenuOpt40960(void)

{
  DAT_10ac5c54 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}

extern int DAT_10ac5d98;
extern int g_brAA2854;


/* WHAT IT DOES: menu option handler: set flag 5D98, request redraw, mark dirty. */
/* @implements 0x100409C0 glide BrMenuOpt409C0 */

int BrMenuOpt409C0(void)

{
  DAT_10ac5d98 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}

extern int DAT_10ac5c4c;
extern int g_brAA2854;


/* WHAT IT DOES: menu option handler: set flag 5C4C, request redraw, mark dirty. */
/* @implements 0x100409F0 glide BrMenuOpt409F0 */

int BrMenuOpt409F0(void)

{
  DAT_10ac5c4c = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}

#endif /* BR_MATCHING_BUILD */

#ifdef BR_MATCHING_BUILD
extern int DAT_10ac5d28;
extern int g_brAA28D8;
extern int g_i0AB3F4;

/* WHAT IT DOES: the same in-place rename toggle as BrExt_10041A00, acting on
 * the list at 0x10AC5D28 instead. */
/* @implements 0x1003B970 glide BrExt_10042410 */
int BrExt_10042410(int param_1)

{
  char *pcVar6;
  
  *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
  *(unsigned int *)(DAT_10ac5d28 + 0x44c + g_i0AB3F4 * 0x438) =
       (unsigned int)(*(int *)(DAT_10ac5d28 + 0x44c + g_i0AB3F4 * 0x438) == 0);
  g_brAA28D8 = *(int *)(DAT_10ac5d28 + 0x44c + g_i0AB3F4 * 0x438);
  if (g_brAA28D8 != 0) {
    pcVar6 = (char *)(DAT_10ac5d28 + g_i0AB3F4 * 0x438 + 0x35);
    strcpy(&DAT_10ac4100, pcVar6);
    strcpy(pcVar6, &DAT_10396f08);
  }
  return 1;
}
#endif /* BR_MATCHING_BUILD */

#ifdef BR_MATCHING_BUILD
extern int           DAT_10ac5a48;   /* src dword */
extern unsigned char DAT_10ac5a4c;   /* src byte  */
extern unsigned char DAT_10ac5a4d;   /* src byte -> widened */
extern int           DAT_10ac5bf8;   /* dst dword */
extern int           DAT_10ac5bfc;   /* dst dword (from byte) */

/* WHAT IT DOES: copy three menu values from their editing copies into their
 * live ones -- the commit step when a player accepts a page rather than
 * cancelling it. Always reports success. */
/* @implements 0x10039F60 glide FUN_10039f60 */
int FUN_10039f60(void)
{
  DAT_10ac5bf8 = DAT_10ac5a48;
  DAT_10ac5c10 = DAT_10ac5a4c;
  DAT_10ac5bfc = DAT_10ac5a4d;
  return 1;
}
#endif /* BR_MATCHING_BUILD */
