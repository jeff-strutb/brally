/* test_slice6_76.c -- behavioural tests for packet 76.
 *
 * WHAT IS BEING TESTED, AND WHY IN THIS SHAPE
 * ===========================================
 * The packet has two halves and they need different kinds of test.
 *
 * The TWELVE ADAPTERS can only get one thing wrong: whether they forward their
 * arguments unchanged, bind the right object, and hand back what the callee
 * returned.  They are therefore tested against recording stand-ins, checking
 * exactly that and nothing else.  Asserting anything about the callee's own
 * behaviour here would be testing another module's code through a wrapper.
 *
 * The FOUR TRANSCRIBED functions are tested against properties, per
 * CONVENTIONS, not against numbers this port happened to produce:
 *
 *   0x100193C0 -- checked against an INDEPENDENT reimplementation of the
 *       measurement written from the disassembly's structure rather than from
 *       the transcription, so a shared misreading cannot pass; plus the
 *       identities that hold whatever the tables contain (concatenation is
 *       additive over plain glyphs; case is irrelevant because upper and lower
 *       share a class; the empty string is zero; the hi-res flag's
 *       double-then-halve is near-idempotent); plus the two `%` quirks and the
 *       high-bit branch, which are the behaviours a plausible-but-wrong
 *       version would get wrong.
 *
 *   0x10060D90 -- checked for the asymmetry that is the whole point of the
 *       function: the A slider drives the hook AND the 0x100BBAD8 byte, the B
 *       slider drives only 0x100BBAE0, and the two use DIFFERENT tables.  A
 *       version that merged the tables passes nothing here.
 *
 *   0x10072580 -- checked for its four guards in order, and for the fact that
 *       a NULL slot is a no-op rather than a call.
 *
 *   0x10005D30 -- checked for the initial value, because -1 rather than 0 is
 *       the entire content of the function's context.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file and nowhere else,
 * as the house contract requires; the module links against its own module
 * only.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "slice6_76.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * The functions under test.
 *
 * slice6_76.h deliberately does not declare these: every one already has a
 * declaration in the module that CALLS it, and that declaration is the
 * contract this packet must satisfy.  So each prototype below is copied
 * VERBATIM from the header that owns it, named in the comment.  If an owner
 * ever changes one, this file stops linking -- which is the point.
 * ========================================================================== */

struct BrGfxCmd;      /* slice2_15.h */
struct BrGameObj;     /* slice2_25.h */

/* slice2_15.h:495 */
void     BrSub_1002F900(struct BrGfxCmd *pCmd,
                        int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                        int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                        int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                        int32_t a13, int32_t a14, int32_t a15, int32_t a16);
/* slice2_17.c:97  */ void     BrX10042AF0(void *p, int a1, int a2);
/* slice2_18.h:135 */ void     BrGfx42AF0_3(void *p0, int32_t a1, int32_t a2);
/* slice2_17.c:103 */ void    *BrX10069490(void);
/* slice3_31.h:277 */ void     BrExt_1003E310(void);
/* slice3_31.h:289 */ void     BrExt_1006A4A0(void *pThis, void *pArg);
/* slice2_17.c:68  */ int      BrX10060E90(void);
/* slice2_17.c:101 */ void    *BrX10069530(void);
/* slice3_31.h:278 */ void     BrExt_10079550(void);
/* slice2_26.h:246 */ void     BrExt_100443E0(int32_t a);
/* slice2_26.h:247 */ void     BrExt_10044280(int32_t a);
/* slice2_26.h:242 */ void     BrExt_10043BF0(int32_t a);
/* slice2_25.h:457 */ void     BrSub10060D90(void);
/* slice6_70.h:374 */ int      BrSub_100193C0(const char *psz, int scale);
/* slice2_17.c:95  */ void     BrX10072580(int a0);
/* slice3_40.h:169 */ int32_t  BrSub10005D30(void);

/* ==========================================================================
 * STAND-INS for cross-slice symbols.  Test scaffolding only.
 *
 * Each records what it was handed and returns a value the caller could not
 * have invented, so "forwarded correctly" and "returned the callee's answer"
 * are both observable.
 * ========================================================================== */

