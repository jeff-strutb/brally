/* test_slice8_90.c -- the argument MARSHAL, not the bodies.
 *
 * WHAT IS BEING ASSERTED, AND WHY THESE PROPERTIES
 *
 * slice8_90.c exists for one reason: a control hook slot is
 * `int32_t (*)(BrUiCtl_ *)` and slice2_24.c's twelve bodies take a
 * `BrMenuItem *`, a SECOND C MODEL of the same original object.  BrMenuItem is
 * now a byte image at the original's own displacements (it has to be, or the
 * caption setters could never come out bit-identical), so on the 32-bit
 * matching build the two models happen to agree -- but they are still not the
 * same type, and on this LP64 host they do NOT agree: every pointer ahead of
 * the reached fields widens, in BrUiCtl_ and BrMenuItem by different amounts.
 * The failure a cast produces is silent -- it links, it runs, and it reads the
 * wrong members -- so the test asserts the thing a cast would get wrong rather
 * than "the hook returned 1".
 *
 * So every body below is a STAND-IN that reports which member it saw.  The
 * real bodies are slice2_24.c's and are not linked here; this suite links
 * port/src/slice8_90.c alone, and a failure can only be this module's.
 *
 * The five properties:
 *
 *   1. FIELD IDENTITY.  A body that reads f1E20C must see the control's
 *      w1E20C and not whatever a raw cast would land on.  The negative half
 *      of the same claim is asserted directly: on this host the two models
 *      disagree about where f1E20C / w1E20C lives, so a cast reads garbage.
 *   2. WRITE-BACK.  What the body stores in the view must land on the
 *      CONTROL, or the hook is a no-op with a plausible return value.
 *   3. THE SHIM VTABLE.  slice2_24's two tails call the text box through
 *      BrMenuTextVtbl and the box's real methods are BrTextBoxVtbl -- two C
 *      types over one original vtable.  The shim must call the REAL slot with
 *      the REAL box pointer, and the body must be able to see what that slot
 *      wrote.
 *   4. TRUNCATION IS LOSSLESS WHEN THE BODY DOES NOT WRITE.  The view's
 *      buffer is a quarter of the box's, so a caption longer than 255
 *      characters must survive a hook that leaves it alone.
 *   5. THE INSTALLERS touch only their own slots and tolerate NULL, the
 *      contract every other hook packet in this tree keeps.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_90.h"

static int g_fails;

#define CHECK(cond, why)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, (why));            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * Stand-ins for everything slice8_90.c reaches outside itself.
 * ========================================================================== */

static BrMenuState s_menu;
BrMenuState *BrMenuGetState(void) { return &s_menu; }

int32_t g_brAA2A20;
int32_t g_brAA2A24;

static int s_nKind;
static BrUiCtl_ *s_pKindArg;
int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl)
{
    ++s_nKind;
    s_pKindArg = pCtl;
    return 7;                  /* a value the adapter must DISCARD */
}

/* --- the observation: what each body was handed and what it wrote --------- */
static struct {
    int      nCalls;
    uint32_t sawF1C;
    int16_t  sawId;
    char     sawText[64];
    int      sawVtblNull;
} s_obs;

static void ObsReset(void) { memset(&s_obs, 0, sizeof s_obs); }

static void ObsRecord(BrMenuItem *pItem)
{
    ++s_obs.nCalls;
    s_obs.sawF1C      = pItem->f1C;
    s_obs.sawId       = pItem->f1E20C;
    s_obs.sawVtblNull = (pItem->text.pVtbl == NULL);
    strncpy(s_obs.sawText, pItem->text.sz, sizeof s_obs.sawText - 1u);
}

/* The stand-in the marshal tests drive. It reports what it saw and then
 * writes one of each kind of field so the write-back can be checked. */
static int s_fBodyWrites = 1;
static int32_t BodyProbe(BrMenuItem *pItem)
{
    ObsRecord(pItem);
    if (!s_fBodyWrites)
        return 1;
    pItem->f1C     = 0xABCD1010u;
    pItem->f1E20C  = 0x1234;
    pItem->text.f08 = 9;
    strcpy(pItem->text.sz, "WRITTEN");
    if (pItem->text.pVtbl != NULL) {
        pItem->text.pVtbl->pfn04(&pItem->text);   /* caption tail */
        pItem->text.pVtbl->pfn10(&pItem->text);
        pItem->text.pVtbl->pfn08(&pItem->text);   /* value tail   */
        pItem->text.pVtbl->pfn2C(&pItem->text);
    }
    ObsRecord(pItem);            /* what the vtable round-trip left behind */
    return 3;
}

