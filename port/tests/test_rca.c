/* test_rca.c -- verify the .rca loader against retail car files. */
#include "br_rca.h"
#include "br_testdata.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

static int load_show(const char *pszPath, BrRca *pRca)
{
    int i;
    if (BrRcaLoad(pRca, pszPath) != 0) { printf("  [FAIL] load %s\n", pszPath); g_fail = 1; return 1; }
    printf("  %-22s name=\"%s\" (%zu bytes)\n", pszPath, pRca->szName, pRca->cbFile);
    printf("      gears:");
    for (i = 0; i < BR_RCA_GEAR_COUNT; i++) printf(" %.2f", pRca->gears[i]);
    printf("\n");
    return 0;
}

int main(void)
{
    BR_REQUIRE_TESTDATA("testdata/bb.rca", "rca");
    BrRca ce, bb;
    int i, descending = 1, differs = 0;

    if (load_show("testdata/ce.rca", &ce)) return 1;
    if (load_show("testdata/bb.rca", &bb)) return 1;

    check(strcmp(ce.szName, "TYPE-CE") == 0, "ce.rca name parsed");
    check(strcmp(bb.szName, "Beach Ball") == 0, "bb.rca name parsed");

    for (i = 1; i < BR_RCA_GEAR_COUNT; i++)
        if (ce.gears[i] >= ce.gears[i - 1]) descending = 0;
    check(descending, "gear ratios strictly descending");
    check(ce.gears[0] > 2.0f && ce.gears[0] < 6.0f, "1st gear in a sane range");
    check(bb.gears[BR_RCA_GEAR_COUNT - 1] > 0.1f, "top gear positive");

    /* the two cars must share a gearbox prefix but diverge -- if our offset
     * were wrong we would see either total equality or total noise */
    for (i = 0; i < BR_RCA_GEAR_COUNT; i++)
        if (ce.gears[i] != bb.gears[i]) differs++;
    check(differs > 0 && differs < BR_RCA_GEAR_COUNT,
          "cars share some gears and differ in others");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
