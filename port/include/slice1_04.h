/* slice1_04.h -- decompiled from BRD3D.dll, address range 0x1001DDB0-0x1002A8A0.
 *
 * What is implemented here:
 *   0x100251A0  BrTexShiftFromSize
 *   0x10027B90  BrTexFormatCode
 *   0x10028200  BrTexAspectFromSize
 *   0x10028630  BrTblFind
 *   0x10028720  BrTexSizeFromShiftAspect
 *   0x1002A8A0  BrEarLoad
 *
 * NOT implemented here, deliberately -- see the note immediately below:
 *   0x1001DDB0, 0x1001DEF0, 0x1001E030   three more frustum clip planes.
 *
 * Field names are positional (fNN = byte offset NN) wherever the meaning was
 * not established from the code itself.
 */
#ifndef SLICE1_04_H
#define SLICE1_04_H

#include <stdint.h>

/* ==========================================================================
 * 0x1001DDB0 / 0x1001DEF0 / 0x1001E030 -- SKIPPED, belongs in slice1_03.c
 * ==========================================================================
 *
 * These three are the remaining half of the clip-plane family whose shared
 * body, node pool and free list agent03 already ported (BrClipPlane in
 * port/src/slice1_03.c, plus BrClipVert / BrClipList / BrClipLerpVert /
 * BrClipPoolInit in slice1_03.h). All six originals are byte-for-byte
 * identical apart from the two x87 loads that compute the plane distance:
 *
 *      0x1001D810   d = f18            BrClipPlaneW           (agent03)
 *      0x1001D9F0   d = f18 + f04      BrClipPlaneWPlusF04    (agent03)
 *      0x1001DB30   d = f18 - f04      BrClipPlaneWMinusF04   (agent03)
 *      0x1001DC70   d = f08 + f18      BrClipPlaneWPlusF08    (agent03)
 *      0x1001DDB0   d = f18 - f08      <- this packet
 *      0x1001DEF0   d = f0C + f18      <- this packet
 *      0x1001E030   d = f18 - f0C      <- this packet
 *
 * Reimplementing them here would fork the 64-node pool and its free list into
 * two independent copies, which is a correctness hazard, not just
 * duplication. The integration is instead three lines in slice1_03.c beside
 * the four that are already there:
 *
 *      static float BrClipDistWMinusF08(const BrClipVert *pV)
 *      { return pV->f18 - pV->f08; }                       // 0x1001DDB0
 *      static float BrClipDistWPlusF0C(const BrClipVert *pV)
 *      { return pV->f0C + pV->f18; }                       // 0x1001DEF0
 *      static float BrClipDistWMinusF0C(const BrClipVert *pV)
 *      { return pV->f18 - pV->f0C; }                       // 0x1001E030
 *
 * plus the three matching one-line wrappers calling BrClipPlane().
 *
 * Cross-check performed while decompiling these three independently: the
 * shared body in slice1_03.c agrees with all three of them instruction for
 * instruction, including the two exit tests in order (count < 2 first, then
 * the iteration counter), the unconditional `pList->pHead = pCur` rotation,
 * the count being left ALONE on the "leaving" edge, and the pool-bounded
 * release walk that looks one node ahead before overwriting each link.
 * Two details are worth recording because they are easy to get backwards:
 *
 *   - The lerp argument order is (outside, inside) with t = dOut/(dOut-dIn)
 *     on BOTH the entering and leaving edges. The originals swap which of
 *     the two loop cursors is which, so the argument order flips between the
 *     two call sites while the meaning stays constant.
 *   - The comparison must be written `inside = (d >= 0.0f)`, not
 *     `outside = (d < 0.0f)`. The original branches on the x87 C0 bit, which
 *     is set for unordered as well as less-than, so a NaN distance is
 *     OUTSIDE. The two spellings differ only on NaN, and only one is right.
 *
 * The constant the originals compare against, 0x1008F3C8, was read out of
 * the shipped DLL: it is 0x00000000, i.e. 0.0f.
 */

/* ==========================================================================
 * Texture size codecs -- 0x100251A0, 0x10028200, 0x10028720
 * ==========================================================================
 *
 * These three are a matched set. 0x100251A0 and 0x10028200 encode a (w,h)
 * pair as a "shift" (0..8, giving the larger dimension as 256 >> shift) plus
 * an "aspect" code (0..6, from 8:1 wide through 1:1 to 1:8 tall);
 * 0x10028720 decodes the pair back. Round-tripping is exact for
 * power-of-two dimensions in 1..256 whose ratio is at most 8:1.
 */

/* 0x100251A0  *pShift = 8 - ceil(log2(max(a,b))), clamped so that
 * max(a,b) <= 1 gives 8. Returns 1 on success.
 *
 * If max(a,b) > 256 it writes 0 and returns 0 -- note the failure path still
 * writes the output, it does not leave it alone.
 *
 * The original is two textually identical ladders, one on `a` when a > b and
 * one on `b` otherwise, so the result depends only on max(a,b) and the
 * function is symmetric in its two size arguments. Non-positive sizes fall
 * into the `<= 1` rung and yield 8. */
