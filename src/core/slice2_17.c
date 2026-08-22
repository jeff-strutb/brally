/* slice2_17.c -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * See slice2_17.h for the per-function notes. General remarks:
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

static BrS17State g_s17;

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
static void s17_emit(uint32_t w0, uint32_t w1)
{
    uint32_t *p = g_s17.pGfx;

    g_s17.pGfx = p + 2;
    p[0] = w0;
    p[1] = w1;
}

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
static uint32_t s17_ptrword(const void *p)
{
    return (uint32_t)(uintptr_t)p;
}

/* ================================================================== */
/* 1. camera / basis matrices                                         */
/* ================================================================== */

/* 0x100309A0 */
/* WHAT IT DOES: builds the transform that puts the world in front of a camera
 * -- given where the camera is, what it is looking at and which way is up, it
 * produces the matrix that turns world positions into positions relative to
 * that camera. This is what a view through the windscreen or from the trackside
 * is set up with. */
/* @implements 0x100309A0 d3d BrMat4LookAt */
void BrMat4LookAt(BrMat4 *pM,
                  float xEye, float yEye, float zEye,
                  float xAt,  float yAt,  float zAt,
                  float xUp,  float yUp,  float zUp)
{
    BrVec3d z, y, x;
    double d;
    double ex = (double)xEye, ey = (double)yEye, ez = (double)zEye;

    /* z = normalise(eye - at). Both operands are floats so the difference
     * is exact in double, matching `fld dword; fsub dword`. */
    z.x = (double)xEye - (double)xAt;
    z.y = (double)yEye - (double)yAt;
    z.z = (double)zEye - (double)zAt;
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

    pM->m[0][0] = (float)x.x; pM->m[1][0] = (float)x.y; pM->m[2][0] = (float)x.z;
    pM->m[0][1] = (float)y.x; pM->m[1][1] = (float)y.y; pM->m[2][1] = (float)y.z;
    pM->m[0][2] = (float)z.x; pM->m[1][2] = (float)z.y; pM->m[2][2] = (float)z.z;
    pM->m[0][3] = 0.0f;       pM->m[1][3] = 0.0f;       pM->m[2][3] = 0.0f;

    /* Translation row: -dot(eye, axis), summed left to right with the
     * first term negated, exactly as the fchs/fsubp chain does it. */
    pM->m[3][0] = (float)((-(ex * x.x) - ey * x.y) - ez * x.z);
    pM->m[3][1] = (float)((-(ex * y.x) - ey * y.y) - ez * y.z);
    pM->m[3][2] = (float)((-(ex * z.x) - ey * z.y) - ez * z.z);
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

/* 0x10030E20 */
/* WHAT IT DOES: builds a camera transform and then squeezes two of its axes
 * down into the byte-sized direction pair the lighting hardware wants, so the
 * scene is lit relative to how it is being viewed. */
/* @implements 0x10030E20 d3d BrLightDirsFromLookAt */
void BrLightDirsFromLookAt(BrMat4 *pM, BrLightPair *pLights,
                           float xEye, float yEye, float zEye,
                           float xAt,  float yAt,  float zAt,
                           float xUp,  float yUp,  float zUp)
{
    BrMat4LookAt(pM, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    s17_pack_dirs(pM, pLights);
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

/* 0x100312A7 */
/* WHAT IT DOES: finds the largest magnitude among twelve numbers, ignoring
 * sign, and never returns less than zero. */
/* @implements 0x100312A7 d3d BrFloat12MaxAbs */
float BrFloat12MaxAbs(const float *pv)
{
    float lo = 0.0f;    /* most negative seen */
    float hi = 0.0f;    /* most positive seen */
    int i;

    for (i = 0; i < 12; ++i) {
        float v = pv[i];

        /* C0 of `fcomp v, 0.0` -- set when v < 0 OR unordered. */
        if (!(v >= 0.0f)) {
            if (lo > v)
                lo = v;
        } else {
            if (hi < v)
                hi = v;
        }
    }

    lo = -lo;
    return (lo > hi) ? lo : hi;
}

/* ================================================================== */
/* 2. RDP fill / scissor emitters                                     */
/* ================================================================== */

static uint32_t s17_rgba5551(int r, int g, int b)
{
    uint32_t v;

    v  = ((uint32_t)r << 8) & 0xF800u;
    v |= ((uint32_t)g << 3) & 0x07C0u;
    v |= ((uint32_t)(b >> 2)) & 0x003Eu;    /* arithmetic shift, as `sar` */
    v |= 1u;                                /* `or al, 1` */
    return v & 0xFFFFu;
}

/* 0x100314E8 */
/* WHAT IT DOES: wipes the whole screen to one flat colour, switching the
 * hardware into its fast fill mode to do it and putting it back afterwards. */
/* @implements 0x100314E8 d3d BrGfxClearScreen */
void BrGfxClearScreen(int r, int g, int b)
{
    uint32_t c = s17_rgba5551(r, g, b);
    /* `shl reg, cl` uses only the low five bits of cl. */
    unsigned sh = (unsigned)g_s17.scaleShift & 31u;
    uint32_t lr;

    s17_emit(0xE7000000u, 0);                       /* pipe sync            */
    s17_emit(0xB900031Du, 0x0F0A4000u);             /* othermode L          */
    s17_emit(0xBA001402u, 0x00300000u);             /* othermode H = fill   */
    s17_emit(0xF7000000u, (c << 16) | c);           /* fill colour          */

    lr  = BR_GFX_FILLRECT;
    lr |= ((((uint32_t)g_s17.screenW << sh) - 1u) & 0xFFFu) << 12;
    lr |=  (((uint32_t)g_s17.screenH << sh) - 1u) & 0xFFFu;
    s17_emit(lr, 0);

    s17_emit(0xE7000000u, 0);
    s17_emit(0xBA001402u, 0);
}

/* 0x10031688 */
/* WHAT IT DOES: fills a rectangle with one flat colour. In the double-size
 * display mode every coordinate is doubled first -- and then the bottom-right
 * corner is doubled a second time on its way into the command while the
 * top-left corner is not scaled at all, so in that mode the rectangle comes out
 * larger than asked for. That asymmetry is the original's. */
/* @implements 0x10031688 d3d BrGfxFillRect */
void BrGfxFillRect(int ulx, int uly, int w, int h, int r, int g, int b)
{
    uint32_t c;
    unsigned sh = (unsigned)g_s17.scaleShift & 31u;
    uint32_t w0;

    if (g_s17.scaleShift != 0) {
        ulx *= 2;
        uly *= 2;
        w   *= 2;
        h   *= 2;
    }

    c = s17_rgba5551(r, g, b);

    s17_emit(0xE7000000u, 0);
    s17_emit(0xB900031Du, 0x0F0A4000u);
    s17_emit(0xBA001402u, 0x00300000u);
    s17_emit(0xF7000000u, (c << 16) | c);

    /* GOTCHA: the lower-right corner is shifted by scaleShift AGAIN after
     * the doubling above, while the upper-left corner below is not shifted
     * at all. Faithful to the original. */
    w0  = BR_GFX_FILLRECT;
    w0 |= (((((uint32_t)(ulx + w)) << sh) - 1u) & 0xFFFu) << 12;
    w0 |=  ((((uint32_t)(uly + h)) << sh) - 1u) & 0xFFFu;
    s17_emit(w0, (((uint32_t)ulx & 0xFFFu) << 12) | ((uint32_t)uly & 0xFFFu));

    s17_emit(0xE7000000u, 0);
    s17_emit(0xBA001402u, 0);
}

/* 0x10031481 */
/* WHAT IT DOES: tells the hardware to use one particular texture out of a
 * table, unless that texture's record is flagged as one to skip, in which case
 * nothing is emitted and whatever texture was in use stays. */
/* @implements 0x10031481 d3d BrGfxEmitTexCmd */
void BrGfxEmitTexCmd(int i, const void *pRecords)
{
    const unsigned char *rec =
        (const unsigned char *)pRecords + (size_t)i * BR_TEXREC_STRIDE;

    if (((s17_ld32(rec + 0x20) >> 20) & 1u) != 0)
        return;

    s17_emit((s17_ld32(rec) & 0x00FFFFFFu) | 0xDC000000u, 1);
}

/* ================================================================== */
/* 3. the prop display list                                           */
/* ================================================================== */

/* 0x1002FB20 */
/* WHAT IT DOES: draws the trackside scenery -- the objects standing around the
 * course. It sets the lighting up from a fixed overhead direction, configures
 * how the objects are shaded and depth-tested, and then walks the list drawing
 * each one. */
/* @implements 0x1002FB20 d3d BrScenePropsDraw */
void BrScenePropsDraw(const BrPropList *pList, const BrMat4 *pViewMtx)
{
    uint32_t *pCombine;
    void *pLights;
    void *pMtx;
    uint32_t depthWord;
    int pass;

    s17_emit(0xE7000000u, 0);
    s17_emit(0xBA001402u, 0x00100000u);

    if (g_s17.f690A1C != 0) {
        s17_emit(0xB900031Du, 0x0C192008u);
        g_s17.f690A1C = 0;
    } else {
        s17_emit(0xB900031Du, 0x0C192038u);
    }

    /* The combiner command is reserved first and filled in by 0x1002F900. */
    pCombine = g_s17.pGfx;
    g_s17.pGfx = pCombine + 2;
    BrRdpSetCombineLERP((BrGfxWords *)pCombine,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3EA, 0x3E9, 0x3F5, 0x3E9,
                        0x3E8, 0,     0x3EC, 0,
                        0,     0,     0,     0x3E8);

    s17_emit(0xF9000000u, 0);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, g_s17.f6C0258);
    s17_emit(0xBA000602u, g_s17.f6C0688);
    s17_emit(0xBA000402u, g_s17.f6C0920);
    s17_emit(0xB7000000u, 1);
    s17_emit(0xB9000002u, 1);
    s17_emit(0xBA001102u, 0);
    s17_emit(0xBA001001u, 0x00010000u);
    s17_emit(0xBA000E02u, 0);
    s17_emit(0xBA000C02u, g_s17.f6C0258);
    s17_emit(0xB6000000u, 0x00853200u);

    /* `neg / sbb / and 0xFFFFF000 / add 0x2000` -- 0x1000 when the two
     * globals differ, 0x2000 when they are equal. */
    depthWord = (g_s17.f6C3364 ^ g_s17.f6C1174) ? 0x1000u : 0x2000u;
    s17_emit(0xB7000000u, depthWord | 0x000A0205u);

    pLights = BrX10069530();
    BrLightDirsFromLookAt(g_s17.pLightMtx, (BrLightPair *)pLights,
                          0.0f, -1.0f, 15.0f,
                          0.0f,  0.0f,  0.0f,
                          0.0f,  1.0f,  0.0f);
    s17_emit(0x03840010u, s17_ptrword(pLights));
    s17_emit(0x03820010u, s17_ptrword((unsigned char *)pLights + 0x10));

    pMtx = BrX10069490();
    BrMat4Copy(pViewMtx, (BrMat4 *)pMtx);     /* SOURCE first -- br_mat.h */
    s17_emit(0x01040040u, s17_ptrword(pMtx));

    s17_emit(0xBB000001u, 0xFFFFFFFFu);
    s17_emit(0xB6000000u, 0x000C0000u);
    s17_emit(0xE8000000u, 0);
    s17_emit(0xF5100000u, 0x07000000u);
    s17_emit(0xF50001F0u, 0x06000000u);
    s17_emit(0xF5000100u, 0x05000000u);

    for (pass = 0; pass < 2; ++pass) {
        uint32_t want = (pass == 0) ? 1u : 0u;
        uint32_t k = 0;
        const BrPropItem *it;

        if ((uint32_t)pList->count == 0)
            continue;

        it = &pList->items[0];
        do {
            uint32_t bit = ((uint32_t)(unsigned char)(~it->f04) >> 3) & 1u;

            if (bit != want)
                goto next;
            if (it->dl == 0)
                goto next;

            if (it->f05 & 4) {
                const uint32_t col = g_s17.pColAA5D0[it->f04 & 3];
                s17_emit(0xBC00000Au, 0);
                s17_emit(0xBC00040Au, 0);
                s17_emit(0xBC00200Au, col);
                s17_emit(0xBC00240Au, col);
            }
            if (it->f04 & 4)
                s17_emit(0xB6000000u, 0x3000u);
            if ((it->f04 & 0x80) && g_s17.f0AA880 != 0)
                s17_emit(0xB6000000u, 0x200u);

            s17_emit(0xBB000001u, 0xFFFFFFFFu);
            s17_emit(0xE8000000u, 0);

            {
                void *pItemMtx = BrX10069490();

                BrMat4Translate(g_s17.pTransMtx, it->x, it->y, it->z);
                BrMat4Copy(g_s17.pTransMtx, (BrMat4 *)pItemMtx);
                s17_emit(0x01040040u, s17_ptrword(pItemMtx));
            }

            s17_emit(0x06000000u, it->dl);
            s17_emit(0xBD000000u, 0);

            if ((it->f04 & 0x80) && g_s17.f0AA880 != 0)
                s17_emit(0xB7000000u, 0x200u);
            if (it->f04 & 4)
                s17_emit(0xB7000000u,
                         (g_s17.f6C3364 ^ g_s17.f6C1174) ? 0x1000u : 0x2000u);
            if (it->f05 & 4) {
                s17_emit(0xBC00000Au, 0xFFFFFF00u);
                s17_emit(0xBC00040Au, 0xFFFFFF00u);
                s17_emit(0xBC00200Au, 0x40404000u);
                s17_emit(0xBC00240Au, 0x40404000u);
            }

        next:
            ++k;
            ++it;
        } while (k < (uint32_t)pList->count);   /* count re-read each pass */
    }

    s17_emit(0xBD000000u, 0);
}

/* ================================================================== */
/* 4. the car table                                                   */
/* ================================================================== */

static unsigned char *s17_car(int i)
{
    return g_s17.pCars + (size_t)i * BR_CAR_STRIDE;
}

/* 0x1002F130 */
/* WHAT IT DOES: adds a car to the race: fetches its colour and its driver's
 * name from the owning player, registers its engine sound, and files it in the
 * car table. The counter is re-read between almost every step rather than being
 * kept, which matters because the owner is recorded against the slot the
 * counter held BEFORE it was bumped. */
/* @implements 0x1002F130 d3d BrCarTableAdd */
void BrCarTableAdd(void *pOwner)
{
    int n = g_s17.nEntB;
    unsigned char *car = s17_car(n);
    int r;

    r = BrX10005DE0(pOwner,
                    car + BR_CAR_OFF_RGB + 0,
                    car + BR_CAR_OFF_RGB + 1,
                    car + BR_CAR_OFF_RGB + 2);

    /* Recomputed from the (unchanged) counter in the original, not cached. */
    BrX10076AE0(s17_car(g_s17.nEntB), r);

    strcpy((char *)(s17_car(g_s17.nEntB) + BR_CAR_OFF_NAME),
           BrX10005E70(pOwner));

    BrX10068260(g_s17.nEntB,
                s17_ld32(s17_car(g_s17.nEntB) + BR_CAR_OFF_TAG));

    n = g_s17.nEntB;
    g_s17.nEntB = n + 1;
    /* The owner pointer lands in the record the OLD counter selects. */
    s17_st32(s17_car(n) + BR_CAR_OFF_OWNER, s17_ptrword(pOwner));
    g_s17.nEntA = g_s17.nEntA + 1;
}

/* 0x1002F230 */
/* WHAT IT DOES: takes a player's car out of the race when that player leaves.
 * It marks the car inactive, silences its engine, and clears it out of every
 * slot that was pointing at it. It scans the whole table rather than stopping
 * at the first match, so a player owning more than one car loses all of
 * them. */
/* @implements 0x1002F230 d3d BrCarTableRemove */
void BrCarTableRemove(const void *pOwner)
{
    int i = 0;
    int arg = 0;                      /* edi: bumped by 2 per record */

    if (g_s17.nEntB <= 0)
        return;

    do {
        unsigned char *car = s17_car(i);

        if (s17_ld32(car + BR_CAR_OFF_OWNER) == s17_ptrword(pOwner)) {
            int j;

            s17_st32(car + BR_CAR_OFF_ACTIVE, 0);
            BrX10072580(arg);

            for (j = 0; j < g_s17.nEntA; ++j) {
                unsigned char *slot = g_s17.pSlots
                                    + (size_t)j * BR_SLOT_STRIDE
                                    + BR_SLOT_OFF_CARPTR;
                if (s17_ld32(slot) == s17_ptrword(car))
                    s17_st32(slot, 0);
            }
        }

        ++i;
        arg += 2;
    } while (i < g_s17.nEntB);         /* the count is re-read every pass */
}

/* 0x1002F2A0 */
/* WHAT IT DOES: takes a copy of each car's championship figures -- points and
 * the other per-car running totals -- and notes that a copy now exists, so the
 * results can be put back after whatever is about to happen. */
/* @implements 0x1002F2A0 d3d BrCarStateSave */
void BrCarStateSave(void)
{
    int i;

    for (i = 0; i < g_s17.nCars; ++i) {
        unsigned char *car = s17_car(i);
        int n;

        g_s17.pSave5C8[i] = s17_ld32(car + BR_CAR_OFF_SAVE4);
        g_s17.pSave728[i] = s17_ld32(car + BR_CAR_OFF_SAVE3);
        g_s17.pSave9C0[i] = s17_ld32(car + BR_CAR_OFF_SAVE1);
        g_s17.pSave748[i] = s17_ld32(car + BR_CAR_OFF_SAVE2);
        g_s17.pSave5B0[i] = s17_ld32(car + BR_CAR_OFF_SAVE0);

        n = g_s17.nSaveDwords;
        if (n > 0)
            memcpy(g_s17.pSave950 + (size_t)i * 12,
                   car + BR_CAR_OFF_SAVEVEC, (size_t)n * 4);
    }

    g_s17.f6909B8 = 1;
}

/* 0x1002F320 */
/* WHAT IT DOES: puts each car's saved championship figures back. When the game
 * is in the right mode and a saved copy exists, it also patches the numbers
 * straight into the drawing commands for the standings display, so the points
 * on screen match what has just been restored. One of the five saved values is
 * written by the save and never read back by anything. */
/* @implements 0x1002F320 d3d BrCarStateRestore */
void BrCarStateRestore(void)
{
    int i;

    if (g_s17.f0AA010 == 0 && g_s17.f6909B8 != 0) {
        for (i = 0; i < g_s17.nCars; ++i) {
            unsigned char *car = s17_car(i);
            unsigned char *blk;
            uint32_t c = g_s17.pSave5C8[i];
            unsigned n1, n2;
            uint16_t v;

            /* DEVIATION: +0x0E8C holds a host pointer, not a 32-bit one, so
             * it is read at the host's pointer width rather than through
             * s17_ld32. Nothing else in this packet touches that field. */
            memcpy(&blk, car + BR_CAR_OFF_CMDPTR, sizeof blk);

            /* movsx of a signed byte into a 16-bit slot. */
            n1 = blk[4];
            n2 = blk[5];
            v = (uint16_t)(int16_t)g_s17.pTblAA210[c];
            memcpy(blk + (size_t)(n2 + n1 * 4) * 2 + 0x1E, &v, sizeof v);

            /* GOTCHA: three different addressings of the same index. This
             * one adds n2 as BYTES and n1*4 as bytes, then +6. */
            n2 = blk[5];
            n1 = blk[4];
            blk[n2 + n1 * 4 + 6] = (unsigned char)(g_s17.pSave5C8[i] & 0xFFu);

            /* ...this one scales (n2 + n1*4 + 0x14) by 4. */
            n2 = blk[5];
            n1 = blk[4];
            s17_st32(blk + (size_t)(n2 + n1 * 4 + 0x14) * 4,
                     g_s17.pSave728[i]);

            /* ...and this reads back the u16 written first. */
            n2 = blk[5];
            n1 = blk[4];
            memcpy(&v, blk + (size_t)(n2 + n1 * 4) * 2 + 0x1E, sizeof v);
            /* DEVIATION: the stub takes an intptr_t first argument so that a
             * string literal and the integer call sites in 0x1002C210 can
             * share one declaration. 0x10008B80 is a bare `ret` anyway. */
            BrStub10008B80((intptr_t)(const void *)"points = %d\n",
                           (unsigned)v);
        }
    }

    for (i = 0; i < g_s17.nCars; ++i) {
        unsigned char *car = s17_car(i);
        int n = g_s17.nSaveDwords;

        if (n > 0)
            memcpy(car + BR_CAR_OFF_SAVEVEC,
                   g_s17.pSave950 + (size_t)i * 12, (size_t)n * 4);

        s17_st32(car + BR_CAR_OFF_SAVE0, g_s17.pSave5B0[i]);
        s17_st32(car + BR_CAR_OFF_SAVE2, g_s17.pSave748[i]);
        s17_st32(car + BR_CAR_OFF_SAVE3, g_s17.pSave728[i]);
        s17_st32(car + BR_CAR_OFF_SAVE4, g_s17.pSave5C8[i]);
        /* pSave9C0 (0x106909C0) is written by the save and never read. */
    }
}

/* ================================================================== */
/* 5. small global glue                                               */
/* ================================================================== */

/* 0x1002BF40 */
/* WHAT IT DOES: asks whether something is already in a list. A null thing is
 * reported as present without the list being looked at, which is how callers
 * get "nothing to do" for free. */
/* @implements 0x1002BF40 d3d BrPtrListContains */
#ifdef BR_MATCHING_BUILD
/* The original takes only pv and reads the list at two absolute addresses
 * (count 0x1067B548, array 0x1067B550).  The port passes the list in. */
extern int   g_br67B548;
extern void *g_br67B550[];

int BrPtrListContains(const void *pv)
{
    int i;
    int n;

    if (pv == NULL)
        return 1;                    /* NULL short-circuits to "present" */

    n = g_br67B548;
    for (i = 0; i < n; ++i)
        if (g_br67B550[i] == pv)
            return 1;

    return 0;
}
#else
int BrPtrListContains(const BrPtrList *pList, const void *pv)
{
    int i;

    if (pv == NULL)
        return 1;                    /* NULL short-circuits to "present" */

    if (pList->n <= 0)
        return 0;

    for (i = 0; i < pList->n; ++i)
        if (pList->ap[i] == pv)
            return 1;

    return 0;
}
#endif

/* 0x1002C210 */
void BrS17BankFlip(void)
{
    int i;
    int bank;
    uint32_t *hdr;
    unsigned char *buf;

    for (i = 0; i < 3; ++i)
        BrStub10008B80(i, 0xFF, 0, 0xFF, 0x7F);

    g_s17.bank ^= 1;
    g_s17.bank578 = BrX10060E90();
    g_s17.bank57C = 0;

    bank = g_s17.bank;
    hdr = g_s17.pBankHdr + (size_t)bank * 3;      /* lea [ecx*4 + base], ecx=3n */
    buf = g_s17.pBankBuf + (size_t)bank * 3 * 0x800;

    hdr[0] = 0;
    hdr[1] = 0;
    hdr[2] = 0;

    for (i = 0; i < 3; ++i)
        s17_st32(buf + (size_t)i * 0x800, 0);
}

/* 0x1002C2A0 */
/* WHAT IT DOES: lets go of one particular long-lived object. What that object
 * is was not established here. */
/* @implements 0x1002C2A0 d3d BrS17Release */
void BrS17Release(void)
{
    /* `mov ecx, 0x106806B0 / jmp 0x100751D0`. The operand is an IMMEDIATE --
     * the ADDRESS of the frame-timer object, not a pointer loaded out of a
     * field -- so the object itself is named here and its address taken.
     * Routing this through g_s17.pThis6806B0 costs a `mov eax, [mem]` the
     * original does not have. */
    BrX100751D0(g_br6806B0);
}

/* 0x1002C2B0 */
/* WHAT IT DOES: books a tidy-up routine to run automatically when the program
 * exits, and reports whether the booking was accepted. */
/* @implements 0x1002C2B0 d3d BrS17RegisterAtExit */
int BrS17RegisterAtExit(void)
{
    return BrXAtExit(BrX1002C2C0);
}

/* 0x1002C2D0 */
/* WHAT IT DOES: draws the scene, but only if drawing is switched on at all.
 * In one particular mode it temporarily zeroes one setting across the draw and
 * puts it back afterwards, so that mode renders differently without the setting
 * being permanently changed. */
/* @implements 0x1002C2D0 d3d BrS17DrawGated */
void BrS17DrawGated(void)
{
    /* DEVIATION: the original's local is an uninitialised `push ecx` slot.
     * It is only ever read when the callee flips 0x106909B0 to -1 without
     * it having been -1 beforehand, in which case the original writes
     * garbage. Seeding from the current value keeps the port deterministic
     * and makes that path a no-op instead of a corruption. */
    int saved = g_s17.f6C2CFC;

    if (g_s17.f6909B0 == 0)
        return;

    if (g_s17.f6909B0 == -1) {
        saved = g_s17.f6C2CFC;
        g_s17.f6C2CFC = 0;
    }

    BrX1003563A(g_s17.f680944);

    if (g_s17.f6909B0 == -1)
        g_s17.f6C2CFC = saved;
}

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

/* 0x10031190 */
/* WHAT IT DOES: hands out the next of thirty-two scratch buffers, cycling
 * round them in turn. If all of them are still in use it waits for one to come
 * free rather than handing out a buffer that is still being read. */
/* @implements 0x10031190 d3d BrScratchRingAlloc */
void *BrScratchRingAlloc(void)
{
    int i;

    if (g_s17.nScratchDepth == BR_SCRATCH_DEPTH)
        BrX10042AF0(g_s17.pScratchWait, 0, 1);   /* NOT incremented here */
    else
        g_s17.nScratchDepth += 1;

    /* MSVC's signed (i + 1) % 32: abs, mask, restore the sign. */
    i = g_s17.iScratch + 1;
    if (i < 0)
        i = -((-i) & 0x1F);
    else
        i = i & 0x1F;
    g_s17.iScratch = i;

    return g_s17.pScratch + (ptrdiff_t)i * BR_SCRATCH_STRIDE;
}

/* 0x100311E4 */
/* WHAT IT DOES: waits until every scratch buffer that has been handed out has
 * come back, which is how the game makes sure the graphics hardware has
 * finished with them before going further. */
/* @implements 0x100311E4 d3d BrScratchRingDrain */
void BrScratchRingDrain(void)
{
    while (g_s17.nScratchDepth != 0) {
        BrX10042AF0(g_s17.pScratchWait, 0, 1);
        g_s17.nScratchDepth -= 1;
    }
}

/* 0x10031212 -- DEVIATION: the original zeroes its own two argument slots
 * on the caller's stack. C parameters are by value, so the stores are not
 * observable and are omitted; the return value is what callers use. */
/* WHAT IT DOES: nothing. It takes two arguments, ignores both, and always
 * answers zero -- a placeholder that fits where a real routine would go. */
/* @implements 0x10031212 d3d BrScratchRingNull */
int BrScratchRingNull(int a0, int a1)
{
    (void)a0;
    (void)a1;
    return 0;
}

/* 0x10031227 */
void BrRenderCountersReset(void)
{
    g_s17.f6C32CC = 0;
    g_s17.f6C56DC = g_s17.f6C32CC;
    g_s17.f6C1178 = g_s17.f6C56DC;

    g_s17.f6C161C = 0;
    g_s17.f6C1610 = g_s17.f6C161C;

    g_s17.f6C33B8 = 0;
    g_s17.f6C06A4 = g_s17.f6C33B8;
    g_s17.f6C069C = g_s17.f6C06A4;
}

/* 0x10031282 */
/* WHAT IT DOES: sets the screen dimensions up at start-up, by doing exactly
 * what the routine below does and nothing else. */
/* @implements 0x10031282 d3d BrScreenSizeInit */
void BrScreenSizeInit(void)
{
    BrScreenSizeApply();
}

/* 0x1003128C */
/* WHAT IT DOES: sets the game's working screen size back to the build's
 * defaults -- 640 by 480 here. */
/* @implements 0x1003128C d3d BrScreenSizeApply */
void BrScreenSizeApply(void)
{
    g_s17.screenW = g_s17.defaultW;     /* 0x100A81C0 = 640 in this build */
    g_s17.screenH = g_s17.defaultH;     /* 0x100A81C4 = 480               */
}

/* 0x10031342 */
/* WHAT IT DOES: nothing at all. It exists so that something expecting a
 * routine to call has one. */
/* @d3donly 0x10031342 BrTexNoOp -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
void BrTexNoOp(void)
{
}

/* 0x10031347 */
/* WHAT IT DOES: works out how many bits are needed to count up to a given
 * texture dimension -- which is what the hardware wants instead of the size
 * itself -- and reports an error for anything above 1024, in which case it
 * leaves the answer as whatever it was. Its other output is always the same
 * fixed value regardless of the size asked about. */
/* @implements 0x10031347 d3d BrTexSizeShift */
void BrTexSizeShift(int size, int *pOut1, int *pOut2)
{
    int n = size - 1;

    if ((n & ~0xFF) != 0) {                 /* `and cl, 0` then test ecx */
        if ((n & ~0x3FF) != 0) {
            char buf[64];
            /* 0x1007C830 is the CRT's sprintf; the message carries n, not
             * the size the caller asked for. */
            snprintf(buf, sizeof buf, "ERROR: unhandled texture size: %d", n);
            BrX10035BBA(buf);
            /* *pOut2 is deliberately left untouched on this path. */
        } else if ((n & ~0x1FF) != 0) {
            *pOut2 = 10;
        } else {
            *pOut2 = 9;
        }
    } else if ((n & 0xF0) != 0) {
        if ((n & 0xC0) != 0)
            *pOut2 = (n & 0x80) ? 8 : 7;
        else
            *pOut2 = (n & 0xE0) ? 6 : 5;
    } else if ((n & 0xFC) != 0) {
        *pOut2 = (n & 0xF8) ? 4 : 3;
    } else if ((n & 0xFE) != 0) {
        *pOut2 = 2;
    } else if (n != 0) {
        *pOut2 = 1;
    } else {
        *pOut2 = 0;
    }

    *pOut1 = 0xFFFF;
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
