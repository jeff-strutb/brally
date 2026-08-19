/* test_slice8_85.c -- the control hooks the six slice6_73.c builders install.
 *
 * WHAT THIS TEST IS FOR
 *
 * Twenty-five hook bodies, each a handful of instructions, each reached only
 * through a function-pointer slot a builder filled.  "It compiles" says
 * nothing about any of them.  What this file checks is the BEHAVIOUR each
 * body was transcribed for -- the value that lands, the field it lands in, the
 * order two side effects happen in, and the four asymmetries the disassembly
 * shows and a plausible-looking rewrite would smooth away:
 *
 *   - 0x1003E950's INVERSION: the flag SET gives the LOWER code (0x68).
 *   - 0x1003EB10 reloads the global on the negative arm, so the acknowledge
 *     sees the OLD value and not the -1.
 *   - 0x1003EE20 clamps what it SENDS and not what it STORES.
 *   - 0x10042AC0 is a one-shot latch: the second call does nothing.
 *
 * Every cross-module symbol is stood in for HERE and nowhere else, so the test
 * links against port/src/slice8_85.c alone and a failure can only be this
 * module's.  The stand-ins are scaffolding: none is decompiled and none is
 * asserted about except where it is the observation itself (the message log,
 * the CD track, the two teardown counters).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_85.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS
 * ========================================================================== */

/* --- storage the module reaches ----------------------------------------- */
BrUi73Ctx     g_br73;
BrUiHook81Ctx g_brHook81;
BrUiNav      *g_pBrUiNav;

int32_t g_brAA28D8;      /* 0x10AA28D8, slice6_73.h  */
int32_t g_br0AB3F4;      /* 0x100AB3F4, slice5_61.h  */
int32_t g_brB4E708;      /* 0x10B4E708, slice2_25.h  */
int32_t g_brB4E70C;      /* 0x10B4E70C               */
int32_t g_br0AB3D8;      /* 0x100AB3D8               */
int32_t g_brAA287C;      /* 0x10AA287C               */

/* --- callees ------------------------------------------------------------- */
int32_t BrFtolTrunc(float f) { return (int32_t)f; }

static int s_nOpen2948;
int BrOptOpen2948(void *pUnused)
{
    /* The real body never reads the argument; the test asserts that this
     * module honours that by passing NULL. */
    CHECK(pUnused == NULL);
    ++s_nOpen2948;
    return 1;
}

static int s_n1003E070;
void BrFn1003E070(void) { ++s_n1003E070; }

static int s_nKind;
static BrUiCtl_ *s_pKindArg;
int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl)
{
    ++s_nKind;
    s_pKindArg = pCtl;
    return 0;
}

static int s_nCdPlay, s_nCdTrack;
void BrCdTrackPlay(int track) { ++s_nCdPlay; s_nCdTrack = track; }

static int s_n1003E310, s_n1006A4A0;
static void *s_pA4A0This, *s_pA4A0Arg;
void BrSub1003E310(void) { ++s_n1003E310; }
void BrSub1006A4A0(void *pThis, void *pArg)
{
    ++s_n1006A4A0; s_pA4A0This = pThis; s_pA4A0Arg = pArg;
}

/* ==========================================================================
 * The observable objects
 * ========================================================================== */

/* Control vtable +0x14 -- the three-argument message send the draw hooks use.
 * The log IS the observation for 0x1003E7A0 / 0x1003E980 / 0x1003E9E0. */
#define MSG_MAX 64
typedef struct Msg { int32_t msg, x, y; } Msg;
static Msg s_aMsg[MSG_MAX];
static int s_cMsg;

static void MsgSend(BrUiCtl_ *pThis, int32_t msg, int32_t x, int32_t y)
{
    (void)pThis;
    if (s_cMsg < MSG_MAX) {
        s_aMsg[s_cMsg].msg = msg;
        s_aMsg[s_cMsg].x   = x;
        s_aMsg[s_cMsg].y   = y;
    }
    ++s_cMsg;
}

/* The SECOND control vtable, and the handler that swaps a control onto it.
 *
 * 0x1003E7A0 and 0x1003E980 read the +0x14 slot ONCE and spill the pointer
 * (0x1003E7F0 / 0x1003E9AA, both into E+4), then reach every later message
 * through the spill (`call [esp+0x28]` / `call [esp+0x20]`).  So a handler
 * that re-points the control's vtable mid-draw does NOT redirect the rest of
 * the row.  Nothing but a swap can tell a cached pointer from a re-read, which
 * is why this stand-in exists.  MsgSendOther logs into its own counter; if the
 * module ever goes back to re-reading `pCtl->pVtbl->f14`, that counter moves
 * and s_cMsg stops. */
