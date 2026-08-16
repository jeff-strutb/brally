/* test_slice2_25.c -- behaviour tests for another module's packet.
 *
 * Everything asserted here is a property the ORIGINAL has, taken from the
 * disassembly, not a number chosen to match the port:
 *
 *   - the cyclers' wrap points, including the two that are not [0..max]:
 *     0x100BD3E0 is 1-based and 0x10AA2A0C skips the value 1
 *   - which cyclers call their side-effect function when NEITHER input is
 *     set (0x10042CF0 does, 0x10044600 does not) -- that asymmetry is the
 *     whole point of those two branches
 *   - the search cyclers terminate after exactly one full circle when every
 *     candidate is rejected, and leave the index where they found it
 *   - 0x100AC648's upper bound follows 0x10AA28FC (11 vs 14)
 *   - the 0x10AA28D8 latch makes the second and third toggle entry points
 *     no-ops
 *   - the screen installers construct at most once and republish on reuse
 *   - the DirectPlay paths: too-few-players, not-all-ready, and the one path
 *     that actually raises DPSESSION_JOINDISABLED
 *
 * All cross-slice calls are stubbed below; the stubs are recording fakes, so
 * the assertions are about control flow rather than about arithmetic the
 * port could get wrong in the same way twice.
 */
#include "slice2_25.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ======================================================================
 * Stand-in lookup tables.  MARKED CLEARLY: these are TEST DATA, not the
 * DLL's. Each is filled with a value that is easy to tell apart from its
 * index so "the derived global is table[index]" can be asserted.
 * ====================================================================== */
#define T24(base) { base+0,base+1,base+2,base+3,base+4,base+5,base+6,base+7, \
                    base+8,base+9,base+10,base+11,base+12,base+13,base+14,base+15, \
                    base+16,base+17,base+18,base+19,base+20,base+21,base+22,base+23 }

const int32_t g_aBrAC308[24] = T24(30800);
const int32_t g_aBrAC368[16] = { 36800,36801,36802,36803,36804,36805,36806,36807,
                                 36808,36809,36810,36811,36812,36813,36814,36815 };