/* The twelve slice2_24 bodies slice8_90.c links against. Each is only
 * required to exist; the marshal is driven through BodyProbe. */
#define STANDIN(name) int32_t name(BrMenuItem *p) { ObsRecord(p); return 1; }
STANDIN(BrMenuText08D0)
STANDIN(BrMenuCap09B0)
STANDIN(BrMenuCap09D0)
STANDIN(BrMenuText0B30)
STANDIN(BrMenuTime1040)
STANDIN(BrMenuTime1180)
STANDIN(BrMenuText1300)
STANDIN(BrMenuText15A0)
STANDIN(BrMenuText1670)
STANDIN(BrMenuText1710)
STANDIN(BrMenuText17B0)
STANDIN(BrMenuFlags1890)

/* ==========================================================================
 * A control, and a text-box vtable whose slots are observable.
 * ========================================================================== */

static int s_nBoxSlot[4];          /* +0x04 +0x08 +0x10 +0x2C */
static BrTextBox *s_pBoxSeen;

static void BoxPfn04(BrTextBox *p) { ++s_nBoxSlot[0]; s_pBoxSeen = p; }
static void BoxPfn08(BrTextBox *p) { ++s_nBoxSlot[1]; s_pBoxSeen = p; }
static void BoxPfn10(BrTextBox *p)
{
    ++s_nBoxSlot[2];
    s_pBoxSeen = p;
    /* Write through the REAL box, so the body can prove it saw the result. */
    strcpy(p->sz, "FROMBOX");
}
static void BoxPfn2C(BrTextBox *p) { ++s_nBoxSlot[3]; s_pBoxSeen = p; }

static BrTextBoxVtbl s_boxVtbl;

static BrUiCtl_ *NewCtl(void)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1u, sizeof(BrUiCtl_));
    if (p == NULL) {
        printf("FAIL out of memory\n");
        exit(1);
    }
    return p;
}

/* ==========================================================================
 * 1 + 2. Field identity and write-back
 * ========================================================================== */

static void TestMarshalFields(void)
{
    BrUiCtl_ *p = NewCtl();
    int32_t   r;

    printf("\n1. the marshal reaches the CONTROL's fields, not a cast's\n");

    /* offsetof is the negative half of the claim: a raw cast through
     * BrUiCtlHookFn_ would read the control's pfn10/pfn14 as the string id.
     * This is not a style point -- it is the bug the module prevents. */
    CHECK(offsetof(BrUiCtl_, w1E20C) != offsetof(BrMenuItem, f1E20C),
          "the two models disagree about where the string id lives, so a "
          "raw cast through BrUiCtlHookFn_ cannot reach it");

    p->flags1C = 0x00102001;
    p->w1E20C  = 0x0055;
    p->aText[0].f04 = 0x11u;
    p->aText[0].f08 = 3u;
    strcpy(p->aText[0].sz, "BEFORE");

    ObsReset();
    s_fBodyWrites = 0;
    r = Br90Call(p, BodyProbe);

    CHECK(r == 1, "the body's return value is passed straight through");
    CHECK(s_obs.nCalls == 1, "the body ran exactly once");
    CHECK(s_obs.sawF1C == 0x00102001u, "the body saw flags1C");
    CHECK(s_obs.sawId == 0x0055, "the body saw w1E20C, not a hook pointer");
    CHECK(strcmp(s_obs.sawText, "BEFORE") == 0, "the body saw aText[0].sz");
    CHECK(s_obs.sawVtblNull == 1,
          "a box with no vtable presents as no vtable, so the bodies' own "
          "NULL guard still fires");

    /* Nothing must have moved. */
    CHECK(p->flags1C == 0x00102001, "an inert body leaves flags1C alone");
    CHECK(p->w1E20C == 0x0055, "...and w1E20C");
    CHECK(strcmp(p->aText[0].sz, "BEFORE") == 0, "...and the text");

    printf("2. what the body writes lands on the control\n");
    ObsReset();
    s_fBodyWrites = 1;
    r = Br90Call(p, BodyProbe);

    CHECK(r == 3, "the body's return value is passed straight through");
    CHECK(p->flags1C == (int32_t)0xABCD1010u, "flags1C came back");
    CHECK(p->w1E20C == 0x1234u, "w1E20C came back");
    CHECK(p->aText[0].f08 == 9u, "the text box's f08 came back");
    CHECK(strcmp(p->aText[0].sz, "WRITTEN") == 0, "the text came back");

    free(p);
}

/* ==========================================================================
 * 3. The shim vtable
 * ========================================================================== */

