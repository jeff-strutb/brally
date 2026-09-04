/* br_dik.c -- the per-frame keyboard poll and its edge detection.
 *
 * RESPONSIBILITY: reading what the player is doing -- pulling the DirectInput
 * keyboard state in and turning it into "pressed just now" answers.
 *
 * Moved here out of the address batches under src/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include "slice3_39.h"
#include "slice6_72.h"   /* BrDInputDev / BR72_DIERR_NOTACQUIRED / Br72Env */

/* WHAT IT DOES: reports which key was newly pressed this frame, taking the
 * first one it finds, or -1 if none were. This is how a "press any key"
 * prompt is answered. */
/* @implements 0x1005FFD0 d3d BrFn1005FFD0 */
int32_t BrFn1005FFD0(void)
{
    int32_t i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        if (g_BrDikEdge[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* WHAT IT DOES: read the keyboard and, if that succeeded, work out which
 * keys changed since last time. The per-frame input poll; a failed read
 * leaves the previous state alone rather than reporting everything as
 * released. */
/* @implements 0x10059020 glide BrDikPollAndEdge */
void BrDikPollAndEdge(void)
{
    if (BrDikGetDeviceState(g_BrDikState) >= 0) {
        BrMenuSub1005FF60();
    }
}

/* WHAT IT DOES: refreshes both sets of input edges for this frame --
 * keyboard keys and controller buttons -- so the menus can tell a fresh
 * press from a held one. */
/* @implements 0x1003E070 d3d BrFn1003E070 */
void BrFn1003E070(void)
{
    BrMenuSub1005FF60();
    BrMenuSub1005FFF0();
}

/* ==========================================================================
 * 0x100771B0
 * ========================================================================== */
/* WHAT IT DOES: reads which keys are down right now. If the game has lost the
 * keyboard to another program in the meantime it quietly grabs it back and
 * reads again. When there is no keyboard device at all it reports what looks
 * like success and leaves the caller's buffer holding whatever was in it
 * before, so keys can appear stuck. */
#ifdef BR_MATCHING_BUILD
typedef int32_t (__stdcall *BrDiGetStateFn)(BrDInputDev *, uint32_t, void *);
typedef int32_t (__stdcall *BrDiAcquireFn)(BrDInputDev *);
/* Orig is `mov eax,[0x118eeee8]` three times, not an env-struct field. */
extern BrDInputDev *g_pBrDik18ABDD0;
#endif

/* WHAT IT DOES: read the whole keyboard state array from DirectInput in one
 * go. Reports failure without touching the caller's buffer when there is no
 * device. */
/* @implements 0x10070490 glide BrDikGetDeviceState */
int32_t BrDikGetDeviceState(uint8_t *pState)
{
#ifdef BR_MATCHING_BUILD
    BrDInputDev *pDev;
    int32_t      hr;
    uint8_t     *p;

    /* Load device first, then pState into esi so the NULL path still
     * `push esi` / `pop esi` (orig does not shrink-wrap the save). */
    pDev = g_pBrDik18ABDD0;
    p = pState;
    if (pDev == NULL) {
        hr = 1;
    } else {
        hr = ((BrDiGetStateFn)pDev->pVtbl->GetDeviceState)(pDev, 0x100u, p);
        if (hr < 0) {
            if (hr == BR72_DIERR_NOTACQUIRED) {
                pDev = g_pBrDik18ABDD0;
                hr = ((BrDiAcquireFn)pDev->pVtbl->Acquire)(pDev);
                if (hr >= 0) {
                    pDev = g_pBrDik18ABDD0;
                    hr = ((BrDiGetStateFn)pDev->pVtbl->GetDeviceState)(pDev, 0x100u, p);
                }
            }
        }
    }
    return hr;
#else
    Br72Env     *pE = g_pBr72Env;
    BrDInputDev *pDev = pE->pDik18ABDD0;
    int32_t      hr;

    if (pDev == NULL) {
        /* GOTCHA: a POSITIVE 1, so a caller testing `>= 0` proceeds with
         * whatever was already in the buffer. */
        return 1;
    }

    hr = pDev->pVtbl->GetDeviceState(pDev, 0x100u, pState);
    if (hr < 0 && hr == BR72_DIERR_NOTACQUIRED) {
        pDev = pE->pDik18ABDD0;             /* the original re-reads it */
        hr = pDev->pVtbl->Acquire(pDev);
        if (hr >= 0) {
            pDev = pE->pDik18ABDD0;         /* and again */
            hr = pDev->pVtbl->GetDeviceState(pDev, 0x100u, pState);
        }
        /* GOTCHA: when the re-acquire fails, ITS hresult is returned, not
         * DIERR_NOTACQUIRED. */
    }
    return hr;
#endif
}
