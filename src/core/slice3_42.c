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

#include "slice3_42.h"

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
    int i;

    for (i = 0; i < BR_CTRL_PROFILES; ++i)
        pThis->profile[i] = g_BrCtrlDefaults[i];

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
/* @implements 0x10069A90 d3d BrCtrlCfgCtor */
BrCtrlCfg *BR_THISCALL1 BrCtrlCfgCtor(BrCtrlCfg *pThis)
{
    BrCtrlCfgInit(pThis);
    return pThis;
}

/* 0x10069A60 */
/* WHAT IT DOES: sets up the one settings block the whole game shares, so
 * every part of the game asking "what did the player choose?" has something
 * to read before the settings file is loaded over the top. */
/* @implements 0x10069A60 d3d BrCtrlCfgInitGlobal */
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
void BrCtrlCfgLoadDefaults(BrCtrlCfg *pThis, int32_t profile)
{
    const int k = BrCtrlProfileIndex(profile);
    pThis->profile[k] = g_BrCtrlDefaults[k];
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
int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);

    return (int32_t)(pCfg->profile[k].e[key][0] & 0xFF00u);
}

/* 0x10069C30 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says WHICH key, button or axis an action is bound to, as a
 * bare number, paired with the kind reported above. The two answers are not
 * symmetric -- for the keyboard layout it never looks at the axis case at
 * all -- so a caller has to know the kind before the number means anything. */
/* @implements 0x10069C30 d3d BrFn10069C30 */
uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);
    const uint16_t   v    = pCfg->profile[k].e[key][0];

    /* The 0x8000 test exists ONLY on the 1/2/3 arms; the fall-through arm
     * reads a plain byte with `mov al,[..]` and never looks at the high
     * half. */
    if (k != 0 && v >= 0x8000u)
        return (uint8_t)(v >> 8);
    return (uint8_t)v;
}

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

/* 0x1006AA50 */
/* WHAT IT DOES: throws away any recording in progress and switches recording
 * off, so the next race starts with an empty replay. How many cars it clears
 * depends on the game mode -- one in the two single-car modes, eight
 * otherwise. */
/* @implements 0x1006AA50 d3d BrReplayReset */
void BrReplayReset(void)
{
    int n;
    int i;

    /* Open-coded: two callers keep BrReplayActiveCount from inlining. */
    n = 8;
    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4)
        n = 1;

    /* The `test ecx,ecx / jle` guard is dead (n is 1 or 8) but is kept as the
     * loop bound so the shape matches. */
    for (i = 0; i < n; ++i)
        g_BrReplayCount[i] = 0;

    g_BrReplayOn = 0;
}

/* 0x1006AAB0 */
/* WHAT IT DOES: writes down where one car is and which way it is facing, into
 * that car's slot for the current replay frame. It does nothing if recording
 * is off, if playback is running, or if this car has already filled its
 * allowance of frames -- the recording simply stops rather than wrapping. */
/* @implements 0x1006AAB0 d3d BrReplayRecord */
void BrReplayRecord(void *pCar)
{
    BrCarState state;
    int32_t    iPlayer;
    int32_t    frame;
    uint32_t   slot;

    iPlayer = BR_CAR_I32(pCar, BR_S42_CAR_OFF_INDEX);

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    frame = g_BrReplayCount[iPlayer];
    if (frame >= (int32_t)BR_REPLAY_FRAMES)
        return;

    /* `shl eax,0x10; add eax,ecx` -- a flat index, not [player][frame]. */
    slot = ((uint32_t)iPlayer << 16) + (uint32_t)frame;

    BrCarRecordToState(&state, pCar);
    BrCarStatePack(&g_BrReplayBuf[slot].rec, &state);
}

/* 0x1006AB20 */
/* WHAT IT DOES: moves the recording on by one frame once every car has been
 * written down, stopping each car's count at the end of its allowance. In the
 * two single-car modes it also nudges the playback position along, so the
 * recording and the thing watching it stay together. */
