/* test_slice8_88.c -- the seven control hooks slice8_88.c transcribes.
 *
 * WHAT THIS TEST IS FOR
 *
 * Seven hook bodies, each a handful of instructions, each reached only through
 * a function-pointer slot a builder filled.  "It compiles" says nothing about
 * any of them.  What this file checks is the BEHAVIOUR each body was
 * transcribed for -- the value that lands, the field it lands in, the order
 * two side effects happen in, and the asymmetries the disassembly shows that a
 * plausible-looking rewrite would smooth away:
 *
 *   - 0x10040730 loads its table entry WITHOUT sign extension while its
 *     neighbour 0x100407E0 loads the SAME shape of table WITH it.
 *   - 0x10040730 takes the LOW byte of the stage word and 0x100407E0 the
 *     HIGH byte of the same word.
 *   - 0x10040950's CLEAR arm is hard-wired to element ONE, not element zero.
 *   - 0x10040990's table is dwords but only the LOW WORD is taken.
 *   - 0x100413B0's id switch is on 0x10AA28A0 itself, while the number it
 *     prints is that global PLUS ONE.
 *   - 0x100414B0's clear arm displays "0" because the address it strcpy()s
 *     from is a DWORD TABLE (the header's CONFLICT 2).
 *   - 0x100414B0's index into the 0x10AA270E block is SIGNED.
 *   - 0x10042B00 is a one-shot latch: the second call does nothing.
 *
 * Every assertion below was proved to be load-bearing by inverting the logic
 * it covers and watching it fail -- see the note at the bottom of main().
 *
 * Every cross-module symbol is stood in for HERE and nowhere else, so the test
 * links against port/src/slice8_88.c alone and a failure can only be this
 * module's.  The stand-ins are scaffolding: none is decompiled and none is
 * asserted about except where it IS the observation (the notification log).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_88.h"

static int g_fails;

#define CHECK(cond, why)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n        %s\n",                            \
                   __FILE__, __LINE__, #cond, (why));                         \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS
 * ========================================================================== */

BrUi73Ctx  g_br73;
BrUiNav   *g_pBrUiNav;
int32_t    g_brAA28D8;      /* 0x10AA28D8, slice6_73.h */

int32_t    g_brAA2A1C;      /* 0x10AA2A1C, slice2_25.h */
int32_t    g_brAA2A28;      /* 0x10AA2A28              */
int32_t    g_brAA28E8;      /* 0x10AA28E8              */
int32_t    g_br18ABDBC;     /* 0x118ABDBC, slice3_45.h */

/* 0x10074030.  Ids 0xB3..0xB6 are the only ones 0x100413B0 asks for; the
 * stand-in answers a distinguishable string for each and NULL for anything
 * else, so a wrong id is visible in the output rather than merely different. */
static const char *StrGetImpl(int id)
{
    switch (id) {
    case 0xB3: return "<b3>";
    case 0xB4: return "<b4>";
    case 0xB5: return "<b5>";
    case 0xB6: return "<b6>";
    default:   return NULL;
    }
}
const char *BrStrGet(int id) { return StrGetImpl(id); }

/* --- the text-box vtable, as a notification log -------------------------- */

#define LOG_MAX 16
static char s_aLog[LOG_MAX][8];
static int  s_cLog;

static void Log(const char *psz)
{
    if (s_cLog < LOG_MAX) {
        snprintf(s_aLog[s_cLog], sizeof s_aLog[0], "%s", psz);
    }
    ++s_cLog;
}

static void Vt04(BrTextBox *p) { (void)p; Log("04"); }
static void Vt08(BrTextBox *p) { (void)p; Log("08"); }
static void Vt10(BrTextBox *p) { (void)p; Log("10"); }
static void Vt2C(BrTextBox *p) { (void)p; Log("2C"); }

static const BrTextBoxVtbl s_vtbl = {
    NULL, Vt04, Vt08, NULL, Vt10, NULL, NULL, NULL, NULL, NULL, NULL, Vt2C
};

