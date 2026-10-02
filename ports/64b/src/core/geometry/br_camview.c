/* br_camview.c -- geometry: the camera view matrix and the sky angles.
 *
 * RESPONSIBILITY: the per-frame camera set-up -- 0x1002A050 BrMat4LookAt
 * (world -> camera) and 0x1002A200 BrLightDirsAndAngles (the two light
 * directions and the sky scroll angles, built on top of LookAt).
 *
 * Moved here out of src/core/slice2_17.c (an address batch, not a module).
 * The batch's preamble is carried over as it was -- including the
 * cross-slice extern declarations and the two globals, now as extern
 * declarations -- because the translation unit's symbol table is load-
 * bearing for BrLightDirsAndAngles: without those declarations VC5
 * schedules its tail differently (refile checked byte-identical against
 * the batch compile for both functions).
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_mat.h"   /* br_globals: its objects */
#include "br_race.h"   /* br_globals: its objects */
#include "slice2_17.h"
#include "br_addr32.h"
#undef BrPtrListContains

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
extern BrS17State g_s17;

/* BrScenePropsDraw's fixed storage (0x100AA5D0, 0x106C08A0, 0x106C0860) and
 * its S17PropItem view went with it to src/core/drawing/br_sceneprops.c. */

/* 0x106806B0 -- the 0x24-byte frame-timer object 0x100751D0 / 0x10075240
 * operate on. slice8_86.c treats it as an opaque byte image (see its
 * br86_ld32 / br86_st32 accessors); it is only ever named by its address, so
 * it is defined here as raw storage. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


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
#define BRS17_THISCALL __fastcall
/* BrX100751D0: prototype in br_funcs.h */
/* XSLICE 0x1002C2C0 */
/* BrX1002C2C0: prototype in br_funcs.h */
/* XSLICE 0x1003563A */
/* BrX1003563A: prototype in br_funcs.h */
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
/* BrX10005DE0: prototype in br_funcs.h */
/* XSLICE 0x10076AE0 */
/* BrX10076AE0: prototype in br_funcs.h */
/* XSLICE 0x10005E70 */
/* BrX10005E70: prototype in br_funcs.h */
/* XSLICE 0x10068260 */
/* BrX10068260: prototype in br_funcs.h */
/* XSLICE 0x10072580 */
/* BrX10072580: prototype in br_funcs.h */
/* XSLICE 0x10042AF0 */
extern void  BrX10042AF0(void *p, int a1, int a2);
/* XSLICE 0x10035BBA */
/* BrX10035BBA: prototype in br_funcs.h */
/* XSLICE 0x10069530 */
/* BrX10069530: prototype in br_funcs.h */
/* XSLICE 0x10069490 */
/* BrX10069490: prototype in br_funcs.h */
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1).
 * Anything at or above 0x1007CC40 is statically linked MSVC CRT, so the
 * platform's own atexit is used instead of porting it. */
/* BrXAtExit: prototype in br_funcs.h */


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

    /* z = normalise(eye - at). The original is `fld dword; fsub dword`: the
     * x87 subtracts the two floats in 80 bits and the result is stored to a
     * double, so the difference is EXACT. Under VC5 the plain float form is
     * what emits that pair -- casting both operands to double first costs
     * fourteen bytes and six extra x87 slots -- but on a target without the
     * 80-bit stack `xEye - xAt` would round to float, so the port keeps the
     * casts and only the matching build drops them. Same value either way on
     * the original hardware. */
    z.x = xEye - xAt;
    z.y = yEye - yAt;
    z.z = zEye - zAt;
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


    /* The nine basis stores first (column by column), then the translation
     * row -dot(eye, axis) for each axis, then the last column. The eye
     * components are read from the PARAMETERS, not from double locals.
     *
     * The translation's leading zero is a FLOAT literal: `0.0f - a*b - ...`.
     * VC5 folds the float zero minus a double product into a plain `fchs`
     * that it schedules like a unary op, which lets the three chains
     * interleave exactly as the original does; the double `0.0 -` and
     * `-(a*b)` spellings pin the negation early and cost 125..141 bytes of
     * scheduling residue.  (The zero-minus form differs from -(dot) only in
     * the sign of a zero result, which no consumer of a view matrix sees.)
     * The m[k][3] zeros are stored from the bottom row up. */
    pM->m[0][0] = (float)x.x; pM->m[1][0] = (float)x.y; pM->m[2][0] = (float)x.z;
    pM->m[0][1] = (float)y.x; pM->m[1][1] = (float)y.y; pM->m[2][1] = (float)y.z;
    pM->m[0][2] = (float)z.x; pM->m[1][2] = (float)z.y; pM->m[2][2] = (float)z.z;
    pM->m[3][0] = (float)(0.0f - xEye * x.x - yEye * x.y - zEye * x.z);
    pM->m[3][1] = (float)(0.0f - xEye * y.x - yEye * y.y - zEye * y.z);
    pM->m[3][2] = (float)(0.0f - xEye * z.x - yEye * z.y - zEye * z.z);
    pM->m[2][3] = 0.0f;
    pM->m[1][3] = 0.0f;
    pM->m[0][3] = 0.0f;
    pM->m[3][3] = 1.0f;
}

