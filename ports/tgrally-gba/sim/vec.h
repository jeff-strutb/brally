/* vec.h -- geometry/vec.c's three-vector helpers, each summed in the game's order */
#ifndef VEC_H
#define VEC_H
#include "sim.h"

static inline fx BrVec3Dot(const fx *a, const fx *b) { return FMUL(b[2], a[2]) + (FMUL(a[0], b[0]) + FMUL(a[1], b[1])); }
static inline void BrVec3Sub(fx *o, const fx *a, const fx *b) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
static inline void BrVec3Add(fx *o, const fx *a, const fx *b) { o[0] = b[0] + a[0]; o[1] = b[1] + a[1]; o[2] = b[2] + a[2]; }
static inline void BrVec3AddTo(fx *a, const fx *b) { a[0] += b[0]; a[1] += b[1]; a[2] += b[2]; }
static inline void BrVec3SubFrom(fx *a, const fx *b) { a[0] -= b[0]; a[1] -= b[1]; a[2] -= b[2]; }
static inline void BrVec3Scale(fx *o, const fx *v, fx s) { o[0] = FMUL(v[0], s); o[1] = FMUL(v[1], s); o[2] = FMUL(v[2], s); }
static inline void BrVec3ScaleBy(fx *v, fx s) { v[0] = FMUL(v[0], s); v[1] = FMUL(v[1], s); v[2] = FMUL(v[2], s); }
static inline void BrVec3MulAdd(fx *o, const fx *a, const fx *b, fx s)
{
    o[0] = FMUL(b[0], s) + a[0];
    o[1] = FMUL(b[1], s) + a[1];
    o[2] = FMUL(b[2], s) + a[2];
}
static inline void BrVec3MulAddTo(fx *a, const fx *b, fx s) { a[0] += FMUL(b[0], s); a[1] += FMUL(b[1], s); a[2] += FMUL(b[2], s); }
static inline void BrVec3Lerp(fx *o, const fx *a, const fx *b, fx t)
{
    o[0] = b[0] + FMUL(a[0] - b[0], t);
    o[1] = b[1] + FMUL(a[1] - b[1], t);
    o[2] = b[2] + FMUL(a[2] - b[2], t);
}
static inline void BrVec3Negate(fx *o, const fx *v) { o[0] = -v[0]; o[1] = -v[1]; o[2] = -v[2]; }
static inline void BrVec3Cross(fx *o, const fx *a, const fx *b)
{
    fx z = FMUL(a[0], b[1]) - FMUL(b[0], a[1]);
    fx y = FMUL(a[2], b[0]) - FMUL(b[2], a[0]);
    o[0] = FMUL(a[1], b[2]) - FMUL(b[1], a[2]);
    o[1] = y;
    o[2] = z;
}
static inline fx BrVec3Length(const fx *v) { return FSQRT(FMUL(v[2], v[2]) + (FMUL(v[0], v[0]) + FMUL(v[1], v[1]))); }
static inline fx BrVec3Dist(const fx *a, const fx *b)
{
    fx dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return FSQRT(FMUL(dx, dx) + FMUL(dy, dy) + FMUL(dz, dz));
}
static inline void BrVec3Div(fx *o, const fx *v, fx s)
{
    s = FDIV(FX(1.0f), s);
    o[0] = FMUL(v[0], s);
    o[1] = FMUL(v[1], s);
    o[2] = FMUL(v[2], s);
}
static inline void BrVec3DivBy(fx *v, fx s) { BrVec3Div(v, v, s); }
static inline void BrVec3Normalise(fx *v)
{
    fx len = FSQRT(FMUL(v[2], v[2]) + (FMUL(v[0], v[0]) + FMUL(v[1], v[1])));
    if (len != FX(0.0f)) {
        len = FDIV(FX(1.0f), len);
        v[0] = FMUL(v[0], len);
        v[1] = FMUL(v[1], len);
        v[2] = FMUL(v[2], len);
    } else {
        v[0] = FX(0.0f);
        v[1] = FX(0.0f);
        v[2] = FX(1.0f);
    }
}
#endif
