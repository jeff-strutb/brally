/* test_br_volume.c -- the host volume service, both directions.
 *
 * THE POINT OF THIS SUITE is that the service is not a stub wearing a
 * different name. The scan it replaces was stubbed to 0 and refused
 * Championship on every machine; the failure mode of "fixing" that is to make
 * it answer 1 on every machine instead, which is the same lie with the sign
 * flipped. So every test here comes in a pair: one tree where the assets are
 * present and the answer must be yes, and one where they are not and the
 * answer must be no.
 *
 * The fixtures are built on disk rather than injected, because the thing under
 * test IS the reading of a real manifest beside real files.
 */
#include "br_volume.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_cFail;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_cFail++;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * Fixtures
 * ========================================================================== */

static char g_szDir[512];

static void fixture_open(void)
{
    char szTmpl[512];

    snprintf(szTmpl, sizeof szTmpl, "%s/br_volume_test_XXXXXX",
             getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(szTmpl) == NULL) {
        printf("FAIL: cannot create a temporary directory\n");
        exit(2);
    }
    snprintf(g_szDir, sizeof g_szDir, "%s", szTmpl);
    BrVolumeSetRoot(g_szDir);
}

static void write_file(const char *pszRel, const char *pszText)
{
    char  szPath[1024];
    FILE *fh;

    snprintf(szPath, sizeof szPath, "%s/%s", g_szDir, pszRel);
    fh = fopen(szPath, "wb");
    if (fh == NULL) {
        printf("FAIL: cannot write %s\n", szPath);
        exit(2);
    }
    fputs(pszText, fh);
    fclose(fh);
}

static void unlink_rel(const char *pszRel)
{
    char szPath[1024];
    snprintf(szPath, sizeof szPath, "%s/%s", g_szDir, pszRel);
    (void)remove(szPath);
}

/* A manifest of the shape tools/extract_iso.py --manifest writes, with the
 * label under test. The extra keys are present because the real one has them
 * and the reader must not be confused by them -- volume_label_joliet in
 * particular is a longer key with volume_label as a prefix. */
static void write_manifest(const char *pszLabel)
{
    char szJson[1024];

    snprintf(szJson, sizeof szJson,
             "{\n"
             "  \"asset_root\": \"testdata\",\n"
             "  \"files\": [\n"
             "    \"cars/ce.rca\",\n"
             "    \"tracks/race.trk\"\n"
             "  ],\n"
             "  \"fingerprint_covers\": \"image size + volume descriptor block\",\n"
             "  \"image\": \"BossRally.BIN\",\n"
             "  \"source_fingerprint\": \"09c564fe44c255201c4d6b8cdf6ad73c\",\n"
             "  \"volume_label\": \"%s\",\n"
             "  \"volume_label_joliet\": \"Boss Rally\"\n"
             "}\n", pszLabel);
    write_file(BR_VOLUME_MANIFEST, szJson);
}

static void mkassets(void)
{
    char szPath[1024];
    snprintf(szPath, sizeof szPath, "%s/cars", g_szDir);
    (void)mkdir(szPath, 0777);
    write_file("cars/ce.rca", "RCar");
}

static void rmassets(void)
{
    char szPath[1024];
    unlink_rel("cars/ce.rca");
    snprintf(szPath, sizeof szPath, "%s/cars", g_szDir);
    (void)rmdir(szPath);
}

/* ==========================================================================
 * The two directions
 * ========================================================================== */

/* ASSETS PRESENT: the recorded label is the disc's, so the game's own test
 * passes and Championship's guard opens. */
static void test_assets_present_says_yes(void)
{
    fixture_open();
    mkassets();
    write_manifest(BR_VOLUME_WANT);

    CHECK(BrVolumeCount() == 1);
    CHECK(BrVolumeLabel(0) != NULL);
    CHECK(BrVolumeLabel(0) != NULL &&
          strcmp(BrVolumeLabel(0), "Boss Rally") == 0);
    CHECK(BrVolumePresent(BR_VOLUME_WANT) == 1);
    CHECK(BrExt_10045A00() != 0);
}

/* NOTHING EXTRACTED: no manifest, so no volume, so no. This is the regression
 * test that the service is not a stub in disguise -- it must be possible for
 * it to say no, and an empty tree is when it must. */
static void test_nothing_extracted_says_no(void)
{
    fixture_open();                     /* a fresh, empty directory */

    CHECK(BrVolumeCount() == 0);
    CHECK(BrVolumeLabel(0) == NULL);
    CHECK(BrVolumePresent(BR_VOLUME_WANT) == 0);
    CHECK(BrExt_10045A00() == 0);
}

