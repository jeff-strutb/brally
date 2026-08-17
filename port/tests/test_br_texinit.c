/* test_br_texinit.c -- 0x10029B50 and 0x10029B10.
 *
 * The level decision is three unsigned comparisons with two literal
 * thresholds, and every plausible way to get it wrong changes the answer only
 * on a narrow band of inputs. So it is tested AT the boundaries rather than
 * near them: one below, exactly on, one above, for each threshold.
 */
#include "br_texinit.h"

#include <stdio.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ---- the thirteen slots, and the ORDER they are written --------------- */
static void test_slot_table(void)
{
    BrTexInitResetForTest();
    CHECK(BR_TEXINIT_NSLOTS == 13);

    /* 0x118ED19C is ELEVENTH in the install order while being the LOWEST
     * address of the thirteen. A version that sorted the slots would look
     * tidier and would not be this program. */
    CHECK(BrTexInitSlotAddr(10) == 0x118ED19Cu);
    CHECK(BrTexInitSlotValue(10) == 0x100298C0u);
    CHECK(BrTexInitSlotAddr(9)  == 0x118ED1E0u);   /* the one before it */
    CHECK(BrTexInitSlotAddr(11) == 0x118ED1E4u);   /* and after */

    /* the two slots the display-list opcodes are known to call through */
    CHECK(BrTexInitSlotAddr(4) == 0x118ED1CCu);    /* opcode 0xDC */
    CHECK(BrTexInitSlotValue(4) == 0x100284E0u);
    CHECK(BrTexInitSlotAddr(5) == 0x118ED1D0u);    /* opcode 0xDD */
    CHECK(BrTexInitSlotValue(5) == 0x100285E0u);

    /* every slot distinct, every value distinct */
    {
        int i, j;
        for (i = 0; i < BR_TEXINIT_NSLOTS; ++i)
            for (j = i + 1; j < BR_TEXINIT_NSLOTS; ++j) {
                CHECK(BrTexInitSlotAddr(i)  != BrTexInitSlotAddr(j));
                CHECK(BrTexInitSlotValue(i) != BrTexInitSlotValue(j));
            }
    }
}

static void test_install_order(void)
{
    int i;
    BrTexInitResetForTest();
    BrTexInit(NULL);
    CHECK(BrTexInitInstalledCount() == 13);
    for (i = 0; i < 13; ++i)
        CHECK(BrTexInitInstalledAt(i) == BrTexInitSlotAddr(i));
}

/* ---- the level decision, AT each boundary ---------------------------- */
static void test_level_low_threshold(void)
{
    BrTexInitResetForTest();
    g_brTexLowThreshold = 1000;
    g_brTexSysMem = 0xFFFFFFFFu;          /* keep the second test out of it */

    CHECK(BrTexChooseLevel(999)  == 2);
    CHECK(BrTexChooseLevel(1000) == 2);   /* `jbe` -- EQUAL takes this arm */
    CHECK(BrTexChooseLevel(1001) != 2);
}

static void test_level_sysmem(void)
{
    BrTexInitResetForTest();
    g_brTexLowThreshold = 0;              /* never the low arm */

    g_brTexSysMem = 0x2000000u;           /* exactly 32 MB */
    CHECK(BrTexChooseLevel(0xFFFFFFFFu) == 1);   /* `jbe` -- equal -> 1 */
    g_brTexSysMem = 0x2000001u;
    CHECK(BrTexChooseLevel(0xFFFFFFFFu) == 0);   /* one byte more -> tested */
}

static void test_level_texmem_boundary(void)
{
    BrTexInitResetForTest();
    g_brTexLowThreshold = 0;
    g_brTexSysMem = 0xFFFFFFFFu;

    /* 0x3D0900 == 4,000,000 DECIMAL, not 4 MiB. The `sbb/neg` is
     * `texmem < 0x3D0900`, so SMALL cards get 1 and large ones 0. */
    CHECK(BrTexChooseLevel(0x3D08FFu) == 1);
    CHECK(BrTexChooseLevel(0x3D0900u) == 0);   /* exactly on -> NOT less */
    CHECK(BrTexChooseLevel(0x3D0901u) == 0);

    /* and the 4 MiB value a reader might assume instead lands on the other
     * side of the real threshold, which is why the constant matters */
    CHECK(BrTexChooseLevel(0x400000u) == 0);
}

/* ---- measurement, including the second TMU --------------------------- */
static uint32_t s_min[2], s_max[2];
static int      s_cMin, s_cMax;
static uint32_t h_min(void *p, int32_t t) { (void)p; s_cMin++; return s_min[t]; }
static uint32_t h_max(void *p, int32_t t) { (void)p; s_cMax++; return s_max[t]; }

static void test_measure(void)
{
    BrTexInitHost h;
    h.pfnTexMinAddress = h_min; h.pfnTexMaxAddress = h_max; h.pUser = NULL;

    BrTexInitResetForTest();
    s_min[0] = 0x1000; s_max[0] = 0x1000 + 2000000u;
    s_min[1] = 0x9000; s_max[1] = 0x9000 + 2000000u;
    s_cMin = s_cMax = 0;

    g_brTexTmuCount = 1;                  /* one unit: the second is NOT read */
    BrTexInit(&h);
    CHECK(BrTexInitMemory() == 2000000u);
    CHECK(s_cMin == 1 && s_cMax == 1);

    BrTexInitResetForTest();
    s_min[0] = 0x1000; s_max[0] = 0x1000 + 2000000u;
    s_min[1] = 0x9000; s_max[1] = 0x9000 + 2000000u;
    s_cMin = s_cMax = 0;
    g_brTexTmuCount = 2;                  /* two units: summed */
    /* The reset zeroes sysmem, and zero satisfies `<= 0x2000000`, so the
     * 32 MB arm would fire before the texmem test is ever reached. Set it
     * high so this case actually exercises the boundary it claims to. */
    g_brTexSysMem = 0xFFFFFFFFu;
    BrTexInit(&h);
    CHECK(BrTexInitMemory() == 4000000u);
    CHECK(s_cMin == 2 && s_cMax == 2);

    /* 4,000,000 is exactly the threshold, and `<` excludes it -> level 0.
     * One byte less flips it. That pins the boundary through the REAL path,
     * not just through BrTexChooseLevel. */
    CHECK(BrTexInitLevel() == 0);
}

static void test_no_host(void)
{
    BrTexInitResetForTest();
    BrTexInit(NULL);
    /* no card -> zero memory -> the low arm, which is the original's own
     * answer for a card with no texture memory */
    CHECK(BrTexInitMemory() == 0);
    CHECK(BrTexInitLevel() == 2);
    CHECK(BrTexInitInstalledCount() == 13);   /* installation still happened */
}

int main(void)
{
    test_slot_table();
    test_install_order();
    test_level_low_threshold();
    test_level_sysmem();
    test_level_texmem_boundary();
    test_measure();
    test_no_host();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_texinit: all checks passed\n");
    return 0;
}
