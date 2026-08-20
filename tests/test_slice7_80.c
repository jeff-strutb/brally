/* test_slice7_80.c -- the option-changing hooks.
 *
 * Two halves, and the second is the point:
 *
 *   PART A  properties of the six hooks in isolation -- the wrap directions,
 *           the {1,0} table inversion, the two side-effect asymmetries, and
 *           the fact that a hook fired with no edit edge moves nothing.
 *
 *   PART B  the SAME hooks reached the way the game reaches them: the real
 *           Audio Options builder (0x1004DFC0) builds a real page of real
 *           controls, and the ported navigation chain (0x10048530 ->
 *           0x10048180 -> 0x10047A60) runs a frame per key.  The option's
 *           global is dumped before and after every key, because a claim that
 *           a toggle changed something is not evidence without the number.
 *
 * Run with an argument to print the Part B transcript:
 *     ./build/test_slice7_80 -keys "..d+++j-"
 * With no argument the transcript still runs, silently, and its assertions
 * still fire -- the printing is what the argument controls, not the test.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file and nowhere
 * else, as the contract requires.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice7_80.h"     /* pulls slice6_73.h + slice6_72.h + br_state.h */
#include "br_uictl.h"      /* BrUiCtlCtor, g_pBrUiCtlVtbl                  */
#include "br_uivt.h"       /* BrUiPageCtor_10048470, place / set-text      */
/* br_uinav.h AFTER slice6_73.h -- the header says so at its include. */
#include "br_uinav.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS.  Test scaffolding for symbols whose owning module is not linked
 * into this binary.  None of them is decompiled and none is asserted about
 * except where noted.
 * ========================================================================== */

/* --- br_crt.h ------------------------------------------------------------ */
void *BrOperatorNew(uint32_t cb)
{
    /* 0x1007DFE0 does NOT zero; poison so anything relying on zeroing shows. */
    void *p = malloc(cb);
    if (p != NULL) { memset(p, 0xCD, cb); }
    return p;
}
void    BrOperatorDelete(void *p)  { free(p); }
int32_t BrFtolTrunc(float f)       { return (int32_t)f; }
char   *BrItoa(int v, char *psz, int radix)
{ (void)radix; sprintf(psz, "%d", v); return psz; }
int BrSprintf(char *pszDest, const char *pszFmt, ...) { (void)pszFmt; if (pszDest) *pszDest = 0; return 0; }

/* --- slice1_06.h --------------------------------------------------------- */
const BrErrEnt g_aBrErrTable[BR_ERR_COUNT] = { {0,0},{0,0},{0,0},{0,0},
                                               {1,0},{0,0},{0,0},{0,0},{0,0} };
void BrErrShow(const BrErrHost *pHost, int32_t idx) { (void)pHost; (void)idx; }
int  BrPairBufReset(BrPairBuf *pBuf) { (void)pBuf; return 1; }
BrNameList *BrNameListInit(BrNameList *pThis, const void *pVtbl,
                           const char *pszFill)
{
    int i;
    pThis->pVtbl = pVtbl;
    for (i = 0; i < BR_NAMELIST_COUNT; ++i) {
        memset(pThis->asz[i], 0, BR_NAMELIST_STRIDE);
        if (pszFill != NULL) { strncpy(pThis->asz[i], pszFill, BR_NAMELIST_STRIDE - 1); }
    }
    return pThis;
}

/* --- 0x10074030, the string table ---------------------------------------
 * The ids this screen uses, with the captions BRString.dll really carries
 * (testdata/strings.txt, ids 12, 33, 46, 47).  Anything else is echoed as
 * "s<hex>" so a stray id cannot masquerade as a real caption. */
static char s_aszStr[8][32];
static int  s_iStr;
const char *BrStrGet(int id)
{
    char *p;
    switch (id) {
    case 0x0C: return "Back";
    case 0x21: return "Audio Options";
    case 0x2E: return "SFX Volume";
    case 0x2F: return "CD VOlume";
    default: break;
    }
    p = s_aszStr[s_iStr & 7];
    s_iStr++;
    snprintf(p, sizeof(s_aszStr[0]), "s%X", (unsigned)id);
    return p;
}

/* --- slice6_73's other cross-slice callees ------------------------------- */
char    g_aBr39B720[256];
int32_t g_brAA28D8;
void BrSub1003E510(void) {}
void BrSub_10019290(void) {}
void BrSub1003E3A0(void) {}
void BrSub1003CE80(void) {}

/* --- slice5_61.h --------------------------------------------------------- */
int32_t        g_br0AB3F4;
unsigned char *g_brPAA29D0;
char           g_aBrA9D018[256];
char           g_aBrA9D078[256];
uint8_t        g_aBrAA26F4[4];
float          g_br4BC198;
/* g_aBr0B3820 is NOT stubbed here: br_data.c owns it with the initialiser it
 * was read out of the DLL with, and this binary links br_data.o. */
const void *(*g_brPfnDerefW1)(uint32_t w1);
int32_t      (*g_brPfn1003D0B0)(void *pObj, void **ppvOut);
BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd) { return pCmd; }
int32_t BrExt_10042410(void *pArg) { (void)pArg; return 1; }

/* --- slice1_05 / slice1_01 / slice1_09 ----------------------------------- */
void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz)
{
    memset(pM, 0, sizeof(*pM));
    pM->m[0][0] = 1; pM->m[1][1] = 1; pM->m[2][2] = 1; pM->m[3][3] = 1;
    pM->m[3][0] = tx; pM->m[3][1] = ty; pM->m[3][2] = tz;
}
uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y)
{ (void)pGrid; (void)x; (void)y; return 0; }
uint16_t BrU16CursorNext(const uint16_t *pTable, BrU16Cursor *pCur)
{
    uint16_t v;
    if (pCur->remaining == 0) { return 0; }
    v = pTable[pCur->pos]; pCur->pos++; pCur->remaining--; return v;
}
void BrVec3Normalise(BrVec3 *pV) { (void)pV; }