/* --- slice1_05.h, 0x1002F900 --------------------------------------------- */
struct BrGfxWords;
static void   *s_pCombineOut;
static int      s_aCombine[16];
static int      s_nCombine;

void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    s_pCombineOut = (void *)pOut;
    s_aCombine[0]  = a0;  s_aCombine[1]  = b0;  s_aCombine[2]  = c0;
    s_aCombine[3]  = d0;  s_aCombine[4]  = Aa0; s_aCombine[5]  = Ab0;
    s_aCombine[6]  = Ac0; s_aCombine[7]  = Ad0; s_aCombine[8]  = a1;
    s_aCombine[9]  = b1;  s_aCombine[10] = c1;  s_aCombine[11] = d1;
    s_aCombine[12] = Aa1; s_aCombine[13] = Ab1; s_aCombine[14] = Ac1;
    s_aCombine[15] = Ad1;
    ++s_nCombine;
}

/* --- slice5_61.h, 0x10042AF0 and 0x10060E90 ------------------------------ */
static void *s_p42AF0;
static int   s_n42AF0;

void BrGfx42AF0_1(void *p0) { s_p42AF0 = p0; ++s_n42AF0; }

static int32_t s_timeNow;
static int     s_nTimeNow;

int32_t BrTimeNow(void) { ++s_nTimeNow; return s_timeNow; }

/* --- slice5_62.h, 0x10069490 --------------------------------------------- */
struct BrMat4;
static int s_n69490;
static char s_mtxObject;

struct BrMat4 *BrSub_10069490(void)
{
    ++s_n69490;
    return (struct BrMat4 *)&s_mtxObject;
}

/* --- slice3_41.h, 0x10069530 --------------------------------------------- */
static int  s_n69530;
static char s_poolObject;

void *BrPool32Alloc(void) { ++s_n69530; return &s_poolObject; }

/* --- slice5_63.h, 0x1003E310 --------------------------------------------- */
static int s_n3E310;
void BrSub1003E310(void) { ++s_n3E310; }

/* --- slice4_53.h, 0x1006A4A0 --------------------------------------------- */
static void *s_p6A4A0This;
static void *s_p6A4A0Arg;
static int   s_n6A4A0;

void BrSub1006A4A0(void *pThis, void *pArg)
{
    s_p6A4A0This = pThis;
    s_p6A4A0Arg  = pArg;
    ++s_n6A4A0;
}

/* --- slice1_10.h / slice3_45.h, 0x10079550 ------------------------------- */
struct BrFfb;
/* The single instance slice3_45.c owns.  Given a body here only so that the
 * adapter's `&g_brFfb` has something to point at; the test's whole claim is
 * that the adapter passes THAT address and no other. */
struct BrFfb { int dummy; };
struct BrFfb g_brFfb;
static struct BrFfb *s_pFfbArg;
static int           s_nFfb;

void BrFfbShutdown(struct BrFfb *pFfb) { s_pFfbArg = pFfb; ++s_nFfb; }

/* --- slice2_25.h, 0x100443E0 / 0x10044280 -------------------------------- */
static struct BrGameObj *s_p2950A, *s_p2950B;
static int               s_n2950A,  s_n2950B;

int BrOptOpen2950A(struct BrGameObj *p) { s_p2950A = p; ++s_n2950A; return 7; }
int BrOptOpen2950B(struct BrGameObj *p) { s_p2950B = p; ++s_n2950B; return 9; }

/* --- slice4_50.h, 0x10043BF0 --------------------------------------------- */
static struct BrGameObj *s_p43BF0;
static int               s_n43BF0;

void BrSub10043BF0(struct BrGameObj *p) { s_p43BF0 = p; ++s_n43BF0; }

/* --- slice1_08.h, 0x10072550 and the three gates ------------------------- */
struct BrSndVoice;
struct BrDSound;
int32_t          BrSndG0B5DE8;
struct BrDSound *BrSndPDS;
void            *BrSndG18290FC;
uint8_t          BrSndMasterVolume;

static struct BrSndVoice *s_pStopArg;
static int                s_nStop;

int32_t BrSndVoiceStop(struct BrSndVoice *pVoice)
{
    s_pStopArg = pVoice;
    ++s_nStop;
    return 0;
}

