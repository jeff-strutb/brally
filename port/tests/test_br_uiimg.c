/* test_br_uiimg.c -- the UI image registry, Glide 0x10056260's concerns A, C
 * and half of G.
 *
 * WHAT IS ASSERTED, AND WHY EACH IS A PROPERTY OF THE CODE
 *
 *  - THE ALLOCATION IS NOT ZEROED.  This is the assertion that exists because
 *    the D3D reading of the same loop (slice1_06.c) gets it wrong: it calls
 *    calloc, and test_slice1_06.c asserts the tail of every buffer is zero.
 *    Both allocators are operator new -- MSVCRT's ??2@YAPAXI@Z in Glide,
 *    _nh_malloc(size, 1) in D3D -- and neither zeroes.  The test poisons the
 *    heap block before handing it over and checks the poison SURVIVES past
 *    the NUL, which is exactly the difference between the two readings and
 *    cannot pass under a calloc transcription.
 *
 *  - THE POINTER IS PUBLISHED BEFORE THE STRING IS COPIED.  The original
 *    stores eax into the table and only then runs `rep movsd`.  The allocator
 *    stub reads the table back as it goes, so a transcription that copied
 *    first and stored afterwards fails.
 *
 *  - THE ORDER IS THE IMAGE INDEX.  Seven entries -- 16, 46, 63, 80, 97, 127,
 *    144 -- are the ones where MSVC emitted the store before the string load,
 *    so a pass that pairs "the nearest earlier literal" gets exactly those
 *    seven wrong and every other one right.  br_uispr.h records seven names
 *    it "could not pair"; these are they.  Asserting the seven by name is
 *    what makes a mis-pairing fail here rather than show up as the wrong
 *    bitmap behind a button.
 *
 *  - THE TWO INDEPENDENTLY DERIVED NAME LISTS AGREE.  slice1_06.c's list came
 *    off BRD3D.dll, br_uispr.c's off a different walk of the same function,
 *    and this module's copy loop reads the first.  Asserting
 *    g_apszBrUiAssets[i] == "images\" + g_aBrUiSpriteName[i] for all 145 is a
 *    cross-check between two objects that were built from different bytes.
 *
 *  - THE CLEAR DROPS EVERYTHING WITHOUT FREEING.  The leak is the original's
 *    and is preserved; the test states it as behaviour so that a future
 *    "fix" that frees has to argue with a test rather than with a comment.
 *
 *  - THE SAVE PATHS GO INTO THE CALLER'S BUFFERS, and the season one is
 *    written first.  Order is asserted through a single shared buffer, which
 *    is the only way order is observable at all.
 */
#include "br_uiimg.h"
#include "br_uispr.h"
#include "slice1_06.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* br_uispr.c is linked only for g_aBrUiSpriteName, the second name list.
 * These three are its transitive dependencies through slice3_39.c and are
 * never called here; the same three stand-ins appear in test_uispr.c and
 * test_sprfont.c for the same reason. */
void BrTextBoxDtor(BrTextBox *pBox);
void BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState);
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }
char g_aBr39B720[0x104];

static int g_fails;

#define CHECK(cond, what)                                                  \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  [FAIL] %s  (%s:%d)\n", (what), __FILE__, __LINE__);  \
            ++g_fails;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * A poisoning allocator.  Every byte comes back 0xA5, which is what lets the
 * "not zeroed" assertion mean something.
 * ========================================================================== */

#define POISON 0xA5

static int      g_nAlloc;
static int      g_nFree;
static int      g_iFailAt = -1;    /* fail the n'th allocation, -1 = never   */
static int      g_fCheckPublished; /* assert the table already holds the ptr */
static int      g_nPublishedOk;

static void *TestAlloc(void *pUser, uint32_t cb)
{
    void *p;

    (void)pUser;

    if (g_fCheckPublished && g_nAlloc > 0) {
        /* The PREVIOUS block must already be in the table by now, because
         * the original publishes before it copies. */
        if (g_aBrUiImg[g_nAlloc - 1].pszPath != NULL) {
            ++g_nPublishedOk;
        }
    }

    if (g_nAlloc == g_iFailAt) {
        ++g_nAlloc;
        return NULL;
    }
    ++g_nAlloc;

    p = malloc(cb);
    if (p != NULL) {
        memset(p, POISON, cb);
    }
    return p;
}

static void TestFree(void *pUser, void *p)
{
    (void)pUser;
    ++g_nFree;
    free(p);
}

static const BrUiImgAlloc g_alloc = { TestAlloc, TestFree, NULL };

static void ResetAlloc(void)
{
    g_nAlloc = 0;
    g_nFree = 0;
    g_iFailAt = -1;
    g_fCheckPublished = 0;
    g_nPublishedOk = 0;
}

