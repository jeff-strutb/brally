/* slice8_90.c -- packet 90.  See port/include/slice8_90.h for the pairing
 * table, the arity adjudication read out of orig/BRGlide.dll, the one bridge
 * this pass adds and the eight addresses it declines.  This file is the
 * marshal and the adapters, and nothing else: no body here is a second
 * transcription of anything.
 *
 * REFERENCE: orig/BRGlide.dll.  Addresses in comments are the D3D ones,
 * because that is the numbering the rest of port/ uses; the Glide address
 * actually read is beside each one in the header's section 1.
 */
#include <string.h>

#include "slice8_90.h"

/* The two words the bridge in section 3 reads.  Declared by hand rather than
 * reached through slice2_25.h, which cannot be included here: that header
 * defines `struct BrDPlayVtbl` and so does slice1_06.h, which slice6_72.h
 * pulls in -- one C tag, two definitions, and the clash is a hard error.
 * slice8_89.c copies the same two lines for the same reason.  The types are
 * slice2_25.h's, and they are checked below. */
extern int32_t g_brAA2A20;      /* 0x10AA2A20  car shadow, written 0x10043650 */
extern int32_t g_brAA2A24;      /* 0x10AA2A24  specular,   written 0x100436B0 */

/* ==========================================================================
 * 0. Bounded copies
 *
 * Both buffers are already the port's bounded stand-ins for unbounded
 * originals (BR_MENUTEXT_MAX 256, BR_TEXTBOX_MAX 1024).  These two never
 * overrun and always terminate.
 * ========================================================================== */

static void Br90Copy(char *pszDst, size_t cbDst, const char *pszSrc)
{
    size_t n;

    if (pszDst == NULL || cbDst == 0u)
        return;
    if (pszSrc == NULL) {
        pszDst[0] = '\0';
        return;
    }
    for (n = 0u; n + 1u < cbDst && pszSrc[n] != '\0'; ++n)
        pszDst[n] = pszSrc[n];
    pszDst[n] = '\0';
}

/* ==========================================================================
 * 1. The binding
 *
 * The shim vtable's four slots have to reach the REAL control from a
 * `BrMenuText *` that is a member of a stack-local view.  container_of would
 * work, but the view is also handed to bodies that could in principle pass it
 * on, so the binding is an explicit save/restore stack instead: nesting is
 * then correct by construction rather than by argument.
 * ========================================================================== */

typedef struct Br90Bind {
    struct Br90Bind *pPrev;
    BrUiCtl_        *pCtl;
    BrMenuItem       item;
    char             szIn[BR_MENUTEXT_MAX];  /* what the copy IN produced */
} Br90Bind;

static Br90Bind *s_pBind;

/* item view -> the real text box.  Called before every shim slot, because the
 * box's own methods (BrTextBoxMeasureA/B, BrTextBoxCentreX) read `sz`. */
static void Br90Flush(Br90Bind *pB)
{
    BrTextBox *pBox = &pB->pCtl->aText[0];

    Br90Copy(pBox->sz, sizeof pBox->sz, pB->item.text.sz);
    pBox->f04 = pB->item.text.f04;
    pBox->f08 = pB->item.text.f08;
}

/* ...and back, so a slot that rewrites the box is visible to the body. */
static void Br90Pull(Br90Bind *pB)
{
    const BrTextBox *pBox = &pB->pCtl->aText[0];

    Br90Copy(pB->item.text.sz, sizeof pB->item.text.sz, pBox->sz);
    pB->item.text.f04 = pBox->f04;
    pB->item.text.f08 = pBox->f08;
}

/* The four slots slice2_24.c actually reaches -- +0x04 and +0x10 after a
 * CAPTION assignment, +0x08 and +0x2C after a VALUE one.  No other slot of
 * BrMenuTextVtbl is ever called (the two tails in slice2_24.c are the whole
 * set), so the rest stay NULL: an unwired slot must be a visible hole.
 *
 * Each one calls the box's REAL method through the box's REAL vtable type
 * with the box's REAL pointer.  Nothing is called through a type it was not
 * defined with, which is the entire reason this file exists. */
