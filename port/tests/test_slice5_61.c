/* test_slice5_61.c -- behavioural tests for the pass-61 packet.
 *
 * Everything below asserts a PROPERTY of the original (a clamp, a wrap, an
 * asymmetry, a fixed-point scale) rather than a value this port happened to
 * produce. Where the original's own numbers are used they come out of
 * orig/BRD3D.dll, not out of slice5_61.c.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file and nowhere
 * else, as the contract requires.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "slice5_61.h"
#include "slice1_03.h"
#include "slice2_15.h"
#include "slice2_25.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS for cross-slice symbols. Test scaffolding only.
 * ========================================================================== */

/* --- slice1_03: the text state ------------------------------------------- */
static BrTextState s_text;
BrTextState *BrTextGetState(void) { return &s_text; }

/* --- slice2_15: the RDP register block ------------------------------------ */
static BrRdpRegs s_regs;
BrRdpRegs *BrRdpGetRegs(void) { return &s_regs; }

/* --- slice2_15: the screen info ------------------------------------------
 * `cy` is 0x100A81C4 in BRD3D and 0x100A7518 in BRGlide (paired in
 * config/globals_shared.csv at 9 votes).  BrGbiCall10024260 reads it, because
 * the Glide build reflects the viewport's Y translate through the screen
 * height instead of negating the Y scale. */
static BrScreenInfo s_screen;
BrScreenInfo *BrScreenGet(void) { return &s_screen; }

/* --- slice5_61's own externs ---------------------------------------------- */
int32_t        g_br0AB3F4;
unsigned char *g_brPAA29D0;
char           g_aBrA9D018[256];
char           g_aBrA9D078[256];
/* One dword, not two bytes -- g_brAA26F4/F5 are spellings of [0] and [1].
 * See the ALIAS RESOLVED note in slice5_61.h. */
uint8_t        g_aBrAA26F4[4];

/* 0x100B3820, first four pairs as they appear in the DLL image. */
const uint8_t g_aBr0B3820[8] = { 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* --- slice2_25: option globals -------------------------------------------- */
BrDPlay *g_brP277B40;
int32_t  g_br0AC648, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC658, g_br0AC65C;
int32_t  g_br0BD3E0, g_brAA2A00, g_brAA2A08, g_brAA2A18;
int32_t  g_br094350, g_br094354, g_br094358, g_br09435C;
int32_t  g_br0B380C, g_br22B34C, g_br22B350;
int32_t  g_br0AA010, g_brAA28FC, g_brAA28D8;
char     g_aBr39B720[BR_OPT_TEXT_MAX];

/* The index tables, read out of orig/BRD3D.dll .data with tools/pe.py. */
const int32_t g_aBrAC420[32] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};
const int32_t g_aBrAC4A0[4]  = { 0, 1, 2, 0 };
const int32_t g_aBrAC4B0[4]  = { 0, 1, 2, 0 };
const int32_t g_aBrAC4C0[6]  = { 0, 1, 2, 3, 4, 0 };
const int32_t g_aBrAC4D8[16] = { 0, 1, 2, 3, 4, 5, 6, 7,
                                 8, 9, 10, 11, 12, 13, 14, 0 };
/* slice2_25.h pins this one at TWO entries (0x100AC518..0x100AC520). */
const int32_t g_aBrAC518[2]  = { 1, 0 };

/* --- availability predicates ---------------------------------------------- */
/* Bit i of these masks says "index i is selectable". */
static uint32_t s_availTrack;   /* 0x1003F320 */
static uint32_t s_availCar;     /* 0x1003F2B0 */
static int      s_nProbeTrack, s_nProbeCar;

int BrSub1003F320(int index)
{
    ++s_nProbeTrack;
    if (index < 0 || index > 31) return 0;
    return (int)((s_availTrack >> index) & 1u);
}

int BrSub1003F2B0(int index)
{
    ++s_nProbeCar;
    if (index < 0 || index > 31) return 0;
    return (int)((s_availCar >> index) & 1u);
}

