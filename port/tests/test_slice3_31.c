/* test_slice3_31.c -- behaviour and invariant tests for slice3_31.c.
 *
 * Every stand-in for a cross-slice callee lives HERE and nowhere else, as the
 * contract requires. They are the minimum needed to observe behaviour: each
 * one records that it ran and, where the original's semantics depend on it,
 * lets the test drive what it does.
 *
 * The tests assert properties of the mechanism -- ordering, the three-outcome
 * return, the documented re-read gotcha, the switch table, the sign extension
 * in the key ring -- not the particular globals any one routine happens to
 * touch.
 */
#include "slice3_31.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_cFail;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_cFail++;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * Stand-ins  (TEST FILE ONLY)
 * ========================================================================== */

/* --- the allocator ------------------------------------------------------- */

static int  g_cNew;         /* how many operator new calls have happened  */
static int  g_fNewFails;    /* when set, operator new returns NULL        */

void *BrOperatorNew(uint32_t cb)
{
    g_cNew++;
    if (g_fNewFails)
        return NULL;
    /* operator new does NOT zero -- fill with a non-zero pattern so any code
     * that relies on zeroing is caught. */
    {
        void *p = malloc(cb);
        if (p != NULL)
            memset(p, 0xA5, cb);
        return p;
    }
}

/* --- the phase vtable and constructor ------------------------------------ */

static int g_cF00;              /* slot +0x00 invocations */
static int g_cF1C;              /* slot +0x1C invocations */
static int g_nLastF00Arg;

static void StubPhaseF00(BrPhase *pThis, int32_t a)
{
    (void)pThis;
    g_cF00++;
    g_nLastF00Arg = a;
}

static void StubPhaseF1C(BrPhase *pThis) { (void)pThis; g_cF1C++; }

static const BrPhaseVtblExt g_PhaseVtbl = {
    { StubPhaseF00 },
    { NULL, NULL, NULL, NULL, NULL, NULL },
    StubPhaseF1C
};

BrPhase *BrPhaseCtor(BrPhase *pThis)
{
    pThis->pVtbl = &g_PhaseVtbl.base;
    pThis->pfn04 = NULL;
    pThis->pfn08 = NULL;
    pThis->f0C   = 0;
    pThis->f68   = 0;
    return pThis;
}

/* --- the game object ------------------------------------------------------ */

static int g_cSlot7;

static void StubSlot7(BrGameSub *pThis) { (void)pThis; g_cSlot7++; }

static const BrGameSubVtbl g_SubVtbl = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, StubSlot7
};

static BrGameSub g_Sub;

/* Storage for the game object. It must be at least BR_GAMEOBJ31_MIN_SIZE
 * bytes because BrSub10047360 reaches +0x3850. */
static unsigned char g_abObj[BR_GAMEOBJ31_MIN_SIZE];

static BrGameObj *ObjReset(void)
{
    BrGameObj *p = (BrGameObj *)(void *)g_abObj;
    memset(g_abObj, 0, sizeof(g_abObj));
    g_Sub.pVtbl = &g_SubVtbl;
    p->pSub     = &g_Sub;
    return p;
}

static uint32_t ObjLd32(unsigned off)
{
    uint32_t v;
    memcpy(&v, g_abObj + off, sizeof(v));
    return v;
}
static void ObjSt32(unsigned off, uint32_t v)
{
    memcpy(g_abObj + off, &v, sizeof(v));
}
static uint16_t ObjLd16(unsigned off)
{
    uint16_t v;
    memcpy(&v, g_abObj + off, sizeof(v));
    return v;
}
static void ObjSt16(unsigned off, uint16_t v)
{
    memcpy(g_abObj + off, &v, sizeof(v));
}

/* --- enter hooks ---------------------------------------------------------- */

static int      g_cEnter;
static BrPhase *g_pLastEnter;
/* When non-NULL, every enter hook repoints the current phase at this object
 * before returning. That is what exercises the documented re-read gotcha. */
static BrPhase     *g_pHijack;
static BrPhaseCtx  *g_pCtxForHijack;

static void EnterCommon(BrPhase *pSelf)
{
    g_cEnter++;
    g_pLastEnter = pSelf;
    if (g_pHijack != NULL)
        g_pCtxForHijack->pAA2904 = g_pHijack;
}

#define ENTER_STUB(name) void name(BrPhase *pSelf) { EnterCommon(pSelf); }
ENTER_STUB(BrExt_10049C20)
ENTER_STUB(BrExt_10049F40)
ENTER_STUB(BrExt_1004A260)
ENTER_STUB(BrPhaseEnterPlaceholder_1004B430)
ENTER_STUB(BrExt_1004F2B0)
ENTER_STUB(BrExt_1004F700)
ENTER_STUB(BrExt_100509F0)
ENTER_STUB(BrExt_10050060)
ENTER_STUB(BrExt_10052030)
ENTER_STUB(BrExt_10052F50)
ENTER_STUB(BrExt_10053CF0)
ENTER_STUB(BrExt_10054B50)

/* --- everything else ------------------------------------------------------ */

