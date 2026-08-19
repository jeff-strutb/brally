#include "br_vecd.h"
#include <stdio.h>
#include <math.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BrVec3d a = {1.0, 2.0, 3.0}, b = {4.0, 5.0, 6.0};
    BrVec3d ux = {1,0,0}, uy = {0,1,0};

    check(BrVec3dDot(&a, &b) == 32.0, "dot of known vectors");
    check(BrVec3dDot(&ux, &uy) == 0.0, "orthogonal axes give zero");
    check(BrVec3dDot(&ux, &ux) == 1.0, "unit vector with itself");

    /* must be genuine double precision: this value is not representable in
     * float, so a float implementation would lose it */
    { BrVec3d p = {1.0 + 1e-12, 0, 0}, q = {1.0, 0, 0};
      check(BrVec3dDot(&p, &q) != 1.0, "retains double precision"); }

    /* summation order is (z*z + y*y) + x*x, not left-to-right */
    { BrVec3d s = {1e16, 1.0, -1e16}, t = {1.0, 1.0, 1.0};
      double got = BrVec3dDot(&s, &t);
      double zyx = (s.z * t.z + s.y * t.y) + s.x * t.x;
      check(got == zyx, "summation order matches the original"); }

    { BrVec3d v = {3.0, 4.0, 0.0};
      check(BrVec3dLenSq(&v) == 25.0, "length squared");
      check(BrVec3dLen(&v) == 5.0, "length"); }

    { BrVec3d v = {3.0, 4.0, 0.0};
      BrVec3dNormalise(&v);
      check(fabs(BrVec3dLen(&v) - 1.0) < 1e-12, "normalise gives unit length"); }

    /* exact-zero guard: a zero vector must survive untouched, not become NaN */
    { BrVec3d z = {0.0, 0.0, 0.0};
      BrVec3dNormalise(&z);
      check(z.x == 0.0 && z.y == 0.0 && z.z == 0.0, "zero vector left alone"); }

    /* cross: destination is the THIRD argument */
    { BrVec3d ux = {1,0,0}, uy = {0,1,0}, out = {9,9,9};
      BrVec3dCross(&ux, &uy, &out);
      check(out.x == 0 && out.y == 0 && out.z == 1,
            "cross(x,y) == +z with out as third arg"); }

    { BrVec3d out;
      BrVec3dCross(&a, &b, &out);
      check(BrVec3dDot(&out, &a) == 0.0 && BrVec3dDot(&out, &b) == 0.0,
            "cross result perpendicular to both inputs"); }

    check(BrPackNormalByte(0.0) == 0, "pack: 0 -> 0");
    check(BrPackNormalByte(1.0) == 127, "pack: +1 clamps to 127 (128 overflows)");
    check(BrPackNormalByte(-1.0) == -128, "pack: -1 -> -128");
    check(BrPackNormalByte(0.5) == 64, "pack: +0.5 -> 64");
    check(BrPackNormalByte(2.0) == 127, "pack: out of range clamps high");
    check(BrPackNormalByte(-9.0) == -128, "pack: out of range clamps low");
    /* halves round up (floor(x+0.5)), not to even */
    check(BrPackNormalByte(1.5 / 128.0) == 2, "pack: halves round up, not to even");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
