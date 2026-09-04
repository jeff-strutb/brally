/* br_port_shims.c -- port-only definitions for symbols the decomp keeps
 * inside `#ifdef BR_MATCHING_BUILD` with no port arm.
 *
 * As functions were matched, their bodies were wrapped in matching-build-only
 * guards.  That is correct for the byte-exact build, but the port link then
 * loses them.  This file supplies port-usable definitions so the game LINKS
 * and RUNS.  It touches no decomp source.
 *
 * THREE CLASSES:
 *   1. Pure, portable functions -- copied verbatim from the guarded decomp
 *      (math / fixed-point unpackers).  These are exact.
 *   2. Not-yet-ported functions -- real stubs, so the boot path links.  Any
 *      that the running game actually reaches will show up as wrong behaviour
 *      and get ported for real.
 *   3. Data globals the decomp only declares `extern`.  Storage with sane
 *      defaults; divisors are non-zero.  Two real data TABLES (g_BrDlTableA,
 *      g_hudSpriteTable) are placeholders -- correct SIZE unknown, correct
 *      CONTENTS need extraction from the binary; rendering that reads them
 *      will be wrong until then, but it links and boots.
 */
#include <stdint.h>
#include <math.h>

/* ====================================================================== */
/* 1. Pure functions -- exact copies of the guarded decomp bodies         */
/* ====================================================================== */

/* -- net/br_fix.c : the ten fixed-point unpackers -- */
float BrFixUnpackS6Q7Neg(int32_t v)
{
    unsigned char b = (unsigned char)v;
    int32_t       s;
    if (b & 0x20) b |= 0xC0u; else b &= 0x3Fu;
    s = (signed char)b;
    return (float)(s * -0.0078125f);
}
float BrFixUnpackS16Q15Neg(int32_t v)
{
    return (float)((int16_t)v * -0.000030517578125f);
}
float BrFixUnpackU8Angle(int32_t v)
{
    return (float)((v & 0xFF) * 1.41015625f);
}
float BrFixUnpackU8Range(int32_t v)
{
    return 400.0f - (v & 0xFF) * -120.63491821289062f;
}
float BrFixUnpackLevel(int32_t v)
{
    unsigned char b = (unsigned char)v;
    if (b == 0) return 0.0f;
    if (b == 1) return 170.0f;
    if (b == 2) return 212.0f;
    return 255.0f;
}
float BrFixUnpackU32Q13(uint32_t v)
{
    return (float)(v * 0.0001220703125f);
}
float BrFixUnpackS24Q1(uint32_t v)
{
    int32_t x = (int32_t)v;
    if (x & 0x800000) return (float)((x | ~0xFFFFFF) * 0.5f);
    return (float)((x & 0xFFFFFF) * 0.5f);
}
float BrFixUnpackS16Q7(int32_t v)
{
    return (float)((int16_t)v * 0.0078125f);
}
float BrFixUnpackS16Q8(int32_t v)
{
    return (float)((int16_t)v * 0.00390625f);
}
float BrFixUnpackS8Q3(int32_t v)
{
    return (float)((int8_t)v * 0.125f);
}

/* -- geometry/br_sin.c : the x87 one-liners -- */
float BrSinF(float x)  { return (float)sin(x); }
float BrSqrtF(float x) { return (float)sqrt(x); }

/* -- geometry/br_mat3.c : 3x3 matrix subtract, all nine elements -- */
void BrMat3Sub(float *pOut, const float *pA, const float *pB)
{
    int i, j;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut[3 * i + j] = pA[3 * i + j] - pB[3 * i + j];
}

/* -- racing/br_rank.c : qsort comparator on a float key -- */
int BrRankCmpKey(const void *pA, const void *pB)
{
    if (*(const float *)pA > *(const float *)pB) return 1;
    if (!(*(const float *)pA >= *(const float *)pB)) return -1;
    return 0;
}

/* -- net/br_car.c : clamp a networked car's state into legal ranges -- */
void BrCarClampPosXY(float *pv)
{
    if (!(*pv >= 0.0f))    *pv = 0.0f;
    if (*pv > 2048.0f)     *pv = 2048.0f;
}
void BrCarClampPosZ(float *pv)
{
    if (!(*pv >= -256.0f)) *pv = -256.0f;
    if (*pv > 256.0f)      *pv = 256.0f;
}
void BrCarClampUnit(float *pv)
{
    if (!(*pv >= -1.0f))   *pv = -1.0f;
    if (*pv > 1.0f)        *pv = 1.0f;
}

