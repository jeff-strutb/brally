/* br_ctrlcfgobj.c -- controls: the control-binding object.
 *
 * The four shipped control layouts (g_BrCtrlDefaults), the live settings
 * block g_BrCtrlCfg, and the routines on it: BrCtrlCfgInit (glide
 * 0x10062D00) restores factory bindings and default options, BrCtrlCfgCopy
 * copies one block into another, and BrCtrlCfgAssign (glide 0x10062B80) binds
 * one action to a freshly pressed input, clearing clashing backup bindings.
 *
 * Filed out of the address batch slice3_42.c, with that file's preamble.
 */

#include <string.h>

/* slice3_42.h declares this cdecl; the original is thiscall with one stack
 * argument.  Hide the prototype so the matching body can carry the
 * __fastcall shape with a struct-typed second argument (never
 * register-eligible, so it cannot claim edx). */
#define BrCtrlCfgLoadDefaults BrCtrlCfgLoadDefaults_cdecl
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#include "slice3_42.h"
#undef BrCtrlCfgLoadDefaults
#undef BrFn10069BC0
#undef BrFn10069C30
typedef struct { int32_t v; } BrCtrlProfileArg;
/* BOTH stack arguments are struct-wrapped. __fastcall skips a struct when it
 * hands out ecx/edx, so wrapping only the FIRST of them lets the SECOND take
 * edx and the function cleans 4 bytes instead of 8. Wrapping both leaves ecx
 * for `this` and puts the pair on the stack, which is thiscall exactly. */
typedef struct { int32_t v; } BrCtrlKindArg;
typedef struct { uint32_t v; } BrCtrlKeyArg;

/* =====================================================================
 * .rdata constants, read out of orig/BRD3D.dll rather than assumed.
 * ===================================================================== */

#define BR_K_0008FA54   0.0f    /* 0x1008FA54 */
#define BR_K_0008FA58   2.0f    /* 0x1008FA58 */
#define BR_K_0008FA5C   1.0f    /* 0x1008FA5C */
#define BR_K_0008FAA8  30.0f    /* 0x1008FAA8 -- the simulation rate */

/* =====================================================================
 * 2. The control-binding object
 * ===================================================================== */

/* 0x100B4098, 0x100B4140, 0x100B41E8, 0x100B4290 -- read byte-for-byte out
 * of the DLL.  Profile 0 is pure keyboard; 1..3 mix in joystick axes (the
 * 0x80xx entries) and buttons (0x01xx / 0x03xx). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x10B4DF30 */

/* The 1/2/3-else dispatch every one of these five routines shares.  Written
 * out as a helper because the original open-codes it four times with the
 * `dec eax; je` idiom and getting the fall-through wrong would be silent. */
static int BrCtrlProfileIndex(int32_t sel)
{
    if (sel == 1) return 1;
    if (sel == 2) return 2;
    if (sel == 3) return 3;
    return 0;
}

/* 0x10069C90 */
/* WHAT IT DOES: puts the player's settings back to how the game ships -- all
 * four control layouts restored to their factory bindings, the keyboard one
 * selected, and the rest of the options block (screen size, sound and
 * gameplay defaults) filled in. This is what a player gets on a first run or
 * after choosing "restore defaults". */