/* --- plain callees --------------------------------------------------------- */
static int s_n44540, s_n5FCF0, s_n3E3A0;
void BrSub10044540(void) { ++s_n44540; }
void BrSub1005FCF0(void) { ++s_n5FCF0; }
void BrSub1003E3A0(void) { ++s_n3E3A0; }

/* --- the Global* platform hooks ------------------------------------------- */
static int   s_nHandle, s_nUnlock, s_nFree;
static void *s_pLastFreed;
void *BrGlobalHandle(void *pMem) { ++s_nHandle; return pMem; }
int   BrGlobalUnlock(void *hMem) { ++s_nUnlock; (void)hMem; return 1; }
void *BrGlobalFree(void *hMem)   { ++s_nFree; s_pLastFreed = hMem; return NULL; }

/* --- 0x1003D0B0 ------------------------------------------------------------ */
/* Layout mirrors the file-local Br61DPSessionDesc2 in slice5_61.c. */
typedef struct TestDesc {
    uint32_t      dwSize;
    uint32_t      dwFlags;
    unsigned char aGuids[0x20];
    uint32_t      dwMaxPlayers;
    uint32_t      dwCurrentPlayers;
    char         *lpszSessionNameA;
    char         *lpszPasswordA;
    uint32_t      dwReserved1;
    uint32_t      dwReserved2;
    int32_t       dwUser1, dwUser2, dwUser3, dwUser4;
} TestDesc;

static TestDesc s_desc;
static int32_t  s_hr;
static int      s_writeOut;          /* does the stand-in fill *ppvOut? */

static int32_t TestGetSessionDesc(void *pObj, void **ppvOut)
{
    (void)pObj;
    if (s_writeOut)
        *ppvOut = &s_desc;
    return s_hr;
}

/* ==========================================================================
 * 0x10019290
 * ========================================================================== */

static void test_19290(void)
{
    s_text.align = (signed char)BR_TEXT_ALIGN_CENTER;
    BrSub_10019290();
    CHECK(s_text.align == BR_TEXT_ALIGN_RIGHT);

    /* It is an unconditional store, so it is idempotent from any state. */
    BrSub_10019290();
    CHECK(s_text.align == BR_TEXT_ALIGN_RIGHT);

    s_text.align = (signed char)BR_TEXT_ALIGN_LEFT;
    BrSub_10019290();
    CHECK(s_text.align == BR_TEXT_ALIGN_RIGHT);
}

/* ==========================================================================
 * 0x10024260  viewport
 * ========================================================================== */

static void PutVp(uint8_t *pBuf, int sx, int sy, int sz, int sw,
                  int tx, int ty, int tz, int tw)
{
    const int a[8] = { sx, sy, sz, sw, tx, ty, tz, tw };
    int i;
    for (i = 0; i < 8; ++i) {
        pBuf[i * 2 + 0] = (uint8_t)((unsigned)a[i] & 0xFFu);
        pBuf[i * 2 + 1] = (uint8_t)(((unsigned)a[i] >> 8) & 0xFFu);
    }
}

/* The 64-bit-host stand-in for the w1 resolver: one slot, one buffer. */
static uint8_t s_vp[16];
static const void *DerefVp(uint32_t w1)
{
    CHECK(w1 == 0x0BADF00Du);      /* the word really does reach the hook */
    return s_vp;
}

