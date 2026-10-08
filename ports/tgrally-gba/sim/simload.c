/* simload.c -- a car from the game's own record of it (BrCar, 0x2090 bytes, and its
 * pad record, big-endian as the console holds them): each field the simulation
 * keeps, at its game offset.  The GBA starts a race from the cars the game set on
 * the grid (tools/race.py keeps their records); host/simcheck.c loads and compares
 * through the same tables. */
#include <stddef.h>
#include "simload.h"

uint32_t sl_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
uint16_t sl_be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

/* a float's bits to the simulation's number (the fixed build without a floating-point unit) */
fx sl_fx(uint32_t bits)
{
#ifdef FX_FLOAT
    union { uint32_t u; float f; } x;
    x.u = bits;
    return x.f;
#else
    int e = (int)(bits >> 23 & 0xFF), sh;
    int64_t m;
    if (e == 0)
        return 0;
    m = (int64_t)((bits & 0x7FFFFF) | 0x800000);
    sh = e - 118;                        /* m * 2^(e - 150), as 32.32 */
    if (sh >= 39)
        m = INT64_MAX >> 1;
    else if (sh >= 0)
        m <<= sh;
    else if (sh > -64)
        m >>= -sh;
    else
        m = 0;
    return bits >> 31 ? -m : m;
#endif
}

#define BF(off, t, m, n) { off, t, offsetof(Body, m), n, #m }
const Field sl_body[] = {
    BF(0x1C, SL_I32, kind, 1), BF(0x20, SL_F32, w, 1), BF(0x24, SL_F32, h, 1), BF(0x28, SL_F32, d, 1),
    BF(0x2C, SL_F32, mass, 1), BF(0x30, SL_F32, I, 9), BF(0x54, SL_F32, Iinv, 9), BF(0x78, SL_F32, st, 17),
    BF(0xBC, SL_F32, m, 16), BF(0xFC, SL_F32, force, 3), BF(0x108, SL_F32, torque, 3), BF(0x114, SL_F32, stA, 17),
    BF(0x158, SL_F32, stB, 17), BF(0x19C, SL_HIT, hit, 1), BF(0x1A0, SL_U8, surface, 1), BF(0x1A4, SL_F32, hitN, 4),
    BF(0x1B4, SL_I32, x1b4, 1), BF(0x1B8, SL_F32, spring, 1), BF(0x1BC, SL_F32, damper, 1), BF(0x1C0, SL_F32, steer, 1),
    BF(0x1C4, SL_F32, spin, 1), BF(0x1C8, SL_F32, inertia, 1), BF(0x1CC, SL_F32, drive, 1), BF(0x1D0, SL_F32, brake, 1),
    BF(0x1D4, SL_F32, angle, 1), BF(0x1D8, SL_F32, depth, 1), BF(0x1DC, SL_F32, box, 4), BF(0x1EC, SL_F32, hitN2, 3),
    BF(0x1F8, SL_I32, stuck, 1), BF(0x1FC, SL_U8, hitSize, 1), BF(0x1FD, SL_U8, tyres, 1), BF(0x1FF, SL_U8, hitPeak, 1),
    BF(0x200, SL_U8, idle, 1), BF(0x203, SL_U8, landed, 1), BF(0x204, SL_U8, slide, 1),
};
const int sl_nbody = sizeof sl_body / sizeof sl_body[0];
#define CF(off, t, m, n) { off, t, offsetof(Car, m), n, #m }
const Field sl_car[] = {
    CF(0x000, SL_F32, mtx0, 16), CF(0x040, SL_F32, wheelMtx, 64), CF(0x140, SL_I32, slot, 1),
    CF(0xDF0, SL_F32, xdf0, 1), CF(0xDF4, SL_F32, xdf4, 1), CF(0xDF8, SL_F32, ratio, 7), CF(0xE14, SL_F32, torque, 4),
    CF(0xE24, SL_F32, xe24, 1), CF(0xE28, SL_I32, gears, 1), CF(0xE2C, SL_I32, frontDrive, 1), CF(0xE30, SL_I32, automatic, 1),
    CF(0xE34, SL_I32, xe34, 1), CF(0xE38, SL_F32, xe38, 1), CF(0xE3C, SL_F32, xe3c, 1), CF(0xE40, SL_I32, gear, 1),
    CF(0xE44, SL_F32, gripR, 1), CF(0xE48, SL_U8, slipR, 1), CF(0xE4C, SL_F32, gripF, 1), CF(0xE50, SL_U8, slipF, 1),
    CF(0xE51, SL_S8, slew, 1), CF(0xE54, SL_I32, firstFrame, 1), CF(0xE68, SL_I32, handling, 1), CF(0xF4C, SL_I32, fly, 1),
    CF(0xFA8, SL_F32, xfa8, 1), CF(0xFAC, SL_I32, xfac, 1), CF(0xFD4, SL_I32, groundHits, 1), CF(0xFE4, SL_F32, speedMph, 1),
    CF(0x1DD4, SL_F32, flyRate, 4), CF(0x1FC0, SL_U16, nearIds, 32), CF(0x2000, SL_I32, gotHit, 1),
    CF(0x2004, SL_U16, farIds, 32), CF(0x2044, SL_I32, farCount, 1), CF(0x2048, SL_F32, groundDist, 1),
    CF(0x204C, SL_I32, groundFace, 1), CF(0x346, SL_U8, sndHitA, 1), CF(0x349, SL_U8, hitAge, 1), CF(0x2063, SL_U8, fade, 1),
    CF(0xF48, SL_I32, camMode, 1), CF(0x1DF0, SL_F32, cams, 4 * 17), CF(0x1F44, SL_F32, cam4, 17),
    CF(0x1F88, SL_F32, camSpeed, 3), CF(0x1F94, SL_F32, camTarget, 3), CF(0x1FA0, SL_F32, camPosA, 3),
    CF(0x1FAC, SL_F32, x1fac, 1), CF(0x1FB4, SL_F32, camPosB, 3),
};
const int sl_ncar = sizeof sl_car / sizeof sl_car[0];
const int sl_bodies[5] = { 0x148, 0x350, 0x558, 0x760, 0x968 };