/* @implements 0x10069C90 d3d BrCtrlCfgInit */
void BR_THISCALL1 BrCtrlCfgInit(BrCtrlCfg *pThis)
{
    pThis->profile[0] = g_BrCtrlDefaults[0];
    pThis->profile[1] = g_BrCtrlDefaults[1];
    pThis->profile[2] = g_BrCtrlDefaults[2];
    pThis->profile[3] = g_BrCtrlDefaults[3];

    pThis->active  = 0;
    pThis->pActive = &pThis->profile[0];
    pThis->f2A8 = 1;
    pThis->f2AC = 1;
    pThis->f2B0 = 1;

    memset(pThis->f2B4, 0, sizeof pThis->f2B4);
    memset(pThis->f3B8, 0, sizeof pThis->f3B8);

    pThis->f7B8 = 0x280;
    pThis->f7BC = 0x1E0;
    pThis->f7C0 = 0x10;
    pThis->f7C4 = 0;
    /* memset, not four field stores: the original builds the address once
     * (`lea ecx,[this+0x7c8]`) and stores through it at 0/4/8/0xc, which is
     * what the intrinsic emits.  Written out as four fields VC5 folds each
     * store to [this+disp] and lets `mov eax,9` steal the register. */
    memset(pThis->f7C8, 0, sizeof pThis->f7C8);

    pThis->f7D8 = 9;
    pThis->f7DC = 9;
    pThis->f7E0 = 2;
    pThis->f7E4 = 0;
    pThis->f7E8 = 0;
    pThis->f7EC = 1;
    pThis->f7F0 = 1;
    pThis->f7F4 = 1;
    pThis->f7F8 = 0;
    pThis->f7FC = 3;
    pThis->f800 = 0;
    pThis->f804 = 0;
    pThis->f808 = 4;
    pThis->f80C = 0;

    memset(pThis->f810, 0, sizeof pThis->f810);
    memset(pThis->f830, 0, sizeof pThis->f830);
    pThis->f870 = 1;
}

/* 0x10069DE0 */
BrCtrlCfg *BrCtrlCfgCopy(BrCtrlCfg *pThis, const BrCtrlCfg *pSrc)
{
    int i;

    for (i = 0; i < BR_CTRL_PROFILES; ++i)
        pThis->profile[i] = pSrc->profile[i];

    pThis->active = pSrc->active;
    /* Not a straight copy: the pointer is rebuilt from `active` so that it
     * points into pThis.  Same 1/2/3-else dispatch. */
    pThis->pActive = &pThis->profile[BrCtrlProfileIndex(pSrc->active)];

    pThis->f2A8 = pSrc->f2A8;
    pThis->f2AC = pSrc->f2AC;
    pThis->f2B0 = pSrc->f2B0;
    memcpy(pThis->f2B4, pSrc->f2B4, sizeof pThis->f2B4);
    memcpy(pThis->f3B8, pSrc->f3B8, sizeof pThis->f3B8);

    pThis->f7B8 = pSrc->f7B8;
    pThis->f7BC = pSrc->f7BC;
    pThis->f7C0 = pSrc->f7C0;
    pThis->f7C4 = pSrc->f7C4;
    memcpy(pThis->f7C8, pSrc->f7C8, sizeof pThis->f7C8);

    pThis->f7D8 = pSrc->f7D8;
    pThis->f7DC = pSrc->f7DC;
    pThis->f7E0 = pSrc->f7E0;
    pThis->f7E4 = pSrc->f7E4;
    pThis->f7E8 = pSrc->f7E8;
    pThis->f7EC = pSrc->f7EC;
    pThis->f7F0 = pSrc->f7F0;
    pThis->f7F4 = pSrc->f7F4;
    pThis->f7F8 = pSrc->f7F8;
    pThis->f7FC = pSrc->f7FC;
    pThis->f800 = pSrc->f800;
    pThis->f804 = pSrc->f804;
    pThis->f808 = pSrc->f808;
    pThis->f80C = pSrc->f80C;

    memcpy(pThis->f810, pSrc->f810, sizeof pThis->f810);
    memcpy(pThis->f830, pSrc->f830, sizeof pThis->f830);
    pThis->f870 = pSrc->f870;

    return pThis;
}

/* 0x10069B10 */
/* WHAT IT DOES: binds one game action -- steer left, brake, look behind -- to
 * a key, button or stick axis the player has just pressed on the redefine
 * screen. It also restores that action's two backup bindings from the shipped
 * defaults, and blanks either of them that would now clash with a binding
 * already in use elsewhere in the layout. */
/* @implements 0x10069B10 d3d BrCtrlCfgAssign */
/* BrCtrlCfgAssign: the placed body is BrCtrlCfgAssign_10062B80.cpp */