/* @implements 0x1006AB20 d3d BrReplayAdvance */
void BrReplayAdvance(void)
{
    int n, i;

    if (g_BrReplayOn == 0)
        return;
    if (g_BrX06909B4 != 0)
        return;

    n = BrReplayActiveCount();
    for (i = 0; i < n; ++i) {
        if (g_BrReplayCount[i] < (int32_t)BR_REPLAY_FRAMES)
            g_BrReplayCount[i]++;
    }

    if (g_BrX0AA010 == 2 || g_BrX0AA010 == 4) {
        /* 0x10B502EC is g_BrReplayCount[1] and 0x11750314 is
         * g_BrReplayCursor[1]; both indices are hard-coded in the original. */
        const int32_t limit = g_BrReplayCount[1] - 1;
        if (g_BrReplayCursor[1] < limit)
            g_BrReplayCursor[1] = g_BrReplayCursor[1] + 1;
    }
}

/* 0x1006ABB0 */
/* WHAT IT DOES: sends the replay back to the start -- every car's playback
 * position returns to its first recorded frame. The recording itself is
 * untouched. */
/* @implements 0x1006ABB0 d3d BrReplayRewind */
void BrReplayRewind(void)
{
    int i;
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCursor[i] = 0;
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
    BrCarState state;
    uint32_t   slot;
    int32_t    cursor;

    cursor = g_BrReplayCursor[iPlayer];
    slot   = ((uint32_t)iPlayer << 16) + (uint32_t)cursor;

    BrCarStateUnpack(&state, &g_BrReplayBuf[slot].rec);

    /* BrCarStateUnpack leaves f78 alone (slice2_12.h says so); the original
     * seeds it from the car's own +0x0FF4 before applying the state. */
    state.f78 = BR_CAR_F32(pCar, BR_S42_CAR_OFF_F0FF4);
    BrCarRecordFromState(pCar, &state);

    if (g_BrX06909E0 == 2) {
        /* Nine bytes, in the original's order.  Note 0x364/0x365 are skipped
         * and 0x36C is written third. */
        BR_CAR_U8(pCar, 0x362) = 0;
        BR_CAR_U8(pCar, 0x363) = 0;
        BR_CAR_U8(pCar, 0x36C) = 0;
        BR_CAR_U8(pCar, 0x366) = 0;
        BR_CAR_U8(pCar, 0x367) = 0;
        BR_CAR_U8(pCar, 0x368) = 0;
        BR_CAR_U8(pCar, 0x369) = 0;
        BR_CAR_U8(pCar, 0x36A) = 0;
        BR_CAR_U8(pCar, 0x36B) = 0;
    }

    if (cursor < g_BrReplayCount[iPlayer] - 2) {
        /* The next slot, unpacked over the SAME buffer. */
        BrCarStateUnpack(&state, &g_BrReplayBuf[slot + 1].rec);

        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 0) =
            (state.f10 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 0)) * BR_K_0008FAA8;
        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 4) =
            (state.f14 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 4)) * BR_K_0008FAA8;
        BR_CAR_F32(pCar, BR_S42_CAR_OFF_VEL + 8) =
            (state.f18 - BR_CAR_F32(pCar, BR_S42_CAR_OFF_POS + 8)) * BR_K_0008FAA8;
    }
}

/* 0x1006ACF0 */
/* WHAT IT DOES: the same as the above, for a car that already knows its own
 * number -- it looks the number up on the car rather than being told it. */
