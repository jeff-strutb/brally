/* br_ctrlcfg.c -- settings.
 *
 * Bringing the shared settings block into existence at its defaults, before
 * the settings file is read over the top of it.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#include "slice3_42.h"

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
