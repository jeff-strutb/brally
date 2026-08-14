/* test_pagemodel.c -- prove slice6_72.h and slice6_73.h still agree about the
 * page object.
 *
 * WHY THIS EXISTS
 *
 * Both headers define `struct BrUiPage_`. One C tag, two definitions. They
 * compile cleanly because they never meet in a translation unit -- and that is
 * exactly the problem: a pointer built through one and read through the other
 * is silently wrong, and no compiler will ever say so.
 *
 * This has already caused a live memory bug. The host link bound slice6_73.c's
 * call to slice3_32.c's page constructor, which wrote a DIFFERENT layout into
 * the allocation: every field from +0x00C on landed at the wrong offset and one
 * store ran past the end of the allocation.
 *
 * The two were then made field-for-field identical by hand. Nothing enforced
 * it. A hand-maintained invariant with no check is a bug with a delay timer, so
 * this is the check: each header is compiled in ITS OWN translation unit, each
 * exports what it believes the layout is, and this file compares them.
 *
 * This is a stopgap. The real fix is migrating both onto br_ui.h, at which
 * point there is one definition and this test becomes unnecessary -- delete it
 * then, and not before.
 */
#include <stdio.h>
#include <stddef.h>

#define P(n) extern const size_t g_p72_##n, g_p73_##n;
P(size) P(cCtl) P(apCtl) P(fX) P(fY) P(pOwner) P(cSel)
#undef P

static int g_checks, g_fails;

static void Cmp(const char *pszField, size_t a, size_t b)
{
    g_checks++;
    if (a != b) {
        g_fails++;
        printf("  [FAIL] %s: slice6_72 says %zu, slice6_73 says %zu\n",
               pszField, a, b);
    }
}

int main(void)
{
    Cmp("sizeof",  g_p72_size,   g_p73_size);
    Cmp("cCtl",    g_p72_cCtl,   g_p73_cCtl);
    Cmp("apCtl",   g_p72_apCtl,  g_p73_apCtl);
    Cmp("fX",      g_p72_fX,     g_p73_fX);
    Cmp("fY",      g_p72_fY,     g_p73_fY);
    Cmp("pOwner",  g_p72_pOwner, g_p73_pOwner);
    Cmp("cSel",    g_p72_cSel,   g_p73_cSel);

    if (g_fails)
        printf("  the two BrUiPage_ definitions have DRIFTED -- a pointer built\n"
               "  through one and read through the other is now silently wrong\n");
    printf("pagemodel: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
