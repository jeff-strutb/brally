/* test_br_save.c -- the .BRF season save format.
 *
 * WHAT THIS SUITE IS EVIDENCE OF
 *
 * There is no retail .BRF anywhere: the game writes saves into its own install
 * directory and none shipped on the disc, so there is nothing to check a
 * decoder against by inspection.  What there IS, is two independent statements
 * of the layout in the binaries -- the writer at 0x100709A0 and the reader at
 * 0x10070610 -- and one of them is already ported, faithfully, as slice4_52.c's
 * BrMenuSub100709A0.
 *
 * So this suite links THAT function, has it produce a real file out of the real
 * globals, and decodes the result.  If br_save.c's idea of the format and the
 * transcribed writer's disagree by one byte, `test_against_real_writer` says
 * so.  The round-trip on its own would only prove br_save.c self-consistent;
 * the cross-check against a separately-derived transcription is what makes it
 * mean something.
 *
 * Everything outside br_save.c, slice4_52.c and slice1_01.c is a TEST STAND-IN,
 * marked below.  None of the stand-ins is reached by any assertion here -- they
 * exist only so slice4_52.o links.  BrAdler32 is deliberately NOT stubbed: the
 * real 0x10001000 body is linked, because the checksum is part of what is
 * under test.
 *
 * The optional retail-file test SKIPs rather than fails, per the asset policy.
 * It does not use BR_REQUIRE_TESTDATA, which exits the whole suite: everything
 * else here runs without any asset, and skipping the lot because one optional
 * file is absent would hide it.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_tmpfile.h"
#include "br_save.h"
#include "slice3_39.h"   /* BrPointI, BR_DIK_COUNT -- for the stand-ins */
#include "slice1_06.h"   /* g_pszBrRallySeasonDat's declared type       */

static int g_fail;

#define CHECK(c, what)                                                        \
    do {                                                                      \
        if (!(c)) { printf("  FAIL: %s\n", (what)); g_fail++; }               \
    } while (0)

/* ==========================================================================
 * TEST STAND-INS -- so slice4_52.o links.  Never called by any assertion.
 * ========================================================================== */

void *BrHandleLookup(void *const *apTable, uint32_t handle)
{
    if (handle < BR_HANDLE_MIN || handle > BR_HANDLE_MAX) { return NULL; }
    return apTable[handle];
}

void BrTables64Clear(uint32_t *pA, uint32_t *pB, uint32_t *pC)
{
    (void)pA; (void)pB; (void)pC;
}

uint8_t   g_BrDikState[BR_DIK_COUNT];
int32_t   g_BrDikPrev [BR_DIK_COUNT];
int32_t   g_BrDikEdge [BR_DIK_COUNT];
static BrPointI g_pt2E80;
BrPointI *g_pBrAA2E80 = &g_pt2E80;

void  *g_brP680584;
int32_t g_brAA288C;

void BrSub100603A0(void *pThis, void *pArg) { (void)pThis; (void)pArg; }

uint32_t BrDPlayRandStep(uint32_t *pSeed)
{
    *pSeed = (*pSeed * 16807u) & 0x07FFFFFFu;
    return *pSeed;
}

int   BrDPlaySendTag3(const void *pLink, int32_t fGate)
{
    (void)pLink; (void)fGate; return 0;
}

void  BrErrShow(const BrErrHost *pHost, int32_t idx) { (void)pHost; (void)idx; }

void *BrOperatorNew(uint32_t cb) { return malloc(cb); }

void *BrUiCtlCtor(void *pThis) { return pThis; }

/* --- the globals slice4_52.c's writer reads ------------------------------ */
const int32_t *g_brPACED34;
int32_t g_brAA2A08, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC65C;

/* Assigned in main(), before anything under test reads it: the path carries
 * the pid so concurrent runs cannot truncate each other's file. */
static char       g_seasonPath[256];
const char *const g_pszBrRallySeasonDat = g_seasonPath;

/* ==========================================================================
 * helpers
 * ========================================================================== */

