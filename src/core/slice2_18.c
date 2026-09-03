/* slice2_18.c -- N64-GBI frame/fog/viewport layer, decompiled from BRD3D.dll.
 *
 * Packet: 0x10031866-0x10033838.  See slice2_18.h for what the module is and
 * how the GBI identification was established.
 *
 * SKIPPED (deliberately, per the contract's "skip rather than guess"):
 *
 *   0x100331FF  The packet's listing for this function starts mid-instruction
 *               -- its first bytes disassemble as `adc al, 0x81`, which is the
 *               tail of a preceding `and edx, 0xFFF`.  The prologue, the frame
 *               size, the number of arguments and roughly the first third of
 *               the body are all outside the listing, and the visible part
 *               already dereferences locals ([ebp-0x40]) that are initialised
 *               there.  The visible tail emits six 0xE1 (G_RDPHALF_1) pairs
 *               forming a 1px border around (x, y, w, h) = ([ebp+8], [ebp+0xC],
 *               [ebp+0x10], [ebp+0x14]), but the opening command and the
 *               colour/mode setup are missing, so no faithful function can be
 *               written.
 *
 *   0x100334D7  Same problem: the listing starts at a bare `jmp` into the
 *               middle of a for-loop.  The loop counter [ebp-0x18] and the
 *               mode flag [ebp-0x14] are both initialised in the missing
 *               prologue, `esi` is popped at the end without ever being pushed
 *               in the visible range, and the argument list is unknown.  The
 *               body walks 0x24-byte records at BrG_6C7C64 and dispatches
 *               through 0x118AA0A4, so guessing the entry state would very
 *               likely produce a wrong-but-plausible function.
 *
 * Everything else in the packet is here.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "slice2_18.h"

/* ------------------------------------------------------------------ */
/* Globals (all file-scope statics in the original -- see the header)  */
/* ------------------------------------------------------------------ */

uint32_t *BrG_6C0680;
uint8_t  *BrG_6C0944;
int32_t   BrG_6C65EC;

int32_t BrG_6C6614, BrG_6C661C, BrG_6C6620, BrG_6C6624, BrG_6C6618;

uint8_t BrG_6C0260, BrG_6C1614, BrG_6C0200, BrG_690BE8;

int32_t BrG_0B4050, BrG_6C3398, BrG_6C64D8, BrG_6C2CF4, BrG_6C1618;

int32_t BrG_0A79CC, BrG_0B380C;
void   *BrG_6C6490;
BrVec3  BrG_4B0378;
void   *BrG_6C2CF8;
float   BrG_6C7C80, BrG_6C7C84;
uint8_t BrG_6C7CC8[3];
float   BrG_6C29A8[16];

BrVec3   BrG_6C0670;
uint8_t  BrG_6C1580, BrG_6C335C, BrG_6C0968;
uint8_t  BrG_690BF0, BrG_6C0960, BrG_6C65BC;
uint8_t  BrG_690FF8[4], BrG_6C6494[4], BrG_6C3358[4];
uint32_t BrG_6C29E8, BrG_6C5AB0;
uint32_t BrG_6C0950[4];

int32_t  BrG_6C65E0, BrG_6C65E4, BrG_6C65E8;
int32_t  BrG_0AA8B4, BrG_0A81C0, BrG_0A81C4;
int32_t  BrG_6C299C, BrG_6C0684, BrG_0AA890;
uint32_t BrG_6C0258, BrG_6C0688, BrG_6C0920;
void    *BrG_0AA730;
void    *BrG_6C3308;
void    *BrG_0AA838, *BrG_0AA860, *BrG_0AA868;
int32_t  BrG_0AA884;
uint8_t *BrG_0AA770;
int32_t  BrG_6C65FC, BrG_6C6604;
int32_t  BrG_6C1628[BR_S18_FRAMEREC_DWORDS];

int32_t BrG_575508, BrG_575500, BrG_57550C, BrG_5754FC;

int32_t  BrG_6C6654, BrG_6C3364, BrG_6C1174;
BrVpRec  BrG_6C1788[BR_S18_VP_SLOTS];
int32_t  BrG_6C62D8, BrG_6C65B8;

int32_t  BrG_6C56E8;
uint16_t BrG_0B5D90;
void    *BrG_691000;
void    *BrG_6C65A0;
uint8_t *BrG_6C6678;

BrOsTask  BrG_6C1588[2];
uintptr_t BrG_0AA728;
uint32_t  BrG_0AA72C;
int32_t   BrG_6C6668, BrG_6C6660, BrG_6C6658, BrG_6C665C;
uint8_t  *BrG_363FF0, *BrG_2E5EC8, *BrG_364304, *BrG_3643BC;
int32_t   BrG_6C1170, BrG_6C6664;
void     *BrG_6C33A0;
void     *BrG_6C3380;
void    (*BrG_6C198C)(void);
int32_t   BrG_6C1608, BrG_6C020C, BrG_6C0208, BrG_6C1620, BrG_0ADFC0;
void     *BrG_AA4020;
void     *BrG_AA3760;
void     *BrG_AA3D50;
void     *BrG_AA3490;
int32_t   BrG_6C65F4, BrG_6C65F8, BrG_6C6598, BrG_6C56E4;
void    (*BrG_B501D0)(uintptr_t);

/* ------------------------------------------------------------------ */
/* Local helpers                                                      */
/* ------------------------------------------------------------------ */

/* The .rdata constants this packet loads, read straight out of the DLL:
 *   0x1008F4E8 100.0   0x1008F4EC 1.0     0x1008F4F0 240.0   0x1008F4F4 248.0
 *   0x1008F4F8 255.0   0x1008F4FC 1024.0  0x1008F500 0.45/1024
 *   0x1008F504 0.9/1024 0x1008F508 160.0  0x1008F50C 0.0     0x1008F510 1/255
 */
#define BR_K_100      100.0f
#define BR_K_240      240.0f
#define BR_K_248      248.0f
#define BR_K_255      255.0f
#define BR_K_1024     1024.0f
#define BR_K_F500     0.0004394531133584678f   /* 0x1008F500 */
#define BR_K_F504     0.0008789062267169356f   /* 0x1008F504 */
#define BR_K_160      160.0f
#define BR_K_INV255   0.003921568859368563f    /* 0x1008F510 */

