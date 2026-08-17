/* test_br_collrespsolve.c -- behavioural pins for the OBB collision response.
 *
 * The golden vectors below were produced by tools/x87emu.py executing the real
 * opcode stream of 0x10067470 out of orig/BRGlide.dll.  They are the original's
 * outputs, not this port's -- so a wrong transcription fails here, and a change
 * that "looks equivalent" but is not is caught.  Regenerate with the emulator,
 * never by copying this port's output back onto itself.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "br_collrespsolve.h"

static int g_fail;

#define CLOSE(a, b) do {                                                     \
    float _a = (a), _b = (b);                                               \
    if (fabsf(_a - _b) > 1e-5f) {                                           \
        printf("  FAIL %s:%d  %s=%.9g want %.9g (d=%.3e)\n",                \
               __FILE__, __LINE__, #a, _a, _b, fabsf(_a - _b));            \
        ++g_fail;                                                           \
    }                                                                      \
} while (0)

struct Case {
    int   mode;
    float ext[3], a[3], planeD, edgeN[3], verts[9];
    float wantOut[3], wantNormal[3];
};

/* {mode, ext, a, planeD, edgeN, verts, {out.xyz, normal.xyz}} */
static const struct Case CASES[] = {
{1, {0.4f,0.5f,0.6f}, {0.6f,0.5f,0.7f}, 0.3f, {0.1f,-0.2f,0.3f}, {0,0,0,0,0,0,0,0,0},
    {0.0780000091f,0.0650000051f,0.0910000056f}, {0,0,0}},
{2, {0.4f,0.5f,0.6f}, {0.6f,0.5f,0.7f}, 0.3f, {0,0,0}, {0.8f,0.1f,0.05f,0.7f,0.12f,0,0.75f,0.08f,0.1f},
    {-0.02999999f,-0.0249999911f,-0.0349999852f}, {0,0,0.5f}},
{2, {0.5f,0.5f,0.5f}, {1,0,0}, 0.0f, {0,0,0}, {0.1f,0.05f,0.8f,0.12f,0,0.7f,0.08f,0.1f,0.75f},
    {-0.0f,-0.0f,-0.0f}, {0,0.5f,0}},
{2, {0.695f,0.979f,0.499f}, {-0.734f,-0.827f,0.458f}, 0.062f, {0.401f,-0.82f,0.248f},
    {-0.601f,0.444f,0.177f,0.585f,0.577f,0.645f,0.723f,-0.575f,-0.077f},
    {-0.349017024f,-0.393238515f,0.217779011f}, {0,0.5f,0}},
{1, {1.305f,0.64f,1.431f}, {0.057f,0.549f,-0.062f}, -0.195f, {-0.461f,-0.985f,-0.592f},
    {0.979f,0.069f,-0.076f,0.8f,0.211f,0.953f,-0.285f,-0.568f,0.842f},
    {0.0191142671f,0.184100583f,-0.0207909569f}, {0,0,0}},
{1, {1.172f,0.774f,1.402f}, {-0.645f,0.006f,-0.648f}, -0.78f, {-0.658f,0.368f,-0.162f},
    {0.873f,0.264f,-0.714f,0.235f,-0.238f,-0.728f,-0.189f,0.8f,0.948f},
    {0.845978141f,-0.007869564f,0.849912941f}, {0,0,0}},
{1, {0.776f,0.909f,1.273f}, {-0.747f,0.883f,-0.048f}, -0.888f, {0.477f,0.437f,0.558f},
    {-0.699f,0.882f,-0.751f,-0.763f,-0.408f,0.982f,-0.18f,0.848f,0.761f},
    {0.665403724f,-0.786548197f,0.0427568667f}, {0,0,0}},
{2, {1.202f,1.237f,1.3f}, {-0.766f,0.645f,-0.628f}, -0.855f, {-0.425f,0.171f,-0.905f},
    {-0.567f,-0.202f,0.645f,-0.201f,-0.172f,-0.258f,-0.815f,0.759f,0.846f},
    {0.407894999f,-0.343462497f,0.334410042f}, {0,-0.5f,0}},
};

int main(void)
{
    unsigned i;
    for (i = 0; i < sizeof CASES / sizeof CASES[0]; ++i) {
        const struct Case *c = &CASES[i];
        BrVec3 ext, a, edgeN, verts[3];

        /* reset the shared bank the way the walker does before each candidate:
         * mode into modeFC, normal cleared (the != 2 path leaves it alone, so
         * clearing lets us assert it was NOT written). */
        memset(&g_brCrPlane, 0, sizeof g_brCrPlane);
        g_brCrPlane.modeFC = (uint32_t)c->mode;

        ext.x = c->ext[0]; ext.y = c->ext[1]; ext.z = c->ext[2];
        a.x = c->a[0]; a.y = c->a[1]; a.z = c->a[2];
        edgeN.x = c->edgeN[0]; edgeN.y = c->edgeN[1]; edgeN.z = c->edgeN[2];
        memcpy(verts, c->verts, sizeof verts);

        BrCrPlaneResolve(&ext, &a, c->planeD, &edgeN, verts);

        CLOSE(g_brCrPlane.out.x, c->wantOut[0]);
        CLOSE(g_brCrPlane.out.y, c->wantOut[1]);
        CLOSE(g_brCrPlane.out.z, c->wantOut[2]);
        CLOSE(g_brCrPlane.normal.x, c->wantNormal[0]);
        CLOSE(g_brCrPlane.normal.y, c->wantNormal[1]);
        CLOSE(g_brCrPlane.normal.z, c->wantNormal[2]);
    }

    if (g_fail) { printf("br_collrespsolve: %d FAILURES\n", g_fail); return 1; }
    printf("br_collrespsolve: all checks passed (0x10067470 vs x87emu golden vectors)\n");
    return 0;
}
