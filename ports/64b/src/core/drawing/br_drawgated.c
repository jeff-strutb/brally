/* br_drawgated.c -- drawing: the gated scene draw.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * 0x10019840 draws the scene only when drawing is enabled, zeroing one
 * setting across the draw in one mode.
 *
 * Filed out of the address batch slice2_17.c.  That file's preamble is
 * carried verbatim (its #ifdef blocks, includes, .rdata constants,
 * cross-slice declarations and small helpers -- the helper set changes
 * /O2 register choice, so it is kept whole); its state block g_s17 is
 * declared in slice2_17.h and defined there.
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_17.h"

/* g_s17 is the port's gathering of scattered originals.  The matching build
 * reads the fields used here as the separate globals they are, by their
 * DAT_ names -- which the image gate resolves from the address they spell. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_PGFX g_BrGfxPtr
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_F6909B0 (*(int *)&g_brRaceHudA)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_F6C2CFC (*(int *)&g_brRaceFlyStep)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_F680944 DAT_105bcaec
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

/* ------------------------------------------------------------------ */
/* Small helpers. Loads and stores go through memcpy so that the byte
 * offsets recovered from the disassembly stay valid without relying on
 * pointer casts being aligned.                                         */

/* (port-only s17_ld32 removed) */


/* (port-only s17_st32 removed) */


/* (port-only s17_ldf removed) */


/* (port-only s17_stf removed) */


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
#define s17_ptrword(p_)   br_addr32((const void *)(p_))   /* a 32-bit display-list address */

/* 0x1002C2D0 */
/* WHAT IT DOES: draws the scene, but only if drawing is switched on at all.
 * In one particular mode it temporarily zeroes one setting across the draw and
 * puts it back afterwards, so that mode renders differently without the setting
 * being permanently changed. */
/* @implements 0x1002C2D0 d3d BrS17DrawGated */
void BrS17DrawGated(void)
{
    /* Orig `push ecx` slot: uninitialised, spilled across the call
     * (`mov [esp],eax` / `mov edx,[esp]`), shared ret via `je`. */
    volatile int saved;

    if (S17_F6909B0 != 0) {
        if (S17_F6909B0 == -1) {
            saved = S17_F6C2CFC;
            S17_F6C2CFC = 0;
        }
        BrAnimUpdate(S17_F680944);
        if (S17_F6909B0 == -1)
            S17_F6C2CFC = saved;
    }
}