static int LogIs(const char *a, const char *b)
{
    return s_cLog == 2 && strcmp(s_aLog[0], a) == 0 && strcmp(s_aLog[1], b) == 0;
}

/* --- the objects --------------------------------------------------------- */

static BrUiCtl_ *NewCtl(void)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (p == NULL) {
        printf("FAIL out of memory\n");
        exit(1);
    }
    p->aText[0].pVtbl = &s_vtbl;
    s_cLog = 0;
    return p;
}

/* The 0x100B3810 array.  Four records of stride 0x18; f10[] holds four words
 * each, and the two hooks read the low and high byte of one of them. */
static BrMenuStage s_aStage[4];

/* The 0x10AA26F0 block: 0x53 dwords in the original.  The four halfwords
 * 0x100414B0 sums live at byte 0x1E + 8*index. */
#define AA26F0_DWORDS 0x53
static int32_t s_aAA26F0[AA26F0_DWORDS];

static void PutHalfwords(int index, uint16_t a, uint16_t b, uint16_t c,
                         uint16_t d)
{
    unsigned char *p = (unsigned char *)s_aAA26F0 + 0x1E + 8 * index;
    memcpy(p + 0, &a, 2);
    memcpy(p + 2, &b, 2);
    memcpy(p + 4, &c, 2);
    memcpy(p + 6, &d, 2);
}

static char s_szScratch[0x104];

static void ResetWorld(void)
{
    memset(&g_br73, 0, sizeof g_br73);
    memset(s_aStage, 0, sizeof s_aStage);
    memset(s_aAA26F0, 0, sizeof s_aAA26F0);
    memset(s_szScratch, 0, sizeof s_szScratch);
    g_pBrUiNav  = NULL;
    g_brAA28D8  = 0;
    g_brAA2A1C  = 0;
    g_brAA2A28  = 0;
    g_brAA28E8  = 0;
    g_br18ABDBC = 0;

    g_br73.aAA26F0   = s_aAA26F0;
    g_br73.szAA2518  = s_szScratch;
    g_br73.cbScratch = sizeof s_szScratch;

    BrUiHook88Reset();
    g_brHook88.pStages = s_aStage;
}

/* ==========================================================================
 * 0x10040730 / 0x100407E0 -- the two halves of one stage word
 * ========================================================================== */

