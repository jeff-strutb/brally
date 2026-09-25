/* slice2_17.c -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * See slice2_17.h for the per-function notes. General remarks:
 *
 * This file is an address BATCH spanning several original TUs, so
 * `g_s17` is a decomp-invented aggregate, not an original file-static. It is
 * external (declared in slice2_17.h) so that the matched functions filed out
 * of this batch -- into drawing/br_gfxfill.c, br_sceneprops.c,
 * br_bankflip.c, br_drawgated.c, br_scratchring.c, racing/br_cartable.c and
 * startup/br_renderreset.c -- reach the same block. Splitting it along the
 * original TU boundaries is still the real fix.
 *
 * - Every FPU sequence in this file was traced through the x87 stack
 *   instruction by instruction (these routines are full of fxch). Where the
 *   original sums three or four products the summation ORDER is preserved
 *   verbatim, because float/double addition is not associative and the
 *   choice is observable.
 *
 * - 0x100309A0 / 0x10030B50 / 0x10030E20 / 0x10030EE0 mix precisions: the
 *   arguments are floats, the intermediate basis is double (the routines
 *   call the br_vecd.h library), and the sixteen matrix stores are float.
 *   Casts are written out explicitly rather than left to the usual
 *   arithmetic conversions, since `floatA - floatB` in C rounds to float
 *   while `fld dword; fsub dword` does not.
 *
 * - The .rdata constants were read out of orig/BRD3D.dll rather than
 *   guessed. They are listed at their addresses below.
 *
 * - SKIPPED, and deliberately absent from this file: 0x10030210. It is the
 *   DirectX SDK's GetDXVersion sample: GetVersionExA, LoadLibraryA of
 *   DDRAW.DLL / DINPUT.DLL, GetProcAddress of DirectDrawCreate /
 *   DirectInputCreateA, and a ladder of COM QueryInterface calls on
 *   IID_IDirectDraw2 (0x1008FCE0), 0x1008FD20 and 0x1008FD30 that yields
 *   0x100/0x200/0x300/0x500/0x600 into *pdwVersion and 1/2 into
 *   *pdwPlatform. There is no portable C99 rendering of it -- it is Win32
 *   and COM from top to bottom -- so writing one would be inventing, not
 *   decompiling.
 */
#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* .rdata constants, read from the DLL.                                */

/* 0x1008F448  dword 0x400F5C29 -- 2.24f, the m/s -> mph factor. */
#define BR_MPH_PER_MS      2.24f

/* 0x1008F4E0  0x3F91DF46A2529D39 -- pi/180. */
#define BR_DEG_TO_RAD      0.017453292519943295

/* 0x1008F4B0  0xC0545F30B4E4E30A and 0x1008F4B8 0xBFD45F30B4E4E30A.
 * Exactly 256x apart. Neither is the correctly rounded -256/pi or -1/pi;
 * the shipped values are used verbatim. */
#define BR_ANG_K256      (-81.48734781601175)
#define BR_ANG_K1        (-0.3183099524062959)

/* ------------------------------------------------------------------ */
/* Module state.                                                       */

/* Declared extern in slice2_17.h: the functions filed out of this batch
 * into their modules reach it from there. */
BrS17State g_s17;

/* BrScenePropsDraw's fixed storage (0x100AA5D0, 0x106C08A0, 0x106C0860) and
 * its S17PropItem view went with it to src/core/drawing/br_sceneprops.c. */

/* 0x106806B0 -- the 0x24-byte frame-timer object 0x100751D0 / 0x10075240
 * operate on. slice8_86.c treats it as an opaque byte image (see its
 * br86_ld32 / br86_st32 accessors); it is only ever named by its address, so
 * it is defined here as raw storage. */
unsigned char g_br6806B0[0x24];

BrS17State *BrS17GetState(void)
{
    return &g_s17;
}

/* ------------------------------------------------------------------ */
/* Cross-slice callees. Stand-ins live in the test file.               */