static void test_24260(void)
{
    uint8_t    *vp = s_vp;
    BrGfxWords  cmd[2];
    BrGfxWords *pNext;

    g_brPfnDerefW1 = DerefVp;

    memset(cmd, 0, sizeof(cmd));
    cmd[0].w0 = 0x00800000u;
    cmd[0].w1 = 0x0BADF00Du;

    /* THESE ASSERTIONS USED TO PIN THE D3D READING and passed, because the
     * port had transcribed BRD3D 0x10024260.  The reference build is BRGlide
     * 0x10023920, which flips Y the other way round:
     *
     *     Y SCALE      Glide 0x1002396A `fmul [0x1007740C]` == +0.25f;
     *                  D3D   0x1002429D `fmul [0x1008F3F0]` == -0.25f.
     *     Y TRANSLATE  Glide 0x100239B0 `fsubr dword [esp]` where [esp] holds
     *                  `fild [0x100A7518]` from 0x10023925, i.e.
     *                  screenHeight - 0.25*v; D3D has no such preamble.
     *
     * The old comment even said "with Y NEGATED on the scale only", quoting
     * slice2_15.h -- a header note derived from the same wrong build, so the
     * "independent" corroboration was the same reading seen twice. */
    s_screen.cy = 480;                       /* 0x100A7518 in the shipped DLL */

    /* An N64 viewport for a 320x240 screen inside a 640x480 frame: scale and
     * translate both (cx*2, cy*2) in 2.2 fixed point. */
    PutVp(vp, 640, 480, 0x1FF, 0, 640, 480, 0x1FF, 0);
    pNext = BrGbiCall10024260(&cmd[0]);

    CHECK(pNext == &cmd[1]);                 /* `add eax, 8` */
    CHECK(s_regs.f4BBF08 == 160.0f);         /* cx / 2 */
    CHECK(g_br4BC198     == 120.0f);         /* cy / 2, NOT negated */
    CHECK(s_regs.f4C0BB0 == 160.0f);
    CHECK(s_regs.f4C0BB8 == 360.0f);         /* 480 - 120 */

    /* THE SCREEN HEIGHT IS A LIVE INPUT, and only to the Y translate. The
     * same payload under a different height must move f4C0BB8 by exactly the
     * difference and leave the other three alone -- which is what separates
     * `H - 0.25*v` from both `0.25*v` and `-0.25*v`. */
    s_screen.cy = 200;
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == 160.0f);
    CHECK(g_br4BC198     == 120.0f);
    CHECK(s_regs.f4C0BB0 == 160.0f);
    CHECK(s_regs.f4C0BB8 ==  80.0f);         /* 200 - 120 */
    s_screen.cy = 480;

    /* z and w of both vectors are ignored: changing only those must not
     * change any output. */
    PutVp(vp, 640, 480, 0x7FFF, 0x7FFF, 640, 480, 0x7FFF, 0x7FFF);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == 160.0f);
    CHECK(g_br4BC198     == 120.0f);
    CHECK(s_regs.f4C0BB0 == 160.0f);
    CHECK(s_regs.f4C0BB8 == 360.0f);

    /* The shorts are SIGN-extended, not zero-extended: 0xFFFF is -1. */
    PutVp(vp, -1, -1, 0, 0, -1, -1, 0, 0);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == -0.25f);
    CHECK(g_br4BC198     == -0.25f);         /* +0.25 * -1, not -0.25 * -1 */
    CHECK(s_regs.f4C0BB0 == -0.25f);
    CHECK(s_regs.f4C0BB8 == 480.25f);        /* 480 - (0.25 * -1) */

    /* ALL FOUR MULTIPLIES CARRY THE SAME SIGN in this build. Feeding equal
     * magnitudes must give the SAME value on the scale pair -- the D3D body
     * gives opposite ones, so this single line separates the two builds. */
    PutVp(vp, 100, 100, 0, 0, 100, 100, 0, 0);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == g_br4BC198);
    /* ...and the translate pair does NOT match, because only Y is reflected. */
    CHECK(s_regs.f4C0BB0 == 25.0f);
    CHECK(s_regs.f4C0BB8 == 455.0f);         /* 480 - 25 */

    /* `fsubr` is `mem - ST0`, not `ST0 - mem`. With the screen height at zero
     * the translate must come out as the NEGATION of the scaled value; the
     * reversed operand order would give the value itself. */
    s_screen.cy = 0;
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4C0BB8 == -25.0f);
    s_screen.cy = 480;

    /* 2.2 fixed point: the four scale steps between successive integers must
     * be exactly 0.25 apart, with no rounding. */
    PutVp(vp, 1, 0, 0, 0, 0, 0, 0, 0);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == 0.25f);
    PutVp(vp, 5, 0, 0, 0, 0, 0, 0, 0);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 == 1.25f);

    /* Extremes of int16 stay exact in a float. */
    PutVp(vp, 32767, -32768, 0, 0, 32767, -32768, 0, 0);
    (void)BrGbiCall10024260(&cmd[0]);
    CHECK(s_regs.f4BBF08 ==  32767.0f * 0.25f);
    CHECK(g_br4BC198     == -32768.0f * 0.25f);
    CHECK(s_regs.f4C0BB0 ==  32767.0f * 0.25f);
    CHECK(s_regs.f4C0BB8 == 480.0f - (-32768.0f * 0.25f));
}