static int s_cMsgOther;
static BrUiCtlVtbl_ s_ctlVtblOther;
static void MsgSendOther(BrUiCtl_ *pThis, int32_t msg, int32_t x, int32_t y)
{ (void)pThis; (void)msg; (void)x; (void)y; ++s_cMsgOther; }

static BrUiCtl_ *s_pSwapCtl;
static int32_t   s_swapOnMsg;
static void MsgSendSwap(BrUiCtl_ *pThis, int32_t msg, int32_t x, int32_t y)
{
    MsgSend(pThis, msg, x, y);
    if (msg == s_swapOnMsg && s_pSwapCtl != NULL) {
        s_pSwapCtl->pVtbl = &s_ctlVtblOther;
    }
}

/* Text-box vtable.  +0x14 is CONFLICT 2: the module calls it through an
 * int-returning type, so the stand-in is declared that way and installed
 * through the same union trick, which is itself part of what is being
 * checked -- if the two ever disagree this call goes wrong. */
static int s_nBox04, s_nBox08, s_nBox10, s_nBox2C, s_nBox14;
static int32_t s_box14Ret;

static void Box04(BrTextBox *p) { (void)p; ++s_nBox04; }
static void Box08(BrTextBox *p) { (void)p; ++s_nBox08; }
static void Box10(BrTextBox *p) { (void)p; ++s_nBox10; }
static void Box2C(BrTextBox *p) { (void)p; ++s_nBox2C; }
static int32_t Box14(BrTextBox *p) { (void)p; ++s_nBox14; return s_box14Ret; }

/* Text-list vtable +0x20 / +0x24 -- CONFLICT 1. */
static int     s_nSel, s_nAck;
static int32_t s_selSent, s_selRet, s_ackSent;

static int32_t ListSel(BrTextList *p, int32_t v)
{ (void)p; ++s_nSel; s_selSent = v; return s_selRet; }
static void ListAck(BrTextList *p, int32_t v)
{ (void)p; ++s_nAck; s_ackSent = v; }

/* Phase vtable. */
static int s_nPh18, s_nPh1C, s_nPh00;
static void *s_ph18Arg;
static int32_t s_ph00Flags;

static void  *Ph00(BrPhase_ *p, int32_t f) { (void)p; ++s_nPh00; s_ph00Flags = f; return p; }
static void   Ph18(BrPhase_ *p, void *a)   { (void)p; ++s_nPh18; s_ph18Arg = a; }
static void   Ph1C(BrPhase_ *p)            { (void)p; ++s_nPh1C; }

static BrUiCtlVtbl_  s_ctlVtbl;
static BrTextBoxVtbl s_boxVtbl;
static BrTextListVtbl s_listVtbl;
static BrPhaseVtbl_  s_phaseVtbl;

static BrScrGlobals  s_scr;
static BrActiveFlags s_active;
static BrUiNav       s_nav;
static char          s_scratchA[64], s_scratchB[64];
static int           s_b4df30, s_b4fbe8;

static BrUiCtl_ *NewCtl(void)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (p == NULL) { printf("FAIL out of memory\n"); ++g_fails; exit(1); }
    p->pVtbl        = &s_ctlVtbl;
    p->aText[0].pVtbl = &s_boxVtbl;
    p->list.pVtbl   = &s_listVtbl;
    return p;
}

