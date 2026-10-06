/* test_slice8_87.c -- the slice2_23 family over br_ui.h's canonical control.
 *
 * WHAT THIS TEST IS FOR
 *
 * Fourteen hook bodies, none of them longer than a screen, every one reached
 * only through a function-pointer slot a builder filled.  "It links" says
 * nothing about any of them.  Every fixture below is built so that the WRONG
 * answer is a DIFFERENT observable value, not merely an absent one -- for the
 * six places where a plausible rewrite would smooth the original away:
 *
 *   1. 0x1003EF90 / 0x1003F210 compare CASE-INSENSITIVELY (_stricmp, not
 *      strcmp -- slice8_87.h CONFLICT 1).  The fixture offers a caption that
 *      differs from the stored one only in case; a strcmp build copies and
 *      this test fails.
 *   2. 0x10AA28B8 is SIGNED (CONFLICT 2).  The fixture uses -1 with a base of
 *      12, so the right reading indexes record 0 and an unsigned reading
 *      indexes record 3072 -- out of range, no caption at all.
 *   3. 0x1003F5E0's out-of-range default is 0x56, the SAME code index 0
 *      produces, while its twin 0x1003F680 sends BOTH index 0 and
 *      out-of-range to 0xFFFF.
 *   4. 0x1003FCB0's fallback is the LITERAL id 0x74 and not a table read.
 *      The fixture leaves the index at 0, whose table entry is 0x73.
 *   5. 0x1003FA00 saves the CONTROL's +0x40 and restores it over
 *      aText[0].y (+0x2F70) -- two different fields.  The fixture gives them
 *      different values, so a "corrected" save/restore is visible.
 *   6. 0x1003EF90's mirror into 0x10B4E1E4 lives INSIDE the differs-branch,
 *      so it goes stale on an unchanged caption.
 *
 * Each of the six was additionally confirmed to FAIL when the bug is
 * temporarily reinstated -- see the run log in the packet report.
 *
 * Every cross-module symbol is stood in for HERE.  The test links against
 * port/src/slice8_87.c and port/src/slice8_85.c (the latter only because
 * 0x1003EC30 is WIRED to slice8_85.c's 0x1003EB10 body rather than
 * transcribed again), so a failure can only be one of those two modules'.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_87.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS -- storage the two modules reach
 * ========================================================================== */

BrUi73Ctx     g_br73;
BrUiHook81Ctx g_brHook81;
BrUiNav      *g_pBrUiNav;
BrS71Globals  g_brS71;
Br72Env      *g_pBr72Env;

int32_t g_brAA28D8;      /* 0x10AA28D8, slice6_73.h  */
int32_t g_br0AB3F4;      /* 0x100AB3F4, slice5_61.h  */
int32_t g_brB4E708;      /* 0x10B4E708, slice2_25.h  */
int32_t g_brB4E70C;      /* 0x10B4E70C               */
int32_t g_br0AB3D8;      /* 0x100AB3D8               */
int32_t g_brAA287C;      /* 0x10AA287C               */
int32_t g_brAA28E8;      /* 0x10AA28E8               */

/* ==========================================================================
 * STAND-INS -- callees
 * ========================================================================== */

int32_t BrFtolTrunc(float f) { return (int32_t)f; }

static int s_n1003E070;
void BrFn1003E070(void) { ++s_n1003E070; }

int BrOptOpen2948(void *pUnused) { (void)pUnused; return 1; }

int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl) { (void)pCtl; return 0; }

/* Reached only by slice8_85.c, which this test links for BrUiHook85_1003EB10.
 * Scaffolding: nothing here is asserted about. */
void BrCdTrackPlay(int track) { (void)track; }
void BrSub1003E310(void) { }
void BrSub1006A4A0(void *pThis, void *pArg) { (void)pThis; (void)pArg; }

/* The string table.  Ids are rendered as "#<hex>" so a caption identifies the
 * id that produced it exactly, and an id nothing maps to answers NULL -- which
 * is what BrStrGet does for a bad id and what the bounded copy then skips. */
static int s_nStrGet;
static int s_lastStrId;

const char *BrStrGet(int id)
{
    static char aBuf[8][32];
    static int  iNext;
    char       *psz;

    ++s_nStrGet;
    s_lastStrId = id;
    if (id < 0) {
        return NULL;
    }
    psz = aBuf[iNext];
    iNext = (iNext + 1) & 7;
    snprintf(psz, sizeof(aBuf[0]), "#%X", (unsigned)id);
    return psz;
}