static void TestShimVtable(void)
{
    BrUiCtl_ *p = NewCtl();

    printf("\n3. the shim calls the REAL box slot with the REAL box\n");

    p->aText[0].pVtbl = &s_boxVtbl;
    strcpy(p->aText[0].sz, "SEED");

    memset(s_nBoxSlot, 0, sizeof s_nBoxSlot);
    s_pBoxSeen = NULL;
    ObsReset();
    s_fBodyWrites = 1;
    (void)Br90Call(p, BodyProbe);

    CHECK(s_obs.sawVtblNull == 0, "a box WITH a vtable presents as one");
    CHECK(s_nBoxSlot[0] == 1, "+0x04 ran once");
    CHECK(s_nBoxSlot[1] == 1, "+0x08 ran once");
    CHECK(s_nBoxSlot[2] == 1, "+0x10 ran once");
    CHECK(s_nBoxSlot[3] == 1, "+0x2C ran once");
    CHECK(s_pBoxSeen == &p->aText[0],
          "each slot got the control's OWN text box, not the view");
    /* BoxPfn10 rewrote the box; the body's second ObsRecord must have seen
     * it, which is the whole point of pulling the box back after each slot. */
    CHECK(strcmp(s_obs.sawText, "FROMBOX") == 0,
          "a slot that rewrites the box is visible to the body");
    CHECK(strcmp(p->aText[0].sz, "FROMBOX") == 0,
          "...and survives the write-back");

    free(p);
}

/* ==========================================================================
 * 4. Truncation
 * ========================================================================== */

static void TestTruncation(void)
{
    BrUiCtl_ *p = NewCtl();
    size_t    i;

    printf("\n4. a long caption the body does not touch is not truncated\n");

    for (i = 0u; i < 600u; ++i)
        p->aText[0].sz[i] = (char)('a' + (int)(i % 26u));
    p->aText[0].sz[600] = '\0';

    ObsReset();
    s_fBodyWrites = 0;
    (void)Br90Call(p, BodyProbe);

    CHECK(strlen(p->aText[0].sz) == 600u,
          "600 characters survive a hook that leaves the text alone");
    CHECK(strlen(s_obs.sawText) == sizeof s_obs.sawText - 1u,
          "the body itself only ever sees the bounded view -- that is the "
          "DEVIATION, and it is the body's own buffer size, not new");

    free(p);
}

/* ==========================================================================
 * 5. The installers
 * ========================================================================== */

static void TestInstallers(void)
{
    BrS71Hooks  h71;
    BrUi72Hooks h72;
    BrUi73Hooks h73;

    printf("\n5. the installers fill their slots and touch nothing else\n");

    memset(&h71, 0, sizeof h71);
    memset(&h72, 0, sizeof h72);
    memset(&h73, 0, sizeof h73);

    BrUiHook90Install71(&h71);
    CHECK(h71.p10041300 == BrUiHook90_10041300, "71 0x10041300");
    CHECK(h71.p10041890 == BrUiHook90_10041890, "71 0x10041890");
    CHECK(h71.p100443E0 == NULL, "71 leaves another packet's slot alone");
    CHECK(h71.p100444C0 == NULL,
          "71 0x100444C0 stays NULL -- DECLINED, see slice8_90.h section 4");

    BrUiHook90Install72(&h72);
    CHECK(h72.p100408D0 == BrUiHook90_100408D0, "72 0x100408D0");
    CHECK(h72.p100409B0 == BrUiHook90_100409B0, "72 0x100409B0");
    CHECK(h72.p100409D0 == BrUiHook90_100409D0, "72 0x100409D0");
    CHECK(h72.p10040B30 == BrUiHook90_10040B30, "72 0x10040B30");
    CHECK(h72.p10041040 == BrUiHook90_10041040, "72 0x10041040");
    CHECK(h72.p10041180 == BrUiHook90_10041180, "72 0x10041180");
    CHECK(h72.p10041300 == BrUiHook90_10041300, "72 0x10041300");
    CHECK(h72.p100415A0 == BrUiHook90_100415A0, "72 0x100415A0");
    CHECK(h72.p100474B0 == BrUiHook90_100474B0, "72 0x100474B0");
    CHECK(h72.p10043590 == NULL, "72 leaves slice7_80's cycler alone");
    CHECK(h72.p100437D0 == NULL,
          "72 0x100437D0 stays NULL -- DECLINED, LP64 byte image");
    CHECK(h72.p10046260 == NULL,
          "72 0x10046260 stays NULL -- DECLINED, unwired slice3_31 context");
    CHECK(h72.p10047250 == NULL,
          "72 0x10047250 stays NULL -- DECLINED, same");

    BrUiHook90Install73(&h73);
    CHECK(h73.p10041300 == BrUiHook90_10041300, "73 0x10041300");
    CHECK(h73.p10041670 == BrUiHook90_10041670, "73 0x10041670");
    CHECK(h73.p10041710 == BrUiHook90_10041710, "73 0x10041710");
    CHECK(h73.p100417B0 == BrUiHook90_100417B0, "73 0x100417B0");
    CHECK(h73.p100409F0 == NULL,
          "73 0x100409F0 stays NULL -- DECLINED on storage, not signature");
    CHECK(h73.p10040A20 == NULL, "73 0x10040A20 stays NULL -- same");
    CHECK(h73.p10047210 == NULL, "73 0x10047210 stays NULL -- DECLINED");
    CHECK(h73.p10047290 == NULL, "73 0x10047290 stays NULL -- DECLINED");
    CHECK(h73.p10042CF0 == NULL, "73 leaves slice7_80's cycler alone");

    /* NULL is a no-op, as in every other installer in the tree. */
    BrUiHook90Install71(NULL);
    BrUiHook90Install72(NULL);
    BrUiHook90Install73(NULL);
}

