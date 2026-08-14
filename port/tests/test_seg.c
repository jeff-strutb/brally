#include "br_seg.h"
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrSegMap m = { 0x80300000u, 0x40000000u };
    uint32_t p;

    p = 0; BrSegFixup(&m, &p);
    check(p == 0, "null stays null");

    p = 0x80300000u; BrSegFixup(&m, &p);
    check(p == 0x40000000u, "base maps to host base");

    p = 0x80300100u; BrSegFixup(&m, &p);
    check(p == 0x40000100u, "offset preserved across the rebase");

    /* below-base must ZERO, not pass through or clamp -- that is what makes
     * unresolvable references crash-safe in the original */
    p = 0x80200000u; BrSegFixup(&m, &p);
    check(p == 0, "below base is zeroed, not passed through");

    check(BrSegResolve(&m, 0) == 0, "resolve(null) == 0");
    check(BrSegResolve(&m, 0x80300100u) == 0x40000100u, "resolve matches fixup");
    check(BrSegResolve(&m, 0x80200000u) == 0, "resolve rejects below-base");

    { BrSegMap s; uint32_t q;
      BrSegSetBases(&s, 0x80300000u, 0x40000000u);
      check(s.n64Base == 0x80300000u && s.hostBase == 0x40000000u,
            "SetBases stores (n64Base, hostBase) in that order");
      /* a swapped order would make this resolve to the wrong direction */
      q = BrSegResolve(&s, 0x80300040u);
      check(q == 0x40000040u, "bases installed the right way round"); }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
