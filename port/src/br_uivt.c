/* br_uivt.c -- 0x10048470 (page constructor), 0x10047EB0 (control vtable
 * +0x34) and 0x10047FB0 (control vtable +0x38), over the BrUiPage_ / BrUiCtl_
 * struct model.
 *
 * See br_uivt.h for why these three coexist with slice3_32.c's byte-image
 * ports of the same addresses. This file is the second transcription; both
 * were derived from the disassembly independently and agree.
 *
 * ==========================================================================
 * 0x10048470 -- the page constructor (77 bytes)
 * ==========================================================================
 *
 * Recovered writes, in the original's own order:
 *
 *   +0x010 = 0                     +0x00C = 0
 *   +0x014 = 0 (a WORD)            rep stosd 0xC8 dwords at +0x018 <- 0
 *   +0x338 = 0                     +0x340 = 0
 *   +0x33C = 0                     +0x344 = 0 (a WORD)
 *   +0x000 = vtable 0x1008F6F8     +0x346 = 0 (a WORD)
 *   +0x004 = 0
 *   +0x008 = 0
 *
 * THE FILL IS A ZERO FILL. `xor eax,eax` at 0x1004847B precedes the
 * `rep stosd` at 0x100484A2 with nothing in between that touches eax, so this
 * one really does write 0 -- unlike four of the nine fills in the CONTROL
 * constructor next door, which write 0xFFFFFFFF. The count 0xC8 == 200 and
 * the base +0x18 are what pin BR_UI_PAGE_CTL_MAX at 200: 0x18 + 200*4 ==
 * 0x338, exactly where the first float begins.
 *
 * GOTCHA, reproduced: the two bytes at +0x016 are the ONLY part of the 0x348
 * object the constructor does not write. `operator new` does not zero, so
 * they are indeterminate on exit. slice3_32.c found and preserved the same.
 *
 * ==========================================================================
 * 0x10047EB0 -- control vtable +0x34, set the text (247 bytes)
 * ==========================================================================
 *
 * `ret 0x10`, so __thiscall plus four stack arguments. The whole body is a
 * setup of the BrTextBox at control +0x2B5C, two dispatches through that
 * box's vtable, and then a rectangle read back OUT of the box.
 *
 * The read-back is the part that matters and the part a careless port loses:
 * +0x2F66 (width) and +0x2F68 (height) are ZEROED before the dispatch and
 * RE-READ after it. The measuring method the dispatch selects is what fills
 * them in, and its results decide the control's +0x48, +0x4A and +0x5C. A
 * port that computed those from the arguments instead would produce plausible
 * numbers that never change when the text does.
 *
 * The dispatch: kind == 3 takes the box's vtable +0x08, anything else takes
 * +0x04. Under slice3_39.h those are BrTextBoxMeasureB (the large digits
 * font) and BrTextBoxMeasureA (the small proportional font).
 *
 * GOTCHA, reproduced: when a2 bit 0 is set the box's vtable +0x28 is called
 * and its float return is DISCARDED (`fstp st(0)`). Under slice3_39.h that is
 * BrTextBoxCentreX, which also stores its result in the box -- so the call is
 * made purely for the side effect.
 *
 * GOTCHA, reproduced: bit 0 is tested on the ARGUMENT a2, not on the item
 * flags field a2 was just OR-ed into. The two differ whenever the field
 * already had bit 0 set.
 *
 * ==========================================================================
 * 0x10047FB0 -- control vtable +0x38, place the control (92 bytes)
 * ==========================================================================
 *
 * `ret 0x20`, so __thiscall plus eight stack arguments. Six stores and three
 * OR-into-place updates, no reads and no calls.
 *
 * GOTCHA, reproduced: the last argument is a WORD and goes to TWO
 * destinations, +0x2A40 and +0x1E20C. Both are 16-bit stores of the low half.
 *
 * GOTCHA: a4 and a5 (2 and 5 at every call site in the corpus) are accepted
 * and never used. They are kept in the signature because the vtable slot's
 * arity is fixed by `ret 0x20`.
 *
 * ORDERING: 0x10047FB0 must run BEFORE 0x10047EB0 on the same control, and
 * every builder does call them in that order. +0x3C and +0x40 are written
 * here and read there -- the text setup copies them into the box and
 * truncates +0x40 into the control's +0x54.
 */
#include "br_uivt.h"
#include <string.h>

/* ==========================================================================
 * 0x10048470 -- the page constructor
 * ========================================================================== */

const BrUiPageVtbl_ g_brUiPageVtbl_1008F6F8;
const void *g_pBrUiPageVtbl = &g_brUiPageVtbl_1008F6F8;

BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis)
{
    int i;

    if (pThis == NULL) {
        return NULL;                /* DEVIATION: the original faults. */
    }

    pThis->f10  = 0;
    pThis->cCtl = 0;
    /* +0x016 is deliberately NOT written -- see the GOTCHA in the banner.
     * This port cannot leave it indeterminate the way the original does
     * without reading uninitialised memory, so it is left to whatever the
     * caller's allocation held, exactly like the original. */
    pThis->fX = 0.0f;               /* the original stores an integer 0 */
    pThis->fY = 0.0f;               /* likewise                         */

    pThis->pVtbl = g_pBrUiPageVtbl;
    pThis->pfn04 = NULL;
    pThis->pfn08 = NULL;
    pThis->pfn0C = NULL;

    /* `mov ecx,0xC8 / rep stosd` from +0x18, with eax == 0. */
    for (i = 0; i < BR_UI_PAGE_CTL_MAX; ++i) {
        pThis->apCtl[i] = NULL;
    }

    pThis->pOwner = NULL;
    pThis->cSel   = 0;
    pThis->iSel   = 0;

    return pThis;                   /* original returns edx in eax */
}

