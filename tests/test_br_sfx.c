/* test_br_sfx.c -- the sound bank and the pitch arithmetic, with no audio
 * device and (mostly) no disc.
 *
 * The assertions are properties of the code, not counts of it:
 *
 *   - the row stride and the table extent are ARITHMETIC facts in the image
 *     (0x100B55F8 + 26*72 == 0x100B5D48 == the bank table, and that + 26*72
 *     lands exactly on the string "r.wav"), so the geometry is checked as
 *     arithmetic rather than as a remembered number;
 *   - the pitch pair is an inverse, so it is checked as a round trip;
 *   - 0x100000000 means "play at the base rate" by construction, so that is
 *     checked as an identity at every base rate the table actually uses;
 *   - the filename rule is checked against the DISC when the disc is
 *     available (testdata/sfx.txt), because that is the only test that can
 *     tell a plausible-but-wrong name table from a right one.
 *
 * The disc-backed part SKIPs, loudly, when the listing has not been
 * extracted -- see port/tests/br_testdata.h and README's asset policy.
 */
#include "br_sfx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------- geometry */

static void test_geometry(void)
{
    int g, s, n;

    printf("geometry\n");

    /* Both tables are 1872 bytes on a 32-bit host: 26 rows of (16 dwords +
     * one double).  That is what makes the two abut and what makes the bank
     * table end on "r.wav". */
    check(BR_SFX_GROUPS * (BR_SFX_SLOTS * 4 + 8) == 1872,
          "26 rows of 72 bytes == 1872, the measured table extent");
    check(BR_SFX_SLOTS * 4 + 8 == BR_SFX_ROW_DWORDS * 4,
          "16 pointers + a double occupy exactly the 18-dword stride");

    /* The flat index the original computes. */
    check(BrSfxVoiceIndex(0, 0) == 0, "voice index of group 0 slot 0");
    check(BrSfxVoiceIndex(1, 0) == BR_SFX_ROW_DWORDS, "one group is 18 apart");
    check(BrSfxVoiceIndex(24, 0) == 432,
          "group 24 (the h layer) is 432 dwords in -- 0x6C0 bytes, as measured");
    check(BrSfxVoiceIndex(25, 0) == 450,
          "group 25 (the r layer) is 450 dwords in -- 0x708 bytes, as measured");
    check(BrSfxVoiceIndex(BR_SFX_GROUPS, 0) == -1, "group past the end rejected");
    check(BrSfxVoiceIndex(0, BR_SFX_SLOTS) == -1, "slot past the end rejected");
    check(BrSfxVoiceIndex(-1, 0) == -1 && BrSfxVoiceIndex(0, -1) == -1,
          "negative indices rejected");

    /* Every valid (group, slot) maps to a distinct index inside the table. */
    n = 0;
    for (g = 0; g < BR_SFX_GROUPS; ++g)
        for (s = 0; s < BR_SFX_SLOTS; ++s) {
            int i = BrSfxVoiceIndex(g, s);
            if (i >= 0 && i < BR_SFX_GROUPS * BR_SFX_ROW_DWORDS)
                n++;
        }
    check(n == BR_SFX_GROUPS * BR_SFX_SLOTS,
          "every slot lands inside the table and none collide with the double");

    /* Cars own even channels; the last car that fits is 7 (channel 14), which
     * is why the engine clears exactly 15 channels. */
    check(BrSfxCarChannel(0) == 0 && BrSfxCarChannel(7) == 14,
          "car i owns channel 2*i");
    check(BrSfxCarChannel(8) == -1, "an eighth car would fall off the 15 channels");
    check(BrSfxCarChannel(-1) == -1, "negative car rejected");
    check(BR_SFX_CHANNEL_ONESHOT % 2 == 1,
          "the one-shot channel is odd, so it never collides with a car's");
}

/* ---------------------------------------------------------------- table */