static int      g_fA00Ok = 1;   /* what 0x10045A00 reports                  */
static int      g_c419D0;       /* 0x100419D0 calls                         */
static void    *g_p419D0Last;
static int      g_c72AF0;
static int32_t  g_n72AF0A;
static uint32_t g_n72AF0B;
static int      g_f3E0E0;       /* what 0x1003E0E0 reports                  */
static int      g_c47660;
static int      g_c79550;
static int      g_c3E310;
static int      g_c6A4A0;
static void    *g_p6A4A0This;
static void    *g_p6A4A0Arg;
static int      g_cEdit;        /* any of 41A00/41AC0/42410/424D0           */
static int      g_nEditWhich;
static int      g_c43260, g_c43330;
static int      g_c5FBC0;
static int      g_c8B80, g_c3DFC0, g_c3E510, g_c3E680;
static void    *g_p74030 = (void *)"msg";

int32_t BrExt_10045A00(void)              { return g_fA00Ok; }
void   *BrExt_10074030(int32_t n)         { (void)n; return g_p74030; }
void    BrExt_100419D0(void *p)           { g_c419D0++; g_p419D0Last = p; }
void    BrExt_10072AF0(int32_t a, uint32_t b)
                                          { g_c72AF0++; g_n72AF0A = a;
                                            g_n72AF0B = b; }
int32_t BrExt_1003E0E0(void)              { return g_f3E0E0; }
void    BrExt_1003E310(void)              { g_c3E310++; }
void    BrExt_10079550(void)              { g_c79550++; }
void    BrExt_1005FBC0(int32_t a)         { (void)a; g_c5FBC0++; }
int32_t BrExt_10041A00(void *p)           { (void)p; g_cEdit++; g_nEditWhich = 1; return 7; }
int32_t BrExt_10041AC0(void *p)           { (void)p; g_cEdit++; g_nEditWhich = 2; return 7; }
int32_t BrExt_10042410(void *p)           { (void)p; g_cEdit++; g_nEditWhich = 3; return 7; }
int32_t BrExt_100424D0(void *p)           { (void)p; g_cEdit++; g_nEditWhich = 4; return 7; }
void    BrExt_10043260(void *p)           { (void)p; g_c43260++; }
void    BrExt_10043330(void *p)           { (void)p; g_c43330++; }
void    BrExt_1006A4A0(void *pThis, void *pArg)
                                          { g_c6A4A0++; g_p6A4A0This = pThis;
                                            g_p6A4A0Arg = pArg; }
void    BrExt_10047660(void)              { g_c47660++; }
void    BrExt_10008B80(void)              { g_c8B80++; }
void    BrExt_1003DFC0(void)              { g_c3DFC0++; }
void    BrExt_1003E510(void)              { g_c3E510++; }
void    BrExt_1003E680(void)              { g_c3E680++; }

/* --- slice2_26 routines this module calls --------------------------------- */

static int g_c451E0, g_c45110, g_c44CB0;

int BrPhaseActivate_100451E0(BrPhaseCtx *pCtx)
{
    /* Enough of the real thing to be observable: publish a phase. */
    g_c451E0++;
    if (pCtx->pAA2918 == NULL) {
        BrPhase *p = (BrPhase *)BrOperatorNew(BR_PHASE_ORIG_SIZE);
        pCtx->pAA2918 = (p != NULL) ? BrPhaseCtor(p) : NULL;
    }
    pCtx->pAA2904 = pCtx->pAA2918;
    return pCtx->pAA2918 != NULL;
}

int BrPhaseActivate_10045110(BrPhaseCtx *pCtx) { (void)pCtx; g_c45110++; return 1; }

int BrPhaseLeave_10044CB0(BrPhaseCtx *pCtx, void *pEntity)
{
    (void)pCtx; (void)pEntity;
    g_c44CB0++;
    return 0;
}

/* --- br_pod ---------------------------------------------------------------- */

static int   g_cPodRead;
static int   g_iPodLast;
static void *g_pvPodLast;

int BrPodRead(BrPod *pPod, int iEntry, void *pvBuffer)
{
    (void)pPod;
    g_cPodRead++;
    g_iPodLast  = iEntry;
    g_pvPodLast = pvBuffer;
    return 1;               /* failure -- BrPodLoadInto must ignore it */
}

/* ==========================================================================
 * Fixtures
 * ========================================================================== */

static BrPhaseCtx   g_Base;
static BrPhaseCtx31 g_Ext;

static void ResetAll(void)
{
    memset(&g_Base, 0, sizeof(g_Base));
    memset(&g_Ext,  0, sizeof(g_Ext));
    BrPhase31SetCtx(&g_Base, &g_Ext);

    g_cNew = 0; g_fNewFails = 0;
    g_cF00 = 0; g_cF1C = 0; g_nLastF00Arg = -1;
    g_cSlot7 = 0;
    g_cEnter = 0; g_pLastEnter = NULL;
    g_pHijack = NULL; g_pCtxForHijack = &g_Base;
    g_fA00Ok = 1; g_c419D0 = 0; g_p419D0Last = NULL;
    g_c72AF0 = 0; g_n72AF0A = 0; g_n72AF0B = 0;
    g_f3E0E0 = 0; g_c47660 = 0; g_c79550 = 0; g_c3E310 = 0; g_c6A4A0 = 0;
    g_cEdit = 0; g_nEditWhich = 0; g_c43260 = 0; g_c43330 = 0; g_c5FBC0 = 0;
    g_c8B80 = 0; g_c3DFC0 = 0; g_c3E510 = 0; g_c3E680 = 0;
    g_c451E0 = 0; g_c45110 = 0; g_c44CB0 = 0;
    g_cPodRead = 0; g_iPodLast = -1; g_pvPodLast = NULL;
    (void)ObjReset();
}

/* A phase that already exists, so the "already built" path can be taken. */
static BrPhase *MakePhase(void)
{
    BrPhase *p = (BrPhase *)malloc(BR_PHASE_ORIG_SIZE);
    return BrPhaseCtor(p);
}