int BrTexShiftFromSize(int *pShift, int a, int b);

/* 0x10028200  *pCode = aspect code for (a, b):
 *
 *      a/b = 8  -> 0     a/b = 4 -> 1     a/b = 2 -> 2
 *      a   = b  -> 3
 *      b/a = 2  -> 4     b/a = 4 -> 5     b/a = 8 -> 6
 *
 * Returns 1 when the ratio is one of those exact powers of two, 0 otherwise
 * (in which case *pCode is the nearest code on that side -- ratios beyond 8:1
 * clamp to 0 or 6). The ratio is computed as (8 * larger) / smaller with
 * signed truncating integer division.
 *
 * GOTCHA: `b` must not be 0 when a > b, and `a` must not be 0 otherwise --
 * the original divides without a guard and this port preserves that. */
int BrTexAspectFromSize(int *pCode, int a, int b);

/* 0x10028720  inverse of the two above.
 *
 *      *pA = 256 >> shift        (shift > 8 is treated as 8, giving 1)
 *      then, per aspect code:
 *        0: *pB = *pA / 8        1: *pB = *pA / 4      2: *pB = *pA / 2
 *        3: *pB = *pA
 *        4: *pB = *pA, *pA /= 2  5: *pB = *pA, *pA /= 4
 *        6: *pB = *pA, *pA /= 8
 *
 * GOTCHA: an aspect code above 6 leaves *pB COMPLETELY UNWRITTEN (the
 * original's second jump table falls through to a bare `ret`), while *pA has
 * already been set. There is no return value to signal it. */
void BrTexSizeFromShiftAspect(int *pA, int *pB, int shift, int aspect);

/* 0x10027B90  maps an (a, b, c) triple to a small code. The meaning of the
 * inputs and of the returned codes was not established, so both are left
 * positional; the mapping itself is exact:
 *
 *      a == 0 && b == 4  ->  c == 1 ? 11 : 2
 *      a == 1 && b == 3  ->  c == 1 ? 12 : 4
 *      a == 1 && b == 4  ->  2
 *      anything else     ->  11
 *
 * 11 is the catch-all, reached by every unhandled combination including
 * a >= 2. In the original the `c == 1` tests are the branchless
 * `dec/neg/sbb/and/add` idiom, which is where the odd constant pairs
 * (0xF7,11) and (0xF8,12) come from. */
int BrTexFormatCode(int a, int b, int c);

/* ==========================================================================
 * Record table search -- 0x10028630
 * ==========================================================================
 *
 * Linear search of an array of 0x2B8-byte records for one matching a probe
 * record. Every compared offset in the table entry is exactly 0x50 below the
 * same offset in the probe, which is what shows the two are the same record
 * type and that the table starts at 0x1057543C + 4 (the original walks a
 * cursor from 0x1057543C + 0x54 in steps of 0x2B8, touching cursor-4,
 * cursor+0, cursor+0x218 and cursor+0x244..0x24B).
 *
 * Count comes from 0x105553F0.
 */
#define BR_TBL_REC_SIZE 0x2B8

typedef struct BrTblRec {
    unsigned char pad00[0x4C];
    int32_t       f4C;                          /* +0x4C */
    int32_t       f50;                          /* +0x50 */
    unsigned char pad54[0x268 - 0x54];
    int32_t       f268;                         /* +0x268 */
    unsigned char pad26C[0x294 - 0x26C];
    unsigned char f294[8];                      /* +0x294 .. +0x29B */
    unsigned char pad29C[BR_TBL_REC_SIZE - 0x29C];
} BrTblRec;

/* Returns the index of the first matching record, or -1.
 *
 * A record matches when f4C and f50 are equal AND EITHER record has
 * f268 != 1, or (both have f268 == 1 and) the eight bytes at +0x294 are
 * equal. The f268 != 1 short-circuit ACCEPTS the record -- it is not a skip.
 * That asymmetry is easy to invert by accident. */
int BrTblFind(const BrTblRec *aRecs, unsigned int count, const BrTblRec *pRec);

/* ==========================================================================
 * EAR "Interactive Around-Sound" loader -- 0x1002A8A0
 * ========================================================================== */

/* Opaque function pointer; the real prototypes live in the EAR SDK. */
typedef void (*BrEarProc)(void);

/* DEVIATION: the original calls the Win32 imports directly. They are behind
 * this vtable so the file builds and can be exercised off Windows. */
typedef struct BrEarPlatform {
    void        *(*pfnGetModuleHandle)(const char *pszName);
    void        *(*pfnLoadLibrary)(const char *pszName);
    BrEarProc    (*pfnGetProcAddress)(void *hModule, const char *pszName);
    unsigned int (*pfnRegisterWindowMessage)(const char *pszName);
} BrEarPlatform;

