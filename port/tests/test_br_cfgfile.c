/* test_br_cfgfile.c -- Glide 0x10063060, the BossRally.cfg reader.
 *
 * WHAT IS PINNED, AND WHY EACH ONE CAN FAIL
 *
 * The offset table and the endianness are checked against an image this file
 * BUILDS BYTE BY BYTE from its own hard-coded offsets -- not against
 * BrCtrlCfgFileEncode's output.  A round trip alone would pass under any
 * mutation applied symmetrically to both halves (a swapped pair of fields, a
 * big-endian codec), and that is exactly the class of dead test this tree has
 * found four of.  The round trip is here too, but only as a second check.
 *
 * The two preserved bugs get a test each, written so that FIXING the bug
 * fails the suite:
 *   - a rejected file resets the object to defaults, because BrCtrlCfgInit
 *     runs after the restore and overwrites it;
 *   - a loaded `active` does not update pActive.
 *
 * No loop here has its termination under test: every one is a fixed count
 * over a constant table, so a mutation cannot turn a failure into a hang.
 */
#include "br_cfgfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_tmpfile.h"

static int g_fails;

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ======================================================================
 * Stand-ins for everything slice3_42.o refers to outside itself.
 * CLEARLY MARKED: none of these is decompiled code, none is called by any
 * test below, and they exist only so the object links.  test_slice3_42.c
 * carries the same set for the same reason.
 * ====================================================================== */