static void Wire(void)
{
    union { void (*pfnVoid)(BrTextBox *); int32_t (*pfnAsk)(BrTextBox *); } u;

    memset(&s_ctlVtbl, 0, sizeof s_ctlVtbl);
    s_ctlVtbl.f14 = MsgSend;

    memset(&s_ctlVtblOther, 0, sizeof s_ctlVtblOther);
    s_ctlVtblOther.f14 = MsgSendOther;
    s_cMsgOther = 0; s_pSwapCtl = NULL; s_swapOnMsg = 0;

    memset(&s_boxVtbl, 0, sizeof s_boxVtbl);
    s_boxVtbl.pfn04 = Box04;
    s_boxVtbl.pfn08 = Box08;
    s_boxVtbl.pfn10 = Box10;
    s_boxVtbl.pfn2C = Box2C;
    u.pfnAsk = Box14;
    s_boxVtbl.pfn14 = u.pfnVoid;

    memset(&s_listVtbl, 0, sizeof s_listVtbl);
    s_listVtbl.f20 = (void *)ListSel;
    s_listVtbl.f24 = (void *)ListAck;

    memset(&s_phaseVtbl, 0, sizeof s_phaseVtbl);
    s_phaseVtbl.f00 = Ph00;
    s_phaseVtbl.f18 = Ph18;
    s_phaseVtbl.f1C = Ph1C;

    memset(&s_scr, 0, sizeof s_scr);
    s_scr.pB4DF30 = &s_b4df30;
    s_scr.pB4FBE8 = &s_b4fbe8;

    memset(&s_active, 0, sizeof s_active);
    memset(&s_nav, 0, sizeof s_nav);
    s_nav.pG      = &s_scr;
    s_nav.pActive = &s_active;
    g_pBrUiNav    = &s_nav;

    memset(&g_br73, 0, sizeof g_br73);
    g_br73.szAA2518  = s_scratchA;
    g_br73.szA9D618  = s_scratchB;
    g_br73.cbScratch = sizeof s_scratchA;

    BrUiHook85Reset();
    memset(&g_brHook81, 0, sizeof g_brHook81);
}

/* ==========================================================================
 * PART A -- the draw hooks
 * ========================================================================== */

static void TestDraw(void)
{
    BrUiCtl_ *p = NewCtl();
    int i;

    printf("\n0x1003E7A0 -- 0x3D, then one 0x3B per 16 px, then 0x3C\n");
    p->aText[0].x     = 100.0f;      /* +0x2F6C */
    p->aText[0].y     = 50.0f;       /* +0x2F70 */
    p->aText[0].width = 48;          /* +0x2F66 -> 48/16 + 1 == 4 */
    s_cMsg = 0;
    CHECK(BrUiHook85_1003E7A0(p) == 1);
    /* x0 = ftol(100) - 3 == 97;  y = ftol(50) - 0x0C == 38 */
    CHECK(s_cMsg == 6);
    CHECK(s_aMsg[0].msg == 0x3D && s_aMsg[0].x == 97 - 8 && s_aMsg[0].y == 38);
    for (i = 0; i < 4; i++) {
        CHECK(s_aMsg[1 + i].msg == 0x3B);
        CHECK(s_aMsg[1 + i].x   == 97 + 16 * i);
        CHECK(s_aMsg[1 + i].y   == 38);
    }
    /* The tail uses the SAVED count and the SAVED start, not the walking x. */
    CHECK(s_aMsg[5].msg == 0x3C && s_aMsg[5].x == 97 + 64 && s_aMsg[5].y == 38);

    /* A width of 0 still gives one bar: 0/16 + 1 == 1. */
    p->aText[0].width = 0;
    s_cMsg = 0;
    (void)BrUiHook85_1003E7A0(p);
    CHECK(s_cMsg == 3);
    CHECK(s_aMsg[2].x == 97 + 16);

    printf("0x1003E980 / 0x1003E9E0 -- 0x74 once, 0x75 per element\n");
    p->x = 10.0f;                    /* +0x3C */
    p->y = 20.0f;                    /* +0x40 */
    g_brB4E708 = 3;
    g_brB4E70C = 1;

    s_cMsg = 0;
    CHECK(BrUiHook85_1003E980(p) == 1);
    CHECK(s_cMsg == 4);
    CHECK(s_aMsg[0].msg == 0x74 && s_aMsg[0].x == 10 && s_aMsg[0].y == 20 + 0x13);
    for (i = 0; i < 3; i++) {
        CHECK(s_aMsg[1 + i].msg == 0x75);
        CHECK(s_aMsg[1 + i].x   == 10 + 12 * i);
        CHECK(s_aMsg[1 + i].y   == 20 + 0x13);
    }

    s_cMsg = 0;
    CHECK(BrUiHook85_1003E9E0(p) == 1);
    CHECK(s_cMsg == 2);              /* the OTHER count, which is 1 */

    /* The bound is compared UNSIGNED and re-read every pass; a zero count
     * emits the header and nothing else. */
    g_brB4E708 = 0;
    s_cMsg = 0;
    (void)BrUiHook85_1003E980(p);
    CHECK(s_cMsg == 1);

    printf("the +0x14 slot is fetched ONCE per hook, not once per message\n");
    /* 0x1003E9AA saves the slot and 0x1003E9C1 sends every 0x75 through the
     * saved pointer, so a 0x74 handler that swaps the vtable must not
     * redirect the row.  Same shape at 0x1003E7F0 / 0x1003E806 / 0x1003E823. */
    g_brB4E708 = 3;
    s_ctlVtbl.f14 = MsgSendSwap;
    s_swapOnMsg   = 0x74;
    s_pSwapCtl    = p;
    p->pVtbl      = &s_ctlVtbl;
    s_cMsg = 0; s_cMsgOther = 0;
    CHECK(BrUiHook85_1003E980(p) == 1);
    CHECK(s_cMsg      == 4);   /* 0x74 + three 0x75, all down the saved ptr */
    CHECK(s_cMsgOther == 0);   /* the swapped-in vtable is never consulted  */
    CHECK(p->pVtbl    == &s_ctlVtblOther);   /* the swap really did happen  */

    /* And the same for the three-message hook: the swap lands on the 0x3D. */
    p->pVtbl          = &s_ctlVtbl;
    p->aText[0].x     = 100.0f;
    p->aText[0].y     = 50.0f;
    p->aText[0].width = 48;      /* -> 0x3D, four 0x3B, 0x3C */
    s_swapOnMsg = 0x3D;
    s_cMsg = 0; s_cMsgOther = 0;
    CHECK(BrUiHook85_1003E7A0(p) == 1);
    CHECK(s_cMsg      == 6);
    CHECK(s_cMsgOther == 0);

    /* ...and on a 0x3B, so the spill is proved live across the loop too. */
    p->pVtbl    = &s_ctlVtbl;
    s_swapOnMsg = 0x3B;
    s_cMsg = 0; s_cMsgOther = 0;
    (void)BrUiHook85_1003E7A0(p);
    CHECK(s_cMsg      == 6);
    CHECK(s_cMsgOther == 0);

    s_ctlVtbl.f14 = MsgSend;
    s_pSwapCtl    = NULL;
    free(p);
}

