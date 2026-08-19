#include "br_state.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrActiveFlags f;
    BrCounted o;

    memset(&f, 0, sizeof(f));
    check(BrIsAnyActive(&f) == 0, "all clear -> inactive");

    memset(&f, 0, sizeof(f)); f.a0 = 1;
    check(BrIsAnyActive(&f) == 1, "an early flag activates");
    memset(&f, 0, sizeof(f)); f.a8 = 1;
    check(BrIsAnyActive(&f) == 1, "a late flag activates");

    /* the override is the whole point: set alone it must read inactive */
    memset(&f, 0, sizeof(f)); f.override = 1;
    check(BrIsAnyActive(&f) == 0, "override alone -> inactive");

    /* and it must suppress the flags that come AFTER it, but not before */
    memset(&f, 0, sizeof(f)); f.override = 1; f.a8 = 1;
    check(BrIsAnyActive(&f) == 0, "override suppresses later flags");
    memset(&f, 0, sizeof(f)); f.override = 1; f.a0 = 1;
    check(BrIsAnyActive(&f) == 1, "override does NOT suppress earlier flags");

    memset(&o, 0, sizeof(o)); o.count = 5;
    check(BrCountedTotal(&o) == 5, "flag clear -> plain count");
    o.flag = 1;
    check(BrCountedTotal(&o) == 6, "flag set -> count + 1");
    o.flag = 99;
    check(BrCountedTotal(&o) == 6, "any non-zero flag adds exactly one");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