static void FillSeason(BrBrfSeason *pS)
{
    int i;

    /* Every byte distinct modulo 251 so a misplaced copy cannot alias, and
     * the block deliberately contains 0x00 and 0xFF. */
    for (i = 0; i < BR_SEASON_BLOCK_SIZE; ++i) {
        pS->aBlock[i] = (unsigned char)((i * 7 + 3) % 251);
    }
    pS->aBlock[0]                        = 0x00;
    pS->aBlock[BR_SEASON_BLOCK_SIZE - 1] = 0xFF;

    /* Values that exercise all four bytes of each dword, and the top bit. */
    pS->aOpt[0] = 0x00000001u;
    pS->aOpt[1] = 0x12345678u;
    pS->aOpt[2] = 0xFFFFFFFFu;
    pS->aOpt[3] = 0x0000001Fu;   /* the track index -- 0x100AC654, 0..0x1F */
    pS->aOpt[4] = 0x80000000u;

    for (i = 0; i < BR_SEASON_TAIL_SIZE; ++i) {
        pS->szName[i] = (char)(unsigned char)(0x80 + (i & 0x7F));
    }
    memcpy(pS->szName, "Sixth Season", 13);   /* including the terminator */
}

static int SameSeason(const BrBrfSeason *pA, const BrBrfSeason *pB)
{
    return memcmp(pA->aBlock, pB->aBlock, BR_SEASON_BLOCK_SIZE) == 0
        && memcmp(pA->aOpt,   pB->aOpt,   sizeof pA->aOpt)      == 0
        && memcmp(pA->szName, pB->szName, BR_SEASON_TAIL_SIZE)  == 0;
}

static size_t SlurpFile(const char *pszPath, unsigned char *pBuf, size_t cbBuf)
{
    FILE  *pf = fopen(pszPath, "rb");
    size_t n;

    if (pf == NULL) { return 0; }
    n = fread(pBuf, 1, cbBuf, pf);
    fclose(pf);
    return n;
}

/* ==========================================================================
 * 1. the constants compose to the file the two functions describe
 * ========================================================================== */

static void test_layout(void)
{
    puts("layout");

    /* 4 magic + 4 checksum + 0x200 payload + 5*4 options + 0x80 name. */
    CHECK(BR_BRF_FILE_SIZE == 0x29C, "  the file is 0x29C == 668 bytes");
    CHECK(BR_BRF_BLOCK_OFF == 8, "  the payload starts at +8");
    CHECK(BR_BRF_TAIL_FROM_END == 0x94,
          "  the tail is the last 0x94 bytes (0x14 options + 0x80 name)");
    CHECK(BR_SEASON_BLOCK_SIZE == 0x200 && BR_SEASON_TAIL_SIZE == 0x80,
          "  and it reuses slice4_52.h's two sizes rather than restating them");
}

/* ==========================================================================
 * 2. round trip
 * ========================================================================== */

static void test_roundtrip_image(void)
{
    unsigned char aImage[BR_BRF_FILE_SIZE];
    BrBrfSeason   in, out;
    int           n;

    puts("encode/decode round trip");

    FillSeason(&in);
    n = BrBrfEncode(aImage, sizeof aImage, &in);
    CHECK(n == BR_BRF_FILE_SIZE, "  encode fills exactly the file");

    memset(&out, 0, sizeof out);
    CHECK(BrBrfDecode(aImage, sizeof aImage, &out) == BR_BRF_OK,
          "  decode accepts it");
    CHECK(SameSeason(&in, &out), "  and every field survives");

    /* The magic is four bytes with no terminator: byte 4 is the checksum's
     * first byte, not a NUL. */
    CHECK(memcmp(aImage, "RSea", 4) == 0, "  magic is the four bytes \"RSea\"");
}

static void test_roundtrip_file(void)
{
    const char *pszPath = BrTmpPath(0, "br_save_rt");
    BrBrfSeason in, out;

    puts("file round trip");

    FillSeason(&in);
    CHECK(BrBrfWriteFile(pszPath, &in) == BR_BRF_OK, "  write succeeds");

    memset(&out, 0, sizeof out);
    CHECK(BrBrfReadFile(pszPath, &out) == BR_BRF_OK, "  read succeeds");
    CHECK(SameSeason(&in, &out), "  and every field survives the file");

    remove(pszPath);
}

/* ==========================================================================
 * 3. what the checksum does and does NOT cover
 *
 * The original checksums the 0x200 payload only, and writes the sum BEFORE
 * the data.  So a byte flipped in the payload is caught and a byte flipped
 * anywhere else is not -- which is a property of the format, not of this
 * decoder, and is exactly the sort of thing a "looks corrupt" heuristic would
 * get wrong.
 * ========================================================================== */