void sl_field_load(const Field *f, uint8_t *obj, const uint8_t *rec)
{
    int i;
    for (i = 0; i < f->n; i++) {
        switch (f->type) {
        case SL_F32: ((fx *)(obj + f->at))[i] = sl_fx(sl_be32(rec + f->off + i * 4)); break;
        case SL_I32: ((int *)(obj + f->at))[i] = (int)sl_be32(rec + f->off + i * 4); break;
        case SL_U8: ((uint8_t *)(obj + f->at))[i] = rec[f->off + i]; break;
        case SL_S8: ((int8_t *)(obj + f->at))[i] = (int8_t)rec[f->off + i]; break;
        case SL_U16: ((uint16_t *)(obj + f->at))[i] = sl_be16(rec + f->off + i * 2); break;
        case SL_HIT: *(const Plane **)(obj + f->at) = sl_be32(rec + f->off) ? &g_track->planes[0] : 0; break;
        }
    }
}

void sl_car_load(Car *c, Pad *pad, const uint8_t *rec, const uint8_t *padrec, uint32_t linkFlags)
{
    int i, k;
    for (i = 0; i < sl_ncar; i++)
        sl_field_load(&sl_car[i], (uint8_t *)c, rec);
    for (k = 0; k < 5; k++) {
        Body *b = k ? &c->wb[k - 1] : &c->body;
        for (i = 0; i < sl_nbody; i++)
            sl_field_load(&sl_body[i], (uint8_t *)b, rec + sl_bodies[k]);
    }
    for (k = 0; k < 16; k++) {
        c->force[k].frame = (int)sl_be32(rec + 0xB70 + k * 0x20 + 4);
        for (i = 0; i < 3; i++) {
            c->force[k].f[i] = sl_fx(sl_be32(rec + 0xB70 + k * 0x20 + 8 + i * 4));
            c->force[k].at[i] = sl_fx(sl_be32(rec + 0xB70 + k * 0x20 + 0x14 + i * 4));
        }
    }
    c->linkFlags = linkFlags;
    c->cam = (int)(sl_be32(rec + 0x1DE8) - 0x8031B760u - (uint32_t)c->slot * 0x2090 - 0x1DF0) / 0x44;
#ifndef FX_FLOAT
    {   /* the torque curve in thousands of rpm: each coefficient's exponent raised to match */
        static const int raise[4] = { 30, 20, 10, 0 };   /* x 2^30, 2^20, 2^10: then x 1e9 / 2^30 .. */
        static const fx fix[4] = { FX(1e9 / 1073741824.0), FX(1e6 / 1048576.0), FX(1e3 / 1024.0), FX(1.0) };
        for (k = 0; k < 4; k++) {
            uint32_t bits = sl_be32(rec + 0xE14 + k * 4);
            if (bits & 0x7F800000)
                bits += (uint32_t)raise[k] << 23;
            c->torqueK[k] = FMUL(sl_fx(bits), fix[k]);
        }
    }
#endif
    pad->flags = sl_be32(padrec);
    pad->throttle = sl_fx(sl_be32(padrec + 0x1C));
    pad->steer = sl_fx(sl_be32(padrec + 0x20));
    pad->kind = padrec[0x25];
    c->pad = pad;
    sim_car_link(c);
}
