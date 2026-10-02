/* br_carcolour.c -- drawing: repainting a car.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_19.c, an address batch and not a module.  A car's
 * paint is a colour written into its own display list and read back out of
 * it again; 0x1002F340 is the sink the two ends share.
 *
 * slice2_19.c's preamble is carried over verbatim, the BrRgbSinkSet rename
 * included -- without it the header's cdecl prototype hides the thiscall
 * shape the bytes show.  An include set that looks redundant has already
 * been shown elsewhere in this module to move VC5's register allocation
 * (see br_rdpmode.c).
 */
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#include "slice1_05.h"   /* g_aBrEntRecs */
#include "slice3_41.h"   /* BrDriverCar, the canonical record */
#include "slice2_19.h"
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* 0x10AC0810 */
/* BrSub100088B0: prototype in br_funcs.h */
/* BrSegSetBases: prototype in br_funcs.h */
#undef BrRgbSinkSet

#include <string.h>

/* WHAT IT DOES: stores a colour as three separate red, green and blue
 * amounts, keeping only the bottom byte of each. */
/* @implements 0x10035CA0 d3d BrRgbSinkSet */
/* Second argument is a struct so it is not register-eligible: __fastcall
 * then puts `this` in ecx and the three ints on the stack, i.e. thiscall. */
typedef struct { int r, g, b; } BrRgbSinkSetArgs;
void BR_THISCALL1 BrRgbSinkSet(BrRgbSink *pSink, BrRgbSinkSetArgs a)
{
    (*(unsigned char *)&((BrDriverCar *)(pSink))->f29AC) = (unsigned char)a.r;
    (*(unsigned char *)&((BrDriverCar *)(pSink))->f29AD) = (unsigned char)a.g;
    (*(unsigned char *)&((BrDriverCar *)(pSink))->f29AE) = (unsigned char)a.b;
}

/* WHAT IT DOES: repaints a car by writing the chosen colour into the twelve
 * body panels of its model and re-submitting them for drawing, then fixes up
 * the last panel's drawing settings differently depending on which extra
 * pieces the car has -- shadows and reflections, judging by there being four
 * separate variants. The colour is written into two slots of each panel while
 * the see-through bit is taken from a third, which looks like an indexing
 * slip in the original and is preserved. */
/* @implements 0x100350EE d3d BrCarGfxSetColour */
void BrCarGfxSetColour(BrCarGfx *pCar, int r, int g, int b)
{
    /* FIVE locals, `sub esp,0x14`. The tail's word pointer is its OWN
     * variable in the original -- ebp-4, and the most-used slot in the
     * function -- not the loop's `pw` reused. Sharing one costs a slot and
     * shifts every displacement in the function. */
    uint16_t  *pwTail;
    int32_t   i;
    BrGfxSlot *pSlot;
    uint16_t  *pw;

    if (g_BrCarCount == 0)
        return;

    for (i = 0; i < 12; i++) {
        uint16_t v;

        pSlot = &BR_PTR32(BrGfxSlot *, pCar->aSlots)[pCar->aSlotIdx[i]];
        pw    = BR_PTR32(uint16_t *, pSlot->aWords);
        /* Nested, not two `continue`s: the original's tests are two near
         * `je`/`jne` straight to the loop increment (0x1002E7FB and
         * 0x1002E810), and the early-exit spelling emits a short branch over
         * a jump instead. */
        if (pw != NULL && ((pSlot->f20 >> 24) & 0xFu) == 1u) {
            /* GOTCHA: the alpha bit is taken from pw[i], the results land in
             * pw[0] and pw[1]. Faithful.
             *
             * The byte swap is INLINE here. BrSwapHalf is a real call at /Od
             * and the original has none; the `and 0xffff` before each shift
             * is the uint16_t read, and the `sar` is the promotion to int. */
            v = (uint16_t)((pw[i] & 1u)
                           | ((uint32_t)r << 11)
                           | ((uint32_t)g << 6)
                           | ((uint32_t)b << 1));
            pw[0] = (uint16_t)(((v << 8) & 0xFF00) | ((v >> 8) & 0xFF));

            v = (uint16_t)((pw[i] & 1u)
                           | (((uint32_t)r & 0x1Eu) << 10)
                           | (((uint32_t)g & 0x1Eu) << 5)
                           | ((uint32_t)b & 0x1Eu));
            pw[1] = (uint16_t)(((v << 8) & 0xFF00) | ((v >> 8) & 0xFF));
        }
    }

    for (i = 0; i < pCar->cDl; i++)
        g_BrGfxSubmit(pCar->aDl[i]);

    pwTail = BR_PTR32(uint16_t *, BR_PTR32(BrGfxSlot *, pCar->aSlots)[pCar->aSlotIdx[11]].aWords);
    /* Wrapped, not two early returns: the original's tests are near `je` and
     * `jne` straight to the function's own `mov esp,ebp` (0x1002EAFF), and
     * the return spelling emits a short branch over a jump instead. */
    if (pwTail != NULL && g_Br0AC300 == 0) {
        if (pCar->aDlExtra[0] != 0) {
            if (g_Br6C661C != 0 || g_Br6C6624 != 0) {
                pwTail[15] = 0x0070u;   /* +0x1E */
                pwTail[10] = 0x8290u;   /* +0x14 */
            } else {
                pwTail[15] = 0x0190u;
                pwTail[10] = 0x01A0u;
            }
            pwTail[14] = 0x0190u;       /* +0x1C */
            pwTail[13] = pwTail[15];        /* +0x1A <- +0x1E */
            pwTail[9]  = 0x01A0u;       /* +0x12 */
            pwTail[8]  = pwTail[10];        /* +0x10 <- +0x14 */
            pwTail[12] = 0x8179u;       /* +0x18 */
            pwTail[7]  = 0x4192u;       /* +0x0E */
            pwTail[11] = 0x6BADu;       /* +0x16 */
            pwTail[6]  = 0x31C6u;       /* +0x0C */
            g_BrGfxSubmit(pCar->aDlExtra[0]);
        }

        if (pCar->aDlExtra[1] != 0) {
            pwTail[14] = 0x00C0u;
            pwTail[13] = pwTail[14];        /* +0x1A <- +0x1C, unlike block 1 */
            pwTail[9]  = 0x04F9u;
            pwTail[8]  = pwTail[9];         /* +0x10 <- +0x12, unlike block 1 */
            pwTail[11] = 0x6BADu;
            pwTail[6]  = 0x31C6u;
            g_BrGfxSubmit(pCar->aDlExtra[1]);
        }

        if (pCar->aDlExtra[2] != 0) {
            pwTail[14] = 0x0190u;
            pwTail[13] = pwTail[15];        /* +0x1A <- +0x1E */
            pwTail[9]  = 0x01A0u;
            pwTail[8]  = pwTail[10];        /* +0x10 <- +0x14 */
            pwTail[11] = 0x38E7u;
            pwTail[6]  = 0xFEFFu;
            g_BrGfxSubmit(pCar->aDlExtra[2]);
        }

        if (pCar->aDlExtra[3] != 0) {
            pwTail[14] = 0x00C0u;
            pwTail[13] = pwTail[14];
            pwTail[9]  = 0x04F9u;
            pwTail[8]  = pwTail[9];
            pwTail[11] = 0x38E7u;
            pwTail[6]  = 0xFEFFu;
        g_BrGfxSubmit(pCar->aDlExtra[3]);
        }
    }
}