/* ==========================================================================
 * 0x10047FB0 -- control vtable +0x38
 * ========================================================================== */

void BrUiCtlPlace_10047FB0(BrUiCtl_ *pThis, BrPhase_ *pOwner,
                           float x, float y, int32_t flags,
                           int32_t a4, int32_t a5, int32_t a6, int32_t a7)
{
    if (pThis == NULL) {
        return;                     /* DEVIATION: the original faults. */
    }

    pThis->pOwner = pOwner;
    pThis->flags1C |= flags;
    pThis->flags24 |= a4;
    pThis->flags28 |= a5;
    pThis->f2968 = a6;

    /* The original stores the two floats as raw dwords, +0x40 first. */
    pThis->y = y;
    pThis->x = x;

    /* One word, two destinations. */
    pThis->aStepId[0] = (uint16_t)a7;
    pThis->w1E20C     = (uint16_t)a7;
}

/* ==========================================================================
 * 0x10047EB0 -- control vtable +0x34
 * ========================================================================== */

void BrUiCtlSetText_10047EB0(BrUiCtl_ *pThis, const void *pText,
                             int32_t a2, int32_t a3, const void *pStyle)
{
    BrTextBox           *pBox;
    const BrTextBoxVtbl *pV;
    const int32_t       *pRc;
    uint8_t              bKind;
    int32_t              nY;

    if (pThis == NULL || pStyle == NULL) {
        return;                     /* DEVIATION: the original faults. */
    }

    pBox  = &pThis->aText[0];
    pV    = pBox->pVtbl;            /* loaded before the stores, as in the
                                     * original (`mov ebx,[ebp+0x2B5C]`) */
    pRc   = (const int32_t *)pStyle;
    bKind = (uint8_t)a3;

    /* `repne scasb` + `rep movs`: strlen(pText) + 1 bytes, i.e. the NUL is
     * copied. The original checks neither the source for NULL nor the
     * destination for room.
     *
     * DEVIATION (memory safety): NULL is skipped, and the copy is bounded by
     * the buffer the element constructor establishes (0x100 dwords from
     * element +0x09). An over-long string is truncated and NUL-terminated
     * where the original would run off the end of the item block and into the
     * control's own fields. */
    if (pText != NULL) {
        size_t cb = strlen((const char *)pText) + 1u;
        if (cb > (size_t)BR_TEXTBOX_MAX) {
            cb = (size_t)BR_TEXTBOX_MAX;
            memcpy(pBox->sz, pText, cb - 1u);
            pBox->sz[cb - 1u] = '\0';
        } else {
            memcpy(pBox->sz, pText, cb);
        }
    }

    pBox->f04 |= (uint32_t)a2;
    pBox->f08  = bKind;

    /* Zeroed BEFORE the dispatch; width and height are what the dispatch
     * fills in. f41C is a width limit and is simply cleared. */
    pBox->f41C   = 0;
    pBox->height = 0;
    pBox->width  = 0;

    pBox->left  = pRc[0];
    pBox->right = pRc[2];

    /* Raw dword copies of the control's +0x3C / +0x40, which 0x10047FB0 put
     * there. Same type on both sides, so this is the same bit pattern. */
    pBox->x = pThis->x;
    pBox->y = pThis->y;

    pBox->f418 = 0;
    pBox->f420 = 0;

    /* DEVIATION (memory safety): the original always has a vtable here,
     * because the element constructor plants 0x1008F728. br_uictl.c leaves it
     * NULL until a host installs slice3_39.h's g_pBrTextBoxVtbl, so the two
     * dispatches are guarded. Every existing caller in the tree guards the
     * same pointer the same way. When it is NULL, width and height stay 0 and
     * the read-back below sees zeroes -- which is exactly what an unmeasured
     * box means. */
    if (pV != NULL) {
        if (bKind == 3u) {
            pV->pfn08(pBox);        /* measure with font B */
        } else {
            pV->pfn04(pBox);        /* measure with font A */
        }

        /* Called for its side effect; the float it returns is dropped. Note
         * the test is on a2, not on pBox->f04. */
        if ((a2 & 1) != 0) {
            (void)pV->pfn28(pBox);
        }
    }

    /* Everything below RE-READS state the dispatch may have changed. */
    nY = BrFtolTrunc(pThis->y);
    pThis->rcTop = nY;
    pThis->rcLeft = pRc[0];
    pThis->rcRight = pRc[2];
    /* `movsx edx,cx` -- height is SIGN-extended before the add. */
    pThis->rcBottom = nY + (int32_t)pBox->height;
    pThis->w48 = (int16_t)pBox->width;
    pThis->w4A = (int16_t)pBox->height;
}

/* ==========================================================================
 * 0x1008F6B8 -- the control vtable
 *
 * Sixteen slots in the original (0x1008F6B8 .. 0x1008F6F7, bounded above by
 * the page vtable at 0x1008F6F8). br_ui.h's BrUiCtlVtbl_ models all sixteen,
 * so +0x3C (0x10048060) has a field now; it used to be missing, which was
 * safe only because nothing indexed past +0x38 through this type.
 * ========================================================================== */

const BrUiCtlVtbl_ g_brUiCtlVtbl_1008F6B8 = {
    NULL,                                       /* +0x00  0x100478A0 */
    NULL, NULL, NULL, NULL,                     /* +0x04 .. +0x10    */
    NULL, NULL, NULL, NULL,                     /* +0x14 .. +0x20    */
    NULL, NULL, NULL, NULL,                     /* +0x24 .. +0x30    */
    BrUiCtlSetText_10047EB0,                    /* +0x34  0x10047EB0 */
    BrUiCtlPlace_10047FB0,                      /* +0x38  0x10047FB0 */
    NULL                                        /* +0x3C  0x10048060 */
};
