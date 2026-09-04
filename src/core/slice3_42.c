/* slice3_42.c -- decompiled from BRD3D.dll, packet 0x100695D0-0x1006CCD0.
 *
 * See slice3_42.h for what each routine does and why.  Everything in this
 * file was traced instruction by instruction.  Five of the packet's 29
 * functions are absent on purpose and reported rather than guessed:
 * 0x1006A8A0 (Win32 registry), and the four large x87 physics routines
 * 0x1006B5F0, 0x1006C1F0, 0x1006C9D0 and 0x1006CCD0.
 *
 * FLOAT PRECISION.  This paragraph used to read "the original is x87 with a
 * 64-bit mantissa ... where [the spills] only affect the last ulp they are
 * not [reproduced].  Flagged once here rather than at every line."  The
 * mantissa is 53 bits, not 64: the CRT's x87 control word is 0x027F (see
 * CONVENTIONS.md), so an unspilled intermediate is EXACTLY a C `double` and
 * computing it in `float` rounds where the original does not.  There is no
 * last-ulp band inside which the spills stop mattering, which is what the old
 * wording licensed.
 *
 * The rule here: unspilled intermediates are `double`; every point where the
 * original stores to a 4-byte slot rounds through a `float` temporary,
 * because the store is the rounding.  Spill points are recorded per function
 * with the instruction that makes them.
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
 * 1. 0x100695D0
 * ===================================================================== */

/* WHAT IT DOES: turn a car's stored orientation quaternion into the 4x4
 * matrix the renderer draws with. Called once per car per frame. */
/* @implements 0x100695D0 d3d BrMat4FromCarState */
void BrMat4FromCarState(BrMat4 *pOut, const BrCarState *pSrc)
{
    const float w = pSrc->f00;      /* the SCALAR -- see the header */
    const float x = pSrc->f04;
    const float y = pSrc->f08;
    const float z = pSrc->f0C;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float ww = w * w;

    const float yz2 = zz + yy;      /* the two sums the original keeps live */
    const float norm = ((ww + yz2) + xx);

    float s;

    /* The original's test is `fcom 0.0` + `test ah,0x40`, i.e. it takes the
     * zero branch ONLY on the equal flag.  An unordered compare sets C3 as
     * well, so a NaN norm would take it too -- but a NaN norm can only come
     * from a NaN component, and then every product below is NaN anyway. */
    if (norm == BR_K_0008FA54) {
        s = 0.0f;
    } else {
        s = BR_K_0008FA58 / norm;
    }

    pOut->m[0][0] = BR_K_0008FA5C - s * yz2;
    pOut->m[1][1] = BR_K_0008FA5C - s * (zz + xx);
    pOut->m[2][2] = BR_K_0008FA5C - s * (yy + xx);

    {
        /* Each cross term is computed once and then spilled to a float slot
         * in the original before the add and the subtract, so both signs see
         * the SAME rounded product.  Reproduced with the temporaries. */
        const float sx = s * x;
        const float sy = s * y;
        const float sz = s * z;

        const float xy = y * sx;    /* s*x*y */
        const float zw = sz * w;    /* s*z*w */
        pOut->m[1][0] = xy - zw;
        pOut->m[0][1] = xy + zw;

        {
            const float xz = z * sx;   /* s*x*z */
            const float yw = w * sy;   /* s*y*w */
            pOut->m[2][0] = xz + yw;
            pOut->m[0][2] = xz - yw;
        }
        {
            const float yz = z * sy;   /* s*y*z */
            const float xw = w * sx;   /* s*x*w */
            pOut->m[2][1] = yz - xw;
            pOut->m[1][2] = yz + xw;
        }
    }

    pOut->m[3][0] = pSrc->f10;
    pOut->m[3][1] = pSrc->f14;
    pOut->m[3][2] = pSrc->f18;

    pOut->m[0][3] = 0.0f;
    pOut->m[1][3] = 0.0f;
    pOut->m[2][3] = 0.0f;
    pOut->m[3][3] = 1.0f;
}

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
    /* orig lea ecx,[this+0x7c8] then four `mov [ecx+n],eax` (the stosd zero),
     * with the 0x7b8 immediates filling the lea delay slot. Field stores
     * fold to [this+disp] and let `mov eax,9` steal eax. */
    pThis->f7C8[0] = 0;
    pThis->f7C8[1] = 0;
    pThis->f7C8[2] = 0;
    pThis->f7C8[3] = 0;

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