/* ==========================================================================
 * 0x10008850
 * ========================================================================== */

static void test_pod_load_into(void)
{
    char       abBuf[8];
    BrPod      pod;
    void      *pv;

    ResetAll();
    memset(&pod, 0, sizeof(pod));

    pv = BrPodLoadInto(&pod, 3, abBuf);

    /* The buffer comes back unchanged, whatever ReadPod said. */
    CHECK(pv == (void *)abBuf);
    CHECK(g_cPodRead == 1);
    /* Argument order survives: index first, buffer second. */
    CHECK(g_iPodLast == 3);
    CHECK(g_pvPodLast == (void *)abBuf);
}

/* ==========================================================================
 * ACTIVATE: the three outcomes
 * ========================================================================== */

static void test_activate_three_outcomes(void)
{
    BrPhase *pFirst;

    /* (a) not yet built, allocation succeeds -> 1, published, hooked, flagged */
    ResetAll();
    CHECK(BrPhaseActivate_10045AF0() == 1);
    CHECK(g_Ext.pAA2924 != NULL);
    CHECK(g_Base.pAA2904 == g_Ext.pAA2924);
    CHECK(g_cEnter == 1);
    CHECK(g_pLastEnter == g_Ext.pAA2924);
    CHECK(g_Ext.pAA2924->f0C == 1);
    CHECK(g_Ext.pAA2924->f68 == 1);
    CHECK(g_cNew == 1);
    pFirst = g_Ext.pAA2924;

    /* (b) already built -> 1, nothing allocated, no hook, flags untouched */
    g_Ext.pAA2924->f0C = 77;
    g_cNew = 0; g_cEnter = 0;
    g_Base.pAA2904 = NULL;
    CHECK(BrPhaseActivate_10045AF0() == 1);
    CHECK(g_Ext.pAA2924 == pFirst);
    CHECK(g_Base.pAA2904 == pFirst);
    CHECK(g_cNew == 0);
    CHECK(g_cEnter == 0);
    CHECK(pFirst->f0C == 77);

    /* (c) allocation fails -> 0, and BOTH globals are left NULL */
    ResetAll();
    g_fNewFails = 1;
    CHECK(BrPhaseActivate_10045AF0() == 0);
    CHECK(g_Ext.pAA2924 == NULL);
    CHECK(g_Base.pAA2904 == NULL);
    CHECK(g_cEnter == 0);
}

/* The gotcha slice2_26.h documents and this range inherits: the current-phase
 * global is re-read after the enter hook, so a hook that repoints it makes the
 * f0C/f68 stores land on the OTHER object. */
static void test_activate_reread_gotcha(void)
{
    BrPhase *pOther;

    ResetAll();
    pOther = MakePhase();
    g_pHijack = pOther;

    CHECK(BrPhaseActivate_10045EA0() == 1);

    /* The object that was built is still in its own slot ... */
    CHECK(g_Ext.pAA2934 != NULL);
    CHECK(g_Ext.pAA2934 != pOther);
    /* ... but the flags went to the hijacker, not to it. */
    CHECK(pOther->f0C == 1);
    CHECK(pOther->f68 == 1);
    CHECK(g_Ext.pAA2934->f0C != 1);
    CHECK(g_Ext.pAA2934->f68 != 1);
    CHECK(g_Base.pAA2904 == pOther);

    free(pOther);
}

/* 0x10045C90 / 0x10045F70 build a SECOND object which gets f0C and not f68,
 * and only on the just-built path of the first. */
static void test_activate_second_object(void)
{
    ResetAll();
    CHECK(BrPhaseActivate_10045F70() == 1);
    CHECK(g_Ext.pAA2938 != NULL);
    CHECK(g_Ext.pAA2978 != NULL);
    CHECK(g_Ext.pAA2978->f0C == 1);
    CHECK(g_Ext.pAA2978->f68 != 1);     /* deliberately NOT set */
    CHECK(g_cNew == 2);

    /* Re-running takes the already-built path: no second object is rebuilt. */
    g_cNew = 0;
    CHECK(BrPhaseActivate_10045F70() == 1);
    CHECK(g_cNew == 0);

    /* 0x10045C90 has the same shape. */
    ResetAll();
    BrExt_10045C90(NULL);
    CHECK(g_Ext.pAA292C != NULL);
    CHECK(g_Ext.pAA2974 != NULL);
    CHECK(g_Ext.pAA2974->f0C == 1);
    CHECK(g_Ext.pAA2974->f68 != 1);
}

/* 0x10045900's guard: a refusal builds nothing and reports. */
static void test_activate_10045900_guard(void)
{
    ResetAll();
    g_fA00Ok = 0;
    CHECK(BrPhaseActivate_10045900() == 0);
    CHECK(g_Ext.pAA291C == NULL);
    CHECK(g_cNew == 0);
    CHECK(g_c419D0 == 1);
    CHECK(g_p419D0Last == g_p74030);    /* the message, not p0AD300 */

    ResetAll();
    g_Base.p0AD300 = (void *)&g_Base;
    g_fA00Ok = 1;
    CHECK(BrPhaseActivate_10045900() == 1);
    CHECK(g_Ext.pAA291C != NULL);
    CHECK(g_c419D0 == 1);
    CHECK(g_p419D0Last == g_Base.p0AD300);
}