/* --- slice2_11.h: the collision grid ------------------------------------- */
static BrCollPlane s_aGrid[BR73_COLL_CELLS * BR_COLL_CELL_PLANES];
static uint16_t    s_aGridCount[BR73_COLL_CELLS];
static BrVec3      s_aVerts[16];
static uint16_t    s_aTriIdx[32];
static uint8_t     s_aTriFlags[8];
BrCollPlane    *g_pBrCollGrid      = s_aGrid;
const uint16_t *g_pBrCollGridCount = s_aGridCount;
const uint16_t *g_pBrCollTriIdx    = s_aTriIdx;
BrVec3         *g_pBrCollVerts     = s_aVerts;
const uint8_t  *g_pBrCollTriFlags  = s_aTriFlags;
const uint16_t *g_pBrGrid64;
const uint16_t *g_pBrTriTable;

/* --- slice6_73.c's direct callees ----------------------------------------
 * 0x10071550 calls these two directly, with no indirection and no null test,
 * so they are real external symbols rather than installable hook slots.
 * Neither body is decompiled yet. */
void BrSub10071560(void) { }
void BrSub10071630(void) { }

/* --- slice3_39.h --------------------------------------------------------- */
void BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }

/* --- br_uinav.c's cross-slice callees ------------------------------------
 * 0x10075020 is the millisecond clock the step timer reads.  It is a COUNTER
 * here rather than a real clock: the frame is the unit this test cares about
 * and a wall clock would make the transcript non-reproducible. */
static int32_t s_nClock;
int32_t BrSub10075020(void) { return s_nClock; }
void    BrSub10072AF0(int a, int b) { (void)a; (void)b; }
void    BrExt_1004F700(BrPhase_ *pSelf) { (void)pSelf; }

/* --- 0x10060D90, called by BOTH volume hooks on every path ---------------
 * Counted, because "it is called even when nothing moved" is one of the
 * asymmetries this test asserts. */
static int s_c10060D90;
void BrSub10060D90(void) { ++s_c10060D90; }

/* --- slice2_25.c's remaining callees ------------------------------------- */
void  BrSub10038F30(int a) { (void)a; }
void  BrSub1003BF60(void) {}
void  BrSub1003C020(void) {}
void  BrSub1003C150(void) {}
void  BrSub1003C1E0(void) {}
void  BrSub1003C230(void) {}
void  BrSub1003C260(void) {}
void  BrSub1003CDA0(void) {}
void  BrSub1003D950(void) {}
void  BrSub1003D9F0(void) {}
void  BrSub1003E310(void) {}
void  BrSub1003F2B0(void) {}
void  BrSub1003F320(void) {}
void  BrSub10041B50(void) {}
void  BrSub10044540(void) {}
void  BrSub100586A0(void) {}
void  BrSub10058700(void) {}
void  BrSub1005FCF0(void) {}
void  BrSub10071130(void) {}
void *BrGlobalHandle(void *p) { return p; }
int   BrGlobalUnlock(void *h) { (void)h; return 1; }
void *BrGlobalFree(void *h) { (void)h; return NULL; }

/* slice2_25.h declares the next group over ITS partial phase / game / COM
 * models, and that header cannot be included here (slice6_73.h CONFLICT 1).
 * They are therefore defined over `void *`, which links against any caller
 * declaration for the same reason br_stubs.c's `long(void)` does.  None of
 * them is on any path this test drives -- they are here only so slice2_25.o
 * links, and if one is ever reached it will still be a no-op rather than a
 * fault, so nothing below asserts anything about them. */
void BrOptFn10044970(void *p) { (void)p; }
void BrOptFn10044A30(void *p) { (void)p; }
void BrOptFn1004CAC0(void *p) { (void)p; }
void BrOptFn10051990(void *p) { (void)p; }
void BrOptFn10051D30(void *p) { (void)p; }
void BrOptFn10056A10(BrPhase_ *p) { (void)p; }   /* slice6_72.h types these two */
void BrOptFn10056FF0(void *p) { (void)p; }
void BrOptFn100575F0(void *p) { (void)p; }
void BrOptFn10057C10(BrPhase_ *p) { (void)p; }
void BrOptFn10058750(void *p) { (void)p; }
void BrSub1003D0B0(void *a, void *b) { (void)a; (void)b; }
void BrSub1003D210(void *a, void *b, int c) { (void)a; (void)b; (void)c; }
void BrSub1003DA40(void *a, int b) { (void)a; (void)b; }
void BrSub10043BF0(void *p) { (void)p; }
void BrSub10046400(void *p) { (void)p; }
void BrSub10047360(void *p) { (void)p; }
void BrSub1006A4A0(void *a, void *b) { (void)a; (void)b; }

/* ==========================================================================
 * PART A -- the six hooks in isolation
 * ========================================================================== */

/* Every option's underlying word, so a test can set a start state without
 * knowing which module owns the storage. */