/* ==========================================================================
 * The control fixture
 * ========================================================================== */

static int s_nBoxF04, s_nBoxF10, s_nBoxF14;
static char s_aCapLog[8][64];        /* every caption the box was handed */
static int  s_nCapLog;
static float s_aCapY[8];             /* aText[0].y at each f04 */

static void BoxF04(BrTextBox *pThis)
{
    ++s_nBoxF04;
    if (s_nCapLog < 8) {
        snprintf(s_aCapLog[s_nCapLog], sizeof(s_aCapLog[0]), "%s", pThis->sz);
        s_aCapY[s_nCapLog] = pThis->y;
        ++s_nCapLog;
    }
}
static void BoxF10(BrTextBox *pThis) { (void)pThis; ++s_nBoxF10; }

/* slice8_85.h's CONFLICT 2, reproduced on the FIXTURE side so the test is
 * exercising the real shape: slice3_39.h types +0x14 `void (*)(BrTextBox *)`
 * and the body reads its LOW BYTE and tests it SIGNED.  The stand-in really
 * does return an int and is installed through the same union the module reads
 * it back through -- a stand-in that returned void would hand the module a
 * garbage register and the confirm arm would be chosen at random. */
static int32_t s_boxAsk;
static int32_t BoxF14i(BrTextBox *pThis) { (void)pThis; ++s_nBoxF14; return s_boxAsk; }

static BrTextBoxVtbl s_boxVtbl;

/* The list's +0x20 "offer a value" slot -- slice8_87.h's Br87ListSel. */
static int32_t s_listAnswer;
static int32_t s_listOffered;
static int     s_nListSel;
static int     s_nListAck;

static int32_t ListSel(BrTextList *pThis, int32_t v)
{
    (void)pThis;
    ++s_nListSel;
    s_listOffered = v;
    return s_listAnswer;
}
static void ListAck(BrTextList *pThis, int32_t v)
{
    (void)pThis; (void)v;
    ++s_nListAck;
}

static BrTextListVtbl s_listVtbl;

static BrUiCtl_    *s_pCtl;
static BrScrGlobals s_scr;
static BrActiveFlags s_active;
static BrUiNav      s_nav;
static Br72Env      s_env;

/* The two injected tables.  Ten 2-byte records; record k is {k+0x10, k+0x20}
 * so byte 0 and byte 1 are always distinguishable. */
static uint8_t s_aB3820[20];
/* Ten 8-byte "objects"; only byte +4 is ever read, and only bit 0x10. */
static unsigned char s_aEnt[10][8];
static void *s_apEnt[10];

static void ResetCounters(void)
{
    s_nBoxF04 = s_nBoxF10 = s_nBoxF14 = 0;
    s_nListSel = s_nListAck = 0;
    s_nStrGet = 0;
    s_lastStrId = -12345;
    s_n1003E070 = 0;
    s_nCapLog = 0;
    memset(s_aCapLog, 0, sizeof(s_aCapLog));
}

static void SetUp(void)
{
    int i;

    s_pCtl = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (s_pCtl == NULL) {
        printf("FAIL out of memory\n");
        exit(1);
    }
    memset(&s_boxVtbl, 0, sizeof(s_boxVtbl));
    s_boxVtbl.pfn04 = BoxF04;
    s_boxVtbl.pfn10 = BoxF10;
    {
        union { void (*pfnVoid)(BrTextBox *); int32_t (*pfnAsk)(BrTextBox *); } u;
        u.pfnAsk = BoxF14i;
        s_boxVtbl.pfn14 = u.pfnVoid;
    }
    s_boxAsk = 0;                       /* <= 0 -> the confirm arm */

    s_pCtl->aText[0].pVtbl = &s_boxVtbl;
    s_pCtl->aText[0].f420  = 1u;        /* take ItemApply's long path */

    s_listVtbl.f20 = (void *)ListSel;
    s_listVtbl.f24 = (void *)ListAck;
    s_pCtl->list.pVtbl = &s_listVtbl;

    memset(&s_scr, 0, sizeof(s_scr));
    memset(&s_active, 0, sizeof(s_active));
    memset(&s_nav, 0, sizeof(s_nav));
    s_nav.pG      = &s_scr;
    s_nav.pActive = &s_active;
    g_pBrUiNav    = &s_nav;

    memset(&s_env, 0, sizeof(s_env));
    g_pBr72Env = &s_env;

    memset(&g_br73, 0, sizeof(g_br73));
    memset(&g_brS71, 0, sizeof(g_brS71));

    for (i = 0; i < 10; i++) {
        s_aB3820[2 * i]     = (uint8_t)(i + 0x10);
        s_aB3820[2 * i + 1] = (uint8_t)(i + 0x20);
        memset(s_aEnt[i], 0, sizeof(s_aEnt[i]));
        s_apEnt[i] = s_aEnt[i];
    }
    BrUiHook87Reset();
    g_brHook87.pB3820  = s_aB3820;
    g_brHook87.cB3820  = 10u;
    g_brHook87.apBD2A8 = s_apEnt;
    g_brHook87.cBD2A8  = 10u;

    ResetCounters();
}

