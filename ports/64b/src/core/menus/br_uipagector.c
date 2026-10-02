#include "br_ui.h"
#include "br_vtables.h"
#include <string.h>
/* br_uipagector.c -- menus: the menu page constructor.
 *
 * 0x100418C0, the compiler-emitted constructor for the front-end page class:
 * installs the method table (0x100776C0) and clears every field including
 * the 800-byte entry array.  Its own TU: br_uiscreen.c renames this symbol
 * to the port copy's name (see br_uinav.h), so the matching body cannot
 * live there.
 */

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
int FUN_100776c0();
/* 64-bit core: declared once, in br_globals.h or its struct's header */



/* WHAT IT DOES: construct a menu page object in place -- installs its method
 * table and clears every field, including the 800-byte entry array. The
 * compiler-emitted constructor for the page class. */
/* @implements 0x100418C0 glide BrUiPageCtor_10048470 */
struct BrUiPage_ *BrUiPageCtor_10048470(struct BrUiPage_ *p)
{
    p->f10 = 0;
    p->cCtl = 0;
    p->fX = 0;
    p->fY = 0;
    p->pVtbl = (const BrUiPageVtbl_ *)g_brVtbl_100776C0;   /* the page vtable */
    p->pfn04 = 0;
    p->pfn08 = 0;
    p->pfn0C = 0;
    memset(p->apCtl, 0, sizeof p->apCtl);
    p->pOwner = 0;
    p->cSel = 0;
    p->iSel = 0;
    return p;
}


