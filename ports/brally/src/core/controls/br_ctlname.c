/* br_ctlname.c -- see br_ctlname.h.
 *
 * RESPONSIBILITY: reading what the player is doing -- specifically, naming
 * the things that can be bound.
 */
/* The original is /MD: sprintf is the imported one (`call edi`). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_ctlname.h"

#include "slice4_52.h"   /* BrStrGet, D3D 0x10074030 == Glide 0x1006D280 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * Storage this module owns
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* Glide 0x10B71C70 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* Glide 0x10B71B08 */

/* Glide 0x100B3B40, 120 records of 0x24, transcribed byte for byte out of
 * BRGlide.dll's .data.  Every name's tail is zero-filled in the image and no
 * name exceeds 13 characters.
 *
 * These are DirectInput DIK_* scan codes and the names are the game's own
 * spellings -- "KEYPAD *", "APP MENU", a bare "\\" for DIK_BACKSLASH. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* ==========================================================================
 * 0x10058AF0 -- build the joystick and mouse name tables
 * ========================================================================== */

/* The original's `call [sprintf]` with a RUNTIME format string, which is what
 * BrStrGet returns.  Wrapped rather than called inline so that the non-literal
 * format does not trip -Wformat-security; the behaviour, including the absence
 * of any bound on the 32-byte destination, is the original's.
 *
 * Both call sites are here: 0x10058B36 passes one integer argument and
 * 0x10058B94 passes none. */
/* (port-only ctlname_sprintf removed) */


/* The tail shared by both arms of both loops.  `code` is the value already
 * stored in the record; the select is the original's
 *     add eax, -base ; cmp eax, 5 ; ja skip ; jmp [table + eax*4]
 * so it is UNSIGNED and a code below the base wraps to a huge number and is
 * skipped rather than indexing backwards. */
/* (port-only ctlname_axis removed) */


/* WHAT IT DOES: fills the mouse and joystick control-name tables used by the
 * controls menu: the first rows of each are "BUTTON n", the rest get the
 * axis names (left, right, up, down, ...) from the string table. */
/* @implements 0x10058AF0 glide BrCtlNameInit */
/* Hand-transcribed from the asm: sprintf straight on BrStrGet's format, the
 * axis arms as a switch whose cases each make their own call (VC5 merges the
 * calls and leaves `push id / jmp`), and the mouse code as an unsigned-char
 * local (`mov al,bl; sub al,0x7e` through a byte stack slot). */
void BrCtlNameInit(void)
{
    int32_t       i;
    unsigned char c;

    memset((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse), 0, BR_CTLNAME_MOUSE_CLEAR);
    memset((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy),   0, BR_CTLNAME_JOY_CLEAR);

    for (i = 0; i < BR_CTLNAME_MOUSE_COUNT; i++) {
        if (i < BR_CTLNAME_MOUSE_BUTTONS) {
            (*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].key = (uint32_t)i;
            sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText,
                    BrStrGet(BR_CTLNAME_STR_BUTTON), i);
        } else {
            c = (unsigned char)(i - 0x7E);
            (*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].key = c;
            switch (c) {
            case 0x86: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC4)); break;
            case 0x87: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC5)); break;
            case 0x88: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC6)); break;
            case 0x89: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC7)); break;
            case 0x8A: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC8)); break;
            case 0x8B: sprintf((*(BrCfgRec (*)[10])&g_aBrCtlNameMouse)[i].szText, BrStrGet(0xC9)); break;
            }
        }
    }

    for (i = 0; i < BR_CTLNAME_JOY_COUNT; i++) {
        if (i < BR_CTLNAME_JOY_BUTTONS) {
            (*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].key = (uint32_t)i;
            sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText,
                    BrStrGet(BR_CTLNAME_STR_BUTTON), i);
        } else {
            (*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].key = (unsigned char)i;
            switch ((unsigned char)i) {
            case 0x80: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC4)); break;
            case 0x81: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC5)); break;
            case 0x82: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC6)); break;
            case 0x83: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC7)); break;
            case 0x84: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC8)); break;
            case 0x85: sprintf((*(BrCfgRec (*)[134])&g_aBrCtlNameJoy)[i].szText, BrStrGet(0xC9)); break;
            }
        }
    }
}

/* ==========================================================================
 * The three tables as slice2_23.c wants them
 * ========================================================================== */

/* (port-only BrCtlNameTables removed) */


/* 0x10039580 BrCtlNameFind, the reader, lives in
 * BrCtlBindingToItem_10039620.cpp: it shares the original's C++ TU with its
 * caller 0x10039620 and is compiled ahead of it there. */