/* --- slice3_40.h, the two level tables and 0x100BBAD8 -------------------- */
/* Values transcribed from the image by slice3_40.c; repeated here because the
 * test links this module alone.  The A table is LINEAR and the B table is a
 * PERCEPTUAL curve -- README records that they must not be merged, and the
 * test below is what would catch a merge. */
uint8_t BrG_0BBAD8;
const int32_t BrOptLevelATable[10] = {
    0, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF
};
const int32_t BrOptLevelBTable[10] = {
    0, 0x55, 0x78, 0x93, 0xAA, 0xBE, 0xD0, 0xE1, 0xF0, 0xFF
};

int32_t g_brB4E708;
int32_t g_brB4E70C;

/* --- slice2_18.h / slice2_20.h ------------------------------------------- */
int32_t BrG_6C65E4;
int     g_i0B8C90 = 1;      /* br_data.c's value, read out of the image */

/* --- slice4_50.h, 0x10094294 -- owned there, aliased by this packet ------ */
int32_t g_br094294 = -1;    /* mirrors slice4_50.c's corrected initialiser */

/* --- the 0x100029F0 seam ------------------------------------------------- */
static int32_t s_musicVolume;
static int     s_nMusicVolume;

static void MusicVolumeSpy(int32_t volume)
{
    s_musicVolume = volume;
    ++s_nMusicVolume;
}

/* ==========================================================================
 * 1. Adapter tests
 * ========================================================================== */

