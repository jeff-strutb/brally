/* br_bankflip.c -- drawing: the double-buffered bank flip.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * 0x100192D0 switches to the other of two buffer banks and clears it.
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

/* 0x1002C210 */
/* WHAT IT DOES: switch to the other of two buffer banks and wipe it ready for
 * use -- double buffering. It resets three channels to a default state, flips
 * the bank index, timestamps the new bank, then clears its buffer in 0x800-byte
 * steps. */
/* @implements 0x1002C210 d3d BrS17BankFlip */
#ifdef BR_MATCHING_BUILD
/* Literal layout: the header triple and the byte banks are FIXED arrays at
 * 0x105B9700 / 0x105B972C, not pointers loaded from the struct; the clear
 * loop stores an immediate 0 every 0x800 bytes, counting down. */
extern int DAT_105b9718;
extern int DAT_105b9720;
extern int DAT_105b9724;
extern unsigned int DAT_105b9700[];
extern unsigned char DAT_105b972c[];
int FUN_10008d60();
int FUN_10059f00();
void BrS17BankFlip(void)
{
    int i;
    int bank;
    unsigned int *hdr;
    unsigned char *buf;

    for (i = 0; i < 3; ++i)
        FUN_10008d60(i, 0xFF, 0, 0xFF, 0x7F);

    DAT_105b9718 ^= 1;
    DAT_105b9720 = FUN_10059f00();
    DAT_105b9724 = 0;

    bank = DAT_105b9718;
    buf = DAT_105b972c + bank * 3 * 0x800;
    hdr = DAT_105b9700 + bank * 3;

    memset(hdr, 0, 0xC);

    for (i = 3; i != 0; i--) {
        *(unsigned int *)buf = 0;
        buf += 0x800;
    }
}
#else
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
#endif
