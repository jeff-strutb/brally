/* br_cdplay.c -- audio.
 *
 * The two CD-music play primitives that sit behind br_cd.c's control surface:
 * the dispatcher that picks a playback route, and the EAR route itself.
 *
 * They live beside br_cd.c rather than inside it because br_cd.c's callers
 * reach BrCdTrackPlay through an implicit declaration; a `void` definition in
 * the same translation unit would conflict with it, and the function text is
 * not ours to change.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include <stdint.h>

/* XSLICE 0x100940A4 -- slice2_11.h's name. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* XSLICE 0x10002870 -- CD play, path A (g_brCdEnabled == 1). */
/* BrSub10002870: prototype in br_funcs.h */
/* XSLICE 0x100027F0 -- CD play, path B. */
/* BrSub100027F0: prototype in br_funcs.h */

/* 0x100027C0 */
/* WHAT IT DOES: starts a music track, choosing between two different ways of
 * playing it depending on how the music was set up. Note the choice is made
 * by testing for one specific setting rather than for "enabled", so any other
 * setting takes the second route. */
/* @implements 0x100027C0 d3d BrCdTrackPlay */
int BrCdTrackPlay(int track)
{
    /* 64-bit core: returns the path's result, which the original leaves in
     * eax through its tail calls and BrCdTrackResume reads. */
    if (DAT_1007b074 == 1) {
        return BrCdTrackRequest(track);
    }
    return BrCdPlayClamped(track);
}

/* ==========================================================================
 * 10. 0x100027F0 -- CD play path B (EAR)
 * ==========================================================================
 *
 * Nested ifs, not early returns: every decline shares the `mov eax,1` tail.
 * The clamped track is stored to 0x10220CD4 before 0x10220C3C is tested, so
 * the selection is recorded even when the medium is down. Clamps are signed
 * (`jge`/`jle`). Both EAR sites are stdcall function-pointer globals
 * (ClearChannel @8, MixEvent @4); cdecl would emit `add esp` after each call.
 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10220CD0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10220C44 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10220C38 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10220CD4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x10220C3C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x100940A8 -- EAR channel id */

/* BrEarClearChannelFn: br_coretypes.h */
/* BrEarMixEventFn: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10575480 _EAR_DLL_ClearChannel@8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1057546C _EAR_DLL_MixEvent@4 */

/* Event block at 0x10220C50. Offsets that this function writes: +0x04 track,
 * +0x08 MixEvent result, +0x1C flags (0x100), +0x42 a cleared word. The
 * address of the block is pushed before those stores. */
/* BrEarMixEvent: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x10220C50 */

/* WHAT IT DOES: pins the requested track to the range the disc currently
 * allows, remembers it as the one that is playing, and if the disc is
 * actually there asks the sound driver to start it. */
/* @implements 0x100027F0 d3d BrCdPlayClamped */
int BrCdPlayClamped(int track)
{
    if (DAT_1007b074) {
        if (g_220CD0) {
            if (track < g_brCdTrackFirst) {
                track = g_brCdTrackFirst;
            }
            if (track > g_brCdTrackLast) {
                track = g_brCdTrackLast;
            }
            g_brCdTrackCur = track;
            if (g_220C3C) {
                g_pfn575480(g_0940A8, 0);
                g_brEarEvent.flags  = 0x100;
                g_brEarEvent.track  = track;
                g_brEarEvent.word42 = 0;
                g_brEarEvent.result = g_pfn57546C(&g_brEarEvent);
            }
        }
    }
    return 1;
}