static void test_checksum_scope(void)
{
    unsigned char aImage[BR_BRF_FILE_SIZE];
    BrBrfSeason   in, out;

    puts("checksum scope");

    FillSeason(&in);
    (void)BrBrfEncode(aImage, sizeof aImage, &in);

    aImage[BR_BRF_BLOCK_OFF + 100] ^= 0x01;
    CHECK(BrBrfDecode(aImage, sizeof aImage, &out) == BR_BRF_ECHECKSUM,
          "  a flipped payload byte is rejected");
    aImage[BR_BRF_BLOCK_OFF + 100] ^= 0x01;

    /* An option dword is outside the checksummed range. */
    aImage[BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE + 2] ^= 0xFF;
    CHECK(BrBrfDecode(aImage, sizeof aImage, &out) == BR_BRF_OK,
          "  a flipped option byte is NOT protected");
    aImage[BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE + 2] ^= 0xFF;

    /* So is the name. */
    aImage[BR_BRF_FILE_SIZE - 1] ^= 0xFF;
    CHECK(BrBrfDecode(aImage, sizeof aImage, &out) == BR_BRF_OK,
          "  a flipped name byte is NOT protected either");
    aImage[BR_BRF_FILE_SIZE - 1] ^= 0xFF;

    aImage[0] = 'X';
    CHECK(BrBrfDecode(aImage, sizeof aImage, &out) == BR_BRF_EMAGIC,
          "  a wrong magic is rejected");
    aImage[0] = 'R';

    /* The real minimum is the three CHECKED reads: 4 + 4 + 0x200.  One byte
     * short of that and the payload read fails. */
    CHECK(BrBrfDecode(aImage, BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE - 1,
                      &out) == BR_BRF_ETRUNC,
          "  an image too short for the payload is rejected");

    /* And 0x208 is genuinely enough: with no checked read left to fail, the
     * original seeks END-0x94 and takes the tail from INSIDE the payload
     * rather than reporting anything.  Reproduced -- this is the boundary,
     * not BR_BRF_FILE_SIZE. */
    CHECK(BrBrfDecode(aImage, BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE, &out)
              == BR_BRF_OK,
          "  a payload-only image is ACCEPTED, with the tail overlapping it");

    /* The seed really is asked for rather than assumed.  adler32(0,NULL,0)
     * is 1, so 512 zero bytes give s1 = 1 and s2 = 512 -- 0x02000001.  With a
     * seed of 0 the same input would give 0. */
    memset(in.aBlock, 0, BR_SEASON_BLOCK_SIZE);
    CHECK(BrBrfChecksum(in.aBlock, BR_SEASON_BLOCK_SIZE) == 0x02000001u,
          "  an all-zero payload sums to 0x02000001, i.e. the seed is 1");
}

/* ==========================================================================
 * 4. the tail is located from the END, not sequentially
 *
 * 0x10070610 seeks to ftell()-0x94 for the option dwords and, separately, to
 * ftell()-0x80 for the name.  A sequential reader would give the same answer
 * on a well-formed file and a different one the moment there is slack after
 * the payload -- so this is the assertion that distinguishes the two, and it
 * is a boundary the original genuinely has.
 * ========================================================================== */

static void test_tail_from_end(void)
{
    const char   *pszPath = BrTmpPath(1, "br_save_slack");
    unsigned char aImage[BR_BRF_FILE_SIZE];
    unsigned char aBig[BR_BRF_FILE_SIZE + 16];
    BrBrfSeason   in, out;
    FILE         *pf;

    puts("tail located from the end");

    FillSeason(&in);
    (void)BrBrfEncode(aImage, sizeof aImage, &in);

    /* 16 bytes of slack spliced in between the payload and the tail. */
    memcpy(aBig, aImage, BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE);
    memset(aBig + BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE, 0x5A, 16);
    memcpy(aBig + BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE + 16,
           aImage + BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE,
           BR_BRF_TAIL_FROM_END);

    memset(&out, 0, sizeof out);
    CHECK(BrBrfDecode(aBig, sizeof aBig, &out) == BR_BRF_OK,
          "  an image with slack still decodes");
    CHECK(SameSeason(&in, &out),
          "  and the options and name come from the END, not from +0x208");

    pf = fopen(pszPath, "wb");
    assert(pf != NULL);
    (void)fwrite(aBig, 1, sizeof aBig, pf);
    fclose(pf);

    memset(&out, 0, sizeof out);
    CHECK(BrBrfReadFile(pszPath, &out) == BR_BRF_OK,
          "  the file reader agrees");
    CHECK(SameSeason(&in, &out), "  byte for byte");
    remove(pszPath);
}