void BrMat4MulVec3(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{ (void)pM; *pOut = *pV; }

void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{ (void)pM; *pOut = *pV; }

void BrMat3MulVec3(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{ (void)pM; *pOut = *pV; }

void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc)
{ (void)pDst; (void)pSrc; }

void BrCarStateUnpack(BrCarState *pDst, const BrCarPacked *pSrc)
{ (void)pDst; (void)pSrc; }

void BrCarRecordToState(BrCarState *pDst, void *pCar)
{ (void)pDst; (void)pCar; }

void BrCarRecordFromState(void *pCar, const BrCarState *pSrc)
{ (void)pCar; (void)pSrc; }

/* ======================================================================
 * Helpers
 * ====================================================================== */

/* Compare every field of the object EXCEPT pActive.  Field-wise rather than
 * memcmp: on LP64 there are four bytes of padding before the pointer, and a
 * memcmp would compare them. */
static int cfg_eq(const BrCtrlCfg *a, const BrCtrlCfg *b)
{
    int i, j, k;

    for (i = 0; i < BR_CTRL_PROFILES; ++i)
        for (j = 0; j < BR_CTRL_ACTIONS; ++j)
            for (k = 0; k < 3; ++k)
                if (a->profile[i].e[j][k] != b->profile[i].e[j][k])
                    return 0;

    if (a->active != b->active) return 0;
    if (a->f2A8 != b->f2A8 || a->f2AC != b->f2AC || a->f2B0 != b->f2B0)
        return 0;
    if (memcmp(a->f2B4, b->f2B4, sizeof a->f2B4) != 0) return 0;
    if (memcmp(a->f3B8, b->f3B8, sizeof a->f3B8) != 0) return 0;
    if (a->f7B8 != b->f7B8 || a->f7BC != b->f7BC) return 0;
    if (a->f7C0 != b->f7C0 || a->f7C4 != b->f7C4) return 0;
    if (memcmp(a->f7C8, b->f7C8, sizeof a->f7C8) != 0) return 0;
    if (a->f7D8 != b->f7D8 || a->f7DC != b->f7DC) return 0;
    if (a->f7E0 != b->f7E0 || a->f7E4 != b->f7E4 || a->f7E8 != b->f7E8)
        return 0;
    if (a->f7EC != b->f7EC || a->f7F0 != b->f7F0 || a->f7F4 != b->f7F4)
        return 0;
    if (a->f7F8 != b->f7F8 || a->f7FC != b->f7FC) return 0;
    if (a->f800 != b->f800 || a->f804 != b->f804) return 0;
    if (a->f808 != b->f808 || a->f80C != b->f80C) return 0;
    if (memcmp(a->f810, b->f810, sizeof a->f810) != 0) return 0;
    if (memcmp(a->f830, b->f830, sizeof a->f830) != 0) return 0;
    if (a->f870 != b->f870) return 0;
    return 1;
}

/* The body pattern.  Index is the offset WITHIN THE BODY, i.e. file offset
 * minus the 8-byte header. */
static unsigned char pat(size_t i)
{
    return (unsigned char)((i * 31u + 7u) & 0xFFu);
}

/* BR_CTRLCFG_HEADER_SIZE spelled as a literal 8 on purpose: this file must
 * not inherit the module's arithmetic. */
#define GOLD_HDR 8

static uint32_t exp32(size_t fileOff)
{
    size_t b = fileOff - GOLD_HDR;
    return (uint32_t)pat(b)
         | ((uint32_t)pat(b + 1) << 8)
         | ((uint32_t)pat(b + 2) << 16)
         | ((uint32_t)pat(b + 3) << 24);
}

static uint16_t exp16(size_t fileOff)
{
    size_t b = fileOff - GOLD_HDR;
    return (uint16_t)((uint32_t)pat(b) | ((uint32_t)pat(b + 1) << 8));
}

/* Build the golden image: header, then 0x870 pattern bytes.  0x878 is
 * written out rather than taken from the header. */
#define GOLD_SIZE 0x878

static void gold_build(unsigned char *p)
{
    size_t i;

    memcpy(p, "RCfg", 4);
    p[4] = 0x02; p[5] = 0x00; p[6] = 0x00; p[7] = 0x00;
    for (i = 0; i < (size_t)GOLD_SIZE - GOLD_HDR; ++i)
        p[GOLD_HDR + i] = pat(i);
}

static int write_file(const char *pszPath, const unsigned char *p, size_t cb)
{
    FILE *pf = fopen(pszPath, "wb");
    size_t n;

    if (pf == NULL)
        return 0;
    n = (cb == 0) ? 0 : fwrite(p, 1, cb, pf);
    fclose(pf);
    return n == cb;
}

/* ======================================================================
 * 1. The offset table and the endianness, against a hand-built image.
 * ====================================================================== */
static void test_layout(void)
{
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_gold");
    BrCtrlCfg     cfg;

    gold_build(aGold);
    CHECK(write_file(szPath, aGold, sizeof aGold));

    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 1);

    /* Every scalar, at the offset the binary puts it at. */
    CHECK((uint32_t)cfg.f2A8 == exp32(0x008));
    CHECK((uint32_t)cfg.f2AC == exp32(0x00C));
    CHECK((uint32_t)cfg.f2B0 == exp32(0x010));

    /* First and last element of each block: pins both base and length. */
    CHECK(cfg.f2B4[0]    == exp32(0x014));
    CHECK(cfg.f2B4[0x40] == exp32(0x114));      /* 0x41 dwords -> 0x104 */
    CHECK(cfg.f3B8[0]    == exp32(0x118));
    CHECK(cfg.f3B8[0xFF] == exp32(0x514));      /* 0x100 dwords -> 0x400 */

    CHECK((uint32_t)cfg.f7B8 == exp32(0x518));
    CHECK((uint32_t)cfg.f7BC == exp32(0x51C));
    CHECK((uint32_t)cfg.f7C0 == exp32(0x520));
    CHECK((uint32_t)cfg.f7C4 == exp32(0x524));

    CHECK(cfg.f7C8[0] == exp32(0x528));
    CHECK(cfg.f7C8[3] == exp32(0x534));

    CHECK((uint32_t)cfg.f7D8 == exp32(0x538));
    CHECK((uint32_t)cfg.f7DC == exp32(0x53C));
    CHECK((uint32_t)cfg.f7E0 == exp32(0x540));
    CHECK((uint32_t)cfg.f7E4 == exp32(0x544));
    CHECK((uint32_t)cfg.f7E8 == exp32(0x548));
    CHECK((uint32_t)cfg.f7EC == exp32(0x54C));
    CHECK((uint32_t)cfg.f7F0 == exp32(0x550));
    CHECK((uint32_t)cfg.f7F4 == exp32(0x554));
    CHECK((uint32_t)cfg.f7F8 == exp32(0x558));
    CHECK((uint32_t)cfg.f7FC == exp32(0x55C));
    CHECK((uint32_t)cfg.f800 == exp32(0x560));
    CHECK((uint32_t)cfg.f804 == exp32(0x564));
    CHECK((uint32_t)cfg.f808 == exp32(0x568));
    CHECK((uint32_t)cfg.f80C == exp32(0x56C));

    CHECK(cfg.f810[0] == exp32(0x570));
    CHECK(cfg.f810[7] == exp32(0x58C));
    CHECK(cfg.f830[0]  == exp32(0x590));
    CHECK(cfg.f830[15] == exp32(0x5CC));

    CHECK((uint32_t)cfg.f870 == exp32(0x5D0));

    /* The ordering oddity: `active` sits between the high fields and the
     * four profile blocks that cover the LOW part of the object. */
    CHECK((uint32_t)cfg.active == exp32(0x5D4));

    CHECK(cfg.profile[0].e[0][0]   == exp16(0x5D8));
    CHECK(cfg.profile[0].e[27][2]  == exp16(0x5D8 + 83 * 2));
    CHECK(cfg.profile[1].e[0][0]   == exp16(0x680));
    CHECK(cfg.profile[2].e[0][0]   == exp16(0x728));
    CHECK(cfg.profile[3].e[0][0]   == exp16(0x7D0));
    CHECK(cfg.profile[3].e[27][2]  == exp16(0x7D0 + 83 * 2));

    remove(szPath);
}