static void TearDown(void)
{
    free(s_pCtl);
    s_pCtl = NULL;
}

/* "not solo": 0x10AA2904 and 0x10AA2964 differ. */
static void MakeNonSolo(void)
{
    static int dummyA, dummyB;

    s_nav.pAA2904 = (BrPhase_ *)&dummyA;
    s_scr.pAA2964 = (BrPhaseFull *)&dummyB;
    g_brAA28E8    = 0;
}

static void MakeSolo(void)
{
    s_nav.pAA2904 = NULL;
    s_scr.pAA2964 = NULL;
    g_brAA28E8    = 0;
}

/* ==========================================================================
 * 0x1003EAE0 -- the list poll
 * ========================================================================== */

static void Test_1003EAE0(void)
{
    SetUp();

    g_br0AB3F4   = 7;
    s_listAnswer = 5;
    CHECK(BrUiHook87_1003EAE0(s_pCtl) == 1);
    CHECK(s_nListSel == 1);
    CHECK(s_listOffered == 7);       /* the OLD value is what is offered */
    CHECK(g_br0AB3F4 == 5);

    /* A negative answer is DECLINED -- the global keeps its value.  An
     * inverted `if` would leave -1 here. */
    s_listAnswer = -1;
    CHECK(BrUiHook87_1003EAE0(s_pCtl) == 1);
    CHECK(g_br0AB3F4 == 5);

    /* Zero is a perfectly good index, not a "no answer". */
    s_listAnswer = 0;
    CHECK(BrUiHook87_1003EAE0(s_pCtl) == 1);
    CHECK(g_br0AB3F4 == 0);

    /* No vtable at all: the DEVIATION answer is -1, i.e. "declined". */
    s_pCtl->list.pVtbl = NULL;
    g_br0AB3F4 = 3;
    CHECK(BrUiHook87_1003EAE0(s_pCtl) == 1);
    CHECK(g_br0AB3F4 == 3);
    CHECK(s_nListAck == 0);          /* this hook never acknowledges */

    TearDown();
}

/* ==========================================================================
 * 0x1003EF90 / 0x1003F020 -- read back into 0x10A9CDF0 and 0x10B4E1E4
 * ========================================================================== */