/* ==========================================================================
 * PART B -- the code and geometry hooks
 * ========================================================================== */

static void TestCode(void)
{
    BrUiCtl_ *p = NewCtl();

    printf("\n0x1003E950 -- the INVERSION: flag SET gives the LOWER code\n");
    g_br0AB3D8 = 1;
    CHECK(BrUiHook85_1003E950(p) == 1);
    CHECK(p->aStepId[0] == 0x68);
    CHECK(p->w1E20C     == 0x68);
    g_br0AB3D8 = 0;
    (void)BrUiHook85_1003E950(p);
    CHECK(p->aStepId[0] == 0x69);
    CHECK(p->w1E20C     == 0x69);

    printf("0x1003EA40 -- x = 8*n + 0x4A, n picked by 0x100AB3D8\n");
    g_brB4E708 = 5;
    g_brB4E70C = 2;
    g_br0AB3D8 = 1;
    CHECK(BrUiHook85_1003EA40(p) == 1);
    CHECK(p->x == (float)(5 * 8 + 0x4A));
    g_br0AB3D8 = 0;
    (void)BrUiHook85_1003EA40(p);
    CHECK(p->x == (float)(2 * 8 + 0x4A));

    printf("0x10040930 -- 0x100AC62C[0x10AA287C], sign-extended from a BYTE\n");
    g_brAA287C = 0; (void)BrUiHook85_10040930(p); CHECK(p->w1E20C == 0x45);
    g_brAA287C = 1; (void)BrUiHook85_10040930(p); CHECK(p->w1E20C == 0x44);
    g_brAA287C = 2; (void)BrUiHook85_10040930(p); CHECK(p->w1E20C == 0x43);
    g_brAA287C = 3; (void)BrUiHook85_10040930(p); CHECK(p->w1E20C == 0x46);
    /* DEVIATION: the original is unbounded here; the port answers 0. */
    g_brAA287C = 9; (void)BrUiHook85_10040930(p); CHECK(p->w1E20C == 0);
    g_brAA287C = 0;

    printf("0x100418D0 -- does NOTHING when 0x10AA28E4 is clear\n");
    p->flags1C = (int32_t)0xFFFF;
    g_brHook81.nAA28E4 = 0;
    CHECK(BrUiHook85_100418D0(p) == 1);
    CHECK(p->flags1C == (int32_t)0xFFFF);
    g_brHook81.nAA28E4 = 1;
    (void)BrUiHook85_100418D0(p);
    CHECK(p->flags1C == (int32_t)(0xFFFFu & 0xFFFFEFEFu));
    g_brHook81.nAA28E4 = 0;

    printf("0x10042AC0 -- a ONE-SHOT latch\n");
    g_brAA28D8 = 0;
    p->aText[0].f420 = 0u;
    CHECK(BrUiHook85_10042AC0(p) == 1);
    CHECK(g_brAA28D8 == 1);
    CHECK(p->aText[0].f420 == 1u);
    /* Second call: the latch is set, so nothing moves. */
    (void)BrUiHook85_10042AC0(p);
    CHECK(p->aText[0].f420 == 1u);
    /* Released, and a NON-ZERO f420 toggles to exactly 0 (`sete`). */
    g_brAA28D8 = 0;
    p->aText[0].f420 = 0x1234u;
    (void)BrUiHook85_10042AC0(p);
    CHECK(p->aText[0].f420 == 0u);
    g_brAA28D8 = 0;

    printf("0x10042AF0 -- `mov eax,1 / ret`\n");
    CHECK(BrUiHook85_10042AF0(p) == 1);
    CHECK(BrUiHook85_10042AF0(NULL) == 1);   /* it never loads the argument */

    free(p);
}