/* 0x1007C8A0 __ftol: truncate toward zero and keep the low 32 bits of the
 * 64-bit x87 result (see the contract's known-correct facts).
 *
 * DEVIATION: the out-of-range case is written out explicitly.  x87 yields the
 * "integer indefinite" 0x8000000000000000 there, whose low dword is 0; C would
 * simply be undefined, so the port tests the range instead of relying on it. */
static int32_t BrFtol(double v)
{
    double t = (v < 0.0) ? ceil(v) : floor(v);

    if (t >= -9223372036854775808.0 && t < 9223372036854775808.0) {
        return (int32_t)(uint32_t)(uint64_t)(int64_t)t;
    }
    return 0;
}

/* `shl reg, 1` -- two's-complement doubling.  Written through unsigned so an
 * overflowing shift is defined here as it is in the original. */
static int32_t BrShl1(int32_t v)
{
    return (int32_t)((uint32_t)v << 1);
}

/* `neg reg` */
static int32_t BrNeg(int32_t v)
{
    return (int32_t)(0u - (uint32_t)v);
}

/* Reserve the next 8-byte display-list slot and advance the cursor.  The
 * original is an open-coded  p = g_6C0680; g_6C0680 += 8;  at every site. */
static uint32_t *BrGfxAlloc8(void)
{
    uint32_t *p = BrG_6C0680;
    BrG_6C0680 += 2;
    return p;
}

static void BrGfxPut(uint32_t w0, uint32_t w1)
{
    uint32_t *p = BrGfxAlloc8();
    p[0] = w0;
    p[1] = w1;
}

/* Base of the display-list arena for the current buffer index. */
static uint32_t *BrGfxDlBase(void)
{
    return (uint32_t *)(void *)(BrG_6C0944
                                + (ptrdiff_t)BrG_6C65EC * 0x17700
                                + 0x200);
}

/* (1 - t) * (float)c + hi * t, evaluated in the original's operand order.
 * `c` reaches the FPU as (float)(int)byte via a fild/fstp dword pair. */
static int32_t BrS18Lerp(float t, uint8_t c, float hi)
{
    double lo = (1.0 - (double)t) * (double)(float)(int32_t)c;
    return BrFtol(lo + (double)hi * (double)t);
}

/* ((v * a) + (255 - a) * base) / 255, the integer blend BrHudColorsUpdate
 * uses six times.  All operands are non-negative here, so the original's
 * signed idiv and C's / agree. */
static uint8_t BrS18Blend255(int32_t v, int32_t a, int32_t base)
{
    return (uint8_t)((v * a + (255 - a) * base) / 255);
}

/* ------------------------------------------------------------------ */
/* 0x10031866  BrFogUpdate                                            */
/* ------------------------------------------------------------------ */

void BrFogUpdate(void)
{
    if (BrG_6C661C != 0) {
        /* The original writes 0 to B, reads it back into G, then into R --
         * three stores of the same zero. */
        BrG_6C0200 = 0;
        BrG_6C1614 = BrG_6C0200;
        BrG_6C0260 = BrG_6C1614;
        BrG_690BE8 = 0x40;
        if (BrG_0B4050 == 2) {
            BrG_6C3398 = 0x3E0;
            BrG_6C64D8 = 0x3FC;
        } else {
            BrG_6C3398 = 0x3C8;
            BrG_6C64D8 = 0x3FC;
        }
    } else if (BrG_6C6620 != 0) {
        BrG_6C0260 = 0xB8;
        BrG_6C1614 = 0xB8;
        BrG_6C0200 = 0xD8;
        BrG_690BE8 = 0x40;
        if (BrG_0B4050 == 2) {
            BrG_6C3398 = 0x3B6;
            BrG_6C64D8 = 0x3E8;
        } else {
            BrG_6C3398 = 0x320;
            BrG_6C64D8 = 0x41A;
        }
    } else if (BrG_6C6624 != 0) {
        BrG_6C0260 = 0x60;
        BrG_6C1614 = 0x68;
        BrG_6C0200 = 0x70;
        BrG_690BE8 = 0x40;

        /* Note the guard: `> 0` AND `& 1`, i.e. a positive ODD value only. */
        if (BrG_0A79CC > 0 && (BrG_0A79CC & 1) != 0) {
            const BrVec3 *pPos = (const BrVec3 *)(const void *)
                                 ((const unsigned char *)BrG_6C6490 + 0x30);
            float d = BrVec3DistXY(pPos, &BrG_4B0378);
            /* fadd 100.0 then fdivr 100.0 -> 100 / (d + 100) */
            float t = (float)(100.0 / ((double)d + (double)BR_K_100));

            BrG_6C0260 = (uint8_t)BrS18Lerp(t, BrG_6C0260, BR_K_240);
            BrG_6C1614 = (uint8_t)BrS18Lerp(t, BrG_6C1614, BR_K_248);
            BrG_6C0200 = (uint8_t)BrS18Lerp(t, BrG_6C0200, BR_K_255);
        }
        if (BrG_0B4050 == 2) {
            BrG_6C3398 = 0x3A2;
            BrG_6C64D8 = 0x3E8;
        } else {
            BrG_6C3398 = 0x352;
            BrG_6C64D8 = 0x401;
        }
    } else if (BrG_6C6618 != 0) {
        const float *pfC = (const float *)(const void *)
                           ((const unsigned char *)BrG_6C2CF8 + 0x38);
        int32_t shade;

        /* (c - near) / (far - near) * 255, truncated, clamped to 0..255. */
        shade = BrFtol((double)((*pfC - BrG_6C7C80) / (BrG_6C7C84 - BrG_6C7C80))
                       * (double)BR_K_255);
        if (shade < 0) {
            shade = 0;
        } else if (shade > 0xFF) {
            shade = 0xFF;
        }

        if (BrG_0B380C == 0) {
            const float *pfCam = (const float *)(const void *)
                                 ((const unsigned char *)BrG_6C6490 + 0x30);
            float f;

            /* fcomp 1024.0 / test ah,0x41 / jne -> zero.  0x41 is C0|C3, so
             * the zero path is taken when the value is LESS OR EQUAL; the
             * product is only formed when it is strictly greater. */
            if (pfCam[1] > BR_K_1024) {
                f = (float)(((double)pfCam[0] * (double)BR_K_F500)
                            * (((double)pfCam[1] - (double)BR_K_1024)
                               * (double)BR_K_F504));
            } else {
                f = 0.0f;
            }

            BrG_6C0260 = (uint8_t)BrS18Lerp(f, BrG_6C7CC8[0], BR_K_160);
            BrG_6C1614 = (uint8_t)BrS18Lerp(f, BrG_6C7CC8[1], BR_K_160);
            BrG_6C0200 = (uint8_t)BrS18Lerp(f, BrG_6C7CC8[2], BR_K_160);
        } else {
            BrG_6C0260 = BrG_6C7CC8[0];
            BrG_6C1614 = BrG_6C7CC8[1];
            BrG_6C0200 = BrG_6C7CC8[2];
        }

        BrG_690BE8 = (uint8_t)shade;

        if (BrG_0B4050 == 2) {
            BrG_6C3398 = 0x3E3;
            BrG_6C64D8 = 0x3E8;
        } else {
            BrG_6C3398 = 0x3D4;
            BrG_6C64D8 = 0x3E8;
        }
    } else {
        BrG_6C0260 = 0;
        BrG_6C1614 = 0;
        BrG_6C0200 = 0;
        BrG_690BE8 = 0xFF;
        BrG_6C3398 = 0;
        BrG_6C64D8 = 0x3E8;
    }

    /* guFog.  0x1F400 == 500 * 256.  Every branch above leaves far != near,
     * so the original never divides by zero and neither does this. */
    {
        int32_t span = BrG_6C64D8 - BrG_6C3398;

        BrG_6C2CF4 = 0x1F400 / span;
        BrG_6C1618 = (int32_t)((uint32_t)(0x1F4 - BrG_6C3398) << 8) / span;

        /* G_MOVEWD, offset 8 == G_MW_FOG.  The original recomputes both
         * quotients inline here rather than reloading the globals; same
         * values. */
        BrGfxPut(0xBC000008u,
                 (((uint32_t)BrG_6C2CF4 & 0xFFFFu) << 16)
                 | ((uint32_t)BrG_6C1618 & 0xFFFFu));
    }

    /* G_SETFOGCOLOR.  `or al, 0xFF` forces alpha to 0xFF. */
    BrGfxPut(0xF8000000u,
             ((uint32_t)BrG_6C0260 << 24)
             | ((uint32_t)BrG_6C1614 << 16)
             | ((uint32_t)BrG_6C0200 << 8)
             | 0xFFu);
}