static void test_table(void)
{
    int g, nMarked, nRowsWithOne;

    printf("bank table\n");

    /* Exactly one slot is marked per generic group, and none for the three
     * per-car groups -- that is what makes the disc need 73 files and not
     * 26*15. */
    nMarked = 0;
    nRowsWithOne = 0;
    for (g = 0; g < BR_SFX_GROUPS; ++g) {
        int s, k = 0;
        for (s = 0; s < BR_SFX_SLOTS; ++s)
            if (BrSfxGroupSlotUsed(g, s)) k++;
        nMarked += k;
        if (k == 1) nRowsWithOne++;
    }
    check(nMarked == 23 && nRowsWithOne == 23,
          "23 groups mark exactly one slot each; the other three mark none");

    check(!BrSfxGroupSlotUsed(BR_SFX_GROUP_ENGINE, 0)
       && !BrSfxGroupSlotUsed(BR_SFX_GROUP_ENGINE_HIGH, 0)
       && !BrSfxGroupSlotUsed(BR_SFX_GROUP_ENGINE_REV, 0),
          "the engine groups are marked by the game, not by .data");

    /* Where the mark is decides which channel the sample can be reached on,
     * and 0x1006BA60 hardcodes channel 1. */
    check(BrSfxGroupSlotUsed(1, BR_SFX_CHANNEL_ONESHOT),
          "group 1 is reachable on the one-shot channel");
    check(BrSfxGroupSlotUsed(13, 3) && BrSfxGroupSlotUsed(14, 3)
       && BrSfxGroupSlotUsed(15, 3),
          "beep / beep2 / water sit on channel 3 instead");
    check(!BrSfxGroupSlotUsed(13, BR_SFX_CHANNEL_ONESHOT),
          "...and are NOT on the one-shot channel");

    /* Base rates: only these four values occur, and each is a plausible
     * mixer rate. */
    for (g = 0; g < BR_SFX_GROUPS; ++g) {
        double r = BrSfxGroupBaseRate(g);
        if (!(r == 11000.0 || r == 11025.0 || r == 22050.0 || r == 30000.0)) {
            printf("  group %d has base rate %f\n", g, r);
            check(0, "every base rate is one of the four in the image");
            break;
        }
    }
    if (g == BR_SFX_GROUPS)
        check(1, "every base rate is one of the four in the image");

    check(BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE) == 11025.0
       && BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE_HIGH) == 22050.0
       && BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE_REV) == 11025.0,
          "the three engine layers' base rates");
    check(BrSfxGroupBaseRate(-1) == 0.0
       && BrSfxGroupBaseRate(BR_SFX_GROUPS) == 0.0,
          "out-of-range groups report no rate");

    /* Group counts, and the loop bound they feed. */
    check(BrSfxGroupCount(BR_SFX_SET_RACE) == 25
       && BrSfxGroupCount(BR_SFX_SET_MENU) == 9,
          "the two set sizes 0x1006C290 stores");
    check(BrSfxGroupCount(7) == 0, "an unknown set has no groups");

    /* `for (row = 1; row < count-1; row++)`: the race set stops before the
     * two per-car rows, and every row it does reach is named. */
    for (g = 1; g < BrSfxGroupCount(BR_SFX_SET_RACE) - 1; ++g)
        if (BrSfxGroupName(BR_SFX_SET_RACE, g) == NULL) break;
    check(g == 24, "the race set names every group its loader reaches (1..23)");

    for (g = 1; g < BrSfxGroupCount(BR_SFX_SET_MENU) - 1; ++g)
        if (BrSfxGroupName(BR_SFX_SET_MENU, g) == NULL) break;
    check(g == 8, "the menu set names every group its loader reaches (1..7)");

    check(BrSfxGroupName(BR_SFX_SET_RACE, BR_SFX_GROUP_ENGINE) == NULL
       && BrSfxGroupName(BR_SFX_SET_RACE, BR_SFX_GROUP_ENGINE_HIGH) == NULL
       && BrSfxGroupName(BR_SFX_SET_RACE, BR_SFX_GROUP_ENGINE_REV) == NULL,
          "the per-car groups carry no fixed name");

    /* The two sets disagree about what a group number means. */
    check(strcmp(BrSfxGroupName(BR_SFX_SET_RACE, 1), "hit-another-car1.wav") == 0
       && strcmp(BrSfxGroupName(BR_SFX_SET_MENU, 1), "front-end5.wav") == 0,
          "group 1 is a different sample in each set");

    /* The shipped duplicate. Asserted because it is easy to 'fix' by
     * accident. */
    check(strcmp(BrSfxGroupName(BR_SFX_SET_RACE, 8),
                 BrSfxGroupName(BR_SFX_SET_RACE, 9)) == 0,
          "groups 8 and 9 really are the same file (rn_dirt), as shipped");
    check(BrSfxGroupBaseRate(8) != BrSfxGroupBaseRate(9),
          "...but at different base rates, which is why both rows exist");
}

/* ------------------------------------------------------------ filenames */