/* ==========================================================================
 * PART C -- the list-poll hooks
 * ========================================================================== */

static void TestPoll(void)
{
    BrUiCtl_ *p = NewCtl();

    printf("\n0x1003EB10 -- the negative arm RELOADS the global\n");
    g_br0AB3F4 = 4;
    g_brAA28D8 = 1;
    s_selRet = 7; s_nSel = s_nAck = 0;
    CHECK(BrUiHook85_1003EB10(p) == 1);
    CHECK(s_nSel == 1 && s_selSent == 4);      /* the OLD value is sent    */
    CHECK(g_br0AB3F4 == 7);                    /* the new one is stored    */
    CHECK(s_nAck == 1 && s_ackSent == 7);

    /* -1 means "no answer": the global is untouched AND the acknowledge sees
     * the reloaded old value, not the -1. */
    s_selRet = -1; s_nSel = s_nAck = 0;
    (void)BrUiHook85_1003EB10(p);
    CHECK(g_br0AB3F4 == 7);
    CHECK(s_nAck == 1 && s_ackSent == 7);

    /* The acknowledge is gated on 0x10AA28D8. */
    g_brAA28D8 = 0;
    s_selRet = 2; s_nAck = 0;
    (void)BrUiHook85_1003EB10(p);
    CHECK(g_br0AB3F4 == 2);
    CHECK(s_nAck == 0);
    g_brAA28D8 = 0;

    printf("0x1003ED10 -- the same shape, no acknowledge, over 0x10AA2A2C\n");
    g_brHook85.nAA2A2C = 3;
    s_selRet = 9; s_nSel = s_nAck = 0;
    CHECK(BrUiHook85_1003ED10(p) == 1);
    CHECK(s_nSel == 1 && s_selSent == 3);
    CHECK(g_brHook85.nAA2A2C == 9);
    CHECK(s_nAck == 0);
    s_selRet = -5;
    (void)BrUiHook85_1003ED10(p);
    CHECK(g_brHook85.nAA2A2C == 9);            /* unchanged */

    printf("0x1003EE20 -- clamps what it SENDS, not what it STORES\n");
    g_br73.nAA2A34 = 5;
    s_selRet = 5;
    (void)BrUiHook85_1003EE20(p);
    CHECK(s_selSent == 5);                     /* in range, sent as-is     */
    g_br73.nAA2A34 = 0x0C;                     /* the boundary is EXCLUSIVE */
    s_selRet = 0x40;
    (void)BrUiHook85_1003EE20(p);
    CHECK(s_selSent == -1);
    CHECK(g_br73.nAA2A34 == 0x40);             /* stored UNCLAMPED         */
    g_br73.nAA2A34 = -3;
    s_selRet = -1;
    (void)BrUiHook85_1003EE20(p);
    CHECK(s_selSent == -1);
    CHECK(g_br73.nAA2A34 == -3);               /* negative answer, no store */

    /* DEVIATION: an unwired list vtable answers -1 rather than faulting. */
    p->list.pVtbl = NULL;
    g_brHook85.nAA2A2C = 11;
    CHECK(BrUiHook85_1003ED10(p) == 1);
    CHECK(g_brHook85.nAA2A2C == 11);
    p->list.pVtbl = &s_listVtbl;

    free(p);
}