/* 0x10069A90 */
/* WHAT IT DOES: brings a settings block into existence with everything at its
 * default, and hands it back. It is the constructor; all the work is the
 * defaulting above. */
/* The original is __thiscall: `this` arrives in ecx and nothing is pushed.
 * BR_THISCALL1 spells that as __fastcall, which for a single pointer argument
 * is the same convention byte for byte -- and that is what lets 0x10069A60
 * below tail-jump straight into it.  BrCtrlCfgInit must carry the convention
 * too, otherwise the inner call here compiles as a cdecl push/add-esp pair
 * where the original passes in the register. */
/* @implements 0x10062B00 glide BrCtrlCfgCtor */
BrCtrlCfg *BR_THISCALL1 BrCtrlCfgCtor(BrCtrlCfg *pThis)
{
    BrCtrlCfgInit(pThis);
    return pThis;
}

/* 0x10069A60 */
/* WHAT IT DOES: sets up the one settings block the whole game shares, so
 * every part of the game asking "what did the player choose?" has something
 * to read before the settings file is loaded over the top. */
/* @implements 0x10062AD0 glide BrCtrlCfgInitGlobal */
/* @n64 0x8022AF64 located */
BrCtrlCfg *BrCtrlCfgInitGlobal(void)
{
    return BrCtrlCfgCtor(&g_BrCtrlCfg);
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

/* 0x10069AA0 */
/* WHAT IT DOES: throws away the player's edits to one control layout and puts
 * that layout back to the shipped bindings. Only layouts 1, 2 and 3 can be
 * named; every other number, including nonsense, resets the first layout. */
/* @implements 0x10069AA0 d3d BrCtrlCfgLoadDefaults */
/* A SWITCH with a constant index in every arm, not one indexed assignment.
 * The original has FOUR fully duplicated `rep movsd` blocks, each with its
 * own epilogue: a distinct source address (stride 0xA8) and a distinct
 * destination displacement (0, 0xA8, 0x150, 0x1F8).  Computing the index
 * once and assigning `profile[k]` produces index arithmetic instead, and
 * loses the whole shape.  The dispatch is `dec eax; je` three times --
 * a switch compare chain on 1, 2, 3 with everything else, including 0,
 * falling to the default arm.
 *
 * Thiscall: pThis in ecx, the profile number at [esp+4], `ret 4`. */
#ifdef BR_MATCHING_BUILD
void __fastcall BrCtrlCfgLoadDefaults(BrCtrlCfg *pThis,
                                      BrCtrlProfileArg profile)
{
    switch (profile.v) {
    case 1:
        pThis->profile[1] = g_BrCtrlDefaults[1];
        break;
    case 2:
        pThis->profile[2] = g_BrCtrlDefaults[2];
        break;
    case 3:
        pThis->profile[3] = g_BrCtrlDefaults[3];
        break;
    default:
        pThis->profile[0] = g_BrCtrlDefaults[0];
        break;
    }
}
#else
void BrCtrlCfgLoadDefaults(BrCtrlCfg *pThis, int32_t profile)
{
    const int k = BrCtrlProfileIndex(profile);
    pThis->profile[k] = g_BrCtrlDefaults[k];
}
#endif

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
    const int            k    = BrCtrlProfileIndex(profile);
    BrCtrlProfile       *pP   = &pThis->profile[k];
    const BrCtrlProfile *pDef = &g_BrCtrlDefaults[k];
    int slot;

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

/* 0x10069BC0 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says what KIND of thing an action is bound to -- keyboard,
 * joystick button or joystick axis -- for one action of one control layout,
 * which is how the redefine screen knows which sort of label to draw. */
/* @implements 0x10069BC0 d3d BrFn10069BC0 */
#ifdef BR_MATCHING_BUILD
/* thiscall: the config is in ecx and both selectors are on the stack, so the
 * `kind` argument is struct-wrapped to keep __fastcall out of edx -- the same
 * trick BrCtrlCfgLoadDefaults uses above.
 *
 * The four arms are written out IN FULL, and that is the whole shape of this
 * function. Factoring the profile choice into a helper (the portable body
 * below does exactly that) collapses them into one indexed load and loses
 * half the function. The original also folds the profile into the ROW index
 * before scaling -- `(key + 28*k) * 3` over one flat table -- rather than
 * indexing a profile and then a row, which is why each arm adds its own
 * literal to `key`. */
int32_t BR_THISCALL1 BrFn10069BC0(void *pThis, BrCtrlKindArg kind,
                                  BrCtrlKeyArg key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    switch (kind.v) {
    case 1:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x1C][0] & 0xFF00u);
    case 2:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x38][0] & 0xFF00u);
    case 3:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x54][0] & 0xFF00u);
    }
    return (int32_t)(pCfg->profile[0].e[key.v][0] & 0xFF00u);
}
#else
int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);

    return (int32_t)(pCfg->profile[k].e[key][0] & 0xFF00u);
}
#endif

