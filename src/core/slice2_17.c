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

/* 0x1002A050 BrMat4LookAt and 0x1002A200 BrLightDirsAndAngles now live in
 * src/core/geometry/br_camview.c. */

/* BrMat4RotateAxis (0x1002A590) is in src/core/geometry/br_mat.c. */

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