static void Test_1003EF90(void)
{
    BrUiCtl_ *pTarget;

    SetUp();
    pTarget = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (pTarget == NULL) { printf("FAIL oom\n"); exit(1); }
    s_env.pAA29E8 = pTarget;

    /* --- empty caption: the bit-4 clear does NOT fire ------------------- */
    pTarget->flags1C = 0x1F;
    s_pCtl->aText[0].sz[0] = '\0';
    snprintf(s_env.szA9CDF0, sizeof(s_env.szA9CDF0), "%s", "seed");
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x1F);            /* untouched */
    CHECK(s_nBoxF04 == 1);                      /* ItemApply DID run */
    /* An empty caption still DIFFERS from "seed", so the copy runs. */
    CHECK(strcmp(s_env.szA9CDF0, "") == 0);
    CHECK(strcmp(g_brHook87.szB4E1E4, "") == 0);

    /* --- non-empty caption: bit 4 is cleared, bits 0..3 survive --------- */
    ResetCounters();
    pTarget->flags1C = 0x1F;
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "Alpha");
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x0F);
    CHECK(strcmp(s_env.szA9CDF0, "Alpha") == 0);
    CHECK(strcmp(g_brHook87.szB4E1E4, "Alpha") == 0);

    /* --- CONFLICT 1.  Case-only difference == UNCHANGED. ----------------
     * _stricmp answers 0, so NEITHER copy runs and the stored spelling keeps
     * its original case.  A strcmp build overwrites both and fails here. */
    ResetCounters();
    memset(g_brHook87.szB4E1E4, 0, sizeof(g_brHook87.szB4E1E4));
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "ALPHA");
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(strcmp(s_env.szA9CDF0, "Alpha") == 0);   /* NOT "ALPHA" */
    /* GOTCHA 6: the mirror is inside the differs-branch, so it stayed empty
     * even though the caption is live.  This IS the defect, preserved. */
    CHECK(g_brHook87.szB4E1E4[0] == '\0');

    /* --- a genuine change updates both --------------------------------- */
    ResetCounters();
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "Beta");
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(strcmp(s_env.szA9CDF0, "Beta") == 0);
    CHECK(strcmp(g_brHook87.szB4E1E4, "Beta") == 0);

    /* --- 0x1003F020 is the bit-4 clear ALONE ---------------------------- */
    ResetCounters();
    pTarget->flags1C = 0x1F;
    CHECK(BrUiHook87_1003F020(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x0F);
    CHECK(s_nBoxF04 == 0);                       /* no apply */
    CHECK(strcmp(s_env.szA9CDF0, "Beta") == 0);  /* no copy */

    ResetCounters();
    pTarget->flags1C = 0x1F;
    s_pCtl->aText[0].sz[0] = '\0';
    CHECK(BrUiHook87_1003F020(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x1F);

    free(pTarget);
    s_env.pAA29E8 = NULL;
    TearDown();
}

/* ==========================================================================
 * 0x1003F210 / 0x1003F280 -- the same over 0x10AA29BC and 0x10A9D018
 * ========================================================================== */

static void Test_1003F210(void)
{
    BrUiCtl_ *pTarget;
    char      aName[BR71_A9D018_SIZE];

    SetUp();
    pTarget = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (pTarget == NULL) { printf("FAIL oom\n"); exit(1); }
    g_brS71.pAA29BC = pTarget;
    memset(aName, 0, sizeof(aName));
    snprintf(aName, sizeof(aName), "%s", "Driver");
    g_brS71.pA9D018 = aName;

    pTarget->flags1C = 0x1F;
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "Rider");
    CHECK(BrUiHook87_1003F210(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x0F);
    CHECK(s_nBoxF04 == 1);
    CHECK(strcmp(aName, "Rider") == 0);

    /* CONFLICT 1 again: case-only, so no copy. */
    ResetCounters();
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "RIDER");
    CHECK(BrUiHook87_1003F210(s_pCtl) == 1);
    CHECK(strcmp(aName, "Rider") == 0);

    /* The copy is bounded by BR71_A9D018_SIZE.  The original's is not; this
     * is DEVIATION 2 and the check is that nothing past the buffer moves. */
    ResetCounters();
    memset(s_pCtl->aText[0].sz, 'x', 300);
    s_pCtl->aText[0].sz[300] = '\0';
    CHECK(BrUiHook87_1003F210(s_pCtl) == 1);
    CHECK(strlen(aName) == (size_t)BR71_A9D018_SIZE - 1u);

    /* 0x1003F280: the clear alone, no apply, no copy. */
    ResetCounters();
    pTarget->flags1C = 0x1F;
    CHECK(BrUiHook87_1003F280(s_pCtl) == 1);
    CHECK(pTarget->flags1C == 0x0F);
    CHECK(s_nBoxF04 == 0);

    free(pTarget);
    g_brS71.pAA29BC = NULL;
    g_brS71.pA9D018 = NULL;
    TearDown();
}

/* ==========================================================================
 * 0x1003F5E0 / 0x1003F680 -- the five-way code switch
 * ========================================================================== */

