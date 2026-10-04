/* br_bootinit.c -- see br_bootinit.h.
 *
 * RESPONSIBILITY: bring the game up.  Glide 0x10032530, the first thing the
 * top-level state machine does.
 */
#include "br_bootinit.h"

#include <stddef.h>

/* The byte-exact form: the eleven calls made directly, in the order the
 * banner in br_bootinit.h lists them.  The port arm below is the same
 * sequence behind an ops table so it can be run with callees missing. */

/* 64-bit core: declared once, in br_globals.h or its struct's header */            /* 0x10AC0810  the POD object  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x1007B074  PlayMusic=      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x105BC72C  the HWND        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x100B55F0  PlaySFX=        */

/* 0x10008D20 is __thiscall with one stack argument: a struct-wrapped
 * __fastcall is the only C spelling that keeps it on the stack. */
typedef struct { const char *psz; } BrPodSetNameArg;
void __fastcall BrPodSetName(void *pThis, BrPodSetNameArg a);   /* 0x10008D20 */
/* BrPodOpen: prototype in br_funcs.h */
/* BrRenderModeRestart: prototype in br_funcs.h */
/* FUN_1006c990: prototype in br_funcs.h */
/* BrLiveryLoadDamage: prototype in br_funcs.h */
/* FUN_10071fc0: prototype in br_funcs.h */
/* FUN_100703d0: prototype in br_funcs.h */
/* BrDispatch_100025C0: prototype in br_funcs.h */
/* BrUiVolumeApply: prototype in br_funcs.h */
/* BrCdTrackPlay: prototype in br_funcs.h */
/* FUN_1006c4d0: prototype in br_funcs.h */

/* WHAT IT DOES: the game's cold start, run once from the first state of the
 * top-level state machine: name and open the POD archive, restart the
 * renderer, put up the splash screen, load the damage bitmaps, bring up
 * DirectInput and the input device, then -- only if music is enabled --
 * start the music backend, apply the volume sliders and play CD track 2,
 * and finally -- only if sound effects are enabled -- create the DirectSound
 * device.  That last call is a tail jump, so it is the only thing whose
 * result leaves this function. */
/* @implements 0x10032530 glide BrBootColdInitRun */
void BrBootColdInitRun(void)
{
    BrPodSetNameArg a;

    a.psz = "BossRally.pod";
    BrPodSetName(&g_brModelMgr, a);
    BrPodOpen(&g_brModelMgr);
    BrRenderModeRestart(3);
    BrImgShowFullScreen("splash.img", 0x2ac7e58b);
    BrLiveryLoadDamage();
    Ctl71FC0_fn(NULL);           /* a thiscall that never reads this */
    FUN_100703d0();
    if (DAT_1007b074 != 0) {
        BrDispatch_100025C0((*(void * *)&g_brOwner5BC72C));
        BrUiVolumeApply();
        BrCdTrackPlay(2);
    }
    if ((*(int *)&DAT_100b51e4[1036]) != 0) {
        BrSndDevOpen();
    }
}