/* @implements 0x1006ACF0 d3d BrReplayApplyCar */
void BrReplayApplyCar(void *pCar)
{
    BrReplayApply(pCar, BR_CAR_I32(pCar, BR_S42_CAR_OFF_INDEX));
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
void BrRbSolveAccel(BrRbBodyFull *pB)
{
    BrVec3 t, u, w;
    BrRbBodyFull *c0 = pB->child[0];
    BrRbBodyFull *c1 = pB->child[1];
    BrRbBodyFull *c2 = pB->child[2];
    BrRbBodyFull *c3 = pB->child[3];

    BrMat4MulVec3(&t, &pB->m, &pB->accel);

    /* The original stores the rotated force back before summing, in the order
     * z, x, y.  Kept because a child that aliases pB would see it. */
    pB->accel.z = t.z;
    pB->accel.x = t.x;
    pB->accel.y = t.y;

    /* The two summation orders genuinely differ -- x starts at child[2] and
     * y starts at child[3].  And Z never sees the children at all. */
    t.x = ((((c2->accel.x + c1->accel.x) + c0->accel.x) + c3->accel.x) + t.x)
          / pB->mass;
    t.y = ((((c3->accel.y + c0->accel.y) + c1->accel.y) + c2->accel.y) + t.y)
          / pB->mass;
    t.z = t.z / pB->mass;

    BrMat4MulVec3Transposed(&pB->accel, &pB->m, &t);

    BrMat4MulVec3(&u, &pB->m, &pB->angAccel);
    BrMat3MulVec3(&w, &pB->invInertia, &u);
    BrMat4MulVec3Transposed(&pB->angAccel, &pB->m, &w);
}

/* 0x1006B260 */
/* WHAT IT DOES: one whole physics pass for a body: wipe last frame's answer,
 * add up everything pushing on the body itself and on the four bodies
 * attached to it, and work out the resulting acceleration and spin. This is
 * the step that decides how a car moves this frame. The attached bodies' spin
 * is deliberately not wiped, so theirs carries over from last frame. */
/* @implements 0x1006B260 d3d BrRbAccumAll */
void BrRbAccumAll(BrRbBodyFull *pB)
{
    int k;

    pB->accel.x = 0.0f;
    pB->accel.y = 0.0f;
    pB->accel.z = 0.0f;
    pB->angAccel.x = 0.0f;
    pB->angAccel.y = 0.0f;
    pB->angAccel.z = 0.0f;

    for (k = 0; k < 4; ++k) {
        /* Only accel; the children's angAccel is left as it was. */
        pB->child[k]->accel.x = 0.0f;
        pB->child[k]->accel.y = 0.0f;
        pB->child[k]->accel.z = 0.0f;
    }

    BrRbAccumOwnForces(pB);
    for (k = 0; k < 4; ++k)
        BrRbAccumChildForces(pB, pB->child[k]);
    BrRbSolveAccel(pB);
}

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
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    BrVec3 p = *pPoint;         /* the original copies it to a stack slot */
    *pOut = BrS42VelAt(pOut, pB, &p);
}

/* 0x1006B430 */
/* WHAT IT DOES: the same question, but about the spot belonging to another
 * body -- how fast is this body moving at the place where that one is
 * attached. */
/* @implements 0x1006B430 d3d BrRbVelAtBodyPoint */
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 p = pAt->f78;
    *pOut = BrS42VelAt(pOut, pB, &p);
}

/* 0x1006B340 */
/* WHAT IT DOES: the same again, except the attachment point is flattened --
 * its height is ignored -- and the answer comes back measured against the
 * world rather than against the body. The caller must not pass the same
 * storage in twice, because the answer slot is used as scratch on the way. */
/* @implements 0x1006B340 d3d BrRbVelAtBodyPointXY */
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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_10b73668;
extern int DAT_10cf3668;

/* WHAT IT DOES: initialize the controller-config subsystem and register its atexit handler. */
/* @implements 0x10062AC0 glide BrCtrlCfgBoot */

int BrCtrlCfgBoot(void)

{
  BrCtrlCfgInitGlobal();
  BrAtexit_10069A70();
  return;
}

/* WHAT IT DOES: return a pointer to the primary replay data buffer. */
/* @implements 0x10063B40 glide BrReplayGetBuf */

char * BrReplayGetBuf(void)

{
  return &DAT_10b73668;
}

/* WHAT IT DOES: return the byte size of the current replay (frame count * 0x18). */
/* @implements 0x10063B50 glide BrReplayGetSize */

int BrReplayGetSize(void)

{
  return g_BrReplayCount[0] * 0x18;
}

/* WHAT IT DOES: return a pointer to the secondary replay data buffer. */
/* @implements 0x10063DA0 glide BrReplayGetBuf2 */

char * BrReplayGetBuf2(void)

{
  return &DAT_10cf3668;
}

extern unsigned int DAT_10b7364c;

/* WHAT IT DOES: store how many 0x18-byte replay records fit in a byte count
 * (the inverse of BrReplayGetSize's count * 0x18). */
/* @implements 0x10063DB0 glide BrReplayCountFromBytes */

unsigned int BrReplayCountFromBytes(unsigned int cb)

{
  /* mov eax,edx; shr eax,4 -- the quotient is copied into EAX because the
   * assignment's value is also RETURNED. */
  return DAT_10b7364c = cb / 0x18;
}

#endif /* BR_MATCHING_BUILD */