/* (port-only s17_pack_dirs removed) */


/* dot(column c of pM, v), in the original's summation order:
 *      (m2*v.z + m1*v.y) + m0*v.x     for columns 0 and 2
 *      (m2*v.z + m0*v.x) + m1*v.y     for column 1
 * The two orders really are different in the original; column 1 is always
 * computed by the shorter three-term chain that starts with z and x. */
/* (port-only s17_dot_col_zyx removed) */


/* (port-only s17_dot_col_zxy removed) */


/* dot(column 2, v) is built as (m1*v.y + m2*v.z) + m0*v.x -- note the pair
 * is (y, z) here where column 0 uses (z, y). Preserved. */
/* (port-only s17_dot_col_yzx removed) */


/* 0x1007C8A0 __ftol: truncate toward zero, take the low dword. */
/* (port-only s17_ftol removed) */


/* 0x10030B50 */
/* WHAT IT DOES: does the lighting set-up of its neighbour above and then works
 * out where two given directions land on the sky -- as a pair of horizontal and
 * vertical angles each, which is how the sky texture is scrolled to follow the
 * camera. The first pair is measured against a fixed scale and the second
 * against one the caller supplies. */
/* @t3 0x1002A200 2026-09-26 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 647/645 insns 203/202 rows 1+2 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: t1's (double)(n * 4) factor is `fild; fmulp` where the original
 * fuses `fimul` (same value, same rounding); the spellings that fuse also
 * CSE n*4 into esi.  Dossier and dead list in the matching arm below.  Do
 * not reopen before the end-grind. */
/* @implements 0x10030B50 d3d BrLightDirsAndAngles */
/* The original inlines every port helper: the packs, the three column dots
 * (as macros, in the original's term orders), atan2 as fpatan and __ftol as
 * a cast.  ONE vector local serves both directions -- the second direction
 * reloads the same frame slots.
 * The frame is ONE 0x30-byte block: a four-double vector (the w slot
 * unused) followed by the t spill and one more double, which is what puts
 * t at +0x20; separate locals put t at +0 whatever their order or names.
 * Each count is written n * 4 for the integer and (double)(n << 2) for the
 * factor, so VC5 keeps n in a callee-saved register and forms n*4 twice.
 * RESIDUE (22 B): t1's factor is `fild; fmulp` where the original fuses
 * `fimul`; the (double)(n * 4) spelling fuses but CSEs n*4 with the
 * integer and keeps the product, not n, in esi.
 * @t4-pass 0x1002A200 1 2026-09-26 probes 117 bytes 647 insns 203 regions 1 rows 3 census no  (t1 factor spellings: (double) of n*4 / n<<2 / 4*n / unsigned forms, int temp m in three placements, factor-first and paren orders)
 * @t4-pass 0x1002A200 2 2026-09-26 probes 85 bytes 647 insns 203 regions 1 rows 3 census yes  (int-part respellings x factor forms; mechanism: int/double pad functions 1..80, extern counts 20..3000, four CRT headers -- all inert, the fimul fork is instruction selection) */
