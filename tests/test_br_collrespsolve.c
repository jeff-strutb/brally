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
#include "br_collresp.h"   /* BrCollPlane/Node, g_pBrCollRespList (walker test) */
#include "slice1_09.h"
#include "slice3_42.h"
#include "slice2_11.h"

static int g_fail;

/* slice3_44.o / br_collresp.o bundle helpers this suite never calls; their
 * cross-module references must still resolve.  Inert stand-ins -- TEST ONLY. */
void BrStub8B80_1p(const void *p0) { (void)p0; }
void BrGbiCall10075330(void *pv) { (void)pv; }
void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{ (void)pM; (void)pV; pOut->x = pOut->y = pOut->z = 0.0f; }
/* br_collresp.o refs not on the BrCrTest path the walker uses */
BrCollPlane *g_pBrCollGrid = 0;
const uint16_t *g_pBrCollGridCount = 0;
short BrCollGridCellAcquire(float x, float y) { (void)x; (void)y; return -1; }
float BrPlaneEval(const BrVec3 *pN, float d, const BrVec3 *pP)
{ (void)pN; (void)d; (void)pP; return 0.0f; }
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pP)
{ (void)pB; (void)pP; pOut->x = pOut->y = pOut->z = 0.0f; }

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

/* ---- 0x10065C80 impulse solver ---------------------------------------- *
 * Golden vectors below are the ORIGINAL's outputs, produced by tools/x87emu.py
 * executing 0x10065C80's real opcode stream over well-conditioned physical
 * inputs (orthonormal orientation, symmetric positive inverse inertia).  See
 * gen_golden.py.  The wider random equivalence sweep (>14000 cases, both paths)
 * lives in that harness; these pin representative branches in-tree. */
struct SolveCase {
    float mass, W[9], M4[16], vel[3], ang[3], N[3], a3[3];
    int   flag, thr, peakIn;
    float arg5;
    int   wantRet;
    float wantVel[3], wantAng[3];
    int   wantB1fc, wantB1ff;
};