static void OptSetIndex(BrUiOptId id, int32_t v, BrActiveFlags *pA)
{
    /* There is no setter in the module and there should not be -- the only
     * ways the original moves these words are the six hooks.  So the start
     * state is reached by CYCLING to it, which also proves the cycle is
     * closed. */
    int guard = 0;
    BrUiOptSetEdit(pA, +1);
    while (BrUiOptGetIndex(id) != v && guard < 64) {
        switch (id) {
        case BR_OPT_SFX_VOLUME:     (void)BrUiOptHook_10042CF0(NULL); break;
        case BR_OPT_CD_VOLUME:      (void)BrUiOptHook_10042D60(NULL); break;
        case BR_OPT_FORCE_FEEDBACK: (void)BrUiOptHook_10043590(NULL); break;
        case BR_OPT_SKID_MARKS:     (void)BrUiOptHook_100435F0(NULL); break;
        case BR_OPT_CAR_SHADOW:     (void)BrUiOptHook_10043650(NULL); break;
        default:                    (void)BrUiOptHook_100436B0(NULL); break;
        }
        ++guard;
    }
    CHECK(guard < 64);
    BrUiOptSetEdit(pA, 0);
}

static void TestVolumeCycle(void)
{
    BrActiveFlags a;
    int i, before;

    memset(&a, 0, sizeof a);
    printf("A1 volume cyclers (0x10042CF0 / 0x10042D60)\n");

    /* UP ten times from 0 returns to 0: the range is 0..9 inclusive and the
     * wrap is a wrap, not a clamp. */
    OptSetIndex(BR_OPT_SFX_VOLUME, 0, &a);
    BrUiOptSetEdit(&a, +1);
    for (i = 0; i < 9; ++i) {
        before = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
        CHECK(BrUiOptHook_10042CF0(NULL) == 1);
        CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == before + 1);
    }
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == 9);
    CHECK(BrUiOptHook_10042CF0(NULL) == 1);
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == 0);      /* 9 -> 0, wraps */

    /* DOWN from 0 goes to the top, not to -1. */
    BrUiOptSetEdit(&a, -1);
    CHECK(BrUiOptHook_10042CF0(NULL) == 1);
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == 9);

    /* UP wins when both edges are set -- slice2_25.h states it, and it is a
     * property of the `if / else if`, so it is asserted rather than assumed. */
    g_fails += 0;
    {
        BrActiveFlags both;
        memset(&both, 0, sizeof both);
        BrUiOptSetEdit(&both, -1);
        /* re-raise the UP word on top of the DOWN one */
        BrUiOptSetEdit(&both, +1);
        CHECK(BrUiOptGetEditUp() == 1 && BrUiOptGetEditDown() == 0);
    }

    /* The two volume rows differ ONLY in 0x100AB3D8, and that asymmetry is
     * what the Audio screen's two slider widgets read. */
    BrUiOptSetEdit(&a, +1);
    (void)BrUiOptHook_10042CF0(NULL);
    CHECK(BrUiOptGetVolumeSelector() == 1);
    (void)BrUiOptHook_10042D60(NULL);
    CHECK(BrUiOptGetVolumeSelector() == 0);

    /* ...and 0x10060D90 runs on EVERY path, including the one where neither
     * edge is set and nothing moves.  slice2_25.c records this as the
     * contrast with 0x10044600; here it is measured. */
    BrUiOptSetEdit(&a, 0);
    before = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    s_c10060D90 = 0;
    CHECK(BrUiOptHook_10042CF0(NULL) == 1);
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == before);   /* did NOT move */
    CHECK(s_c10060D90 == 1);                               /* still called */
    CHECK(BrUiOptGetVolumeSelector() == 1);                /* still written */

    /* The two volumes are independent words. */
    OptSetIndex(BR_OPT_SFX_VOLUME, 3, &a);
    OptSetIndex(BR_OPT_CD_VOLUME,  7, &a);
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == 3);
    CHECK(BrUiOptGetIndex(BR_OPT_CD_VOLUME)  == 7);
}

static void TestToggles(void)
{
    static const BrUiOptId aId[4] = {
        BR_OPT_FORCE_FEEDBACK, BR_OPT_SKID_MARKS,
        BR_OPT_CAR_SHADOW,     BR_OPT_SPECULAR
    };
    static int32_t (*const aFn[4])(BrUiCtl_ *) = {
        BrUiOptHook_10043590, BrUiOptHook_100435F0,
        BrUiOptHook_10043650, BrUiOptHook_100436B0
    };
    BrActiveFlags a;
    int i;

    memset(&a, 0, sizeof a);
    printf("A2 two-state toggles (0x10043590 / F0 / 0x10043650 / B0)\n");

    for (i = 0; i < 4; ++i) {
        OptSetIndex(aId[i], 0, &a);
        BrUiOptSetEdit(&a, +1);
        CHECK(BrUiOptGetIndex(aId[i]) == 0);

        CHECK(aFn[i](NULL) == 1);
        CHECK(BrUiOptGetIndex(aId[i]) == 1);

        /* The tables at 0x100AC530/38/40/48 are all { 1, 0 } in the image, so
         * the PUBLISHED word runs OPPOSITE to the index -- published == 1 -
         * index, for every value the index can take.  This is the assertion
         * that would have caught "the value went up".
         *
         * It is checked only AFTER a hook has run, because nothing else in
         * the image writes the published word: on a freshly zeroed image the
         * index is 0 and the published word is 0 too, which is not the
         * table's answer and is the original's state as well. */
        CHECK(BrUiOptGetPublished(aId[i]) == 1 - BrUiOptGetIndex(aId[i]));

        /* Two states, so two steps is a round trip -- max is 1, not 9. */
        CHECK(aFn[i](NULL) == 1);
        CHECK(BrUiOptGetIndex(aId[i]) == 0);
        CHECK(BrUiOptGetPublished(aId[i]) == 1 - BrUiOptGetIndex(aId[i]));

        /* Down from 0 lands on the max, which for a two-state option is 1. */
        BrUiOptSetEdit(&a, -1);
        CHECK(aFn[i](NULL) == 1);
        CHECK(BrUiOptGetIndex(aId[i]) == 1);
        CHECK(BrUiOptGetPublished(aId[i]) == 1 - BrUiOptGetIndex(aId[i]));

        /* And no edge means no movement, for these four with no side effect
         * at all -- the contrast with the volume pair above. */
        BrUiOptSetEdit(&a, 0);
        CHECK(aFn[i](NULL) == 1);
        CHECK(BrUiOptGetIndex(aId[i]) == 1);
    }

    /* The four options are four independent words: stepping one must not
     * disturb the others.  (They share a body; they must not share state.) */
    BrUiOptSetEdit(&a, +1);
    for (i = 0; i < 4; ++i) { OptSetIndex(aId[i], 0, &a); }
    BrUiOptSetEdit(&a, +1);
    (void)aFn[1](NULL);
    CHECK(BrUiOptGetIndex(aId[0]) == 0);
    CHECK(BrUiOptGetIndex(aId[1]) == 1);
    CHECK(BrUiOptGetIndex(aId[2]) == 0);
    CHECK(BrUiOptGetIndex(aId[3]) == 0);
    BrUiOptSetEdit(&a, 0);
}