/* 0x10046170 sets nAA2854 on BOTH paths -- including the already-built one. */
static void test_activate_10046170_prologue_on_both_paths(void)
{
    ResetAll();
    CHECK(BrPhaseActivate_10046170() == 1);
    CHECK(g_Ext.nAA2854 == 3);
    CHECK(g_c72AF0 == 1);
    CHECK(g_n72AF0A == 3);
    CHECK(g_n72AF0B == 0x00200020u);

    g_Ext.nAA2854 = 0;
    g_c72AF0 = 0;
    CHECK(BrPhaseActivate_10046170() == 1);     /* already built */
    CHECK(g_cEnter == 1);                        /* hook did NOT run again */
    CHECK(g_Ext.nAA2854 == 3);                   /* but the prologue did */
    CHECK(g_c72AF0 == 1);
}

/* 0x10046260's epilogue runs only on the just-built path. */
static void test_activate_10046260_epilogue(void)
{
    ResetAll();
    g_Ext.pAA29AC = MakePhase();

    CHECK(BrPhaseActivate_10046260() == 1);
    CHECK(g_Base.n0AA010 == 2);
    CHECK(g_Base.n0AC304 == 1);
    CHECK(g_Ext.b680738 == 0xFF);
    CHECK(g_Ext.nAD0984 == 1);
    CHECK(g_c8B80 == 1 && g_c3DFC0 == 1 && g_c3E510 == 1);
    CHECK(g_Ext.pAA29AC->pfn08 != NULL);

    /* The thunk really is slice2_26's 0x10044CB0. */
    g_Ext.pAA29AC->pfn08(ObjReset());
    CHECK(g_c44CB0 == 1);

    /* Second call: already built, so no epilogue. */
    g_c8B80 = g_c3DFC0 = g_c3E510 = 0;
    CHECK(BrPhaseActivate_10046260() == 1);
    CHECK(g_c8B80 == 0 && g_c3DFC0 == 0 && g_c3E510 == 0);

    free(g_Ext.pAA29AC);
}

/* ==========================================================================
 * HOOK installers
 * ========================================================================== */

static void test_hook_installers(void)
{
    ResetAll();
    g_Ext.pAA29C8 = MakePhase();
    CHECK(BrPhaseHook_10045780(NULL) == 1);
    CHECK(g_c451E0 == 1);                                /* the even family */
    CHECK(g_Ext.pAA29C8->pfn08 == BrPhaseLeave_10046750);
    free(g_Ext.pAA29C8);

    ResetAll();
    g_Base.pAA29F4 = MakePhase();
    CHECK(BrPhaseHook_100457A0(NULL) == 1);
    CHECK(g_Ext.pAA2928 != NULL);                        /* the odd family  */
    CHECK(g_Base.pAA29F4->pfn08 == BrPhaseLeaveNamed_10046790);
    free(g_Base.pAA29F4);

    /* 0x10046380 is the twin of slice2_26's 0x10045050 but finishes with
     * n0AA010 = 2, not 0. */
    ResetAll();
    g_Base.pAA29B4 = MakePhase();
    CHECK(BrPhaseHook_10046380(NULL) == 1);
    CHECK(g_c45110 == 1);
    CHECK(g_Base.n0AC304 == 1);
    CHECK(g_Base.n0AA010 == 2);
    CHECK(g_Base.pAA29B4->pfn08 == BrPhaseLeave_10046D20);
    free(g_Base.pAA29B4);
}

/* ==========================================================================
 * LEAVE
 * ========================================================================== */

/* The shared prologue: the sub-object's slot +0x1C runs, then the current
 * phase is notified through slot +0x00 with the argument 1, then the current
 * phase is repointed. A NULL current phase is tolerated. */
static void test_leave_prologue(void)
{
    BrGameObj *pObj;
    BrPhase   *pCur, *pNext;

    ResetAll();
    pObj  = ObjReset();
    pCur  = MakePhase();
    pNext = MakePhase();
    g_Base.pAA2904 = pCur;
    g_Base.pAA2908 = pNext;
    g_Base.pAA290C = MakePhase();
    g_Ext.pAA29AC  = MakePhase();

    BrPhaseLeave_10046450(pObj);

    CHECK(g_cSlot7 == 1);
    CHECK(g_cF00 == 1);
    CHECK(g_nLastF00Arg == 1);          /* always 1 */
    CHECK(g_Base.pAA290C == NULL);      /* the documented clears */
    CHECK(g_Ext.pAA29AC == NULL);
    CHECK(g_Base.pAA2904 == pNext);     /* repointed */

    free(pCur); free(pNext);

    /* With no current phase the notify is skipped but everything else runs. */
    ResetAll();
    pObj = ObjReset();
    g_Base.pAA2904 = NULL;
    g_Base.pAA2908 = NULL;
    BrPhaseLeave_10046450(pObj);
    CHECK(g_cSlot7 == 1);
    CHECK(g_cF00 == 0);
}

/* The two LEAVE routines with a tail run it after the repoint. */
static void test_leave_tails(void)
{
    ResetAll();
    BrPhaseLeave_10046560(ObjReset());
    CHECK(g_c79550 == 1);

    ResetAll();
    g_Ext.pB4DF30 = (void *)&g_Ext;
    g_Ext.pB4FBE8 = (void *)&g_Base;
    BrPhaseLeave_100466C0(ObjReset());
    CHECK(g_c3E310 == 1);
    CHECK(g_c6A4A0 == 1);
    /* thiscall: 0x10B4DF30 is `this`, 0x10B4FBE8 is the argument -- not the
     * other way round. */
    CHECK(g_p6A4A0This == (void *)&g_Ext);
    CHECK(g_p6A4A0Arg  == (void *)&g_Base);
}