/* ------------------------------------------------------------------ */
/* 0x10031D3F  BrFogFactorAtPoint                                     */
/* ------------------------------------------------------------------ */

float BrFogFactorAtPoint(const BrVec3 *pPoint)
{
    float clip[4];
    float v;

    if (BrG_6C6618 == 0) {
        return 0.0f;
    }

    BrMat4TransformPoint4(clip, pPoint, BrG_6C29A8);

    /* z / w, then the same fm/fo the RDP was handed, then /255. */
    v = clip[2] / clip[3];
    v = (float)((((double)v * (double)(float)BrG_6C2CF4)
                 + (double)(float)BrG_6C1618) * (double)BR_K_INV255);

    /* fcomp 0.0 / test ah,1 (C0 = "less than") / je -> not less, keep going */
    if (v < 0.0f) {
        return 0.0f;
    }
    /* fcomp 1.0 / test ah,0x41 (C0|C3 = "less or equal") / jne -> return v.
     * A NaN sets C0|C2|C3 and so takes this branch: NaN is returned, not
     * clamped.  Faithful. */
    if (v <= 1.0f) {
        return v;
    }
    return 1.0f;
}

/* ------------------------------------------------------------------ */
/* 0x10031DCF  BrHudColorsUpdate                                      */
/* ------------------------------------------------------------------ */

