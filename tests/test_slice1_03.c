#include "slice1_03.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

static int near0(float v) { return fabsf(v) < 1e-5f; }

/* ---- shared clip fixtures ------------------------------------------- */

#define POOL_N 64
static BrClipVert g_pool[POOL_N];

/* Build a circular list of cVerts nodes out of aNodes, w[] into f18 and
 * x[] into f04, and point the list at aNodes[0]. */
static void ring(BrClipList *pList, BrClipVert *aNodes, int n,
                 const float *w, const float *x)
{
    int i;
    for (i = 0; i < n; i++) {
        memset(&aNodes[i], 0, sizeof(aNodes[i]));
        aNodes[i].pNext = &aNodes[(i + 1) % n];
        aNodes[i].f18 = w[i];
        aNodes[i].f04 = (x != NULL) ? x[i] : 0.0f;
        aNodes[i].f0C = (float)(i + 1);   /* a payload field to watch */
    }
    pList->pHead = &aNodes[0];
    pList->cVerts = n;
}

/* Walk the ring; return the number of nodes before coming back to the head,
 * or -1 if it does not close within `limit` steps. */
static int ring_len(const BrClipList *pList, int limit)
{
    const BrClipVert *p = pList->pHead;
    int n = 0;
    if (p == NULL) return 0;
    do {
        p = p->pNext;
        if (p == NULL) return -1;
        if (++n > limit) return -1;
    } while (p != pList->pHead);
    return n;
}

/* ---- text hooks ------------------------------------------------------ */

static int   g_measureScale;
static char  g_drawn[64];
static int   g_drawCount;

static int measure_hook(const char *psz, int scale)
{
    g_measureScale = scale;
    return (int)strlen(psz) * 10;   /* deterministic 10px per char */
}
static void draw_hook(const char *psz)
{
    strncpy(g_drawn, psz, sizeof(g_drawn) - 1);
    g_drawn[sizeof(g_drawn) - 1] = '\0';
    g_drawCount++;
}

/* ---- app-message hooks ---------------------------------------------- */

static int32_t g_m5arg;
static int     g_m5hits;
static int32_t g_m107[3];
static void   *g_m107pv1, *g_m107pv5;
static int     g_m107hits;

static void msg5_hook(int32_t f08) { g_m5arg = f08; g_m5hits++; }
static void msg107_hook(void *pv1, int32_t a, int32_t b, int32_t c, void *pv5)
{ g_m107pv1 = pv1; g_m107[0] = a; g_m107[1] = b; g_m107[2] = c;
  g_m107pv5 = pv5; g_m107hits++; }

/* ---- COM stubs ------------------------------------------------------- */

static void *g_p24arg;
static int   g_p24hits;
static int   com_pfn24(BrComObj *pThis, void *pv)
{ (void)pThis; g_p24arg = pv; g_p24hits++; return 0x5A; }

static int   g_lockDepthAtCall;
static int   g_lockDepth;
static void *g_p68args[5];
static void  lock_enter(void *p) { (void)p; g_lockDepth++; }
static void  lock_leave(void *p) { (void)p; g_lockDepth--; }
static int   com_pfn68(BrComObj *pThis, void *a2, void *a3, void *a4,
                       void *a5, void *a6)
{
    (void)pThis;
    g_lockDepthAtCall = g_lockDepth;
    g_p68args[0] = a2; g_p68args[1] = a3; g_p68args[2] = a4;
    g_p68args[3] = a5; g_p68args[4] = a6;
    return 0x1234;
}