const int32_t g_aBrAC3B0[6]  = { 3300,3301,3302,3303,3304,3305 };
const int32_t g_aBrAC3C8[5]  = { 3380,3381,3382,3383,3384 };
const int32_t g_aBrAC420[32] = T24(4200);
const int32_t g_aBrAC4A0[4]  = { 4700,4701,4702,4703 };
const int32_t g_aBrAC4B0[4]  = { 4800,4801,4802,4803 };
const int32_t g_aBrAC4C0[6]  = { 4900,4901,4902,4903,4904,4905 };
/* 0x100AC4D8 must be big enough for the 0..14 range 0x10042EE0 can reach. */
const int32_t g_aBrAC4D8[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
const int32_t g_aBrAC518[2]  = { 5180,5181 };
const int32_t g_aBrAC520[4]  = { 0,1,2,3 };
const int32_t g_aBrAC530[2]  = { 5300,5301 };
const int32_t g_aBrAC538[2]  = { 5380,5381 };
const int32_t g_aBrAC540[2]  = { 5400,5401 };
const int32_t g_aBrAC548[2]  = { 5480,5481 };

static const BrRec2A8 s_recPlain  = { 0, 0x00, 0, 0, 0 };
static const BrRec2A8 s_recFlagged= { 0, 0x10, 0, 0, 0 };
const BrRec2A8 *const g_aBrBD2A8[16] = {
    &s_recPlain, &s_recFlagged, &s_recPlain, &s_recPlain,
    &s_recPlain, &s_recPlain,   &s_recPlain, &s_recPlain,
    &s_recPlain, &s_recPlain,   &s_recPlain, &s_recPlain,
    &s_recPlain, &s_recPlain,   &s_recPlain, &s_recPlain
};

unsigned char g_aBrB4FBE8[16];

/* ======================================================================
 * Cross-slice stand-ins.  ALL FAKE.
 * ====================================================================== */

static int  s_n60D90, s_n44540, s_n1005FCF0, s_n3C020, s_n3C1E0, s_n3C260ret;
static int  s_n3C150, s_n3CDA0, s_n3C230, s_n3CE80, s_n3D9F0, s_n3D950;
static int  s_n3D210, s_nGlobalUnlock, s_nGlobalFree, s_nCtor;
static int  s_nSetSessionDesc;
static uint32_t s_lastSessionFlags;
static BrDPSessionDesc *s_pDescToHandOut;

/* Bitmask of indices the two "is this selectable" predicates accept. */
static uint32_t s_okMask3F320, s_okMask3F2B0;

char *BrItoa(int value, char *pszBuf, int radix)
{
    /* every call site in this packet uses radix 10 */
    if (radix == 10) sprintf(pszBuf, "%d", value);
    else             sprintf(pszBuf, "%x", (unsigned)value);
    return pszBuf;
}

int BrSprintf(char *pszDest, const char *pszFmt, ...)
{
    int n;
    va_list ap;
    va_start(ap, pszFmt);
    n = vsprintf(pszDest, pszFmt, ap);
    va_end(ap);
    return n;
}

/* The fake string table renders every id as "<id>" so the assertions can
 * check WHICH id was looked up. Format ids get a "%s" so BrSprintf works. */
static char s_strbuf[8][64];
static int  s_strslot;
const char *BrStrGet(int id)
{
    char *p = s_strbuf[s_strslot++ & 7];
    switch (id) {
    case BR_OPT_STR_TRACK:  strcpy(p, "T:%s");  break;
    case BR_OPT_STR_CAR:    strcpy(p, "C:%s");  break;
    case BR_OPT_STR_BD3E0:  strcpy(p, "N:%s");  break;
    case BR_OPT_STR_AA2A00: strcpy(p, "A:%s");  break;
    case BR_OPT_STR_AA2A18: strcpy(p, "B:%s");  break;
    case BR_OPT_STR_LOCKED: strcpy(p, "*");     break;
    default:                sprintf(p, "<%d>", id); break;
    }
    return p;
}

void BrSub10038F30(int a)               { (void)a; }
void BrSub1003BF60(void)                { }
void BrSub1003C020(void)                { ++s_n3C020; }
void BrSub1003C150(void)                { ++s_n3C150; }
void BrSub1003C1E0(void)                { ++s_n3C1E0; }
void BrSub1003C230(void)                { ++s_n3C230; }
int  BrSub1003C260(void)                { return s_n3C260ret; }
void BrSub1003CDA0(void)                { ++s_n3CDA0; }
void BrSub1003CE80(void)                { ++s_n3CE80; }
void BrSub1003D0B0(BrDPlay *p, BrDPSessionDesc **pp) { (void)p; *pp = s_pDescToHandOut; }
void BrSub1003D210(void *a, BrOptUi *b, int c) { (void)a; (void)b; (void)c; ++s_n3D210; }
void BrSub1003D950(BrOptUi *p, int a)   { (void)p; (void)a; ++s_n3D950; }
void BrSub1003D9F0(BrOptUi *p)          { (void)p; ++s_n3D9F0; }
void BrSub1003DA40(BrOptUi *p, int a)   { p->f08 = a; }
void BrSub1003E310(void)                { }
void BrSub1003E680(void)                { }
int  BrSub1003F2B0(int i) { return (i >= 0 && i < 32) ? (int)((s_okMask3F2B0 >> i) & 1u) : 0; }
int  BrSub1003F320(int i) { return (i >= 0 && i < 32) ? (int)((s_okMask3F320 >> i) & 1u) : 0; }
void BrSub10041B50(void)                { }
void BrSub10043BF0(BrGameObj *p)        { (void)p; }
void BrSub10044540(void)                { ++s_n44540; }
void BrSub10046400(BrGameObj *p)        { (void)p; }
void BrSub10047360(BrGameObj *p)        { (void)p; }
int  BrSub10058700(void)                { return 77; }
void BrSub100586A0(void)                { int i; for (i = 0; i < BR_SLOT_COUNT; ++i)
                                          { g_aBrAA2538[i].id = BR_SLOT_EMPTY;
                                            g_aBrAA2538[i].a = 0; g_aBrAA2538[i].b = 0; } }
void BrSub1005FCF0(void)                { ++s_n1005FCF0; }
void BrSub10060D90(void)                { ++s_n60D90; }
void BrSub1006A4A0(void *a, void *b)    { (void)a; (void)b; }
void BrSub10071130(int a, int b)        { (void)a; (void)b; }
void BrSub10072AF0(int a, int b)        { (void)a; (void)b; }

/* Slot +0x00 is the MSVC scalar deleting destructor and RETURNS `this`
 * (0x10048850 ends `mov eax,esi / ret 4`). This range discards the result.
 * The table has nine slots (br_phase.h BrPhaseVtbl_); only +0x00 is driven
 * here, and the rest are left NULL so a stray call faults rather than
 * running into whatever followed the old one-slot model. */
static void *fakeDtor(BrOptObj *pThis, int fDelete)
{
    (void)fDelete;
    return pThis;
}
static const BrOptObjVtbl s_optVtbl = {
    fakeDtor,               /* +0x00 */
    NULL, NULL, NULL,       /* +0x04 +0x08 +0x0C */
    NULL, NULL, NULL,       /* +0x10 +0x14 +0x18 */
    NULL, NULL              /* +0x1C +0x20 */
};

BrOptObj *BrOptObjCtor(BrOptObj *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl = &s_optVtbl;
    ++s_nCtor;
    return pThis;
}

/* Handler installed into pfn04: records which one ran, and on whom. */
static int       s_nHandler;
static BrOptObj *s_pHandlerThis;
static void handler(BrOptObj *pThis) { ++s_nHandler; s_pHandlerThis = pThis; }

void BrOptFn1004CAC0(BrOptObj *p) { handler(p); }
void BrOptFn10051990(BrOptObj *p) { handler(p); }
void BrOptFn10051D30(BrOptObj *p) { handler(p); }
void BrOptFn100558A0(BrOptObj *p) { handler(p); }
void BrOptFn10056A10(BrOptObj *p) { handler(p); }
void BrOptFn10056FF0(BrOptObj *p) { handler(p); }
void BrOptFn100575F0(BrOptObj *p) { handler(p); }
void BrOptFn10057C10(BrOptObj *p) { handler(p); }
void BrOptFn10058750(BrOptObj *p) { handler(p); }
/* An ENTITY record, not the screen object -- see the adjudication in
 * slice2_25.h. These are only stored and compared here, never called. */
void BrOptFn10044970(void *pEntity) { (void)pEntity; }
void BrOptFn10044A30(void *pEntity) { (void)pEntity; }

void *BrGlobalHandle(void *p) { return p; }
int   BrGlobalUnlock(void *h) { (void)h; ++s_nGlobalUnlock; return 0; }
void *BrGlobalFree(void *h)   { (void)h; ++s_nGlobalFree; return NULL; }

/* ======================================================================
 * Fixtures
 * ====================================================================== */

static long fakeSetSessionDesc(BrDPlay *pThis, BrDPSessionDesc *pDesc, uint32_t f)
{
    (void)pThis; (void)f;
    ++s_nSetSessionDesc;
    s_lastSessionFlags = pDesc->dwFlags;
    return 0;
}
static const BrDPlayVtbl s_dpVtbl = { { 0 }, fakeSetSessionDesc };
static BrDPlay           s_dp     = { &s_dpVtbl };

static BrOptUi s_ui;

static void reset(void)
{
    int i;

    g_brAA33D4 = g_brAA33D0 = 0;
    g_br0AC648 = g_br0AC64C = g_br0AC650 = g_br0AC654 = 0;
    g_br0AC658 = g_br0AC65C = 0;
    g_br0BD3E0 = BR_OPT_BD3E0_MIN;
    g_brAA2A00 = g_brAA2A08 = g_brAA2A0C = g_brAA2A18 = 0;
    g_brAA2A1C = g_brAA2A20 = g_brAA2A24 = g_brAA2A28 = 0;
    g_brB4E708 = g_brB4E70C = 0;
    g_br0AA010 = g_br0AB3D8 = g_br0AB3E0 = g_br0B4050 = g_br22AF18 = 0;
    g_br690A18 = g_brA9CFFC = g_brA9D000 = 0;
    g_brAA2854 = g_brAA285C = g_brAA2878 = g_brAA287C = g_brAA2884 = 0;
    g_brAA2888 = g_brAA288C = g_brAA2890 = g_brAA2894 = g_brAA2898 = 0;
    g_brAA289C = g_brAA28D8 = g_brAA28E8 = g_brAA28FC = 0;
    g_brAA2958 = g_brAA29A8 = 0;
    g_brP277B40 = NULL;
    g_brPA9D008 = &s_ui;
    g_brP680584 = NULL;
    g_brPAA29D8 = NULL;
    g_brPAA29D4 = NULL;
    g_brPAA2904 = g_brPAA2908 = g_brPAA2940 = g_brPAA2948 = NULL;
    g_brPAA294C = g_brPAA2950 = g_brPAA2954 = g_brPAA296C = NULL;
    g_brPAA2970 = g_brPAA298C = g_brPAA2998 = g_brPAA29B8 = NULL;
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        g_aBrAA2538[i].id = BR_SLOT_EMPTY;
        g_aBrAA2538[i].a = g_aBrAA2538[i].b = 0;
    }
    memset(g_aBrA9CDF0, 0, sizeof(g_aBrA9CDF0));
    memset(g_aBrA9DD28, 0, sizeof(g_aBrA9DD28));
    memset(g_aBr39B720, 0, sizeof(g_aBr39B720));
    memset(g_aBr1782BC8, 0, sizeof(g_aBr1782BC8));
    s_n60D90 = s_n44540 = s_n1005FCF0 = s_n3C020 = s_n3C1E0 = 0;
    s_n3C150 = s_n3CDA0 = s_n3C230 = s_n3CE80 = s_n3D9F0 = s_n3D950 = 0;
    s_n3D210 = s_nGlobalUnlock = s_nGlobalFree = s_nCtor = s_nHandler = 0;
    s_nSetSessionDesc = 0;
    s_lastSessionFlags = 0;
    s_n3C260ret = 1;
    s_pDescToHandOut = NULL;
    s_okMask3F320 = 0xFFFFFFFFu;
    s_okMask3F2B0 = 0xFFFFFFFFu;
    s_pHandlerThis = NULL;
}