void BrHudColorsUpdate(void)
{
    int32_t i;

    if (BrG_6C661C == 0) {
        BrG_6C0670.x = 15.0f;   /* 0x41700000 */
        BrG_6C0670.y = 10.0f;   /* 0x41200000 */
        BrG_6C0670.z = 20.0f;   /* 0x41A00000 */
        BrVec3Normalise(&BrG_6C0670);
    }

    if (BrG_6C6620 != 0 || BrG_6C6624 != 0) {
        int32_t r = BrG_6C0260, g = BrG_6C1614, b = BrG_6C0200;

        BrG_6C1580 = (uint8_t)((r + 0x2FD) >> 2);
        BrG_6C335C = (uint8_t)((g + 0x2FD) >> 2);
        BrG_6C0968 = (uint8_t)((b + 0x264) >> 2);
        /* The original spells x/8 as (x + ((x>>31)&7)) >> 3; all inputs are
         * non-negative so this is a plain divide. */
        BrG_690BF0 = (uint8_t)((r * 5) / 8);
        BrG_6C0960 = (uint8_t)((g * 5) / 8);
        BrG_6C65BC = (uint8_t)((b * 5) / 8);
    } else if (BrG_6C661C != 0) {
        BrG_6C1580 = 0xDD;
        BrG_6C335C = 0xEE;
        BrG_6C0968 = 0xFF;
        BrG_690BF0 = 0x3C;
        BrG_6C0960 = 0x39;
        BrG_6C65BC = 0x36;
    } else if (BrG_6C6618 != 0) {
        int32_t r = BrG_6C0260, g = BrG_6C1614, b = BrG_6C0200;
        int32_t a = BrG_690BE8;

        BrG_6C1580 = BrS18Blend255((r + 0xFF) >> 1, a, 0xFF);
        BrG_6C335C = BrS18Blend255((g + 0xFF) >> 1, a, 0xFF);
        BrG_6C0968 = BrS18Blend255((b + 0xCC) >> 1, a, 0xCC);
        BrG_690BF0 = BrS18Blend255((r * 4) / 5, a, 0x66);
        BrG_6C0960 = BrS18Blend255((g * 4) / 5, a, 0x66);
        BrG_6C65BC = BrS18Blend255((b * 4) / 5, a, 0x77);
    } else {
        BrG_6C1580 = 0xFF;
        BrG_6C335C = 0xFF;
        BrG_6C0968 = 0xCC;
        BrG_690BF0 = 0x66;
        BrG_6C0960 = 0x66;
        BrG_6C65BC = 0x77;
    }

    if (BrG_6C661C != 0) {
        BrG_690FF8[0] = 0x22; BrG_6C6494[0] = 0x22; BrG_6C3358[0] = 0x22;
        BrG_690FF8[1] = 0x44; BrG_6C6494[1] = 0x44; BrG_6C3358[1] = 0x44;
        BrG_690FF8[2] = 0x66; BrG_6C6494[2] = 0x66; BrG_6C3358[2] = 0x66;
        BrG_690FF8[3] = 0xFF; BrG_6C6494[3] = 0xFF; BrG_6C3358[3] = 0xFF;
    } else if (BrG_6C6620 != 0) {
        /* Note the third column does NOT follow the first two here. */
        BrG_690FF8[0] = 0xD0; BrG_6C6494[0] = 0xD0; BrG_6C3358[0] = 0xF0;
        BrG_690FF8[1] = 0xE0; BrG_6C6494[1] = 0xE0; BrG_6C3358[1] = 0xFF;
        BrG_690FF8[2] = 0xF0; BrG_6C6494[2] = 0xF0; BrG_6C3358[2] = 0xFF;
        BrG_690FF8[3] = 0xFF; BrG_6C6494[3] = 0xFF; BrG_6C3358[3] = 0xFF;
    } else {
        /* A 1/4, 1/2, 3/4, 1 ramp of the colour picked above. */
        BrG_690FF8[0] = (uint8_t)(BrG_6C1580 >> 2);
        BrG_6C6494[0] = (uint8_t)(BrG_6C335C >> 2);
        BrG_6C3358[0] = (uint8_t)(BrG_6C0968 >> 2);
        BrG_690FF8[1] = (uint8_t)(BrG_6C1580 >> 1);
        BrG_6C6494[1] = (uint8_t)(BrG_6C335C >> 1);
        BrG_6C3358[1] = (uint8_t)(BrG_6C0968 >> 1);
        BrG_690FF8[2] = (uint8_t)((BrG_6C1580 >> 1) + (BrG_6C1580 >> 2));
        BrG_6C6494[2] = (uint8_t)((BrG_6C335C >> 1) + (BrG_6C335C >> 2));
        BrG_6C3358[2] = (uint8_t)((BrG_6C0968 >> 1) + (BrG_6C0968 >> 2));
        BrG_690FF8[3] = BrG_6C1580;
        BrG_6C6494[3] = BrG_6C335C;
        BrG_6C3358[3] = BrG_6C0968;
    }

    BrG_6C29E8 = ((uint32_t)BrG_690BF0 << 24)
                 | ((uint32_t)BrG_6C0960 << 16)
                 | ((uint32_t)BrG_6C65BC << 8);
    BrG_6C5AB0 = ((uint32_t)BrG_6C1580 << 24)
                 | ((uint32_t)BrG_6C335C << 16)
                 | ((uint32_t)BrG_6C0968 << 8);

    /* Alpha is left at zero in all five words -- the original never ORs in
     * 0xFF here the way BrFogUpdate does. */
    for (i = 0; i < 4; i++) {
        BrG_6C0950[i] = ((uint32_t)BrG_690FF8[i] << 24)
                        | ((uint32_t)BrG_6C6494[i] << 16)
                        | ((uint32_t)BrG_6C3358[i] << 8);
    }
}

/* ------------------------------------------------------------------ */
/* 0x100322E6  BrFrameBegin                                           */
/* ------------------------------------------------------------------ */

void BrFrameBegin(int32_t *pRec, int32_t fHiRes)
{
    int32_t mode;

    if ((fHiRes ^ BrG_6C65E4) != 0) {
        BrG_6C65E0 = 1;
        BrG_6C65E4 = fHiRes;
    }

    BrStub8B80_5i(0, 0, 0x82, 0, 0xFF);

    mode = BrG_0AA8B4;
    if (mode == 1) {
        pRec[0] = 0;
        pRec[1] = 0;
        pRec[2] = BrG_0A81C0;
        pRec[3] = BrG_0A81C4;
    } else if (mode == 2) {
        pRec[0x58 / 4] = 8;
        pRec[0x5C / 4] = (BrG_6C299C >> 1) + 1;
        pRec[0x60 / 4] = BrG_6C0684 - 0x60;
        pRec[0x64 / 4] = (BrG_6C299C >> 1) - 8;
        pRec[0] = 8;
        pRec[1] = 8;
        pRec[2] = BrG_6C0684 - 0x60;
        pRec[3] = (BrG_6C299C >> 1) - 8;
    }
    /* Any other value of BrG_0AA8B4 leaves the record untouched. */

    BrGfx31227();
    BrGfx69580();

    BrG_6C0680 = BrGfxDlBase();

    /* neg / sbb / and: 0x2000 when non-zero, 0 when zero. */
    BrG_6C0258 = (BrG_0AA890 != 0) ? 0x2000u : 0u;
    BrG_6C0688 = 0x40;
    BrG_6C0920 = 0;

    BrGfxPut(0xBC000006u, 0);   /* G_MOVEWD, G_MW_SEGMENT */
    BrGfxPut(0xE7000000u, 0);   /* G_RDPPIPESYNC */

    BrScissorSet(0, 0, BrG_6C0684, BrG_6C299C);

    {
        uint32_t *pSlot = BrG_6C0680;
        BrG_6C0680 += 2;    /* the original bumps the cursor AFTER the push */
        BrGfx2F900(pSlot,
                   0, 0, 0, 0x3EB,
                   0, 0, 0, 0x3EB,
                   0, 0, 0, 0x3E8,
                   0, 0, 0, 0x3E8);
    }

    BrGfxPut(0xBA001001u, 0);
    BrGfxPut(0xBA000E02u, 0);
    BrGfxPut(0xBA001102u, 0);
    BrGfxPut(0xBA001301u, 0x80000u);
    BrGfxPut(0xBA000C02u, BrG_6C0258);
    BrGfxPut(0xBA000903u, 0xC00u);
    BrGfxPut(0xBA000801u, 0);
    BrGfxPut(0xB9000002u, 1);
    BrGfxPut(0xB900031Du, 0x0F0A4000u);
    BrGfxPut(0xBA000602u, BrG_6C0688);
    BrGfxPut(0xBA000602u, BrG_6C0920);   /* same opcode twice, two operands */
    BrGfxPut(0xBA001402u, 0);
    BrGfxPut(0xF9000000u, 0);            /* G_SETBLENDCOLOR */
    /* G_MTX with the float identity at 0x100AA730.
     * DEVIATION: a host pointer is truncated to the 32 bits a GBI word holds. */
    BrGfxPut(0x01020040u, (uint32_t)(uintptr_t)BrG_0AA730);
    BrGfxPut(0xB6000000u, 0x001F3204u);  /* G_CLEARGEOMETRYMODE */
    BrGfxPut(0xB7000000u, 0x2000u);      /* G_SETGEOMETRYMODE */

    if (BrG_0AA884 != 0) {
        BrGfxPut(0xB7000000u, 0x800000u);
    } else {
        BrGfxPut(0xB6000000u, 0x800000u);
    }

    /* G_DL into one of the 0x28-byte canned sub-lists at 0x100AA770. */
    BrGfxPut(0x06000000u,
             (uint32_t)(uintptr_t)(BrG_0AA770 + (ptrdiff_t)BrG_6C65FC * 0x28));
    BrGfxPut(0xBB000000u, 0);            /* G_TEXTURE */

    BrStub8B80_1i(0x40);
    BrStub8B80_1i(0x10);
    BrStub8B80_1i(BrG_6C6604 != 0 ? 1 : 2);
}