/* ==========================================================================
 * 5. the cross-check: the already-ported writer at 0x100709A0
 * ========================================================================== */

static void test_against_real_writer(void)
{
    static int32_t block[BR_SEASON_BLOCK_SIZE / 4];
    unsigned char  aFile[BR_BRF_FILE_SIZE + 64];
    unsigned char  aMine[BR_BRF_FILE_SIZE];
    BrBrfSeason    in, out;
    size_t         n;
    int            i;

    puts("cross-check against slice4_52.c's 0x100709A0");

    FillSeason(&in);

    /* The writer reads the payload through a dword pointer; hand it the same
     * bytes the encoder was given, byte-wise, so no aliasing assumption is
     * smuggled in. */
    memcpy(block, in.aBlock, BR_SEASON_BLOCK_SIZE);
    g_brPACED34 = block;

    /* The writer's five sources, in ITS order -- which is the order the file
     * carries them: 0x10AA2A08, 0x100AC64C, 0x100AC650, 0x100AC654,
     * 0x100AC65C.  0x100AC658 sits between the last two and is not saved. */
    g_brAA2A08 = (int32_t)in.aOpt[0];
    g_br0AC64C = (int32_t)in.aOpt[1];
    g_br0AC650 = (int32_t)in.aOpt[2];
    g_br0AC654 = (int32_t)in.aOpt[3];
    g_br0AC65C = (int32_t)in.aOpt[4];

    memcpy(g_brAD0990, in.szName, BR_SEASON_TAIL_SIZE);

    remove(g_pszBrRallySeasonDat);
    BrMenuSub100709A0();

    n = SlurpFile(g_pszBrRallySeasonDat, aFile, sizeof aFile);
    remove(g_pszBrRallySeasonDat);

    CHECK(n == (size_t)BR_BRF_FILE_SIZE,
          "  the transcribed writer produces exactly 0x29C bytes");
    if (n != (size_t)BR_BRF_FILE_SIZE) { return; }

    /* The strong assertion: br_save.c's layout and the transcription of
     * 0x100709A0 agree on every byte, including the checksum. */
    (void)BrBrfEncode(aMine, sizeof aMine, &in);
    CHECK(memcmp(aMine, aFile, BR_BRF_FILE_SIZE) == 0,
          "  and it is byte-identical to BrBrfEncode's");

    memset(&out, 0, sizeof out);
    CHECK(BrBrfDecode(aFile, n, &out) == BR_BRF_OK,
          "  the decoder accepts the real writer's file");
    CHECK(SameSeason(&in, &out), "  and recovers every field it was given");

    /* The magic really does come out of the writable global, not a literal:
     * change it and the file changes with it.  (Nothing in either binary
     * writes that global, which is why the decoder may use a literal.) */
    for (i = 0; i < 4; ++i) { g_brB5D94[i] = "ZZZZ"[i]; }
    remove(g_pszBrRallySeasonDat);
    BrMenuSub100709A0();
    n = SlurpFile(g_pszBrRallySeasonDat, aFile, sizeof aFile);
    remove(g_pszBrRallySeasonDat);
    CHECK(n == (size_t)BR_BRF_FILE_SIZE && memcmp(aFile, "ZZZZ", 4) == 0,
          "  the magic is copied from 0x100B5D94, not compiled in");
    CHECK(BrBrfDecode(aFile, n, &out) == BR_BRF_EMAGIC,
          "  so the decoder rejects that file");
    for (i = 0; i < 4; ++i) { g_brB5D94[i] = "RSea"[i]; }
}

/* ==========================================================================
 * 6. what the load screen's 100-slot name list is built from
 *
 * 0x1005CF20 reads the last 0x80 bytes of every match and nothing else; it
 * never looks at the magic or the checksum.  0x1005CE30 then atoi()s the
 * digits after the prefix to pick the slot.
 * ========================================================================== */