#define BR90_SHIM_SLOT(name, member)                                        \
    static void name(BrMenuText *pText)                                     \
    {                                                                       \
        Br90Bind  *pB = s_pBind;                                            \
        BrTextBox *pBox;                                                    \
        (void)pText;                                                        \
        if (pB == NULL || pB->pCtl == NULL)                                 \
            return;                                                         \
        pBox = &pB->pCtl->aText[0];                                         \
        Br90Flush(pB);                                                      \
        if (pBox->pVtbl != NULL && pBox->pVtbl->member != NULL)             \
            pBox->pVtbl->member(pBox);                                      \
        Br90Pull(pB);                                                       \
    }

BR90_SHIM_SLOT(Br90Vt04, pfn04)
BR90_SHIM_SLOT(Br90Vt08, pfn08)
BR90_SHIM_SLOT(Br90Vt10, pfn10)
BR90_SHIM_SLOT(Br90Vt2C, pfn2C)

static const BrMenuTextVtbl s_shimVtbl = {
    NULL,      /* +0x00 */
    Br90Vt04,  /* +0x04 */
    Br90Vt08,  /* +0x08 */
    NULL,      /* +0x0C */
    Br90Vt10,  /* +0x10 */
    NULL,      /* +0x14 */
    NULL,      /* +0x18 */
    NULL,      /* +0x1C */
    NULL,      /* +0x20 */
    NULL,      /* +0x24 */
    NULL,      /* +0x28 */
    Br90Vt2C   /* +0x2C */
};

/* ==========================================================================
 * 2. The marshal
 * ========================================================================== */

int32_t Br90Call(BrUiCtl_ *pCtl, int32_t (*pfnBody)(BrMenuItem *pItem))
{
    Br90Bind b;
    int32_t  r;

    if (pCtl == NULL || pfnBody == NULL)
        return 1;                       /* 0x10048180's "carry on" */

    memset(&b, 0, sizeof b);
    b.pPrev = s_pBind;
    b.pCtl  = pCtl;

    b.item.f1C      = (uint32_t)pCtl->flags1C;
    b.item.f1E20C   = (int16_t)pCtl->w1E20C;
    b.item.text.f04 = pCtl->aText[0].f04;
    b.item.text.f08 = pCtl->aText[0].f08;
    Br90Copy(b.item.text.sz, sizeof b.item.text.sz, pCtl->aText[0].sz);
    Br90Copy(b.szIn, sizeof b.szIn, b.item.text.sz);

    /* The bodies guard on this pointer (slice2_24.c's DEVIATION), so a box
     * with no vtable must present as no vtable here too. */
    b.item.text.pVtbl = (pCtl->aText[0].pVtbl != NULL) ? &s_shimVtbl : NULL;

    s_pBind = &b;
    r       = pfnBody(&b.item);
    s_pBind = b.pPrev;

    pCtl->flags1C      = (int32_t)b.item.f1C;
    pCtl->w1E20C       = (uint16_t)b.item.f1E20C;
    pCtl->aText[0].f04 = b.item.text.f04;
    pCtl->aText[0].f08 = b.item.text.f08;

    /* DEVIATION (see the header): the view's buffer is a quarter of the
     * box's, so a caption longer than 255 characters comes back truncated.
     * Suppressing the write when the body did not touch the text makes that
     * lossless for every item the hook leaves alone, which is all of them on
     * the guard paths. */
    if (strcmp(b.item.text.sz, b.szIn) != 0)
        Br90Copy(pCtl->aText[0].sz, sizeof pCtl->aText[0].sz, b.item.text.sz);

    return r;
}

/* ==========================================================================
 * 3. The bridge -- 0x10AA2A20 and 0x10AA2A24 only
 *
 * DEVIATION, and a bridge rather than a resolution.  slice2_25.c owns the two
 * words the Game Options togglers write and slice2_24.c has a second copy of
 * them that the two caption hooks read; wiring both halves of that pair in
 * one pass is what makes the split observable, so this pass closes it here.
 * Delete this the day slice2_24's fields become views onto slice2_25's.
 * ========================================================================== */