/* A MANIFEST ALONE VOUCHES FOR NOTHING. The file it names is not there, so
 * there is no volume -- the br_wireaudio.c lesson, applied here. */
static void test_manifest_without_assets_says_no(void)
{
    fixture_open();
    write_manifest(BR_VOLUME_WANT);     /* no cars/ce.rca beside it */

    CHECK(BrVolumeCount() == 0);
    CHECK(BrExt_10045A00() == 0);

    /* ...and the same manifest with the assets restored does say yes, so the
     * refusal above is about the assets and not about the manifest. */
    mkassets();
    CHECK(BrVolumeCount() == 1);
    CHECK(BrExt_10045A00() != 0);

    /* ...and taking them away again takes the volume away again, which no
     * cached answer would allow. */
    rmassets();
    CHECK(BrVolumeCount() == 0);
    CHECK(BrExt_10045A00() == 0);
}

/* THE COMPARISON IS THE GAME'S: strcmp, case-sensitive, whole string.
 * 0x10037823..0x10037852 compares bytes and stops at the NUL, so none of these
 * near-misses may pass. Each one is a relaxation somebody could plausibly
 * write: case-insensitive, prefix, suffix, substring, trimmed. */
static void test_comparison_is_exact(void)
{
    static const char *const apszNear[] = {
        "BOSS RALLY", "boss rally", "Boss rally", "boss Rally",
        "Boss Rally 2", "Boss", "Rally", "BossRally", "Boss  Rally",
        " Boss Rally", "Boss Rally ", "Boss Rall", ""
    };
    size_t i;

    for (i = 0; i < sizeof apszNear / sizeof apszNear[0]; i++) {
        fixture_open();
        mkassets();
        write_manifest(apszNear[i]);
        if (BrExt_10045A00() != 0)
            printf("FAIL: label %-14s was accepted as \"Boss Rally\"\n",
                   apszNear[i]);
        CHECK(BrExt_10045A00() == 0);
        CHECK(BrVolumePresent(BR_VOLUME_WANT) == 0);
        /* the volume is still THERE -- it just is not the one being asked for */
        CHECK(BrVolumeCount() == 1);
        CHECK(BrVolumePresent(apszNear[i]) == 1);
    }
}

/* THE LONGER KEY MUST NOT BE READ AS THE SHORTER ONE. "volume_label_joliet"
 * has "volume_label" as a prefix, and the manifest always carries both. If the
 * reader matched on the prefix it would read whichever came first in the file
 * -- and json.dump sorts keys, so volume_label does come first, which would
 * hide the bug on the real manifest. This fixture puts them the other way
 * round so the bug cannot hide. */
static void test_key_is_not_a_prefix_match(void)
{
    fixture_open();
    mkassets();
    write_file(BR_VOLUME_MANIFEST,
               "{\n"
               "  \"volume_label_joliet\": \"Not The Label\",\n"
               "  \"volume_label\": \"Boss Rally\",\n"
               "  \"files\": [\"cars/ce.rca\"]\n"
               "}\n");

    CHECK(BrVolumeLabel(0) != NULL &&
          strcmp(BrVolumeLabel(0), "Boss Rally") == 0);
    CHECK(BrExt_10045A00() != 0);
}

/* A LABEL LONGER THAN THE ORIGINAL'S BUFFER IS TRUNCATED, and a truncated
 * label does not compare equal. 0x10037802 pushes 0x104 as the buffer size. */
static void test_overlong_label_truncates_and_refuses(void)
{
    char szJson[BR_VOLUME_LABEL_MAX * 2 + 256];
    char szLabel[BR_VOLUME_LABEL_MAX * 2];
    size_t i;

    fixture_open();
    mkassets();
    for (i = 0; i < sizeof szLabel - 1; i++)
        szLabel[i] = 'A';
    szLabel[sizeof szLabel - 1] = '\0';
    snprintf(szJson, sizeof szJson,
             "{\"volume_label\": \"%s\", \"files\": [\"cars/ce.rca\"]}\n",
             szLabel);
    write_file(BR_VOLUME_MANIFEST, szJson);

    CHECK(BrVolumeCount() == 1);
    CHECK(BrVolumeLabel(0) != NULL &&
          strlen(BrVolumeLabel(0)) == BR_VOLUME_LABEL_MAX - 1);
    CHECK(BrExt_10045A00() == 0);
}