/* ==========================================================================
 * 0x10042AF0
 * ========================================================================== */

static void test_42AF0(void)
{
    /* The body is `mov eax,1 / ret`: the argument must not be dereferenced,
     * so passing NULL has to be safe. */
    BrGfx42AF0_1(NULL);
    CHECK(1);
}

/* ==========================================================================
 * 0x10060E90 -> 0x10078C10
 * ========================================================================== */

#define TIME_STEP 1562500u

static void test_timenow(void)
{
    uint64_t *pCtr = BrTimeNowCounter();
    int32_t   a, b;
    int       i;

    *pCtr = 0;
    a = BrTimeNow();
    CHECK((uint32_t)a == TIME_STEP);             /* first call, from .bss 0 */

    /* Successive calls advance by exactly one step, in 32-bit arithmetic. */
    b = BrTimeNow();
    CHECK((uint32_t)b - (uint32_t)a == TIME_STEP);

    /* The n-th call is n * step, modulo 2^32. */
    *pCtr = 0;
    for (i = 1; i <= 100; ++i) {
        int32_t v = BrTimeNow();
        CHECK((uint32_t)v == (uint32_t)((uint64_t)i * TIME_STEP));
    }

    /* The low dword is returned SIGNED, so the result goes negative once the
     * counter passes 2^31 -- and it must still be monotonic modulo 2^32. */
    *pCtr = 0x7FFFFFFFu;
    a = BrTimeNow();
    CHECK(a < 0);
    CHECK((uint32_t)a == 0x7FFFFFFFu + TIME_STEP);

    /* And it wraps through zero without losing a step. */
    *pCtr = 0xFFFFFFF0u;
    a = BrTimeNow();
    CHECK((uint32_t)a - 0xFFFFFFF0u == TIME_STEP);
    /* The 64-bit accumulator keeps counting even though only 32 bits show. */
    CHECK(*pCtr > 0xFFFFFFFFu);
}

/* ==========================================================================
 * 0x10042410
 * ========================================================================== */

static unsigned char s_recs[0x4000];
static unsigned char s_sub[0x100];
static BrGameObj     s_obj;

static unsigned char *Rec(int n)
{
    return s_recs + (size_t)n * BR61_REC29D0_STRIDE;
}

static int32_t RecFlag(int n)
{
    int32_t v;
    memcpy(&v, Rec(n) + BR61_REC29D0_OFF_FLAG, sizeof(v));
    return v;
}

static void SetRecFlag(int n, int32_t v)
{
    memcpy(Rec(n) + BR61_REC29D0_OFF_FLAG, &v, sizeof(v));
}

static char *RecName(int n)
{
    return (char *)Rec(n) + BR61_REC29D0_OFF_NAME;
}