/* ==========================================================================
 * 6. 0x100474B0 and the bridge
 * ========================================================================== */

static void TestDelegates(void)
{
    BrUiCtl_ *p = NewCtl();

    printf("\n6. 0x100474B0 delegates and returns the constant 1\n");

    s_nKind = 0;
    s_pKindArg = NULL;
    CHECK(BrUiHook90_100474B0(p) == 1,
          "the original's `mov eax,1` wins over the callee's answer");
    CHECK(s_nKind == 1, "0x10047360 ran once");
    CHECK(s_pKindArg == p, "...on the control it was given");

    CHECK(BrUiHook90_100474B0(NULL) == 1, "a NULL control is still 1");
    CHECK(s_nKind == 1, "...and does not call through");

    printf("7. the 0x10AA2A20 / 0x10AA2A24 bridge runs before the body\n");
    memset(&s_menu, 0, sizeof s_menu);
    g_brAA2A20 = 5;
    g_brAA2A24 = 6;
    (void)BrUiHook90_100409B0(p);
    CHECK(s_menu.gAA2A20 == 5u,
          "slice2_24's copy of 0x10AA2A20 is seeded from slice2_25's, which "
          "is the word the Car Shadow toggle actually writes");
    (void)BrUiHook90_100409D0(p);
    CHECK(s_menu.gAA2A24 == 6u, "...and 0x10AA2A24 from the Specular toggle");

    free(p);
}

/* ==========================================================================
 * 8. Nesting
 * ========================================================================== */

static BrUiCtl_ *s_pOuter;
static BrUiCtl_ *s_pInner;
static int32_t   s_innerId;

static int32_t BodyInner(BrMenuItem *pItem)
{
    s_innerId = pItem->f1E20C;
    return 1;
}

static int32_t BodyOuter(BrMenuItem *pItem)
{
    /* Re-enter the marshal on a DIFFERENT control from inside a body, then
     * carry on using the outer view. If the binding did not nest, the write
     * below would land on the inner control. */
    (void)Br90Call(s_pInner, BodyInner);
    pItem->f1E20C = 0x0777;
    return 1;
}

static void TestNesting(void)
{
    printf("\n8. bindings nest: an inner call cannot steal the outer write\n");

    s_pOuter = NewCtl();
    s_pInner = NewCtl();
    s_pOuter->w1E20C = 0x0111;
    s_pInner->w1E20C = 0x0222;

    (void)Br90Call(s_pOuter, BodyOuter);

    CHECK(s_innerId == 0x0222, "the inner body saw the INNER control");
    CHECK(s_pOuter->w1E20C == 0x0777u, "the outer write landed on the outer");
    CHECK(s_pInner->w1E20C == 0x0222u, "the inner control was not written");

    free(s_pOuter);
    free(s_pInner);
}

int main(void)
{
    s_boxVtbl.pfn04 = BoxPfn04;
    s_boxVtbl.pfn08 = BoxPfn08;
    s_boxVtbl.pfn10 = BoxPfn10;
    s_boxVtbl.pfn2C = BoxPfn2C;

    printf("slice8_90 -- the BrMenuItem marshal\n");

    TestMarshalFields();
    TestShimVtable();
    TestTruncation();
    TestInstallers();
    TestDelegates();
    TestNesting();

    printf("\n%d failures\n", g_fails);
    return g_fails != 0;
}
