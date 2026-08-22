#include "slice2_24.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* =====================================================================
 * Stand-ins for the cross-slice imports.  TEST ONLY -- none of these is a
 * port of the original.
 * ===================================================================== */

#define STRTAB_N 0x12F
static char  g_strtab[STRTAB_N][32];
static int   g_strtabSet[STRTAB_N];
static int   g_cStringById;

char *BrStringById(int32_t id)
{
    g_cStringById++;
    if (id < 1 || id >= STRTAB_N || !g_strtabSet[id])
        return NULL;
    return g_strtab[id];
}

static void strtab_set(int32_t id, const char *psz)
{
    snprintf(g_strtab[id], sizeof g_strtab[id], "%s", psz);
    g_strtabSet[id] = 1;
}

static int g_cFF30, g_cFF60, g_cFFF0, g_c709A0, g_c44B90, g_c44E20;
void BrMenuSub1005FF30(void) { g_cFF30++; }
void BrMenuSub1005FF60(void) { g_cFF60++; }
void BrMenuSub1005FFF0(void) { g_cFFF0++; }
void BrMenuSub100709A0(void) { g_c709A0++; }
void BrMenuSub10044B90(int32_t n) { (void)n; g_c44B90++; }
void BrMenuSub10044E20(int32_t n) { (void)n; g_c44E20++; }

uint8_t *g_pBrMenuACED34;

/* =====================================================================
 * A vtable that records which slots ran, in order.
 * ===================================================================== */

static int g_calls[16];
static int g_cCalls;

static void rec(int slot) { if (g_cCalls < 16) g_calls[g_cCalls++] = slot; }
static void BR_THISCALL1 v00(BrMenuText *p) { (void)p; rec(0x00); }
static void BR_THISCALL1 v04(BrMenuText *p) { (void)p; rec(0x04); }
static void BR_THISCALL1 v08(BrMenuText *p) { (void)p; rec(0x08); }
static void BR_THISCALL1 v0C(BrMenuText *p) { (void)p; rec(0x0C); }
static void BR_THISCALL1 v10(BrMenuText *p) { (void)p; rec(0x10); }
static void BR_THISCALL1 v14(BrMenuText *p) { (void)p; rec(0x14); }
static void BR_THISCALL1 v18(BrMenuText *p) { (void)p; rec(0x18); }
static void BR_THISCALL1 v1C(BrMenuText *p) { (void)p; rec(0x1C); }
static void BR_THISCALL1 v20(BrMenuText *p) { (void)p; rec(0x20); }
static void BR_THISCALL1 v24(BrMenuText *p) { (void)p; rec(0x24); }
static void BR_THISCALL1 v28(BrMenuText *p) { (void)p; rec(0x28); }
static void BR_THISCALL1 v2C(BrMenuText *p) { (void)p; rec(0x2C); }

static const BrMenuTextVtbl g_vt = {
    v00, v04, v08, v0C, v10, v14, v18, v1C, v20, v24, v28, v2C
};

static BrMenuItem g_item;

static void item_reset(void)
{
    memset(&g_item, 0, sizeof g_item);
    g_item.text.pVtbl = &g_vt;
    g_cCalls = 0;
}

/* Freshly zero the module state, then restore the three indices the image
 * ships non-zero, exactly as the loader would. */
static BrMenuState *state_reset(void)
{
    BrMenuState *pSt = BrMenuGetState();
    memset(pSt, 0, sizeof *pSt);
    pSt->g0AC648 = 2u;
    pSt->g0AC64C = 1u;
    pSt->g0AC650 = 1u;
    return pSt;
}

static int calls_are(int a, int b)
{
    return g_cCalls == 2 && g_calls[0] == a && g_calls[1] == b;
}

/* =====================================================================
 * A stage table.  Field values are arbitrary; only the ADDRESSING is under
 * test, and that is fixed by the disassembly.
 * ===================================================================== */