static void test_scan(void)
{
    const char   *pszPath = BrTmpPath(2, "br_save_scan");
    unsigned char aImage[BR_BRF_FILE_SIZE];
    char          szName[BR_SEASON_TAIL_SIZE];
    char          szFile[64];
    BrBrfSeason   in;
    FILE         *pf;

    puts("the season-file scan");

    FillSeason(&in);
    (void)BrBrfEncode(aImage, sizeof aImage, &in);

    /* Corrupt the magic AND the payload: the scan must still yield the name,
     * because it reads neither. */
    aImage[0] = 'X';
    aImage[BR_BRF_BLOCK_OFF] ^= 0xFF;

    pf = fopen(pszPath, "wb");
    assert(pf != NULL);
    (void)fwrite(aImage, 1, sizeof aImage, pf);
    fclose(pf);

    memset(szName, 0, sizeof szName);
    CHECK(BrBrfReadName(pszPath, szName) == BR_BRF_OK,
          "  a name is read from a file whose magic is wrong");
    CHECK(memcmp(szName, in.szName, BR_SEASON_TAIL_SIZE) == 0,
          "  and it is the whole 0x80-byte tail");
    remove(pszPath);

    CHECK(BrBrfReadName(BrTmpPath(3, "br_save_absent"), szName) == BR_BRF_EIO
          && szName[0] == '\0',
          "  a missing file leaves the slot empty, not stale");

    /* 0x10041DF0 builds the name; 0x1005CE30 takes it apart again. */
    CHECK(BrBrfFileName(szFile, sizeof szFile, BR_BRF_PREFIX_SEASON, 12) == 17
          && strcmp(szFile, "RallySeason12.brf") == 0,
          "  the named save is \"RallySeason\" + itoa(slot) + \".brf\"");
    CHECK(BrBrfSlotIndex(szFile, BR_BRF_PREFIX_SEASON) == 12,
          "  and the scan recovers 12 from it");
    CHECK(BrBrfSlotIndex("RallySeason0.brf", BR_BRF_PREFIX_SEASON) == 0,
          "  slot 0 round-trips");
    CHECK(BrBrfSlotIndex("RallySeason99.brf", BR_BRF_PREFIX_SEASON) == 99,
          "  and so does the last slot the 100-entry list can hold");

    /* Reproduced quirks, not tidied: the prefix is not verified and a
     * non-numeric tail silently means slot 0. */
    CHECK(BrBrfSlotIndex("TimeAttack7.grf", BR_BRF_PREFIX_TIMEATTACK) == 7,
          "  the same code serves the TimeAttack list off the phase's +0xC4");
    CHECK(BrBrfSlotIndex("AutoSave.brf", BR_BRF_PREFIX_SEASON) == 0,
          "  a name that does not match the prefix silently lands on slot 0");

    CHECK(BR_BRF_SCAN_MAX == BR_NAMELIST_COUNT,
          "  the scan's 100-entry cap is the name list's 100 slots");
}

/* ==========================================================================
 * 7. optional: a real save, if anyone ever produces one
 * ========================================================================== */

static void test_retail_file(void)
{
    static const char *const pszPath = "testdata/RallySeason0.brf";
    BrBrfSeason              s;
    FILE                    *pf;
    int                      rc;

    pf = fopen(pszPath, "rb");
    if (pf == NULL) {
        printf("  SKIP retail file: needs %s -- the game writes saves into "
               "its own install directory and none shipped on the disc, so "
               "this file can only come from a real playthrough\n", pszPath);
        return;
    }
    fclose(pf);

    rc = BrBrfReadFile(pszPath, &s);
    CHECK(rc == BR_BRF_OK, "  a real save decodes");
    if (rc == BR_BRF_OK) {
        printf("  retail save name: \"%.*s\"\n",
               (int)BR_SEASON_TAIL_SIZE, s.szName);
    }
}

int main(void)
{
    snprintf(g_seasonPath, sizeof g_seasonPath, "%s",
             BrTmpPath(0, "br_save_writer"));

    test_layout();
    test_roundtrip_image();
    test_roundtrip_file();
    test_checksum_scope();
    test_tail_from_end();
    test_against_real_writer();
    test_scan();
    test_retail_file();

    printf("test_br_save: %d failures\n", g_fail);
    return g_fail != 0;
}