static void Test_CodeSwitch(void)
{
    static const uint16_t aLo[5] = { 0x56, 0x57, 0x59, 0x5B, 0x5D };
    static const uint16_t aHi[5] = { 0xFFFF, 0x58, 0x5A, 0x5C, 0x5E };
    int i;

    SetUp();

    for (i = 0; i < 5; i++) {
        s_env.nAA2A18 = i;
        s_pCtl->w1E20C = 0x1234;
        CHECK(BrUiHook87_1003F5E0(s_pCtl) == 1);
        CHECK(s_pCtl->w1E20C == aLo[i]);

        s_pCtl->w1E20C = 0x1234;
        CHECK(BrUiHook87_1003F680(s_pCtl) == 1);
        CHECK(s_pCtl->w1E20C == aHi[i]);
    }

    /* POINT 3.  Out of range -- and NEGATIVE, which the `ja` treats as a
     * huge unsigned -- lands on the default.  0x1003F5E0's default is 0x56,
     * which is index 0's answer; 0x1003F680's is 0xFFFF, likewise index 0's.
     * A build that used -1 as 0x1003F5E0's default fails the first line. */
    s_env.nAA2A18 = 5;
    CHECK(BrUiHook87_1003F5E0(s_pCtl) == 1);
    CHECK(s_pCtl->w1E20C == 0x56u);
    CHECK(BrUiHook87_1003F680(s_pCtl) == 1);
    CHECK(s_pCtl->w1E20C == 0xFFFFu);

    s_env.nAA2A18 = -1;
    CHECK(BrUiHook87_1003F5E0(s_pCtl) == 1);
    CHECK(s_pCtl->w1E20C == 0x56u);
    CHECK(BrUiHook87_1003F680(s_pCtl) == 1);
    CHECK(s_pCtl->w1E20C == 0xFFFFu);

    TearDown();
}

/* ==========================================================================
 * The five one-table caption hooks
 * ========================================================================== */