static void up(void)   { g_brAA33D4 = 1; g_brAA33D0 = 0; }
static void down(void) { g_brAA33D4 = 0; g_brAA33D0 = 1; }
static void idle(void) { g_brAA33D4 = 0; g_brAA33D0 = 0; }

/* ======================================================================
 * Tests
 * ====================================================================== */

static void test_plain_cycler(void)
{
    int i;

    printf("plain cycler (0x10042C80, 0x100AC65C, 0..7)\n");

    reset();
    idle();
    g_br0AC65C = 3;
    check(BrOptCycleAC65C() == 1, "returns 1");
    check(g_br0AC65C == 3, "no input leaves the index alone");
    check(g_br094350 == 3, "derived value is republished even when idle");

    reset();
    up();
    for (i = 0; i < BR_OPT_AC65C_MAX; ++i) BrOptCycleAC65C();
    check(g_br0AC65C == BR_OPT_AC65C_MAX, "steps up to the maximum");
    BrOptCycleAC65C();
    check(g_br0AC65C == 0, "wraps max -> 0");

    reset();
    down();
    BrOptCycleAC65C();
    check(g_br0AC65C == BR_OPT_AC65C_MAX, "wraps 0 -> max");

    reset();
    g_brAA33D4 = 1;
    g_brAA33D0 = 1;
    g_br0AC65C = 4;
    BrOptCycleAC65C();
    check(g_br0AC65C == 5, "0x10AA33D4 wins when both inputs are set");

    /* the table-mapped variants publish table[index], not the index */
    reset();
    up();
    BrOptCycleAC64C();
    check(g_br0AC64C == 1 && g_br09435C == g_aBrAC4A0[1],
          "0x10042DC0 publishes table[index]");
    reset();
    up();
    BrOptCycleAA2A08();
    check(g_brAA2A08 == 1 && g_br094354 == g_aBrAC518[1],
          "0x10042E80 wraps at 1 and publishes table[index]");
    up();
    BrOptCycleAA2A08();
    check(g_brAA2A08 == 0, "0x10042E80 is a two-state option");
}

