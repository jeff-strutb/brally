/* test_mat.c -- verify the decompiled matrix routines. */
#include "br_mat.h"
#include <stdio.h>
#include <math.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }
static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

int main(void)
{
    BrMat4 id = {{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
    /* 90 degrees about Z: x -> y, y -> -x */
    BrMat4 rz = {{{0,-1,0,0},{1,0,0,0},{0,0,1,0},{0,0,0,1}}};
    BrVec3 v = {1.0f, 2.0f, 3.0f}, r, r2;

    BrMat4MulVec3(&r, &id, &v);
    check(near(r.x,1) && near(r.y,2) && near(r.z,3), "identity is a no-op");

    { BrVec3 ux = {1,0,0};
      BrMat4MulVec3(&r, &rz, &ux);
      check(near(r.x,0) && near(r.y,1) && near(r.z,0),
            "row-major: Rz * x == +y"); }

    /* transpose of a rotation is its inverse: M^T (M v) == v */
    BrMat4MulVec3(&r, &rz, &v);
    BrMat4MulVec3Transposed(&r2, &rz, &r);
    check(near(r2.x,v.x) && near(r2.y,v.y) && near(r2.z,v.z),
          "transposed multiply inverts the rotation");

    /* the two must genuinely differ for a non-symmetric matrix */
    BrMat4MulVec3(&r, &rz, &v);
    BrMat4MulVec3Transposed(&r2, &rz, &v);
    check(!(near(r.x,r2.x) && near(r.y,r2.y)),
          "transposed form is not identical to the plain form");

    /* the fourth column must be ignored, not read */
    { BrMat4 junk = rz; int i;
      for (i = 0; i < 4; i++) junk.m[i][3] = 1e9f;
      BrMat4MulVec3(&r2, &junk, &v);
      BrMat4MulVec3(&r, &rz, &v);
      check(near(r.x,r2.x) && near(r.y,r2.y) && near(r.z,r2.z),
            "fourth column ignored"); }

    /* copy: source first. If the order were flipped this would copy the
     * uninitialised destination over the source and the check would fail. */
    { BrMat4 dst; int i, k; float *p = &dst.m[0][0];
      for (i = 0; i < 16; i++) p[i] = -1.0f;
      BrMat4Copy(&rz, &dst);
      { int ok = 1;
        for (i = 0; i < 4; i++) for (k = 0; k < 4; k++)
            if (dst.m[i][k] != rz.m[i][k]) ok = 0;
        check(ok, "copy is source-first and copies all 16 elements"); } }

    { BrMat4 m; BrVec3 out;
      BrMat4Identity(&m);
      BrMat4MulVec3(&out, &m, &v);
      check(near(out.x,v.x) && near(out.y,v.y) && near(out.z,v.z),
            "identity built by BrMat4Identity is a no-op"); }

    /* ---- frustum ---- */
    { BrMat4 fm; float n = 1.0f, fa = 100.0f;
      check(BrMat4Frustum(&fm, -2, 2, -1, 1, n, fa) == 0, "frustum builds");
      check(fm.m[2][3] == -1.0f, "[2][3] is the hardcoded -1");
      check(near(fm.m[0][0], 2*n/4.0f), "[0][0] = 2n/(r-l)");
      check(near(fm.m[1][1], 2*n/2.0f), "[1][1] = 2n/(t-b)");
      check(fm.m[2][0] == 0.0f && fm.m[2][1] == 0.0f,
            "symmetric frustum has no off-axis skew");
      check(fm.m[0][1]==0 && fm.m[1][0]==0 && fm.m[3][3]==0,
            "zero slots are zero");

      /* row-vector convention: near plane must map to NDC z = -1, far to +1 */
      { float z, w;
        z = -n * fm.m[2][2] + fm.m[3][2];  w = -(-n);
        check(near(z / w, -1.0f), "near plane maps to NDC -1");
        z = -fa * fm.m[2][2] + fm.m[3][2]; w = -(-fa);
        check(near(z / w, 1.0f), "far plane maps to NDC +1"); }

      /* asymmetric frustum must produce skew terms */
      { BrMat4 am; BrMat4Frustum(&am, 0, 4, -1, 1, n, fa);
        check(am.m[2][0] != 0.0f, "asymmetric frustum skews [2][0]"); }

      /* degenerate inputs must be rejected and leave the matrix alone */
      { BrMat4 keep; BrMat4Copy(&fm, &keep);
        check(BrMat4Frustum(&fm, 1, 1, -1, 1, n, fa) != 0, "l==r rejected");
        check(BrMat4Frustum(&fm, -1, 1, 2, 2, n, fa) != 0, "b==t rejected");
        check(BrMat4Frustum(&fm, -1, 1, -1, 1, 5, 5) != 0, "n==f rejected");
        check(fm.m[0][0] == keep.m[0][0], "rejected call leaves matrix intact"); } }

    /* ---- perspective ---- */
    { BrMat4 pm; unsigned short pn = 0; float n = 1.0f, fa = 100.0f;
      check(BrMat4Perspective(&pm, &pn, 90.0f, 1.0f, n, fa) == 0,
            "perspective builds");
      check(pn == 1, "perspNorm is the hardcoded 1");
      /* at 90 deg vertical FOV with aspect 1, tan(45) = 1 so h = w = n,
       * giving [0][0] = [1][1] = 2n/(2n) = 1 */
      check(near(pm.m[1][1], 1.0f), "90 deg FOV gives [1][1] == 1");
      check(near(pm.m[0][0], 1.0f), "aspect 1 gives [0][0] == [1][1]");
      check(pm.m[2][3] == -1.0f, "delegates to the frustum layout");

      /* aspect widens x only */
      { BrMat4 wm; BrMat4Perspective(&wm, &pn, 90.0f, 2.0f, n, fa);
        check(near(wm.m[1][1], 1.0f), "aspect leaves [1][1] alone");
        check(near(wm.m[0][0], 0.5f), "aspect 2 halves [0][0]"); }

      /* narrower FOV must magnify */
      { BrMat4 nm; BrMat4Perspective(&nm, &pn, 45.0f, 1.0f, n, fa);
        check(nm.m[1][1] > pm.m[1][1], "narrower FOV magnifies"); } }

    { BrMat4 sm; BrVec3 sv = {1,2,3}, so;
      BrMat4Scale(&sm, 2.0f, 3.0f, 4.0f);
      check(sm.m[3][3] == 1.0f, "scale matrix has hardcoded 1 at [3][3]");
      check(sm.m[0][1]==0 && sm.m[2][0]==0, "off-diagonal is zero");
      BrMat4MulVec3(&so, &sm, &sv);
      check(near(so.x,2) && near(so.y,6) && near(so.z,12), "scale applies per axis");
      BrMat4Scale(&sm, 1,1,1);
      BrMat4MulVec3(&so, &sm, &sv);
      check(near(so.x,1) && near(so.y,2) && near(so.z,3), "unit scale is identity"); }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