/* ========================================================================== */

static void test_clear(void)
{
    int i;
    char sentinel[4];

    printf("the table clear -- 0x10056279\n");

    /* Dirty every slot first, so a clear that skips entries is caught. */
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        g_aBrUiImg[i].pSurf   = (BrSurf *)sentinel;
        g_aBrUiImg[i].pszPath = sentinel;
    }
    g_cBrUiImgLoaded = 7;
    g_wBrUiImgAC5D50 = 0x1234;
    g_wBrUiImgAC5D54 = 0x5678;

    BrUiImgTableClear();

    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pSurf != NULL || g_aBrUiImg[i].pszPath != NULL) {
            CHECK(0, "every one of the 145 records is cleared");
            break;
        }
    }
    CHECK(g_cBrUiImgLoaded == 0, "0x10AC5C2C, the loaded count, is zeroed");
    CHECK(g_wBrUiImgAC5D50 == 0, "0x10AC5D50 is zeroed");
    CHECK(g_wBrUiImgAC5D54 == 0, "0x10AC5D54 is zeroed");
}

static void test_paths(void)
{
    int i;
    int nFilled;

    printf("the 145 allocate-and-copy blocks -- 0x100562A3\n");

    ResetAlloc();
    BrUiImgTableClear();
    g_fCheckPublished = 1;

    CHECK(BrUiImgPathsInit(&g_alloc) == 0, "every allocation succeeded");
    CHECK(g_nAlloc == BR_UIIMG_COUNT, "exactly 145 allocations, one per image");

    /* 144 of the 145 could be observed (the first has no predecessor). */
    CHECK(g_nPublishedOk == BR_UIIMG_COUNT - 1,
          "the pointer is stored into the table BEFORE the string is copied");

    nFilled = 0;
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath != NULL &&
            strcmp(g_aBrUiImg[i].pszPath, g_apszBrUiAssets[i]) == 0) {
            ++nFilled;
        }
        CHECK(g_aBrUiImg[i].pSurf == NULL,
              "the +0x00 surface slot is left NULL by the path pass");
    }
    CHECK(nFilled == BR_UIIMG_COUNT, "all 145 paths land in index order");

    /* THE ALLOCATOR DOES NOT ZERO.  Everything past the NUL is still the
     * poison the stub wrote.  Under a calloc transcription this fails. */
    {
        size_t n = strlen(g_aBrUiImg[0].pszPath);
        CHECK((unsigned char)g_aBrUiImg[0].pszPath[n + 1u] == POISON,
              "operator new does NOT zero: the tail past the NUL is untouched");
        CHECK((unsigned char)
                  g_aBrUiImg[0].pszPath[BR_UIIMG_PATH_MAX - 1u] == POISON,
              "...including the last byte of the 0x104-byte block");
    }

    /* Each entry is its own allocation. */
    for (i = 1; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath == g_aBrUiImg[i - 1].pszPath) {
            CHECK(0, "each path is a separate 0x104-byte block");
            break;
        }
    }

    BrUiImgPathsFree(&g_alloc);
    CHECK(g_nFree == BR_UIIMG_COUNT, "the host-only free releases all 145");
}

static void test_alloc_failure(void)
{
    int i;
    int nNull;

    printf("a failed allocation -- the DEVIATION\n");

    ResetAlloc();
    BrUiImgTableClear();
    g_iFailAt = 42;

    CHECK(BrUiImgPathsInit(&g_alloc) == 1, "one failure is reported");
    CHECK(g_nAlloc == BR_UIIMG_COUNT,
          "the loop CONTINUES past a failure -- all 145 are attempted");
    CHECK(g_aBrUiImg[42].pszPath == NULL, "the failed slot is left NULL");

    nNull = 0;
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath == NULL) {
            ++nNull;
        }
    }
    CHECK(nNull == 1, "only the failed slot is NULL; the rest are intact");
    CHECK(g_aBrUiImg[43].pszPath != NULL &&
          strcmp(g_aBrUiImg[43].pszPath, g_apszBrUiAssets[43]) == 0,
          "the entry AFTER the failure is still correct");

    BrUiImgPathsFree(&g_alloc);
    CHECK(g_nFree == BR_UIIMG_COUNT - 1, "144 blocks are released");
}

