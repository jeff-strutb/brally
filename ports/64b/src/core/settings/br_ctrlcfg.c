/* br_ctrlcfg.c -- settings.
 *
 * Bringing the shared settings block into existence at its defaults, before
 * the settings file is read over the top of it.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

/* slice3_42.h declares this cdecl; the original is thiscall with one stack
 * argument.  Hide the prototype so the matching body can carry the
 * __fastcall shape with a struct-typed second argument (never
 * register-eligible, so it cannot claim edx). */
#define BrCtrlCfgLoadDefaults BrCtrlCfgLoadDefaults_cdecl
#include "slice3_42.h"
#undef BrCtrlCfgLoadDefaults
typedef struct { int32_t v; } BrCtrlProfileArg;

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
void __fastcall BrCtrlCfgLoadDefaults(BrCtrlCfg *pThis, int32_t profile)
{
    switch (profile) {
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