#define L_ZYX(c, v) ((double)pM->m[2][c] * (v).z + (double)pM->m[1][c] * (v).y + (double)pM->m[0][c] * (v).x)
#define L_ZXY(c, v) ((double)pM->m[2][c] * (v).z + (double)pM->m[0][c] * (v).x + (double)pM->m[1][c] * (v).y)
#define L_YZX(c, v) ((double)pM->m[1][c] * (v).y + (double)pM->m[2][c] * (v).z + (double)pM->m[0][c] * (v).x)
void BrLightDirsAndAngles(BrMat4 *pM, BrLightPair *pLights,
                          BrSkyAngles *pAngles,
                          float xEye, float yEye, float zEye,
                          float xAt,  float yAt,  float zAt,
                          float xUp,  float yUp,  float zUp,
                          float xA, float yA, float zA,
                          float xB, float yB, float zB,
                          int nS1, int nT1)
{
    struct { BrVec3d v; double w; double t; double u; } L;
#define v L.v

    BrMat4LookAt(pM, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    pLights->dir0[0] = FUN_1002a490((double)pM->m[0][0]);
    pLights->dir0[1] = FUN_1002a490((double)pM->m[1][0]);
    pLights->dir0[2] = FUN_1002a490((double)pM->m[2][0]);
    pLights->dir1[0] = FUN_1002a490((double)pM->m[0][1]);
    pLights->dir1[1] = FUN_1002a490((double)pM->m[1][1]);
    pLights->dir1[2] = FUN_1002a490((double)pM->m[2][1]);

    v.x = (double)xA; v.y = (double)yA; v.z = (double)zA;
    BrVec3dNormalise(&v);
    L.t = L_ZXY(1, v);
    pAngles->s0 = 0x100 - (int32_t)(atan2(L_ZYX(0, v), L_YZX(2, v)) * BR_ANG_K256);
    pAngles->t0 = 0x100 - (int32_t)(asin(L.t) * BR_ANG_K256);

    v.x = (double)xB; v.y = (double)yB; v.z = (double)zB;
    BrVec3dNormalise(&v);
    L.t = L_ZXY(1, v);
    pAngles->s1 = nS1 * 4
        - (int32_t)(atan2(L_ZYX(0, v), L_YZX(2, v)) * (double)(nS1 << 2) * BR_ANG_K1);
    pAngles->t1 = nT1 * 4
        - (int32_t)(asin(L.t) * (double)(nT1 << 2) * BR_ANG_K1);
#undef v

}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrVec3Direction: prototype in br_funcs.h */

#ifndef BR_DLCMD_DEFINED
#define BR_DLCMD_DEFINED
typedef struct BrDlCmd { int op; int arg; } BrDlCmd;
#endif
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: build the camera matrix for the current view by pointing it
 * from the camera's position at its target, squaring up the three axes with
 * two cross products so they are mutually perpendicular, then pulling the
 * eye slightly back along its own forward axis. Does nothing if there is no
 * active camera. */
/* @implements 0x10011D20 glide FUN_10011d20 */
void FUN_10011d20(void)

{
  BrMat4 m;
  
  if (((*(int *)&DAT_105ccb68[4]) != 0) && (g_pBrRaceLapRec != 0)) {
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.m[2][0] = 0.0f;
    m.m[2][1] = 0.0f;
    m.m[2][2] = 1.0f;
    BrVec3Direction((BrVec3 *)m.m[1], (BrVec3 *)(g_pBrRaceLapRec + 0x4c), (BrVec3 *)(g_pBrRaceLapRec + 0x58));
    BrVec3Cross((BrVec3 *)m.m[0], (BrVec3 *)m.m[1], (BrVec3 *)m.m[2]);
    BrVec3Cross((BrVec3 *)m.m[1], (BrVec3 *)m.m[2], (BrVec3 *)m.m[0]);
    m.m[3][0] = *(float *)(g_pBrRaceLapRec + 0x4c);
    m.m[3][1] = *(float *)(g_pBrRaceLapRec + 0x50);
    m.m[3][2] = (*(float *)(g_pBrRaceLapRec + 0x54) + g_brRaceFade) - _DAT_10077284;
    BrVec3MulAdd((BrVec3 *)m.m[3], (BrVec3 *)m.m[3], (BrVec3 *)m.m[0], -0.3f);
    BrVec3MulAdd((BrVec3 *)m.m[3], (BrVec3 *)m.m[3], (BrVec3 *)m.m[1], -0.6f);
    { BrDlCmd *pEmit_ = (*(BrDlCmd * *)&g_BrGfxPtr)++; pEmit_->op = 0x1060040; pEmit_->arg = (int)&(DAT_100a9ec0[0]); }
    { BrDlCmd *pEmit_ = (*(BrDlCmd * *)&g_BrGfxPtr)++; pEmit_->op = 0x1030040; pEmit_->arg = br_addr32(g_BrMtxSlot); }
    BrMat4Scale(&g_BrDrawCombined, 0.0009765625f, 0.0009765625f, 0.0009765625f);
    BrMat4Mul(&g_BrDrawCombined, &m, &m);   /* m = view * m (slice1_05's A, B, out order) */
    BrScenePropsDraw((const BrPropList *)DAT_105bcaec, &m);
  }
  return;
}