/* The name reset: both destinations end up equal to the source, and the
 * source is left alone. Round-trip over several sources. */
static void test_leave_name_reset(void)
{
    static const char *asz[] = { "", "A", "player one", "0123456789abcdef012345678901" };
    size_t i;

    for (i = 0; i < sizeof(asz) / sizeof(asz[0]); i++) {
        ResetAll();
        memset(g_Ext.szAA2518, 'x', sizeof(g_Ext.szAA2518));
        memset(g_Ext.szA9D618, 'y', sizeof(g_Ext.szA9D618));
        strcpy(g_Ext.sz39B720, asz[i]);
        g_Ext.nAA29C0 = g_Ext.nAA29CC = g_Ext.nAA28E4 = 5;
        g_Ext.pAA2928 = MakePhase();

        BrPhaseLeaveNamed_10046790(ObjReset());

        CHECK(strcmp(g_Ext.szAA2518, asz[i]) == 0);
        CHECK(strcmp(g_Ext.szA9D618, asz[i]) == 0);
        CHECK(strcmp(g_Ext.sz39B720, asz[i]) == 0);   /* source untouched */
        CHECK(g_Ext.n0AB3F4 == -1);
        CHECK(g_Ext.pAA2928 == NULL);
        CHECK(g_Ext.nAA29C0 == 0 && g_Ext.nAA29CC == 0 && g_Ext.nAA28E4 == 0);
    }

    /* An unterminated source must not run off the end (DEVIATION 2). */
    ResetAll();
    memset(g_Ext.sz39B720, 'Z', sizeof(g_Ext.sz39B720));
    BrPhaseLeaveNamed_10046790(ObjReset());
    CHECK(strlen(g_Ext.szAA2518) == (size_t)BR_NAME31_LEN - 1u);
    CHECK(strlen(g_Ext.szA9D618) == (size_t)BR_NAME31_LEN - 1u);

    /* 0x10046E10 is the odd one out: it clears pAA2924 / nAA28E0 instead. */
    ResetAll();
    strcpy(g_Ext.sz39B720, "hello");
    g_Ext.pAA2924 = MakePhase();
    g_Ext.pAA2928 = MakePhase();
    g_Ext.nAA28E0 = 9;
    g_Ext.nAA29C0 = 9;
    BrPhaseLeaveNamed_10046E10(ObjReset());
    CHECK(g_Ext.pAA2924 == NULL);
    CHECK(g_Ext.nAA28E0 == 0);
    CHECK(g_Ext.pAA2928 != NULL);       /* NOT cleared by this one */
    CHECK(g_Ext.nAA29C0 == 9);          /* nor this */
    CHECK(strcmp(g_Ext.szA9D618, "hello") == 0);
    free(g_Ext.pAA2928);
}

/* 0x10047340 clears only ONE of the two name buffers. */
static void test_name_clear_asymmetry(void)
{
    ResetAll();
    memset(g_Ext.szAA2518, 'a', sizeof(g_Ext.szAA2518) - 1);
    memset(g_Ext.szA9D618, 'b', sizeof(g_Ext.szA9D618) - 1);
    g_Ext.nAA28A4 = 4;
    g_Ext.bAA26F5 = 4;

    CHECK(BrPhaseNameClear_10047340() == 1);
    CHECK(g_Ext.szA9D618[0] == '\0');
    CHECK(g_Ext.szAA2518[0] == 'a');    /* untouched */
    CHECK(g_Ext.nAA28A4 == 0);
    CHECK(g_Ext.bAA26F5 == 0);
}

/* 0x10046F60: the phase saved out of pAA292C is notified even though
 * pAA2904 has already been NULLed, and pAA2904 ends at pAA2908. */
static void test_leave_10046F60(void)
{
    BrPhase *pCur, *pSecond, *pEnd;

    ResetAll();
    pCur    = MakePhase();
    pSecond = MakePhase();
    pEnd    = MakePhase();
    g_Base.pAA2904 = pCur;
    g_Ext.pAA292C  = pSecond;
    g_Ext.pAA2974  = MakePhase();
    g_Base.pAA2908 = pEnd;

    BrPhaseLeave_10046F60(ObjReset());

    CHECK(g_cF00 == 2);                 /* current, then the saved one */
    CHECK(g_Ext.pAA292C == NULL);
    CHECK(g_Ext.pAA2974 == NULL);
    CHECK(g_Base.pAA2904 == pEnd);

    free(pCur); free(pSecond); free(pEnd);
}

/* 0x10046FD0 tears three phases down through vtable slot +0x1C. */
static void test_leave_10046FD0(void)
{
    ResetAll();
    g_Ext.pAA2934 = MakePhase();
    g_Ext.pAA2938 = MakePhase();
    g_Ext.pAA293C = MakePhase();
    g_Ext.pAA2974 = MakePhase();
    g_Base.pAA2908 = MakePhase();

    BrPhaseLeave_10046FD0(ObjReset());

    CHECK(g_cF1C == 3);
    CHECK(g_Ext.pAA2934 == NULL);
    CHECK(g_Ext.pAA2938 == NULL);
    CHECK(g_Ext.pAA293C == NULL);
    CHECK(g_Ext.pAA2974 == NULL);
    CHECK(g_Base.pAA2904 == g_Base.pAA2908);

    /* NULL slots are skipped, not called. */
    g_cF1C = 0;
    BrPhaseLeave_10046FD0(ObjReset());
    CHECK(g_cF1C == 0);
}