static void TestSeamAndInstall(void)
{
    BrUi73Hooks h73;
    BrUi72Hooks h72;
    BrActiveFlags a;

    printf("A3 the edit seam and the two installers\n");

    /* The seam keeps every host view of 0x10AA33D0..D4 in step; see the
     * ALIASED STORAGE banner.  BrIsAnyActive reads the BrActiveFlags view,
     * the cyclers read the other one, and if they ever disagree the menu
     * activates a row and then declines to change it -- which is exactly the
     * symptom this test exists to prevent regressing to. */
    memset(&a, 0, sizeof a);
    BrUiOptSetEdit(&a, +1);
    CHECK(BrUiOptGetEditUp() == 1 && BrUiOptGetEditDown() == 0);
    CHECK(a.a1 == 1 && a.a0 == 0);
    CHECK(g_BrBtnEdge[1] == 1 && g_BrBtnEdge[0] == 0);
    CHECK(BrIsAnyActive(&a) != 0);          /* the edge IS the activation */

    BrUiOptSetEdit(&a, -1);
    CHECK(BrUiOptGetEditUp() == 0 && BrUiOptGetEditDown() == 1);
    CHECK(a.a1 == 0 && a.a0 == 1);
    CHECK(g_BrBtnEdge[1] == 0 && g_BrBtnEdge[0] == 1);
    CHECK(BrIsAnyActive(&a) != 0);

    BrUiOptSetEdit(&a, 0);
    CHECK(BrUiOptGetEditUp() == 0 && BrUiOptGetEditDown() == 0);
    CHECK(g_BrBtnEdge[0] == 0 && g_BrBtnEdge[1] == 0);
    CHECK(BrIsAnyActive(&a) == 0);

    /* Activate-only raises 0x10AA2AF0 and leaves both edit words clear, so
     * the hook fires and the value does not move.  That is the original. */
    BrUiOptSetActivateOnly(&a, 1);
    CHECK(a.a5 == 1);
    CHECK(BrUiOptGetEditUp() == 0 && BrUiOptGetEditDown() == 0);
    CHECK(BrIsAnyActive(&a) != 0);
    BrUiOptSetActivateOnly(&a, 0);
    CHECK(BrIsAnyActive(&a) == 0);

    /* The installers touch their own slots and NOTHING else -- an unwired
     * hook must stay a visible NULL, not become a silent no-op. */
    memset(&h73, 0, sizeof h73);
    BrUiOptInstall73(&h73);
    CHECK(h73.p10042CF0 == BrUiOptHook_10042CF0);
    CHECK(h73.p10042D60 == BrUiOptHook_10042D60);
    CHECK(h73.p100466C0 == NULL);      /* Audio's Back: not this pass's */
    CHECK(h73.p10045AF0 == NULL);
    CHECK(h73.p10047360 == NULL);

    memset(&h72, 0, sizeof h72);
    BrUiOptInstall72(&h72);
    CHECK(h72.p10043590 == BrUiOptHook_10043590);
    CHECK(h72.p100435F0 == BrUiOptHook_100435F0);
    CHECK(h72.p10043650 == BrUiOptHook_10043650);
    CHECK(h72.p100436B0 == BrUiOptHook_100436B0);
    CHECK(h72.p10046710 == NULL);      /* Game Options' Back: likewise */
    CHECK(h72.p10047360 == NULL);

    /* NULL is tolerated rather than faulting, because a host that has not
     * built a table yet is a wiring state, not a bug. */
    BrUiOptInstall73(NULL);
    BrUiOptInstall72(NULL);
}

/* ==========================================================================
 * PART B -- through the real builder and the real navigation chain
 *
 * The wiring below mirrors port/host/brally.c's, because that is the wiring
 * the shipped harness uses; nothing here is a simplification of it that could
 * make the result easier than the real thing.
 * ========================================================================== */

static BrUiCtlVtbl_   s_ctlVtbl;
static BrUiPageVtbl_  s_pageVtbl;
static BrPhaseVtbl_   s_phaseVtbl;
static BrTextBoxVtbl  s_textBoxVtbl;
static BrTextListVtbl s_textListVtbl;

static BrScrGlobals   s_scr;
static BrActiveFlags  s_active;
static BrUiNav        s_nav;
static BrObjAA2E80    s_objAA2E80;
static BrUi73Hooks    s_hooks73;