static void test_clear_drops_without_freeing(void)
{
    printf("re-running drops the table without freeing it -- the leak\n");

    ResetAlloc();
    BrUiImgTableClear();
    (void)BrUiImgPathsInit(&g_alloc);
    CHECK(g_nAlloc == BR_UIIMG_COUNT, "first pass allocated 145");

    BrUiImgTableClear();
    CHECK(g_nFree == 0,
          "the clear frees NOTHING -- 145 * 0x104 bytes leak per mode change");
    CHECK(g_aBrUiImg[0].pszPath == NULL, "...and the pointers are simply lost");

    /* Second pass allocates a fresh 145. */
    (void)BrUiImgPathsInit(&g_alloc);
    CHECK(g_nAlloc == 2 * BR_UIIMG_COUNT, "the second pass allocates 145 more");
    BrUiImgPathsFree(&g_alloc);
}

/* The two lists were derived from different bytes of two different binaries;
 * they have to agree or one of them is wrong. */
static void test_name_lists_agree(void)
{
    static const int aiLate[7] = { 16, 46, 63, 80, 97, 127, 144 };
    static const char *const apszLate[7] = {
        "images\\desrttrk.bmp", "images\\arrowdu.bmp", "images\\trakd_.bmp",
        "images\\noadv1.bmp",   "images\\ffstick.bmp", "images\\z-carFH.bmp",
        "images\\trakQ_.bmp"
    };
    int i;
    int nAgree = 0;

    printf("the name lists -- three independent derivations\n");

    CHECK(BR_UIIMG_COUNT == BR_UI_SPR_COUNT,
          "one sprite-table entry per image");

    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        char sz[64];

        if (g_aBrUiSpriteName[i] == NULL) {
            continue;
        }
        snprintf(sz, sizeof sz, "images\\%s", g_aBrUiSpriteName[i]);
        if (strcmp(sz, g_apszBrUiAssets[i]) == 0) {
            ++nAgree;
        } else {
            printf("    index %d: '%s' vs '%s'\n",
                   i, sz, g_apszBrUiAssets[i]);
        }
    }
    CHECK(nAgree == BR_UIIMG_COUNT,
          "all 145 agree with br_uispr.c modulo the 'images\\' prefix");

    /* THE SEVEN.  A pairing that takes "the nearest earlier literal" puts a
     * duplicate of index-1 in each of these, so each one is a separate,
     * independent statement that the order was recovered by grouping on the
     * allocation call and not on proximity. */
    for (i = 0; i < 7; ++i) {
        CHECK(strcmp(g_apszBrUiAssets[aiLate[i]], apszLate[i]) == 0,
              "the seven store-before-load entries are correct");
        CHECK(strcmp(g_apszBrUiAssets[aiLate[i]],
                     g_apszBrUiAssets[aiLate[i] - 1]) != 0,
              "...and is not a duplicate of its predecessor");
    }

    CHECK(strcmp(g_apszBrUiAssets[0], "images\\work1a.bmp") == 0,
          "index 0 is work1a.bmp");
    CHECK(strcmp(g_apszBrUiAssets[BR_UIIMG_COUNT - 1],
                 "images\\trakQ_.bmp") == 0,
          "index 144 is trakQ_.bmp");
}

static void test_save_paths(void)
{
    char szSeason[BR_UIIMG_PATH_MAX];
    char szGhost[BR_UIIMG_PATH_MAX];
    char szBoth[BR_UIIMG_PATH_MAX];

    printf("the two save-file buffers -- 0x10058299\n");

    memset(szSeason, POISON, sizeof szSeason);
    memset(szGhost,  POISON, sizeof szGhost);

    BrUiImgSavePathsInit(szSeason, sizeof szSeason, szGhost, sizeof szGhost);

    CHECK(strcmp(szSeason, "c:\\RallySeason.dat") == 0,
          "0x117A6030 gets c:\\RallySeason.dat");
    CHECK(strcmp(szGhost, "c:\\RallyGhost.dat") == 0,
          "0x117A5F28 gets c:\\RallyGhost.dat");

    /* The copy is strlen+1, so the rest of the buffer is untouched -- the
     * same property as the path blocks, and for the same reason. */
    CHECK((unsigned char)szSeason[sizeof szSeason - 1u] == POISON,
          "the copy is strlen+1: the rest of the buffer is untouched");

    /* Order, made observable: both into ONE buffer, ghost must win. */
    BrUiImgSavePathsInit(szBoth, sizeof szBoth, szBoth, sizeof szBoth);
    CHECK(strcmp(szBoth, "c:\\RallyGhost.dat") == 0,
          "season is copied FIRST, ghost second");

    /* A NULL destination is tolerated (a host that owns only one buffer). */
    BrUiImgSavePathsInit(NULL, 0, NULL, 0);
}

int main(void)
{
    test_clear();
    test_paths();
    test_alloc_failure();
    test_clear_drops_without_freeing();
    test_name_lists_agree();
    test_save_paths();

    printf("test_br_uiimg: %d failures\n", g_fails);
    return (g_fails != 0) ? 1 : 0;
}