static void test_one_based_cycler(void)
{
    printf("1-based cycler (0x100430B0, 0x100BD3E0, 1..12)\n");

    reset();
    up();
    g_br0BD3E0 = BR_OPT_BD3E0_MAX;
    BrOptCycleBD3E0();
    check(g_br0BD3E0 == BR_OPT_BD3E0_MIN, "wraps 12 -> 1, NOT to 0");

    reset();
    down();
    g_br0BD3E0 = BR_OPT_BD3E0_MIN;
    BrOptCycleBD3E0();
    check(g_br0BD3E0 == BR_OPT_BD3E0_MAX, "wraps 1 -> 12");

    /* 0 must be unreachable in either direction from any legal value */
    {
        int v, seenZero = 0;
        reset();
        up();
        for (v = 0; v < 40; ++v) { BrOptCycleBD3E0(); if (g_br0BD3E0 == 0) seenZero = 1; }
        down();
        for (v = 0; v < 40; ++v) { BrOptCycleBD3E0(); if (g_br0BD3E0 == 0) seenZero = 1; }
        check(!seenZero, "0 is never produced");
    }

    reset();
    idle();
    g_br0BD3E0 = 5;
    BrOptCycleBD3E0();
    check(g_br0AC658 == 5, "0x100AC658 mirrors the option");

    /* the message is only formatted when DirectPlay is present */
    reset();
    idle();
    g_br0BD3E0 = 9;
    g_brP277B40 = &s_dp;
    BrOptCycleBD3E0();
    check(s_n3D210 == 1, "message is emitted when 0x10277B40 is set");
    reset();
    idle();
    BrOptCycleBD3E0();
    check(s_n3D210 == 0, "no message when 0x10277B40 is NULL");
}