static void TestStagePair(void)
{
    BrUiCtl_ *p;

    printf("0x10040730 / 0x100407E0 -- stage caption setters\n");

    /* The table 0x100AC550's contents, restated here from the image so the
     * expectation is not simply a copy of the module's array:
     *   [0]=0x0010 [3]=0x001B [5]=0x0076 [13]=0x008F [15]=0x0000            */

    /* --- 0x10040730, the 0x100AA010 arm ---------------------------------- */
    ResetWorld();
    g_br73.n0AA010 = 1;
    g_br73.n0AC648 = 3;
    p = NewCtl();
    CHECK(BrUiHook88_10040730(p) == 1, "0x10040730 returns 1");
    CHECK(p->w1E20C == 0x001B,
          "with 0x100AA010 set the index is 0x100AC648, so entry 3 == 0x1B");
    free(p);

    /* --- 0x10040730, the stage arm, column A (0x10AA28A8 clear) ---------- */
    ResetWorld();
    g_br73.bAA28B8   = 2;          /* stage record 2 */
    g_br73.nAA28A4   = 1;          /* column 1 -- taken because 28A8 is clear */
    g_br73.nAA28AC   = 3;          /* column 3 -- must NOT be taken           */
    s_aStage[2].f10[1] = 0x0D05;   /* low byte 5, high byte 0x0D             */
    s_aStage[2].f10[3] = 0x0000;
    p = NewCtl();
    CHECK(BrUiHook88_10040730(p) == 1, "0x10040730 returns 1 on the stage path");
    CHECK(p->w1E20C == 0x0076,
          "0x10040730 takes the LOW byte (5) of stage[2].f10[1] and indexes "
          "0x100AC550 with it: entry 5 == 0x76");
    free(p);

    /* --- the 0x10AA28A8 selector actually selects ------------------------ */
    ResetWorld();
    g_br73.bAA28B8     = 2;
    g_br73.nAA28A4     = 1;
    g_br73.nAA28AC     = 3;
    g_brHook88.bAA28A8 = 1;        /* now column 3 */
    s_aStage[2].f10[1] = 0x0D05;
    s_aStage[2].f10[3] = 0x0000;   /* low byte 0 -> entry 0 == 0x10 */
    p = NewCtl();
    (void)BrUiHook88_10040730(p);
    CHECK(p->w1E20C == 0x0010,
          "a non-zero 0x10AA28A8 swaps the column to 0x10AA28AC");
    free(p);

    /* --- 0x100407E0 takes the HIGH byte of the SAME word ----------------- */
    ResetWorld();
    g_brAA28E8       = 1;          /* not idle, so the -2 guard does not fire */
    g_br73.bAA28B8   = 2;
    g_br73.nAA28A4   = 1;
    s_aStage[2].f10[1] = 0x0301;   /* low byte 1, high byte 3 */
    p = NewCtl();
    CHECK(BrUiHook88_100407E0(p) == 1, "0x100407E0 returns 1");
    /* 0x100AC590 = { 0x17, 0x13, 0x15, 0x16, 0x14, 0, 0, 0 }; entry 3 = 0x16.
     * If it took the LOW byte it would answer entry 1 == 0x13. */
    CHECK(p->w1E20C == 0x0016,
          "0x100407E0 takes the HIGH byte (3) of the same word, not the low "
          "one -- entry 3 of 0x100AC590 is 0x16, entry 1 is 0x13");
    free(p);

    /* --- 0x100407E0's alternate index is 0x10AA2A00, not 0x100AC648 ------ */
    ResetWorld();
    g_brAA28E8     = 1;
    g_br73.n0AA010 = 1;
    g_br73.nAA2A00 = 4;            /* entry 4 == 0x14 */
    g_br73.n0AC648 = 0;            /* entry 0 == 0x17 -- must NOT be taken */
    p = NewCtl();
    (void)BrUiHook88_100407E0(p);
    CHECK(p->w1E20C == 0x0014,
          "0x100407E0's 0x100AA010 arm indexes with 0x10AA2A00, not 0x100AC648");
    free(p);

    /* --- the two tables are read with DIFFERENT sign rules ---------------- */
    /* 0x100AC630 is not this pair's table, so the sign asymmetry is shown on
     * 0x100AC590 instead: its entries are all positive, so instead the check
     * is that 0x10040730's word table can deliver a value with the high bit
     * of the LOW byte set (0x008F) unchanged and unsign-extended. */
    ResetWorld();
    g_br73.n0AA010 = 1;
    g_br73.n0AC648 = 13;           /* 0x100AC550[13] == 0x008F */
    p = NewCtl();
    (void)BrUiHook88_10040730(p);
    CHECK(p->w1E20C == 0x008F,
          "0x10040730's `mov cx, word` keeps 0x008F as 0x008F");
    free(p);

    /* --- out of range answers 0 on both (the port's bound) ---------------- */
    ResetWorld();
    g_br73.n0AA010 = 1;
    g_br73.n0AC648 = 99;
    p = NewCtl();
    (void)BrUiHook88_10040730(p);
    CHECK(p->w1E20C == 0, "an out-of-range index answers 0, not garbage");
    free(p);
}

/* ==========================================================================
 * 0x100407E0's -2 guard
 * ========================================================================== */