static void test_filenames(void)
{
    char buf[64];
    int n;

    printf("filenames\n");

    n = BrSfxCarFileName(BR_SFX_GROUP_ENGINE, 1, NULL, buf, sizeof(buf));
    check(n == (int)strlen(buf) && strcmp(buf, "sfx/ce.wav") == 0,
          "car 1 engine -> sfx/ce.wav");
    BrSfxCarFileName(BR_SFX_GROUP_ENGINE_HIGH, 1, NULL, buf, sizeof(buf));
    check(strcmp(buf, "sfx/ceh.wav") == 0, "high layer -> sfx/ceh.wav");
    BrSfxCarFileName(BR_SFX_GROUP_ENGINE_REV, 1, NULL, buf, sizeof(buf));
    check(strcmp(buf, "sfx/cer.wav") == 0, "rev layer -> sfx/cer.wav");

    BrSfxCarFileName(BR_SFX_GROUP_ENGINE, BR_SFX_CARS, NULL, buf, sizeof(buf));
    check(strcmp(buf, "sfx/mn.wav") == 0, "the last car code");

    /* The bank stores car+1, so 0 is "no car" and must not index the NULL
     * slot 0 of the code table. */
    check(BrSfxCarFileName(BR_SFX_GROUP_ENGINE, 0, NULL, buf, sizeof(buf)) < 0
          && buf[0] == '\0',
          "bank entry 0 means no car and yields no name");
    check(BrSfxCarFileName(BR_SFX_GROUP_ENGINE, BR_SFX_CARS + 1, NULL,
                           buf, sizeof(buf)) < 0,
          "a car index past the table is rejected");
    check(BrSfxCarFileName(1, 1, NULL, buf, sizeof(buf)) < 0,
          "a non-engine group has no per-car name");

    /* SFXDir= replaces the prefix wholesale, separator included. */
    BrSfxCarFileName(BR_SFX_GROUP_ENGINE, 1, "d:\\rally\\sfx\\", buf, sizeof(buf));
    check(strcmp(buf, "d:\\rally\\sfx\\ce.wav") == 0,
          "the prefix is substituted verbatim, separator and all");

    n = BrSfxGroupFileName(BR_SFX_SET_RACE, 13, NULL, buf, sizeof(buf));
    check(n == (int)strlen(buf) && strcmp(buf, "sfx/beep.wav") == 0,
          "a generic group name is used unchanged -- no extension appended");
    check(BrSfxGroupFileName(BR_SFX_SET_RACE, BR_SFX_GROUP_ENGINE_REV, NULL,
                             buf, sizeof(buf)) < 0,
          "the per-car groups have no generic name");

    /* Bounded: a buffer one short of the answer fails and leaves nothing. */
    check(BrSfxCarFileName(BR_SFX_GROUP_ENGINE, 1, NULL, buf, 10) < 0
          && buf[0] == '\0',
          "a buffer one byte short fails and writes nothing");
    check(BrSfxCarFileName(BR_SFX_GROUP_ENGINE, 1, NULL, buf, 11) == 10,
          "...and exactly enough succeeds");
}

/* --------------------------------------------------------------- pitch */

#define BR_SFX_UNITY  ((int64_t)1 << 32)

