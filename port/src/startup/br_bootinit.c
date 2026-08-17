/* br_bootinit.c -- see br_bootinit.h.
 *
 * RESPONSIBILITY: bring the game up.  Glide 0x10032530, the first thing the
 * top-level state machine does.
 */
#include "br_bootinit.h"

#include <stddef.h>

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