/* 0x10069C30 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says WHICH key, button or axis an action is bound to, as a
 * bare number, paired with the kind reported above. The two answers are not
 * symmetric -- for the keyboard layout it never looks at the axis case at
 * all -- so a caller has to know the kind before the number means anything. */
/* @implements 0x10069C30 d3d BrFn10069C30 */
#ifdef BR_MATCHING_BUILD
/* Same thiscall shape and the same written-out arms as BrFn10069BC0 above:
 * both stack arguments struct-wrapped, and each arm folds its own literal
 * into the flat row index rather than picking a profile first.
 *
 * The 0x8000 test exists ONLY on the 1/2/3 arms. The fall-through arm reads a
 * plain byte with `mov al,[..]` and never looks at the high half, which is why
 * it is written as a byte-typed read rather than as the shared expression with
 * the test skipped. VC5 cross-jumps the tails of arms 2 and 3 by itself (arm 3
 * ends in a `jmp` into arm 2) and leaves arm 1 with its own copy -- that is
 * the compiler's layout, not a difference in how the three are spelled. */
uint8_t BR_THISCALL1 BrFn10069C30(void *pThis, BrCtrlKindArg kind,
                                  BrCtrlKeyArg key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    switch (kind.v) {
    case 1: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x1C][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 2: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x38][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 3: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x54][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    }
    return (uint8_t)pCfg->profile[0].e[key.v][0];
}
#else
uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);
    const uint16_t   v    = pCfg->profile[k].e[key][0];

    if (k != 0 && v >= 0x8000u)
        return (uint8_t)(v >> 8);
    return (uint8_t)v;
}
#endif

/* =====================================================================
 * 3. Replay recorder
 * ===================================================================== */

/* Explicitly initialised (rather than left tentative) so it lands in .bss
 * with natural alignment; a 12 MiB common symbol makes some linkers
 * over-align the whole section. */
BrReplaySlot g_BrReplayBuf[BR_REPLAY_PLAYERS * BR_REPLAY_FRAMES] = { { { { 0 } }, { 0 } } }; /* 0x10B50308 */
int32_t      g_BrReplayCount[BR_REPLAY_PLAYERS];                  /* 0x10B502E8 */
int32_t      g_BrReplayOn;                                        /* 0x11750308 */
int32_t      g_BrReplayCursor[BR_REPLAY_PLAYERS];                 /* 0x11750310 */

int32_t  g_BrX0AA010;
int32_t  g_BrX06909B4;
int32_t  g_BrX06909E0;
uint32_t g_BrX18ABAD0;

/* Byte access into an untyped car record, the slice2_17.h convention. */
#define BR_CAR_F32(p, off) (*(float *)(void *)((unsigned char *)(p) + (off)))
#define BR_CAR_I32(p, off) (*(int32_t *)(void *)((unsigned char *)(p) + (off)))
#define BR_CAR_U8(p, off)  (*((unsigned char *)(p) + (off)))

/* `mov ecx,8; cmp eax,2; je L; cmp eax,4; jne M; L: mov ecx,1` */
static int BrReplayActiveCount(void)
{
    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4)
        return 1;
    return 8;
}

/* 0x1006AAB0 */
/* WHAT IT DOES: writes down where one car is and which way it is facing, into
 * that car's slot for the current replay frame. It does nothing if recording
 * is off, if playback is running, or if this car has already filled its
 * allowance of frames -- the recording simply stops rather than wrapping. */
/* @implements 0x1006AAB0 d3d BrReplayRecord */
void BrReplayRecord(void *pCar)
{
    int32_t iPlayer;
    int32_t frame;

    iPlayer = BR_CAR_I32(pCar, BR_S42_CAR_OFF_INDEX);

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    frame = g_BrReplayCount[iPlayer];
    if (frame >= (int32_t)BR_REPLAY_FRAMES)
        return;

    /* orig pushes esi only on this path (`lea esi,[eax*8+g_BrReplayBuf]`
     * must survive RecordToState). Nested so the 0xa0 state is the slow
     * path's frame, not a prologue that also saves esi. */
    {
        BrCarState state;
        BrCarRecordToState(&state, pCar);
        BrCarStatePack(
            &g_BrReplayBuf[((uint32_t)iPlayer << 16) + (uint32_t)frame].rec,
            &state);
    }
}