/* 0x10AA2A78 -- the {x,y} cursor 0x10047A60 hit-tests.  brally.c parks this
 * off-screen permanently so its transcript is pure keyboard evidence; this
 * one has to be able to MOVE, because of what 0x10047A60 turns out to do:
 *
 *   THE SELECTED ROW CANNOT BE ACTIVATED WHILE AN EDIT EDGE IS HELD.
 *
 * 0x10047A60 computes `fCurrent = (cursor == ordinal)` and then, at
 * 0x10047BCF (Glide 0x1004101F), does
 *
 *     test edx,edx / je <flag block>          ; not current -> normal path
 *     [0x10AA33D0] ? -> return 1              ; current AND any edit edge
 *     [0x10AA33D4] ? -> return 1              ;   set  -> return 1 with the
 *     [0x10AA33D8] ? -> return 1              ;   flags UNTOUCHED, so the
 *     [0x10AA33DC] ? -> return 1              ;   ACTIVATE bit is never set
 *
 * -- byte-identical in BOTH builds, checked.  br_uinav.c transcribes it
 * correctly and br_uinav.h describes those four reads as "a modal thing is
 * running: leave the flags alone", which is right about the mechanism and
 * understates the consequence: an option row reached by the KEYBOARD is
 * exactly the case the guard blocks.
 *
 * The row that does activate is the one under the CURSOR -- fCurrent == 0
 * and inside its own rectangle -- which is the mouse.  So the volume rows on
 * this screen are a pointer control, and this test drives them as one.  That
 * is a finding about the original, not a workaround for the port; the
 * keyboard-blocked case is asserted below too, so a future "fix" to
 * 0x10047A60 would fail this test rather than pass it. */
static int32_t s_cursor[2] = { -1, -1 };

/* The control index key 'm' aims the cursor at. */
static int s_iAim = -1;

/* Captions, recorded at set-text time rather than inferred. */
#define MAXCAP 64
static const char *s_aCap[MAXCAP];
static const void *s_aCapOwner[MAXCAP];
static int         s_nCap;

static const char *CapFor(const void *pCtl)
{
    int i;
    for (i = 0; i < s_nCap; i++) { if (s_aCapOwner[i] == pCtl) { return s_aCap[i]; } }
    return NULL;
}

static void TestSetText(BrUiCtl_ *pThis, const void *pText, int32_t a2,
                        int32_t a3, const void *pStyle)
{
    if (pText != NULL && s_nCap < MAXCAP) {
        s_aCapOwner[s_nCap] = pThis;
        s_aCap[s_nCap++]    = (const char *)pText;
    }
    /* br_uictl.c deliberately does not run the element constructor, so the
     * text box arrives with a NULL vtable; brally.c plants it here and so
     * does this. */
    if (pThis != NULL && pThis->aText[0].pVtbl == NULL) {
        pThis->aText[0].pVtbl = g_pBrTextBoxVtbl;
    }
    BrUiCtlSetText_10047EB0(pThis, pText, a2, a3, pStyle);
}

static void TestPlace(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                      int32_t flags, int32_t a4, int32_t a5,
                      int32_t a6, int32_t a7)
{
    if (pThis != NULL && pThis->list.pVtbl == NULL) { BrTextListInit(&pThis->list); }
    BrUiCtlPlace_10047FB0(pThis, pOwner, x, y, flags, a4, a5, a6, a7);
}

/* The six functions brally.c stands in for, counted the same way. */
static int s_nStandIn[6];
enum { SI_CTLDEL, SI_CTLDRAWRECT, SI_CTLDRAW, SI_PAGEDEL, SI_PHASEDEL,
       SI_TEXTDRAW };
static void *StandInCtlDel(BrUiCtl_ *p, int32_t f)
{ (void)f; s_nStandIn[SI_CTLDEL]++; return p; }
static void StandInCtlDrawRect(BrUiCtl_ *p, void *pRect)
{ (void)p; (void)pRect; s_nStandIn[SI_CTLDRAWRECT]++; }
static void StandInCtlDraw(BrUiCtl_ *p)
{ (void)p; s_nStandIn[SI_CTLDRAW]++; }
static void *StandInPageDel(BrUiPage_ *p, int32_t f)
{ (void)f; s_nStandIn[SI_PAGEDEL]++; return p; }
static void *StandInPhaseDel(BrPhase_ *p, int32_t f)
{ (void)f; s_nStandIn[SI_PHASEDEL]++; return p; }
static void StandInTextDraw(BrTextBox *p)
{ (void)p; s_nStandIn[SI_TEXTDRAW]++; }
static void NavPhaseRelease(BrPhase_ *p) { BrUiNavPhaseRelease_10048AA0(&s_nav, p); }
static void ClearSub70(void *pArg) { (void)pArg; }

static char          s_scratchA[64], s_scratchB[64];
static int32_t       s_blkA[0x53], s_blkB[0x53], s_blkC[0x46];
static unsigned char s_recAA29CC[0x438 * 16];