static void test_skip_one_cycler(void)
{
    printf("skip-1 cycler (0x10043400, 0x10AA2A0C)\n");

    reset();
    up();
    g_brAA2A0C = 0;
    BrOptCycleAA2A0C();
    check(g_brAA2A0C == 2, "stepping up from 0 skips 1 and lands on 2");
    BrOptCycleAA2A0C();
    check(g_brAA2A0C == 3, "2 -> 3");
    BrOptCycleAA2A0C();
    check(g_brAA2A0C == 0, "3 wraps to 0");

    reset();
    down();
    g_brAA2A0C = 2;
    BrOptCycleAA2A0C();
    check(g_brAA2A0C == 0, "stepping down from 2 skips 1 and lands on 0");
    BrOptCycleAA2A0C();
    check(g_brAA2A0C == 3, "0 wraps to 3");

    {
        int v, seenOne = 0;
        reset();
        up();
        for (v = 0; v < 30; ++v) { BrOptCycleAA2A0C(); if (g_brAA2A0C == 1) seenOne = 1; }
        down();
        for (v = 0; v < 30; ++v) { BrOptCycleAA2A0C(); if (g_brAA2A0C == 1) seenOne = 1; }
        check(!seenOne, "1 is never selected in either direction");
    }

    /* the record pointer follows the table VALUE, with everything outside
     * 1..3 folded onto record 0 */
    reset();
    idle();
    g_brAA2A0C = 0;
    BrOptCycleAA2A0C();
    check(g_brB4E1D4 == (void *)g_aBrB4DF30[0], "value 0 selects record 0");
    check(g_brB4E728 == 0, "0x10B4E728 holds the raw index");
    idle();
    g_brAA2A0C = 3;
    BrOptCycleAA2A0C();
    check(g_brB4E1D4 == (void *)g_aBrB4DF30[3], "value 3 selects record 3");
}

static void test_side_effect_asymmetry(void)
{
    printf("side-effect asymmetry (0x10042CF0 vs 0x10044600)\n");

    reset();
    idle();
    BrOptCycleB4E708();
    check(s_n60D90 == 1, "0x10042CF0 calls 0x10060D90 even with no input");
    check(g_br0AB3D8 == 1, "0x10042CF0 raises 0x100AB3D8");

    reset();
    idle();
    BrOptCycleB4E70C();
    check(s_n60D90 == 1, "0x10042D60 calls 0x10060D90 even with no input");
    check(g_br0AB3D8 == 0, "0x10042D60 clears 0x100AB3D8");

    reset();
    idle();
    BrOptCycleAA2A18();
    check(s_n44540 == 0, "0x10044600 does NOT call 0x10044540 with no input");
    up();
    BrOptCycleAA2A18();
    check(s_n44540 == 1, "0x10044600 calls 0x10044540 once edited");
    check(g_brAA2A18 == 1, "0x10AA2A18 stepped");

    reset();
    up();
    g_brAA2A18 = BR_OPT_AA2A18_MAX;
    BrOptCycleAA2A18();
    check(g_brAA2A18 == 0, "0x10AA2A18 wraps 4 -> 0");
    down();
    BrOptCycleAA2A18();
    check(g_brAA2A18 == BR_OPT_AA2A18_MAX, "0x10AA2A18 wraps 0 -> 4");
}