/* 0x1006AB20 */
/* WHAT IT DOES: moves the recording on by one frame once every car has been
 * written down, stopping each car's count at the end of its allowance. In the
 * two single-car modes it also nudges the playback position along, so the
 * recording and the thing watching it stay together. */
/* @implements 0x1006AB20 d3d BrReplayAdvance */
void BrReplayAdvance(void)
{
    int n;
    int32_t *p;
    int32_t left;

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    /* Same open-coded 8-or-1 as Reset: orig `mov ecx,8; cmp esi,2; je;
     * cmp esi,4; jne; mov ecx,1` then `test ecx,ecx / jle`. A helper call
     * is the extra `call` in the bag. */
    n = 8;
    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4)
        n = 1;

    /* orig `test ecx,ecx; jle` THEN `mov eax,&count; mov edx,ecx`. Setup
     * must sit inside the taken arm so it does not hoist above the jle. */
    if (n > 0) {
        p = g_BrReplayCount;
        left = n;
        do {
            if (*p < (int32_t)BR_REPLAY_FRAMES)
                ++*p;
            ++p;
        } while (--left);
    }

    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4) {
        /* orig: eax=count[1], ecx=cursor[1], `dec eax; cmp ecx,eax; jge;
         * mov eax,ecx; inc eax; store`. Reuse the count register as the
         * store source so the copy is `mov eax,ecx` not `inc` in place. */
        int32_t a = g_BrReplayCount[1];
        int32_t b = g_BrReplayCursor[1];
        if (b < --a)
            g_BrReplayCursor[1] = ++b;
    }
}

/* 0x1006ABD0 */
/* WHAT IT DOES: puts a car where the recording says it was, for the frame the
 * replay is currently showing. It also fakes the car's speed by looking ahead
 * to the next recorded frame and measuring how far the car is about to move,
 * so anything driven by speed -- engine note, wheel spin -- still behaves;
 * near the very end of the recording, with no next frame to look at, the
 * speed is left as it was. In one playback mode a handful of the car's
 * damage or effect flags are cleared as well. */
/* @implements 0x1006ABD0 d3d BrReplayApply */
void BrReplayApply(void *pCar, int32_t iPlayer)
{
    BrCarState    state;
    BrReplaySlot *pSlot;
    unsigned char z;

    pSlot = &g_BrReplayBuf[((uint32_t)iPlayer << 16)
                           + (uint32_t)g_BrReplayCursor[iPlayer]];

    BrCarStateUnpack(&state, &pSlot->rec);

    /* orig `mov edx,[car+0xFF4]; mov [state.f78],edx` -- dword copy, not
     * fld/fstp. Unpack leaves f78 alone. */
    *(int32_t *)&state.f78 = BR_CAR_I32(pCar, BR_S42_CAR_OFF_F0FF4);
    BrCarRecordFromState(pCar, &state);

    if (g_BrX06909E0 == 2) {
        /* orig `xor al,al` then nine `mov [esi+off],al`. 0x364/0x365 skipped;
         * 0x36C is written third. */
        z = 0;
        BR_CAR_U8(pCar, 0x362) = z;
        BR_CAR_U8(pCar, 0x363) = z;
        BR_CAR_U8(pCar, 0x36C) = z;
        BR_CAR_U8(pCar, 0x366) = z;
        BR_CAR_U8(pCar, 0x367) = z;
        BR_CAR_U8(pCar, 0x368) = z;
        BR_CAR_U8(pCar, 0x369) = z;
        BR_CAR_U8(pCar, 0x36A) = z;
        BR_CAR_U8(pCar, 0x36B) = z;
    }

    /* orig reloads both cursor and count from [iPlayer*4+disp], then
     * `add ebx,0x18` for the next slot rather than rebuilding the index. */
    if (g_BrReplayCursor[iPlayer] < g_BrReplayCount[iPlayer] - 2) {
        BrCarStateUnpack(&state, &pSlot[1].rec);

        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 0) =
            (state.f10 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 0)) * BR_K_0008FAA8;
        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 4) =
            (state.f14 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 4)) * BR_K_0008FAA8;
        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 8) =
            (state.f18 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 8)) * BR_K_0008FAA8;
    }
}