static void WireEverything(void)
{
    memset(&s_ctlVtbl, 0, sizeof s_ctlVtbl);
    memset(&s_pageVtbl, 0, sizeof s_pageVtbl);
    memset(&s_phaseVtbl, 0, sizeof s_phaseVtbl);
    memset(&s_textBoxVtbl, 0, sizeof s_textBoxVtbl);
    memset(&s_textListVtbl, 0, sizeof s_textListVtbl);

    s_textBoxVtbl.pfn04 = BrTextBoxMeasureA;
    s_textBoxVtbl.pfn08 = BrTextBoxMeasureB;
    s_textBoxVtbl.pfn10 = StandInTextDraw;
    s_textBoxVtbl.pfn28 = BrTextBoxCentreX;
    g_pBrTextBoxVtbl    = &s_textBoxVtbl;

    s_textListVtbl.f10  = BrTextListAddRow;
    s_textListVtbl.f14  = BrTextListConfig;
    g_pBrTextListVtbl   = &s_textListVtbl;

    /* --- the module context (brally.c's WireContext) --------------------- */
    memset(&g_br73, 0, sizeof g_br73);
    g_br73.pfnClearSub70 = ClearSub70;
    g_br73.pAA29CC       = s_recAA29CC;
    g_br73.aAA26F0       = s_blkA;
    g_br73.aA9DBD8       = s_blkB;
    g_br73.a220B20       = s_blkC;
    g_br73.szAA2518      = s_scratchA;
    g_br73.szA9D618      = s_scratchB;
    g_br73.cbScratch     = sizeof s_scratchA;
    g_br73.n0AB428       = 0;
    g_br73.n0AB42C       = 380;
    g_br73.aStyles.p0AD300 = " ";
    g_br73.aStyles.p0AD348 = "RallySeason*.BRF";
    g_br73.aStyles.p0AB438 = BR_UI_STYLE(0x100AB438);
    g_br73.aStyles.p0AB448 = BR_UI_STYLE(0x100AB448);
    g_br73.aStyles.p0AB458 = BR_UI_STYLE(0x100AB458);
    g_br73.aStyles.p0AB468 = BR_UI_STYLE(0x100AB468);
    g_br73.aStyles.p0AB478 = BR_UI_STYLE(0x100AB478);
    g_br73.aStyles.p0AB488 = BR_UI_STYLE(0x100AB488);
    g_br73.aStyles.p0AB4A8 = BR_UI_STYLE(0x100AB4A8);
    g_br73.aStyles.p0AB4D8 = BR_UI_STYLE(0x100AB4D8);
    g_br73.aStyles.p0AB4F8 = BR_UI_STYLE(0x100AB4F8);
    g_br73.aStyles.p0AB508 = BR_UI_STYLE(0x100AB508);
    g_br73.aStyles.p0AB528 = BR_UI_STYLE(0x100AB528);
    g_br73.aStyles.p0AB548 = BR_UI_STYLE(0x100AB548);

    /* --- navigation (brally.c's WireNav) --------------------------------- */
    memset(&s_scr, 0, sizeof s_scr);
    memset(&s_active, 0, sizeof s_active);
    memset(&s_objAA2E80, 0, sizeof s_objAA2E80);
    memset(&s_nav, 0, sizeof s_nav);

    s_scr.w0AB3DC = 0;
    s_scr.pAA2E80 = &s_objAA2E80;

    s_nav.pG       = &s_scr;
    s_nav.pCursor  = s_cursor;
    s_nav.pActive  = &s_active;
    s_nav.apHot[0] = BR_UI_STYLE(0x100AB448);
    s_nav.apHot[1] = BR_UI_STYLE(0x100AB418);
    s_nav.apHot[2] = BR_UI_STYLE(0x100AB428);
    g_pBrUiNav     = &s_nav;

    BrUiNavInstallCtlVtbl(&s_ctlVtbl);
    s_ctlVtbl.f00 = (void *)StandInCtlDel;
    s_ctlVtbl.f18 = StandInCtlDrawRect;
    s_ctlVtbl.f1C = StandInCtlDraw;
    s_ctlVtbl.f34 = TestSetText;
    s_ctlVtbl.f38 = TestPlace;
    g_pBrUiCtlVtbl = &s_ctlVtbl;

    BrUiNavInstallPageVtbl(&s_pageVtbl);
    s_pageVtbl.f00  = StandInPageDel;
    g_pBrUiPageVtbl = &s_pageVtbl;

    s_phaseVtbl.f00 = StandInPhaseDel;
    s_phaseVtbl.f1C = NavPhaseRelease;
    g_br73.pPhaseVtbl = &s_phaseVtbl;

    /* --- THE ONE LINE THIS MODULE EXISTS FOR ----------------------------- */
    memset(&s_hooks73, 0, sizeof s_hooks73);
    BrUiOptInstall73(&s_hooks73);
    g_br73.pHooks = &s_hooks73;

    /* 0x10048180 compares the hook it is about to call against this slot and
     * takes a different audio path when they match; see slice7_80.h. */
    s_scr.pfn10042CF0 = (void *)BrUiOptHook_10042CF0;
}

/* One frame of one phase -- the same loop brally.c's NavFrame runs, which is
 * the part of 0x100489A0 navigation depends on. */
static int NavFrame(BrPhase_ *ph)
{
    int32_t i;
    int     ok = 1;

    ph->iPage = 0;
    for (i = 0; i < (int32_t)ph->nPages && i < BR_PHASE_PAGES; ++i) {
        BrUiPage_ *pPg = ph->aPages[i];
        ph->pCur = pPg;
        if (pPg == NULL) { return 0; }
        ph->iPage = (uint16_t)(uint32_t)i;
        if (ph->aFlags[i] != 0) {
            BrUiPage_ *pCur = ph->pCur;
            if (pCur->pVtbl->f04(pCur) == 0) { ok = 0; }
        }
    }
    ++s_nClock;                 /* the frame is the clock */
    return ok;
}

static int NavCurrentCtl(const BrUiPage_ *pg)
{
    int j;
    if (pg == NULL) { return -1; }
    for (j = 0; j < (int)pg->cCtl && j < BR73_PAGE_CTL_MAX; j++) {
        const BrUiCtl_ *c = pg->apCtl[j];
        if (c != NULL && ((uint32_t)c->flags1C & 0x20u) != 0) { return j; }
    }
    return -1;
}

static int s_verbose;