/* 0x10032873 */
/* WHAT IT DOES: starts a frame at normal resolution: it resets the drawing-
 * command cursor to this frame's buffer and lays down the fixed block of
 * commands every frame opens with -- scissor, blend and combine setup,
 * geometry switches, the identity matrix and the viewport. */
/* @implements 0x10032873 d3d BrFrameBeginRec */
void BrFrameBeginRec(int32_t *pRec)
{
    BrFrameBegin(pRec, 0);
}

/* 0x10032886 */
/* WHAT IT DOES: starts a frame at the high resolution instead, using the
 * game's own frame record. Switching resolution mid-run is noticed and
 * reloads the state that everything downstream keys off when it halves or
 * doubles a rectangle. */
/* @implements 0x10032886 d3d BrFrameBeginHiRes */
void BrFrameBeginHiRes(void)
{
    BrFrameBegin(BrG_6C1628, 1);
}

/* 0x1003289A */
/* WHAT IT DOES: does nothing. It sits in a run of frame-setup routines and
 * is empty in this build; whether it ever had a body is not established. */
/* @d3donly 0x1003289A BrFrameNop -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrFrameNop(void)
{
}

/* ------------------------------------------------------------------ */
/* 0x1003289F  BrScissorSet                                           */
/* ------------------------------------------------------------------ */

void BrScissorSet(int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t *p;
    int32_t   a, b;

    if (x < BrG_575508) {
        w -= (BrG_575508 - x);
        x = BrG_575508;
    }
    if (x + w > BrG_575500) {
        w = BrG_575500 - x;
    }
    if (w < 0) {
        w = 0;
    }

    if (y < BrG_57550C) {
        h -= (BrG_57550C - y);
        y = BrG_57550C;
    }
    if (y + h > BrG_5754FC) {
        h = BrG_5754FC - y;
    }
    if (h < 0) {
        h = 0;
    }

    if (BrG_6C65E4 != 0) {
        x = BrShl1(x);
        y = BrShl1(y);
        w = BrShl1(w);
        h = BrShl1(h);
    }

    BrGfxPut(0xE7000000u, 0);   /* G_RDPPIPESYNC */

    p = BrGfxAlloc8();

    /* Each coordinate goes int -> float -> *1.0f -> __ftol before masking.
     * The multiply by 1.0 is a no-op the original still performs. */
    a = BrFtol((double)((float)x) * 1.0);
    b = BrFtol((double)((float)y) * 1.0);
    p[0] = 0xE2000000u
           | (((uint32_t)a & 0xFFFu) << 12)
           | ((uint32_t)b & 0xFFFu);

    a = BrFtol((double)((float)(x + w)) * 1.0);
    b = BrFtol((double)((float)(y + h)) * 1.0);
    p[1] = (((uint32_t)a & 0xFFFu) << 12)
           | ((uint32_t)b & 0xFFFu);
}

/* ------------------------------------------------------------------ */
/* 0x10032A42  BrViewportSet                                          */
/* ------------------------------------------------------------------ */

void BrViewportSet(int32_t x, int32_t y, int32_t w, int32_t h,
                   int32_t fScissor)
{
    BrVpRec *pVp;

    BrG_6C6654 = (BrG_6C6654 + 1) & 0x1F;

    if (fScissor != 0) {
        /* |w|, but h is passed through unchanged -- asymmetric, faithful. */
        int32_t aw = (w < 0) ? BrNeg(w) : w;
        BrScissorSet(x, y, aw, h);
    }

    if (BrG_6C65E4 != 0) {
        x = BrShl1(x);
        y = BrShl1(y);
        w = BrShl1(w);
        h = BrShl1(h);
    }

    pVp = &BrG_6C1788[BrG_6C6654 & 0x1F];

    if (w < 0) {
        if (BrG_6C3364 != 0) {
            pVp->vscale[0] = (int16_t)(int32_t)((uint32_t)w * (uint32_t)-2);
        } else {
            pVp->vscale[0] = (int16_t)BrShl1(w);
        }
        w = BrNeg(w);
        BrG_6C1174 = 1;
    } else {
        if (BrG_6C3364 != 0) {
            pVp->vscale[0] = (int16_t)(int32_t)((uint32_t)w * (uint32_t)-2);
        } else {
            pVp->vscale[0] = (int16_t)BrShl1(w);
        }
        BrG_6C1174 = 0;
    }

    pVp->vscale[1] = (int16_t)BrShl1(h);
    pVp->vscale[2] = 0x1FF;
    pVp->vscale[3] = 0;

    /* centre * 4, spelled (w + 2x) * 2 -- note w is already |w| here. */
    pVp->vtrans[0] = (int16_t)BrShl1(w + BrShl1(x));
    pVp->vtrans[1] = (int16_t)BrShl1(h + BrShl1(y));
    pVp->vtrans[2] = 0x1FF;
    pVp->vtrans[3] = 0;

    /* G_MOVEMEM, viewport.
     * DEVIATION: a host pointer truncated into a 32-bit GBI word. */
    BrGfxPut(0x03800010u, (uint32_t)(uintptr_t)pVp);

    BrG_6C62D8 = pVp->vscale[2];
    BrG_6C65B8 = pVp->vtrans[2];
}