/* 0x1006AD10 */
/* WHAT IT DOES: works the replay transport from the buttons the player is
 * holding -- play, step one frame either way, jump ten frames either way --
 * and moves every car's playback position by the amount decided, stopping at
 * the two ends of the recording. Holding a jump beats holding a single step,
 * and asking for play beats both. Letting go of everything while scrubbing
 * leaves the replay paused. */
/* @implements 0x1006AD10 d3d BrReplaySeek */
void BrReplaySeek(void)
{
    const uint32_t bits = g_BrX18ABAD0;
    int32_t        step = 0;
    int32_t        state;
    int            i;

    if (bits & 0x00200000u) {
        state = 3; step = 1;  g_BrX06909E0 = 3;
    } else if (bits & 0x00400000u) {
        state = 3; step = -1; g_BrX06909E0 = 3;
    } else {
        state = g_BrX06909E0;
    }

    /* The second pair is tested unconditionally and overrides the first. */
    if (bits & 0x00800000u) {
        state = 3; step = 10;  g_BrX06909E0 = 3;
    } else if (bits & 0x01000000u) {
        state = 3; step = -10; g_BrX06909E0 = 3;
    }

    if (bits & 0x00100000u) {
        state = 1; g_BrX06909E0 = 1;
    }

    if (state == 1) {
        step = 1;                   /* `mov esi,eax` with eax == 1 */
    } else if (state == 3 && step == 0) {
        g_BrX06909E0 = 2;
    }

    for (i = 0; i < BR_REPLAY_PLAYERS; ++i) {
        int32_t v = g_BrReplayCursor[i] + step;

        /* Store first, clamp after -- exactly as the original does. */
        g_BrReplayCursor[i] = v;
        if (v < 0) {
            g_BrReplayCursor[i] = 0;
        } else {
            const int32_t hi = g_BrReplayCount[i] - 1;
            if (v > hi)
                g_BrReplayCursor[i] = hi;
        }
    }
}

/* =====================================================================
 * 4. 0x1006AE20
 * ===================================================================== */

BrFxRecord g_BrFx1750338[BR_FX_RECORDS];
uint32_t   g_BrFx1754E50[BR_FX_PAIRS][2];
int32_t    g_BrX1754E38;
int32_t    g_BrX17554A0, g_BrX17554A4;
int32_t    g_BrX17554C8, g_BrX17554CC;
int32_t    g_BrX17554D0, g_BrX17554D4;
int32_t    g_BrX17554D8, g_BrX17554DC;
int32_t    g_BrX17554E0, g_BrX17554E4;

void BrFxClearAll(void)
{
    int i;

    g_BrX17554C8 = 0;
    g_BrX17554A0 = 0;
    g_BrX17554CC = 0;
    g_BrX17554A4 = 0;
    g_BrX17554D0 = 0;
    g_BrX17554D8 = 0;
    g_BrX17554D4 = 0;
    g_BrX17554DC = 0;

    /* GOTCHA: seven dwords per record, but the cursor advances by eight.
     * f1C is deliberately (or at least reproducibly) left alone. */
    for (i = 0; i < BR_FX_RECORDS; ++i) {
        g_BrFx1750338[i].f00 = 0;
        g_BrFx1750338[i].f04 = 0;
        g_BrFx1750338[i].f08 = 0;
        g_BrFx1750338[i].f0C = 0;
        g_BrFx1750338[i].f10 = 0;
        g_BrFx1750338[i].f14 = 0;
        g_BrFx1750338[i].f18 = 0;
    }

    for (i = 0; i < BR_FX_PAIRS; ++i) {
        g_BrFx1754E50[i][0] = 0;
        g_BrFx1754E50[i][1] = 0;
    }

    g_BrX1754E38 = 0;
    g_BrX17554E4 = 0;
    g_BrX17554E0 = 0;
}

/* =====================================================================
 * 5. Rigid-body force accumulation
 * ===================================================================== */

static BrVec3 BrS42Cross(const BrVec3 *pA, const BrVec3 *pB)
{
    BrVec3 r;
    r.x = pA->y * pB->z - pA->z * pB->y;
    r.y = pA->z * pB->x - pA->x * pB->z;
    r.z = pA->x * pB->y - pA->y * pB->x;
    return r;
}