/* Resolution order, which is also the order the original stores them in.
 * The global each slot came from is listed for the coordinator's benefit. */
enum {
    BR_EAR_AAA_VALIDATE = 0,        /* 0x105754B0  _EAR_DLL_AAA_Validate@4 */
    BR_EAR_ASSIGN_HWND,             /* 0x1057548C */
    BR_EAR_CHANGE_CHANNEL_CONTROL,  /* 0x10575484 */
    BR_EAR_CLEAR_CHANNEL,           /* 0x10575480 */
    BR_EAR_EAR_INACTIVE,            /* 0x10575460 */
    BR_EAR_GET_EVENT_STATUS,        /* 0x105754B4 */
    BR_EAR_GET_LAST_ERROR,          /* 0x105754C4 */
    BR_EAR_GET_VERSION,             /* 0x10575450 */
    BR_EAR_INITIALIZE_EAR,          /* 0x105754C0 */
    BR_EAR_MIX_EVENT,               /* 0x1057546C */
    BR_EAR_MOVE_EVENT,              /* 0x10575488 */
    BR_EAR_REGISTER_BANK,           /* 0x105754CC */
    BR_EAR_REGISTER_CHANNEL,        /* 0x105754A0 */
    BR_EAR_REGISTER_ENVIRONMENT,    /* 0x105754AC */
    BR_EAR_REGISTER_MATRIX,         /* 0x105754D0 */
    BR_EAR_REGISTER_PRESET,         /* 0x105754C8 */
    BR_EAR_RESET_EAR,               /* 0x10575494 */
    BR_EAR_SET_ATTENUATION_LEVEL,   /* 0x10575454 */
    BR_EAR_SET_USER_DISTANCE_UNIT,  /* 0x105754B8 */
    BR_EAR_SHOW_LAST_ERROR,         /* 0x105754A8 */
    BR_EAR_SHUTDOWN_BANK,           /* 0x10575458 */
    BR_EAR_SHUTDOWN_CHANNEL,        /* 0x10575470 */
    BR_EAR_SHUTDOWN_EAR,            /* 0x10575474 */
    BR_EAR_SHUTDOWN_ENVIRONMENT,    /* 0x105754A4 */
    BR_EAR_SHUTDOWN_EVENT,          /* 0x10575498 */
    BR_EAR_SHUTDOWN_MATRIX,         /* 0x1057545C */
    BR_EAR_SHUTDOWN_PRESET,         /* 0x10575490 */
    BR_EAR_START_EVENT,             /* 0x1057547C */
    BR_EAR_START_TIMER,             /* 0x1057549C */
    BR_EAR_SHUTDOWN_TIMER,          /* 0x10575468 */
    BR_EAR_UPDATE_EAR,              /* 0x105754BC -- doubles as the "loaded" flag */
    BR_EAR_PROC_COUNT
};

typedef struct BrEarState {
    void        *hModule;                  /* 0x10575464 */
    BrEarProc    apfn[BR_EAR_PROC_COUNT];
    unsigned int windowMessage;            /* 0x10575478 */
    int          usedLoadLibrary;          /* 0x105754D4 */
    int          fallbackTried;            /* 0x105754D8 */
    int          preferPds;                /* 0x105754DC */
} BrEarState;

extern const char *const g_apszBrEarProc[BR_EAR_PROC_COUNT];
extern const char *const g_pszBrEarDllPds;   /* "earpds.dll" */
extern const char *const g_pszBrEarDllIas;   /* "earias.dll" */
extern const char *const g_pszBrEarWndMsg;   /* "EAR Interactive Around-Sound" */

/* 0x1002A8A0  load the EAR DLL and bind its 31 exports. Returns 1 on success
 * (or if already bound), 0 on failure.
 *
 * usePds != 0 loads "earpds.dll" directly. usePds == 0 tries "earias.dll"
 * first and, only then, falls back to "earpds.dll". Each attempt is
 * GetModuleHandle first, LoadLibrary second.
 *
 * GOTCHA (original bug, preserved): the "already loaded" early-out at the top
 * tests hModule != NULL && apfn[BR_EAR_UPDATE_EAR] != NULL. If a previous
 * call resolved UpdateEar but some OTHER export came back NULL, that call
 * returned 0 -- yet the next call sees both flags set and returns 1 with a
 * half-bound table.
 *
 * GOTCHA (original bug, preserved): when usePds != 0 and BOTH GetModuleHandle
 * and LoadLibrary fail, the original does not return; it proceeds to resolve
 * all 31 exports against a NULL module handle. The result is still 0, just
 * after 31 pointless calls. */
int BrEarLoad(BrEarState *pState, const BrEarPlatform *pPlat, int usePds);

#endif /* SLICE1_04_H */