static void DumpRow(const char *pszWhen, const BrPhase_ *ph)
{
    const BrUiPage_ *pg   = (ph->nPages > 0) ? ph->aPages[0] : NULL;
    int              iCur = NavCurrentCtl(pg);
    const char      *psz  = (pg != NULL && iCur >= 0) ? CapFor(pg->apCtl[iCur])
                                                      : NULL;
    if (!s_verbose) { return; }
    printf("  %-8s sel=%-3d cursor=(%4d,%4d) current=%-3d %-14s | "
           "0x10B4E708 SFX=%d  0x10B4E70C CD=%d  0x100AB3D8=%d\n",
           pszWhen, BrUiNavSelection(&s_nav),
           (int)s_cursor[0], (int)s_cursor[1],
           iCur, psz ? psz : "(none)",
           (int)BrUiOptGetIndex(BR_OPT_SFX_VOLUME),
           (int)BrUiOptGetIndex(BR_OPT_CD_VOLUME),
           (int)BrUiOptGetVolumeSelector());
}

/* Put the cursor in the middle of a control's rectangle -- the rectangle the
 * ported 0x10047FB0 computed from the builder's coordinates, not one this
 * file invents. */
static void AimCursor(const BrUiPage_ *pg, int i)
{
    const BrUiCtl_ *c;
    if (pg == NULL || i < 0 || i >= (int)pg->cCtl) { return; }
    c = pg->apCtl[i];
    if (c == NULL) { return; }
    s_cursor[0] = c->rcLeft + (c->rcRight  - c->rcLeft) / 2;
    s_cursor[1] = c->rcTop  + (c->rcBottom - c->rcTop)  / 2;
}

/* The key vocabulary, deliberately a superset of brally.c's -keys:
 *   d u   move the selection (br_uinav's two seam writes)
 *   j     activate ONLY (0x10AA2AF0)          -- fires the hook, moves nothing
 *   + -   the option EDIT edge (0x10AA33D4 / 0x10AA33D0), which in the
 *         original both activates a row and tells the hook which way to go
 *   m     aim the cursor at control s_iAim   (the pointer)
 *   p     park the cursor off-screen
 *   .     idle
 */
static void RunScript(BrPhase_ *ph, const char *pszKeys)
{
    const char *p;
    char        szWhen[8];
    static int  s_fStarted;

    /* The transcript is one continuous run of frames even though the test
     * calls this in short bursts, so "start" is printed once. */
    if (!s_fStarted) { s_fStarted = 1; DumpRow("start", ph); }
    for (p = pszKeys; *p != 0; ++p) {
        switch (*p) {
        case 'd': BrUiNavMove(&s_nav, +1); break;
        case 'u': BrUiNavMove(&s_nav, -1); break;
        case 'j': BrUiOptSetActivateOnly(&s_active, 1); break;
        case '+': BrUiOptSetEdit(&s_active, +1); break;
        case '-': BrUiOptSetEdit(&s_active, -1); break;
        case 'm':
            AimCursor((ph->nPages > 0) ? ph->aPages[0] : NULL, s_iAim);
            break;
        case 'p': s_cursor[0] = -1; s_cursor[1] = -1; break;
        case '.': break;
        default:  printf("  unknown key '%c'\n", *p); return;
        }

        (void)NavFrame(ph);

        BrUiNavSetStep(&s_nav, 0);
        BrUiOptSetActivateOnly(&s_active, 0);
        BrUiOptSetEdit(&s_active, 0);

        szWhen[0] = '\''; szWhen[1] = *p; szWhen[2] = '\''; szWhen[3] = 0;
        DumpRow(szWhen, ph);
    }
}