/* ==========================================================================
 * PART D -- the text hooks and 0x1003EE50
 * ========================================================================== */

static void TestText(void)
{
    BrUiCtl_ *p = NewCtl();

    printf("\n0x1003F050 / 0x1003F0B0 -- apply, then copy back only on a diff\n");
    strcpy(p->aText[0].sz, "AUDIO");
    p->aText[0].f420 = 0u;              /* the early-out arm of 0x1003EE50 */
    s_nBox04 = s_nBox10 = s_nBox14 = s_n1003E070 = 0;
    CHECK(BrUiHook85_1003F050(p) == 1);
    CHECK(s_nBox04 == 1);               /* +0x04 always runs               */
    CHECK(s_nBox10 == 1);               /* +0x10 always runs               */
    CHECK(s_nBox14 == 0);               /* +0x14 does NOT, on this arm     */
    CHECK(s_n1003E070 == 0);
    CHECK(strcmp(g_brHook85.szB4E740, "AUDIO") == 0);

    /* Second call, same text: the strcmp matches and no copy happens.  The
     * apply still runs, which is what the counters show. */
    s_nBox04 = 0;
    (void)BrUiHook85_1003F050(p);
    CHECK(s_nBox04 == 1);
    CHECK(strcmp(g_brHook85.szB4E740, "AUDIO") == 0);

    /* 0x1003F0B0 is the same body over the other buffer. */
    strcpy(p->aText[0].sz, "VIDEO");
    CHECK(BrUiHook85_1003F0B0(p) == 1);
    CHECK(strcmp(g_brHook85.szB4E760, "VIDEO") == 0);
    CHECK(strcmp(g_brHook85.szB4E740, "AUDIO") == 0);   /* untouched */

    printf("0x1003EE50's confirm arm -- reached when +0x14 answers <= 0\n");
    p->aText[0].f420 = 1u;
    p->flags1C       = 0;
    g_brAA28D8       = 1;
    s_active.override = 0;
    s_box14Ret = 0;                     /* <= 0 -> take the confirm block  */
    s_nBox14 = s_n1003E070 = 0;
    strcpy(p->aText[0].sz, "X");
    (void)BrUiHook85_1003F050(p);
    CHECK(s_nBox14 == 1);
    CHECK(s_n1003E070 == 1);
    CHECK(g_brAA28D8 == 0);             /* cleared, override being 0       */
    CHECK(p->aText[0].f420 == 0u);

    /* +0x14 answers > 0 AND flag bit 1 is clear -> the block is SKIPPED. */
    p->aText[0].f420 = 1u;
    p->flags1C       = 0;
    g_brAA28D8       = 1;
    s_box14Ret = 1;
    s_n1003E070 = 0;
    (void)BrUiHook85_1003F050(p);
    CHECK(s_n1003E070 == 0);
    CHECK(g_brAA28D8 == 1);             /* untouched                       */
    CHECK(p->aText[0].f420 == 1u);

    /* ...but flag bit 1 SET forces the block even when +0x14 says yes. */
    p->flags1C = 2;
    s_n1003E070 = 0;
    (void)BrUiHook85_1003F050(p);
    CHECK(s_n1003E070 == 1);
    CHECK((p->flags1C & 2) == 0);       /* `and al,0xFD`                   */
    g_brAA28D8 = 0;

    /* The override suppresses only the three stores, never 0x1003E070. */
    p->aText[0].f420 = 1u;
    p->flags1C       = 2;
    g_brAA28D8       = 1;
    s_active.override = 1;
    s_n1003E070 = 0;
    (void)BrUiHook85_1003F050(p);
    CHECK(s_n1003E070 == 1);
    CHECK(g_brAA28D8 == 1);
    CHECK(p->aText[0].f420 == 1u);
    s_active.override = 0;
    g_brAA28D8 = 0;

    printf("0x10040A50 / 0x10040AC0 -- \"%%d\" of g + 1, then +0x08 and +0x2C\n");
    g_br73.nAA28A0 = 41;
    g_br73.nAA28A4 = 6;
    p->aText[0].sz[0] = '\0';
    s_nBox08 = s_nBox2C = 0;
    CHECK(BrUiHook85_10040A50(p) == 1);
    CHECK(strcmp(g_br73.szAA2518, "42") == 0);
    CHECK(strcmp(p->aText[0].sz, "42") == 0);
    CHECK(s_nBox08 == 1 && s_nBox2C == 1);

    CHECK(BrUiHook85_10040AC0(p) == 1);
    CHECK(strcmp(g_br73.szA9D618, "7") == 0);
    CHECK(strcmp(p->aText[0].sz, "7") == 0);

    free(p);
}