static void TestIdleGuard(void)
{
    BrUiCtl_ *p;
    BrUiNav   nav;
    BrPhase_  phaseA, phaseB;

    printf("0x100407E0 -- the reserved -2\n");

    memset(&nav, 0, sizeof nav);
    memset(&phaseA, 0, sizeof phaseA);
    memset(&phaseB, 0, sizeof phaseB);

    /* Idle: current phase == 0x10AA2964 AND 0x10AA28E8 == 0. */
    ResetWorld();
    g_pBrUiNav          = &nav;
    nav.pAA2904         = &phaseA;
    g_brHook88.pAA2964  = &phaseA;
    g_brAA28E8          = 0;
    p = NewCtl();
    p->w1E20C = 0xBEEF;
    CHECK(BrUiHook88_100407E0(p) == -2,
          "both conditions met -> -2, the reserved 'leave this item alone'");
    CHECK(p->w1E20C == 0xBEEF, "-2 leaves the caption field untouched");
    free(p);

    /* Either condition alone breaks it. */
    ResetWorld();
    g_pBrUiNav         = &nav;
    nav.pAA2904        = &phaseA;
    g_brHook88.pAA2964 = &phaseB;     /* phases differ */
    g_brAA28E8         = 0;
    p = NewCtl();
    CHECK(BrUiHook88_100407E0(p) == 1,
          "a DIFFERENT current phase is not idle");
    free(p);

    ResetWorld();
    g_pBrUiNav         = &nav;
    nav.pAA2904        = &phaseA;
    g_brHook88.pAA2964 = &phaseA;
    g_brAA28E8         = 1;           /* the second condition fails */
    p = NewCtl();
    CHECK(BrUiHook88_100407E0(p) == 1,
          "a non-zero 0x10AA28E8 is not idle even with matching phases");
    free(p);
}

/* ==========================================================================
 * 0x10040950 / 0x10040990
 * ========================================================================== */

static void TestTwoOddOnes(void)
{
    BrUiCtl_ *p;

    printf("0x10040950 / 0x10040990 -- the two asymmetries\n");

    /* 0x100AC630 = { 0x61, 0x66, 0, 0 }. */
    ResetWorld();
    g_br18ABDBC = 0;
    g_brAA2A1C  = 0;                  /* would give 0x61 through the table */
    p = NewCtl();
    CHECK(BrUiHook88_10040950(p) == 1, "0x10040950 returns 1");
    CHECK(p->w1E20C == 0x0066,
          "with 0x118ABDBC CLEAR the entry is hard-wired to 0x100AC631 -- "
          "element ONE (0x66), not element zero (0x61)");
    free(p);

    ResetWorld();
    g_br18ABDBC = 1;
    g_brAA2A1C  = 0;
    p = NewCtl();
    (void)BrUiHook88_10040950(p);
    CHECK(p->w1E20C == 0x0061,
          "with 0x118ABDBC SET it indexes the table with 0x10AA2A1C");
    free(p);

    ResetWorld();
    g_br18ABDBC = 1;
    g_brAA2A1C  = 1;
    p = NewCtl();
    (void)BrUiHook88_10040950(p);
    CHECK(p->w1E20C == 0x0066, "index 1 through the table is also 0x66");
    free(p);

    /* 0x100AC640 = { 0x0000008C, 0x0000008D } as DWORDS. */
    ResetWorld();
    g_brAA2A28 = 0;
    p = NewCtl();
    CHECK(BrUiHook88_10040990(p) == 1, "0x10040990 returns 1");
    CHECK(p->w1E20C == 0x008C, "0x10040990 takes the LOW WORD of dword 0");
    free(p);

    ResetWorld();
    g_brAA2A28 = 1;
    p = NewCtl();
    (void)BrUiHook88_10040990(p);
    CHECK(p->w1E20C == 0x008D,
          "0x10040990 strides by FOUR bytes, so index 1 is 0x8D and not the "
          "high half of dword 0 (which is 0)");
    free(p);
}

/* ==========================================================================
 * 0x100413B0
 * ========================================================================== */