static void test_42410(void)
{
    int32_t rv;

    memset(s_recs, 0, sizeof(s_recs));
    memset(s_sub, 0xAB, sizeof(s_sub));
    s_obj.pSub    = (BrGameSub *)s_sub;
    g_brPAA29D0   = s_recs;

    /* --- old flag zero: the flag becomes 1 and the names are swapped ------ */
    g_br0AB3F4 = 2;
    SetRecFlag(2, 0);
    strcpy(RecName(2), "OLD-NAME");
    strcpy(g_aBr39B720, "EDITED");
    memset(g_aBrA9D078, 0, sizeof(g_aBrA9D078));
    g_brAA28D8 = 0x5A;

    rv = BrExt_10042410(&s_obj);

    CHECK(rv == 1);                              /* always 1 */
    CHECK(RecFlag(2) == 1);
    CHECK(g_brAA28D8 == 1);                      /* re-read, so 0 or 1 only */
    CHECK(strcmp(g_aBrA9D078, "OLD-NAME") == 0); /* saved */
    CHECK(strcmp(RecName(2), "EDITED") == 0);    /* installed */

    /* +0x70 of the sub-object is zeroed; the bytes around it are not. */
    CHECK(s_sub[0x70] == 0 && s_sub[0x71] == 0 &&
          s_sub[0x72] == 0 && s_sub[0x73] == 0);
    CHECK(s_sub[0x6F] == 0xAB && s_sub[0x74] == 0xAB);

    /* --- old flag non-zero: the flag becomes 0 and nothing else moves ----- */
    g_br0AB3F4 = 3;
    SetRecFlag(3, 7);
    strcpy(RecName(3), "KEEP");
    strcpy(g_aBr39B720, "SHOULD-NOT-APPEAR");
    strcpy(g_aBrA9D078, "UNTOUCHED");

    rv = BrExt_10042410(&s_obj);

    CHECK(rv == 1);
    CHECK(RecFlag(3) == 0);
    CHECK(g_brAA28D8 == 0);
    CHECK(strcmp(RecName(3), "KEEP") == 0);
    CHECK(strcmp(g_aBrA9D078, "UNTOUCHED") == 0);

    /* --- calling twice on the same record TOGGLES it back ---------------- */
    g_br0AB3F4 = 4;
    SetRecFlag(4, 0);
    strcpy(RecName(4), "A");
    strcpy(g_aBr39B720, "B");
    (void)BrExt_10042410(&s_obj);
    CHECK(RecFlag(4) == 1);
    CHECK(strcmp(RecName(4), "B") == 0);
    (void)BrExt_10042410(&s_obj);
    CHECK(RecFlag(4) == 0);
    CHECK(strcmp(RecName(4), "B") == 0);   /* second pass leaves it alone */

    /* --- the index really scales by 0x438 -------------------------------- */
    g_br0AB3F4 = 5;
    SetRecFlag(5, 0);
    SetRecFlag(6, 0);
    (void)BrExt_10042410(&s_obj);
    CHECK(RecFlag(5) == 1);
    CHECK(RecFlag(6) == 0);   /* the neighbour is not disturbed */
}

/* ==========================================================================
 * 0x1003E510
 * ========================================================================== */

static void ResetCounters(void)
{
    s_n44540 = s_n5FCF0 = s_n3E3A0 = 0;
    s_nProbeTrack = s_nProbeCar = 0;
    s_nHandle = s_nUnlock = s_nFree = 0;
    s_pLastFreed = NULL;
}