/* ======================================================================
 * 2. The header checks.
 * ====================================================================== */
static void test_magic(void)
{
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_magic");
    BrCtrlCfg     cfg, ref;

    BrCtrlCfgInit(&ref);

    /* One wrong byte in each of the four positions. */
    {
        static const int aPos[4] = { 0, 1, 2, 3 };
        int i;
        for (i = 0; i < 4; ++i) {
            gold_build(aGold);
            aGold[aPos[i]] = (unsigned char)(aGold[aPos[i]] ^ 0x20u);
            CHECK(write_file(szPath, aGold, sizeof aGold));
            BrCtrlCfgInit(&cfg);
            cfg.f870 = 0x5A5A;                 /* a marker Init does not set */
            CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
            CHECK(cfg_eq(&cfg, &ref));         /* reset, marker gone         */
        }
    }

    /* A NUL in the middle: strncmp and memcmp must agree, and both reject. */
    gold_build(aGold);
    aGold[2] = 0x00;
    CHECK(write_file(szPath, aGold, sizeof aGold));
    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);

    /* A fifth byte is NOT part of the magic: the file carries no terminator
     * and the reader never looks at offset 4 as a character. */
    gold_build(aGold);
    CHECK(write_file(szPath, aGold, sizeof aGold));
    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 1);

    remove(szPath);
}

static void test_version(void)
{
    static const unsigned char aBad[5] = { 0x00, 0x01, 0x03, 0x04, 0xFF };
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_ver");
    BrCtrlCfg     cfg;
    int           i;

    for (i = 0; i < 5; ++i) {
        gold_build(aGold);
        aGold[4] = aBad[i];
        CHECK(write_file(szPath, aGold, sizeof aGold));
        BrCtrlCfgInit(&cfg);
        CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
    }

    /* The version is the whole DWORD, not just its low byte: 0x00000102 is
     * not 2.  This is what stops the check being a byte compare. */
    gold_build(aGold);
    aGold[5] = 0x01;
    CHECK(write_file(szPath, aGold, sizeof aGold));
    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);

    remove(szPath);
}

/* ======================================================================
 * 3. All-or-nothing fields, and the failure arm's reset.
 * ====================================================================== */