static void test_search_cyclers(void)
{
    printf("search cyclers (0x10042B30 track, 0x10042EE0 car)\n");

    /* Nothing selectable: the search makes exactly one circle and stops on
     * the value it started the circle FROM -- which is the first candidate,
     * i.e. the original index already stepped once, not the original index.
     * (The `cmp eax,esi` is against the post-step value, not the entry
     * value.) So a rejected-everything step still moves the option by one. */
    reset();
    up();
    s_okMask3F320 = 0;
    g_br0AC654 = 5;
    check(BrOptCycleTrack() == 1, "track cycler returns 1");
    check(g_br0AC654 == 6, "nothing selectable -> stops on the first candidate");

    reset();
    down();
    s_okMask3F320 = 0;
    g_br0AC654 = 5;
    BrOptCycleTrack();
    check(g_br0AC654 == 4, "same downward");

    /* Only index 9 selectable: from 5, stepping up must land on 9. */
    reset();
    up();
    s_okMask3F320 = 1u << 9;
    g_br0AC654 = 5;
    BrOptCycleTrack();
    check(g_br0AC654 == 9, "skips rejected indices upward");

    reset();
    down();
    s_okMask3F320 = 1u << 2;
    g_br0AC654 = 5;
    BrOptCycleTrack();
    check(g_br0AC654 == 2, "skips rejected indices downward");

    /* Wrap across the top of the 32-entry range. */
    reset();
    up();
    s_okMask3F320 = 1u << 1;
    g_br0AC654 = BR_OPT_TRACK_MAX;
    BrOptCycleTrack();
    check(g_br0AC654 == 1, "search wraps past index 0x1F");

    reset();
    idle();
    g_br0AC654 = 7;
    BrOptCycleTrack();
    check(g_br22B34C == g_aBrAC420[7], "track publishes table[index]");

    /* 0x100AC648's ceiling follows 0x10AA28FC. */
    reset();
    up();
    g_brAA28FC = 0;
    g_br0AC648 = BR_OPT_AC648_MAX_BASE;
    BrOptCycleCar();
    check(g_br0AC648 == 0, "car wraps at 11 when 0x10AA28FC is clear");

    reset();
    up();
    g_brAA28FC = 1;
    g_br0AC648 = BR_OPT_AC648_MAX_BASE;
    BrOptCycleCar();
    check(g_br0AC648 == BR_OPT_AC648_MAX_BASE + 1,
          "car goes past 11 when 0x10AA28FC is set");

    reset();
    up();
    g_brAA28FC = 1;
    g_br0AC648 = BR_OPT_AC648_MAX_EXTRA;
    BrOptCycleCar();
    check(g_br0AC648 == 0, "car wraps at 14 when 0x10AA28FC is set");

    reset();
    up();
    s_okMask3F2B0 = 0;
    g_br0AC648 = 4;
    BrOptCycleCar();
    check(g_br0AC648 == 5, "car: nothing selectable -> first candidate");

    /* The flagged record appends one extra string to the message. */
    reset();
    idle();
    g_brP277B40 = &s_dp;
    g_br0AC648 = 1;                       /* table value 1 -> s_recFlagged */
    BrOptCycleCar();
    check(strchr(g_aBrA9DD28, '*') == NULL,
          "message buffer is cleared after being shown");
    check(s_n3D210 == 1, "car message went out once");
}

static void test_toggle_latch(void)
{
    static BrGameObj s_game;

    printf("toggle latch (0x10042A90 / 0x10042AC0 / 0x10042B00)\n");

    reset();
    memset(&s_game, 0, sizeof(s_game));
    check(BrOptToggle2F7C_A(&s_game) == 1, "returns 1");
    check(s_game.f2F7C == 1, "first call toggles 0 -> 1");
    check(g_brAA28D8 == 1, "the latch is raised");

    BrOptToggle2F7C_B(&s_game);
    BrOptToggle2F7C_C(&s_game);
    BrOptToggle2F7C_A(&s_game);
    check(s_game.f2F7C == 1,
          "the other two entry points are no-ops while the latch is up");

    g_brAA28D8 = 0;
    BrOptToggle2F7C_B(&s_game);
    check(s_game.f2F7C == 0, "clearing the latch re-enables the toggle");
}

static void test_screen_installer(void)
{
    BrOptObj *p;

    printf("screen installers (0x10043260 and friends)\n");

    reset();
    check(BrOptOpen296C(NULL) == 1, "returns 1");
    p = g_brPAA296C;
    check(p != NULL, "the object was created");
    check(s_nCtor == 1, "the constructor ran once");
    check(s_nHandler == 1 && s_pHandlerThis == p, "pfn04 ran on the object");
    check(g_brPAA2904 == p, "the object was published in 0x10AA2904");
    check(p->f0C == 1 && p->f68 == 1, "+0x0C and +0x68 were raised");

    g_brPAA2904 = NULL;
    check(BrOptOpen296C(NULL) == 1, "second call also returns 1");
    check(s_nCtor == 1, "no second construction");
    check(s_nHandler == 1, "pfn04 is not re-run on reuse");
    check(g_brPAA2904 == p, "reuse still republishes 0x10AA2904");
    check(g_brPAA296C == p, "the slot is unchanged");
    free(p);

    /* 0x10043E70's tail runs on BOTH the create and the reuse path. */
    reset();
    g_brA9CFFC = 0; g_brA9D000 = 0; g_brAA287C = 1;
    BrOptOpen2948(NULL);
    check(s_n3C020 == 1, "0x10043E70 runs its tail on the create path");
    BrOptOpen2948(NULL);
    check(s_n3C020 == 2, "0x10043E70 runs its tail on the reuse path too");
    g_brAA287C = 2;
    BrOptOpen2948(NULL);
    check(s_n3C020 == 2, "and not when 0x10AA287C is 2");
    free(g_brPAA2948);
}

