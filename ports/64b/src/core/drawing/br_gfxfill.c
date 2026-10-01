/* br_gfxfill.c -- drawing: RDP fill and texture commands.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Writes N64-style display-list commands through the g_s17.pGfx cursor:
 * select one texture out of a record table (0x1002AB32), clear the whole
 * screen to one colour (0x1002AB99), and fill a rectangle (0x1002AD39).
 * The two empty functions that sit between them in the original,
 * 0x1002AB8F and 0x1002AB94, are in br_gfxrect.c.
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
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_17.h"

/* g_s17 is the port's gathering of scattered originals.  The matching build
 * reads the fields used here as the separate globals they are, by their
 * DAT_ names -- which the image gate resolves from the address they spell. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_PGFX DAT_106e7710
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_SCREENW DAT_106e7714
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define S17_SCREENH DAT_106e9a2c
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
/* @implements 0x1002AB99 glide BrGfxClearScreen */
/* @implements 0x100314E8 d3d BrGfxClearScreen */
/* /Od TU: packing inlined, colour is a 16-bit slot, each emit has its own
 * cursor local and re-reads pGfx to bump. screenW sits at pGfx+4 in the
 * original (DAT_106e7714); g_s17.screenW is a later field. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrGfxClearScreen(int r, int g, int b)
{
    unsigned short c;
    unsigned int *p1, *p2, *p3, *p4, *p5, *p6, *p7;

    /* Keep r/g/b as unmodified params; temps live in eax/ecx/edx.
     * `| 1` is on the packed int so it is `or al,1` before the word store. */
    c = (unsigned short)(((r << 8) & 0xF800)
                       | ((g << 3) & 0x7C0)
                       | ((b >> 2) & 0x3E)
                       | 1);

    p1 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p1[0] = 0xE7000000u;
    p1[1] = 0;

    p2 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p2[0] = 0xB900031Du;
    p2[1] = 0x0F0A4000u;

    p3 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p3[0] = 0xBA001402u;
    p3[1] = 0x00300000u;

    p4 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p4[0] = 0xF7000000u;
    p4[1] = (unsigned)c | ((unsigned)c << 16);

    p5 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p5[0] = 0xE1000000u
          | ((((DAT_106e7714 << DAT_106ed674) - 1) & 0xFFF) << 12)
          | (((DAT_106e9a2c << DAT_106ed674) - 1) & 0xFFF);
    p5[1] = 0;

    p6 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p6[0] = 0xE7000000u;
    p6[1] = 0;

    p7 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p7[0] = 0xBA001402u;
    p7[1] = 0;
}

/* 0x10031688 */
/* WHAT IT DOES: fills a rectangle with one flat colour. In the double-size
 * display mode every coordinate is doubled first -- and then the bottom-right
 * corner is doubled a second time on its way into the command while the
 * top-left corner is not scaled at all, so in that mode the rectangle comes out
 * larger than asked for. That asymmetry is the original's. */
/* @implements 0x1002AD39 glide BrGfxFillRect */
/* @implements 0x10031688 d3d BrGfxFillRect */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrGfxFillRect(int ulx, int uly, int w, int h, int r, int g, int b)
{
    unsigned short c;
    unsigned int *p1, *p2, *p3, *p4, *p5, *p6, *p7;

    if (DAT_106ed674 != 0) {
        ulx = ulx << 1;
        uly = uly << 1;
        w = w << 1;
        h = h << 1;
    }

    c = (unsigned short)(((r << 8) & 0xF800)
                       | ((g << 3) & 0x7C0)
                       | ((b >> 2) & 0x3E)
                       | 1);

    p1 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p1[0] = 0xE7000000u;
    p1[1] = 0;

    p2 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p2[0] = 0xB900031Du;
    p2[1] = 0x0F0A4000u;

    p3 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p3[0] = 0xBA001402u;
    p3[1] = 0x00300000u;

    p4 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p4[0] = 0xF7000000u;
    p4[1] = (unsigned)c | ((unsigned)c << 16);

    p5 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p5[0] = 0xE1000000u
          | (((((ulx + w) << DAT_106ed674) - 1) & 0xFFF) << 12)
          | ((((uly + h) << DAT_106ed674) - 1) & 0xFFF);
    p5[1] = ((ulx & 0xFFF) << 12) | (uly & 0xFFF);

    p6 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p6[0] = 0xE7000000u;
    p6[1] = 0;

    p7 = (unsigned int *)S17_PGFX;
    S17_PGFX = S17_PGFX + 2;
    p7[0] = 0xBA001402u;
    p7[1] = 0;
}

/* 0x10031481 */
/* WHAT IT DOES: tells the hardware to use one particular texture out of a
 * table, unless that texture's record is flagged as one to skip, in which case
 * nothing is emitted and whatever texture was in use stays. */
/* @implements 0x1002AB32 glide BrGfxEmitTexCmd */
/* @implements 0x10031481 d3d BrGfxEmitTexCmd */
void BrGfxEmitTexCmd(int i, const void *pRecords)
{
    /* Orig /Od: two `imul i,0x24`, dword `[base+i*0x24+disp]`,
     * `if (((f20>>20)&1)==0) { p=pGfx; pGfx=pGfx+2; *p=w0; p[1]=1; }`.
     * `if (bit) return` becomes je+jmp; the do-while(0) emit macro
     * emits xor/test/jne under /Od; bumping from the local CSEs the
     * second pGfx load. */
    if (((*(unsigned int *)((char *)pRecords + i * BR_TEXREC_STRIDE + 0x20)
          >> 20) & 1) == 0) {
        unsigned int *p = (unsigned int *)S17_PGFX;
        S17_PGFX = S17_PGFX + 2;
        p[0] = (*(unsigned int *)((char *)pRecords + i * BR_TEXREC_STRIDE)
                & 0x00FFFFFFu) | 0xDC000000u;
        p[1] = 1;
    }
}