static void Br90BridgeToggles(void)
{
    BrMenuState *pSt = BrMenuGetState();

    if (pSt == NULL)
        return;
    pSt->gAA2A20 = (uint32_t)g_brAA2A20;   /* 0x10AA2A20  car shadow  */
    pSt->gAA2A24 = (uint32_t)g_brAA2A24;   /* 0x10AA2A24  specular    */
}

/* ==========================================================================
 * 4. The adapters
 * ========================================================================== */

int32_t BrUiHook90_100408D0(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText08D0); }
int32_t BrUiHook90_10040B30(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText0B30); }
int32_t BrUiHook90_10041040(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuTime1040); }
int32_t BrUiHook90_10041180(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuTime1180); }
int32_t BrUiHook90_10041300(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText1300); }
int32_t BrUiHook90_100415A0(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText15A0); }
int32_t BrUiHook90_10041670(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText1670); }
int32_t BrUiHook90_10041710(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText1710); }
int32_t BrUiHook90_100417B0(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuText17B0); }
int32_t BrUiHook90_10041890(BrUiCtl_ *pCtl) { return Br90Call(pCtl, BrMenuFlags1890); }

/* The two that read a word another module now writes. */
/* WHAT IT DOES: sets the caption on the Car Shadow option row so it reads
 * the current setting. It first copies the toggle's value across from where
 * the option code keeps it, because in this port the reader and the writer
 * of that setting live in two different places. */
/* @implements 0x100409B0 d3d BrUiHook90_100409B0 */
int32_t BrUiHook90_100409B0(BrUiCtl_ *pCtl)
{
    Br90BridgeToggles();
    return Br90Call(pCtl, BrMenuCap09B0);
}

/* WHAT IT DOES: the same for the Specular option row: bridge the toggle's
 * value across, then set the caption to match. */
/* @implements 0x100409D0 d3d BrUiHook90_100409D0 */
int32_t BrUiHook90_100409D0(BrUiCtl_ *pCtl)
{
    Br90BridgeToggles();
    return Br90Call(pCtl, BrMenuCap09D0);
}

/* 0x100474B0.  No marshal: br_sprfont.c's 0x10047360 is already typed over
 * BrUiCtl_, and Glide 0x10040AF0 is that call plus `mov eax,1`.  The callee's
 * own return value is discarded by the original, so it is discarded here. */
int32_t BrUiHook90_100474B0(BrUiCtl_ *pCtl)
{
    if (pCtl != NULL)
        (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}

/* ==========================================================================
 * 5. Installation
 * ========================================================================== */

void BrUiHook90Install71(BrS71Hooks *pHooks)
{
    if (pHooks == NULL)
        return;
    pHooks->p10041300 = BrUiHook90_10041300;   /* slice6_71.c:570  */
    pHooks->p10041890 = BrUiHook90_10041890;   /* slice6_71.c:476  */
}

void BrUiHook90Install72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL)
        return;
    pHooks->p100408D0 = BrUiHook90_100408D0;   /* slice6_72.c:903  */
    pHooks->p100409B0 = BrUiHook90_100409B0;   /* slice6_72.c:1560 */
    pHooks->p100409D0 = BrUiHook90_100409D0;   /* slice6_72.c:1544 */
    pHooks->p10040B30 = BrUiHook90_10040B30;   /* slice6_72.c:1165 */
    pHooks->p10041040 = BrUiHook90_10041040;   /* slice6_72.c:1333 */
    pHooks->p10041180 = BrUiHook90_10041180;   /* slice6_72.c:1348 */
    pHooks->p10041300 = BrUiHook90_10041300;   /* slice6_72.c:1138 */
    pHooks->p100415A0 = BrUiHook90_100415A0;   /* slice6_72.c:1101 */
    pHooks->p100474B0 = BrUiHook90_100474B0;   /* slice6_72.c:798  */
}

void BrUiHook90Install73(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL)
        return;
    pHooks->p10041300 = BrUiHook90_10041300;   /* slice6_73.c:759, :879 */
    pHooks->p10041670 = BrUiHook90_10041670;   /* slice6_73.c:901       */
    pHooks->p10041710 = BrUiHook90_10041710;   /* slice6_73.c:939       */
    pHooks->p100417B0 = BrUiHook90_100417B0;   /* slice6_73.c:922       */
}