static void test_3E510(void)
{
    /* --- the unconditional part ------------------------------------------ */
    ResetCounters();
    g_br0AC65C = 9;
    g_br0AA010 = 1;            /* not 6 */
    g_br0AC654 = 3;  s_availTrack = 0xFFFFFFFFu;
    g_br0AC648 = 2;  s_availCar   = 0xFFFFFFFFu;
    g_br0AC64C = 1;  g_br0AC650 = 2;  g_brAA2A08 = 1;  g_brAA2A00 = 3;
    g_br0AC658 = 42;

    BrSub1003E510();

    CHECK(s_n3E3A0 == 1);                  /* 0x1003E3A0 always runs first */
    CHECK(g_br094350 == 9);                /* = 0x100AC65C                 */
    CHECK(s_n44540 == 0);                  /* only when 0x100AA010 == 6    */
    CHECK(s_n5FCF0 == 1);                  /* 0x1005FCF0 always runs last  */

    CHECK(g_br22B34C == g_aBrAC420[3]);
    CHECK(g_br09435C == g_aBrAC4A0[1]);
    CHECK(g_br094358 == g_aBrAC4B0[2]);
    CHECK(g_br094354 == g_aBrAC518[1]);

    CHECK(g_br0AC654 == 3);                /* already selectable, untouched */
    CHECK(g_br0AC648 == 2);
    CHECK(g_br0B380C == g_aBrAC4D8[2]);
    CHECK(g_br0BD3E0 == 42);               /* straight from 0x100AC658      */
    CHECK(g_br22B350 == g_aBrAC4C0[3]);

    /* --- 0x100AA010 == 6 additionally calls 0x10044540 ------------------- */
    ResetCounters();
    g_br0AA010 = 6;
    BrSub1003E510();
    CHECK(s_n44540 == 1);

    /* --- 0x100AA010 == 0 takes the byte-pair branch instead -------------- */
    ResetCounters();
    g_br0AA010 = 0;
    g_brAA26F4 = 0;
    g_brAA26F5 = 1;            /* index 1 -> the pair (0x04, 0x00) */
    g_br0B380C = 0x77;
    g_br22B350 = 0x77;
    g_br0AC648 = 2;
    BrSub1003E510();
    CHECK(g_br0B380C == g_aBr0B3820[2]);
    CHECK(g_br22B350 == g_aBr0B3820[3]);
    CHECK(s_nProbeCar == 0);               /* the car sweep is not reached  */
    CHECK(g_br0AC648 == 2);
    CHECK(s_n5FCF0 == 1);

    /* The pair index is byte1 + 12*byte0, so byte0 dominates. */
    ResetCounters();
    g_brAA26F4 = 0;
    g_brAA26F5 = 0;
    BrSub1003E510();
    CHECK(g_br0B380C == g_aBr0B3820[0]);
    CHECK(g_br22B350 == g_aBr0B3820[1]);

    /* --- the track sweep skips unselectable indices and wraps ------------ */
    ResetCounters();
    g_br0AA010 = 1;
    g_br0AC654 = 30;
    s_availTrack = (1u << 5);              /* only index 5 is selectable */
    s_availCar   = 0xFFFFFFFFu;
    g_br0AC648   = 0;
    BrSub1003E510();
    CHECK(g_br0AC654 == 5);                /* 30,31 -> wrap to 0 -> ... -> 5 */

    /* Nothing selectable: the index comes back to where it started and no
     * failure is reported. */
    ResetCounters();
    g_br0AC654 = 12;
    s_availTrack = 0;
    BrSub1003E510();
    CHECK(g_br0AC654 == 12);

    /* --- the car sweep's bound depends on 0x10AA28FC --------------------- */
    ResetCounters();
    s_availTrack = 0xFFFFFFFFu;
    g_br0AC654   = 0;
    g_brAA28FC   = 0;                      /* bound 11 */
    g_br0AC648   = 11;
    s_availCar   = (1u << 3) | (1u << 12) | (1u << 14);
    BrSub1003E510();
    CHECK(g_br0AC648 == 3);                /* 12 and 14 are out of bounds */

    ResetCounters();
    g_brAA28FC = 1;                        /* bound 14 */
    g_br0AC648 = 11;
    s_availCar = (1u << 3) | (1u << 12) | (1u << 14);
    BrSub1003E510();
    CHECK(g_br0AC648 == 12);               /* now 12 is reachable */
}

/* ==========================================================================
 * 0x1003CE80
 * ========================================================================== */

static BrDPlay s_dplay;