static void test_pitch(void)
{
    static const double aRate[4] = { 11000.0, 11025.0, 22050.0, 30000.0 };
    int i;
    uint32_t hz;
    int ok;

    printf("pitch\n");

    /* 2^32 is "play at the base rate" by construction. */
    ok = 1;
    for (i = 0; i < 4; ++i) {
        if (BrSfxRatioFromHz((uint32_t)aRate[i], aRate[i]) != BR_SFX_UNITY) ok = 0;
        if (BrSfxHzFromRatio(BR_SFX_UNITY, aRate[i]) != (uint32_t)aRate[i]) ok = 0;
    }
    check(ok, "0x100000000 is unity pitch at every base rate in the table");

    check(BrSfxRatioFromHz(44100, 22050.0) == 2 * BR_SFX_UNITY,
          "double the base rate is exactly 2.0 in 32.32");
    check(BrSfxRatioFromHz(11025, 22050.0) == BR_SFX_UNITY / 2,
          "half the base rate is exactly 0.5 in 32.32");
    check(BrSfxHzFromRatio(BR_SFX_UNITY / 2, 22050.0) == 11025,
          "...and back again");

    /* Round trip over the whole range an engine note can plausibly reach.
     *
     * It is NOT the identity, and that is the interesting part.  Both
     * directions truncate toward zero, so the ratio is at most one unit below
     * the exact value and the recovered frequency is therefore hz or hz-1 --
     * never hz+1, and never further off than that.  A pitch that is written
     * and then read back drifts DOWN by at most a hertz per trip.
     *
     * Asserting "== hz" here looked right and failed on the very first case
     * (1000 Hz at base 11000 comes back 999).  The bound is the property; the
     * equality was an expectation. */
    ok = 1;
    for (i = 0; i < 4 && ok; ++i)
        for (hz = 1000; hz <= 60000; hz += 7) {
            uint32_t back = BrSfxHzFromRatio(BrSfxRatioFromHz(hz, aRate[i]),
                                             aRate[i]);
            if (back != hz && back != hz - 1) {
                printf("  %u Hz at base %f round-tripped to %u\n",
                       hz, aRate[i], back);
                ok = 0;
                break;
            }
        }
    check(ok, "Hz -> 32.32 -> Hz loses at most one hertz, and never gains");

    check(BrSfxHzFromRatio(BrSfxRatioFromHz(1000, 11000.0), 11000.0) == 999,
          "...and the loss is real: 1000 Hz at base 11000 comes back 999");
    check(BrSfxHzFromRatio(BrSfxRatioFromHz(22050, 11025.0), 11025.0) == 22050,
          "an exact power of two round-trips unchanged");

    /* Monotonic: a faster note is never a smaller ratio. */
    ok = 1;
    for (hz = 100; hz < 50000; hz += 13)
        if (BrSfxRatioFromHz(hz, 22050.0) > BrSfxRatioFromHz(hz + 13, 22050.0)) {
            ok = 0; break;
        }
    check(ok, "the ratio is monotonic in frequency");

    check(BrSfxRatioFromHz(0, 22050.0) == 0
       && BrSfxHzFromRatio(0, 22050.0) == 0,
          "silence at zero maps to zero both ways");

    /* GOTCHA, preserved: no guard on an unbound channel's zero base rate.
     * The divide gives an infinity and _ftol stores the indefinite value. */
    check(BrSfxRatioFromHz(22050, 0.0) == (int64_t)((uint64_t)1 << 63),
          "a zero base rate yields the x87 integer indefinite, not a trap");
    check(BrSfxRatioFromHz(0, 0.0) == (int64_t)((uint64_t)1 << 63),
          "0/0 is a NaN and takes the same side (unordered compares, not less)");

    /* A realistic large ratio: four times the base rate. */
    check(BrSfxHzFromRatio(4 * BR_SFX_UNITY, 11025.0) == 44100,
          "four times the base rate converts to 4 * 11025");

    /* _ftol keeps the LOW DWORD only on the Hz side.  With a base rate of
     * exactly 2^32 the conversion is the identity in exact arithmetic, so
     * whatever comes back is purely the truncation: the top 32 bits are gone.
     * (0x10077C08's 2^32 and 0x10077C00's 2^-32 cancel, which is what makes
     * this a clean probe rather than a contrived one.) */
    check(BrSfxHzFromRatio(((int64_t)1 << 32) + 5, 4294967296.0) == 5,
          "only the low dword of the _ftol result survives");
    check(BrSfxHzFromFloat(11025.9f) == 11025,
          "the float entry point truncates toward zero");
    check(BrSfxHzFromFloat(-1.5f) == (uint32_t)-1,
          "...toward zero for negatives too, kept as the low dword");
    check(BrSfxHzFromFloat(1.0e30f) == 0,
          "an out-of-range float reads back as 0, per _ftol's low dword");
}

/* --------------------------------------------------------- engine curve */