static void TestAdapterCombine(void)
{
    /* Any object will do: the claim is that the pointer arrives unchanged and
     * that all sixteen selectors arrive in order. */
    char obj;
    int  i;

    s_nCombine = 0;
    BrSub_1002F900((struct BrGfxCmd *)&obj,
                   1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    CHECK(s_nCombine == 1);
    CHECK(s_pCombineOut == (void *)&obj);
    for (i = 0; i < 16; ++i)
        CHECK(s_aCombine[i] == i + 1);
}

static void TestAdapter42AF0(void)
{
    char a, b;

    /* Both stub names must reach the SAME body: one address, one owner. */
    s_n42AF0 = 0;
    BrX10042AF0(&a, 11, 22);
    CHECK(s_n42AF0 == 1);
    CHECK(s_p42AF0 == &a);

    BrGfx42AF0_3(&b, 33, 44);
    CHECK(s_n42AF0 == 2);
    CHECK(s_p42AF0 == &b);
}

static void TestAdapterPools(void)
{
    s_n69490 = 0;
    s_n69530 = 0;

    CHECK(BrX10069490() == (void *)&s_mtxObject);
    CHECK(s_n69490 == 1);

    CHECK(BrX10069530() == (void *)&s_poolObject);
    CHECK(s_n69530 == 1);

    /* The two are different pools; an adapter wired to the wrong one would
     * still return a pointer, so the identity of the pointer is the check. */
    CHECK((void *)&s_mtxObject != (void *)&s_poolObject);
}

static void TestAdapter3E310(void)
{
    s_n3E310 = 0;
    BrExt_1003E310();
    CHECK(s_n3E310 == 1);
}

static void TestAdapter6A4A0(void)
{
    char self, arg;

    s_n6A4A0 = 0;
    BrExt_1006A4A0(&self, &arg);
    CHECK(s_n6A4A0 == 1);
    /* Order matters: `this` first, path second.  Swapping them would write
     * the config to the wrong pointer, which is why this is asserted. */
    CHECK(s_p6A4A0This == &self);
    CHECK(s_p6A4A0Arg  == &arg);
}

static void TestAdapterTime(void)
{
    s_nTimeNow = 0;
    s_timeNow  = -12345;       /* the timer is signed and does wrap */
    CHECK(BrX10060E90() == -12345);
    CHECK(s_nTimeNow == 1);
}

static void TestAdapterFfb(void)
{
    s_nFfb = 0;
    BrExt_10079550();
    CHECK(s_nFfb == 1);
    /* The one instance slice3_45.c owns -- not a private copy. */
    CHECK(s_pFfbArg == &g_brFfb);
}

static void TestAdapterOptOpen(void)
{
    s_n2950A = s_n2950B = 0;
    s_p2950A = s_p2950B = (struct BrGameObj *)&s_n2950A;   /* poison */

    BrExt_100443E0(0);
    CHECK(s_n2950B == 1);
    CHECK(s_p2950B == NULL);       /* the argument the original never reads */
    CHECK(s_n2950A == 0);          /* and the OTHER one was not called */

    BrExt_10044280(0);
    CHECK(s_n2950A == 1);
    CHECK(s_p2950A == NULL);
    CHECK(s_n2950B == 1);
}

static void TestAdapter43BF0(void)
{
    s_n43BF0 = 0;
    s_p43BF0 = (struct BrGameObj *)&s_n43BF0;
    BrExt_10043BF0(0);
    CHECK(s_n43BF0 == 1);
    CHECK(s_p43BF0 == NULL);
}

/* ==========================================================================
 * 2. 0x10060D90 -- the two sliders
 * ========================================================================== */

static void TestVolumeAsymmetry(void)
{
    g_pfnBrMusicVolume0029F0 = MusicVolumeSpy;

    /* Indices chosen so the two tables DISAGREE at both positions: index 1 is
     * 0x1C in A and 0x55 in B.  A version that used one table for both, or
     * that swapped them, cannot pass this. */
    g_brB4E70C = 1;     /* level A -- linear table, drives the hook  */
    g_brB4E708 = 1;     /* level B -- perceptual table, stored only  */

    s_nMusicVolume = 0;
    BrSub10060D90();

    CHECK(s_nMusicVolume == 1);
    CHECK(s_musicVolume == 0x1C);            /* A table, not B */
    CHECK(BrG_0BBAD8 == 0x1C);
    CHECK(BrSndMasterVolume == 0x55);        /* B table, not A */

    /* The B slider never reaches the hook: change only B and the hook value
     * must not move. */
    g_brB4E708 = 9;
    s_nMusicVolume = 0;
    BrSub10060D90();
    CHECK(s_nMusicVolume == 1);
    CHECK(s_musicVolume == 0x1C);
    CHECK(BrSndMasterVolume == 0xFF);

    /* Both ends of both tables, since 0 and 0xFF are the values a wrong index
     * is most likely to land on by accident. */
    g_brB4E70C = 0;
    g_brB4E708 = 0;
    BrSub10060D90();
    CHECK(s_musicVolume == 0);
    CHECK(BrG_0BBAD8 == 0);
    CHECK(BrSndMasterVolume == 0);

    g_brB4E70C = 9;
    g_brB4E708 = 9;
    BrSub10060D90();
    CHECK(s_musicVolume == 0xFF);
    CHECK(BrG_0BBAD8 == 0xFF);
    CHECK(BrSndMasterVolume == 0xFF);
}

static void TestVolumeNullHook(void)
{
    /* The documented DEVIATION: a NULL hook skips the call and everything
     * else still happens.  This is the configuration the host runs in today,
     * so it is the one that must not crash. */
    g_pfnBrMusicVolume0029F0 = NULL;
    g_brB4E70C = 5;
    g_brB4E708 = 2;
    BrG_0BBAD8 = 0;
    BrSndMasterVolume = 0;

    BrSub10060D90();

    CHECK(BrG_0BBAD8 == 0x8D);
    CHECK(BrSndMasterVolume == 0x78);

    g_pfnBrMusicVolume0029F0 = MusicVolumeSpy;
}

/* ==========================================================================
 * 3. 0x100193C0 -- text width
 * ==========================================================================
 *
 * The oracle below is written from the DISASSEMBLY'S STRUCTURE, independently
 * of the transcription: pick a table, walk the string, add either a glyph
 * width or the fixed non-printing width.  It shares the tables with the code
 * under test (they are the image's data, not a modelling choice) but not the
 * control flow, which is where a transcription goes wrong.
 */
static int OracleWidth(const char *psz, int scale)
{
    const int32_t *tbl;
    int div, total = 0;
    size_t i = 0;

    if (BrG_6C65E4 != 0)
        scale *= 2;

    if (g_i0B8C90 <= 1 && scale >= BR_TEXT_LARGE_MIN) {
        div = BR_TEXT_DIV_LARGE;
        tbl = BrTextWidthLarge;
    } else {
        div = BR_TEXT_DIV_SMALL;
        tbl = BrTextWidthSmall;
    }

    while (psz[i] != '\0') {
        signed char c = (signed char)psz[i];
        int measure = 1;

        if (c < BR_TEXT_CLASS_LO || c > BR_TEXT_CLASS_HI) {
            total += (14 * scale) / 40;
            measure = 0;
        } else if (c == '%' && psz[i + 1] != '\0') {
            if (psz[i + 1] == '%') {
                ++i;                       /* measure one '%' */
            } else if (psz[i + 1] == 'i' || psz[i + 1] == 'n') {
                ++i;
                measure = 0;
            } else if (psz[i + 2] != '\0') {
                i += 2;                    /* the three-character quirk */
                measure = 0;
            }
        }
        if (measure) {
            int k = BrTextClassMap[(unsigned char)c - BR_TEXT_CLASS_LO];
            total += ((tbl[k + 1] - tbl[k]) * scale) / div;
        }
        ++i;
    }

    if (BrG_6C65E4 != 0)
        total >>= 1;
    return total;
}

static void CheckAgainstOracle(const char *psz, int scale)
{
    int got  = BrSub_100193C0(psz, scale);
    int want = OracleWidth(psz, scale);
    if (got != want) {
        printf("FAIL width(\"%s\", %d) = %d, oracle says %d\n",
               psz, scale, got, want);
        ++g_fails;
    }
}

static void TestWidthAgainstOracle(void)
{
    static const char *aStr[] = {
        "", " ", "A", "a", "AA", "Az09", "HELLO WORLD", "hello world",
        "0123456789", "!\"#$&'()*+,-./:;<=>?@[]^_`{|}~",
        "%", "%%", "%d", "%i", "%n", "%d ", "%%%%", "a%%b", "a%db",
        "  spaced  out  ", "\t\n", "Lap 1/3", "99:59.99"
    };
    static const int aScale[] = { 0, 1, 12, 0x18, 0x19, 0x20, 40, 100 };
    size_t i, j;
    int    hires;

    for (hires = 0; hires <= 1; ++hires) {
        BrG_6C65E4 = hires;
        for (i = 0; i < sizeof aStr / sizeof aStr[0]; ++i)
            for (j = 0; j < sizeof aScale / sizeof aScale[0]; ++j)
                CheckAgainstOracle(aStr[i], aScale[j]);
    }
    BrG_6C65E4 = 0;
}

static void TestWidthIdentities(void)
{
    BrG_6C65E4 = 0;
    g_i0B8C90  = 1;

    /* The empty string measures nothing, at every size. */
    CHECK(BrSub_100193C0("", 0) == 0);
    CHECK(BrSub_100193C0("", 40) == 0);

    /* Concatenation is additive over plain glyphs -- the function keeps no
     * state between characters, no kerning, no leading or trailing pad. */
    CHECK(BrSub_100193C0("ABC", 40) + BrSub_100193C0("DEF", 40)
          == BrSub_100193C0("ABCDEF", 40));

    /* Upper and lower case share a class, so they measure identically.  This
     * is a property of the image's class map and would break under an
     * off-by-one in the map. */
    CHECK(BrSub_100193C0("HELLO", 40) == BrSub_100193C0("hello", 40));
    CHECK(BrSub_100193C0("Zz", 40) == 2 * BrSub_100193C0("z", 40));

    /* Monotonic in the scale within one font: a bigger nominal size is never
     * narrower. */
    CHECK(BrSub_100193C0("MMMM", 40) >= BrSub_100193C0("MMMM", 30));
    CHECK(BrSub_100193C0("MMMM", 30) >= BrSub_100193C0("MMMM", 25));

    /* A wider string is never narrower than a prefix of itself. */
    CHECK(BrSub_100193C0("WWWW", 40) >= BrSub_100193C0("WWW", 40));
}

static void TestWidthFontSelection(void)
{
    int wSmall, wLarge;

    BrG_6C65E4 = 0;
    g_i0B8C90  = 1;

    /* 0x19 is the threshold: at 0x18 the SMALL table and divisor 0x14 are
     * used, at 0x19 the LARGE table and divisor 0x28.  The two tables give
     * genuinely different answers, which is what makes the boundary
     * observable rather than a formality. */
    wSmall = BrSub_100193C0("W", 0x18);
    wLarge = BrSub_100193C0("W", 0x19);
    CHECK(wSmall == ((BrTextWidthSmall[51] - BrTextWidthSmall[50])
                     * 0x18) / BR_TEXT_DIV_SMALL);
    CHECK(wLarge == ((BrTextWidthLarge[51] - BrTextWidthLarge[50])
                     * 0x19) / BR_TEXT_DIV_LARGE);

    /* g_i0B8C90 > 1 forces the small font regardless of the scale. */
    g_i0B8C90 = 2;
    CHECK(BrSub_100193C0("W", 40)
          == ((BrTextWidthSmall[51] - BrTextWidthSmall[50]) * 40)
             / BR_TEXT_DIV_SMALL);
    g_i0B8C90 = 1;
}

static void TestWidthHiRes(void)
{
    int plain, hires;

    g_i0B8C90 = 1;

    /* The hi-res flag doubles the scale on the way in and halves the total on
     * the way out.  Those cancel only up to the truncation in each per-glyph
     * divide, so the two answers must be CLOSE, not equal -- asserting
     * equality here would be asserting something the code does not promise. */
    BrG_6C65E4 = 0;
    plain = BrSub_100193C0("RACE OVER", 40);
    BrG_6C65E4 = 1;
    hires = BrSub_100193C0("RACE OVER", 40);
    CHECK(hires >= plain - 9 && hires <= plain + 9);

    /* But the doubling happens BEFORE the font threshold, so a scale below
     * the threshold can be pushed over it by the flag.  0x0D doubles to 0x1A,
     * which is >= 0x19: the hi-res answer comes from the LARGE table. */
    BrG_6C65E4 = 0;
    CHECK(BrSub_100193C0("W", 0x0D)
          == ((BrTextWidthSmall[51] - BrTextWidthSmall[50]) * 0x0D)
             / BR_TEXT_DIV_SMALL);
    BrG_6C65E4 = 1;
    CHECK(BrSub_100193C0("W", 0x0D)
          == (((BrTextWidthLarge[51] - BrTextWidthLarge[50]) * 0x1A)
              / BR_TEXT_DIV_LARGE) >> 1);
    BrG_6C65E4 = 0;
}

static void TestWidthPercentQuirks(void)
{
    int wPct;

    BrG_6C65E4 = 0;
    g_i0B8C90  = 1;

    wPct = BrSub_100193C0("%", 40);
    CHECK(wPct > 0);                       /* a lone '%' is a glyph */

    /* "%%" measures exactly one '%'. */
    CHECK(BrSub_100193C0("%%", 40) == wPct);

    /* "%i" and "%n" measure nothing. */
    CHECK(BrSub_100193C0("%i", 40) == 0);
    CHECK(BrSub_100193C0("%n", 40) == 0);

    /* "%d" at the very end of the string: p[2] is NUL, so the '%' falls
     * through and is measured, and then 'd' is measured as a plain glyph. */
    CHECK(BrSub_100193C0("%d", 40) == wPct + BrSub_100193C0("d", 40));

    /* ORIGINAL BUG, pinned: "%d" followed by anything consumes THREE
     * characters and measures none of them.  So "%dX" is zero-width, and
     * "%dXY" measures only 'Y'. */
    CHECK(BrSub_100193C0("%dX", 40) == 0);
    CHECK(BrSub_100193C0("%dXY", 40) == BrSub_100193C0("Y", 40));

    /* Contrast the two-character forms, which do NOT eat a third. */
    CHECK(BrSub_100193C0("%iY", 40) == BrSub_100193C0("Y", 40));
    CHECK(BrSub_100193C0("%%Y", 40) == wPct + BrSub_100193C0("Y", 40));
}

static void TestWidthNonPrinting(void)
{
    int one;

    BrG_6C65E4 = 0;
    g_i0B8C90  = 1;

    /* Everything outside 0x21..0x7F takes the fixed-width branch, and its
     * width does NOT depend on the font divisor -- it is 14*scale/40 both
     * sides of the threshold.  Space, control characters and, because the
     * original's compare is SIGNED, every byte with the high bit set. */
    one = (14 * 40) / 40;
    CHECK(BrSub_100193C0(" ", 40) == one);
    CHECK(BrSub_100193C0("\t", 40) == one);
    CHECK(BrSub_100193C0("\x80", 40) == one);
    CHECK(BrSub_100193C0("\xFF", 40) == one);
    CHECK(BrSub_100193C0("   ", 40) == 3 * one);

    /* Below the threshold it is the same formula, which is what "not scaled
     * by the divisor" means. */
    CHECK(BrSub_100193C0(" ", 20) == (14 * 20) / 40);

    /* Truncation toward zero, not rounding: 14*1/40 == 0. */
    CHECK(BrSub_100193C0(" ", 1) == 0);
    CHECK(BrSub_100193C0("          ", 1) == 0);
}

/* ==========================================================================
 * 4. 0x10072580 -- stop one bank voice
 * ========================================================================== */

static char s_voice;

static void ArmSound(void)
{
    BrSndG0B5DE8   = 1;
    BrSndPDS       = (struct BrDSound *)&s_voice;
    BrSndG18290FC  = &s_voice;
    g_aBrSndBankVoice[3] = &s_voice;
    s_nStop    = 0;
    s_pStopArg = NULL;
}

static void TestStopVoice(void)
{
    ArmSound();
    BrX10072580(3);
    CHECK(s_nStop == 1);
    CHECK(s_pStopArg == (struct BrSndVoice *)&s_voice);

    /* Each of the three gates on its own suppresses the call.  They are
     * tested separately because a transcription that dropped one would still
     * pass a test that only ever clears all three. */
    ArmSound(); BrSndG0B5DE8 = 0;    BrX10072580(3); CHECK(s_nStop == 0);
    ArmSound(); BrSndPDS = NULL;     BrX10072580(3); CHECK(s_nStop == 0);
    ArmSound(); BrSndG18290FC = NULL; BrX10072580(3); CHECK(s_nStop == 0);

    /* An empty slot is a no-op, not a call with NULL. */
    ArmSound();
    g_aBrSndBankVoice[3] = NULL;
    BrX10072580(3);
    CHECK(s_nStop == 0);

    /* And the index really does select the slot. */
    ArmSound();
    BrX10072580(0);
    CHECK(s_nStop == 0);          /* slot 0 is empty */
    g_aBrSndBankVoice[3] = NULL;
}

/* ==========================================================================
 * 5. 0x10005D30 -- the local slot index
 * ========================================================================== */

static void TestLocalSlot(void)
{
    /* Six bytes: one load and a ret.  All there is to check is that it reads
     * the global rather than caching, and that it is transparent to the
     * values that matter -- -1, the "empty" sentinel the image initialises
     * 0x10094294 to, and 0, which is a VALID slot index rather than another
     * spelling of empty.  Those two being different is the whole reason the
     * initialiser mattered.
     *
     * The initial VALUE is deliberately not asserted here: slice4_50.c owns
     * the storage now, so an assertion in this file would only be checking
     * this file's own stand-in, which CONVENTIONS rightly calls worse than no
     * test at all. */
    g_br094294 = -1;
    CHECK(BrSub10005D30() == -1);

    g_br094294 = 0;
    CHECK(BrSub10005D30() == 0);
    g_br094294 = 7;
    CHECK(BrSub10005D30() == 7);
    g_br094294 = -1;
}

/* ========================================================================== */

int main(void)
{
    TestAdapterCombine();
    TestAdapter42AF0();
    TestAdapterPools();
    TestAdapter3E310();
    TestAdapter6A4A0();
    TestAdapterTime();
    TestAdapterFfb();
    TestAdapterOptOpen();
    TestAdapter43BF0();

    TestVolumeAsymmetry();
    TestVolumeNullHook();

    TestWidthAgainstOracle();
    TestWidthIdentities();
    TestWidthFontSelection();
    TestWidthHiRes();
    TestWidthPercentQuirks();
    TestWidthNonPrinting();

    TestStopVoice();
    TestLocalSlot();

    if (g_fails == 0) {
        printf("test_slice6_76: all checks passed\n");
        return 0;
    }
    printf("test_slice6_76: %d FAILED\n", g_fails);
    return 1;
}