/* 0x1006AEB0 */
void BrRbAccumOwnForces(BrRbBodyFull *pB)
{
    const BrRbForce *pN;
    /* DEVIATION: the original's `v` is an uninitialised stack slot that is
     * only written on the kind==0 and kind==1 paths, so a kind >= 2 node uses
     * whatever the previous node left there -- and uninitialised stack on the
     * first node.  Reading uninitialised storage is undefined in C, so this
     * starts at zero.  The carry-over between nodes IS reproduced. */
    BrVec3 v;
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;

    for (pN = pB->pForces; pN != NULL; pN = pN->pNext) {
        if (pN->kind == 0) {
            v = pN->f;
        } else if (pN->kind == 1) {
            BrMat4MulVec3Transposed(&v, &pB->m, &pN->f);
        }

        pB->accel.x += v.x;
        pB->accel.y += v.y;
        pB->accel.z += v.z;

        if (pB->mode != 2) {
            BrVec3 r, t;
            BrMat4MulVec3Transposed(&r, &pB->m, &pN->r);
            t = BrS42Cross(&r, &v);
            pB->angAccel.x += t.x;
            pB->angAccel.y += t.y;
            pB->angAccel.z += t.z;
        }
    }
}

/* 0x1006AFF0 */
void BrRbAccumChildForces(BrRbBodyFull *pParent, BrRbBodyFull *pChild)
{
    const BrRbForce *pN;
    /* Same stale-slot construction as above; same DEVIATION. */
    BrVec3 v;
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;

    for (pN = pChild->pForces; pN != NULL; pN = pN->pNext) {
        BrVec3 flat, b;

        if (pN->kind == 0)
            BrMat4MulVec3(&v, &pParent->m, &pN->f);
        if (pN->kind == 1)
            v = pN->f;

        /* Computed BEFORE the force is folded in and BEFORE the f1B4 test,
         * from the pre-accumulation v.  Z is dropped. */
        flat.x = v.x;
        flat.y = v.y;
        flat.z = 0.0f;
        BrMat4MulVec3Transposed(&b, &pParent->m, &flat);

        pChild->accel.x += v.x;
        pChild->accel.y += v.y;
        pChild->accel.z += v.z;

        if (pChild->f1B4 != 0.0f) {
            BrVec3 lever, a, t;

            /* 0xEC..0xF4 == m[3][0..2], the child's translation row. */
            lever.x = pChild->m.m[BR_S42_LEVER_ROW][0];
            lever.y = pChild->m.m[BR_S42_LEVER_ROW][1];
            lever.z = pChild->m.m[BR_S42_LEVER_ROW][2];
            BrMat4MulVec3Transposed(&a, &pParent->m, &lever);

            t = BrS42Cross(&a, &b);
            pParent->angAccel.x += t.x;
            pParent->angAccel.y += t.y;
            pParent->angAccel.z += t.z;
        }
    }
}

/* 0x1006B170 */
/* WHAT IT DOES: turns all the pushes and twists that have been piled onto a
 * physical body this frame into how fast it is about to speed up and how fast
 * it is about to start spinning -- heavier bodies respond less, and the shape
 * of the body decides how readily it turns. The forces on the four bodies
 * attached to it are folded in too, but only sideways and forwards: the
 * up-and-down direction ignores them entirely, which looks like an oversight
 * in the original rather than an intention. */
/* @implements 0x1006B170 d3d BrRbSolveAccel */
/* @n64 0x8025980C located */
void BrRbSolveAccel(BrRbBodyFull *pB)
{
    BrVec3 t, u, w;

    BrMat4MulVec3(&t, &pB->m, &pB->accel);

    /* The original stores the rotated force back before summing, in the order
     * z, x, y.  Kept because a child that aliases pB would see it. */
    pB->accel.z = t.z;
    pB->accel.x = t.x;
    pB->accel.y = t.y;

    /* orig x: fld child[3], fadd [2],[1],[0], fadd t.x.  y: fld child[0],
     * fadd [1],[2],[3], fadd t.y.  Z never sees the children.  Named child
     * locals spill six extra stack movs. */
    t.x = ((((pB->child[3]->accel.x + pB->child[2]->accel.x)
             + pB->child[1]->accel.x) + pB->child[0]->accel.x) + t.x)
          / pB->mass;
    t.y = ((((pB->child[0]->accel.y + pB->child[1]->accel.y)
             + pB->child[2]->accel.y) + pB->child[3]->accel.y) + t.y)
          / pB->mass;
    t.z = t.z / pB->mass;

    BrMat4MulVec3Transposed(&pB->accel, &pB->m, &t);

    BrMat4MulVec3(&u, &pB->m, &pB->angAccel);
    BrMat3MulVec3(&w, &pB->invInertia, &u);
    BrMat4MulVec3Transposed(&pB->angAccel, &pB->m, &w);
}

