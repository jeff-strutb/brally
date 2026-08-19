/* test_vec.c -- verify the decompiled vector math by mathematical identity. */
#include "br_vec.h"
#include <stdio.h>
#include <math.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }
static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }
static int veq(const BrVec3 *v, float x, float y, float z)
{ return near(v->x, x) && near(v->y, y) && near(v->z, z); }

int main(void)
{
    BrVec3 a = { 1.0f, 2.0f, 3.0f };
    BrVec3 b = { 4.0f, 5.0f, 6.0f };
    BrVec3 r, s;

    /* handedness: x cross y must be +z, not -z. This is the thing an FPU
     * mis-trace gets wrong, so it is the assertion that matters most. */
    { BrVec3 ux = {1,0,0}, uy = {0,1,0};
      BrVec3Cross(&r, &ux, &uy);
      check(veq(&r, 0, 0, 1), "cross(x,y) == +z (right-handed)"); }

    BrVec3Cross(&r, &a, &b);
    check(veq(&r, -3.0f, 6.0f, -3.0f), "cross of known vectors");
    check(near(BrVec3Dot(&r, &a), 0.0f) && near(BrVec3Dot(&r, &b), 0.0f),
          "cross is perpendicular to both inputs");

    /* aliasing must be safe -- the original buffers components too */
    r = a; BrVec3Cross(&r, &r, &b);
    check(veq(&r, -3.0f, 6.0f, -3.0f), "cross tolerates out aliasing a");

    check(near(BrVec3Dot(&a, &b), 32.0f), "dot");

    BrVec3Sub(&r, &b, &a);
    check(veq(&r, 3, 3, 3), "sub");

    r = a; BrVec3AddTo(&r, &b);
    check(veq(&r, 5, 7, 9), "add-to");

    BrVec3Scale(&r, &a, 2.0f);
    check(veq(&r, 2, 4, 6), "scale");

    r = a; BrVec3ScaleBy(&r, 3.0f);
    check(veq(&r, 3, 6, 9), "scale-by");

    BrVec3MulAdd(&r, &a, &b, 2.0f);
    check(veq(&r, 9, 12, 15), "mul-add: a + b*s");

    r = a; BrVec3MulAddTo(&r, &b, 2.0f);
    check(veq(&r, 9, 12, 15), "mul-add-to matches mul-add");

    /* lerp endpoints: the original computes (a-b)*t+b, so t=0 gives b */
    BrVec3Lerp(&r, &a, &b, 0.0f);
    check(veq(&r, 4, 5, 6), "lerp t=0 yields b");
    BrVec3Lerp(&r, &a, &b, 1.0f);
    check(veq(&r, 1, 2, 3), "lerp t=1 yields a");
    BrVec3Lerp(&r, &a, &b, 0.5f);
    BrVec3Sub(&s, &a, &b); BrVec3ScaleBy(&s, 0.5f); BrVec3AddTo(&s, &b);
    check(veq(&r, s.x, s.y, s.z), "lerp midpoint consistent");

    BrVec3Negate(&r, &a);
    check(veq(&r, -1, -2, -3), "negate");

    BrVec3Add(&r, &a, &b);
    check(veq(&r, 5, 7, 9), "add");

    r = b; BrVec3SubFrom(&r, &a);
    check(veq(&r, 3, 3, 3), "sub-from");

    BrVec3Div(&r, &b, 2.0f);
    check(veq(&r, 2.0f, 2.5f, 3.0f), "div");

    r = b; BrVec3DivBy(&r, 2.0f);
    check(veq(&r, 2.0f, 2.5f, 3.0f), "div-by matches div");

    /* reciprocal-multiply must match the original bit-for-bit, not merely
     * approximately: three divides would round differently */
    { float s = 3.0f; BrVec3 q; BrVec3Div(&q, &a, s);
      check(q.x == a.x * (1.0f / s), "div uses reciprocal-multiply"); }

    BrVec3Midpoint(&r, &a, &b);
    check(veq(&r, 2.5f, 3.5f, 4.5f), "midpoint");

    BrVec3Zero(&r);
    check(veq(&r, 0, 0, 0), "zero");

    check(near(BrVec3DistSq(&a, &b), 27.0f), "dist-squared");
    { BrVec3 z = {0,0,0};
      check(near(BrVec3DistSq(&a, &z), BrVec3Dot(&a, &a)),
            "dist-squared from origin equals dot with self"); }

    { BrVec3 p = {0,0,0}, q = {3,4,0};
      check(near(BrVec3Dist(&p, &q), 5.0f), "distance is 3-4-5");
      check(near(BrVec3Dist(&p,&q)*BrVec3Dist(&p,&q), BrVec3DistSq(&p,&q)),
            "distance squared matches DistSq");
      check(BrVec3Dist(&p, &p) == 0.0f, "distance to self is zero"); }

    { BrVec3 v = {3.0f, 4.0f, 0.0f}, z = {0,0,0};
      check(near(BrVec3Length(&v), 5.0f), "length is 3-4-5");
      check(BrVec3Length(&z) == 0.0f, "length of zero vector is zero");
      check(near(BrVec3Length(&v)*BrVec3Length(&v), BrVec3Dot(&v,&v)),
            "length squared agrees with dot-with-self"); }

    /* 0x1003B170 keeps all three squares and both partial sums in the x87
     * registers and rounds ONCE, at `fstp dword [esp]` (0x1003B1A2), before
     * the fsqrt wrapper.  A transcription that rounds to float32 after every
     * operation loses each square to underflow separately, and the three
     * cases below are where that is visible.  Exact equality, not `near`:
     * these pin a rounding decision, and a tolerance would hide it.
     *
     * Both expected values are sqrtf(0x1p-149f) -- the sum of squares rounds
     * to exactly one float32 denormal step in each case. */
    { BrVec3 a = {2e-23f, 2e-23f, 2e-23f};   /* per-step: each square -> 0   */
      BrVec3 b = {3e-23f, 3e-23f, 0.0f};     /* per-step: each square -> 1ulp */
      BrVec3 c = {1e-23f, 0.0f, 0.0f};       /* underflows either way        */
      check(BrVec3Length(&a) == 0x1.6a09e6p-75f,
            "length rounds once: (2e-23)^3 is 3.74339207e-23, not 0");
      check(BrVec3Length(&b) == 0x1.6a09e6p-75f,
            "length rounds once: (3e-23,3e-23,0) is 3.74339207e-23, "
            "not 5.29395592e-23");
      check(BrVec3Length(&c) == 0.0f,
            "the single round is to float32, so 1e-23 still underflows"); }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