static void test_time_attack(void)
{
    int32_t idx;

    printf("0x10042880 -- time-attack setup\n");

    reset();
    g_br680738 = -1;
    idx = 7;
    check(BrOptBeginTimeAttack(NULL, &idx) == 0,
          "returns 0 when (signed char)0x10680738 is negative");
    check(s_n1005FCF0 == 0, "and does not run the tail");
    check(g_br0AA010 == 2, "but the head has already run");

    reset();
    g_br680738 = 3;
    g_br68073F = 2;
    g_brAD0978 = 1; g_brAD097C = 2; g_brAD0980 = 1;
    g_brAD0984 = 9; g_brAD0988 = 5; g_brAD098C = 4;
    {
        static int32_t s_src[BR_OPT_AA26F0_COUNT];
        int i;
        for (i = 0; i < BR_OPT_AA26F0_COUNT; ++i) s_src[i] = 1000 + i;
        g_brPACED34 = s_src;
        idx = 7;
        check(BrOptBeginTimeAttack(NULL, &idx) == 1, "returns 1");
        check(strcmp(g_aBr1782BC8, "TimeAttack7.grf") == 0,
              "builds \"TimeAttack<n>.grf\" into 0x11782BC8");
        check(g_aBrAA26F0[0] == 1000 &&
              g_aBrAA26F0[BR_OPT_AA26F0_COUNT - 1] == 1000 + BR_OPT_AA26F0_COUNT - 1,
              "copies 0x53 dwords from 0x10ACED34");
    }
    check(g_br0AC648 == 3, "0x100AC648 takes the signed byte at 0x10680738");
    check(g_brAA2A00 == 2, "0x10AA2A00 takes the signed byte at 0x1068073F");
    check(g_br0AC654 == 9 && g_br22B34C == g_aBrAC420[9],
          "0x100AC654 restored and mapped");
    check(g_br0AC658 == 4 && g_br0BD3E0 == 4, "both take 0x10AD098C");
    check(g_br094350 == 5, "0x10094350 takes 0x10AD0988 directly");
    check(g_brAA28E8 == 1 && g_brAA289C == 1, "the two done flags are raised");
    check(s_n1005FCF0 == 1, "0x1005FCF0 ran");

    /* three digits still fit between the two overlapping buffers */
    reset();
    g_br680738 = 0;
    g_brPACED34 = g_aBrAA26F0;
    idx = 123;
    BrOptBeginTimeAttack(NULL, &idx);
    check(strcmp(g_aBr1782BC8, "TimeAttack123.grf") == 0,
          "three-digit index still fits the original's frame layout");
}

static void test_lobby_slot_update(void)
{
    static BrGameObj      s_game;
    static BrDPSessionDesc s_desc;

    printf("0x10043810 -- lobby slot update\n");

    reset();
    memset(&s_game, 0, sizeof(s_game));
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 3;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 1;
    g_brAA288C  = 0;
    g_aBrAA2538[2].id = 41;
    s_ui.f08 = 41;

    check(BrOpt3810(&s_game) == 1, "returns 1 when 0x10AA288C is clear");
    check(g_aBrAA2538[2].a == 1, "slot.a set when dwCurrentPlayers > 1");
    check(s_nGlobalUnlock == 1 && s_nGlobalFree == 1,
          "the session descriptor is unlocked and freed");

    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 1;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 1;
    g_aBrAA2538[2].id = 41;
    g_aBrAA2538[2].a  = 1;
    s_ui.f08 = 41;
    BrOpt3810(&s_game);
    check(g_aBrAA2538[2].a == 0, "slot.a cleared when dwCurrentPlayers == 1");

    /* no slot carries the UI's id -> nothing is written at all */
    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 9;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 1;
    s_ui.f08 = 4242;
    BrOpt3810(&s_game);
    {
        int i, any = 0;
        for (i = 0; i < BR_SLOT_COUNT; ++i) if (g_aBrAA2538[i].a) any = 1;
        check(!any, "no matching slot -> no slot is touched");
    }
    check(s_nGlobalFree == 1, "the descriptor is still released");
}

