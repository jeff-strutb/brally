/* test_slice2_26.c -- behavioural tests for BRD3D.dll 0x100447D0-0x100456B0.
 *
 * Everything below the "stand-ins" banner exists ONLY so this translation
 * unit links on its own. Those definitions are test doubles for cross-slice
 * callees (each marked XSLICE in slice2_26.h) and must never be linked into
 * the real build. That includes BrSlotsReset, whose real body is in
 * port/src/br_slots.c -- drop the stand-in here if that object is ever added
 * to this test's link line.
 *
 * The tests assert the properties that are easy to lose in a port of this
 * range: which globals are read BEFORE and which AFTER a call (the reload
 * semantics), which paths reach an epilogue, what happens when the allocation
 * fails, and the handful of routines that break the pattern of their
 * neighbours.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice2_26.h"

/* ====================================================================== */
/* harness                                                                */
/* ====================================================================== */

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* ====================================================================== */
/* stand-ins for the cross-slice callees                                  */
/* ====================================================================== */

typedef struct Calls {
    int n1003D0B0, n10043BF0, n10043CD0, n10043E70;
    int n100440D0, n100443E0, n10044280, n10038F30;
    int n100419D0, n1003DB00, n1003BF60, n1003C020;
    int n1003C150, n1003CDA0, n1003DFC0, n1003E510;
    int n1003E680, n10041BD0, n1007AC00, n10045C90;
    int n10008B80, nSlotsReset, nNew, nCtor;
    int n10046CD0, n10046DC0;
    int aEnter[16];             /* one counter per enter hook, see EnterId  */
    int nEnterTotal;
    int nF18, nF1C, nF00, nF7C;
} Calls;

static Calls g_c;

/* knobs */
static BrPhaseCtx *g_pCtx;          /* the ctx the doubles may poke        */
static int         g_allocFail;     /* make operator new return NULL       */
static BrPhase    *g_pRedirect;     /* enter hook re-points pAA2904 at this */
static int32_t     g_mode1003C020;  /* value 0x1003C020 writes to nAA287C   */
static int         g_setMode;       /* ...only when this is set             */
static BrHostItem *g_pItemOut;      /* what 0x1003D0B0 hands back           */
static int32_t     g_seenAC304;     /* n0AC304 as the enter hook saw it     */
static void       *g_pLastEntity;   /* argument seen by the +0x08 hooks     */
static BrPhase    *g_pNotified;     /* the phase whose vtable +0x00 ran     */
static int32_t     g_notifyArg;
static void       *g_pSlotsSeen;
static void       *g_p1003DB00_a;
static void       *g_p1003DB00_b;
static BrHost     *g_pF7CSelf;
static void       *g_pF7CItem;
static int32_t     g_nF7CArg;
static void       *g_p10045C90Arg;
static int32_t     g_nF18Arg;

/* memory */
void *BrOperatorNew(uint32_t cb)
{
    void *p;
    ++g_c.nNew;
    if (g_allocFail)
        return NULL;
    p = malloc(cb);
    if (p != NULL)
        memset(p, 0xCD, cb);   /* operator new does NOT zero */
    return p;
}

static const BrPhaseVtbl *g_pPhaseVtbl;   /* set up in main */

BrPhase *BrPhaseCtor(BrPhase *pThis)
{
    ++g_c.nCtor;
    pThis->pVtbl = g_pPhaseVtbl;
    pThis->pfn04 = NULL;
    pThis->pfn08 = NULL;
    pThis->f0C   = 0;
    pThis->f68   = 0;
    return pThis;
}

/* br_slots.h / 0x100586A0 */
void BrSlotsReset(BrSlotTable *pTable)
{
    ++g_c.nSlotsReset;
    g_pSlotsSeen = pTable;
}

void BrExt_1003D0B0(BrHost *pHost, BrHostItem **ppOut)
{
    (void)pHost;
    ++g_c.n1003D0B0;
    *ppOut = g_pItemOut;
}

void BrExt_10043BF0(int32_t a) { (void)a; ++g_c.n10043BF0; }
void BrExt_10043CD0(int32_t a) { (void)a; ++g_c.n10043CD0; }
void BrExt_10043E70(int32_t a) { (void)a; ++g_c.n10043E70; }
void BrExt_100440D0(int32_t a) { (void)a; ++g_c.n100440D0; }
void BrExt_100443E0(int32_t a) { (void)a; ++g_c.n100443E0; }
void BrExt_10044280(int32_t a) { (void)a; ++g_c.n10044280; }
void BrExt_10038F30(int32_t a) { (void)a; ++g_c.n10038F30; }

void BrExt_100419D0(void *p) { (void)p; ++g_c.n100419D0; }

void BrExt_1003DB00(BrObjA9D008 *pObj, void *p)
{
    ++g_c.n1003DB00;
    g_p1003DB00_a = pObj;
    g_p1003DB00_b = p;
}

void BrExt_1003BF60(void) { ++g_c.n1003BF60; }

