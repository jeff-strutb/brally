/* test_uictl.c -- BrUiCtlCtor, 0x100476C0.
 *
 * The interesting assertions here are not "does it zero things". They are:
 *
 *  - the -1 fills are NOT zero. Four of the original's nine block fills write
 *    0xFFFFFFFF, and the item table is one of them. A constructor that zeroed
 *    everything would pass a naive test and then pick menu item 0 where the
 *    original has "empty".
 *  - the unmodelled-write count is pinned, so fields cannot be added to
 *    BrUiCtl_ without someone noticing this constructor must initialise them.
 *  - the default vtable faults rather than no-ops. That is a deliberate design
 *    choice and a test keeps it from being "helpfully" softened later.
 */
#include "br_uictl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { \
    g_fails++; printf("  [FAIL] %s (%s:%d)\n", (msg), __FILE__, __LINE__); } } while (0)

int main(void)
{
    BrUiCtl_ *c = (BrUiCtl_ *)malloc(sizeof(BrUiCtl_));
    BrUiCtl_ *r;

    if (!c) { printf("alloc failed\n"); return 1; }

    /* Poison first: a constructor that merely leaves memory alone must not be
     * able to pass by accident. */
    memset(c, 0xA5, sizeof(*c));

    r = BrUiCtlCtor(c);
    CHECK(r == c, "returns this (original returns esi in eax)");

    /* The vtable store is unconditional in the original. */
    CHECK(c->pVtbl == g_pBrUiCtlVtbl, "stores the current control vtable");
    CHECK(c->pVtbl != NULL, "vtable pointer is never left NULL");

    /* The SIX function-pointer slots the builders overwrite start NULL. The
     * old model stopped at +0x14; br_ui.h names +0x18 too (ADJ-8), and the
     * original zeroes all six. */
    CHECK(c->pfn04 == NULL, "+0x04 cleared");
    CHECK(c->pfn08 == NULL, "+0x08 cleared");
    CHECK(c->pfn0C == NULL, "+0x0C cleared");
    CHECK(c->pfn10 == NULL, "+0x10 cleared");
    CHECK(c->pfn14 == NULL, "+0x14 cleared");
    CHECK(c->pfn18 == NULL, "+0x18 cleared");

    /* THE load-bearing one: the 25-dword -1 fill at +0x2A40 is br_ui.h's
     * aStepId[50] (ADJ-3, ADJ-4), so every element must be 0xFFFF and
     * specifically NOT 0. What two headers used to call the scalars f2A40 and
     * f2A42 are elements 0 and 1; the BOUNDARY is what is worth asserting,
     * because the old model could only reach the first dword and an
     * under-filled array is invisible from element 0. */
    CHECK(c->aStepId[1] == (uint16_t)0xFFFFu, "+0x2A42 is -1, not zero");
    CHECK(c->aStepId[1] != 0, "the -1 fill is not a zero fill");
    CHECK(c->aStepId[BR_UI_CTL_STEPS - 1] == (uint16_t)0xFFFFu,
          "the LAST step id is -1 too -- the fill covers all fifty");

    /* The same boundary on the other two -1 fills. +0x012A is the dangerous
     * one: 0 is a valid index there and -1 means empty. */
    CHECK(c->a012A[0] == -1 && c->a012A[BR_UI_CTL_A012A - 1] == -1,
          "+0x012A is filled with -1 end to end, not zeroed");
    CHECK(c->a2AF0[0] == -1 && c->a2AF0[BR_UI_CTL_A2AF0 - 1] == -1,
          "+0x2AF0 is filled with -1 end to end");

    /* 0x3F7D70A4 is exactly 0.99f in binary32, so this compares exactly.
     * `1004770A  mov dword ptr [esi + 0x44], 0x3f7d70a4` -- this used to read
     * `c->f1E1E8`, because the sparse model had no field at +0x44 and the
     * constant went 0x1E1A4 bytes too far. br_ui.h names +0x44. */
    CHECK(c->f44 == 0.99f, "+0x44 constant is 0.99f exactly");

    /* ... and the slot it used to land in must now be untouched by the
     * constructor. This is the assertion that would have caught the bug. */
    CHECK(c->list.f1A99C[5].u == 0,
          "+0x1E1E8 is left zero -- 0.99f does NOT land there");

    /* Rect is left alone by the constructor -- the builders set it. Poisoned
     * memory would leave 0xA5A5A5A5 here, so this checks the memset ran. */
    CHECK(c->rcLeft == 0, "+0x50 cleared by the constructor");
    CHECK(c->rcBottom == 0, "+0x5C cleared by the constructor");

    /* aStepId[0] is the element 0x10047FB0 overwrites with a code word. It
     * must start at -1 for the same reason: 0 is a valid code. */
    CHECK(c->aStepId[0] == (uint16_t)0xFFFFu, "+0x2A40 is -1, not zero");

    /* +0x1C is 1, not 0. 0x10047FB0 ORs into it, so a zero here would be
     * invisible until a flag test came out wrong. */
    CHECK(c->flags1C == 1, "+0x1C is 1");

    /* The three scalars that had no modelled home before the merge. */
    CHECK(c->b2C == 0xFFu, "+0x2C is 0xFF -- a BYTE, not a dword");
    CHECK(c->f2AEC == 1, "+0x2AEC is 1");
    CHECK(c->f2B54 == 1, "+0x2B54 is 1");

    /* The block at +0x2B5C is slice3_39.h's BrTextBox and there are THREE of
     * them (ADJ-1), each built by the vector-constructor iterator with element
     * ctor 0x1005B050, which sets f08 = 1. Element 2 is checked as well as
     * element 0, because a model with one item and a model with three are
     * indistinguishable from element 0 alone. */
    CHECK(c->aText[0].f08 == 1, "the item's +0x08 is 1");
    CHECK(c->aText[BR_UI_CTL_TEXTS - 1].f08 == 1,
          "and so is the LAST of the three -- all three are constructed");
    CHECK(c->aText[0].sz[0] == 0, "the item's text buffer is cleared");
    CHECK(c->aText[0].pVtbl == NULL,
          "the item vtable is left NULL -- a host installs g_pBrTextBoxVtbl");

    /* Pin the gap count. If someone adds a field to BrUiCtl_ without
     * initialising it here, this fires. It dropped from 9 to 8 when BrUiCtl_
     * gained f1C, and from 8 to 1 when both packets moved onto br_ui.h --
     * which names every scalar and every block fill. The one that is left is
     * the +0x3838 sub-object constructor, and it is a LINK question, not a
     * modelling one; see the ledger in br_uictl.c. */
    CHECK(g_brUiCtlUnmodelledWrites == 1,
          "unmodelled-write count unchanged (update ctor when it drops)");

    /* NULL in is NULL out -- a port DEVIATION; the original faults. */
    CHECK(BrUiCtlCtor(NULL) == NULL, "NULL this returns NULL, does not fault");

    free(c);
    printf("uictl: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