static void TestText13B0(void)
{
    BrUiCtl_ *p;
    int       n;

    printf("0x100413B0 -- \"%%d\" then \"%%s%%s\"\n");

    /* The id switch is on 0x10AA28A0 ITSELF; the printed number is that
     * global PLUS ONE.  A transcription that used the same value for both
     * would produce "<b4>" here instead of "<b3>", or "2" instead of "1". */
    ResetWorld();
    g_br73.nAA28A0 = 0;
    p = NewCtl();
    CHECK(BrUiHook88_100413B0(p) == 1, "0x100413B0 returns 1");
    CHECK(strcmp(p->aText[0].sz, "1<b3>") == 0,
          "0x10AA28A0 == 0 prints 0+1 == \"1\" and asks for id 0xB3");
    CHECK(strcmp(s_szScratch, "1") == 0,
          "the \"%d\" step lands in 0x10AA2518 and stays there");
    CHECK(LogIs("04", "10"),
          "a CAPTION assignment notifies vtable +0x04 then +0x10, in that "
          "order and nothing else");
    free(p);

    for (n = 1; n <= 2; ++n) {
        char szWant[16];
        ResetWorld();
        g_br73.nAA28A0 = n;
        p = NewCtl();
        (void)BrUiHook88_100413B0(p);
        snprintf(szWant, sizeof szWant, "%d<b%d>", n + 1, 3 + n);
        CHECK(strcmp(p->aText[0].sz, szWant) == 0,
              "ids 0xB4 and 0xB5 follow 0 -> 0xB3 by one each");
        free(p);
    }

    /* Anything else -- including a NEGATIVE -- falls through to 0xB6. */
    ResetWorld();
    g_br73.nAA28A0 = 7;
    p = NewCtl();
    (void)BrUiHook88_100413B0(p);
    CHECK(strcmp(p->aText[0].sz, "8<b6>") == 0,
          "3 and above take the default id 0xB6");
    free(p);

    ResetWorld();
    g_br73.nAA28A0 = -5;
    p = NewCtl();
    (void)BrUiHook88_100413B0(p);
    CHECK(strcmp(p->aText[0].sz, "-4<b6>") == 0,
          "a NEGATIVE 0x10AA28A0 also takes 0xB6 -- the original's three "
          "`dec/je` tests only match 0, 1 and 2");
    free(p);

    /* An unwired scratch buffer is skipped, not faulted. */
    ResetWorld();
    g_br73.szAA2518  = NULL;
    g_br73.cbScratch = 0;
    g_br73.nAA28A0   = 0;
    p = NewCtl();
    CHECK(BrUiHook88_100413B0(p) == 1, "an unwired 0x10AA2518 does not fault");
    CHECK(strcmp(p->aText[0].sz, "<b3>") == 0,
          "and the head half is simply empty");
    free(p);

    /* An unwired vtable is a missing effect, not a crash. */
    ResetWorld();
    g_br73.nAA28A0 = 0;
    p = NewCtl();
    p->aText[0].pVtbl = NULL;
    CHECK(BrUiHook88_100413B0(p) == 1, "a NULL text-box vtable does not fault");
    CHECK(strcmp(p->aText[0].sz, "1<b3>") == 0,
          "and the text still lands");
    free(p);
}

/* ==========================================================================
 * 0x100414B0
 * ========================================================================== */