/* 0x10047120's clear is guarded by three conditions at once. */
static void test_leave_10047120_guard(void)
{
    int i;
    /* Every combination that must NOT clear. */
    struct { int32_t n; uint8_t a, b; int fClear; } aCase[] = {
        { 0, 0, 0, 0 },     /* nAA26F0 must be > 0            */
        { -1, 0, 0, 0 },
        { 1, 1, 0, 0 },     /* bAA26F4 must be 0              */
        { 1, 0, 1, 0 },     /* bAA26F5 must be 0              */
        { 1, 0, 0, 1 }      /* only this one clears           */
    };

    for (i = 0; i < (int)(sizeof(aCase) / sizeof(aCase[0])); i++) {
        ResetAll();
        g_Ext.nAA26F0 = aCase[i].n;
        g_Ext.bAA26F4 = aCase[i].a;
        g_Ext.bAA26F5 = aCase[i].b;
        memset(g_Ext.aAA26F6, 0x11, sizeof(g_Ext.aAA26F6));
        memset(g_Ext.aAA270E, 0x11, sizeof(g_Ext.aAA270E));
        memset(g_Ext.aAA2740, 0x11, sizeof(g_Ext.aAA2740));
        g_Ext.nAA28C4 = 3;
        g_Ext.pAA296C = MakePhase();
        g_Base.pAA2904 = MakePhase();

        BrPhaseLeave_10047120(ObjReset());

        CHECK(g_Ext.aAA26F6[0] == (aCase[i].fClear ? 0x00 : 0x11));
        CHECK(g_Ext.aAA2740[95] == (aCase[i].fClear ? 0x00 : 0x11));
        /* Unconditional parts happen regardless. */
        CHECK(g_Ext.nAA28C4 == 0);
        CHECK(g_cSlot7 == 1);
        CHECK(g_Ext.pAA296C == NULL);
        /* This routine notifies pAA296C, and never touches pAA2904. */
        CHECK(g_Base.pAA2904 != NULL);
        free(g_Base.pAA2904);
    }
}

/* 0x10047290 takes exactly one of three branches. */
static void test_leave_10047290_branches(void)
{
    ResetAll();
    g_Ext.nAA28B0 = 1;
    g_Ext.nAA28B4 = 1;
    BrPhaseLeave_10047290(ObjReset());
    CHECK(g_c5FBC0 == 1);
    CHECK(g_c43260 == 1);
    CHECK(g_c43330 == 0);           /* 28B0 wins over 28B4 */
    CHECK(g_Ext.nAA28B0 == 0);
    CHECK(g_Ext.nAA28B4 == 1);      /* the loser is NOT cleared */

    ResetAll();
    g_Ext.nAA28B4 = 1;
    BrPhaseLeave_10047290(ObjReset());
    CHECK(g_c43330 == 1);
    CHECK(g_Ext.nAA28B4 == 0);

    ResetAll();
    BrPhaseLeave_10047290(ObjReset());
    CHECK(g_c43260 == 0 && g_c43330 == 0);
    CHECK(g_Ext.pAA292C != NULL);   /* the fallback ran BrExt_10045C90 */
}

/* ==========================================================================
 * The small helpers
 * ========================================================================== */

static void test_guard_and_edit(void)
{
    ResetAll();
    g_f3E0E0 = 0;
    CHECK(BrPhaseGuard_100471F0(ObjReset()) == 1);
    CHECK(g_cSlot7 == 0);           /* nothing ran */

    ResetAll();
    g_f3E0E0 = 1;
    CHECK(BrPhaseGuard_100471F0(ObjReset()) == -1);
    CHECK(g_cSlot7 == 1);

    /* The asymmetry: the pending key is cleared on both acting branches and
     * NOT on the do-nothing branch, and the callee's result is discarded. */
    ResetAll();
    g_Ext.nAA2AD4 = 1;
    g_Ext.nAA33E4 = 'q';
    CHECK(BrPhaseEdit_10047210(NULL) == -1);
    CHECK(g_nEditWhich == 1);
    CHECK(g_Ext.nAA33E4 == 0);

    ResetAll();
    g_f3E0E0 = 1;
    g_Ext.nAA33E4 = 'q';
    CHECK(BrPhaseEdit_10047210(NULL) == -1);
    CHECK(g_nEditWhich == 2);
    CHECK(g_Ext.nAA33E4 == 0);

    ResetAll();
    g_Ext.nAA33E4 = 'q';
    CHECK(BrPhaseEdit_10047210(NULL) == 1);
    CHECK(g_cEdit == 0);
    CHECK(g_Ext.nAA33E4 == 'q');    /* left pending */

    /* 0x10047250 is the same shape with the other pair. */
    ResetAll();
    g_Ext.nAA2AD4 = 1;
    CHECK(BrPhaseEdit_10047250(NULL) == -1);
    CHECK(g_nEditWhich == 3);
    ResetAll();
    g_f3E0E0 = 1;
    CHECK(BrPhaseEdit_10047250(NULL) == -1);
    CHECK(g_nEditWhich == 4);
}