static void PrimeGlobals(void)
{
    g_br0AC648 = g_br0B380C = -1;
    g_brAA2A00 = g_br22B350 = -1;
    g_brAA2A18 = -1;
    g_br0BD3E0 = g_br0AC658 = -1;
    memset(g_aBrA9D018, 0, sizeof(g_aBrA9D018));
}

static void test_3CE80(void)
{
    static char szName[] = "HOST SESSION";

    g_brPfn1003D0B0 = TestGetSessionDesc;
    s_availTrack    = 0xFFFFFFFFu;

    /* --- no DirectPlay object: a pure early-out ------------------------- */
    ResetCounters();
    PrimeGlobals();
    g_brP277B40 = NULL;
    s_writeOut  = 1;
    s_hr        = 0;
    BrSub1003CE80();
    CHECK(g_br0AC648 == -1);               /* nothing written */
    CHECK(s_nFree == 0 && s_nUnlock == 0); /* nothing released */
    CHECK(s_n44540 == 0);

    /* --- failure that wrote nothing: no release ------------------------- */
    ResetCounters();
    PrimeGlobals();
    g_brP277B40 = &s_dplay;
    s_writeOut  = 0;
    s_hr        = (int32_t)0x80004005;     /* any negative HRESULT */
    BrSub1003CE80();
    CHECK(s_nFree == 0 && s_nUnlock == 0);
    CHECK(g_br0AC648 == -1);
    CHECK(s_n44540 == 0);

    /* --- failure that DID write: the buffer is unlocked then freed ------ */
    ResetCounters();
    PrimeGlobals();
    s_writeOut = 1;
    s_hr       = (int32_t)0x80004005;
    BrSub1003CE80();
    CHECK(s_nUnlock == 1 && s_nFree == 1);
    CHECK(s_pLastFreed == (void *)&s_desc);
    CHECK(s_nHandle == 2);                 /* GlobalHandle before each */
    CHECK(g_br0AC648 == -1);               /* still no globals written */

    /* --- success: the four dwUser fields fan out into six globals ------- */
    ResetCounters();
    PrimeGlobals();
    s_writeOut = 1;
    s_hr       = 0;
    g_br0AC654 = 4;
    s_desc.dwUser1 = 11;
    s_desc.dwUser2 = 22;
    s_desc.dwUser3 = 33;
    s_desc.dwUser4 = 44;
    s_desc.lpszSessionNameA = szName;

    BrSub1003CE80();

    CHECK(g_br0AC648 == 11 && g_br0B380C == 11);
    CHECK(g_brAA2A00 == 22 && g_br22B350 == 22);
    CHECK(g_brAA2A18 == 33);
    CHECK(g_br0BD3E0 == 44 && g_br0AC658 == 44);
    CHECK(s_n44540 == 1);
    CHECK(strcmp(g_aBrA9D018, "HOST SESSION") == 0);
    CHECK(s_nUnlock == 1 && s_nFree == 1);

    /* --- success with a NULL name: the copy is skipped, the free is not - */
    ResetCounters();
    PrimeGlobals();
    strcpy(g_aBrA9D018, "PREVIOUS");
    s_desc.lpszSessionNameA = NULL;
    BrSub1003CE80();
    CHECK(strcmp(g_aBrA9D018, "PREVIOUS") == 0);
    CHECK(s_nUnlock == 1 && s_nFree == 1);

    /* --- the track sweep runs on the success path too ------------------- */
    ResetCounters();
    PrimeGlobals();
    g_br0AC654   = 30;
    s_availTrack = (1u << 7);
    s_desc.lpszSessionNameA = szName;
    BrSub1003CE80();
    CHECK(g_br0AC654 == 7);
}

/* ========================================================================== */

int main(void)
{
    test_19290();
    test_24260();
    test_42AF0();
    test_timenow();
    test_42410();
    test_3E510();
    test_3CE80();

    if (g_fails == 0)
        printf("slice5_61: all checks passed\n");
    else
        printf("slice5_61: %d FAILURE(S)\n", g_fails);
    return g_fails != 0;
}