static void test_start_race(void)
{
    static BrDPSessionDesc s_desc;

    printf("0x10043A00 -- start the race\n");

    /* fewer than two players */
    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 1;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    check(BrOpt3A00() == 1, "always returns 1");
    check(s_nSetSessionDesc == 0, "no SetSessionDesc with one player");
    check(s_n3D210 == 1, "the message went out");
    check(s_nGlobalFree == 1, "descriptor released");

    /* enough players, but an occupied slot is not ready */
    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 4;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 1;
    g_aBrAA2538[3].id = 12;      /* occupied ... */
    g_aBrAA2538[3].a  = 0;       /* ... and not ready */
    BrOpt3A00();
    check(s_nSetSessionDesc == 0, "an unready slot blocks the start");
    check(g_brAA288C == 0, "0x10AA288C is not raised");

    /* an EMPTY slot with a == 0 must NOT block */
    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 4;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 1;
    BrOpt3A00();
    check(s_nSetSessionDesc == 1, "all-empty slots do not block the start");
    check((s_lastSessionFlags & 0x20u) != 0,
          "DPSESSION_JOINDISABLED was raised before SetSessionDesc");
    check(g_brAA288C == 1, "0x10AA288C raised");
    check(s_n3D9F0 == 1, "0x1003D9F0 ran");
    check(s_nGlobalUnlock == 1 && s_nGlobalFree == 1, "descriptor released");

    /* not a networked session: the other branch entirely */
    reset();
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.dwCurrentPlayers = 4;
    s_pDescToHandOut = &s_desc;
    g_brP277B40 = &s_dp;
    g_brAA2884  = 0;
    BrOpt3A00();
    check(s_n3D950 == 1 && s_nSetSessionDesc == 0,
          "0x10AA2884 clear takes the 0x1003D950 branch");

    /* no descriptor at all: bail out without touching the Global* pair */
    reset();
    s_pDescToHandOut = NULL;
    g_brP277B40 = &s_dp;
    check(BrOpt3A00() == 1, "returns 1 with no descriptor");
    check(s_nGlobalFree == 0, "and does not free anything");
}

static void test_open_2950(void)
{
    printf("0x10044280 / 0x100443E0 -- the two 0x10AA2950 entries\n");

    reset();
    g_brAA2878  = 1;             /* skip the gate entirely */
    g_brPAA29B8 = (BrOptObj *)calloc(1, sizeof(BrOptObj));
    check(BrOptOpen2950A(NULL) == 1, "single-player entry returns 1");
    check(g_brAA2884 == 0, "0x10AA2884 cleared");
    check(g_brPAA29B8->pfnHook == BrOptFn10044970, "installs 0x10044970");
    free(g_brPAA2950);
    free(g_brPAA29B8);

    reset();
    g_brPAA29B8 = (BrOptObj *)calloc(1, sizeof(BrOptObj));
    check(BrOptOpen2950B(NULL) == 1, "networked entry returns 1");
    check(g_brAA2884 == 1, "0x10AA2884 raised");
    check(g_br22AF18 == 2, "0x1022AF18 set to 2");
    check(g_brPAA29B8->pfnHook == BrOptFn10044A30, "installs 0x10044A30");
    free(g_brPAA2950);
    free(g_brPAA29B8);

    /* mode 2 needs at least seven characters at 0x10A9CDF0 */
    reset();
    g_brAA2878 = 0;
    g_brAA287C = 2;
    strcpy(g_aBrA9CDF0, "short");
    check(BrOptOpen2950A(NULL) == 1, "short name still returns 1");
    check(g_brPAA2950 == NULL, "but no screen is opened");
    check(g_brAA2898 == 1, "0x10AA2898 was raised before the length test");

    reset();
    g_brAA2878 = 0;
    g_brAA287C = 2;
    strcpy(g_aBrA9CDF0, "longenough");
    g_brPAA29D8 = NULL;
    check(BrOptOpen2950A(NULL) == 1, "long name, no 0x10AA29D8");
    check(s_n3C1E0 == 1, "falls back to 0x1003C1E0");
    check(g_brPAA2950 == NULL, "still no screen");
}

int main(void)
{
    printf("test_slice2_25\n");
    test_plain_cycler();
    test_one_based_cycler();
    test_skip_one_cycler();
    test_side_effect_asymmetry();
    test_search_cyclers();
    test_toggle_latch();
    test_screen_installer();
    test_time_attack();
    test_lobby_slot_update();
    test_start_race();
    test_open_2950();
    printf(g_fail ? "FAILED\n" : "ALL PASS\n");
    return g_fail;
}