/* 0x1006B260 BrRbAccumAll now lives in src/core/driving/br_rbaccum.c. */

/* The body of 0x1006B430 and 0x1006B510, which differ only in where the
 * point comes from. */
static BrVec3 BrS42VelAt(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pP)
{
    BrVec3 r, sum;
    double cx, cy, cz;

    BrMat4MulVec3Transposed(&r, &pB->m, pP);

    /* All three routines write pB->vel into *pOut and then read it back to
     * form the sum.  In 0x1006B340 that intermediate is the FINAL content of
     * pOut until the closing matrix multiply overwrites it, so the store is
     * kept here rather than folded away. */
    *pOut = pB->vel;

    /* SPILL MAP, checked in all three callers -- 0x1006B510, 0x1006B430 and
     * 0x1006B340 -- because a shared C body is only honest if the bodies it
     * stands for agree:
     *
     *   r comes back from 0x10074770 through a stack BrVec3, so it IS
     *   float-rounded, and reading it out of `r` here reproduces that.
     *
     *   The six products and three differences of the cross stay in x87
     *   registers.  The ONLY store among them is `fst dword [esp+0x14]` at
     *   1006B5C0 (0x1006B510) and 1006B4E4 (0x1006B430) -- and that slot is
     *   never reloaded, so it rounds nothing; the add at 1006B5CB takes the
     *   register copy `fst` left behind.  0x1006B340 has no such store at
     *   all.  So all three cross components reach their add unrounded.
     *
     *   Each sum is stored exactly once -- straight into pOut for the first
     *   two, into a stack BrVec3 for 0x1006B340 -- so it rounds to float
     *   there and nowhere earlier.  Returning a BrVec3 by value is that
     *   store.
     *
     * This is why the cross is written out here instead of calling
     * BrS42Cross: that helper returns a BrVec3, which would round all three
     * components a step early.  BrS42Cross is left alone because its other
     * caller (BrRbAccumOwnForces, 0x1006AEB0) has not been traced for spill
     * points, and widening it on the strength of this function's evidence
     * would be assuming the answer for a function nobody has read. */
    cx = (double)pB->angVel.y * (double)r.z
       - (double)pB->angVel.z * (double)r.y;
    cy = (double)pB->angVel.z * (double)r.x
       - (double)pB->angVel.x * (double)r.z;
    cz = (double)pB->angVel.x * (double)r.y
       - (double)pB->angVel.y * (double)r.x;

    sum.x = (float)(cx + (double)pOut->x);
    sum.y = (float)(cy + (double)pOut->y);
    sum.z = (float)(cz + (double)pOut->z);
    return sum;
}

/* 0x1006B510 */
/* WHAT IT DOES: answers how fast one particular spot on a moving, spinning
 * body is travelling -- which is not the same as how fast the body is
 * travelling, because a spinning body drags its edges along faster than its
 * middle. The spot is given directly. */
/* @implements 0x1006B510 d3d BrRbVelAtPoint */
#ifdef BR_MATCHING_BUILD
/* BrS42VelAt RETURNS A BrVec3, so MSVC will not inline it and the original
 * has no call there -- the whole 137-byte gap is one factored helper. The
 * body is spelled out here; the spill map in BrS42VelAt's banner above is
 * what says the six products and three differences stay in x87 registers.
 *
 * RESIDUE (22 regnorm, -32 bytes, x87 SCHEDULING): the original loads all
 * SIX r components onto the x87 stack up front and interleaves the three
 * cross terms through them -- 16 `fxch` and one `fst` that is never reloaded.
 * Ours evaluates the three in sequence. Was 137 bytes short and 43 regnorm
 * before the helper came inline.
 *
 * DEAD PROBES, do not re-run:
 *  - assigning the three sums straight into *pOut with no temps (worse, -40);
 *  - reordering the three cross terms AND the three adds to y, z, x, which is
 *    the order the original's `fsubp`s complete in and the order its six
 *    `fld`s pair up in -- 5+22 regnorm becomes 7+24, slightly WORSE
 *    (2026-09-03).
 *
 * ‼ AND THE REASON THE ORDER DOES NOT HELP IS NOW UNDERSTOOD. The six `fld`s
 * are hoisted above the `add esp,0xC` that cleans the call's three arguments:
 * once esp moves, every `[esp+N]` displacement for `r` changes, so MSVC loads
 * all six uses of r BEFORE adjusting the stack and then shuffles them with 16
 * `fxch` -- plus one `fld st(2)` duplicate and one dead `fst`. That is a
 * consequence of where the CALL's cleanup sits, not of how the arithmetic is
 * spelled, which is why every term ordering leaves it unchanged. A source
 * lever here would have to move the stack cleanup, not the expressions. */
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    BrVec3 p = *pPoint;
    BrVec3 r;
    float cx, cy, cz;

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    /* FIELD-WISE, not `*pOut = pB->vel`: the struct assignment copies through
     * a `lea` base pointer and costs edi, where the original loads each
     * component at its own displacement off the body. */
    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    /* r FIRST, angVel as the memory operand: the original's products are
     * `fmul dword ptr [esi+0xA0..A8]` against an r component already on the
     * x87 stack. Written the other way round it loads both and uses fmulp.
     * And FLOAT, not double -- double temps spill as `fstp qword [esp]`,
     * and the original never spills a qword. */
    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    pOut->x = cx + pOut->x;
    pOut->y = cy + pOut->y;
    pOut->z = cz + pOut->z;
}
#else
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    BrVec3 p = *pPoint;         /* the original copies it to a stack slot */
    *pOut = BrS42VelAt(pOut, pB, &p);
}
#endif

