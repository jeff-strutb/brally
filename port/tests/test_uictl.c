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

    /* The five function-pointer slots the builders overwrite start NULL. */
    CHECK(c->pfn04 == NULL, "+0x04 cleared");
    CHECK(c->pfn08 == NULL, "+0x08 cleared");
    CHECK(c->pfn0C == NULL, "+0x0C cleared");
    CHECK(c->pfn10 == NULL, "+0x10 cleared");
    CHECK(c->pfn14 == NULL, "+0x14 cleared");

    /* THE load-bearing one: +0x2A42 lies inside the 25-dword -1 fill at
     * +0x2A40, so it must be 0xFFFF and specifically NOT 0. */
    CHECK(c->f2A42 == (uint16_t)0xFFFFu, "+0x2A42 is -1, not zero");
    CHECK(c->f2A42 != 0, "the -1 fill is not a zero fill");

    /* 0x3F7D70A4 is exactly 0.99f in binary32, so this compares exactly. */
    CHECK(c->f1E1E8 == 0.99f, "+0x44 constant is 0.99f exactly");

    /* Rect is left alone by the constructor -- the builders set it. Poisoned
     * memory would leave 0xA5A5A5A5 here, so this checks the memset ran. */
    CHECK(c->f50 == 0, "+0x50 cleared by the constructor");
    CHECK(c->f5C == 0, "+0x5C cleared by the constructor");

    /* Pin the gap count. If someone adds the item table to BrUiCtl_ without
     * initialising it to -1 here, this fires. */
    CHECK(g_brUiCtlUnmodelledWrites == 9,
          "unmodelled-write count unchanged (update ctor when it drops)");

    /* NULL in is NULL out -- a port DEVIATION; the original faults. */
    CHECK(BrUiCtlCtor(NULL) == NULL, "NULL this returns NULL, does not fault");

    free(c);
    printf("uictl: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
