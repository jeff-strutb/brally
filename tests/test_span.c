#include "br_span.h"
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrSpanGrid g;
    BrSpanReset(&g);

    check(!BrSpanTest(&g, 10, 10), "empty grid contains nothing");

    BrSpanAdd(&g, 10, 5);
    check(BrSpanTest(&g, 10, 5), "point inside after insert");
    check(!BrSpanTest(&g, 11, 5), "point outside the span");

    BrSpanAdd(&g, 20, 5);
    check(BrSpanTest(&g, 15, 5), "span widened to cover between endpoints");
    check(BrSpanTest(&g, 10, 5) && BrSpanTest(&g, 20, 5), "endpoints inclusive");
    check(!BrSpanTest(&g, 21, 5), "just past the span excluded");

    /* clamping, not rejection: the original clamps both coordinates */
    BrSpanAdd(&g, -5, 200);
    check(BrSpanTest(&g, 0, 63), "out-of-range insert clamps to (0,63)");

    BrSpanAdd(&g, 999, 63);
    check(BrSpanTest(&g, 63, 63), "large column clamps to 63");

    /* row bounds are inclusive per the original's jl/jg */
    BrSpanAdd(&g, 1, 0);
    check(BrSpanTest(&g, 1, 0), "row 0 inclusive");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
