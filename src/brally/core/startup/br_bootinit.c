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

extern unsigned char g_AC0810[];            /* 0x10AC0810  the POD object  */
extern int           g_brCdEnabled;         /* 0x1007B074  PlayMusic=      */
extern void         *DAT_105bc72c;          /* 0x105BC72C  the HWND        */
extern int           DAT_100b55f0;          /* 0x100B55F0  PlaySFX=        */

/* 0x10008D20 is __thiscall with one stack argument: a struct-wrapped
 * __fastcall is the only C spelling that keeps it on the stack. */
typedef struct { const char *psz; } BrPodSetNameArg;
void __fastcall BrPodSetName(void *pThis, BrPodSetNameArg a);   /* 0x10008D20 */
void __fastcall BrPodOpen(void *pThis);                           /* 0x10008AB0 */
void BrRenderModeRestart(int mode);                               /* 0x100639D0 */
void FUN_1006c990(const char *pszImage, unsigned int key);        /* 0x1006C990 */
void BrLiveryLoadDamage(void);                                    /* 0x1005A480 */
void FUN_10071fc0(void);                                          /* 0x10071FC0 */
void FUN_100703d0(void);                                          /* 0x100703D0 */
void BrDispatch_100025C0(void *hWnd);                             /* 0x100028E0 */
void BrUiVolumeApply(void);                                       /* 0x10059E00 */
void BrCdTrackPlay(int track);                                    /* 0x10002AF0 */
void FUN_1006c4d0(void);                                          /* 0x1006C4D0 */

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
    BrPodSetName(g_AC0810, a);
    BrPodOpen(g_AC0810);
    BrRenderModeRestart(3);
    FUN_1006c990("splash.img", 0x2ac7e58b);
    BrLiveryLoadDamage();
    FUN_10071fc0();
    FUN_100703d0();
    if (g_brCdEnabled != 0) {
        BrDispatch_100025C0(DAT_105bc72c);
        BrUiVolumeApply();
        BrCdTrackPlay(2);
    }
    if (DAT_100b55f0 != 0) {
        FUN_1006c4d0();
    }
}