static void test_key_ring(void)
{
    int i;

    /* Nothing pending -> nothing happens. */
    ResetAll();
    BrPhaseKeyPush_10047610();
    CHECK(g_Ext.nAA2A48 == 0);
    CHECK(g_c47660 == 0);

    /* 'A'..'Z' are lowercased; the key is consumed; the sink is notified. */
    ResetAll();
    g_Ext.nAA33E4 = 'A';
    BrPhaseKeyPush_10047610();
    CHECK(g_Ext.aA9E150[0] == 'a');
    CHECK(g_Ext.nAA2A48 == 1);
    CHECK(g_Ext.nAA33E4 == 0);
    CHECK(g_c47660 == 1);

    /* The boundaries of the A-Z window are inclusive on both sides and
     * nothing outside it moves. */
    ResetAll();
    {
        static const int aIn[]  = { 'A', 'Z', '@', '[', 'a', 'z', '0' };
        static const int aOut[] = { 'a', 'z', '@', '[', 'a', 'z', '0' };
        for (i = 0; i < (int)(sizeof(aIn) / sizeof(aIn[0])); i++) {
            g_Ext.nAA33E4 = aIn[i];
            BrPhaseKeyPush_10047610();
            CHECK(g_Ext.aA9E150[i] == aOut[i]);
        }
    }

    /* Bytes >= 0x80 fail the SIGNED compare and are stored sign-extended. */
    ResetAll();
    g_Ext.nAA33E4 = 0xE9;
    BrPhaseKeyPush_10047610();
    CHECK(g_Ext.aA9E150[0] == -23);

    /* Only the low byte of the pending word is looked at. */
    ResetAll();
    g_Ext.nAA33E4 = 0x1241;             /* low byte 'A' */
    BrPhaseKeyPush_10047610();
    CHECK(g_Ext.aA9E150[0] == 'a');

    /* The index wraps AFTER the store, so entry 31 is written and the next
     * key lands back at 0. */
    ResetAll();
    for (i = 0; i < 32; i++) {
        g_Ext.nAA33E4 = 'a' + (i % 26);
        BrPhaseKeyPush_10047610();
    }
    CHECK(g_Ext.nAA2A48 == 0);
    CHECK(g_Ext.aA9E150[31] == 'a' + (31 % 26));
    g_Ext.nAA33E4 = '!';
    BrPhaseKeyPush_10047610();
    CHECK(g_Ext.aA9E150[0] == '!');
    CHECK(g_Ext.nAA2A48 == 1);
}

/* 0x10047360, the countdown state machine. */
static void test_gameobj_step(void)
{
    BrGameObj   *pObj;
    BrObjAA2E80  obj2E80;
    int          n;

    /* Three separate vetoes, each of which leaves the counter alone. */
    ResetAll();
    pObj = ObjReset();
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_10 | BR_GAMEOBJ_FLAG_100);
    BrSub10047360(pObj);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 0);

    ResetAll();
    pObj = ObjReset();
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_1000000 | BR_GAMEOBJ_FLAG_100);
    BrSub10047360(pObj);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 0);

    ResetAll();
    pObj = ObjReset();
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_100);
    ObjSt32(BR_GAMEOBJ_OFF_F3850, BR_GAMEOBJ_FLAG_1000000);
    BrSub10047360(pObj);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 0);

    /* Without the 0x100 bit nothing steps either. */
    ResetAll();
    pObj = ObjReset();
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, 0);
    BrSub10047360(pObj);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 0);

    /* The nAA284C override: state becomes 4 AND the flags are not written
     * back, so the 0x100 bit survives. */
    ResetAll();
    pObj = ObjReset();
    memset(&obj2E80, 0, sizeof(obj2E80));
    obj2E80.f34 = 1;
    g_Ext.pAA2E80 = &obj2E80;
    g_Ext.nAA284C = 1;
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_100);
    BrSub10047360(pObj);
    CHECK(g_abObj[BR_GAMEOBJ_OFF_STATE] == 4);
    CHECK(ObjLd32(BR_GAMEOBJ_OFF_FLAGS) & BR_GAMEOBJ_FLAG_100);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 0);

    /* With all four dwords clear the override does not fire. */
    ResetAll();
    pObj = ObjReset();
    memset(&obj2E80, 0, sizeof(obj2E80));
    g_Ext.pAA2E80 = &obj2E80;
    g_Ext.nAA284C = 1;
    ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_100);
    ObjSt16(BR_GAMEOBJ_OFF_COUNT, 1);
    BrSub10047360(pObj);
    CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 2);
    CHECK(g_abObj[BR_GAMEOBJ_OFF_STATE] == 0);
    CHECK((ObjLd32(BR_GAMEOBJ_OFF_FLAGS) & BR_GAMEOBJ_FLAG_100) == 0);

    /* The four named counter values and the default, one per run. */
    {
        static const struct { uint16_t before; int fState; uint8_t state;
                              uint16_t after; } aCase[] = {
            { 1,  1, 0, 2  },   /* -> 2  : state 0 */
            { 2,  1, 1, 3  },   /* -> 3  : state 1 */
            { 3,  1, 2, 4  },   /* -> 4  : state 2 */
            { 51, 1, 4, 52 },   /* -> 52 : state 4 */
            { 4,  0, 0, 2  },   /* -> 5  : default, counter reset to 2 */
            { 52, 0, 0, 2  },   /* -> 53 : out of range, reset to 2    */
            { 0,  0, 0, 2  }    /* -> 1  : (int16)1-2 = -1, unsigned   */
                                /*         wrap -> default             */
        };
        for (n = 0; n < (int)(sizeof(aCase) / sizeof(aCase[0])); n++) {
            ResetAll();
            pObj = ObjReset();
            ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_100);
            ObjSt16(BR_GAMEOBJ_OFF_COUNT, aCase[n].before);
            g_abObj[BR_GAMEOBJ_OFF_STATE] = 0xEE;
            BrSub10047360(pObj);

            CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == aCase[n].after);
            if (aCase[n].fState)
                CHECK(g_abObj[BR_GAMEOBJ_OFF_STATE] == aCase[n].state);
            else
                CHECK(g_abObj[BR_GAMEOBJ_OFF_STATE] == 0xEE);  /* untouched */
            /* Every path that gets this far clears the 0x100 bit. */
            CHECK((ObjLd32(BR_GAMEOBJ_OFF_FLAGS) & BR_GAMEOBJ_FLAG_100) == 0);
        }
    }

    /* Every counter in 5..51 takes the default arm -- the whole run of 4s in
     * the index table, not just its ends. */
    for (n = 5; n <= 51; n++) {
        ResetAll();
        pObj = ObjReset();
        ObjSt32(BR_GAMEOBJ_OFF_FLAGS, BR_GAMEOBJ_FLAG_100);
        ObjSt16(BR_GAMEOBJ_OFF_COUNT, (uint16_t)(n - 1));
        g_abObj[BR_GAMEOBJ_OFF_STATE] = 0xEE;
        BrSub10047360(pObj);
        CHECK(ObjLd16(BR_GAMEOBJ_OFF_COUNT) == 2);
        CHECK(g_abObj[BR_GAMEOBJ_OFF_STATE] == 0xEE);
    }
}