/* -- drawing/br_textmode.c : clear a text flag -- */
extern uint8_t g_br4B0358;
void BrTextFlag358Clear(void) { g_br4B0358 = 0; }

/* -- audio/br_musiccmd.c : fire-and-forget sound-group play -- */
extern int32_t BrSndPlayGroup(int32_t a, uint32_t b, int32_t c);
void BrSub10072AF0(int a, int b)
{
    (void)BrSndPlayGroup((int32_t)a, (uint32_t)b, 0);
}

/* ====================================================================== */
/* 2. Not-yet-ported functions -- stubs so the boot path links            */
/* ====================================================================== */

/* Car control bodies (0x1005C8B0 human, 0x1005D770 AI).  Until ported, cars
 * receive no control input -- they will sit inert rather than crash. */
void BrCtlHumanBody(void *pCar) { (void)pCar; }
void BrCtlAiBody(void *pCar)    { (void)pCar; }

/* Segment-base setter for the N64 address map (0x100...).  Track data that
 * relies on this resolving will read from the wrong base until ported. */
void BrSegSetBasesG(uint32_t n64Base, void *pHost) { (void)n64Base; (void)pHost; }

/* thiscall helper targets referenced by tiny_stubs.c. */
void   BrSub10008760(void *self) { (void)self; }
void   BrSub10008D60(void *self) { (void)self; }
double BrSub1001DC40(int x)      { (void)x; return 0.0; }

/* uipoll helper (0x1003FAC0), returns an int status. */
int FUN_1003fac0(int a) { (void)a; return 0; }

/* ====================================================================== */
/* 3. Data globals -- storage the decomp only declares extern             */
/* ====================================================================== */

/* -- GBI 2D-rect state (slice4_51.c).  The _A75xx pair is the screen size;
 *    the _K constants are fixed-point divisors and MUST be non-zero. -- */
float BrGbiRectK_HALF  = 2.0f;   /* screenW / HALF -> half width          */
float BrGbiRectK_FIXED = 4.0f;   /* 10.2 fixed-point coord -> pixels       */
int   BrGbiRectG_A7514 = 640;    /* screen width  (0x100A7514)             */
int   BrGbiRectG_A7518 = 480;    /* screen height (0x100A7518)             */
float BrGbiRectG_A9A54 = 1.0f;
float BrGbiRectG_5D17C4 = 0.0f;
int   BrGbiRectG_5CDA04 = 0;
float BrGbiRectG_5CCD44 = 0.0f;
float BrGbiRectG_5CD9F4 = 0.0f;
float BrGbiRectG_5CCCF8 = 0.0f;
float BrGbiRectG_5D17A4 = 0.0f;
float BrGbiRectG_5D17B4 = 0.0f;
float BrGbiRectG_5CE2D0 = 0.0f;
int   BrGbiRectG_5D17C8 = 0;
int   BrGbiRectG_18ED198 = 0;
int   BrGbiRectG_186C954 = 0;
int   BrGbiRectG_186C950 = 0;
int   BrGbiRectG_18EC988 = 0;

/* -- misc scalar state -- */
int      DAT_10ac5a4c = 0;
int      DAT_10ac5a4d = 0;
int16_t  DAT_10ac5b3a = 0;
void    *DAT_10ac5d44 = 0;   /* BrCtl* */
int      DAT_10ac5d88 = 0;
int      DAT_10b71a48 = 0;
int      DAT_10b71a4c = 0;
int      DAT_10b71a50 = 0;
int      DAT_10b71a54 = 0;
int      _DAT_10ac5bbc = 0;
int      g_AC0810 = 0;       /* object bases */
int      g_B71290 = 0;

/* -- arrays -- */
/* Accumulator array (br_accum.c).  Nothing on the boot/menu path calls
 * BrAccumAddClamp; sized generously and zeroed. */
float g_Br6C5468[256] = {0};

/* Real ROM data tables.  SIZE and CONTENTS are placeholders -- code that
 * reads these (display-list opcode decode, HUD sprite layout) will be wrong
 * until the bytes are extracted from the binary, but the link is satisfied. */
const unsigned char g_BrDlTableA[256] = {0};
const uint8_t       g_hudSpriteTable[4096] = {0};