/* ------------------------------------------------------------------ */
/* 0x10032C38  BrViewportSetFull                                      */
/* ------------------------------------------------------------------ */

void BrViewportSetFull(int32_t x, int32_t y, int32_t w, int32_t h,
                       int32_t fScissor)
{
    BrVpRec *pVp;

    BrG_6C6654 = (BrG_6C6654 + 1) & 0x1F;

    if (fScissor != 0) {
        int32_t aw = (w < 0) ? BrNeg(w) : w;
        /* Unlike BrViewportSet, this one emits its own pipe sync first. */
        BrGfxPut(0xE7000000u, 0);
        BrScissorSet(x, y, aw, h);
    }

    if (BrG_6C65E4 != 0) {
        x = BrShl1(x);
        y = BrShl1(y);
        w = BrShl1(w);
        h = BrShl1(h);
    }
    /* Dead in the original too: nothing below reads x/y/w/h again. */
    (void)x; (void)y; (void)w; (void)h;

    BrG_6C1174 = 0;

    pVp = &BrG_6C1788[BrG_6C6654 & 0x1F];

    pVp->vscale[0] = (int16_t)BrShl1(BrG_0A81C0);
    pVp->vscale[1] = (int16_t)BrShl1(BrG_0A81C4);
    pVp->vscale[2] = 0x1FF;
    pVp->vscale[3] = 0;

    /* (n/2)*4 with the divide rounding toward zero (cdq/sub/sar idiom). */
    pVp->vtrans[0] = (int16_t)((BrG_0A81C0 / 2) * 4);
    pVp->vtrans[1] = (int16_t)((BrG_0A81C4 / 2) * 4);
    pVp->vtrans[2] = 0x1FF;
    pVp->vtrans[3] = 0;

    BrGfxPut(0x03800010u, (uint32_t)(uintptr_t)pVp);

    BrG_6C62D8 = pVp->vscale[2];
    BrG_6C65B8 = pVp->vtrans[2];
}

/* ------------------------------------------------------------------ */
/* 0x10032DF2  BrViewportReEmit                                       */
/* ------------------------------------------------------------------ */

void BrViewportReEmit(void)
{
    BrVpRec *pVp = &BrG_6C1788[BrG_6C6654 & 0x1F];

    BrGfxPut(0x03800010u, (uint32_t)(uintptr_t)pVp);

    BrG_6C62D8 = pVp->vscale[2];
    BrG_6C65B8 = pVp->vtrans[2];
}

/* 0x1003348E, 0x10033493 */
/* WHAT IT DOES: does nothing. One of a pair of empty routines in this build,
 * sitting between the viewport and heads-up-display code. */
/* @d3donly 0x1003348E BrGfxNopA -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrGfxNopA(void) { }
void BrGfxNopB(void) { }

/* ------------------------------------------------------------------ */
/* 0x10033780 / 0x100337AE / 0x100337E9  HUD text                     */
/* ------------------------------------------------------------------ */

void BrHudTextBegin(void)
{
    if (BrG_6C56E8 == 0) {
        BrG_6C56E8 = 1;
        BrG_0B5D90 = 0;
        BrGfx42AF0_1(BrG_691000);
    }
}

void BrHudTextEnd(void)
{
    BrHudTextBegin();
    BrGfx42AF0_3(BrG_691000, 0, 1);
    BrGfx60E00(&BrG_6C65A0);
    BrG_0B5D90 = 1;
    BrG_6C56E8 = 0;
}

void BrHudDrawAll(void)
{
    int32_t i;

    BrHudTextEnd();

    /* The bound really is a literal 1 in this build. */
    for (i = 0; i < 1; i++) {
        uint8_t *pRec = BrG_6C6678 + (ptrdiff_t)i * 0x15C;
        BrEnt35CE0(pRec);
        BrEnt35FC0(pRec);
    }
}

/* ------------------------------------------------------------------ */
/* 0x10033838  BrFrameEnd                                             */
/* ------------------------------------------------------------------ */