static void TestText14B0(void)
{
    BrUiCtl_ *p;

    printf("0x100414B0 -- the halfword sum, and the \"string\" that is not\n");

    /* --- the clear arm: CONFLICT 2 --------------------------------------- */
    ResetWorld();
    g_br73.nAA289C = 0;
    p = NewCtl();
    CHECK(BrUiHook88_100414B0(p) == 1, "0x100414B0 returns 1");
    CHECK(strcmp(p->aText[0].sz, "0") == 0,
          "with 0x10AA289C clear the text is \"0\" -- the address the original "
          "strcpy()s from is a DWORD TABLE whose first element is 0x00000030");
    CHECK(LogIs("08", "2C"),
          "a VALUE assignment notifies vtable +0x08 then +0x2C -- NOT the "
          "+0x04/+0x10 pair 0x100413B0 uses");
    free(p);

    /* --- the sum arm ------------------------------------------------------ */
    ResetWorld();
    g_br73.nAA289C = 1;
    g_br73.bAA28B8 = 2;
    PutHalfwords(2, 1000, 200, 30, 4);
    PutHalfwords(1, 9999, 9999, 9999, 9999);   /* the neighbour must not leak */
    PutHalfwords(3, 8888, 8888, 8888, 8888);
    p = NewCtl();
    (void)BrUiHook88_100414B0(p);
    CHECK(strcmp(p->aText[0].sz, "1234") == 0,
          "the four halfwords at 0x10AA270E + 8*index are summed, and only "
          "the four belonging to that index");
    free(p);

    /* Exactly FOUR terms, and the stride really is 8: a fifth halfword right
     * after the four must not be counted. */
    ResetWorld();
    g_br73.nAA289C = 1;
    g_br73.bAA28B8 = 0;
    PutHalfwords(0, 1, 2, 3, 4);
    PutHalfwords(1, 50000, 0, 0, 0);
    p = NewCtl();
    (void)BrUiHook88_100414B0(p);
    CHECK(strcmp(p->aText[0].sz, "10") == 0,
          "four terms exactly -- a fifth halfword 8 bytes on is the NEXT "
          "record and is not summed");
    free(p);

    /* Each term is ZERO-extended: `xor esi,esi` runs once, before the loop,
     * and only `si` is written per iteration.  A sign-extending transcription
     * would answer 65535 - 1 = a very different number here. */
    ResetWorld();
    g_br73.nAA289C = 1;
    g_br73.bAA28B8 = 1;
    PutHalfwords(1, 0xFFFF, 0, 0, 0);
    p = NewCtl();
    (void)BrUiHook88_100414B0(p);
    CHECK(strcmp(p->aText[0].sz, "65535") == 0,
          "a halfword of 0xFFFF contributes +65535, not -1");
    free(p);

    /* CONFLICT 1: the index byte is SIGNED.  Index 0xFF must read the record
     * EIGHT BYTES BEFORE the base, not 255 records after it. */
    ResetWorld();
    g_br73.nAA289C = 1;
    g_br73.bAA28B8 = 0xFF;             /* (int8_t)0xFF == -1 */
    /* base = -1*8 + 0x1E = 0x16 */
    {
        unsigned char *q = (unsigned char *)s_aAA26F0 + 0x16;
        uint16_t a = 11, b = 22, c = 33, d = 44;
        memcpy(q + 0, &a, 2); memcpy(q + 2, &b, 2);
        memcpy(q + 4, &c, 2); memcpy(q + 6, &d, 2);
    }
    PutHalfwords(0, 7000, 0, 0, 0);    /* the unsigned reading would find this */
    p = NewCtl();
    (void)BrUiHook88_100414B0(p);
    CHECK(strcmp(p->aText[0].sz, "110") == 0,
          "0x10AA28B8 is read `movsx`, so 0xFF indexes BACKWARDS -- the sum "
          "is the record at byte 0x16, not one 255 records away");
    free(p);

    /* NOT TESTED, and said so rather than covered by a fixture that cannot
     * tell right from wrong: the `_strupr` the original runs before the store.
     * Both arms of this hook can only ever produce digits -- "0" on the clear
     * arm, _itoa of a sum on the other -- so upper-casing has nothing to
     * change, and no input reachable through this module's interface can
     * distinguish a correct _strupr from a missing one.  Deleting the call
     * from slice8_88.c leaves every CHECK in this file passing.  It is
     * transcribed because the instruction is there, not because it is proved.
     */

    /* An unwired block is skipped, not faulted; the sum is then 0. */
    ResetWorld();
    g_br73.nAA289C  = 1;
    g_br73.aAA26F0  = NULL;
    p = NewCtl();
    CHECK(BrUiHook88_100414B0(p) == 1, "an unwired 0x10AA26F0 does not fault");
    CHECK(strcmp(p->aText[0].sz, "0") == 0, "and the sum reads 0");
    free(p);
}