static const struct SolveCase SOLVE_CASES[] = {
/* separating (ret 0) */
{ 1000.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, {0,3,0}, {0,0,0}, {0,1,0}, {0,1,0}, 1,0,0,0.0f,
  0, {0,3,0}, {0,0,0}, 0, 0 },
/* flat ground, flag0 */
{ 1000.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, {0.4f,-5,0.2f}, {0.1f,0,0.1f}, {0,1,0}, {0,1,0}, 0,0,0,0.0f,
  1, {0.400000006f,0.249999255f,0.200000003f}, {0.100000001f,0,0.100000001f}, 5, 0 },
/* flat ground, flag1 */
{ 1000.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, {0.4f,-5,0.2f}, {0.1f,0,0.1f}, {0,1,0}, {0,1,0}, 1,0,0,0.0f,
  1, {0.366068214f,0.249999255f,0.170592457f}, {0.0664075464f,-0.00192280044f,0.129068226f}, 5, 0 },
/* angled hit, rotated body */
{ 850.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {0.860089338f,-0.17434874f,-0.479425539f,0, 0.0509402927f,0.964440821f,-0.25934338f,0,
   0.507593752f,0.198636399f,0.838386644f,0, 0,0,0,1},
  {-2,-4,1.5f}, {0.3f,-0.2f,0.1f}, {0.267f,0.535f,0.802f}, {0.267f,0.535f,0.802f}, 1,0,0,0.0f,
  1, {-1.09996414f,-2.78904486f,2.05025983f}, {0.126357257f,-0.259596437f,0.319907457f}, 1, 0 },
/* effect path (thr 30) */
{ 1000.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, {1,-6,0.5f}, {0.2f,-0.1f,0.05f}, {0,1,0}, {0,1,0}, 1,30,0,0.0f,
  1, {0.903859973f,-0.330001146f,0.437678635f}, {0.130021363f,-0.103347935f,0.133409947f}, 6, 156 },
/* effect path, peak_in 150 */
{ 1200.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {0.860089338f,-0.17434874f,-0.479425539f,0, 0.0509402927f,0.964440821f,-0.25934338f,0,
   0.507593752f,0.198636399f,0.838386644f,0, 0,0,0,1},
  {-1.5f,-5,2}, {0.1f,0.2f,-0.1f}, {0.267f,0.535f,0.802f}, {0.267f,0.535f,0.802f}, 1,40,150,0.0f,
  1, {-0.625100255f,-3.75034404f,2.59965539f}, {-0.135536164f,0.0174314287f,0.269673347f}, 1, 150 },
/* light hit thr 11 flag0 */
{ 700.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {0.860089338f,-0.17434874f,-0.479425539f,0, 0.0509402927f,0.964440821f,-0.25934338f,0,
   0.507593752f,0.198636399f,0.838386644f,0, 0,0,0,1},
  {0.5f,-3,-1}, {0.05f,0.1f,0}, {0.267f,0.535f,0.802f}, {0.267f,0.535f,0.802f}, 0,11,0,0.0f,
  1, {1.41233444f,-1.8624171f,0.241841868f}, {0.263745457f,-0.45437637f,0.169777066f}, 2, 138 },
/* high-speed impact -- |approach| > 27 exercises the intensity clamp and the
 * saturating peak byte */
{ 1000.0f, {0.0012f,0.0001f,5e-05f,0.0001f,0.0018f,3e-05f,5e-05f,3e-05f,0.0009f},
  {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, {3,-60,2}, {0.2f,-0.1f,0.05f}, {0,1,0}, {0,1,0}, 1,30,0,0.0f,
  1, {2.70140028f,-3.30000257f,1.80421364f}, {-0.020013649f,-0.110620648f,0.308950335f}, 27, 255 },
};

/* relative-tolerant compare -- the emulator carries doubles and rounds to f32
 * on store; the port is f32 throughout, so agreement is to a few ulPS of the
 * magnitude, not bit-exact. */
#define RCLOSE(a, b) do {                                                    \
    float _a = (a), _b = (b), _s = fabsf(_b) > 1.0f ? fabsf(_b) : 1.0f;      \
    if (fabsf(_a - _b) > 2e-4f * _s) {                                       \
        printf("  FAIL %s:%d  %s=%.9g want %.9g (d=%.3e)\n",                 \
               __FILE__, __LINE__, #a, _a, _b, fabsf(_a - _b));             \
        ++g_fail;                                                           \
    }                                                                      \
} while (0)

static void run_solver_cases(void)
{
    unsigned i;
    for (i = 0; i < sizeof SOLVE_CASES / sizeof SOLVE_CASES[0]; ++i) {
        const struct SolveCase *c = &SOLVE_CASES[i];
        BrMat3 W;
        BrMat4 M;
        BrVec3 vel, ang, N, a3;
        BrCrEffect eff;
        int ret;

        memcpy(W.m, c->W, sizeof W.m);
        memcpy(M.m, c->M4, sizeof M.m);
        vel.x = c->vel[0]; vel.y = c->vel[1]; vel.z = c->vel[2];
        ang.x = c->ang[0]; ang.y = c->ang[1]; ang.z = c->ang[2];
        N.x = c->N[0]; N.y = c->N[1]; N.z = c->N[2];
        a3.x = c->a3[0]; a3.y = c->a3[1]; a3.z = c->a3[2];
        memset(&eff, 0, sizeof eff);
        eff.threshold = (uint8_t)c->thr;
        eff.peak      = (uint8_t)c->peakIn;

        ret = BrCrImpulseSolve(c->mass, &W, &M, &vel, &ang, &N, &a3,
                               c->flag, c->arg5, &eff);

        if (ret != c->wantRet) {
            printf("  FAIL case %u: ret=%d want %d\n", i, ret, c->wantRet);
            ++g_fail;
        }
        RCLOSE(vel.x, c->wantVel[0]);
        RCLOSE(vel.y, c->wantVel[1]);
        RCLOSE(vel.z, c->wantVel[2]);
        RCLOSE(ang.x, c->wantAng[0]);
        RCLOSE(ang.y, c->wantAng[1]);
        RCLOSE(ang.z, c->wantAng[2]);
        if (ret) {
            /* effect bytes are exact; colour is the normal's dwords verbatim */
            uint32_t nbits[3];
            memcpy(nbits, &N, sizeof nbits);
            if (eff.intensity != (uint8_t)c->wantB1fc) {
                printf("  FAIL case %u: intensity=%u want %d\n",
                       i, eff.intensity, c->wantB1fc); ++g_fail;
            }
            if (eff.peak != (uint8_t)c->wantB1ff) {
                printf("  FAIL case %u: peak=%u want %d\n",
                       i, eff.peak, c->wantB1ff); ++g_fail;
            }
            if (eff.color[0] != nbits[0] || eff.color[1] != nbits[1] ||
                eff.color[2] != nbits[2]) {
                printf("  FAIL case %u: colour != normal bits\n", i); ++g_fail;
            }
        }
    }
}

/* ---- 0x10065980 contact kick ------------------------------------------ *
 * Golden vectors are the original's outputs from tools/x87emu.py over the same
 * opcode stream; colour source is the shared bank g_brCrPlane.normal, set to
 * (1.0, 0.5, 0.25) below.  wantB1fc/ff == -1 means "not written" (threshold<10).
 * The wider random sweep (6000 cases) lives in the harness. */
struct KickCase {
    float vel[3], ang[3], N[3];
    int   dampFlag, spinFlag, thr, peakIn;
    int   wantRet;
    float wantVel[3], wantAng[3];
    int   wantB1fc, wantB1ff;
};

static const struct KickCase KICK_CASES[] = {
/* separating (ret 0) */
{ {0,3,0}, {0.1f,-0.2f,0.3f}, {0,1,0}, 0,0,0,0,
  0, {0,3,0}, {0.100000001f,-0.200000003f,0.300000012f}, -1, -1 },
/* reflect only */
{ {0.4f,-5,0.2f}, {0.1f,-0.2f,0.3f}, {0,1,0}, 0,0,0,0,
  1, {0.400000006f,0.25f,0.200000003f}, {0.100000001f,-0.200000003f,0.300000012f}, -1, -1 },
/* reflect + damp(dampFlag) */
{ {0.4f,-5,0.2f}, {0.1f,-0.2f,0.3f}, {0,1,0}, 1,0,0,0,
  1, {0.359999985f,0.224999994f,0.179999992f}, {0.100000001f,-0.200000003f,0.300000012f}, -1, -1 },
/* reflect + spin(spinFlag) */
{ {0.4f,-5,0.2f}, {0.1f,-0.2f,0.3f}, {0,1,0}, 0,1,0,0,
  1, {0.400000006f,0.25f,0.200000003f}, {0,0.200000003f,0}, -1, -1 },
/* angled spin */
{ {-2,-4,1.5f}, {0.3f,-0.2f,0.4f}, {0.266904199f,0.534808038f,0.801712237f}, 0,1,0,0,
  1, {-1.587901f,-3.17425871f,2.73784018f}, {0.142515615f,-0.00265426887f,0.0757984519f}, -1, -1 },
/* effect thr30 */
{ {1,-6,0.5f}, {0.2f,-0.1f,0.05f}, {0,1,0}, 0,0,30,0,
  1, {0.899999976f,0.269999743f,0.449999988f}, {0.200000003f,-0.100000001f,0.0500000007f}, 6, 156 },
/* effect + damp + spin, peak_in 150 */
{ {-1.5f,-5,2}, {0.1f,0.2f,-0.1f}, {0.303045763f,0.505076272f,0.808122036f}, 1,1,40,150,
  1, {-0.863517821f,-3.46419621f,2.55728555f}, {0.0225461796f,-0.00887959171f,0.00444477657f}, 1, 150 },
/* high-speed impact -- exercises the intensity clamp and peak saturation */
{ {3,-60,2}, {0.2f,-0.1f,0.05f}, {0,1,0}, 0,0,30,0,
  1, {2.69999981f,2.69999647f,1.79999995f}, {0.200000003f,-0.100000001f,0.0500000007f}, 27, 255 },
/* threshold == 10 boundary (the effect path fires at >= 10, not > 10) */
{ {0.5f,-4,0.3f}, {0.15f,-0.25f,0.35f}, {0,1,0}, 0,1,10,0,
  1, {0.449999988f,0.179999828f,0.270000011f}, {0,0.25f,0}, 4, 146 },
};

static void run_kick_cases(void)
{
    static const float BANK[3] = { 1.0f, 0.5f, 0.25f };
    unsigned i;
    for (i = 0; i < sizeof KICK_CASES / sizeof KICK_CASES[0]; ++i) {
        const struct KickCase *c = &KICK_CASES[i];
        BrVec3 vel, ang, N;
        BrCrEffect eff;
        int ret;

        vel.x = c->vel[0]; vel.y = c->vel[1]; vel.z = c->vel[2];
        ang.x = c->ang[0]; ang.y = c->ang[1]; ang.z = c->ang[2];
        N.x = c->N[0]; N.y = c->N[1]; N.z = c->N[2];
        /* colour source is the shared bank */
        g_brCrPlane.normal.x = BANK[0];
        g_brCrPlane.normal.y = BANK[1];
        g_brCrPlane.normal.z = BANK[2];
        memset(&eff, 0, sizeof eff);
        eff.threshold = (uint8_t)c->thr;
        eff.peak      = (uint8_t)c->peakIn;

        ret = BrCrContactKick(&vel, &ang, &N, c->dampFlag, c->spinFlag, &eff);

        if (ret != c->wantRet) {
            printf("  FAIL kick %u: ret=%d want %d\n", i, ret, c->wantRet);
            ++g_fail;
        }
        RCLOSE(vel.x, c->wantVel[0]);
        RCLOSE(vel.y, c->wantVel[1]);
        RCLOSE(vel.z, c->wantVel[2]);
        RCLOSE(ang.x, c->wantAng[0]);
        RCLOSE(ang.y, c->wantAng[1]);
        RCLOSE(ang.z, c->wantAng[2]);
        if (c->wantB1fc >= 0) {
            uint32_t bank[3];
            memcpy(bank, &g_brCrPlane.normal, sizeof bank);
            if (eff.intensity != (uint8_t)c->wantB1fc) {
                printf("  FAIL kick %u: intensity=%u want %d\n",
                       i, eff.intensity, c->wantB1fc); ++g_fail;
            }
            if (eff.peak != (uint8_t)c->wantB1ff) {
                printf("  FAIL kick %u: peak=%u want %d\n",
                       i, eff.peak, c->wantB1ff); ++g_fail;
            }
            if (eff.color[0] != bank[0] || eff.color[1] != bank[1] ||
                eff.color[2] != bank[2]) {
                printf("  FAIL kick %u: colour != bank bits\n", i); ++g_fail;
            }
        }
    }
}

/* ---- 0x10067710 response walker --------------------------------------- *
 * End-to-end goldens: the emulator drove the whole walker (loop, transform,
 * BrCrTest, plane selection, BrCrPlaneResolve, BrCrImpulseSolve, and the
 * push-out + quaternion rebuild) over a one-contact list.  These pin the C
 * walker calling the real ported callees.  Body is mass 1000, identity
 * orientation/box; each row varies velocity, inertia, extents and geometry. */
struct WalkCase {
    float vel[3], ang[3], invI[9], ext[4], tri[9];
    float wantVel[3], wantAng[3];
};
static const struct WalkCase WALK_CASES[] = {
{ {0.3f,-5,0.2f},{0.05f,0,0.05f},{1e-3f,0,0,0,1e-3f,0,0,0,1e-3f},{1,1,1,1},
  {-1,0,0.3f,1,0,0.3f,0,0,-0.8f}, {0.6535f,-3.2325f,1.2605f},{-2.071f,-1.60217e-08f,0.757f} },
{ {0.3f,-5,0.2f},{0.05f,0,0.05f},{1e-3f,0,0,0,1e-3f,0,0,0,1e-3f},{1,1,1,1},
  {-1,-0.2f,0.3f,1,-0.1f,0.3f,0,0,-0.8f}, {-0.516727f,-1.3964f,-0.0700198f},{-1.61679f,-0.273353f,1.44344f} },
{ {-2,-4,1.5f},{0.3f,-0.2f,0.1f},{8e-4f,1e-4f,0,1e-4f,1.2e-3f,0,0,0,9e-4f},{1.2f,0.8f,1.5f,0.5f},
  {-1,-0.3f,0.3f,1,-0.3f,0.3f,0,0.6f,-0.7f}, {-2.3474f,-2.82101f,2.42729f},{0.192112f,-0.771049f,0.611593f} },
{ {1,-6,-0.5f},{0.1f,0.2f,-0.1f},{1e-3f,0,0,0,1e-3f,0,0,0,1e-3f},{2,3,1,0.5f},
  {-1.2f,0,0.2f,1.2f,0,0.2f,0,0,-0.9f}, {2.86f,-1.97f,1.36f},{-1.14f,0.2f,1.14f} },
};

static void run_walker_cases(void)
{
    static const BrMat4 IDENT = {{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
    unsigned i;
    for (i = 0; i < sizeof WALK_CASES / sizeof WALK_CASES[0]; ++i) {
        const struct WalkCase *c = &WALK_CASES[i];
        BrVec3 v0 = {c->tri[0],c->tri[1],c->tri[2]};
        BrVec3 v1 = {c->tri[3],c->tri[4],c->tri[5]};
        BrVec3 v2 = {c->tri[6],c->tri[7],c->tri[8]};
        BrVec3 e1 = {v1.x-v0.x, v1.y-v0.y, v1.z-v0.z};
        BrVec3 e2 = {v2.x-v0.x, v2.y-v0.y, v2.z-v0.z};
        BrVec3 pn = {e1.y*e2.z-e1.z*e2.y, e1.z*e2.x-e1.x*e2.z, e1.x*e2.y-e1.y*e2.x};
        float ln = sqrtf(pn.x*pn.x + pn.y*pn.y + pn.z*pn.z);
        BrCollPlane plane;
        BrCollRespNode node;
        BrMat3 invI;
        BrMat4 orient = IDENT, matBox = IDENT;
        BrRbState next;
        BrVec3 savePos = {0,0,0}, quatSrc = {0,0,0};
        BrCrEffect eff;
        int r;

        pn.x/=ln; pn.y/=ln; pn.z/=ln;
        memset(&plane, 0, sizeof plane);
        plane.nx = pn.x; plane.ny = pn.y; plane.nz = pn.z;
        plane.pV0 = &v0; plane.pV1 = &v1; plane.pV2 = &v2;
        node.pPlane = &plane; node.pNext = NULL;
        g_pBrCollRespList = &node;
        memcpy(invI.m, c->invI, sizeof invI.m);
        memset(&next, 0, sizeof next);
        next.vel.x = c->vel[0]; next.vel.y = c->vel[1]; next.vel.z = c->vel[2];
        next.angVel.x = c->ang[0]; next.angVel.y = c->ang[1]; next.angVel.z = c->ang[2];
        memset(&eff, 0, sizeof eff);

        r = BrCrRespWalk(1000.0f, &invI, &orient, c->ext, &next,
                         &savePos, &quatSrc, &eff, &matBox);
        if (r != 1) { printf("  FAIL walk %u: ret=%d\n", i, r); ++g_fail; }
        RCLOSE(next.vel.x, c->wantVel[0]);
        RCLOSE(next.vel.y, c->wantVel[1]);
        RCLOSE(next.vel.z, c->wantVel[2]);
        RCLOSE(next.angVel.x, c->wantAng[0]);
        RCLOSE(next.angVel.y, c->wantAng[1]);
        RCLOSE(next.angVel.z, c->wantAng[2]);
    }

    /* post-update pin: non-zero position/quaternion pushes the body out and
     * rebuilds the orientation (scenario 0 geometry). */
    {
        BrVec3 v0 = {-1,0,0.3f}, v1 = {1,0,0.3f}, v2 = {0,0,-0.8f};
        BrCollPlane plane;
        BrCollRespNode node;
        BrMat3 invI = {{1e-3f,0,0, 0,1e-3f,0, 0,0,1e-3f}};
        BrMat4 orient = {{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
        BrMat4 matBox = {{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
        float ext[4] = {1,1,1,1};
        BrRbState next;
        BrVec3 savePos = {0.2f,-0.1f,0.4f}, quatSrc = {0.9f,0.1f,0.2f};
        BrCrEffect eff;

        memset(&plane, 0, sizeof plane);
        plane.ny = 1.0f; plane.pV0 = &v0; plane.pV1 = &v1; plane.pV2 = &v2;
        node.pPlane = &plane; node.pNext = NULL; g_pBrCollRespList = &node;
        memset(&next, 0, sizeof next);
        next.vel.x = 0.3f; next.vel.y = -5.0f; next.vel.z = 0.2f;
        next.angVel.x = 0.05f; next.angVel.z = 0.05f;
        next.pos.x = 0.3f; next.pos.y = -0.2f; next.pos.z = 0.5f;
        memset(&eff, 0, sizeof eff);

        BrCrRespWalk(1000.0f, &invI, &orient, ext, &next,
                     &savePos, &quatSrc, &eff, &matBox);
        /* push-out along the plane normal (0,1,0): pos.y -0.2 -> -0.09 */
        RCLOSE(next.pos.x, 0.3f);
        RCLOSE(next.pos.y, -0.09f);
        RCLOSE(next.pos.z, 0.5f);
        /* quaternion restored from quatSrc, then matrix rebuilt from it */
        RCLOSE(next.quat.f00, 0.9f);
        RCLOSE(next.quat.f04, 0.1f);
        RCLOSE(next.quat.f08, 0.2f);
        RCLOSE(orient.m[0][0], 0.78f);
        RCLOSE(orient.m[0][2], -0.36f);
    }
}

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

    run_solver_cases();
    run_kick_cases();
    run_walker_cases();

    if (g_fail) { printf("br_collrespsolve: %d FAILURES\n", g_fail); return 1; }
    printf("br_collrespsolve: all checks passed (0x10067470 + 0x10065C80 + "
           "0x10065980 + 0x10067710 vs x87emu golden vectors)\n");
    return 0;
}