static void Test_Captions(void)
{
    SetUp();

    /* 0x1003FC40: k_AC3F0[0x10AA287C] == {0x55,0x56,0x57,0x8C}. */
    g_brAA287C = 0;
    CHECK(BrUiHook87_1003FC40(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#55") == 0);
    /* TWO +0x04 calls per caption: the copy's own, then ItemApply's. */
    CHECK(s_nBoxF04 == 2);
    g_brAA287C = 3;
    CHECK(BrUiHook87_1003FC40(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#8C") == 0);
    /* DEVIATION: out of range answers -1 and the caption is left alone. */
    g_brAA287C = 4;
    CHECK(BrUiHook87_1003FC40(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#8C") == 0);

    /* POINT 4.  0x1003FCB0's fallback is the LITERAL 0x74, and the index it
     * would otherwise use is 0 -- whose table entry is 0x73.  A build that
     * dropped the fallback and read the table shows "#73" here. */
    s_env.n18ABDBC = 0;
    s_env.nAA2A1C  = 0;
    CHECK(BrUiHook87_1003FCB0(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#74") == 0);

    s_env.n18ABDBC = 1;
    CHECK(BrUiHook87_1003FCB0(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#73") == 0);
    s_env.nAA2A1C  = 1;
    CHECK(BrUiHook87_1003FCB0(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#74") == 0);

    /* The three two-entry siblings, each on its OWN index -- so a body that
     * read a neighbour's global would show the wrong entry. */
    s_env.nAA2A28 = 1; s_env.nAA2A20 = 0; s_env.nAA2A24 = 0;
    CHECK(BrUiHook87_1003FD30(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#74") == 0);
    CHECK(BrUiHook87_1003FDA0(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#73") == 0);
    CHECK(BrUiHook87_1003FE10(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#73") == 0);

    s_env.nAA2A28 = 0; s_env.nAA2A20 = 1; s_env.nAA2A24 = 1;
    CHECK(BrUiHook87_1003FD30(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#73") == 0);
    CHECK(BrUiHook87_1003FDA0(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#74") == 0);
    CHECK(BrUiHook87_1003FE10(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#74") == 0);

    TearDown();
}

/* ==========================================================================
 * 0x1003FA00 -- the big one
 * ========================================================================== */

static void Test_1003FA00(void)
{
    SetUp();

    /* --- solo: id 0x1B, nothing else consulted -------------------------- */
    MakeSolo();
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#1B") == 0);
    CHECK(s_nStrGet == 1);

    /* --- not solo, 0x100AA010 != 0: the index is 0x100AC648 straight ---- */
    ResetCounters();
    MakeNonSolo();
    g_br73.n0AA010 = 1;
    g_br73.n0AC648 = 13;             /* k_AC308[13] == 0x7D */
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#7D") == 0);

    /* --- POINT 2.  0x10AA28B8 is SIGNED. --------------------------------
     * base 12 with i == -1 gives record 0, whose byte 0 is 0x10 -- out of
     * k_AC308's 16 entries, so the caption is left alone.  Use base 24 so
     * record 1 is selected instead: byte 0 == 0x11, still out of range.
     * The DISCRIMINATING observation is therefore the RECORD, read back
     * through 0x1003FE80's byte-1 sibling below; here the check is that the
     * signed reading stays IN the injected table at all -- an unsigned
     * reading gives 12 + 12*255 == 3072, which Br87Rec bounds to 0. */
    ResetCounters();
    g_br73.n0AA010 = 0;
    g_br73.bAA28B8 = 0xFFu;          /* -1 as int8_t */
    g_br73.nAA28A4 = 12;             /* 0x10AA28A8 == 0 selects this one */
    g_br73.nAA28AC = 96;
    s_scr.bAA28A8  = 0u;
    /* record 12 + 12*(-1) == 0 -> byte 0 == 0x10 -> out of k_AC308 */
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);

    /* Make the discrimination direct: shrink the record table's payload so
     * record 0 maps INSIDE k_AC308 and record 0 alone does. */
    ResetCounters();
    s_aB3820[0] = 5u;                /* record 0, byte 0 -> k_AC308[5] == 0x7C */
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#7C") == 0);
    s_aB3820[0] = 0x10u;             /* put it back */

    /* --- 0x10AA28A8 selects the OTHER base ------------------------------ */
    ResetCounters();
    s_scr.bAA28A8  = 1u;
    g_br73.nAA28AC = 13;             /* 13 + 12*(-1) == 1 */
    s_aB3820[2]    = 1u;             /* record 1, byte 0 -> k_AC308[1] == 0x78 */
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#78") == 0);
    s_aB3820[2] = 0x11u;
    s_scr.bAA28A8 = 0u;

    /* --- POINT 5.  The bit-0x10 arm: the 0xB0 caption comes FIRST, is
     * drawn at y == 130.0, and the value put back afterwards is the
     * CONTROL's +0x40 -- not aText[0].y's own previous value. ------------ */
    ResetCounters();
    g_br73.n0AA010 = 1;
    g_br73.n0AC648 = 1;              /* k_AC308[1] == 0x78, entity index 1 */
    s_aEnt[1][4] = 0x10;
    s_pCtl->y            = 42.5f;    /* control +0x40  */
    s_pCtl->aText[0].y   = 99.0f;    /* control +0x2F70 */
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    /* Four log entries: two captions x (copy's +0x04, ItemApply's +0x04). */
    CHECK(s_nCapLog == 4);
    CHECK(strcmp(s_aCapLog[0], "#B0") == 0);      /* the extra one FIRST */
    CHECK(s_aCapY[0] == 130.0f);                  /* drawn at 130.0 */
    CHECK(s_aCapY[1] == 130.0f);
    CHECK(strcmp(s_aCapLog[2], "#78") == 0);      /* then the real one */
    CHECK(s_aCapY[2] == 42.5f);                   /* at the restored y */
    CHECK(s_pCtl->aText[0].y == 42.5f);           /* +0x40 restored over it */
    CHECK(s_pCtl->y == 42.5f);                    /* +0x40 itself untouched */
    s_aEnt[1][4] = 0;

    /* With the bit clear there is exactly ONE caption and no y traffic. */
    ResetCounters();
    s_pCtl->aText[0].y = 99.0f;
    CHECK(BrUiHook87_1003FA00(s_pCtl) == 1);
    CHECK(s_nCapLog == 2);
    CHECK(strcmp(s_aCapLog[0], "#78") == 0);
    CHECK(s_pCtl->aText[0].y == 99.0f);

    TearDown();
}

/* ==========================================================================
 * 0x1003FE80 -- byte 1 of the same record, and the 8-unit lift
 * ========================================================================== */

static void Test_1003FE80(void)
{
    SetUp();

    /* --- solo: id 0x1C, drawn 8 units up, y restored EXACTLY ------------ */
    MakeSolo();
    s_pCtl->aText[0].y = 55.25f;
    CHECK(BrUiHook87_1003FE80(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#1C") == 0);
    CHECK(s_nCapLog == 2);
    CHECK(s_aCapY[0] == 47.25f);          /* 55.25 - 8 while it was drawn */
    CHECK(s_aCapY[1] == 47.25f);
    CHECK(s_pCtl->aText[0].y == 55.25f);  /* and put back exactly */

    /* --- not solo, 0x100AA010 != 0: index is 0x10AA2A00 ----------------- */
    ResetCounters();
    MakeNonSolo();
    g_br73.n0AA010 = 1;
    g_br73.nAA2A00 = 6;                   /* k_AC3B0[6] == 0x9D */
    CHECK(BrUiHook87_1003FE80(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#9D") == 0);

    /* --- BYTE 1, not byte 0.  Record 0 is {0x10, 0x20} by construction;
     * make byte 1 land inside k_AC3B0 and leave byte 0 outside it, so a body
     * that read byte 0 produces no caption at all. ----------------------- */
    ResetCounters();
    g_br73.n0AA010 = 0;
    g_br73.bAA28B8 = 0xFFu;               /* -1 */
    g_br73.nAA28A4 = 12;                  /* record 12 + 12*(-1) == 0 */
    s_scr.bAA28A8  = 0u;
    s_aB3820[0] = 0x40u;                  /* byte 0: OUT of k_AC3B0's 12 */
    s_aB3820[1] = 4u;                     /* byte 1: k_AC3B0[4] == 0x87 */
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "keep");
    CHECK(BrUiHook87_1003FE80(s_pCtl) == 1);
    CHECK(strcmp(s_pCtl->aText[0].sz, "#87") == 0);

    TearDown();
}

/* ==========================================================================
 * ItemApply's own contract, exercised through 0x1003EF90
 * ========================================================================== */

static void Test_ItemApply(void)
{
    BrUiCtl_ *pTarget;

    SetUp();
    pTarget = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (pTarget == NULL) { printf("FAIL oom\n"); exit(1); }
    s_env.pAA29E8 = pTarget;
    snprintf(s_pCtl->aText[0].sz, sizeof(s_pCtl->aText[0].sz), "%s", "t");

    /* f420 == 0 is the EARLY OUT: f04 and f10 run, f14 does not, and none of
     * the confirm block's side effects happen. */
    ResetCounters();
    s_pCtl->aText[0].f420 = 0u;
    g_brAA28D8 = 1;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(s_nBoxF04 == 1 && s_nBoxF10 == 1 && s_nBoxF14 == 0);
    CHECK(s_n1003E070 == 0);
    CHECK(g_brAA28D8 == 1);

    /* f420 != 0 and the +0x14 slot answering <= 0 takes the confirm arm. */
    ResetCounters();
    s_pCtl->aText[0].f420 = 1u;
    s_pCtl->flags1C       = 0;
    s_active.override     = 0;
    g_brAA28D8            = 1;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(s_nBoxF14 == 1);
    CHECK(s_n1003E070 == 1);
    CHECK(g_brAA28D8 == 0);
    CHECK(s_pCtl->aText[0].f420 == 0u);
    CHECK(s_nBoxF10 == 1);                 /* exactly once, at the bottom */

    /* override != 0 suppresses the three clears but NOT 0x1003E070. */
    ResetCounters();
    s_pCtl->aText[0].f420 = 1u;
    s_active.override     = 1;
    g_brAA28D8            = 1;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(s_n1003E070 == 1);
    CHECK(g_brAA28D8 == 1);
    CHECK(s_pCtl->aText[0].f420 == 1u);
    s_active.override = 0;

    /* `test al,al / jle` then `test byte [ebx+0x1C],2 / je`: a POSITIVE ask
     * with flag bit 1 CLEAR skips the whole confirm block.  Only +0x10 runs.
     * An implementation that dropped either half of the condition fires
     * 0x1003E070 here. */
    ResetCounters();
    s_pCtl->aText[0].f420 = 1u;
    s_pCtl->flags1C       = 0;
    s_boxAsk              = 1;
    g_brAA28D8            = 1;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(s_nBoxF14 == 1);
    CHECK(s_n1003E070 == 0);
    CHECK(g_brAA28D8 == 1);
    CHECK(s_pCtl->aText[0].f420 == 1u);
    CHECK(s_nBoxF10 == 1);

    /* ...but the SAME positive ask WITH bit 1 set takes the confirm arm. */
    ResetCounters();
    s_pCtl->flags1C = 2;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK(s_n1003E070 == 1);
    CHECK(g_brAA28D8 == 0);
    s_boxAsk = 0;

    /* `and al,0xFD` clears bit 1 and leaves the upper 24 bits alone. */
    ResetCounters();
    s_pCtl->aText[0].f420 = 1u;
    s_pCtl->flags1C       = (int32_t)0xABCD0003;
    CHECK(BrUiHook87_1003EF90(s_pCtl) == 1);
    CHECK((uint32_t)s_pCtl->flags1C == 0xABCD0001u);

    free(pTarget);
    s_env.pAA29E8 = NULL;
    TearDown();
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

static void Test_Install(void)
{
    BrS71Hooks  h71;
    BrUi72Hooks h72;
    BrUi73Hooks h73;

    memset(&h71, 0, sizeof(h71));
    memset(&h72, 0, sizeof(h72));
    memset(&h73, 0, sizeof(h73));

    BrUiHook87Install71(&h71);
    CHECK(h71.p1003EAE0 == BrUiHook87_1003EAE0);
    CHECK(h71.p1003F210 == BrUiHook87_1003F210);
    CHECK(h71.p1003F280 == BrUiHook87_1003F280);
    CHECK(h71.p1003F720 == NULL);      /* no builder installs it */
    CHECK(h71.p10047360 == NULL);      /* slice8_84.c's, not this module's */

    BrUiHook87Install72(&h72);
    /* PRE-FLIGHT (1): the slot gets slice8_85.c's body, not a new one. */
    CHECK(h72.p1003EC30 == BrUiHook85_1003EB10);
    CHECK(h72.p1003EF90 == BrUiHook87_1003EF90);
    CHECK(h72.p1003F020 == BrUiHook87_1003F020);
    CHECK(h72.p1003F5E0 == BrUiHook87_1003F5E0);
    CHECK(h72.p1003F680 == BrUiHook87_1003F680);
    CHECK(h72.p1003FA00 == BrUiHook87_1003FA00);
    CHECK(h72.p1003FCB0 == BrUiHook87_1003FCB0);
    CHECK(h72.p1003FD30 == BrUiHook87_1003FD30);
    CHECK(h72.p1003FDA0 == BrUiHook87_1003FDA0);
    CHECK(h72.p1003FE10 == BrUiHook87_1003FE10);
    CHECK(h72.p1003FE80 == BrUiHook87_1003FE80);
    CHECK(h72.p1003E7A0 == NULL);      /* slice8_85.c owns it */

    BrUiHook87Install73(&h73);
    CHECK(h73.p1003FC40 == BrUiHook87_1003FC40);
    /* PRE-FLIGHT (2): a VISIBLE hole.  If a later pass ports 0x1003ECB0 it
     * must delete this line deliberately. */
    CHECK(h73.p1003ECB0 == NULL);

    /* NULL is a no-op, not a fault. */
    BrUiHook87Install71(NULL);
    BrUiHook87Install72(NULL);
    BrUiHook87Install73(NULL);
}

/* ==========================================================================
 * Unwired hosts: every hook must survive a tree with nothing plugged in
 * ========================================================================== */

static void Test_Unwired(void)
{
    BrUiCtl_ *pCtl = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));

    if (pCtl == NULL) { printf("FAIL oom\n"); exit(1); }

    g_pBrUiNav = NULL;
    g_pBr72Env = NULL;
    memset(&g_brS71, 0, sizeof(g_brS71));
    BrUiHook87Reset();

    CHECK(BrUiHook87_1003EAE0(pCtl) == 1);
    CHECK(BrUiHook87_1003EF90(pCtl) == 1);
    CHECK(BrUiHook87_1003F020(pCtl) == 1);
    CHECK(BrUiHook87_1003F210(pCtl) == 1);
    CHECK(BrUiHook87_1003F280(pCtl) == 1);
    CHECK(BrUiHook87_1003F5E0(pCtl) == 1);
    CHECK(BrUiHook87_1003F680(pCtl) == 1);
    CHECK(BrUiHook87_1003FA00(pCtl) == 1);
    CHECK(BrUiHook87_1003FC40(pCtl) == 1);
    CHECK(BrUiHook87_1003FCB0(pCtl) == 1);
    CHECK(BrUiHook87_1003FD30(pCtl) == 1);
    CHECK(BrUiHook87_1003FDA0(pCtl) == 1);
    CHECK(BrUiHook87_1003FE10(pCtl) == 1);
    CHECK(BrUiHook87_1003FE80(pCtl) == 1);

    free(pCtl);
}

int main(void)
{
    Test_1003EAE0();
    Test_1003EF90();
    Test_1003F210();
    Test_CodeSwitch();
    Test_Captions();
    Test_1003FA00();
    Test_1003FE80();
    Test_ItemApply();
    Test_Install();
    Test_Unwired();

    if (g_fails != 0) {
        printf("test_slice8_87: %d FAILED\n", g_fails);
        return 1;
    }
    printf("test_slice8_87: all checks passed\n");
    return 0;
}