/* The stage table is g_brStages now, not a host pointer the test can aim --
 * see slice2_24.h.  The fixture is therefore written INTO it. */
#define g_stages g_brStages

static void stages_init(void)
{
    int i, k;
    memset(g_stages, 0, sizeof g_stages);
    for (i = 0; i < 4; i++) {
        g_stages[i].f00 = 0x40 + i;
        g_stages[i].f08 = 10 + 5 * i;
        for (k = 0; k < 4; k++) {
            /* low byte and high byte differ so the two readers cannot be
             * confused with each other; both stay inside the live part of
             * the tables they index */
            unsigned lo = (unsigned)(i + k);
            unsigned hi = (unsigned)(i + 2 * k) % 5u;
            g_stages[i].f10[k] = (uint16_t)((hi << 8) | lo);
        }
    }
}

int main(void)
{
    printf("slice2_24\n");

    /* ---- the stage record really is 0x18 bytes wide ------------------ */
    printf("\nlayout\n");
    check(sizeof(BrMenuStage) == 0x18,
          "BrMenuStage is 0x18 bytes, the stride three separate access "
          "patterns in the packet agree on");

    /* =================================================================
     * BrMenuFormatLapTime
     * ================================================================= */
    printf("\nBrMenuFormatLapTime\n");
    {
        char sz[32];
        union { uint32_t u; float f; } nan;
        int  i, ok = 1, firstBad = -1;

        BrMenuFormatLapTime(sz, sizeof sz, 0.0f);
        check(strcmp(sz, "--:--") == 0, "exactly 0 renders as --:--");

        BrMenuFormatLapTime(sz, sizeof sz, -1.0f);
        check(strcmp(sz, "--:--") == 0, "negatives render as --:--");

        nan.u = 0x7FC00000u;
        BrMenuFormatLapTime(sz, sizeof sz, nan.f);
        check(strcmp(sz, "--:--") == 0,
              "NaN renders as --:-- (the x87 guard tests C3|C0, and unordered "
              "sets both)");

        BrMenuFormatLapTime(sz, sizeof sz, 1e-6f);
        check(strcmp(sz, "0:00.00") == 0,
              "a positive time below one hundredth still formats");

        /* The one real invariant of the pipeline: whatever it prints, the
         * three fields always add back up to the truncated centisecond
         * count.  This holds even where the 0.01f multiply shifts a second
         * into the hundredths column. */
        for (i = 1; i <= 20000; i++) {
            float   t = (float)i * 0.173f;
            int     m = -1, s = -1, h = -1;
            int32_t centi;

            if (t >= 3600.0f)
                break;
            BrMenuFormatLapTime(sz, sizeof sz, t);
            if (sscanf(sz, "%d:%d.%d", &m, &s, &h) != 3) {
                ok = 0; firstBad = i; break;
            }
            centi = (int32_t)((double)t * (double)100.0f);
            if (m * 6000 + s * 100 + h != centi) {
                ok = 0; firstBad = i; break;
            }
        }
        if (!ok)
            printf("      first mismatch at i=%d\n", firstBad);
        check(ok, "min*6000 + sec*100 + hundredths == trunc(t*100) for every "
                  "sampled time");

        /* The documented consequence of using 0.01f rather than an integer
         * divide: an exact whole second lands in the hundredths column.  It
         * is asserted so that anyone "fixing" the formatter to integer maths
         * trips this test on purpose. */
        BrMenuFormatLapTime(sz, sizeof sz, 1.0f);
        check(strcmp(sz, "0:00.100") == 0,
              "exactly 1.000 s prints as 0:00.100 -- 100 * 0.01f is just "
              "under 1.0, so the seconds count truncates to 0");

        BrMenuFormatLapTime(sz, sizeof sz, 60.0f);
        check(strcmp(sz, "0:59.100") == 0,
              "the same slip happens at EVERY exact multiple of 100 "
              "centiseconds, not just at one second");

        BrMenuFormatLapTime(sz, sizeof sz, 1.015f);
        check(strcmp(sz, "0:01.01") == 0,
              "one centisecond off a whole second and the columns are right "
              "again");

        BrMenuFormatLapTime(sz, sizeof sz, 61.015f);
        check(strcmp(sz, "1:01.01") == 0, "the minutes column carries");

        BrMenuFormatLapTime(sz, sizeof sz, 125.37f);
        check(strcmp(sz, "2:05.37") == 0,
              "and a plain lap time formats as MM:SS.hh with the seconds "
              "zero-padded");
    }

    /* =================================================================
     * Caption setters
     * ================================================================= */
    printf("\ncaption setters\n");
    {
        BrMenuState *pSt = state_reset();
        stages_init();

        item_reset();
        pSt->g0AA010 = 1;                 /* take the g0AC648 path */
        check(BrMenuCap0730(&g_item) == 1 && g_item.f1E20C == 0x11,
              "0x10040730 with 0x100AA010 set reads 0x100AC550[0x100AC648], "
              "and the image ships 0x100AC648 == 2");

        item_reset();
        pSt->g0AA010 = 0;
        pSt->gAA28B8 = 2;                 /* stage record 2 */
        pSt->gAA28A8 = 1;                 /* select the 0x10AA28AC column */
        pSt->gAA28AC = 1;
        pSt->gAA28A4 = 3;
        check(BrMenuCap0730(&g_item) == 1,
              "0x10040730 returns 1 on the stage path");
        check(g_item.f1E20C == (int16_t)0x1B,
              "0x10040730 takes the LOW byte of stage[2].f10[1] (== 3) and "
              "indexes 0x100AC550 with it");

        item_reset();
        pSt->gAA28A8 = 0;                 /* now the 0x10AA28A4 column (3) */
        check(BrMenuCap0730(&g_item) == 1 && g_item.f1E20C == (int16_t)0x76,
              "the byte at 0x10AA28A8 chooses which of 0x10AA28A4 / "
              "0x10AA28AC supplies the column: low byte of stage[2].f10[3] "
              "is 5, and 0x100AC550[5] is 0x76");

        item_reset();
        pSt->gAA2904 = 7; pSt->gAA2964 = 7; pSt->gAA28E8 = 0;
        g_item.f1E20C = 0x5A5A;
        check(BrMenuCap07A0(&g_item) == -2 && g_item.f1E20C == 0x5A5A,
              "0x100407A0 answers the reserved -2 and leaves the item alone "
              "when 0x10AA2904 == 0x10AA2964 and 0x10AA28E8 is clear");

        pSt->gAA28E8 = 1;                 /* break the guard */
        item_reset();
        check(BrMenuCap07A0(&g_item) == 1 && g_item.f1E20C == 0x42,
              "0x100407A0 otherwise reads 0x100AC570[2] == 0x42");

        item_reset();
        pSt->g0AA010 = 0;
        pSt->gAA28B8 = 2;
        pSt->gAA28A8 = 1;
        pSt->gAA28AC = 1;
        /* high byte of stage[2].f10[1] is (2 + 2*1) % 5 == 4 */
        check(BrMenuCap07E0(&g_item) == 1 && g_item.f1E20C == (int16_t)0x14,
              "0x100407E0 reads the HIGH byte of the same stage word, not "
              "the low one, and indexes 0x100AC590 with it");

        pSt->gAA28AC = 0;                 /* high byte becomes 2 */
        item_reset();
        check(BrMenuCap07E0(&g_item) == 1 && g_item.f1E20C == (int16_t)0x15,
              "0x100407E0 tracks the column index too");

        state_reset();
        item_reset();
        check(BrMenuCap0870(&g_item) == 1 && g_item.f1E20C == 0x1D,
              "0x10040870 -> 0x100AC598[0]");
        item_reset();
        check(BrMenuCap0890(&g_item) == 1 && g_item.f1E20C == 0x19,
              "0x10040890 -> 0x100AC59C[0x100AC64C] with the shipped 1");
        item_reset();
        check(BrMenuCap08B0(&g_item) == 1 && g_item.f1E20C == 0x0E,
              "0x100408B0 -> 0x100AC5A0[0x100AC650] with the shipped 1");
        item_reset();
        check(BrMenuCap0930(&g_item) == 1 && g_item.f1E20C == 0x45,
              "0x10040930 -> 0x100AC62C[0]");
        item_reset();
        check(BrMenuCap09B0(&g_item) == 1 && g_item.f1E20C == 0x63,
              "0x100409B0 -> 0x100AC634[0]");
        item_reset();
        check(BrMenuCap09D0(&g_item) == 1 && g_item.f1E20C == 0x65,
              "0x100409D0 -> 0x100AC638[0]");
        item_reset();
        check(BrMenuCap1870(&g_item) == 1 && g_item.f1E20C == 0x32,
              "0x10041870 -> 0x100AC628[0]");
        item_reset();
        check(BrMenuCap0990(&g_item) == 1 && g_item.f1E20C == (int16_t)0x8C,
              "0x10040990 takes the low word of the DWORD table 0x100AC640");

        /* the 0x10040950 asymmetry */
        item_reset();
        {
            BrMenuState *p = BrMenuGetState();
            p->g18ABDBC = 0;
            p->gAA2A1C  = 0;
            check(BrMenuCap0950(&g_item) == 1 && g_item.f1E20C == 0x66,
                  "0x10040950 with 0x118ABDBC clear is hard-wired to "
                  "0x100AC631 -- element ONE, not element zero");
            item_reset();
            p->g18ABDBC = 1;
            check(BrMenuCap0950(&g_item) == 1 && g_item.f1E20C == 0x61,
                  "0x10040950 with 0x118ABDBC set indexes with 0x10AA2A1C");
        }
    }

    /* =================================================================
     * State seeding
     * ================================================================= */
    printf("\nstate seeding\n");
    {
        BrMenuState *pSt = state_reset();

        pSt->gAA25DC = 0xAABBCCDDu;
        pSt->gAA25D4 = 0x7F;
        pSt->gAA25D8 = 0x12345678u;
        check(BrMenuSeedFrom25D4() == 1, "0x100409F0 returns 1");
        check(pSt->gAA28A0 == 0xAABBCCDDu && pSt->gAA28B8 == 0x7F &&
              pSt->gAA28A4 == 0x12345678u,
              "0x100409F0 copies a full DWORD into 0x10AA28A4");

        state_reset();
        pSt->gAA26F0 = 0x11223344u;
        pSt->gAA26F4 = 0x40;
        pSt->gAA26F5 = 0xAB;
        check(BrMenuSeedFrom26F0() == 1, "0x10040A20 returns 1");
        check(pSt->gAA28A0 == 0x11223344u && pSt->gAA28B8 == 0x40 &&
              pSt->gAA28A4 == 0xABu,
              "0x10040A20 fills the same field from a ZERO-EXTENDED BYTE "
              "-- the asymmetry with 0x100409F0 is in the original");
    }

    /* =================================================================
     * Text setters and the two vtable tails
     * ================================================================= */
    printf("\ntext setters\n");
    {
        BrMenuState *pSt = state_reset();

        pSt->gAA28A0 = 6;
        item_reset();
        check(BrMenuText0A50(&g_item) == 1 &&
              strcmp(g_item.text.sz, "7") == 0,
              "0x10040A50 prints 0x10AA28A0 + 1");
        check(calls_are(0x08, 0x2C),
              "a VALUE assignment runs vtable +0x08 then +0x2C, in that order");

        pSt->gAA28A4 = 9;
        item_reset();
        check(BrMenuText0AC0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "10") == 0,
              "0x10040AC0 prints 0x10AA28A4 + 1");

        strtab_set(0x37, "Stage");
        pSt->gAA28A4 = 2;
        item_reset();
        check(BrMenuText0B30(&g_item) == 1 &&
              strcmp(g_item.text.sz, "Stage  3") == 0,
              "0x10040B30 builds string(0x37) + two spaces + the number, and "
              "does NOT uppercase it");
        check(calls_are(0x04, 0x10),
              "a CAPTION assignment runs vtable +0x04 then +0x10");

        pSt->g0BD3E0 = 12;
        pSt->gAA2904 = 0; pSt->gAA2964 = 0; pSt->gAA28E8 = 0;
        item_reset();
        g_item.text.sz[0] = 'X'; g_item.text.sz[1] = '\0';
        check(BrMenuText08D0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "X") == 0 && g_cCalls == 0,
              "0x100408D0 still answers 1 when the idle guard fires, but "
              "touches neither the text nor the vtable");
        pSt->gAA28E8 = 1;
        item_reset();
        check(BrMenuText08D0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "12") == 0 && calls_are(0x08, 0x2C),
              "0x100408D0 formats straight into the item's own buffer");

        pSt->gAA28A4 = 4;
        item_reset();
        check(BrMenuText1670(&g_item) == 1 &&
              strcmp(g_item.text.sz, "5") == 0 && calls_are(0x08, 0x2C),
              "0x10041670 prints 0x10AA28A4 + 1");
        pSt->gAA28C4 = 4;
        item_reset();
        check(BrMenuText1710(&g_item) == 1 &&
              strcmp(g_item.text.sz, "4") == 0,
              "0x10041710 is 0x10041670 WITHOUT the +1");
    }

    /* =================================================================
     * The clamped differences
     * ================================================================= */
    printf("\nclamped differences\n");
    {
        BrMenuState *pSt = state_reset();
        stages_init();

        pSt->gAA289C = 0;                 /* forces record 0 */
        pSt->gAA28C4 = 3;
        item_reset();
        check(BrMenuText15A0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "7") == 0,
              "0x100415A0 with 0x10AA289C clear uses stage[0].f08");

        pSt->gAA289C = 1;
        pSt->gAA28B8 = 3;
        item_reset();
        check(BrMenuText15A0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "22") == 0,
              "0x100415A0 otherwise indexes with the sign-extended byte at "
              "0x10AA28B8");

        pSt->gAA28C4 = 1000;
        item_reset();
        check(BrMenuText15A0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "0") == 0,
              "0x100415A0 clamps a negative difference to zero, it does not "
              "print a minus sign");

        pSt->g220B24 = 2;
        pSt->gAA28C4 = 4;
        item_reset();
        check(BrMenuText17B0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "16") == 0 && calls_are(0x08, 0x2C),
              "0x100417B0 uses 0x10220B24 as the record index");
        pSt->gAA28C4 = 999;
        item_reset();
        check(BrMenuText17B0(&g_item) == 1 &&
              strcmp(g_item.text.sz, "0") == 0,
              "0x100417B0 clamps too");
    }

    /* =================================================================
     * 0x10041300 -- the double lookup and the in-place uppercase
     * ================================================================= */
    printf("\n0x10041300\n");
    {
        BrMenuState *pSt = state_reset();
        stages_init();
        pSt->gAA289C = 0;                 /* record 0 -> f00 == 0x40 */

        strtab_set(0x40, "monte carlo");
        g_cStringById = 0;
        item_reset();
        check(BrMenuText1300(&g_item) == 1 &&
              strcmp(g_item.text.sz, "MONTE CARLO") == 0,
              "0x10041300 uppercases the stage name");
        check(g_cStringById == 2,
              "the string id is looked up TWICE -- once to measure, once to "
              "use");
        check(strcmp(g_strtab[0x40], "MONTE CARLO") == 0,
              "_strupr works in place, so the string TABLE entry itself is "
              "left uppercased");

        strtab_set(0x40, "");
        item_reset();
        g_item.text.sz[0] = 'Z'; g_item.text.sz[1] = '\0';
        check(BrMenuText1300(&g_item) == 0 && g_cCalls == 0 &&
              g_item.text.sz[0] == 'Z',
              "an empty string makes 0x10041300 bail out with 0 and touch "
              "nothing");
    }

    /* =================================================================
     * Flag pokers
     * ================================================================= */
    printf("\nflag pokers\n");
    {
        BrMenuState *pSt = state_reset();

        item_reset();
        g_item.f1C     = 0xFFFFFFFFu;
        g_item.f1E20C  = 0x1234;
        g_item.text.f08 = 0x77;
        pSt->gAA28E0 = 1;
        check(BrMenuFlags1890(&g_item) == 1 &&
              g_item.f1C == 0xFFFFEFEFu &&
              g_item.f1E20C == 0x1234 && g_item.text.f08 == 0x77,
              "0x10041890 with the flag SET only clears bits 0x1010");

        item_reset();
        g_item.f1C      = 0x00000101u;
        g_item.f1E20C   = 0x1234;
        g_item.text.f08 = 0x77;
        pSt->gAA28E0 = 0;
        check(BrMenuFlags1890(&g_item) == 1 &&
              g_item.f1C == 0x00001111u &&
              g_item.f1E20C == 2 && g_item.text.f08 == 0,
              "0x10041890 with the flag CLEAR also forces the string id to 2 "
              "and zeroes item +0x2B64 -- the branches are not symmetric");

        /* round trip: setting then clearing restores every other bit */
        item_reset();
        g_item.f1C = 0xDEADBEEFu;
        pSt->gAA28E0 = 0;
        (void)BrMenuFlags1890(&g_item);
        pSt->gAA28E0 = 1;
        (void)BrMenuFlags1890(&g_item);
        check(g_item.f1C == (0xDEADBEEFu & 0xFFFFEFEFu),
              "set-then-clear leaves every bit outside 0x1010 untouched");

        item_reset();
        g_item.f1C      = 0xFFFFFFFFu;
        g_item.f1E20C   = 0x1234;
        g_item.text.f08 = 0x77;
        pSt->gAA28E4 = 0;
        check(BrMenuFlags18D0(&g_item) == 1 && g_item.f1C == 0xFFFFFFFFu &&
              g_item.f1E20C == 0x1234 && g_item.text.f08 == 0x77,
              "0x100418D0 with the flag clear does nothing at all -- it is "
              "the odd one out of the three");
        pSt->gAA28E4 = 1;
        check(BrMenuFlags18D0(&g_item) == 1 && g_item.f1C == 0xFFFFEFEFu,
              "0x100418D0 with the flag set clears bits 0x1010");

        item_reset();
        g_item.f1C = 0u;
        pSt->gAA28E8 = 0;
        check(BrMenuFlags18F0(&g_item) == 1 && g_item.f1C == 0x1010u &&
              g_item.f1E20C == 2,
              "0x100418F0 is 0x10041890 driven by 0x10AA28E8");
    }

    /* =================================================================
     * Entry / exit
     * ================================================================= */
    printf("\nentry / exit\n");
    {
        BrMenuState *pSt = state_reset();

        g_cFF30 = g_cFF60 = g_cFFF0 = 0;
        check(BrMenuEnter() == 1 && pSt->gAA28D8 == 1 && pSt->gAA2844 == 1 &&
              g_cFF30 == 1 && g_cFF60 == 1 && g_cFFF0 == 1,
              "0x10040680 arms the module and runs its three helpers once");
        check(BrMenuEnter() == 1 && g_cFF30 == 1,
              "0x10040680 is idempotent -- 0x10AA2844 gates it");

        state_reset();
        pSt->gACEE50 = -1;
        pSt->g0BD3E0 = 0;
        g_c44B90 = g_c44E20 = 0;
        check(BrMenuLeaveTo2() == 1 && g_c44B90 == 0 && pSt->g0AA010 == 2,
              "0x10041930 compares 0x10ACEE50 with 0x100BD3E0 SIGNED, so -1 "
              "does not trigger the teardown");
        state_reset();
        pSt->gACEE50 = 5;
        pSt->g0BD3E0 = 5;
        check(BrMenuLeaveTo2() == 1 && g_c44B90 == 1 && g_c44E20 == 1 &&
              pSt->g0AA010 == 2,
              "equality does trigger it (the branch is `jl`, so >= runs)");
    }

    /* =================================================================
     * 0x10041B50
     * ================================================================= */
    printf("\n0x10041B50\n");
    {
        BrMenuState *pSt = state_reset();
        static uint8_t rec[0x200];
        size_t i;

        g_c709A0 = 0;
        pSt->gACED34_present = 0;
        g_pBrMenuACED34 = NULL;
        BrMenuAutoSaveName();
        check(g_c709A0 == 0 && pSt->g1782CD0[0] == '\0',
              "a null 0x10ACED34 makes 0x10041B50 return before it writes "
              "the file name");

        for (i = 0; i < sizeof rec; i++)
            rec[i] = 0xEE;
        rec[4] = 0; rec[5] = 0;
        pSt->gACED34_present = 1;
        g_pBrMenuACED34 = rec;
        BrMenuAutoSaveName();
        check(strcmp(pSt->g1782CD0, "AutoSave.brf") == 0,
              "the name goes to 0x11782CD0");
        check(g_c709A0 == 1, "0x100709A0 runs");
        {
            int cleared = 1;
            for (i = 0x06; i < 0x06 + 24; i++) if (rec[i] != 0) cleared = 0;
            for (i = 0x1E; i < 0x1E + 48; i++) if (rec[i] != 0) cleared = 0;
            for (i = 0x50; i < 0x50 + 96; i++) if (rec[i] != 0) cleared = 0;
            check(cleared,
                  "the three runs cleared are 24, 48 and 96 bytes at +6, "
                  "+0x1E and +0x50");
            check(rec[0x50 + 96] == 0xEE && rec[0x00] == 0xEE,
                  "and nothing outside them is touched");
        }

        for (i = 0; i < sizeof rec; i++)
            rec[i] = 0xEE;
        rec[4] = 1;
        g_c709A0 = 0;
        pSt->g1782CD0[0] = '\0';
        BrMenuAutoSaveName();
        check(rec[0x06] == 0xEE && g_c709A0 == 1 &&
              strcmp(pSt->g1782CD0, "AutoSave.brf") == 0,
              "a non-zero byte at +4 skips the clearing but neither the name "
              "nor the 0x100709A0 call");
    }

    {
        BrMenuState *pSt = BrMenuGetState();
        pSt->gAA28A8 = 7;
        pSt->gAA28D0 = 9;
        check(BrMenuClearAA28A8() == 1 && pSt->gAA28A8 == 0,
              "0x1003E8C0 clears 0x10AA28A8");
        check(BrMenuSetAA28A8() == 1 && pSt->gAA28A8 == 1,
              "0x1003E8B0 sets 0x10AA28A8");
        check(BrMenuSetAA28D0_0() == 1 && pSt->gAA28D0 == 0,
              "0x100412C0 stores 0");
        check(BrMenuSetAA28D0_1() == 1 && pSt->gAA28D0 == 1,
              "0x100412D0 stores 1");
        check(BrMenuSetAA28D0_2() == 1 && pSt->gAA28D0 == 2,
              "0x100412E0 stores 2");
        check(BrMenuSetAA28D0_3() == 1 && pSt->gAA28D0 == 3,
              "0x100412F0 stores 3");
    }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
