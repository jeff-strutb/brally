/* br_bootinit.c -- see br_bootinit.h.
 *
 * RESPONSIBILITY: bring the game up.  Glide 0x10032530, the first thing the
 * top-level state machine does.
 */
#include "br_bootinit.h"

#include <stddef.h>

#ifdef BR_MATCHING_BUILD
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
#else /* !BR_MATCHING_BUILD */

static int32_t s_aSkipped[BR_COLDINIT_COUNT];

/* `fHooked` is the caller's `pfn != NULL`.  Passed as an int rather than as
 * a void * because converting a function pointer to an object pointer is not
 * something C99 defines. */
static int hooked(BrBootColdInitStep step, int fHooked)
{
    if (fHooked) {
        return 1;
    }
    ++s_aSkipped[step];
    return 0;
}

int32_t BrBootColdInit(const BrBootColdInitOps *pOps,
                       int32_t fPlayMusic, int32_t fPlaySfx)
{
    if (pOps == NULL) {
        return 0;
    }

    /* 0x10032530 / 0x10032535 / 0x1003253A.  `push name` then `mov ecx,obj`
     * then `call` -- __thiscall, so the name is the only stack argument and
     * the callee's `ret 4` removes it. */
    if (hooked(BR_COLDINIT_POD_SETNAME, pOps->pfnPodSetName != NULL)) {
        pOps->pfnPodSetName(pOps->pPod, BR_BOOTINIT_POD_NAME);
    }
    /* 0x1003253F / 0x10032544.  Same object, no arguments. */
    if (hooked(BR_COLDINIT_POD_OPEN, pOps->pfnPodOpen != NULL)) {
        pOps->pfnPodOpen(pOps->pPod);
    }

    /* 0x10032549.  cdecl, one argument, `add esp,4`. */
    if (hooked(BR_COLDINIT_100639D0, pOps->pfn100639D0 != NULL)) {
        pOps->pfn100639D0(BR_BOOTINIT_639D0_ARG);
    }

    /* 0x10032553.  cdecl, two arguments pushed right-to-left, so the call
     * reads 0x1006C990("splash.img", 0x2AC7E58B) and `add esp,8` clears
     * both. */
    if (hooked(BR_COLDINIT_1006C990, pOps->pfn1006C990 != NULL)) {
        pOps->pfn1006C990(BR_BOOTINIT_SPLASH_NAME, BR_BOOTINIT_SPLASH_ARG);
    }

    /* 0x10032565 / 0x1003256A / 0x1003256F.  Three no-argument calls with no
     * stack cleanup between them. */
    if (hooked(BR_COLDINIT_1005A480, pOps->pfn1005A480 != NULL)) {
        pOps->pfn1005A480();
    }
    if (hooked(BR_COLDINIT_10071FC0, pOps->pfn10071FC0 != NULL)) {
        pOps->pfn10071FC0();
    }
    if (hooked(BR_COLDINIT_100703D0, pOps->pfn100703D0 != NULL)) {
        pOps->pfn100703D0();
    }

    /* 0x10032574 -- ONE guard over THREE calls.  With PlayMusic=0 the volume
     * tables at 0x10059E00 never run either. */
    if (fPlayMusic != 0) {
        if (hooked(BR_COLDINIT_100028E0, pOps->pfn100028E0 != NULL)) {
            pOps->pfn100028E0(pOps->hWnd);
        }
        if (hooked(BR_COLDINIT_10059E00, pOps->pfn10059E00 != NULL)) {
            pOps->pfn10059E00();
        }
        if (hooked(BR_COLDINIT_10002AF0, pOps->pfn10002AF0 != NULL)) {
            pOps->pfn10002AF0(BR_BOOTINIT_CD_TRACK);
        }
    }

    /* 0x1003259A.  A TAIL JUMP: 0x1006C4D0's return value is this function's.
     * When the guard fails, eax still holds the zero that failed the test. */
    if (fPlaySfx != 0) {
        if (hooked(BR_COLDINIT_1006C4D0, pOps->pfn1006C4D0 != NULL)) {
            return pOps->pfn1006C4D0();
        }
        /* No hook: the tail call did not happen, and there is no value it
         * would have produced.  0 is the same thing the not-taken arm
         * returns; it is not a stand-in for the callee. */
        return 0;
    }
    return 0;
}

int32_t BrBootColdInitSkipped(BrBootColdInitStep step)
{
    if (step < 0 || step >= BR_COLDINIT_COUNT) {
        return 0;
    }
    return s_aSkipped[step];
}

void BrBootColdInitResetForTest(void)
{
    int i;
    for (i = 0; i < BR_COLDINIT_COUNT; i++) {
        s_aSkipped[i] = 0;
    }
}
#endif /* !BR_MATCHING_BUILD */