void BrExt_1003C020(void)
{
    ++g_c.n1003C020;
    if (g_setMode && g_pCtx != NULL)
        g_pCtx->nAA287C = g_mode1003C020;
}

void BrExt_1003C150(void) { ++g_c.n1003C150; }
void BrExt_1003CDA0(void) { ++g_c.n1003CDA0; }
void BrExt_1003DFC0(void) { ++g_c.n1003DFC0; }
void BrExt_1003E510(void) { ++g_c.n1003E510; }
void BrExt_1003E680(void) { ++g_c.n1003E680; }
void BrExt_10041BD0(void) { ++g_c.n10041BD0; }
void BrExt_1007AC00(void) { ++g_c.n1007AC00; }
void BrExt_10008B80(void) { ++g_c.n10008B80; }

void BrExt_10045C90(void *p)
{
    ++g_c.n10045C90;
    g_p10045C90Arg = p;
}

/* the enter hooks */
enum EnterId {
    E_10058750 = 0, E_10059760, E_10059BB0, E_1005A6E0,
    E_1004A580,     E_1004B430, E_1004BDC0, E_1004C4A0,
    E_1004D1F0,     E_1004D640, E_1004DB00, E_1004DFC0,
    E_1004E830
};

static void EnterCommon(int id, BrPhase *pSelf)
{
    ++g_c.aEnter[id];
    ++g_c.nEnterTotal;
    (void)pSelf;
    if (g_pCtx != NULL) {
        g_seenAC304 = g_pCtx->n0AC304;
        if (g_pRedirect != NULL)
            g_pCtx->pAA2904 = g_pRedirect;   /* exercise the reload */
    }
}

void BrExt_10058750(BrPhase *p) { EnterCommon(E_10058750, p); }
void BrExt_10059760(BrPhase *p) { EnterCommon(E_10059760, p); }
void BrExt_10059BB0(BrPhase *p) { EnterCommon(E_10059BB0, p); }
void BrExt_1005A6E0(BrPhase *p) { EnterCommon(E_1005A6E0, p); }
void BrPhaseEnterPlaceholder_1004A580(BrPhase *p) { EnterCommon(E_1004A580, p); }
void BrPhaseEnterPlaceholder_1004B430(BrPhase *p) { EnterCommon(E_1004B430, p); }
void BrPhaseEnterPlaceholder_1004BDC0(BrPhase *p) { EnterCommon(E_1004BDC0, p); }
void BrPhaseEnterPlaceholder_1004C4A0(BrPhase *p) { EnterCommon(E_1004C4A0, p); }
void BrExt_1004D1F0(BrPhase *p) { EnterCommon(E_1004D1F0, p); }
void BrExt_1004D640(BrPhase *p) { EnterCommon(E_1004D640, p); }
void BrExt_1004DB00(BrPhase *p) { EnterCommon(E_1004DB00, p); }
void BrExt_1004DFC0(BrPhase *p) { EnterCommon(E_1004DFC0, p); }
void BrExt_1004E830(BrPhase *p) { EnterCommon(E_1004E830, p); }

/* the +0x08 hooks */
void BrExt_10046CD0(void *pEntity) { ++g_c.n10046CD0; g_pLastEntity = pEntity; }
void BrExt_10046DC0(void *pEntity) { ++g_c.n10046DC0; g_pLastEntity = pEntity; }

/* ====================================================================== */
/* fixtures                                                               */
/* ====================================================================== */

static void PhaseF00(BrPhase *pThis, int32_t a)
{
    ++g_c.nF00;
    g_pNotified = pThis;
    g_notifyArg = a;
}

static void SubF18(BrEntSub *pThis, int32_t a)
{
    (void)pThis;
    ++g_c.nF18;
    g_nF18Arg = a;
}

static void SubF1C(BrEntSub *pThis)
{
    (void)pThis;
    ++g_c.nF1C;
}

static void HostF7C(BrHost *pSelf, void *pItem, int32_t a)
{
    ++g_c.nF7C;
    g_pF7CSelf = pSelf;
    g_pF7CItem = pItem;
    g_nF7CArg  = a;
}

static BrPhaseVtbl   g_phaseVtbl;
static BrEntSubVtbl  g_subVtbl;
static BrHostVtbl    g_hostVtbl;

/* An entity record. The union forces the alignment the pointer stored at
 * +0x2AE8 needs; the record itself is 0x2B68 bytes in the original. */
typedef union Entity {
    void          *align;
    unsigned char  b[0x2B68];
} Entity;

static BrEntSub g_sub;

static void EntityInit(Entity *pE)
{
    memset(pE, 0, sizeof(*pE));
    g_sub.pVtbl = &g_subVtbl;
    memcpy(&pE->b[BR_ENTITY_OFF_SUB], &(void *){ &g_sub }, sizeof(void *));
}

static uint32_t EntityFlags(const Entity *pE)
{
    uint32_t v;
    memcpy(&v, &pE->b[BR_ENTITY_OFF_FLAGS], sizeof(v));
    return v;
}