static void test_engine(void)
{
    double   hz;
    uint32_t f, prev;
    float    rpm;
    int      ok;

    printf("engine curve\n");

    /* The scale is 55/7 Hz per RPM, so unity against the 11000 the ratio is
     * derived with lands at 1400 RPM.  Stated as the arithmetic, because a
     * remembered "11000 at 1400" is exactly the kind of number that survives
     * a wrong constant. */
    check(BrSfxEngineHz(1400.0f, 1.0f) > 10999.99
       && BrSfxEngineHz(1400.0f, 1.0f) < 11000.01,
          "1400 RPM is 11000 Hz -- 55/7 Hz per RPM");
    check(BrSfxEngineHz(2800.0f, 1.0f) > 1.999 * BrSfxEngineHz(1400.0f, 1.0f)
       && BrSfxEngineHz(2800.0f, 1.0f) < 2.001 * BrSfxEngineHz(1400.0f, 1.0f),
          "the curve is linear in RPM");

    /* The doppler is a plain multiplier on the low layer as well as being the
     * high layer's entire input. */
    check(BrSfxEngineHz(1400.0f, 2.0f) > 1.999 * BrSfxEngineHz(1400.0f, 1.0f),
          "doppler multiplies the low layer too");

    /* Magnitude fold: a negative RPM sounds like its positive twin.  The
     * original does this with two multiply constants, +0.5 and -0.5, not an
     * fabs, and the sign of zero decides which arm it takes. */
    check(BrSfxEngineHz(-1400.0f, 1.0f) == BrSfxEngineHz(1400.0f, 1.0f),
          "a negative RPM folds to its magnitude rather than silencing");
    check(BrSfxEngineHz(0.0f, 1.0f) == 0.0, "zero RPM is zero Hz");

    /* Both clamps, and the NaN that takes the same exit as a negative. */
    check(BrSfxEngineHz(1.0e9f, 1.0f) == 0.0,
          "past 100000 Hz the engine goes SILENT rather than clamping loud");
    check(BrSfxEngineHz(1400.0f, -1.0f) == 0.0,
          "a negative doppler drives it below zero and it silences");
    {
        float nan = 0.0f;
        nan = nan / nan;
        check(BrSfxEngineHz(nan, 1.0f) == 0.0,
              "a NaN RPM takes the -0.5 arm and then both clamps: silence");
        check(BrSfxEngineHz(1400.0f, nan) == 0.0, "...and so does a NaN doppler");
    }

    /* The ratio is NOT BrSfxRatioFromHz: it uses the float reciprocal of
     * 11000, while the channel it is read back through carries 11025.  The
     * two disagree by a quarter of a percent and the image says so. */
    check(BrSfxEngineRatio(11000.0) != BrSfxRatioFromHz(11000, 11025.0),
          "the engine ratio is derived against 11000, not the row's 11025");
    {
        /* Against 11000 the two DO describe the same pitch -- but not the
         * same integer.  A reciprocal multiply by a float carries only 24
         * bits, so it lands a few hundred units of 2^32 below the exact
         * divide.  That is a part in 10^7 of pitch, i.e. inaudible, and it is
         * the difference between "the constant is 1/11000" and "the constant
         * is something else entirely" -- which is what this pins. */
        int64_t a = BrSfxEngineRatio(11000.0);
        int64_t b = BrSfxRatioFromHz(11000, 11000.0);
        double  rel = (double)(b - a) / (double)b;
        printf("  reciprocal vs divide at 11000 Hz: %lld vs %lld (%.2e)\n",
               (long long)a, (long long)b, rel);
        check(rel > 0.0 && rel < 1.0e-6,
              "the float reciprocal lands just SHORT of the exact divide, by <1e-6");
    }

    hz = BrSfxEngineHz(1400.0f, 1.0f);
    f  = BrSfxHzFromRatio(BrSfxEngineRatio(hz),
                          BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE));
    printf("  1400 RPM -> %.4f Hz -> applied %u Hz on an %.0f Hz row\n",
           hz, f, BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE));
    check(f == 11024,
          "the round trip through the row's 11025 lands 24 Hz sharp, as shipped");

    /* The audible span of the sweep, from the two RPM literals the gearbox
     * writes: idle 800 and redline 8000. */
    f = BrSfxHzFromRatio(BrSfxEngineRatio(BrSfxEngineHz(800.0f, 1.0f)), 11025.0);
    check(f == 6299, "idle (800 RPM) plays the loop at 6299 Hz");
    f = BrSfxHzFromRatio(BrSfxEngineRatio(BrSfxEngineHz(8000.0f, 1.0f)), 11025.0);
    check(f == 62999, "redline (8000 RPM) plays it at 62999 Hz -- a 10:1 sweep");

    /* Monotonic across the whole sweep: an engine that dips in pitch as it
     * revs is the classic symptom of a truncation done in the wrong place. */
    ok = 1;
    prev = 0;
    for (rpm = 0.0f; rpm <= 8000.0f; rpm += 5.0f) {
        f = BrSfxHzFromRatio(BrSfxEngineRatio(BrSfxEngineHz(rpm, 1.0f)), 11025.0);
        if (f < prev) { ok = 0; break; }
        prev = f;
    }
    check(ok, "the applied frequency never falls as RPM rises");

    /* The high layer is doppler and nothing else. */
    check(BrSfxEngineHighHz(1.0f) == 22050,
          "doppler 1.0 is unity on the high layer -- its row's rate is 22050");
    check(BrSfxEngineHighHz(0.5f) == 11025 && BrSfxEngineHighHz(2.0f) == 44100,
          "and it scales linearly");
    check(BrSfxEngineHighHz(0.0f) == 0, "a zero doppler is a zero frequency");
}