/* A MANIFEST WITH NO LABEL IS NOT A VOLUME. Provenance without the field the
 * game tests is not an answer to the game's question. */
static void test_manifest_without_label_says_no(void)
{
    fixture_open();
    mkassets();
    write_file(BR_VOLUME_MANIFEST,
               "{\"image\": \"BossRally.BIN\", \"files\": [\"cars/ce.rca\"]}\n");

    CHECK(BrVolumeCount() == 0);
    CHECK(BrExt_10045A00() == 0);
}

/* Malformed JSON refuses rather than guessing. */
static void test_malformed_manifest_says_no(void)
{
    fixture_open();
    mkassets();
    write_file(BR_VOLUME_MANIFEST, "{\"volume_label\": \"Boss Rally");
    CHECK(BrVolumeCount() == 0);
    CHECK(BrExt_10045A00() == 0);

    fixture_open();
    mkassets();
    write_file(BR_VOLUME_MANIFEST, "not json at all\n");
    CHECK(BrVolumeCount() == 0);
    CHECK(BrExt_10045A00() == 0);
}

/* BrVolumePresent(NULL) is not a match, and does not crash. */
static void test_null_label(void)
{
    fixture_open();
    mkassets();
    write_manifest(BR_VOLUME_WANT);
    CHECK(BrVolumePresent(NULL) == 0);
}

/* ==========================================================================
 * The REAL extracted tree, when the builder has one
 * ==========================================================================
 *
 * Everything above runs on fixtures, which proves the mechanism and nothing
 * about this machine. This one asks the question the project actually cares
 * about -- does THIS tree's testdata/ answer yes -- and SKIPS rather than
 * passes when there is no extraction here, because a machine with no disc must
 * not report a green that a machine with a disc would have earned.
 */
static int test_real_tree(void)
{
    char  szPath[1024];
    FILE *fh;
    int   fManifest, fHave;

    BrVolumeSetRoot(BR_VOLUME_ROOT_DEFAULT);

    /* THE SKIP IS ON THE MANIFEST, NOT ON THE ANSWER, and that distinction is
     * what makes this test able to fail. Skipping whenever no volume is found
     * would excuse every possible defect: an extraction that recorded the
     * wrong label, or recorded no file inventory at all, would report "no
     * volume" and be waved through as "this machine has no disc".
     *
     * So: a machine with no manifest has nothing extracted and is skipped. A
     * machine WITH a manifest has had a successful extraction -- extract_assets.sh
     * writes it last, from the tree it has just filled -- and that manifest is
     * then required to produce a volume labelled "Boss Rally". */
    snprintf(szPath, sizeof szPath, "%s/%s",
             BR_VOLUME_ROOT_DEFAULT, BR_VOLUME_MANIFEST);
    fh = fopen(szPath, "rb");
    fManifest = (fh != NULL);
    if (fh != NULL)
        fclose(fh);

    /* Scan BEFORE reporting: BrVolumeWhy() describes the LAST scan, and the
     * fixtures above have each left one behind. Printing it without scanning
     * the real root first attributes a fixture's reason to this machine. */
    fHave = (BrVolumeCount() == 1);

    if (!fManifest) {
        printf("  real tree: nothing extracted here -- %s\n", BrVolumeWhy());
        CHECK(!fHave);          /* no manifest must mean no volume */
        return 0;
    }
    printf("  real tree: %s -- %s\n",
           fHave ? BrVolumeLabel(0) : "NO VOLUME", BrVolumeWhy());
    CHECK(fHave);
    CHECK(fHave && strcmp(BrVolumeLabel(0), BR_VOLUME_WANT) == 0);
    CHECK(BrExt_10045A00() != 0);
    return 1;
}

int main(void)
{
    int fReal;

    test_assets_present_says_yes();
    test_nothing_extracted_says_no();
    test_manifest_without_assets_says_no();
    test_comparison_is_exact();
    test_key_is_not_a_prefix_match();
    test_overlong_label_truncates_and_refuses();
    test_manifest_without_label_says_no();
    test_malformed_manifest_says_no();
    test_null_label();
    fReal = test_real_tree();

    if (g_cFail != 0) {
        printf("%d failure(s)\n", g_cFail);
        return 1;
    }
    if (!fReal)
        printf("test_br_volume: all tests passed "
               "(the real-tree check found no extraction and was skipped)\n");
    else
        printf("test_br_volume: all tests passed\n");
    return 0;
}
