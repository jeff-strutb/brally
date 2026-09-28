/* br_renderreset.c -- startup: reset renderer tallies and screen size.
 *
 * RESPONSIBILITY: startup/ -- bring the game up and set its defaults.
 *
 * Zeroes the renderer's per-frame counters (0x1002A8D7) and puts the
 * working screen size back to the build defaults (0x1002A93C).  Their
 * neighbour BrScreenSizeInit (0x1002A932) is in startup/br_stubs.c.
 *
 * Filed out of the address batch slice2_17.c.  That file's preamble is
 * carried verbatim (its #ifdef blocks, includes, .rdata constants,
 * cross-slice declarations and small helpers -- the helper set changes
 * /O2 register choice, so it is kept whole); its state block g_s17 is
 * declared in slice2_17.h and defined there.
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

/* g_s17 is the port's gathering of scattered originals.  The matching build
 * reads the fields used here as the separate globals they are, by their
 * DAT_ names -- which the image gate resolves from the address they spell. */
#ifdef BR_MATCHING_BUILD
extern uint32_t *DAT_106e7710;
#define S17_PGFX DAT_106e7710
extern int DAT_106e7714;
#define S17_SCREENW DAT_106e7714
extern int DAT_106e9a2c;
#define S17_SCREENH DAT_106e9a2c
extern int DAT_100a7514;
#define S17_DEFAULTW DAT_100a7514
extern int DAT_100a7518;
#define S17_DEFAULTH DAT_100a7518
extern int DAT_106ea35c;
#define S17_F6C32CC DAT_106ea35c
extern int DAT_106ec76c;
#define S17_F6C56DC DAT_106ec76c
extern int DAT_106e8208;
#define S17_F6C1178 DAT_106e8208
extern int DAT_106e86ac;
#define S17_F6C161C DAT_106e86ac
extern int DAT_106e86a0;
#define S17_F6C1610 DAT_106e86a0
extern int DAT_106ea448;
#define S17_F6C33B8 DAT_106ea448
extern int DAT_106e7734;
#define S17_F6C06A4 DAT_106e7734
extern int DAT_106e772c;
#define S17_F6C069C DAT_106e772c
#else
#define S17_PGFX g_s17.pGfx
#define S17_SCREENW g_s17.screenW
#define S17_SCREENH g_s17.screenH
#define S17_DEFAULTW g_s17.defaultW
#define S17_DEFAULTH g_s17.defaultH
#define S17_F6C32CC g_s17.f6C32CC
#define S17_F6C56DC g_s17.f6C56DC
#define S17_F6C1178 g_s17.f6C1178
#define S17_F6C161C g_s17.f6C161C
#define S17_F6C1610 g_s17.f6C1610
#define S17_F6C33B8 g_s17.f6C33B8
#define S17_F6C06A4 g_s17.f6C06A4
#define S17_F6C069C g_s17.f6C069C
#endif
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

/* g_s17 is declared in slice2_17.h and defined in slice2_17.c. */

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
        uint32_t *p_ = S17_PGFX;                                      \
                                                                        \
        S17_PGFX = p_ + 2;                                            \
        p_[0] = (w0_);                                                  \
        p_[1] = (w1_);                                                  \
    } while (0)

/* DEVIATION: the original stores raw 32-bit pointers into the display list
 * (`mov [eax+4], esi`). On a 64-bit host that cannot round-trip, so the low
 * 32 bits are stored, exactly as the original would have. Consumers of the
 * stream in this port must not dereference these words. */
#define s17_ptrword(p_)   ((uint32_t)(uintptr_t)(const void *)(p_))

/* 0x10031227 */
/* WHAT IT DOES: zero the renderer's per-frame tallies -- eight counters in
 * three groups, cleared as chains (a=0; b=a; c=b) exactly as the original
 * does. Called at the top of a frame so each frame's totals start clean. */
/* @implements 0x10031227 d3d BrRenderCountersReset */
void BrRenderCountersReset(void)
{
    S17_F6C32CC = 0;
    S17_F6C56DC = S17_F6C32CC;
    S17_F6C1178 = S17_F6C56DC;

    S17_F6C161C = 0;
    S17_F6C1610 = S17_F6C161C;

    S17_F6C33B8 = 0;
    S17_F6C06A4 = S17_F6C33B8;
    S17_F6C069C = S17_F6C06A4;
}

/* 0x1003128C */
/* WHAT IT DOES: sets the game's working screen size back to the build's
 * defaults -- 640 by 480 here. */
/* @implements 0x1003128C d3d BrScreenSizeApply */
void BrScreenSizeApply(void)
{
    S17_SCREENW = S17_DEFAULTW;     /* 0x100A81C0 = 640 in this build */
    S17_SCREENH = S17_DEFAULTH;     /* 0x100A81C4 = 480               */
}