/* ------------------------------------------------- the disc, when present */

/* testdata/sfx.txt is one lower-cased "sfx/name.wav" per line, produced by
 * tools/extract_assets.sh from the builder's own disc image.  Nothing is
 * committed; see README's asset policy. */
#define BR_SFX_LISTING "testdata/sfx.txt"

static char  g_disc[128][64];
static int   g_cDisc;

static int disc_load(void)
{
    char line[256];
    FILE *f = fopen(BR_SFX_LISTING, "rb");
    if (f == NULL)
        return 0;
    while (g_cDisc < 128 && fgets(line, sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0 || n >= sizeof(g_disc[0])) continue;
        memcpy(g_disc[g_cDisc++], line, n + 1);
    }
    fclose(f);
    return g_cDisc > 0;
}

static int disc_has(const char *psz)
{
    char low[64];
    int i;
    size_t n = strlen(psz), k;
    if (n >= sizeof(low)) return 0;
    for (k = 0; k < n; ++k)
        low[k] = (psz[k] >= 'A' && psz[k] <= 'Z') ? (char)(psz[k] + 32) : psz[k];
    low[n] = '\0';
    for (i = 0; i < g_cDisc; ++i)
        if (strcmp(g_disc[i], low) == 0) return 1;
    return 0;
}

static void test_against_disc(void)
{
    char buf[64];
    char seen[128][64];
    int  cSeen = 0;
    int  g, c, i, missing = 0;
    static const int aEngine[3] = {
        BR_SFX_GROUP_ENGINE, BR_SFX_GROUP_ENGINE_HIGH, BR_SFX_GROUP_ENGINE_REV
    };

    printf("against the disc\n");
    if (!disc_load()) {
        printf("  SKIP: needs %s -- run tools/extract_assets.sh "
               "(see README, 'Asset policy')\n", BR_SFX_LISTING);
        return;
    }

    /* Every name the table can produce must exist on the disc. */
    for (i = 0; i < 3; ++i)
        for (c = 1; c <= BR_SFX_CARS; ++c) {
            int k, dup = 0;
            BrSfxCarFileName(aEngine[i], c, NULL, buf, sizeof(buf));
            if (!disc_has(buf)) { printf("  absent: %s\n", buf); missing++; continue; }
            for (k = 0; k < cSeen; ++k) if (strcmp(seen[k], buf) == 0) dup = 1;
            if (!dup && cSeen < 128) strcpy(seen[cSeen++], buf);
        }
    for (g = 1; g < BrSfxGroupCount(BR_SFX_SET_RACE) - 1; ++g) {
        int k, dup = 0;
        BrSfxGroupFileName(BR_SFX_SET_RACE, g, NULL, buf, sizeof(buf));
        if (!disc_has(buf)) { printf("  absent: %s\n", buf); missing++; continue; }
        for (k = 0; k < cSeen; ++k) if (strcmp(seen[k], buf) == 0) dup = 1;
        if (!dup && cSeen < 128) strcpy(seen[cSeen++], buf);
    }
    for (g = 1; g < BrSfxGroupCount(BR_SFX_SET_MENU) - 1; ++g) {
        int k, dup = 0;
        BrSfxGroupFileName(BR_SFX_SET_MENU, g, NULL, buf, sizeof(buf));
        if (!disc_has(buf)) { printf("  absent: %s\n", buf); missing++; continue; }
        for (k = 0; k < cSeen; ++k) if (strcmp(seen[k], buf) == 0) dup = 1;
        if (!dup && cSeen < 128) strcpy(seen[cSeen++], buf);
    }
    check(missing == 0, "every name the bank can build is a real file in SFX/");

    /* ...and the table accounts for all of them.  This is the assertion that
     * would catch a missing row: 73 files, 73 reachable names. */
    printf("  disc has %d wavs; the bank reaches %d distinct names\n",
           g_cDisc, cSeen);
    check(cSeen == g_cDisc,
          "the bank reaches every .wav on the disc and invents none");
}

int main(void)
{
    test_geometry();
    test_table();
    test_filenames();
    test_pitch();
    test_engine();
    test_against_disc();

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
