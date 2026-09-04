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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

/* XSLICE 0x100940A4 -- slice2_11.h's name. */
extern int g_brCdEnabled;

/* XSLICE 0x10002870 -- CD play, path A (g_brCdEnabled == 1). */
extern void BrSub10002870(int track);
/* XSLICE 0x100027F0 -- CD play, path B. */
extern void BrSub100027F0(int track);

/* 0x100027C0 */
/* WHAT IT DOES: starts a music track, choosing between two different ways of
 * playing it depending on how the music was set up. Note the choice is made
 * by testing for one specific setting rather than for "enabled", so any other
 * setting takes the second route. */
/* @implements 0x100027C0 d3d BrCdTrackPlay */
void BrCdTrackPlay(int track)
{
    if (g_brCdEnabled == 1) {
        BrSub10002870(track);
        return;
    }
    BrSub100027F0(track);
}

#ifdef BR_MATCHING_BUILD
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

extern int g_brCdPlaying;     /* 0x10220CD0 */
extern int g_brCdTrackFirst;  /* 0x10220C44 */
extern int g_brCdTrackLast;   /* 0x10220C38 */
extern int g_brCdTrackCur;    /* 0x10220CD4 */
extern int g_brCdMediaOk;     /* 0x10220C3C */
extern int g_br0940A8;        /* 0x100940A8 -- EAR channel id */

typedef int (__stdcall *BrEarClearChannelFn)(int channel, int flags);
typedef int (__stdcall *BrEarMixEventFn)(void *pEvent);

extern BrEarClearChannelFn g_pfn575480;   /* 0x10575480 _EAR_DLL_ClearChannel@8 */
extern BrEarMixEventFn     g_pfn57546C;   /* 0x1057546C _EAR_DLL_MixEvent@4 */

/* Event block at 0x10220C50. Offsets that this function writes: +0x04 track,
 * +0x08 MixEvent result, +0x1C flags (0x100), +0x42 a cleared word. The
 * address of the block is pushed before those stores. */
typedef struct BrEarMixEvent {
    short f00;            /* +0x00  0x10220C50 */
    short f02;            /* +0x02 */
    int   track;          /* +0x04  0x10220C54 */
    int   result;         /* +0x08  0x10220C58 */
    char  pad0C[0x10];
    int   flags;          /* +0x1C  0x10220C6C */
    char  pad20[0x22];
    short word42;         /* +0x42  0x10220C92 */
} BrEarMixEvent;

extern BrEarMixEvent g_brEarEvent;        /* 0x10220C50 */

/* WHAT IT DOES: pins the requested track to the range the disc currently
 * allows, remembers it as the one that is playing, and if the disc is
 * actually there asks the sound driver to start it. */
/* @implements 0x100027F0 d3d BrCdPlayClamped */
int BrCdPlayClamped(int track)
{
    if (g_brCdEnabled) {
        if (g_brCdPlaying) {
            if (track < g_brCdTrackFirst) {
                track = g_brCdTrackFirst;
            }
            if (track > g_brCdTrackLast) {
                track = g_brCdTrackLast;
            }
            g_brCdTrackCur = track;
            if (g_brCdMediaOk) {
                g_pfn575480(g_br0940A8, 0);
                g_brEarEvent.flags  = 0x100;
                g_brEarEvent.track  = track;
                g_brEarEvent.word42 = 0;
                g_brEarEvent.result = g_pfn57546C(&g_brEarEvent);
            }
        }
    }
    return 1;
}
#endif /* BR_MATCHING_BUILD */