void BrFrameEnd(void)
{
    BrOsTask *pTask;
    uint32_t *pBase;
    int32_t   n;

    BrGfxPut(0xE9000000u, 0);   /* G_RDPFULLSYNC */
    BrGfxPut(0xB8000000u, 0);   /* G_ENDDL */

    pTask = &BrG_6C1588[BrG_6C65EC];

    pTask->type            = 1;
    pTask->ucode           = 0x118AB150u;
    pTask->ucode_data      = 0x118AB160u;
    pTask->flags           = 2;
    pTask->flags          |= 4;
    pTask->output_buff     = BrG_0AA728;
    pTask->output_buff_size =
        (uint32_t)(BrG_0AA72C - ((uint32_t)BrG_6C6668 << 3));
    pTask->ucode_size      = 0x1000;
    pTask->ucode_data_size = 0x800;
    /* `mov edx, 0x106C2D0F / and edx, ~0xF` -- a folded align-up of a
     * hardcoded address, i.e. the constant 0x106C2D00. */
    pTask->dram_stack      = 0x106C2D00u;
    pTask->dram_stack_size = 0x400;

    pBase = BrGfxDlBase();
    pTask->data_ptr  = (uintptr_t)pBase;
    /* Byte length rounded down to a multiple of 8 (sar 3 / shl 3). */
    pTask->data_size = (uint32_t)(((BrG_6C0680 - pBase) >> 1) << 3);

    n = (int32_t)((BrG_6C0680 - pBase) >> 1);
    if (n > BrG_6C6660) {
        BrG_6C6660 = n;
    }

    n = (int32_t)((BrG_363FF0 - BrG_2E5EC8) >> 3);
    if (n > BrG_6C6658) {
        BrG_6C6658 = n;
    }

    n = (int32_t)((BrG_364304 - BrG_3643BC) >> 5);
    if (n > BrG_6C665C) {
        BrG_6C665C = n;
    }

    BrG_6C1170 = (int32_t)((BrG_6C0680 - pBase) >> 1);
    if (BrG_6C1170 > BR_S18_GLIST_LIMIT) {
        BrFatal("HUGE GLIST ERROR");   /* 0x100AAB44 */
    }

    BrStub8B80_0();

    if (BrG_6C6664 != 0) {
        BrStub8B80_5i(0, 0, 0, 0, 0xFF);
        BrGfx42AF0_3(BrG_6C33A0, 0, 1);
        BrStub8B80_5i(0, 0xFF, 0xFF, 0, 0xFF);

        if (BrG_6C198C != NULL) {
            BrG_6C198C();
            BrG_6C198C = NULL;
        }

        BrStub8B80_5i(0, 0, 0, 0, 0xFF);
        BrGfx42AF0_3(BrG_6C3380, 0, 1);

        /* GOTCHA, faithful: the test is on BrG_6C1608 but the call and the
         * clear are both on BrG_6C198C -- which the block above has already
         * NULLed.  So this can only ever fire on the first pass, and only if
         * 0x106C1608 happens to be set.
         * DEVIATION: the NULL check is added; the original would jump to 0. */
        if (BrG_6C1608 != 0) {
            if (BrG_6C198C != NULL) {
                BrG_6C198C();
            }
            BrG_6C198C = NULL;
        }

        BrStub8B80_0();

        BrG_6C020C = BrTimeNow();
        BrG_6C1620 = BrG_6C020C - BrG_6C0208;

        if (BrG_6C65E0 != 0) {
            BrG_6C65E0 -= 1;
            if (BrG_6C65E0 == 0) {
                if (BrG_6C65E4 != 0) {
                    BrStub8B80_1p(BrG_0ADFC0 == 2 ? BrG_AA4020 : BrG_AA3760);
                } else {
                    BrStub8B80_1p(BrG_0ADFC0 == 2 ? BrG_AA3D50 : BrG_AA3490);
                }
                BrStub8B80_1i(1);
                BrG_6C65E8 = BrG_6C65E4;
            }
        }

        BrStub8B80_5i(1, 0x20, 0x20, 0x20, 0xFF);
        BrStub8B80_5i(2, 0x20, 0x20, 0x20, 0xFF);
        BrStub8B80_5i(0, 0x20, 0x20, 0x20, 0xFF);

        if (BrG_6C65F4 == 0 && BrG_6C65F8 == 0) {
            BrStub8B80_1i(0);
        }
        if (BrG_6C65F8 != 0) {
            BrG_6C65F8 -= 1;
        }

        BrGfx2C210();

        BrG_6C0208 = BrTimeNow();
        /* Note: 0x106C020C is REUSED here as a delta, overwriting the
         * timestamp it held two lines ago. */
        BrG_6C020C = BrTimeNow() - BrG_6C020C;
    } else {
        BrG_6C6664 += 1;
    }

    BrStub8B80_5i(0, 0xC8, 0, 0xC8, 0xFF);
    BrG_6C56E4 = BrG_6C6598;
    BrStub8B80_5i(2, 0xC8, 0, 0, 0xFF);
    BrStub8B80_5i(1, 0xC8, 0x64, 0, 0xFF);

    /* DEVIATION: NULL check added; the original calls through unconditionally. */
    if (BrG_B501D0 != NULL) {
        BrG_B501D0(pTask->data_ptr);
    }

    BrG_6C65EC ^= 1;
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_106ed674;
extern int DAT_106ed670;
extern int DAT_100aa044;
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_106e9a2c;
extern int DAT_106e7714;
extern int DAT_106e79d4;
extern int DAT_106ed67c;
extern int DAT_106e72e8;
extern int DAT_100aa020;
extern int DAT_106e7718;
extern int DAT_106e79b0;
extern char DAT_100a9ec0;
extern char DAT_100a9f00;
extern int DAT_106ed68c;
extern int DAT_100aa014;
extern int DAT_106ed694;
int FUN_10008d60();
int FUN_1002bf50();
int FUN_1001cf90();
int FUN_1002a8d7();
int FUN_100625f0();

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002BF4B glide BrNop_1002BF4B */

void BrNop_1002BF4B(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002C509 glide BrNop_1002C509 */

void BrNop_1002C509(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB3F glide BrNop_1002CB3F */

void BrNop_1002CB3F(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB44 glide BrNop_1002CB44 */

void BrNop_1002CB44(void)

{
  return;
}

/* WHAT IT DOES: build the frame-opening display list: reset the write pointer into this
 * frame's 96000-byte command buffer, then emit the fixed F3D-style preamble (segment,
 * sync, viewport via 0x1001CF90, othermode/geometry-mode settings, fog, the 0x28-stride
 * palette DL at 0x100A9F00) and the three mode pokes through 0x10008D60. The command
 * stream is a struct {op,arg} and every emit POST-INCREMENTS the global pointer -- that
 * is what puts each emit's temp in its own /Od stack slot and the two compiler temps
 * (switch selector, post-inc copy) at the frame bottom. */
/* @implements 0x1002B997 glide BrFrameBeginDl */

typedef struct BrDlCmd { int op; int arg; } BrDlCmd;
extern BrDlCmd *DAT_106e7710;

#define BR_EMIT(c,a) { \
  BrDlCmd *p_ = DAT_106e7710++; \
  p_->op = (c); \
  p_->arg = (a); }

void BrFrameBeginDl(int *param_1,int param_2)
{
  if (param_2 ^ DAT_106ed674) {
    DAT_106ed670 = 1;
    DAT_106ed674 = param_2;
  }
  FUN_10008d60(0,0,0x82,0,0xff);
  switch (DAT_100aa044) {
  case 1:
    *param_1 = 0;
    param_1[1] = 0;
    param_1[2] = DAT_100a7514;
    param_1[3] = DAT_100a7518;
    break;
  case 2:
    param_1[0x16] = 8;
    param_1[0x17] = (DAT_106e9a2c >> 1) + 1;
    param_1[0x18] = DAT_106e7714 + -0x60;
    param_1[0x19] = (DAT_106e9a2c >> 1) + -8;
    *param_1 = 8;
    param_1[1] = 8;
    param_1[2] = DAT_106e7714 + -0x60;
    param_1[3] = (DAT_106e9a2c >> 1) + -8;
    break;
  }
  FUN_1002a8d7();
  FUN_100625f0();
  DAT_106e7710 = (BrDlCmd *)(DAT_106e79d4 + DAT_106ed67c * 96000 + 0x200);
  DAT_106e72e8 = DAT_100aa020 ? 0x2000 : 0;
  DAT_106e7718 = 0x40;
  DAT_106e79b0 = 0;
  BR_EMIT(0xbc000006, 0)
  BR_EMIT(0xe7000000, 0)
  FUN_1002bf50(0,0,DAT_106e7714,DAT_106e9a2c);
  FUN_1001cf90(DAT_106e7710++,0,0,0,0x3eb,0,0,0,0x3eb,0,0,0,1000,0,0,0,1000);
  BR_EMIT(0xba001001, 0)
  BR_EMIT(0xba000e02, 0)
  BR_EMIT(0xba001102, 0)
  BR_EMIT(0xba001301, 0x80000)
  BR_EMIT(0xba000c02, DAT_106e72e8)
  BR_EMIT(0xba000903, 0xc00)
  BR_EMIT(0xba000801, 0)
  BR_EMIT(0xb9000002, 1)
  BR_EMIT(0xb900031d, 0xf0a4000)
  BR_EMIT(0xba000602, DAT_106e7718)
  BR_EMIT(0xba000602, DAT_106e79b0)
  BR_EMIT(0xba001402, 0)
  BR_EMIT(0xf9000000, 0)
  BR_EMIT(0x1020040, (int)&DAT_100a9ec0)
  BR_EMIT(0xb6000000, 0x1f3204)
  BR_EMIT(0xb7000000, 0x2000)
  if (DAT_100aa014 != 0) {
    BR_EMIT(0xb7000000, 0x800000)
  }
  else {
    BR_EMIT(0xb6000000, 0x800000)
  }
  BR_EMIT(0x6000000, (int)(&DAT_100a9f00 + DAT_106ed68c * 0x28))
  BR_EMIT(0xbb000000, 0)
  FUN_10008d60(0x40);
  FUN_10008d60(0x10);
  FUN_10008d60(DAT_106ed694 ? 1 : 2);
  return;
}

/* WHAT IT DOES: sets the clipping rectangle for everything drawn after it --
 * how split-screen halves and mirror insets are kept from spilling over each
 * other. The rectangle is trimmed to the screen bounds first (only the SIZE is
 * trimmed at the far edges, so a fully off-screen rectangle still emits a
 * zero-size one rather than being dropped), then doubled if the hi-res flag is
 * set, which can push it back outside.
 *
 * 0x1002BF50 -- /Od, and it belongs to THIS translation unit: 0x1002BF4B
 * (BrNop_1002BF4B, 5 bytes) ends exactly at 0x1002BF50. It was transcribed in
 * slice5_62.c, an /O2 file, where the unoptimised frame could never match; the
 * body is the same, only the home and the reload-everything spelling differ.
 *
 * The four float round-trips are the original's own: `fild [arg]` into a
 * float32 temp, `fld` it back, `fmul` the 0x100774B4 scale, then __ftol. That
 * is an explicit `(float)` cast in the source, and it is lossy above 2^24, so
 * it is kept rather than folded into a direct fild-and-scale. */
/* @implements 0x1002BF50 glide BrSub_1003289F */

extern int   DAT_104b16b0;   /* minimum X */
extern int   DAT_104b16a8;   /* maximum X */
extern int   DAT_104b16b4;   /* minimum Y */
extern int   DAT_104b16a4;   /* maximum Y */
extern int   DAT_106ed674;   /* hi-res: double every coordinate */
extern float DAT_100774b4;   /* the fixed-point scale */

void BrSub_1003289F(int param_1,int param_2,int param_3,int param_4)

{
  BrDlCmd *piVar1;

  if (param_1 < DAT_104b16b0) {
    param_3 = param_3 - (DAT_104b16b0 - param_1);
    param_1 = DAT_104b16b0;
  }
  if (param_1 + param_3 > DAT_104b16a8) {
    param_3 = DAT_104b16a8 - param_1;
  }
  if (param_3 < 0) {
    param_3 = 0;
  }
  if (param_2 < DAT_104b16b4) {
    param_4 = param_4 - (DAT_104b16b4 - param_2);
    param_2 = DAT_104b16b4;
  }
  if (param_2 + param_4 > DAT_104b16a4) {
    param_4 = DAT_104b16a4 - param_2;
  }
  if (param_4 < 0) {
    param_4 = 0;
  }
  if (DAT_106ed674 != 0) {
    param_1 = param_1 * 2;
    param_2 = param_2 * 2;
    param_3 = param_3 * 2;
    param_4 = param_4 * 2;
  }
  piVar1 = DAT_106e7710++;
  piVar1->op = 0xe7000000;
  piVar1->arg = 0;
  {
    BrDlCmd *piVar2 = DAT_106e7710++;
    piVar2->op = (((int)((float)param_1 * DAT_100774b4) & 0xfff) << 12)
               | 0xe2000000
               | ((int)((float)param_2 * DAT_100774b4) & 0xfff);
    piVar2->arg = (((int)((float)(param_1 + param_3) * DAT_100774b4) & 0xfff) << 12)
                | ((int)((float)(param_2 + param_4) * DAT_100774b4) & 0xfff);
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