static void EntitySetFlags(Entity *pE, uint32_t v)
{
    memcpy(&pE->b[BR_ENTITY_OFF_FLAGS], &v, sizeof(v));
}

static Entity      g_ent;      /* the argument passed to the leave routines */
static Entity      g_entD8;    /* the record pAA29D8 points at             */
static BrPhaseCtx  g_ctx;
static BrSlotTable g_slots;
static BrHost      g_host;
static BrObjA9D008 g_a9d008;
static int         g_ad300;

static void Setup(void)
{
    memset(&g_c, 0, sizeof(g_c));
    memset(&g_ctx, 0, sizeof(g_ctx));
    memset(&g_slots, 0, sizeof(g_slots));
    EntityInit(&g_ent);
    EntityInit(&g_entD8);

    g_allocFail    = 0;
    g_pRedirect    = NULL;
    g_setMode      = 0;
    g_mode1003C020 = 0;
    g_pItemOut     = NULL;
    g_seenAC304    = -1;
    g_pLastEntity  = NULL;
    g_pNotified    = NULL;
    g_notifyArg    = 0;
    g_pSlotsSeen   = NULL;
    g_pF7CSelf     = NULL;
    g_pF7CItem     = NULL;
    g_nF7CArg      = -1;
    g_nF18Arg      = -1;

    g_host.pVtbl   = &g_hostVtbl;

    g_ctx.pSlots   = &g_slots;
    g_ctx.p0AD300  = &g_ad300;
    g_ctx.p277B40  = &g_host;
    g_ctx.pA9D008  = &g_a9d008;
    g_a9d008.f08   = NULL;

    g_pCtx = &g_ctx;
}

/* A phase object owned by the test (not one the code allocates). */
static BrPhase *MakePhase(void)
{
    BrPhase *p = (BrPhase *)malloc(sizeof(BrPhase));
    p->pVtbl = &g_phaseVtbl;
    p->pfn04 = NULL;
    p->pfn08 = NULL;
    p->f0C   = 0;
    p->f68   = 0;
    return p;
}

/* ====================================================================== */
/* tests                                                                  */
/* ====================================================================== */

/* The singleton is built exactly once; the prologue runs every time. */
static void TestActivateIsIdempotent(void)
{
    BrPhase *pFirst;

    Setup();
    CHECK(BrPhaseActivate_10044B90(&g_ctx) == 1);
    pFirst = g_ctx.pAA295C;
    CHECK(pFirst != NULL);
    CHECK(g_ctx.pAA2904 == pFirst);
    CHECK(g_c.nCtor == 1);
    CHECK(g_c.aEnter[E_10059760] == 1);
    CHECK(pFirst->pfn04 == BrExt_10059760);
    CHECK(pFirst->f0C == 1);
    CHECK(pFirst->f68 == 1);
    CHECK(g_c.n100419D0 == 1);

    g_ctx.pAA2904 = NULL;
    CHECK(BrPhaseActivate_10044B90(&g_ctx) == 1);
    CHECK(g_ctx.pAA295C == pFirst);     /* not rebuilt */
    CHECK(g_ctx.pAA2904 == pFirst);     /* but re-selected */
    CHECK(g_c.nCtor == 1);
    CHECK(g_c.aEnter[E_10059760] == 1); /* the enter hook does NOT re-run */
    CHECK(g_c.n100419D0 == 2);          /* the prologue DOES re-run */
    free(pFirst);
}

/* An allocation failure returns 0, leaves both globals NULL, and skips the
 * enter hook -- and, for 0x10044F50, the whole epilogue. */
static void TestActivateAllocFailure(void)
{
    Setup();
    g_allocFail = 1;
    CHECK(BrPhaseActivate_10044D00(&g_ctx) == 0);
    CHECK(g_ctx.pAA2964 == NULL);
    CHECK(g_ctx.pAA2904 == NULL);
    CHECK(g_c.nCtor == 0);
    CHECK(g_c.nEnterTotal == 0);
    /* the prologue still ran */
    CHECK(g_ctx.nAA28C8 == 0 && g_ctx.nAA28CC == 0);

    Setup();
    g_allocFail = 1;
    CHECK(BrPhaseActivate_10044F50(&g_ctx) == 0);
    CHECK(g_c.n10008B80 == 0);
    CHECK(g_c.n1003DFC0 == 0);
    CHECK(g_c.n1003E510 == 0);
    CHECK(g_ctx.n0AA010 == 1);      /* prologue ran before the failure */
    CHECK(g_c.n1003E680 == 1);

    Setup();
    g_allocFail = 1;
    CHECK(BrPhaseActivate_10045460(&g_ctx) == 0);
    CHECK(g_c.n1007AC00 == 0);      /* the tail is skipped only here */
}

/* 0x10044E20 copies the LOWER source into the HIGHER destination. */
static void TestActivate10044E20Crossover(void)
{
    Setup();
    g_ctx.nACEE8C = 0x11111111;
    g_ctx.nACEE94 = 0x22222222;
    CHECK(BrPhaseActivate_10044E20(&g_ctx) == 1);
    CHECK(g_ctx.nAA28CC == 0x11111111);
    CHECK(g_ctx.nAA28C8 == 0x22222222);
    free(g_ctx.pAA2968);
}