/* ==========================================================================
 * PART E -- the phase-changing hooks and the six one-liners
 * ========================================================================== */

static void TestPhase(void)
{
    BrUiCtl_ *p = NewCtl();
    BrPhase_  owner, root, cur, back;

    memset(&owner, 0, sizeof owner);
    memset(&root,  0, sizeof root);
    memset(&cur,   0, sizeof cur);
    memset(&back,  0, sizeof back);
    owner.pVtbl = &s_phaseVtbl;
    cur.pVtbl   = &s_phaseVtbl;
    p->pOwner   = &owner;

    printf("\n0x10043FA0 -- owner vtable +0x18 with 1, then go to the root\n");
    s_nav.pAA2904 = &cur;
    s_nav.pAA2908 = &root;
    s_nPh18 = 0;
    CHECK(BrUiHook85_10043FA0(p) == 0);          /* 0, which stops the frame */
    CHECK(s_nPh18 == 1);
    CHECK(s_ph18Arg == (void *)(size_t)1);       /* CONFLICT 4 */
    CHECK(s_nav.pAA2904 == &root);

    printf("0x100466C0 -- release, notify, then 0x10AA2904 <- 0x10AA2918\n");
    s_nav.pAA2904 = &cur;
    g_brHook81.pAA2918  = &back;
    g_brHook85.nAA2984  = 7;
    s_nPh1C = s_nPh00 = s_n1003E310 = s_n1006A4A0 = 0;
    CHECK(BrUiHook85_100466C0(p) == 0);
    CHECK(s_nPh1C == 1);                         /* owner +0x1C, first      */
    CHECK(s_nPh00 == 1 && s_ph00Flags == 1);     /* the CURRENT phase, +0x00 */
    CHECK(g_brHook85.nAA2984 == 0);
    CHECK(s_nav.pAA2904 == &back);
    CHECK(s_n1003E310 == 1 && s_n1006A4A0 == 1);
    CHECK(s_pA4A0This == &s_b4df30 && s_pA4A0Arg == &s_b4fbe8);

    /* The destination is read BEFORE the clear, so a NULL 0x10AA2918 is
     * carried through rather than replaced by anything. */
    s_nav.pAA2904 = &cur;
    g_brHook81.pAA2918 = NULL;
    (void)BrUiHook85_100466C0(p);
    CHECK(s_nav.pAA2904 == NULL);

    printf("0x10044010 .. 0x100440B0 -- mode, then one of two tail calls\n");
    s_nOpen2948 = s_nKind = 0;
    g_brAA287C = 0x7F;
    CHECK(BrUiHook85_10044010(p) == 1);
    CHECK(g_brAA287C == 0 && s_nOpen2948 == 1 && s_nKind == 0);
    CHECK(BrUiHook85_10044050(p) == 1);
    CHECK(g_brAA287C == 1 && s_nOpen2948 == 2);
    CHECK(BrUiHook85_10044090(p) == 1);
    CHECK(g_brAA287C == 2 && s_nOpen2948 == 3);
    CHECK(s_nKind == 0);

    /* The +0x0C members take the OTHER tail call, and they DO pass the
     * control -- which is the whole difference CONFLICT 5 warns about. */
    s_pKindArg = NULL;
    CHECK(BrUiHook85_10044030(p) == 1);
    CHECK(g_brAA287C == 0 && s_nKind == 1 && s_pKindArg == p);
    CHECK(BrUiHook85_10044070(p) == 1);
    CHECK(g_brAA287C == 1 && s_nKind == 2);
    CHECK(BrUiHook85_100440B0(p) == 1);
    CHECK(g_brAA287C == 2 && s_nKind == 3);
    CHECK(s_nOpen2948 == 3);                     /* still three */

    printf("0x1004E810 -- CD track *pRow + 2\n");
    {
        int32_t row = 5;
        s_nCdPlay = 0;
        CHECK(BrUiHook85_1004E810(NULL, &row) == 1);
        CHECK(s_nCdPlay == 1 && s_nCdTrack == 7);
    }

    free(p);
}