int main(void)
{
    /* =============================================================
     * BrClipLerpVert -- 0x1001D940
     * ============================================================= */
    printf("BrClipLerpVert\n");
    {
        BrClipVert a, b, *p;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        a.f04 = 1; a.f08 = 2; a.f0C = 3; a.f10 = 4; a.f14 = 5;
        a.f18 = 6; a.f1C = 7; a.f20 = 8; a.f24 = 9;
        b.f04 = 11; b.f08 = 12; b.f0C = 13; b.f10 = 14; b.f14 = 15;
        b.f18 = 16; b.f1C = 17; b.f20 = 18; b.f24 = 19;

        BrClipPoolInit(g_pool, POOL_N);
        check(BrClipPoolCount() == POOL_N, "fresh pool holds every node");

        p = BrClipLerpVert(&a, &b, 0.0f);
        check(p != NULL && p->f04 == 1 && p->f18 == 6 && p->f24 == 9,
              "t=0 reproduces the FIRST argument");
        check(BrClipPoolCount() == POOL_N - 1, "one node consumed");

        p = BrClipLerpVert(&a, &b, 1.0f);
        check(p != NULL && p->f04 == 11 && p->f18 == 16 && p->f24 == 19,
              "t=1 reproduces the SECOND argument");

        p = BrClipLerpVert(&a, &b, 0.5f);
        check(p != NULL && p->f04 == 6 && p->f18 == 11 && p->f24 == 14,
              "t=0.5 is the midpoint of all nine fields");
    }

    /* the pool is finite and the original never grows it */
    {
        int i;
        BrClipVert a, b, *p = NULL;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        BrClipPoolInit(g_pool, POOL_N);
        for (i = 0; i < POOL_N; i++) p = BrClipLerpVert(&a, &b, 0.5f);
        check(p != NULL && BrClipPoolCount() == 0, "pool drains to empty");
        check(BrClipLerpVert(&a, &b, 0.5f) == NULL,
              "exhausted pool yields NULL (original would fault)");
    }

    /* =============================================================
     * BrClipPlaneW -- 0x1001D810
     * ============================================================= */
    printf("BrClipPlaneW\n");
    {
        /* all inside: nothing may be added, removed or freed */
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { 1.0f, 2.0f, 3.0f };
        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, NULL);
        BrClipPlaneW(&l);
        check(l.cVerts == 3, "wholly-inside polygon keeps its count");
        check(ring_len(&l, 8) == 3, "wholly-inside polygon keeps its ring");
        check(BrClipPoolCount() == POOL_N, "wholly-inside consumes no nodes");
        /* documented rotation: the head advances one vertex per call */
        check(l.pHead == &v[1], "head is rotated forward by exactly one");
    }
    {
        /* one vertex behind the plane -> 4-gon, both new vertices ON it */
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { 1.0f, -1.0f, 1.0f };
        const BrClipVert *p;
        int i, nOnPlane = 0, nBehind = 0, n;

        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, NULL);
        BrClipPlaneW(&l);

        check(l.cVerts == 4, "one-vertex-out triangle becomes a 4-gon");
        n = ring_len(&l, 16);
        check(n == 4, "ring really does hold four nodes");

        p = l.pHead;
        for (i = 0; i < 4 && i < n; i++) {
            if (near0(p->f18)) nOnPlane++;
            if (p->f18 < -1e-5f) nBehind++;
            p = p->pNext;
        }
        check(nBehind == 0, "no surviving vertex is behind the plane");
        check(nOnPlane == 2, "exactly two vertices sit ON the plane");
        check(BrClipPoolCount() == POOL_N - 2,
              "two pool nodes spent, none reclaimed (the dropped vertex "
              "was not pool storage)");
    }
    {
        /* wholly outside: the original bails as soon as the count drops
         * below 2, so it stops at 1 rather than reaching 0 */
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { -1.0f, -2.0f, -3.0f };
        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, NULL);
        BrClipPlaneW(&l);
        check(l.cVerts < 2, "wholly-outside polygon is rejected (count < 2)");
        check(BrClipPoolCount() == POOL_N,
              "non-pool vertices are dropped, never freed into the pool");
    }
    {
        /* the same rejection, but with vertices that ARE pool storage:
         * now they must come back */
        BrClipList l;
        const float w[3] = { -1.0f, -2.0f, -3.0f };
        int before;
        BrClipPoolInit(g_pool, POOL_N);
        /* take the first three nodes off the free list by hand */
        g_pool[0].pNext = NULL;
        BrClipPoolInit(g_pool + 3, POOL_N - 3);
        before = BrClipPoolCount();
        ring(&l, g_pool, 3, w, NULL);
        BrClipPlaneW(&l);
        check(BrClipPoolCount() == before,
              "pool-resident discards are recycled, non-pool ones are not");
    }
    {
        /* NaN must behave exactly like a behind-the-plane vertex, because
         * the x87 compare the original branches on sets C0 for unordered as
         * well as for less-than. Compare the two runs rather than asserting
         * a count: if NaN were treated as INSIDE the polygon would come out
         * untouched at 3 vertices instead. */
        BrClipVert vNan[3], vNeg[3];
        BrClipList lNan, lNeg;
        float wNan[3], wNeg[3];
        const BrClipVert *p;
        int i, nNan = 0;

        wNan[0] = 1.0f; wNan[1] = (float)NAN; wNan[2] = 1.0f;
        wNeg[0] = 1.0f; wNeg[1] = -1.0f;      wNeg[2] = 1.0f;

        BrClipPoolInit(g_pool, POOL_N);
        ring(&lNan, vNan, 3, wNan, NULL);
        BrClipPlaneW(&lNan);

        BrClipPoolInit(g_pool, POOL_N);
        ring(&lNeg, vNeg, 3, wNeg, NULL);
        BrClipPlaneW(&lNeg);

        check(lNan.cVerts == lNeg.cVerts &&
              ring_len(&lNan, 16) == ring_len(&lNeg, 16),
              "a NaN distance clips exactly like a behind-plane one");
        check(lNan.cVerts != 3,
              "...and specifically is not silently kept as inside");

        p = lNan.pHead;
        for (i = 0; i < lNan.cVerts; i++) {
            if (p->f18 != p->f18) nNan++;
            p = p->pNext;
        }
        check(nNan == 0, "the NaN vertex itself is gone from the ring");
    }

    /* =============================================================
     * the other three planes -- 0x1001D9F0 / 0x1001DB30 / 0x1001DC70
     * ============================================================= */
    printf("other clip planes\n");
    {
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { 1.0f, 1.0f, 1.0f };
        const float x[3] = { 0.0f, -2.0f, 0.0f };   /* w+x < 0 at vertex 1 */
        const BrClipVert *p;
        int i, nOnPlane = 0, nBehind = 0;

        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, x);
        BrClipPlaneWPlusF04(&l);
        check(l.cVerts == 4, "f18+f04 plane clips a straddling triangle");
        p = l.pHead;
        for (i = 0; i < 4; i++) {
            if (near0(p->f18 + p->f04)) nOnPlane++;
            if (p->f18 + p->f04 < -1e-5f) nBehind++;
            p = p->pNext;
        }
        check(nBehind == 0 && nOnPlane == 2,
              "new vertices land exactly on the f18+f04 plane");
    }
    {
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { 1.0f, 1.0f, 1.0f };
        const float x[3] = { 0.0f, 2.0f, 0.0f };    /* w-x < 0 at vertex 1 */
        const BrClipVert *p;
        int i, nOnPlane = 0, nBehind = 0;

        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, x);
        BrClipPlaneWMinusF04(&l);
        check(l.cVerts == 4, "f18-f04 plane clips a straddling triangle");
        p = l.pHead;
        for (i = 0; i < 4; i++) {
            if (near0(p->f18 - p->f04)) nOnPlane++;
            if (p->f18 - p->f04 < -1e-5f) nBehind++;
            p = p->pNext;
        }
        check(nBehind == 0 && nOnPlane == 2,
              "new vertices land exactly on the f18-f04 plane");
    }
    {
        /* f08+f18: the same polygon must be untouched by the f04 planes,
         * which is what shows the four routines really do read different
         * fields rather than being copy-paste duplicates */
        BrClipVert v[3];
        BrClipList l;
        const float w[3] = { 1.0f, 1.0f, 1.0f };
        int i;

        BrClipPoolInit(g_pool, POOL_N);
        ring(&l, v, 3, w, NULL);
        for (i = 0; i < 3; i++) v[i].f08 = 0.0f;
        v[1].f08 = -2.0f;                 /* f08+f18 < 0 only at vertex 1 */

        BrClipPlaneWMinusF04(&l);
        check(l.cVerts == 3, "f04 plane ignores an f08-only excursion");
        BrClipPlaneWPlusF08(&l);
        check(l.cVerts == 4, "f08+f18 plane does clip it");
    }

    /* =============================================================
     * Text -- 0x10019300 / 0x100192A0
     * ============================================================= */
    printf("text\n");
    {
        uint32_t gfx[8];
        BrTextState *pSt = BrTextGetState();

        memset(gfx, 0, sizeof(gfx));
        memset(pSt, 0, sizeof(*pSt));
        pSt->pGfx = gfx;
        pSt->scale = 7;
        pSt->pfnMeasure = measure_hook;
        pSt->pfnDrawString = draw_hook;

        pSt->align = BR_TEXT_ALIGN_LEFT;
        BrTextDraw("abcd", 100, 50);
        check(pSt->x == 100 && pSt->y == 50, "left align uses x as given");
        check(g_drawCount == 1 && strcmp(g_drawn, "abcd") == 0,
              "the string reaches the draw callback");
        check(gfx[0] == 0xB6000000u && gfx[1] == 1u,
              "gfx command is 0xB6000000 / 0x00000001");
        check(pSt->pGfx == gfx + 2, "gfx cursor advances by two dwords");

        pSt->align = BR_TEXT_ALIGN_RIGHT;
        BrTextDraw("abcd", 100, 50);
        check(pSt->x == 100 - 40, "right align subtracts the full width");
        check(g_measureScale == 7,
              "the scale global, not an argument, is handed to measure");

        pSt->align = BR_TEXT_ALIGN_CENTER;
        BrTextDraw("abcd", 100, 50);
        check(pSt->x == 100 - 20, "centre align subtracts half the width");

        /* the quirk worth keeping: an unrecognised align leaves x alone */
        pSt->x = 4242;
        pSt->align = 3;
        BrTextDraw("abcd", 100, 77);
        check(pSt->x == 4242, "unknown align does NOT write x");
        check(pSt->y == 77, "unknown align still writes y");

        check(pSt->pGfx == gfx + 8, "one command emitted per draw call");
    }
    {
        BrTextState *pSt = BrTextGetState();
        memset(pSt, 0, sizeof(*pSt));
        BrTextSetColors(1, 2, 3, 4, 5, 6);
        check(pSt->f0A74A8 == 1 && pSt->f0A74AC == 2 && pSt->f0A74B0 == 3,
              "first triple goes to 0x100A74A8..B0");
        check(pSt->f4B0368 == 4 && pSt->f4B036C == 5 && pSt->f4B0370 == 6,
              "second triple goes to 0x104B0368..70");
        check(pSt->f4B0364 == 1, "the 0x104B0364 flag is raised");
    }

    /* =============================================================
     * BrFormatTime / BrHudDrawTimeEntry -- 0x100171F0
     * ============================================================= */
    printf("time formatting\n");
    {
        char sz[64];
        BrFormatTime(sz, sizeof(sz), "", 125.5f);
        check(strcmp(sz, "2:05.50") == 0, "125.5s -> 2:05.50");

        BrFormatTime(sz, sizeof(sz), "T ", 0.0f);
        check(strcmp(sz, "T 0:00.00") == 0, "prefix is prepended verbatim");

        /* minutes are NOT wrapped into hours */
        BrFormatTime(sz, sizeof(sz), "", 3661.25f);
        check(strcmp(sz, "61:01.25") == 0, "minutes past 60 are not wrapped");

        /* every division truncates toward zero, so negatives come out with
         * negative fields rather than borrowing -- faithful to the original */
        BrFormatTime(sz, sizeof(sz), "", -1.5f);
        check(strcmp(sz, "0:-1.-50") == 0,
              "negative input truncates toward zero in every field");
    }
    {
        BrTextState *pSt = BrTextGetState();
        memset(pSt, 0, sizeof(*pSt));
        pSt->align = BR_TEXT_ALIGN_LEFT;
        pSt->pfnDrawString = draw_hook;
        g_drawCount = 0;
        BrHudDrawTimeEntry("LAP", "", 125.5f, 20, 30);
        check(g_drawCount == 2, "two lines are drawn");
        /* the LABEL is drawn second, so it is the one that leaves its
         * coordinates behind */
        check(strcmp(g_drawn, "LAP") == 0 && pSt->y == 30,
              "time line first at y+15, label second at y");
    }

    /* =============================================================
     * BrAppMsgDispatch -- 0x1000BEA0
     * ============================================================= */
    printf("app message dispatch\n");
    {
        BrAppMsgHooks *pH = BrAppMsgGetHooks();
        BrAppMsg m;
        int pv1 = 0, pv5 = 0;
        int32_t ids[] = { 0, 3, 5, 0x21, 0x22, 0x30, 0x31, 0x106, 0x107,
                          0x108, 0x200 };
        size_t i;

        memset(pH, 0, sizeof(*pH));
        pH->pfnMsg5 = msg5_hook;
        pH->pfnMsg107 = msg107_hook;

        memset(&m, 0, sizeof(m));
        m.f08 = 0x88; m.f0C = 0xCC; m.f10 = 0x10;

        g_m5hits = g_m107hits = 0;
        for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
            m.id = ids[i];
            BrAppMsgDispatch(&pv1, &m, NULL, NULL, &pv5);
        }
        check(g_m5hits == 1 && g_m107hits == 1,
              "of eleven ids only 0x5 and 0x107 do anything");
        check(g_m5arg == 0x88, "id 5 forwards pMsg->f08");
        check(g_m107pv1 == &pv1 && g_m107pv5 == &pv5,
              "id 0x107 forwards arg1 and arg5");
        check(g_m107[0] == 0xCC && g_m107[1] == 0x10 && g_m107[2] == 0x88,
              "id 0x107 forwards f0C, f10, f08 -- in THAT order");

        /* the 0x100AC300 gate suppresses id 5 only */
        pH->f0AC300 = 1;
        g_m5hits = g_m107hits = 0;
        m.id = 5;   BrAppMsgDispatch(&pv1, &m, NULL, NULL, &pv5);
        m.id = 0x107; BrAppMsgDispatch(&pv1, &m, NULL, NULL, &pv5);
        check(g_m5hits == 0, "the 0x100AC300 gate blocks id 5");
        check(g_m107hits == 1, "the gate does not touch id 0x107");
    }

    /* =============================================================
     * COM glue -- 0x1000C4A0 / 0x1000C4D0
     * ============================================================= */
    printf("com glue\n");
    {
        BrComVtbl vt;
        BrComObj  obj;
        BrComHolder h;
        int arg = 0;
        int rc;

        memset(&vt, 0, sizeof(vt));
        vt.pfn24 = com_pfn24;
        obj.pVtbl = &vt;

        *BrComGetHolderSlot() = NULL;
        g_p24hits = 0;
        check(BrComHolderRelease() == 0 && g_p24hits == 0,
              "null holder is a no-op");

        h.pObj = NULL; h.f04 = NULL; h.pArg = &arg;
        *BrComGetHolderSlot() = &h;
        check(BrComHolderRelease() == 0 && g_p24hits == 0,
              "null object is a no-op");

        h.pObj = &obj; h.pArg = NULL;
        check(BrComHolderRelease() == 0 && g_p24hits == 0,
              "null +0x08 argument is a no-op, and it is NOT cleared twice");

        h.pObj = &obj; h.pArg = &arg;
        rc = BrComHolderRelease();
        check(rc == 0x5A, "the vtable slot-9 result is returned");
        check(g_p24hits == 1 && g_p24arg == &arg,
              "slot 9 is called as (pObj, holder->pArg)");
        check(h.pArg == NULL, "+0x08 is cleared after the call");
    }
    {
        BrComVtbl vt;
        BrComObj  obj;
        BrComLockHooks *pL = BrComGetLockHooks();
        int a = 0, b = 0, c = 0, d = 0, e = 0;
        int rc;

        memset(&vt, 0, sizeof(vt));
        vt.pfn68 = com_pfn68;
        obj.pVtbl = &vt;

        pL->pCrit = &a;
        pL->pfnEnter = lock_enter;
        pL->pfnLeave = lock_leave;
        g_lockDepth = 0;

        rc = BrComCallLocked68(&obj, &a, &b, &c, &d, &e);
        check(rc == 0x1234, "the vtable slot-26 result is returned");
        check(g_lockDepthAtCall == 1, "the call happens inside the lock");
        check(g_lockDepth == 0, "the lock is released afterwards");
        check(g_p68args[0] == &a && g_p68args[1] == &b &&
              g_p68args[2] == &c && g_p68args[3] == &d &&
              g_p68args[4] == &e,
              "all five trailing arguments forwarded in order");
    }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