/* 0x1006B430 */
/* WHAT IT DOES: the same question, but about the spot belonging to another
 * body -- how fast is this body moving at the place where that one is
 * attached. */
/* @implements 0x1006B430 d3d BrRbVelAtBodyPoint */
/* @n64 0x80267410 located */
#ifdef BR_MATCHING_BUILD
/* Same inlining and the same three float facts as BrRbVelAtPoint above --
 * BrS42VelAt returns a BrVec3 and so is never inlined; the velocity copy is
 * field-wise; the products put the rotated point first and stay float.
 *
 * RESIDUE (23 regnorm, -33 bytes): the same x87 scheduling as
 * BrRbVelAtPoint -- the original holds all six products on the stack at once
 * and interleaves the three terms through them; ours evaluates in sequence.
 * Was 138 bytes short before the helper came inline. */
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 r;
    BrVec3 p = pAt->f78;
    float cx, cy, cz;

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    pOut->x = cx + pOut->x;
    pOut->y = cy + pOut->y;
    pOut->z = cz + pOut->z;
}
#else
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 p = pAt->f78;
    *pOut = BrS42VelAt(pOut, pB, &p);
}
#endif

/* 0x1006B340 */
/* WHAT IT DOES: the same again, except the attachment point is flattened --
 * its height is ignored -- and the answer comes back measured against the
 * world rather than against the body. The caller must not pass the same
 * storage in twice, because the answer slot is used as scratch on the way. */
/* @implements 0x1006B340 d3d BrRbVelAtBodyPointXY */
#ifdef BR_MATCHING_BUILD
/* Same inlining and the same three float facts, except the sum goes to a
 * stack BrVec3 rather than into *pOut, because the closing matrix multiply
 * reads it. BrS42VelAt's banner records that this one has no `fst` at all. */
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt)
{
    /* TWO stack vectors, `sub esp,0x18`, not three: the sums go back into
     * `r` -- the original's closing `fstp` triple targets the very slot the
     * transform wrote -- and there is no separate sum variable.
     *
     * RESIDUE (6 regnorm, +10 bytes): the two vectors are SWAPPED in the
     * frame. The original puts the transform's INPUT at the deeper slot
     * (E-0x18) and its output at E-0xC; ours does the reverse, and every
     * displacement follows. Every instruction is otherwise in place, 72
     * against 73. Probed and dead: swapping the declarations, and renaming
     * both locals twice -- the /Od name-hash homing recorded in
     * BrCarGfxReadColour does not apply at /O2. */
    BrVec3 p;
    BrVec3 r;
    float cx, cy, cz;

    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = 0.0f;                 /* the original stores a literal 0 dword */

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    r.x = cx + pOut->x;
    r.y = cy + pOut->y;
    r.z = cz + pOut->z;

    BrMat4MulVec3(pOut, &pB->m, &r);
}
#else
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt)
{
    BrVec3 p, sum;

    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = 0.0f;                 /* the original stores a literal 0 dword */

    sum = BrS42VelAt(pOut, pB, &p);
    BrMat4MulVec3(pOut, &pB->m, &sum);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_10b73668;
extern int DAT_10cf3668;


extern unsigned int DAT_10b7364c;


#endif /* BR_MATCHING_BUILD */
