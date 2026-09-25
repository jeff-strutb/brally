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

#ifdef BR_MATCHING_BUILD
/* slice3_42.h declares this cdecl; the original is thiscall with one stack
 * argument.  Hide the prototype so the matching body can carry the
 * __fastcall shape with a struct-typed second argument (never
 * register-eligible, so it cannot claim edx). */
#define BrCtrlCfgLoadDefaults BrCtrlCfgLoadDefaults_cdecl
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#endif
#include "slice3_42.h"
#ifdef BR_MATCHING_BUILD
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
#endif

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
const BrCtrlProfile g_BrCtrlDefaults[BR_CTRL_PROFILES] = {
    { { /* 0x100B4098 */
        { 0x00CB, 0x004B, 0x00CB }, { 0x00CD, 0x004D, 0x00CD },
        { 0x00C8, 0x0048, 0x00C8 }, { 0x009D, 0x009D, 0x001D },
        { 0x00D0, 0x0050, 0x00D0 }, { 0x001E, 0x001E, 0x001E },
        { 0x002C, 0x002C, 0x002C }, { 0x0036, 0x0036, 0x002A },
        { 0x00D3, 0x0053, 0x00D3 }, { 0x00CF, 0x004F, 0x00CF },
        { 0x00D1, 0x0051, 0x00D1 }, { 0x00D2, 0x0052, 0x00D2 },
        { 0x00C8, 0x0048, 0x00C8 }, { 0x00D0, 0x0050, 0x00D0 },
        { 0x001C, 0x009C, 0x001C }, { 0x0001, 0x0001, 0x0001 },
        { 0x0039, 0x0039, 0x0039 }, { 0x003B, 0x003B, 0x003B },
        { 0x003C, 0x003C, 0x003C }, { 0x003D, 0x003D, 0x003D },
        { 0x003E, 0x003E, 0x003E }, { 0x0000, 0x0039, 0x004C },
        { 0x0000, 0x00CD, 0x004D }, { 0x0000, 0x00CB, 0x004B },
        { 0x0000, 0x00C9, 0x0049 }, { 0x0000, 0x00C7, 0x0047 },
        { 0x0000, 0x000D, 0x004E }, { 0x0000, 0x000C, 0x004A }
    } },
    { { /* 0x100B4140 */
        { 0x8000, 0x004B, 0x00CB }, { 0x8100, 0x004D, 0x00CD },
        { 0x8200, 0x0048, 0x00C8 }, { 0x8300, 0x009D, 0x001D },
        { 0x0103, 0x0050, 0x00D0 }, { 0x0100, 0x001E, 0x001E },
        { 0x0101, 0x002C, 0x002C }, { 0x0036, 0x0036, 0x002A },
        { 0x00D3, 0x0053, 0x00D3 }, { 0x00CF, 0x004F, 0x00CF },
        { 0x00D1, 0x0051, 0x00D1 }, { 0x00D2, 0x0052, 0x00D2 },
        { 0x0100, 0x0048, 0x00C8 }, { 0x0101, 0x0050, 0x00D0 },
        { 0x8200, 0x009C, 0x001C }, { 0x0001, 0x0001, 0x0001 },
        { 0x0039, 0x0039, 0x0039 }, { 0x003B, 0x003B, 0x003B },
        { 0x003C, 0x003C, 0x003C }, { 0x003D, 0x003D, 0x003D },
        { 0x003E, 0x003E, 0x003E }, { 0x0000, 0x0039, 0x004C },
        { 0x0000, 0x00CD, 0x004D }, { 0x0000, 0x00CB, 0x004B },
        { 0x0000, 0x00C9, 0x0049 }, { 0x0000, 0x00C7, 0x0047 },
        { 0x0000, 0x000D, 0x004E }, { 0x0000, 0x000C, 0x004A }
    } },
    { { /* 0x100B41E8 */
        { 0x8000, 0x004B, 0x00CB }, { 0x8100, 0x004D, 0x00CD },
        { 0x0100, 0x0048, 0x00C8 }, { 0x0101, 0x009D, 0x001D },
        { 0x0102, 0x0050, 0x00D0 }, { 0x001E, 0x001E, 0x001E },
        { 0x002C, 0x002C, 0x002C }, { 0x0036, 0x0036, 0x002A },
        { 0x00D3, 0x0053, 0x00D3 }, { 0x00CF, 0x004F, 0x00CF },
        { 0x00D1, 0x0051, 0x00D1 }, { 0x00D2, 0x0052, 0x00D2 },
        { 0x8200, 0x0048, 0x00C8 }, { 0x8300, 0x0050, 0x00D0 },
        { 0x0100, 0x009C, 0x001C }, { 0x0001, 0x0001, 0x0001 },
        { 0x0039, 0x0039, 0x0039 }, { 0x003B, 0x003B, 0x003B },
        { 0x003C, 0x003C, 0x003C }, { 0x003D, 0x003D, 0x003D },
        { 0x003E, 0x003E, 0x003E }, { 0x0000, 0x0039, 0x004C },
        { 0x0000, 0x00CD, 0x004D }, { 0x0000, 0x00CB, 0x004B },
        { 0x0000, 0x00C9, 0x0049 }, { 0x0000, 0x00C7, 0x0047 },
        { 0x0000, 0x000D, 0x004E }, { 0x0000, 0x000C, 0x004A }
    } },
    { { /* 0x100B4290 */
        { 0x8600, 0x004B, 0x00CB }, { 0x8700, 0x004D, 0x00CD },
        { 0x0300, 0x0048, 0x00C8 }, { 0x009D, 0x009D, 0x001D },
        { 0x0301, 0x0050, 0x00D0 }, { 0x001E, 0x001E, 0x001E },
        { 0x002C, 0x002C, 0x002C }, { 0x0036, 0x0036, 0x002A },
        { 0x00D3, 0x0053, 0x00D3 }, { 0x00CF, 0x004F, 0x00CF },
        { 0x00D1, 0x0051, 0x00D1 }, { 0x00D2, 0x0052, 0x00D2 },
        { 0x00C8, 0x0048, 0x00C8 }, { 0x00D0, 0x0050, 0x00D0 },
        { 0x001C, 0x009C, 0x001C }, { 0x0001, 0x0001, 0x0001 },
        { 0x0039, 0x0039, 0x0039 }, { 0x003B, 0x003B, 0x003B },
        { 0x003C, 0x003C, 0x003C }, { 0x003D, 0x003D, 0x003D },
        { 0x003E, 0x003E, 0x003E }, { 0x0000, 0x0039, 0x004C },
        { 0x0000, 0x00CD, 0x004D }, { 0x0000, 0x00CB, 0x004B },
        { 0x0000, 0x00C9, 0x0049 }, { 0x0000, 0x00C7, 0x0047 },
        { 0x0000, 0x000D, 0x004E }, { 0x0000, 0x000C, 0x004A }
    } }
};