static void test_ticks(void)
{
    ResetAll();
    CHECK(BrPhaseTick_100474B0(ObjReset()) == 1);

    /* 0x100475F0 pushes a pending key first. */
    ResetAll();
    (void)ObjReset();
    g_Ext.nAA33E4 = 'B';
    CHECK(BrPhaseTick_100475F0((BrGameObj *)(void *)g_abObj) == 1);
    CHECK(g_Ext.aA9E150[0] == 'b');
    CHECK(g_Ext.nAA33E4 == 0);
}

static void test_mode_callbacks(void)
{
    ResetAll(); BrPhaseMode_100474D0();
    CHECK(g_Ext.nAA28F0 == 1 && g_n72AF0A == 2 && g_Ext.nAA2854 == 2);
    CHECK(g_n72AF0B == 0x00200020u);

    ResetAll(); BrPhaseMode_10047500();
    CHECK(g_Ext.nAA28F8 == 1 && g_n72AF0A == 2 && g_Ext.nAA2854 == 2);

    ResetAll(); BrPhaseMode_10047530();
    CHECK(g_Ext.nAA28FC == 1 && g_n72AF0A == 2 && g_Ext.nAA2854 == 2);

    /* The odd one: a 16-bit sentinel and mode 3, not a flag and mode 2. */
    ResetAll(); BrPhaseMode_10047560();
    CHECK(g_Ext.n0AC6A4 == 0x7FFF && g_n72AF0A == 3 && g_Ext.nAA2854 == 3);

    ResetAll(); BrPhaseMode_10047590();
    CHECK(g_Ext.nAA2A40 == 1 && g_n72AF0A == 2 && g_Ext.nAA2854 == 2);

    ResetAll(); BrPhaseMode_100475C0();
    CHECK(g_Ext.nAA28F4 == 1 && g_n72AF0A == 2 && g_Ext.nAA2854 == 2);

    /* The six write six DIFFERENT globals -- no two alias. */
    ResetAll();
    BrPhaseMode_100474D0();
    CHECK(g_Ext.nAA28F4 == 0 && g_Ext.nAA28F8 == 0 && g_Ext.nAA28FC == 0
          && g_Ext.nAA2A40 == 0);
}

/* The three one-statement gotos read no argument and only move pAA2904. */
static void test_gotos(void)
{
    ResetAll();
    g_Ext.pAA2974 = MakePhase();
    g_Ext.pAA292C = MakePhase();
    g_Ext.pAA293C = MakePhase();

    BrPhaseGoto_10046F50();
    CHECK(g_Base.pAA2904 == g_Ext.pAA2974);
    BrPhaseGoto_10046FC0();
    CHECK(g_Base.pAA2904 == g_Ext.pAA292C);
    BrPhaseGoto_10047050();
    CHECK(g_Base.pAA2904 == g_Ext.pAA293C);

    /* No phase was notified and no sub-object was driven. */
    CHECK(g_cF00 == 0 && g_cF1C == 0 && g_cSlot7 == 0);

    free(g_Ext.pAA2974); free(g_Ext.pAA292C); free(g_Ext.pAA293C);
}

/* ==========================================================================
 * main
 * ========================================================================== */

int main(void)
{
    test_pod_load_into();
    test_activate_three_outcomes();
    test_activate_reread_gotcha();
    test_activate_second_object();
    test_activate_10045900_guard();
    test_activate_10046170_prologue_on_both_paths();
    test_activate_10046260_epilogue();
    test_hook_installers();
    test_leave_prologue();
    test_leave_tails();
    test_leave_name_reset();
    test_name_clear_asymmetry();
    test_leave_10046F60();
    test_leave_10046FD0();
    test_leave_10047120_guard();
    test_leave_10047290_branches();
    test_guard_and_edit();
    test_key_ring();
    test_gameobj_step();
    test_ticks();
    test_mode_callbacks();
    test_gotos();

    if (g_cFail != 0) {
        printf("%d failure(s)\n", g_cFail);
        return 1;
    }
    printf("test_slice3_31: all tests passed\n");
    return 0;
}