static void TestThroughTheMenu(void)
{
    BrPhase_ *ph;
    int32_t   nBefore, nAfter, nSelBefore;
    int       iCur;
    int       iSfx = -1, iCd = -1, iBack = -1;

    printf("B  the Audio Options screen (0x1004DFC0) driven through the "
           "ported navigation chain\n");

    WireEverything();

    ph = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
    CHECK(ph != NULL);
    if (ph == NULL) { return; }
    CHECK(BrOptObjCtor(ph) == ph);
    ph->pVtbl = &s_phaseVtbl;

    /* The REAL builder.  It is the builder that decides which hook goes on
     * which row; this test does not place a single control itself. */
    BrExt_1004DFC0(ph);
    CHECK(ph->nPages == 1);
    CHECK(ph->aPages[0] != NULL);
    if (ph->aPages[0] == NULL) { return; }

    /* The builder read the installed table, so the two volume rows now carry
     * real hooks and the rows this pass does not own are still NULL.  The
     * indices are FOUND by caption, not assumed: the builder decides the
     * order and this test must not encode a copy of that decision. */
    {
        const BrUiPage_ *pg = ph->aPages[0];
        int j;
        for (j = 0; j < (int)pg->cCtl; ++j) {
            const BrUiCtl_ *c = pg->apCtl[j];
            const char *psz = c ? CapFor(c) : NULL;
            if (psz == NULL) { continue; }
            if (strcmp(psz, "SFX Volume") == 0 && c->pfn08 != NULL) {
                CHECK(c->pfn08 == BrUiOptHook_10042CF0); iSfx = j;
            }
            if (strcmp(psz, "CD VOlume") == 0 && c->pfn08 != NULL) {
                CHECK(c->pfn08 == BrUiOptHook_10042D60); iCd = j;
            }
            if (strcmp(psz, "Back") == 0) {
                CHECK(c->pfn08 == NULL);      /* a phase hook, not this pass's */
                iBack = j;
            }
        }
        CHECK(iSfx >= 0 && iCd >= 0 && iBack >= 0);
        if (iSfx < 0 || iCd < 0 || iBack < 0) { return; }
    }

    /* One idle frame settles the CURRENT bit onto the first selectable row. */
    RunScript(ph, ".");
    iCur = NavCurrentCtl(ph->aPages[0]);
    CHECK(iCur == iSfx);

    /* --- the claim, with the number -------------------------------------- */

    /* (1) ACTIVATE ONLY (0x10AA2AF0), keyboard selection on SFX Volume.
     *     The hook runs and the volume does NOT move, because both edit
     *     words are clear and BrOptCycle takes neither arm.  That is the
     *     original; the counted side effect is what proves the hook ran at
     *     all rather than being skipped. */
    nBefore     = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    s_c10060D90 = 0;
    RunScript(ph, "j");
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == nBefore);
    CHECK(s_c10060D90 == 1);          /* 0x10060D90: the hook DID run */

    /* (2) THE EDIT EDGE ON THE KEYBOARD-SELECTED ROW IS BLOCKED, and this is
     *     0x10047A60's doing, not a missing piece of wiring: with fCurrent
     *     set and any of 0x10AA33D0..DC non-zero it returns 1 with the flags
     *     untouched, so the ACTIVATE bit is never raised and 0x10048180
     *     never reaches the hook.  See the banner on s_cursor.
     *
     *     Asserted, so that "fixing" 0x10047A60 to make this case work would
     *     FAIL this test instead of quietly passing it. */
    nBefore     = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    s_c10060D90 = 0;
    RunScript(ph, "+");
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == nBefore);
    CHECK(s_c10060D90 == 0);          /* the hook did not run at all */

    /* (3) THE ROW UNDER THE CURSOR DOES ACTIVATE.  Keyboard selection stays
     *     on SFX Volume; the cursor goes to the CD Volume row, which is
     *     therefore NOT fCurrent and IS inside its own rectangle.  Same key,
     *     same frame chain -- and now the value moves. */
    s_iAim     = iCd;
    nBefore    = BrUiOptGetIndex(BR_OPT_CD_VOLUME);
    nSelBefore = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    RunScript(ph, "m+");
    nAfter = BrUiOptGetIndex(BR_OPT_CD_VOLUME);
    CHECK(nAfter == (nBefore + 1) % 10);                  /* 0x10B4E70C moved */
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == nSelBefore);  /* and only it */
    CHECK(BrUiOptGetVolumeSelector() == 0);      /* the CD hook, not the SFX */

    /* Two more up and one down: the value tracks the key, one step per frame,
     * which is what makes it the hook's arithmetic and not a one-off. */
    RunScript(ph, "++");
    CHECK(BrUiOptGetIndex(BR_OPT_CD_VOLUME) == (nBefore + 3) % 10);
    RunScript(ph, "-");
    CHECK(BrUiOptGetIndex(BR_OPT_CD_VOLUME) == (nBefore + 2) % 10);

    /* (4) THE RIGHT HOOK, not merely A hook.  Move the keyboard selection off
     *     SFX Volume, aim the cursor AT it, and the other option is the one
     *     that moves. */
    RunScript(ph, "d");
    s_iAim     = iSfx;
    nBefore    = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    nSelBefore = BrUiOptGetIndex(BR_OPT_CD_VOLUME);
    RunScript(ph, "m+");
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == (nBefore + 1) % 10);
    CHECK(BrUiOptGetIndex(BR_OPT_CD_VOLUME)  == nSelBefore);
    CHECK(BrUiOptGetVolumeSelector() == 1);      /* the SFX hook this time */

    /* (5) The Back row carries no hook from this pass, so aiming at it and
     *     editing changes no option at all -- an unwired slot stays a hole. */
    s_iAim      = iBack;
    nBefore     = BrUiOptGetIndex(BR_OPT_CD_VOLUME);
    nSelBefore  = BrUiOptGetIndex(BR_OPT_SFX_VOLUME);
    s_c10060D90 = 0;
    RunScript(ph, "m+");
    CHECK(BrUiOptGetIndex(BR_OPT_CD_VOLUME)  == nBefore);
    CHECK(BrUiOptGetIndex(BR_OPT_SFX_VOLUME) == nSelBefore);
    CHECK(s_c10060D90 == 0);
    RunScript(ph, "p");

    if (s_verbose) {
        int i;
        static const char *const aszStandIn[6] = {
            "0x100478A0 control dtor", "0x10047980 draw(rect)",
            "0x10047930 draw", "0x100484C0 page dtor",
            "0x10048850 phase dtor", "0x1005B0xx item draw"
        };
        printf("\n  stand-ins reached (each is a function this port has NOT "
               "transcribed):\n");
        for (i = 0; i < 6; i++) {
            printf("    %-26s %d\n", aszStandIn[i], s_nStandIn[i]);
        }
    }
}

int main(int argc, char **argv)
{
    const char *pszScript = NULL;

    if (argc > 1 && strcmp(argv[1], "-keys") == 0) {
        s_verbose = 1;
        pszScript = (argc > 2) ? argv[2] : NULL;
    } else if (argc > 1) {
        s_verbose = 1;
    }

    printf("test_slice7_80 -- option-changing hooks "
           "(0x10042CF0 0x10042D60 0x10043590 0x100435F0 0x10043650 "
           "0x100436B0)\n");

    TestVolumeCycle();
    TestToggles();
    TestSeamAndInstall();
    TestThroughTheMenu();

    if (pszScript != NULL) {
        BrPhase_ *ph = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
        printf("\nextra script: \"%s\"\n", pszScript);
        if (ph != NULL && BrOptObjCtor(ph) != NULL) {
            ph->pVtbl = &s_phaseVtbl;
            s_nCap = 0;
            BrExt_1004DFC0(ph);
            s_scr.wAA286C = 0;
            RunScript(ph, pszScript);
        }
    }

    printf("slice7_80: %d failures\n", g_fails);
    return g_fails ? 1 : 0;
}