static void test_truncation(void)
{
    /* Every field boundary that matters, plus one byte short of the end. */
    static const size_t aLen[] = {
        0, 1, 3, 4, 7, 8, 9, 0x00B, 0x013, 0x117, 0x517,
        0x527, 0x58F, 0x5CF, 0x5D3, 0x5D7, 0x67F, 0x877
    };
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_trunc");
    BrCtrlCfg     cfg, ref;
    size_t        i;

    BrCtrlCfgInit(&ref);
    gold_build(aGold);

    for (i = 0; i < sizeof aLen / sizeof aLen[0]; ++i) {
        CHECK(write_file(szPath, aGold, aLen[i]));
        BrCtrlCfgInit(&cfg);
        cfg.f870   = 0x1234;
        cfg.active = 3;
        cfg.profile[0].e[0][0] = 0xBEEF;
        CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
        /* BUG 1: not restored to the markers -- reset to defaults. */
        CHECK(cfg_eq(&cfg, &ref));
    }

    /* And the exact length succeeds, so the list above is testing
     * truncation and not something that always fails. */
    CHECK(write_file(szPath, aGold, sizeof aGold));
    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 1);

    remove(szPath);
}

/* BUG 1, stated on its own: the failure arm copies the old settings back and
 * then BrCtrlCfgInit overwrites them.  If the dead restore is ever made live
 * -- by dropping the Init -- this fails. */
static void test_failure_resets_to_defaults(void)
{
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_reset");
    BrCtrlCfg     cfg, ref, saved;

    BrCtrlCfgInit(&ref);

    /* Start from settings that are NOT the defaults in several places. */
    BrCtrlCfgInit(&cfg);
    cfg.f7B8 = 1024;
    cfg.f7BC = 768;
    cfg.f7E0 = 7;
    cfg.active = 2;
    cfg.profile[1].e[3][0] = 0x0141;
    cfg.f3B8[9] = 0xDEADBEEFu;
    saved = cfg;

    CHECK(!cfg_eq(&saved, &ref));       /* the fixture really does differ */

    gold_build(aGold);
    aGold[4] = 0x03;                    /* a version the reader rejects   */
    CHECK(write_file(szPath, aGold, sizeof aGold));

    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
    CHECK(cfg_eq(&cfg, &ref));          /* defaults ... */
    CHECK(!cfg_eq(&cfg, &saved));       /* ... not the settings it saved */

    remove(szPath);
}

/* A missing file is the ONE failure that does not touch the object: the
 * original returns before the temporary is even constructed. */
static void test_missing_file(void)
{
    const char *szPath = BrTmpPath(1, "br_cfgfile_absent");
    BrCtrlCfg   cfg, saved;

    remove(szPath);

    BrCtrlCfgInit(&cfg);
    cfg.f7B8 = 1024;
    cfg.active = 2;
    cfg.profile[2].e[5][1] = 0x8001;
    saved = cfg;

    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
    CHECK(cfg_eq(&cfg, &saved));        /* untouched, NOT reset */

    CHECK(BrCtrlCfgReadFile(&cfg, NULL) == 0);
    CHECK(BrCtrlCfgReadFile(NULL, szPath) == 0);
}

/* ======================================================================
 * 4. BUG 2 -- pActive survives the load.
 * ====================================================================== */
static void test_pactive_not_rebuilt(void)
{
    unsigned char aGold[GOLD_SIZE];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_pactive");
    BrCtrlCfg     cfg;

    gold_build(aGold);
    /* Put a definite `active` in the file: file offset 0x5D4, value 1. */
    aGold[0x5D4] = 0x01; aGold[0x5D5] = 0x00;
    aGold[0x5D6] = 0x00; aGold[0x5D7] = 0x00;
    CHECK(write_file(szPath, aGold, sizeof aGold));

    /* This is what 0x10007F40's `ReadJoystick=3` leaves behind: the pair
     * (active, pActive) set together, before the config file is read. */
    BrCtrlCfgInit(&cfg);
    cfg.active  = 3;
    cfg.pActive = &cfg.profile[3];

    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 1);

    CHECK(cfg.active == 1);                    /* the file won ... */
    CHECK(cfg.pActive == &cfg.profile[3]);     /* ... but the pointer did not
                                                * follow it.  Rebuilding it
                                                * here fails this line. */

    /* And on the failure arm pActive IS rebuilt, because BrCtrlCfgInit sets
     * it -- the asymmetry is the whole finding. */
    aGold[4] = 0x03;
    CHECK(write_file(szPath, aGold, sizeof aGold));
    cfg.active  = 3;
    cfg.pActive = &cfg.profile[3];
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 0);
    CHECK(cfg.pActive == &cfg.profile[0]);

    remove(szPath);
}