/* 0x10044F50's three tail calls happen on the just-built path only, while
 * 0x10045460's happens on both. */
static void TestEpiloguePaths(void)
{
    BrPhase *p;

    Setup();
    CHECK(BrPhaseActivate_10044F50(&g_ctx) == 1);
    CHECK(g_c.n10008B80 == 1);
    CHECK(g_c.n1003DFC0 == 1);
    CHECK(g_c.n1003E510 == 1);
    CHECK(g_c.n1003E680 == 1);
    CHECK(g_ctx.n0AA010 == 1);

    g_ctx.n0AA010 = 99;
    CHECK(BrPhaseActivate_10044F50(&g_ctx) == 1);
    CHECK(g_c.n10008B80 == 1);          /* unchanged on the existing path */
    CHECK(g_c.n1003DFC0 == 1);
    CHECK(g_c.n1003E510 == 1);
    CHECK(g_c.n1003E680 == 2);          /* prologue re-runs */
    CHECK(g_ctx.n0AA010 == 1);
    free(g_ctx.pAA290C);

    Setup();
    CHECK(BrPhaseActivate_10045460(&g_ctx) == 1);
    CHECK(g_c.n1007AC00 == 1);
    p = g_ctx.pAA2990;
    CHECK(BrPhaseActivate_10045460(&g_ctx) == 1);
    CHECK(g_c.n1007AC00 == 2);          /* both paths reach it */
    CHECK(g_ctx.pAA2990 == p);
    CHECK(g_c.aEnter[E_1004D640] == 1);
    free(p);

    Setup();
    CHECK(BrPhaseActivate_10045520(&g_ctx) == 1);
    CHECK(g_c.n1007AC00 == 1);
    CHECK(g_c.aEnter[E_1004DB00] == 1);
    free(g_ctx.pAA2994);
}

/* The current-phase global is re-read after the enter hook runs, so f0C/f68
 * land on whatever the hook selected -- not on the object just built. */
static void TestEnterHookReload(void)
{
    BrPhase *pOther;

    Setup();
    pOther = MakePhase();
    g_pRedirect = pOther;

    CHECK(BrPhaseActivate_10045110(&g_ctx) == 1);
    CHECK(g_ctx.pAA2914 != NULL);
    CHECK(g_ctx.pAA2904 == pOther);
    CHECK(pOther->f0C == 1);
    CHECK(pOther->f68 == 1);
    CHECK(g_ctx.pAA2914->f0C == 0);   /* the new object gets neither flag */
    CHECK(g_ctx.pAA2914->f68 == 0);
    free(g_ctx.pAA2914);
    free(pOther);
}

/* Every activate routine wires its own slot to its own enter hook. */
static void TestSlotToHookWiring(void)
{
    Setup();
    CHECK(BrPhaseActivate_10045110(&g_ctx) == 1);
    CHECK(g_ctx.pAA2914 != NULL && g_ctx.pAA2914->pfn04 == BrPhaseEnterPlaceholder_1004A580);
    CHECK(BrPhaseActivate_100451E0(&g_ctx) == 1);
    CHECK(g_ctx.pAA2918 != NULL && g_ctx.pAA2918->pfn04 == BrPhaseEnterPlaceholder_1004BDC0);
    CHECK(BrPhaseActivate_100452C0(&g_ctx) == 1);
    CHECK(g_ctx.pAA297C != NULL && g_ctx.pAA297C->pfn04 == BrPhaseEnterPlaceholder_1004C4A0);
    CHECK(BrPhaseActivate_10045390(&g_ctx) == 1);
    CHECK(g_ctx.pAA2980 != NULL && g_ctx.pAA2980->pfn04 == BrExt_1004D1F0);
    CHECK(BrPhaseActivate_100455E0(&g_ctx) == 1);
    CHECK(g_ctx.pAA2984 != NULL && g_ctx.pAA2984->pfn04 == BrExt_1004DFC0);
    CHECK(BrPhaseActivate_100456B0(&g_ctx) == 1);
    CHECK(g_ctx.pAA2988 != NULL && g_ctx.pAA2988->pfn04 == BrExt_1004E830);
    CHECK(BrPhaseActivate_10044D00(&g_ctx) == 1);
    CHECK(g_ctx.pAA2964 != NULL && g_ctx.pAA2964->pfn04 == BrExt_10059BB0);
    /* each one is a distinct object */
    CHECK(g_c.nCtor == 7);
    CHECK(g_c.nEnterTotal == 7);
    CHECK(g_ctx.pAA2904 == g_ctx.pAA2964);   /* the last one activated */

    free(g_ctx.pAA2914); free(g_ctx.pAA2918); free(g_ctx.pAA297C);
    free(g_ctx.pAA2980); free(g_ctx.pAA2984); free(g_ctx.pAA2988);
    free(g_ctx.pAA2964);
}