/* ==========================================================================
 * PART F -- installation
 * ========================================================================== */

static void TestInstall(void)
{
    BrUi73Hooks h;

    printf("\nBrUiHook85Install -- 25 slots, and nothing else\n");
    memset(&h, 0, sizeof h);
    BrUiHook85Install(&h);

    CHECK(h.p1003E7A0 == BrUiHook85_1003E7A0);
    CHECK(h.p1003E950 == BrUiHook85_1003E950);
    CHECK(h.p1003E980 == BrUiHook85_1003E980);
    CHECK(h.p1003E9E0 == BrUiHook85_1003E9E0);
    CHECK(h.p1003EA40 == BrUiHook85_1003EA40);
    CHECK(h.p1003EB10 == BrUiHook85_1003EB10);
    CHECK(h.p1003ED10 == BrUiHook85_1003ED10);
    CHECK(h.p1003EE20 == BrUiHook85_1003EE20);
    CHECK(h.p1003F050 == BrUiHook85_1003F050);
    CHECK(h.p1003F0B0 == BrUiHook85_1003F0B0);
    CHECK(h.p10040930 == BrUiHook85_10040930);
    CHECK(h.p10040A50 == BrUiHook85_10040A50);
    CHECK(h.p10040AC0 == BrUiHook85_10040AC0);
    CHECK(h.p100418D0 == BrUiHook85_100418D0);
    CHECK(h.p10042AC0 == BrUiHook85_10042AC0);
    CHECK(h.p10042AF0 == BrUiHook85_10042AF0);
    CHECK(h.p10043FA0 == BrUiHook85_10043FA0);
    CHECK(h.p10044010 == BrUiHook85_10044010);
    CHECK(h.p10044030 == BrUiHook85_10044030);
    CHECK(h.p10044050 == BrUiHook85_10044050);
    CHECK(h.p10044070 == BrUiHook85_10044070);
    CHECK(h.p10044090 == BrUiHook85_10044090);
    CHECK(h.p100440B0 == BrUiHook85_100440B0);
    CHECK(h.p100466C0 == BrUiHook85_100466C0);
    CHECK(h.p1004E810 != NULL);

    /* The slots this module must NOT claim: two are slice7_80.c's, four are
     * slice7_81.c's, two are the host's, and the rest are the holes
     * slice8_85.h's NOT PORTED section names.  A future pass that fills one of
     * them here without updating that section trips this. */
    CHECK(h.p10042CF0 == NULL);   /* slice7_80.c   */
    CHECK(h.p10042D60 == NULL);   /* slice7_80.c   */
    CHECK(h.p10045AA0 == NULL);   /* slice7_81.c   */
    CHECK(h.p100463C0 == NULL);   /* slice7_81.c   */
    CHECK(h.p10046620 == NULL);   /* slice7_81.c   */
    CHECK(h.p100470E0 == NULL);   /* slice7_81.c   */
    CHECK(h.p10045AF0 == NULL);   /* br_uinav.c    */
    CHECK(h.p10047360 == NULL);   /* br_sprfont.c  */
    CHECK(h.p1003ECB0 == NULL);   /* not ported    */
    CHECK(h.p1003FC40 == NULL);
    CHECK(h.p10041300 == NULL);
    CHECK(h.p100413B0 == NULL);
    CHECK(h.p10041670 == NULL);
    CHECK(h.p10041710 == NULL);
    CHECK(h.p100417B0 == NULL);
    CHECK(h.p10041DF0 == NULL);
    CHECK(h.p10042020 == NULL);
    CHECK(h.p10047210 == NULL);
    CHECK(h.p10047290 == NULL);
    CHECK(h.p100409F0 == NULL);   /* PAGE hook, aliased storage -- see .h */
    CHECK(h.p10040A20 == NULL);

    /* A NULL argument is a no-op, as in BrUiOptInstall73 / BrUiHook81Install. */
    BrUiHook85Install(NULL);
}

int main(void)
{
    Wire();

    TestDraw();
    TestCode();
    TestPoll();
    TestText();
    TestPhase();
    TestInstall();

    printf("\ntest_slice8_85: %s (%d failure%s)\n",
           g_fails ? "FAIL" : "ok", g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