/* ======================================================================
 * 5. No length check, no EOF check: trailing bytes are ignored.
 * ====================================================================== */
static void test_trailing_bytes(void)
{
    unsigned char aLong[GOLD_SIZE + 64];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_long");
    BrCtrlCfg     cfg;
    size_t        i;

    gold_build(aLong);
    for (i = 0; i < 64; ++i)
        aLong[GOLD_SIZE + i] = 0xAA;
    CHECK(write_file(szPath, aLong, sizeof aLong));

    BrCtrlCfgInit(&cfg);
    CHECK(BrCtrlCfgReadFile(&cfg, szPath) == 1);
    CHECK((uint32_t)cfg.f2A8 == exp32(0x008));

    remove(szPath);
}

/* ======================================================================
 * 6. The encoder: size, header, and a round trip through the real reader.
 * ====================================================================== */
static void test_encode_and_round_trip(void)
{
    unsigned char aImg[GOLD_SIZE + 8];
    const char   *szPath = BrTmpPath(0, "br_cfgfile_rt");
    BrCtrlCfg     src, dst;
    int           n, i, j, k;

    BrCtrlCfgInit(&src);
    /* Values chosen so a byte-order slip is visible in every width. */
    src.f2A8 = 0x01020304;
    src.f2AC = -2;
    src.f2B0 = 0x7F000001;
    src.f2B4[0x40] = 0xFEEDFACEu;
    src.f3B8[0xFF] = 0x11223344u;
    src.f7B8 = 1024;
    src.f7BC = 768;
    src.f7C8[3] = 0xCAFEBABEu;
    src.f810[7] = 0x00FF00FFu;
    src.f830[15] = 0x80000000u;
    src.f870 = -1;
    src.active = 2;
    for (i = 0; i < BR_CTRL_PROFILES; ++i)
        for (j = 0; j < BR_CTRL_ACTIONS; ++j)
            for (k = 0; k < 3; ++k)
                src.profile[i].e[j][k] =
                    (uint16_t)(0x8000u + (unsigned)(i * 100 + j * 3 + k));

    n = BrCtrlCfgFileEncode(aImg, sizeof aImg, &src);
    CHECK(n == GOLD_SIZE);

    CHECK(memcmp(aImg, "RCfg", 4) == 0);
    CHECK(aImg[4] == 0x02 && aImg[5] == 0 && aImg[6] == 0 && aImg[7] == 0);
    /* The header's little-endian claim, at a known field. */
    CHECK(aImg[0x008] == 0x04 && aImg[0x009] == 0x03 &&
          aImg[0x00A] == 0x02 && aImg[0x00B] == 0x01);

    CHECK(BrCtrlCfgFileEncode(aImg, (size_t)GOLD_SIZE - 1, &src) == -1);
    CHECK(BrCtrlCfgFileEncode(NULL, sizeof aImg, &src) == -1);
    CHECK(BrCtrlCfgFileEncode(aImg, sizeof aImg, NULL) == -1);

    CHECK(write_file(szPath, aImg, (size_t)n));
    BrCtrlCfgInit(&dst);
    CHECK(BrCtrlCfgReadFile(&dst, szPath) == 1);
    CHECK(cfg_eq(&dst, &src));

    remove(szPath);
}

int main(void)
{
    test_layout();
    test_magic();
    test_version();
    test_truncation();
    test_failure_resets_to_defaults();
    test_missing_file();
    test_pactive_not_rebuilt();
    test_trailing_bytes();
    test_encode_and_round_trip();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_cfgfile: all checks passed\n");
    return 0;
}