BrCtrlCfg g_BrCtrlCfg;      /* 0x10B4DF30 */

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
void BrCtrlCfgAssign(BrCtrlCfg *pThis, int32_t profile, int32_t action,
                     int32_t hi, int32_t lo)
{
    BrCtrlProfile       *pP;
    const BrCtrlProfile *pDef;
    int slot;

    /* The 1/2/3-else dispatch, open-coded: the original has NO call here --
     * it selects the profile/default PAIR with the `dec eax; je` chain
     * (glide 0x10062b80: ebp=this+k*0x?A8, edi=defaults+same), and the image
     * build cannot place a call to a helper the original never emitted.
     * BrCtrlProfileIndex stays for the callers whose originals differ. */
    if (profile == 1) {
        pP = &pThis->profile[1]; pDef = &g_BrCtrlDefaults[1];
    } else if (profile == 2) {
        pP = &pThis->profile[2]; pDef = &g_BrCtrlDefaults[2];
    } else if (profile == 3) {
        pP = &pThis->profile[3]; pDef = &g_BrCtrlDefaults[3];
    } else {
        pP = &pThis->profile[0]; pDef = &g_BrCtrlDefaults[0];
    }

    /* The original spells this ((lo ^ hi) & 0xFF) ^ hi, truncated to 16 bits;
     * that keeps hi's byte 1 and lo's byte 0 and drops everything else. */
    pP->e[action][0] =
        (uint16_t)(((uint32_t)((uint8_t)((uint32_t)lo ^ (uint32_t)hi))) ^ (uint32_t)hi);

    /* DEVIATION: the original wraps the body below in a `dec ebx; jne` loop
     * that runs it 28 times per slot.  The loop counter is used for nothing
     * else, the body rewrites the same slot from the same default and then
     * re-scans, and the scan only ever looks at slot 0 of each action -- never
     * at the slot being written.  So iterations 2..28 cannot change anything.
     * Collapsed to one pass. */
    for (slot = 1; slot <= 2; ++slot) {
        const uint16_t v = pDef->e[action][slot];
        int i;

        pP->e[action][slot] = v;
        for (i = 0; i < BR_CTRL_ACTIONS; ++i) {
            if (pP->e[i][0] == v) {
                pP->e[action][slot] = 0;
                break;
            }
        }
    }
}