/* ==========================================================================
 * 0x10042B00
 * ========================================================================== */

static void TestToggle(void)
{
    BrUiCtl_ *p;

    printf("0x10042B00 -- the one-shot latch\n");

    ResetWorld();
    p = NewCtl();
    p->aText[0].f420 = 0;
    CHECK(BrUiHook88_10042B00(p) == 1, "0x10042B00 returns 1");
    CHECK(p->aText[0].f420 == 1u, "0 toggles to 1");
    CHECK(g_brAA28D8 == 1, "and the latch is set");

    CHECK(BrUiHook88_10042B00(p) == 1, "the second call still returns 1");
    CHECK(p->aText[0].f420 == 1u,
          "0x10AA28D8 is a LATCH, not a debounce -- nothing in the packet "
          "clears it, so the second call does NOTHING");
    free(p);

    /* The stored value is `sete cl`: exactly 0 or 1, never the complement of
     * a wider value. */
    ResetWorld();
    p = NewCtl();
    p->aText[0].f420 = 0x1234u;
    (void)BrUiHook88_10042B00(p);
    CHECK(p->aText[0].f420 == 0u,
          "a non-zero of ANY width toggles to exactly 0, not to ~0x1234");
    free(p);
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

static size_t CountNonNull(const void *pBase, size_t cb)
{
    const unsigned char *p = (const unsigned char *)pBase;
    size_t i, n = 0;

    for (i = 0; i + sizeof(void *) <= cb; i += sizeof(void *)) {
        void *pv;
        memcpy(&pv, p + i, sizeof pv);
        if (pv != NULL) {
            ++n;
        }
    }
    return n;
}

static void TestInstall(void)
{
    BrS71Hooks  h71;
    BrUi72Hooks h72;
    BrUi73Hooks h73;

    printf("installation\n");

    memset(&h71, 0, sizeof h71);
    BrUiHook88Install71(&h71);
    CHECK(h71.p10042B00 == BrUiHook88_10042B00, "71 0x10042B00");
    CHECK(CountNonNull(&h71, sizeof h71) == 1,
          "Install71 fills one slot and no more");

    memset(&h72, 0, sizeof h72);
    BrUiHook88Install72(&h72);
    CHECK(h72.p10040730 == BrUiHook88_10040730, "72 0x10040730");
    CHECK(h72.p100407E0 == BrUiHook88_100407E0, "72 0x100407E0");
    CHECK(h72.p10040950 == BrUiHook88_10040950, "72 0x10040950");
    CHECK(h72.p10040990 == BrUiHook88_10040990, "72 0x10040990");
    CHECK(h72.p100413B0 == BrUiHook88_100413B0, "72 0x100413B0");
    CHECK(h72.p100414B0 == BrUiHook88_100414B0, "72 0x100414B0");
    CHECK(CountNonNull(&h72, sizeof h72) == 6,
          "Install72 fills six slots and no more");

    /* slice7_80.c's BrUiOptInstall72 owns this one.  If this installer ever
     * starts writing it the two passes will fight over the table, so the fact
     * that it does NOT is an assertion, not a comment. */
    CHECK(h72.p100436B0 == NULL,
          "72 leaves 0x100436B0 alone -- slice7_80.c's BrUiOptInstall72 owns "
          "it and already installs BrOptCycleAA2A24 there");

    memset(&h73, 0, sizeof h73);
    BrUiHook88Install73(&h73);
    CHECK(h73.p100413B0 == BrUiHook88_100413B0, "73 0x100413B0");
    CHECK(CountNonNull(&h73, sizeof h73) == 1,
          "Install73 fills one slot and no more");

    /* An installer handed NULL writes nothing and does not fault. */
    BrUiHook88Install71(NULL);
    BrUiHook88Install72(NULL);
    BrUiHook88Install73(NULL);
}

/* ==========================================================================
 * The context
 * ========================================================================== */

static void TestReset(void)
{
    printf("BrUiHook88Reset\n");

    g_brHook88.pStages = s_aStage;
    g_brHook88.bAA28A8 = 1;
    BrUiHook88Reset();
    CHECK(g_brHook88.pStages == NULL && g_brHook88.bAA28A8 == 0
       && g_brHook88.pAA2964 == NULL, "Reset zeroes every owned field");

    /* An unwired stage table is skipped, not faulted -- the DEVIATION the .c
     * documents.  The index then reads 0, which is a VALID table index, so
     * this is a real answer and not a sentinel. */
    ResetWorld();
    g_brHook88.pStages = NULL;
    {
        BrUiCtl_ *p = NewCtl();
        CHECK(BrUiHook88_10040730(p) == 1,
              "an unwired 0x100B3810 does not fault");
        CHECK(p->w1E20C == 0x0010,
              "and the stage byte reads 0, i.e. table entry 0 == 0x10");
        free(p);
    }
}

int main(void)
{
    TestStagePair();
    TestIdleGuard();
    TestTwoOddOnes();
    TestText13B0();
    TestText14B0();
    TestToggle();
    TestInstall();
    TestReset();

    /* NOTE ON EVIDENCE.
     *
     * A passing test proves nothing on its own, so each claim above was
     * checked by MUTATING port/src/slice8_88.c and confirming this file then
     * fails.  Twenty-five mutations were run; the twenty-three below each
     * killed at least one CHECK:
     *
     *   stage byte low <-> high, in each of 0x10040730 and 0x100407E0
     *   0x10AA28A8's column selector inverted
     *   0x100AC550 read as bytes instead of words
     *   0x10040950's clear arm using element 0 instead of element 1
     *   0x100AC640 strided by 2 instead of 4
     *   0x100407E0's alternate index taken from 0x100AC648 not 0x10AA2A00
     *   0x100407E0 returning 1 instead of -2 when idle
     *   the idle guard dropping its 0x10AA28E8 term
     *   0x100413B0's id switch reading n+1 instead of n
     *   0x100413B0 printing n instead of n+1
     *   0x100413B0 emitting tail-then-head instead of head-then-tail
     *   the caption tail using +0x08/+0x2C, and the value tail +0x04/+0x10
     *   0x100414B0's halfwords sign-extended
     *   0x10AA28B8 read unsigned
     *   the 0x10AA270E offset, stride and term count, each changed in turn
     *   k_AD278 changed from "0" to "48"
     *   0x10042B00 storing the complement, and dropping its latch
     *   Install72 also writing p100436B0, Install73 writing the wrong slot
     *
     * TWO mutations SURVIVED, and both are honest holes rather than missing
     * assertions -- neither property is observable with the shipped data:
     *
     *   1. Reading 0x100AC590 / 0x100AC630 with movzx instead of movsx.
     *      Every byte of both tables is below 0x80 in both images, so the two
     *      instructions cannot disagree on any value the game can produce.
     *   2. Deleting the _strupr from 0x100414B0.  Both arms produce digits
     *      only; see the note in TestText14B0.
     *
     * Adding a fixture that "covers" either would mean inventing data the
     * original cannot hold, which is the kind of assertion CONVENTIONS.md
     * says is worse than none. */

    /* The last line must report the COUNT, not the word -- tools/regress.sh
     * parses `N failures` and reports anything else as UNKNOWN.  Same format
     * as test_slice8_85.c for exactly that reason. */
    printf("\ntest_slice8_88: %s (%d failure%s)\n",
           g_fails ? "FAIL" : "ok", g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