/* WHAT IT DOES: reads a car's current paint colour back out of its model and
 * expands it to full red, green and blue values, which is how the menus show
 * the player what colour the car is. The colour was stored more coarsely than
 * it was chosen, so what comes back is close to but not exactly what went
 * in. */
/* @implements 0x10035452 d3d BrCarGfxReadColour */
/* @n64 0x8021D2A0 located */
/* True __thiscall with THREE stack args and no edx setup. That IS reachable:
 * declare every stack argument as a ONE-MEMBER STRUCT, which is never
 * register-eligible, so ecx takes `this`, edx is left alone and no dummy has
 * to be materialised. (The `int unused_edx` spelling used here before cost an
 * `xor edx,edx` at the call and pushed the guard's `jne` from short to near.)
 * See docs/VC5-IDIOMS.md, "CALLING one is ALSO reachable".
 *
 * Everything else is /Od-literal: pw[0] is RE-READ for every term (no `c`
 * local), and the locals are declared in the original's home order
 * (pSlot, b, r, g, pw -> -4,-8,-0xc,-0x10,-0x14). */
typedef struct BrRgbArg { int v; } BrRgbArg;
/* BrRgbSinkSet3: prototype in br_funcs.h */

void BrCarGfxReadColour(BrRgbSink *pSink, const BrCarGfx *pCar)
{
    /* /Od homes locals by an internal NAME hash, not declaration order --
     * these single-letter names (a=slot, y=r, z=g, b=b, pw) are the set
     * that reproduces the original's frame layout
     * (slot=-4, b=-8, r=-0xc, g=-0x10, pw=-0x14); probed empirically. */
    const BrGfxSlot *a;
    BrRgbArg b, y, z;          /* the three struct args ARE the three locals */
    const uint16_t  *pw;

    a  = &BR_PTR32(BrGfxSlot *, pCar->aSlots)[pCar->aSlotIdx[2]];
    pw = BR_PTR32(uint16_t *, a->aWords);

    /* Nested ifs, no early returns: /Od emits ONE je-to-epilogue per
     * guard; `if (...) return;` costs a jne/jmp pair. */
    if (pw != NULL) {
        if (((a->f20 >> 24) & 0xFu) == 1u) {
            y.v = ((pw[0] >> 8) & 0xF8) | ((pw[0] >> 13) & 7);
            z.v = ((pw[0] >> 3) & 0xF8) | ((pw[0] >>  8) & 7);
            b.v = ((pw[0] << 2) & 0xF8) | ((pw[0] >>  3) & 7);
            {
                BrRgbSinkSetArgs rgb;
                rgb.r = y.v;
                rgb.g = z.v;
                rgb.b = b.v;
                BrRgbSinkSet(pSink, rgb);
            }
        }
    }
}