/* The leave prologue: the entity sub-object is always poked, the +0x18 slot
 * only when nA9D000 is set, and the notification carries the literal 1. */
static void TestLeavePrologue(void)
{
    BrPhase *pCur, *pNext;

    Setup();
    pCur  = MakePhase();
    pNext = MakePhase();
    g_ctx.pAA2904 = pCur;
    g_ctx.pAA2940 = pNext;
    g_ctx.nA9D000 = 0;

    CHECK(BrPhaseLeave_10044AE0(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF1C == 1);
    CHECK(g_c.nF18 == 0);           /* nA9D000 clear -> no +0x18 */
    CHECK(g_c.n10038F30 == 0);
    CHECK(g_c.nF00 == 1);
    CHECK(g_pNotified == pCur);
    CHECK(g_notifyArg == 1);
    CHECK(g_ctx.pAA2904 == pNext);
    CHECK(g_c.n1003BF60 == 1);

    /* a NULL current phase is skipped, not dereferenced */
    Setup();
    g_ctx.pAA2904 = NULL;
    CHECK(BrPhaseLeave_10044AE0(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF1C == 1);
    CHECK(g_c.nF00 == 0);
    CHECK(g_ctx.pAA2904 == NULL);   /* pAA2940 was NULL too */

    /* nA9D000 set: +0x18 with 0, then 0x10038F30, then +0x1C */
    Setup();
    g_ctx.nA9D000 = 1;
    CHECK(BrPhaseLeave_10044970(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF18 == 1);
    CHECK(g_nF18Arg == 0);
    CHECK(g_c.n10038F30 == 1);
    CHECK(g_c.nF1C == 1);

    free(pCur);
    free(pNext);
}

/* 0x10044AE0 clears five globals that its neighbour 0x10044B40 leaves
 * alone, and vice versa. */
static void TestLeaveClearSets(void)
{
    BrPhase *pDead, *pNext;

    Setup();
    pDead = MakePhase();
    pNext = MakePhase();
    g_ctx.pAA2940 = pNext;
    g_ctx.pAA2948 = pDead;
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nAA29B8 = 7;
    g_ctx.nAA29D4 = 7;
    g_ctx.nAA2880 = 7;
    g_ctx.nAA298C = 7;
    g_ctx.nAA29E8 = 7;

    CHECK(BrPhaseLeave_10044AE0(&g_ctx, &g_ent) == 0);
    CHECK(g_ctx.pAA2948 == NULL);
    CHECK(g_ctx.pAA29D8 == NULL);
    CHECK(g_ctx.nAA29B8 == 0);
    CHECK(g_ctx.nAA29D4 == 0);
    CHECK(g_ctx.nAA2880 == 0);
    CHECK(g_ctx.nAA298C == 7);      /* untouched here */
    CHECK(g_ctx.nAA29E8 == 7);
    CHECK(g_ctx.pAA2904 == pNext);

    Setup();
    g_ctx.pAA2940 = pNext;
    g_ctx.pAA2948 = pDead;
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nAA298C = 7;
    g_ctx.nAA29E8 = 7;
    CHECK(BrPhaseLeave_10044B40(&g_ctx, &g_ent) == 0);
    CHECK(g_ctx.nAA298C == 0);
    CHECK(g_ctx.nAA29E8 == 0);
    CHECK(g_ctx.pAA2948 == pDead);  /* untouched here */
    CHECK(g_ctx.pAA29D8 == &g_entD8);
    CHECK(g_ctx.pAA2904 == pNext);
    CHECK(g_c.n1003BF60 == 0);      /* 0x10044B40 does not call it */

    free(pDead);
    free(pNext);
}

/* The three plain "drop my slot and fall back" routines. */
static void TestLeaveFallbacks(void)
{
    BrPhase *pA, *pB;

    Setup();
    pA = MakePhase();
    pB = MakePhase();
    g_ctx.pAA295C = pA;
    g_ctx.pAA2908 = pB;
    CHECK(BrPhaseLeave_10044C70(&g_ctx, &g_ent) == 0);
    CHECK(g_ctx.pAA295C == NULL);
    CHECK(g_ctx.pAA2904 == pB);

    Setup();
    g_ctx.pAA295C = pA;
    g_ctx.pAA290C = pB;
    g_ctx.nAA29AC = 7;
    CHECK(BrPhaseLeave_10044CB0(&g_ctx, &g_ent) == 0);
    CHECK(g_ctx.pAA290C == NULL);
    CHECK(g_ctx.nAA29AC == 0);
    CHECK(g_ctx.pAA2904 == pA);

    Setup();
    g_ctx.pAA295C = pA;
    g_ctx.pAA2964 = pB;
    CHECK(BrPhaseLeave_10044DE0(&g_ctx, &g_ent) == 0);
    CHECK(g_ctx.pAA2964 == NULL);
    CHECK(g_ctx.pAA2904 == pA);

    free(pA);
    free(pB);
}

/* 0x10044F00 notifies the phase it is dropping, not the current one. */
static void TestLeave10044F00NotifiesOther(void)
{
    BrPhase *pCur, *pDrop, *pNext;

    Setup();
    pCur  = MakePhase();
    pDrop = MakePhase();
    pNext = MakePhase();
    g_ctx.pAA2904 = pCur;
    g_ctx.pAA2968 = pDrop;
    g_ctx.pAA295C = pNext;

    CHECK(BrPhaseLeave_10044F00(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF00 == 1);
    CHECK(g_pNotified == pDrop);      /* NOT pCur */
    CHECK(g_ctx.pAA2968 == NULL);
    CHECK(g_ctx.pAA2904 == pNext);
    CHECK(g_ctx.n0AA010 == 2);

    /* with the dropped slot already NULL nothing is notified */
    Setup();
    g_ctx.pAA2904 = pCur;
    g_ctx.pAA2968 = NULL;
    CHECK(BrPhaseLeave_10044F00(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF00 == 0);

    free(pCur);
    free(pDrop);
    free(pNext);
}

/* nAA287C is re-read after 0x1003C020 runs, so a mode that routine changes is
 * the one the 2/3 test sees. And only 0x10044A30 clears the +0x2B64 byte. */
static void TestModeReloadAndEntityBytes(void)
{
    Setup();
    g_ctx.pAA29D8  = &g_entD8;
    g_ctx.nA9D000  = 0;
    g_ctx.nAA287C  = 0;             /* takes the 0x1003C020 arm */
    g_setMode      = 1;
    g_mode1003C020 = 3;             /* ...which switches the mode to 3 */
    EntitySetFlags(&g_entD8, 0xFFFFFFFFu);
    g_entD8.b[BR_ENTITY_OFF_F2B64] = 0x5A;

    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
    CHECK(g_c.n1003C020 == 1);
    CHECK(g_entD8.b[BR_ENTITY_OFF_F2B64] == 0);           /* re-read won */
    CHECK((EntityFlags(&g_entD8) & BR_ENTITY_FLAG_1C_10) == 0);
    CHECK((EntityFlags(&g_entD8) & ~BR_ENTITY_FLAG_1C_10) == 0xFFFFFFEFu);

    /* control: 0x1003C020 leaves the mode at 0, so the 2/3 arm is skipped */
    Setup();
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nA9D000 = 0;
    g_ctx.nAA287C = 0;
    g_setMode     = 0;
    EntitySetFlags(&g_entD8, 0xFFFFFFFFu);
    g_entD8.b[BR_ENTITY_OFF_F2B64] = 0x5A;
    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
    CHECK(g_c.n1003C020 == 1);
    CHECK(g_entD8.b[BR_ENTITY_OFF_F2B64] == 0x5A);        /* untouched */
    CHECK(EntityFlags(&g_entD8) == 0xFFFFFFFFu);          /* also untouched */

    /* mode 2 outright: no 0x1003C020, and the byte still goes */
    Setup();
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nAA287C = 2;
    EntitySetFlags(&g_entD8, BR_ENTITY_FLAG_1C_10);
    g_entD8.b[BR_ENTITY_OFF_F2B64] = 0x5A;
    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
    CHECK(g_c.n1003C020 == 0);
    CHECK(g_entD8.b[BR_ENTITY_OFF_F2B64] == 0);
    CHECK(EntityFlags(&g_entD8) == 0);

    /* 0x10044970 never touches +0x2B64, sets nAA2898, and clears the flag
     * bit even for mode 2 */
    Setup();
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nAA287C = 2;
    EntitySetFlags(&g_entD8, 0xFFFFFFFFu);
    g_entD8.b[BR_ENTITY_OFF_F2B64] = 0x5A;
    CHECK(BrPhaseLeave_10044970(&g_ctx, &g_ent) == 0);
    CHECK(g_entD8.b[BR_ENTITY_OFF_F2B64] == 0x5A);
    CHECK((EntityFlags(&g_entD8) & BR_ENTITY_FLAG_1C_10) == 0);
    CHECK(g_ctx.nAA2898 == 1);

    /* ...and it clears the bit even when the mode arm is not taken at all */
    Setup();
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nA9D000 = 1;          /* skips the 0x1003C020 arm */
    g_ctx.nAA287C = 0;
    EntitySetFlags(&g_entD8, BR_ENTITY_FLAG_1C_10 | 1u);
    CHECK(BrPhaseLeave_10044970(&g_ctx, &g_ent) == 0);
    CHECK(g_c.n1003C020 == 0);
    CHECK(EntityFlags(&g_entD8) == 1u);

    /* 0x10044A30, by contrast, does NOT clear the bit up front: with the
     * mode outside 2/3 the flags survive untouched */
    Setup();
    g_ctx.pAA29D8 = &g_entD8;
    g_ctx.nA9D000 = 1;
    g_ctx.nAA287C = 1;
    EntitySetFlags(&g_entD8, BR_ENTITY_FLAG_1C_10 | 1u);
    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
    CHECK(EntityFlags(&g_entD8) == (BR_ENTITY_FLAG_1C_10 | 1u));
    CHECK(g_ctx.nAA2898 == 0);   /* and it does not set this */
}

/* A NULL pAA29D8 is tested, not dereferenced. */
static void TestLeaveNullEntityGlobal(void)
{
    Setup();
    g_ctx.pAA29D8 = NULL;
    g_ctx.nAA287C = 3;
    CHECK(BrPhaseLeave_10044970(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
}

/* The hook installers, and the guard flag that brackets the activation. */
static void TestHooks(void)
{
    BrPhase *pB4, *pB0, *pF4;

    Setup();
    pB4 = MakePhase();
    g_ctx.pAA29B4 = pB4;
    g_ctx.n0AC304 = 0x7F;
    CHECK(BrPhaseHook_10045050(&g_ctx, &g_ent) == 1);
    CHECK(g_seenAC304 == 0);            /* cleared across the activation */
    CHECK(g_ctx.n0AC304 == 1);          /* and set again afterwards */
    CHECK(pB4->pfn08 == BrExt_10046CD0);
    CHECK(g_ctx.n0AA010 == 0);
    CHECK(g_ctx.pAA2914 != NULL);       /* it went through 0x10045110 */
    CHECK(g_c.aEnter[E_1004A580] == 1);
    free(g_ctx.pAA2914);

    Setup();
    pB0 = MakePhase();
    g_ctx.pAA29B0 = pB0;
    g_ctx.n0AA010 = 9;
    CHECK(BrPhaseHook_10045090(&g_ctx, &g_ent) == 1);
    CHECK(g_c.n10045C90 == 1);
    CHECK(g_p10045C90Arg == &g_ent);
    CHECK(g_c.n10041BD0 == 0);
    CHECK(pB0->pfn08 == BrExt_10046DC0);
    CHECK(g_ctx.n0AA010 == 0);

    Setup();
    g_ctx.pAA29B0 = pB0;
    pB0->pfn08 = NULL;
    CHECK(BrPhaseHook_100450C0(&g_ctx, &g_ent) == 1);
    CHECK(g_c.n10041BD0 == 1);          /* the only difference */
    CHECK(g_c.n10045C90 == 1);
    CHECK(pB0->pfn08 == BrExt_10046DC0);

    /* the dispatcher forwards its own argument and returns 0, not 1 */
    Setup();
    pF4 = MakePhase();
    pF4->pfn08 = BrExt_10046DC0;
    g_ctx.pAA29F4 = pF4;
    g_ctx.n0AA010 = 9;
    CHECK(BrPhaseDispatch_100450F0(&g_ctx, &g_ent) == 0);
    CHECK(g_c.n10046DC0 == 1);
    CHECK(g_pLastEntity == &g_ent);
    CHECK(g_ctx.n0AA010 == 0);

    free(pB4);
    free(pB0);
    free(pF4);
}

/* 0x100447D0, host present. */
static void TestBootWithHost(void)
{
    BrHostItem item;

    Setup();
    g_hostVtbl.f7C = HostF7C;
    item.f00 = 0;
    item.f04 = 0xFFFFFFFFu;
    g_pItemOut = &item;
    g_ctx.nAA2884 = 1;
    g_ctx.nAA2888 = 0;
    g_a9d008.f08 = (void *)&item;

    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 1);
    CHECK(g_ctx.nA9CFFC == 1);
    CHECK(g_c.nSlotsReset == 1);
    CHECK(g_pSlotsSeen == &g_slots);
    CHECK(g_c.n1003D0B0 == 1);
    CHECK(item.f04 == 0xFFFFFFDFu);          /* bit 0x20 cleared */
    CHECK(g_c.nF7C == 1);
    CHECK(g_pF7CSelf == &g_host);
    CHECK(g_pF7CItem == &item);
    CHECK(g_nF7CArg == 0);
    CHECK(g_c.n10043BF0 == 1 && g_c.n10043CD0 == 1 && g_c.n10043E70 == 1);
    CHECK(g_c.n100440D0 == 1 && g_c.n100443E0 == 1);
    CHECK(g_c.n10044280 == 0);               /* the nAA2884 == 0 arm */
    CHECK(g_ctx.pAA2954 != NULL);
    CHECK(g_ctx.pAA2954->pfn04 == BrExt_10058750);
    CHECK(g_ctx.pAA2904 == g_ctx.pAA2954);
    CHECK(g_ctx.n0AA010 == 6);
    CHECK(g_c.n1003C150 == 1);
    CHECK(g_ctx.nAA2888 == 1);
    CHECK(g_c.n1003CDA0 == 0);
    CHECK(g_c.n1003DB00 == 1);
    CHECK(g_p1003DB00_a == &g_a9d008);
    CHECK(g_p1003DB00_b == (void *)&item);
    free(g_ctx.pAA2954);
}

/* 0x100447D0, the other side of every branch. */
static void TestBootWithoutHost(void)
{
    Setup();
    g_hostVtbl.f7C = HostF7C;
    g_ctx.nAA2884 = 0;
    g_ctx.nAA2888 = 5;
    g_a9d008.f08 = NULL;

    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 1);
    CHECK(g_c.n1003D0B0 == 0);               /* gated on nAA2884 */
    CHECK(g_c.nF7C == 0);
    CHECK(g_c.n100440D0 == 0 && g_c.n100443E0 == 0);
    CHECK(g_c.n10044280 == 1);
    CHECK(g_c.n1003C150 == 0);
    CHECK(g_c.n1003CDA0 == 0);               /* whole block gated too */
    CHECK(g_ctx.nAA2888 == 5);
    CHECK(g_c.n1003DB00 == 0);               /* f08 was NULL */
    CHECK(g_ctx.n0AA010 == 6);
    free(g_ctx.pAA2954);

    /* nAA2884 set and nAA2888 already non-zero -> the other arm */
    Setup();
    g_ctx.nAA2884 = 1;
    g_ctx.nAA2888 = 5;
    g_pItemOut = NULL;                       /* 0x1003D0B0 hands back NULL */
    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 1);
    CHECK(g_c.n1003D0B0 == 1);
    CHECK(g_c.nF7C == 0);                    /* NULL item -> no +0x7C call */
    CHECK(g_c.n1003CDA0 == 1);
    CHECK(g_c.n1003C150 == 0);
    CHECK(g_ctx.nAA2888 == 5);
    free(g_ctx.pAA2954);

    /* no host object at all */
    Setup();
    g_ctx.nAA2884 = 1;
    g_ctx.p277B40 = NULL;
    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 1);
    CHECK(g_c.n1003D0B0 == 0);
    CHECK(g_c.nF7C == 0);
    free(g_ctx.pAA2954);

    /* allocation failure stops everything downstream of the activation */
    Setup();
    g_ctx.nAA2884 = 1;
    g_allocFail = 1;
    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 0);
    CHECK(g_c.n10043BF0 == 1);               /* the teardown still ran */
    CHECK(g_ctx.n0AA010 == 0);               /* but never reached the 6 */
    CHECK(g_c.n1003C150 == 0);
    CHECK(g_c.n1003DB00 == 0);
    CHECK(g_ctx.pAA2904 == NULL);

    /* a pA9D008 of NULL is tested, not dereferenced */
    Setup();
    g_ctx.pA9D008 = NULL;
    CHECK(BrPhaseActivate_100447D0(&g_ctx) == 1);
    CHECK(g_c.n1003DB00 == 0);
    free(g_ctx.pAA2954);
}

/* Every leave routine returns 0 and every activate routine returns 1 on a
 * successful run -- the two families disagree deliberately. */
static void TestReturnValueFamilies(void)
{
    Setup();
    CHECK(BrPhaseActivate_10044B90(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10044D00(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10044E20(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10044F50(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10045110(&g_ctx) == 1);
    CHECK(BrPhaseActivate_100451E0(&g_ctx) == 1);
    CHECK(BrPhaseActivate_100452C0(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10045390(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10045460(&g_ctx) == 1);
    CHECK(BrPhaseActivate_10045520(&g_ctx) == 1);
    CHECK(BrPhaseActivate_100455E0(&g_ctx) == 1);
    CHECK(BrPhaseActivate_100456B0(&g_ctx) == 1);
    CHECK(g_c.nCtor == 12);

    free(g_ctx.pAA295C); free(g_ctx.pAA2964); free(g_ctx.pAA2968);
    free(g_ctx.pAA290C); free(g_ctx.pAA2914); free(g_ctx.pAA2918);
    free(g_ctx.pAA297C); free(g_ctx.pAA2980); free(g_ctx.pAA2990);
    free(g_ctx.pAA2994); free(g_ctx.pAA2984); free(g_ctx.pAA2988);

    Setup();
    CHECK(BrPhaseLeave_10044970(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044A30(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044AE0(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044B40(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044C70(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044CB0(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044DE0(&g_ctx, &g_ent) == 0);
    CHECK(BrPhaseLeave_10044F00(&g_ctx, &g_ent) == 0);
    CHECK(g_c.nF1C == 8);         /* one per leave, always */
}

int main(void)
{
    g_phaseVtbl.f00 = PhaseF00;
    g_subVtbl.f18   = SubF18;
    g_subVtbl.f1C   = SubF1C;
    g_hostVtbl.f7C  = HostF7C;
    g_pPhaseVtbl    = &g_phaseVtbl;

    TestActivateIsIdempotent();
    TestActivateAllocFailure();
    TestActivate10044E20Crossover();
    TestEpiloguePaths();
    TestEnterHookReload();
    TestSlotToHookWiring();
    TestLeavePrologue();
    TestLeaveClearSets();
    TestLeaveFallbacks();
    TestLeave10044F00NotifiesOther();
    TestModeReloadAndEntityBytes();
    TestLeaveNullEntityGlobal();
    TestHooks();
    TestBootWithHost();
    TestBootWithoutHost();
    TestReturnValueFamilies();

    printf("test_slice2_26: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}
