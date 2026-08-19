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

    /* ---------------------------------------------------------------- *
     * LITERALS READ OUT OF THE FILES.
     *
     * Everything above this line is a range, an ordering or a difference
     * count, and all of them stay true when the decode is wrong: making
     * rd_u32le read p[1] where it should read p[0] corrupts the low
     * mantissa byte of every float and moves nothing far enough to break
     * a "descending" or a "sane range" test.  Nothing above ever reads
     * auParams at all, so `pRca->auParams[i] = v` could be deleted
     * outright.
     *
     * These are the raw dwords at those file offsets, taken with
     * `xxd -s 0x94 testdata/ce.rca`, not from any comment.  ce.rca +0x94
     * is 00 00 00 01 on disc, i.e. 0x01000000 little-endian.
     * ---------------------------------------------------------------- */
    check(ce.auParams[0]  == 0x01000000u, "ce +0x94 word is 0x01000000");
    check(bb.auParams[0]  == 0x02010000u, "bb +0x94 word is 0x02010000");
    check(ce.auParams[1]  == 0x00000000u, "ce +0x98 word is zero");
    /* +0x9C, gear 1.  Chosen over +0x94 for the byte-order check because
     * its four bytes 52 B8 4E 40 are all different, so any p[n] mix-up in
     * rd_u32le shows.  0x404EB852 is exactly the float 3.23f. */
    check(ce.auParams[2]  == 0x404EB852u, "ce +0x9C word is 0x404EB852");
    /* The LAST word of the block: the loop must run all 24 times. */
    check(ce.auParams[BR_RCA_PARAM_COUNT - 1] == 0x406A9280u,
          "ce +0xF0, the 24th word, is 0x406A9280");
    check(bb.auParams[BR_RCA_PARAM_COUNT - 1] == 0x408A3AE7u,
          "bb +0xF0, the 24th word, is 0x408A3AE7");

    /* The same dwords as floats.  These are exact -- 3.23f IS 0x404EB852
     * -- so `==` is right here and a tolerance would be the hole again. */
    check(ce.gears[0] == 3.23f, "ce gear 1 is exactly 3.23f");
    check(ce.gears[5] == 0.6f,  "ce gear 6 is exactly 0.6f");
    check(bb.gears[5] == 0.65f, "bb gear 6 is exactly 0.65f");
    check(ce.afParams[2] == ce.gears[0],
          "gears[] is afParams[] from index 2, not a separate decode");

    /* br_rca.h claims +0xC8 is the collision box and quotes ce.rca as
     * (3.5, 2.0, 0.8, 0.7).  The bytes agree, so the claim is pinned here
     * rather than left as prose. */
    check(ce.afParams[13] == 3.5f && ce.afParams[14] == 2.0f
       && ce.afParams[15] == 0.8f && ce.afParams[16] == 0.7f,
          "ce collision box at +0xC8 is (3.5, 2.0, 0.8, 0.7)");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