/* XSLICE 0x10008B80 */  /* a bare `ret` in this build -- see the contract */
extern void BrStub10008B80(intptr_t a0, ...);
/* XSLICE 0x10060E90 */
extern int   BrX10060E90(void);
/* XSLICE 0x100751D0
 * 0x1002C2A0 tail-jumps into it with the object in ecx and nothing on the
 * stack: it is a C++ __thiscall method. MSVC 5.0's C front end cannot spell
 * __thiscall, but for a single pointer argument __fastcall is byte-identical
 * at the call site (arg1 in ecx, no stack cleanup), so that is what the
 * matching build uses. Off MSVC the qualifier vanishes and it is an ordinary
 * one-argument function. */
#if defined(_MSC_VER)
#define BRS17_THISCALL __fastcall
#else
#define BRS17_THISCALL
#endif
extern void BRS17_THISCALL BrX100751D0(void *pThis);
/* XSLICE 0x1002C2C0 */
extern void  BrX1002C2C0(void);
/* XSLICE 0x1003563A */
extern void  BrX1003563A(int a0);
/* XSLICE 0x100397C0 */
extern void  BrX100397C0(void);
/* XSLICE 0x10034C66 */
extern void  BrX10034C66(void (*pfn)(void));
/* XSLICE 0x1002C500 */
extern void  BrX1002C500(void);
/* XSLICE 0x10075F10 */
extern void  BrX10075F10(void *pThis);
/* XSLICE 0x100664C0 */
extern void  BrX100664C0(void *pThis);
/* XSLICE 0x10005DE0 */
extern int   BrX10005DE0(void *pOwner, unsigned char *pb0,
                         unsigned char *pb1, unsigned char *pb2);
/* XSLICE 0x10076AE0 */
extern void  BrX10076AE0(void *pThis, int a0);
/* XSLICE 0x10005E70 */
extern const char *BrX10005E70(void *pOwner);
/* XSLICE 0x10068260 */
extern void  BrX10068260(int i, uint32_t tag);
/* XSLICE 0x10072580 */
extern void  BrX10072580(int a0);
/* XSLICE 0x10042AF0 */
extern void  BrX10042AF0(void *p, int a1, int a2);
/* XSLICE 0x10035BBA */
extern void  BrX10035BBA(const char *psz);
/* XSLICE 0x10069530 */
extern void *BrX10069530(void);
/* XSLICE 0x10069490 */
extern void *BrX10069490(void);
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1).
 * Anything at or above 0x1007CC40 is statically linked MSVC CRT, so the
 * platform's own atexit is used instead of porting it. */
extern int   BrXAtExit(void (*pfn)(void));

/* ------------------------------------------------------------------ */
/* Small helpers. Loads and stores go through memcpy so that the byte
 * offsets recovered from the disassembly stay valid without relying on
 * pointer casts being aligned.                                         */

static uint32_t s17_ld32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_st32(unsigned char *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static float s17_ldf(const unsigned char *p)
{
    float v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void s17_stf(unsigned char *p, float v)
{
    memcpy(p, &v, sizeof v);
}

/* g_6C0680 is advanced by 8 bytes and then the two words are written --
 * the original reads the cursor, bumps the global, and only then stores. */
#define s17_emit(w0_, w1_)                                              \
    do {                                                                \
        uint32_t *p_ = g_s17.pGfx;                                      \
                                                                        \
        g_s17.pGfx = p_ + 2;                                            \
        p_[0] = (w0_);                                                  \
        p_[1] = (w1_);                                                  \
    } while (0)

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
#define s17_ptrword(p_)   ((uint32_t)(uintptr_t)(const void *)(p_))

/* ================================================================== */
/* 1. camera / basis matrices                                         */
/* ================================================================== */

/* 0x100309A0 */
/* WHAT IT DOES: builds the transform that puts the world in front of a camera
 * -- given where the camera is, what it is looking at and which way is up, it
 * produces the matrix that turns world positions into positions relative to
 * that camera. This is what a view through the windscreen or from the trackside
 * is set up with. */
/* @t4-pass 0x1002A050 1 2026-09-13 probes 83 bytes 427 insns 140 regions 3 rows 20 census yes  (tools/crank.py) */
/* @implements 0x100309A0 d3d BrMat4LookAt */
void BrMat4LookAt(BrMat4 *pM,
                  float xEye, float yEye, float zEye,
                  float xAt,  float yAt,  float zAt,
                  float xUp,  float yUp,  float zUp)
{
    BrVec3d z, y, x;
    double d;

    /* z = normalise(eye - at). The original is `fld dword; fsub dword`: the
     * x87 subtracts the two floats in 80 bits and the result is stored to a
     * double, so the difference is EXACT. Under VC5 the plain float form is
     * what emits that pair -- casting both operands to double first costs
     * fourteen bytes and six extra x87 slots -- but on a target without the
     * 80-bit stack `xEye - xAt` would round to float, so the port keeps the
     * casts and only the matching build drops them. Same value either way on
     * the original hardware. */
#ifdef BR_MATCHING_BUILD
    z.x = xEye - xAt;
    z.y = yEye - yAt;
    z.z = zEye - zAt;
#else
    z.x = (double)xEye - (double)xAt;
    z.y = (double)yEye - (double)yAt;
    z.z = (double)zEye - (double)zAt;
#endif
    BrVec3dNormalise(&z);

    /* y = normalise(up - dot(up, z) * z). Note the dot's argument order:
     * the original pushes (up, z), i.e. pA = up. */
    y.x = (double)xUp;
    y.y = (double)yUp;
    y.z = (double)zUp;
    d = BrVec3dDot(&y, &z);
    y.x = y.x - d * z.x;
    y.y = y.y - d * z.y;
    y.z = y.z - d * z.z;
    BrVec3dNormalise(&y);

    /* x = y cross z. BrVec3dCross puts the OUTPUT third (br_vecd.h). */
    BrVec3dCross(&y, &z, &x);


    /* Translation row: -dot(eye, axis), summed left to right with the
     * first term negated, exactly as the fchs/fsubp chain does it. The eye
     * components are read from the PARAMETERS here, not from double locals
     * hoisted at the top: naming them costs three extra `fld dword` /
     * `fstp qword` pairs and eighteen bytes of frame.
     *
     * TWO SOURCE FACTS, found 2026-09-05, that took this from 4+6 and four
     * bytes short to SIZE- AND INSTRUCTION-EXACT at register-blind 0+0:
     *  1. the block is written PER AXIS -- each column's translation, then
     *     that column's three matrix stores -- not as nine stores followed
     *     by three translations.  Pure address order over the whole block
     *     is byte-identical to the old column-major form (202 diffs); it is
     *     the interleave that matters, and the translation must come FIRST
     *     within each axis (stores-first is 121, translation-first 141 but
     *     at 0+0 and size-exact).
     *  2. the leading negation is spelled `0.0 - a*b`, not `-(a*b)`.  The
     *     subtract-from-zero keeps the negation as part of the sum chain;
     *     `-(...)` emits the `fchs` early and costs two `fxch` and 4 bytes.
     *     (`0.0 -` and `-(...)` differ only in the sign of a zero result,
     *     which no consumer of a view matrix can observe.)
     * RESIDUE, 141 diff bytes, register-blind 0+0, size-exact, TWO regions:
     * the original interleaves the THREE columns' translation chains with
     * each other -- `fmul` of the next column's term is emitted before the
     * `fchs` of the previous column's first product -- where ours finishes
     * one chain's negation first.  Same instructions, same count, different
     * pipelining depth.  Dead: all three translations hoisted above the
     * stores (0+5 / 0+3), nine named product temps (0+4), a named double
     * temp per translation (0+2), negating the eye operand instead of the
     * product (3+4), negating the whole dot (12+8), and reversing the
     * m[k][3] zero stores (inert). */
    pM->m[3][0] = (float)(((0.0 - xEye * x.x) - yEye * x.y) - zEye * x.z);
    pM->m[0][0] = (float)x.x; pM->m[1][0] = (float)x.y; pM->m[2][0] = (float)x.z;
    pM->m[3][1] = (float)(((0.0 - xEye * y.x) - yEye * y.y) - zEye * y.z);
    pM->m[0][1] = (float)y.x; pM->m[1][1] = (float)y.y; pM->m[2][1] = (float)y.z;
    pM->m[3][2] = (float)(((0.0 - xEye * z.x) - yEye * z.y) - zEye * z.z);
    pM->m[0][2] = (float)z.x; pM->m[1][2] = (float)z.y; pM->m[2][2] = (float)z.z;
    pM->m[0][3] = 0.0f;       pM->m[1][3] = 0.0f;       pM->m[2][3] = 0.0f;
    pM->m[3][3] = 1.0f;
}

static void s17_pack_dirs(const BrMat4 *pM, BrLightPair *pLights)
{
    pLights->dir0[0] = BrPackNormalByte((double)pM->m[0][0]);
    pLights->dir0[1] = BrPackNormalByte((double)pM->m[1][0]);
    pLights->dir0[2] = BrPackNormalByte((double)pM->m[2][0]);
    pLights->dir1[0] = BrPackNormalByte((double)pM->m[0][1]);
    pLights->dir1[1] = BrPackNormalByte((double)pM->m[1][1]);
    pLights->dir1[2] = BrPackNormalByte((double)pM->m[2][1]);
}

/* dot(column c of pM, v), in the original's summation order:
 *      (m2*v.z + m1*v.y) + m0*v.x     for columns 0 and 2
 *      (m2*v.z + m0*v.x) + m1*v.y     for column 1
 * The two orders really are different in the original; column 1 is always
 * computed by the shorter three-term chain that starts with z and x. */
static double s17_dot_col_zyx(const BrMat4 *pM, int c, const BrVec3d *v)
{
    double s = (double)pM->m[2][c] * v->z + (double)pM->m[1][c] * v->y;
    return s + (double)pM->m[0][c] * v->x;
}

static double s17_dot_col_zxy(const BrMat4 *pM, int c, const BrVec3d *v)
{
    double s = (double)pM->m[2][c] * v->z + (double)pM->m[0][c] * v->x;
    return s + (double)pM->m[1][c] * v->y;
}

/* dot(column 2, v) is built as (m1*v.y + m2*v.z) + m0*v.x -- note the pair
 * is (y, z) here where column 0 uses (z, y). Preserved. */
static double s17_dot_col_yzx(const BrMat4 *pM, int c, const BrVec3d *v)
{
    double s = (double)pM->m[1][c] * v->y + (double)pM->m[2][c] * v->z;
    return s + (double)pM->m[0][c] * v->x;
}

/* 0x1007C8A0 __ftol: truncate toward zero, take the low dword. */
static int32_t s17_ftol(double v)
{
    return (int32_t)v;
}

/* 0x10030B50 */
/* WHAT IT DOES: does the lighting set-up of its neighbour above and then works
 * out where two given directions land on the sky -- as a pair of horizontal and
 * vertical angles each, which is how the sky texture is scrolled to follow the
 * camera. The first pair is measured against a fixed scale and the second
 * against one the caller supplies. */
/* @implements 0x10030B50 d3d BrLightDirsAndAngles */
void BrLightDirsAndAngles(BrMat4 *pM, BrLightPair *pLights,
                          BrSkyAngles *pAngles,
                          float xEye, float yEye, float zEye,
                          float xAt,  float yAt,  float zAt,
                          float xUp,  float yUp,  float zUp,
                          float xA, float yA, float zA,
                          float xB, float yB, float zB,
                          int nS1, int nT1)
{
    BrVec3d a, b;
    double t;

    BrMat4LookAt(pM, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    s17_pack_dirs(pM, pLights);

    /* --- first direction, half-revolution count hardcoded at 0x100 --- */
    a.x = (double)xA; a.y = (double)yA; a.z = (double)zA;
    BrVec3dNormalise(&a);

    /* t is computed BEFORE the atan2 in the original and spilled to the
     * stack across the __ftol call; kept in that order. */
    t = s17_dot_col_zxy(pM, 1, &a);
    pAngles->s0 = 0x100 - s17_ftol(atan2(s17_dot_col_zyx(pM, 0, &a),
                                         s17_dot_col_yzx(pM, 2, &a))
                                   * BR_ANG_K256);
    pAngles->t0 = 0x100 - s17_ftol(asin(t) * BR_ANG_K256);

    /* --- second direction, counts from the arguments --------------- */
    b.x = (double)xB; b.y = (double)yB; b.z = (double)zB;
    BrVec3dNormalise(&b);

    t = s17_dot_col_zxy(pM, 1, &b);
    /* `fimul` first, then the constant: (theta * n) * k, not theta * (n*k). */
    pAngles->s1 = nS1 * 4
        - s17_ftol(atan2(s17_dot_col_zyx(pM, 0, &b),
                         s17_dot_col_yzx(pM, 2, &b))
                   * (double)(nS1 * 4) * BR_ANG_K1);
    pAngles->t1 = nT1 * 4
        - s17_ftol(asin(t) * (double)(nT1 * 4) * BR_ANG_K1);
}

/* The three degenerate tests are `fcomp v, 0.0` reading C3 only, so an
 * unordered compare (NaN) also counts as "equal to zero". */
static int s17_is_zero_or_nan(float v)
{
    return !(v < 0.0f) && !(v > 0.0f);
}

static void s17_identity(BrMat4 *pM)
{
    int i, j;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            pM->m[i][j] = (i == j) ? 1.0f : 0.0f;
}

/* 0x10030EE0 */
/* WHAT IT DOES: builds the transform that turns things a given number of
 * degrees about any axis you name. It does it by building a frame of reference
 * around the axis, spinning flat inside that frame, and then undoing the frame.
 * An axis of zero length -- or one containing a not-a-number -- yields the
 * do-nothing transform instead. */
/* @implements 0x10030EE0 d3d BrMat4RotateAxis */
void BrMat4RotateAxis(BrMat4 *pM, float degrees, float x, float y, float z)
{
    BrMat4 basis, basisT, rot;
    double ang;
    float c, s;
    int i, j;

    if (s17_is_zero_or_nan(x) && s17_is_zero_or_nan(y) && s17_is_zero_or_nan(z)) {
        s17_identity(pM);
        return;
    }

    /* up = (y, z, x): a cyclic shift of the axis, not a fixed world up. */
    BrMat4LookAt(&basis, x, y, z, 0.0f, 0.0f, 0.0f, y, z, x);

    /* Drop the translation row and the fourth column that BrMat4LookAt
     * filled in, leaving a pure rotation. */
    basis.m[0][3] = 0.0f;
    basis.m[1][3] = 0.0f;
    basis.m[2][3] = 0.0f;
    basis.m[3][0] = 0.0f;
    basis.m[3][1] = 0.0f;
    basis.m[3][2] = 0.0f;
    basis.m[3][3] = 1.0f;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            basisT.m[i][j] = basis.m[j][i];

    ang = (double)degrees * BR_DEG_TO_RAD;
    s = (float)sin(ang);
    c = (float)cos(ang);

    rot.m[0][0] =  c;    rot.m[0][1] = s;    rot.m[0][2] = 0.0f; rot.m[0][3] = 0.0f;
    rot.m[1][0] = -s;    rot.m[1][1] = c;    rot.m[1][2] = 0.0f; rot.m[1][3] = 0.0f;
    rot.m[2][0] = 0.0f;  rot.m[2][1] = 0.0f; rot.m[2][2] = 1.0f; rot.m[2][3] = 0.0f;
    rot.m[3][0] = 0.0f;  rot.m[3][1] = 0.0f; rot.m[3][2] = 0.0f; rot.m[3][3] = 1.0f;

    BrMat4Mul(&basis, &rot, pM);
    BrMat4Mul(pM, &basisT, pM);     /* aliased -- BrMat4Mul handles it */
}

/* 0x1002A957 */
/* WHAT IT DOES: finds the largest magnitude among twelve numbers, ignoring
 * sign, and never returns less than zero. */
/* @t4-pass 0x1002A957 1 2026-09-07 probes 149 bytes 115 insns 38 regions 1 rows 55 census yes  (tools/crank.py) */
/* @t4-pass 0x1002A957 2 2026-09-07 probes 150 bytes 115 insns 38 regions 1 rows 55 census yes  (tools/crank.py) */
/* @implements 0x1002A957 glide BrFloat12MaxAbs */
/* @implements 0x100312A7 d3d BrFloat12MaxAbs */
/* @n64 0x80217420 located */
float BrFloat12MaxAbs(const float *pv)
{
    /* This TU compiles /Od /Op, so fn.py's /O2 numbers are phantom here -- use
     * a direct /Od /Op compile to score it.
     *
     * The INSTRUCTION STREAM is now exact: same 55 instructions in the same
     * order, frame 0x18, six slots. Two source shapes got it there from 48
     * differing bytes to 23, and both are reusable /Od facts:
     *
     *  1. The cursor step is `(v = *p++)` INSIDE the condition, not `v = *p;
     *     p++;` as two statements. MSVC /Od defers the postfix increment until
     *     after the comparison's operand is consumed, so the original's
     *     `p += 4` sits BETWEEN the `fcomp/fnstsw` and the `test ah,1`. Two
     *     separate statements put it before the `fld` and cost six bytes.
     *  2. The tail is `if (lo > hi) return lo; return hi;`, not a ternary. The
     *     ternary allocates a seventh slot for the result (frame 0x1C) and
     *     the original has six.
     *
     * PARKED at 23 bytes, and every one of them is a STACK SLOT NUMBER -- the
     * six locals are permuted against the original:
     *     orig  -4 hi   -8 end   -0xC p    -0x10 zero  -0x14 v   -0x18 lo
     *     ours  -4 ?    -8 ?     -0xC lo   -0x10 zero  -0x14 hi  -0x18 v
     *
     * ‼ DECLARATION ORDER IS INERT UNDER /Od. Seven different orders of these
     * six locals were compiled and every one produced byte-identical output;
     * do not probe another. SCOPE IS NOT INERT: moving `v` into the while body
     * (as below) moved three slots and is what put `zero` on -0x10. That is
     * the only knob found so far and it is worth one more session -- the
     * remaining question is what puts a FLOAT on -4 ahead of the two pointers,
     * which no arrangement tried so far does.
     * 2026-09-12: the knob is the NAMES.  28 name sets measured (/Od /Op,
     * slots decoded from the init stores): renaming hi/lo/p/end permutes the
     * slots, renaming zero/v never does; `pp` for p puts end on -4 and p on
     * -0xC, `stop`/`pend` for end puts lo on -8, `h` for hi puts hi on -0x10.
     * No set tried gives the original's order, and the order is NOT a simple
     * hash of the name (sum/first/last/length/polynomial over 2..127 buckets
     * with either tie-break all fail the 28 observations).  The same knob
     * landed 0x1002CEE9 (16 sets) and 0x1002AF17 (38 sets) in br_framebegin.c
     * -- brute force works when the function has 4-5 locals. */
    float hi;
    float lo;
    float zero;
    const float *p;
    const float *end;

    hi = 0.0f;
    lo = 0.0f;
    zero = 0.0f;
    p = pv;
    end = pv + 12;

    while (p < end) {
        float v;
        if ((v = *p++) < zero) {
            if (lo > v)
                lo = v;
        } else {
            if (hi < v)
                hi = v;
        }
    }
    lo = -lo;
    if (lo > hi)
        return lo;
    return hi;
}

/* ================================================================== */
/* 2-4. filed out                                                     */
/* ================================================================== */

/* The RDP fill / texture emitters (BrGfxEmitTexCmd, BrGfxClearScreen,
 * BrGfxFillRect) are in src/core/drawing/br_gfxfill.c; the prop display
 * list (BrScenePropsDraw) is in src/core/drawing/br_sceneprops.c; the car
 * table and its save/restore (BrCarTableAdd, BrCarTableRemove,
 * BrCarStateSave, BrCarStateRestore) are in src/core/racing/br_cartable.c. */

/* ================================================================== */
/* 5. small global glue                                               */
/* ================================================================== */

/* 0x1002C210 BrS17BankFlip is in src/core/drawing/br_bankflip.c. */

/* 0x1002C2A0 BrS17Release, 0x1002C2B0 BrS17RegisterAtExit and
 * 0x10019800 BrS17Init now live in src/core/startup/br_s17life.c. */

/* 0x1002C2D0 BrS17DrawGated is in src/core/drawing/br_drawgated.c. */

/* 0x1002C320 */
/* WHAT IT DOES: draws one frame of the world -- the scene, then a second pass
 * over it -- with a colour marker set around each part, which is how the frame
 * was profiled on a debug machine. The whole thing is skipped while one
 * suppression flag is raised. */
/* @d3donly 0x1002C320 BrS17DrawFrame -- glide twin 0x10019890 claimed by br_racebegin.c:BrRaceHudFrame */
void BrS17DrawFrame(void)
{
    if (g_s17.f6909B4 != 0)
        return;

    BrStub10008B80(0, 0x80, 0x80, 0xF0, 0xFF);
    BrS17DrawGated();
    BrStub10008B80(0, 0, 0, 0xC0, 0xFF);
    BrX100397C0();
    BrStub10008B80(0, 0, 0x82, 0, 0xFF);
}

/* 0x1002C390 */
/* WHAT IT DOES: switches the game into one particular mode, sets a second mode
 * value alongside it, and installs the routine that will run for it. Which mode
 * that is -- what the player would see -- was not established here. */
/* @d3donly 0x1002C390 BrS17SetMode4 -- glide twin 0x10019900 claimed by br_racebegin.c:BrRaceEnterOutro */
void BrS17SetMode4(void)
{
    g_s17.f0AA010 = 4;
    g_s17.f6805B8 = 2;
    BrX10034C66(BrX1002C500);
}

/* 0x1002C410 */
/* WHAT IT DOES: counts down a chain of countdown records by one each, walking
 * forward until it reaches a record that is not in use. An entirely empty chain
 * is left alone. */
/* @d3donly 0x1002C410 BrS17TimerTick -- glide twin 0x10019980 claimed by br_racebegin.c:BrRaceCueRewind */
void BrS17TimerTick(void *pRecords)
{
    unsigned char *rec = (unsigned char *)pRecords;

    if (s17_ld32(rec + 0x0C) == 0)
        return;

    do {
        s17_st32(rec, s17_ld32(rec) - 1u);
        rec += BR_TICKREC_STRIDE;
    } while (s17_ld32(rec + 0x0C) != 0);
}

/* 0x1002C430 */
/* WHAT IT DOES: works out how fast a car is actually travelling, in miles per
 * hour, from its velocity in all three directions -- this is the number the
 * speedometer shows. A car with the relevant flag clear keeps its old speed,
 * but the follow-up step runs either way. */
/* @d3donly 0x1002C430 BrCarUpdateSpeedMph -- glide twin 0x100199A0 claimed by br_racebegin.c:BrRaceCarCtlOutro */
void BrCarUpdateSpeedMph(void *pCar)
{
    unsigned char *car = (unsigned char *)pCar;

    if (s17_ld32(car + 0x730) != 0) {
        float x = s17_ldf(car + 0x1E8);
        float y = s17_ldf(car + 0x1EC);
        float z = s17_ldf(car + 0x1F0);
        /* (x*x + z*z) + y*y, then sqrt, then * 2.24 -- the sum order is
         * the original's: y and z are spilled and multiplied first. */
        float sum = (x * x + z * z) + y * y;

        s17_stf(car + 0x1030, (float)sqrt((double)sum) * BR_MPH_PER_MS);
    }

    BrX10075F10(car);              /* called even when +0x730 is zero */
}

/* 0x1002C4A0 */
/* WHAT IT DOES: releases every one of the currently active player slots, one
 * at a time. */
/* @d3donly 0x1002C4A0 BrS17SlotsRelease -- glide twin 0x10019A10 claimed by br_racebegin.c:BrRaceDriverReset */
void BrS17SlotsRelease(void)
{
    int i;

    for (i = 0; i < g_s17.nEntA; ++i)
        BrX100664C0(g_s17.pSlots + (size_t)i * BR_SLOT_STRIDE);
}

/* 0x10031190 BrScratchRingAlloc and 0x100311E4 BrScratchRingDrain are in
 * src/core/drawing/br_scratchring.c. */

/* 0x10031212 BrScratchRingNull is filed in src/core/startup/br_stubs.c. */

/* 0x10031212 BrScratchRingNull and 0x10031282 BrScreenSizeInit are filed in
 * src/core/startup/br_stubs.c; 0x10031227 BrRenderCountersReset and
 * 0x1003128C BrScreenSizeApply are in src/core/startup/br_renderreset.c. */

/* 0x10031342 */
/* WHAT IT DOES: nothing at all. It exists so that something expecting a
 * routine to call has one. */
/* @d3donly 0x10031342 BrTexNoOp -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrTexNoOp(void)
{
}

#ifdef BR_MATCHING_BUILD
/* XSLICE 0x10080580 */ extern void BrX10080580(void);
/* XSLICE 0x10080120 */ extern void BrX10080120(void);
/* XSLICE 0x100801B0 */ extern void BrX100801B0(void);
/* XSLICE 0x100800C0 */ extern void BrX100800C0(void);
/* XSLICE 0x10080190 */ extern void BrX10080190(void);

/* XSLICE 0x100BDAB8 */ extern void (*g_0BDAB8)(void);
/* XSLICE 0x100BDABC */ extern void (*g_0BDABC)(void);
/* XSLICE 0x100BDAC0 */ extern void (*g_0BDAC0)(void);
/* XSLICE 0x100BDAC4 */ extern void (*g_0BDAC4)(void);
/* XSLICE 0x100BDAC8 */ extern void (*g_0BDAC8)(void);
/* XSLICE 0x100BDACC */ extern void (*g_0BDACC)(void);

/* 0x1007C7F0 */
/* WHAT IT DOES: installs the six helpers used to print floating-point numbers
 * -- convert, strip trailing zeros, assign, force a decimal point, test the
 * sign -- with the converter occupying both the first and last slots. */
/* @d3donly 0x1007C7F0 BrSub1007C7F0 -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
void BrSub1007C7F0(void)
{
    /* eax is loaded with BrX10080580 first and reused for g_0BDAB8 / g_0BDACC. */
    g_0BDABC = BrX10080120;
    g_0BDAB8 = BrX10080580;
    g_0BDAC0 = BrX100801B0;
    g_0BDAC4 = BrX100800C0;
    g_0BDAC8 = BrX10080190;
    g_0BDACC = BrX10080580;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD

#endif /* BR_MATCHING_BUILD */
