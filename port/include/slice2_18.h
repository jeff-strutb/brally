/* slice2_18.h -- N64-GBI frame/fog/viewport layer, decompiled from BRD3D.dll.
 *
 * Another module's packet: 0x10031866-0x10033838, sixteen functions. Fourteen are
 * ported here; two (0x100331FF, 0x100334D7) are skipped -- see slice2_18.c for
 * the reason, which is that the packet's disassembly for both begins part-way
 * into the function body.
 *
 * WHAT THIS MODULE IS
 * -------------------
 * BRD3D.dll is a PC re-implementation of the N64 title's graphics layer, and
 * this cluster still speaks the N64 GBI. That was established from the words
 * these functions write, not assumed:
 *
 *   0xF8000000  G_SETFOGCOLOR   -- data is (r<<24)|(g<<16)|(b<<8)|0xFF
 *   0xBC000008  G_MOVEWD, offset 8 = G_MW_FOG, data (fm<<16)|fo
 *   0xBC000006  G_MOVEWD, offset 6 = G_MW_SEGMENT
 *   0xE7000000  G_RDPPIPESYNC
 *   0xE9000000  G_RDPFULLSYNC
 *   0xB8000000  G_ENDDL
 *   0x06000000  G_DL
 *   0x03800010  G_MOVEMEM, viewport -- data is a 16-byte Vp_t
 *   0x01020040  G_MTX (the matrix at 0x100AA730 is a float identity)
 *
 * The fog multiplier/offset pair computed at the tail of BrFogUpdate is
 * exactly libultra's guFog:  fm = 500*256/(max-min), fo = (500-min)*256/
 * (max-min).  0x1F400 in the original is 500*256.  That, plus the Vp_t
 * layout below, is what pins the identification.
 *
 * The viewport records are N64 Vp_t: four s16 of vscale then four s16 of
 * vtrans, each in the RDP's 2.2 fixed point, hence the *4 (written as two
 * doublings) that the code applies to half-width and to centre.  vscale.z and
 * vtrans.z are the usual 0x1FF.
 *
 * GLOBALS
 * -------
 * The original keeps all of this in file-scope globals.  They are reproduced
 * here as real globals named after their addresses (the BrG_ADDR convention
 * already used by slice1_07.h/slice1_08.h) rather than folded into a context
 * struct, so that the ported functions keep the original signatures.  Several
 * of these addresses have 10-35 users across the DLL, so other slices will
 * name the same objects; integration should expect to merge, not to find
 * them disjoint.
 *
 * ARGUMENT ORDER
 * --------------
 * BrScissorSet/BrViewportSet/BrViewportSetFull all take (x, y, w, h) -- note
 * that the last two are an EXTENT, not a second corner; the code adds them to
 * the first pair.  BrViewportSet accepts a NEGATIVE w (mirrored viewport) and
 * only takes |w| for the scissor call.
 */
#ifndef SLICE2_18_H
#define SLICE2_18_H

#include <stdint.h>
#include <stddef.h>

#include "br_vec.h"

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

/* 0x106C1588 -- N64 OSTask_t, 0x40 bytes, indexed by BrG_6C65EC.
 *
 * The field names come from the standard libultra layout, which the write
 * offsets match exactly (0x14 = 0x1000 ucode_size, 0x1C = 0x800
 * ucode_data_size, 0x24 = 0x400 dram_stack_size are all the stock values).
 *
 * DEVIATION: the five fields that hold addresses are uintptr_t rather than
 * the original's u32, so that a host pointer survives being stored here.
 * Nothing reads them back as 32-bit. */
typedef struct BrOsTask {
    uint32_t  type;             /* +0x00 */
    uint32_t  flags;            /* +0x04 */
    uintptr_t ucode_boot;       /* +0x08  not written by this module */
    uint32_t  ucode_boot_size;  /* +0x0C  not written by this module */
    uintptr_t ucode;            /* +0x10 */
    uint32_t  ucode_size;       /* +0x14 */
    uintptr_t ucode_data;       /* +0x18 */
    uint32_t  ucode_data_size;  /* +0x1C */
    uintptr_t dram_stack;       /* +0x20 */
    uint32_t  dram_stack_size;  /* +0x24 */
    uintptr_t output_buff;      /* +0x28 */
    uint32_t  output_buff_size; /* +0x2C */
    uintptr_t data_ptr;         /* +0x30 */
    uint32_t  data_size;        /* +0x34 */
    uintptr_t yield_data_ptr;   /* +0x38  not written by this module */
    uint32_t  yield_data_size;  /* +0x3C  not written by this module */
} BrOsTask;

/* Viewport ring at 0x106C1788: 32 records of 16 bytes, indexed by
 * BrG_6C6654 & 0x1F.  Laid out as N64 Vp_t. */
#define BR_S18_VP_SLOTS 32
typedef struct BrVpRec {
    int16_t vscale[4];   /* +0x00 */
    int16_t vtrans[4];   /* +0x08 */
} BrVpRec;

/* The record BrFrameBegin fills in.  Its full shape is not established: the
 * function writes int32s at +0x00,+0x04,+0x08,+0x0C and, when
 * BrG_0AA8B4 == 2, also at +0x58,+0x5C,+0x60,+0x64.  Indexed positionally. */
#define BR_S18_FRAMEREC_DWORDS 0x1A   /* 0x68 bytes */

/* ------------------------------------------------------------------ */
/* Cross-slice callees                                                */
/* ------------------------------------------------------------------ */

/* XSLICE 0x1003B0A0 */
/* sqrt(dx*dx + dy*dy) of the X and Y components only -- a 2D distance
 * between two BrVec3.  Z is never loaded. Order is (a, b). */
extern float BrVec3DistXY(const BrVec3 *pA, const BrVec3 *pB);

/* XSLICE 0x1003B2A0 */
/* Full 4-component transform, row-vector convention (same as br_mat.h):
 *   out[i] = v.x*M[0][i] + v.y*M[1][i] + v.z*M[2][i] + M[3][i]
 * Destination first, then the vector, then the matrix. */
extern void BrMat4TransformPoint4(float pOut[4], const BrVec3 *pV,
                                  const float *pM);

/* XSLICE 0x1003AE50 */
extern void BrVec3Normalise(BrVec3 *pV);

/* XSLICE 0x10008B80 -- a bare `ret` in this build (see CONTRACT).  It is
 * called cdecl with 0, 1 or 5 arguments, which C99 cannot express in one
 * prototype, so the port declares one name per observed arity.  All of them
 * are the same original function. */
extern void BrStub8B80_0(void);
extern void BrStub8B80_1i(int32_t a0);
extern void BrStub8B80_1p(const void *p0);
extern void BrStub8B80_5i(int32_t a0, int32_t a1, int32_t a2,
                          int32_t a3, int32_t a4);

/* XSLICE 0x10042AF0 -- called with 1 and with 3 arguments; split as above. */
extern void BrGfx42AF0_1(void *p0);
extern void BrGfx42AF0_3(void *p0, int32_t a1, int32_t a2);

/* XSLICE 0x1002F900 -- seventeen cdecl arguments; the first is the reserved
 * 8-byte display-list slot, the remaining sixteen are four groups of four. */
extern void BrGfx2F900(uint32_t *pCmd,
                       int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                       int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                       int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                       int32_t a13, int32_t a14, int32_t a15, int32_t a16);

/* XSLICE 0x10031227 */ extern void BrGfx31227(void);
/* XSLICE 0x10069580 */ extern void BrGfx69580(void);
/* XSLICE 0x1002C210 */ extern void BrGfx2C210(void);
/* XSLICE 0x10060E90 */ extern int32_t BrTimeNow(void);
/* XSLICE 0x10060E00 */ extern void BrGfx60E00(void *p0);
/* XSLICE 0x10035BBA */ extern void BrFatal(const char *pszMsg);
/* XSLICE 0x10035CE0 -- __thiscall in the original (this in ecx). */
extern void BrEnt35CE0(void *pThis);
/* XSLICE 0x10035FC0 -- __thiscall in the original (this in ecx). */
extern void BrEnt35FC0(void *pThis);

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

/* Display-list write cursor.  0x106C0680 is a byte pointer in the original
 * and every producer bumps it by 8; modelled as a u32 cursor bumped by 2. */
extern uint32_t *BrG_6C0680;    /* 0x106C0680 */
extern uint8_t  *BrG_6C0944;    /* 0x106C0944  DL arena base */
extern int32_t   BrG_6C65EC;    /* 0x106C65EC  double-buffer index, 0 or 1 */

/* Mode selectors.  Exactly one of these four is expected to be non-zero;
 * BrFogUpdate and BrHudColorsUpdate test them in this priority order. */
extern int32_t BrG_6C6614;      /* 0x106C6614  forces player-car translucent */
extern int32_t BrG_6C661C;      /* 0x106C661C */
extern int32_t BrG_6C6620;      /* 0x106C6620 */
extern int32_t BrG_6C6624;      /* 0x106C6624 */
extern int32_t BrG_6C6618;      /* 0x106C6618 */

/* Fog colour, one byte per channel; packed R<<24|G<<16|B<<8|0xFF. */
extern uint8_t BrG_6C0260;      /* 0x106C0260  R */
extern uint8_t BrG_6C1614;      /* 0x106C1614  G */
extern uint8_t BrG_6C0200;      /* 0x106C0200  B */
extern uint8_t BrG_690BE8;      /* 0x10690BE8  blend weight, 0..255 */

extern int32_t BrG_0B4050;      /* 0x100B4050 */
extern int32_t BrG_6C3398;      /* 0x106C3398  fog near, world units */
extern int32_t BrG_6C64D8;      /* 0x106C64D8  fog far,  world units */
extern int32_t BrG_6C2CF4;      /* 0x106C2CF4  fog multiplier (guFog fm) */
extern int32_t BrG_6C1618;      /* 0x106C1618  fog offset     (guFog fo) */

extern int32_t BrG_0A79CC;      /* 0x100A79CC */
extern int32_t BrG_0B380C;      /* 0x100B380C */
extern void   *BrG_6C6490;      /* 0x106C6490  +0x30 is a BrVec3, +0x34 a float */
extern BrVec3  BrG_4B0378;      /* 0x104B0378 */
extern void   *BrG_6C2CF8;      /* 0x106C2CF8  +0x38 is a float */
extern float   BrG_6C7C80;      /* 0x106C7C80 */
extern float   BrG_6C7C84;      /* 0x106C7C84 */
extern uint8_t BrG_6C7CC8[3];   /* 0x106C7CC8..CA  base fog colour, R,G,B */
extern float   BrG_6C29A8[16];  /* 0x106C29A8  view-projection matrix */

/* Derived HUD/overlay colours. */
extern BrVec3  BrG_6C0670;      /* 0x106C0670..78 */
extern uint8_t BrG_6C1580;      /* 0x106C1580  R */
extern uint8_t BrG_6C335C;      /* 0x106C335C  G */
extern uint8_t BrG_6C0968;      /* 0x106C0968  B */
extern uint8_t BrG_690BF0;      /* 0x10690BF0  R (dim variant) */
extern uint8_t BrG_6C0960;      /* 0x106C0960  G (dim variant) */
extern uint8_t BrG_6C65BC;      /* 0x106C65BC  B (dim variant) */
extern uint8_t BrG_690FF8[4];   /* 0x10690FF8..FB  4-step R ramp */
extern uint8_t BrG_6C6494[4];   /* 0x106C6494..97  4-step G ramp */
extern uint8_t BrG_6C3358[4];   /* 0x106C3358..5B  4-step B ramp */
extern uint32_t BrG_6C29E8;     /* 0x106C29E8 */
extern uint32_t BrG_6C5AB0;     /* 0x106C5AB0 */
extern uint32_t BrG_6C0950[4];  /* 0x106C0950  the ramp as packed RGBA */

/* Frame setup. */
extern int32_t  BrG_6C65E0;     /* 0x106C65E0  countdown, reloaded on change */
extern int32_t  BrG_6C65E4;     /* 0x106C65E4  hi-res flag: doubles all rects */
extern int32_t  BrG_6C65E8;     /* 0x106C65E8 */
extern int32_t  BrG_0AA8B4;     /* 0x100AA8B4  1 or 2 select the record layout */
extern int32_t  BrG_0A81C0;     /* 0x100A81C0  screen width  */
extern int32_t  BrG_0A81C4;     /* 0x100A81C4  screen height */
extern int32_t  BrG_6C299C;     /* 0x106C299C */
extern int32_t  BrG_6C0684;     /* 0x106C0684 */
extern int32_t  BrG_0AA890;     /* 0x100AA890 */
extern uint32_t BrG_6C0258;     /* 0x106C0258 */
extern uint32_t BrG_6C0688;     /* 0x106C0688 */
extern uint32_t BrG_6C0920;     /* 0x106C0920 */
extern void    *BrG_0AA730;     /* 0x100AA730 (glide 0x100A9EC0) float identity
                                 * matrix; G_MTX 0x01060040 payload -- the base
                                 * modelview several render fns load (e.g. the
                                 * glide frontier 0x10011D20). */
extern int32_t  BrG_0AA884;     /* 0x100AA884 */
extern uint8_t *BrG_0AA770;     /* 0x100AA770  array of 0x28-byte sub-lists */
extern int32_t  BrG_6C65FC;     /* 0x106C65FC  index into the above */
extern int32_t  BrG_6C6604;     /* 0x106C6604 */
extern int32_t  BrG_6C1628[BR_S18_FRAMEREC_DWORDS];  /* 0x106C1628 */

/* Screen clip bounds used by BrScissorSet.  Note the pairing: 0x10575508 /
 * 0x10575500 bound X, and 0x1057550C / 0x105754FC bound Y -- the four
 * addresses are NOT in min/max order. */
extern int32_t BrG_575508;      /* 0x10575508  X min */
extern int32_t BrG_575500;      /* 0x10575500  X max */
extern int32_t BrG_57550C;      /* 0x1057550C  Y min */
extern int32_t BrG_5754FC;      /* 0x105754FC  Y max */

/* Viewport ring. */
extern int32_t BrG_6C6654;      /* 0x106C6654  ring index, kept & 0x1F */
extern int32_t BrG_6C3364;      /* 0x106C3364  horizontal-flip flag */
extern int32_t BrG_6C1174;      /* 0x106C1174  set when w was negative */
extern BrVpRec BrG_6C1788[BR_S18_VP_SLOTS];  /* 0x106C1788 */
extern int32_t BrG_6C62D8;      /* 0x106C62D8  copy of vscale.z */
extern int32_t BrG_6C65B8;      /* 0x106C65B8  copy of vtrans.z */

/* HUD text. */
extern int32_t  BrG_6C56E8;     /* 0x106C56E8  re-entry guard */
extern uint16_t BrG_0B5D90;     /* 0x100B5D90 */
extern void    *BrG_691000;     /* 0x10691000 */
extern void    *BrG_6C65A0;     /* 0x106C65A0 */
extern uint8_t *BrG_6C6678;     /* 0x106C6678  records of stride 0x15C */

/* Frame end. */
extern BrOsTask BrG_6C1588[2];  /* 0x106C1588 */
extern uintptr_t BrG_0AA728;    /* 0x100AA728 */
extern uint32_t  BrG_0AA72C;    /* 0x100AA72C */
extern int32_t   BrG_6C6668;    /* 0x106C6668 */
extern int32_t   BrG_6C6660;    /* 0x106C6660  high-water: DL commands */
extern int32_t   BrG_6C6658;    /* 0x106C6658  high-water */
extern int32_t   BrG_6C665C;    /* 0x106C665C  high-water */
extern uint8_t  *BrG_363FF0;    /* 0x10363FF0 */
extern uint8_t  *BrG_2E5EC8;    /* 0x102E5EC8 */
extern uint8_t  *BrG_364304;    /* 0x10364304 */
extern uint8_t  *BrG_3643BC;    /* 0x103643BC */
extern int32_t   BrG_6C1170;    /* 0x106C1170  DL command count this frame */
extern int32_t   BrG_6C6664;    /* 0x106C6664  frames-skipped counter */
extern void     *BrG_6C33A0;    /* 0x106C33A0 */
extern void     *BrG_6C3380;    /* 0x106C3380 */
extern void    (*BrG_6C198C)(void);   /* 0x106C198C  one-shot callback */
extern int32_t   BrG_6C1608;    /* 0x106C1608 */
extern int32_t   BrG_6C020C;    /* 0x106C020C */
extern int32_t   BrG_6C0208;    /* 0x106C0208 */
extern int32_t   BrG_6C1620;    /* 0x106C1620 */
extern int32_t   BrG_0ADFC0;    /* 0x100ADFC0 */
extern void     *BrG_AA4020;    /* 0x10AA4020 */
extern void     *BrG_AA3760;    /* 0x10AA3760 */
extern void     *BrG_AA3D50;    /* 0x10AA3D50 */
extern void     *BrG_AA3490;    /* 0x10AA3490 */
extern int32_t   BrG_6C65F4;    /* 0x106C65F4 */
extern int32_t   BrG_6C65F8;    /* 0x106C65F8 */
extern int32_t   BrG_6C6598;    /* 0x106C6598 */
extern int32_t   BrG_6C56E4;    /* 0x106C56E4 */
extern void    (*BrG_B501D0)(uintptr_t dataPtr);  /* 0x10B501D0  task submit */

/* Number of display-list commands past which BrFrameEnd calls BrFatal. */
#define BR_S18_GLIST_LIMIT 0x2EE0

/* ------------------------------------------------------------------ */
/* Functions                                                          */
/* ------------------------------------------------------------------ */

/* 0x10031866 -- pick the fog colour and near/far for the current mode, then
 * emit G_MOVEWD(G_MW_FOG) and G_SETFOGCOLOR.  Also leaves the guFog
 * multiplier/offset in BrG_6C2CF4 / BrG_6C1618 for BrFogFactorAtPoint. */
void BrFogUpdate(void);

/* 0x10031D3F -- fog factor in [0,1] for a world-space point.
 *
 * Transforms the point by BrG_6C29A8, forms z/w, applies the SAME fm/fo pair
 * the RDP is given, divides by 255 and clamps.  Returns 0.0f outright when
 * BrG_6C6618 is zero (fog off).
 *
 * The clamp is written as two separate compares in the original and its
 * NaN behaviour is asymmetric: a NaN survives the low compare (not less
 * than 0) and then takes the "<= 1" branch, so a NaN is RETURNED, not
 * clamped.  Reproduced. */
float BrFogFactorAtPoint(const BrVec3 *pPoint);

/* 0x10031DCF -- derive the HUD/overlay colour set from the fog colour.
 * In the BrG_6C6618 branch every channel is an integer lerp between the fog
 * colour and a fixed colour, weighted by BrG_690BE8/255. */
void BrHudColorsUpdate(void);

/* 0x100322E6 -- begin a frame: reset the display-list cursor to the arena for
 * BrG_6C65EC, fill in the caller's record, and emit the fixed opening block
 * of GBI commands.
 *
 * `fHiRes` is XORed against BrG_6C65E4; any change reloads BrG_6C65E0 and
 * latches the new value.  Everything downstream that halves or doubles a
 * rectangle keys off BrG_6C65E4, not off this argument. */
void BrFrameBegin(int32_t *pRec, int32_t fHiRes);

/* 0x10032873 -- BrFrameBegin(pRec, 0). */
void BrFrameBeginRec(int32_t *pRec);
/* 0x10032886 -- BrFrameBegin(BrG_6C1628, 1). */
void BrFrameBeginHiRes(void);
/* 0x1003289A -- empty in this build. */
void BrFrameNop(void);

/* 0x1003289F -- clamp (x, y, w, h) to the screen bounds, double it when
 * BrG_6C65E4 is set, then emit G_RDPPIPESYNC and one 0xE2 command holding
 * (x,y) and (x+w, y+h) in 12-bit fields.
 *
 * The clamp order matters and is asymmetric: the low edge is corrected by
 * SHRINKING the extent and moving the origin, the high edge only by shrinking
 * the extent, and only then is a negative extent floored at 0.  Two
 * consequences, both verified against the original and both easy to get
 * wrong:
 *   - a rectangle entirely off the LOW edge collapses to zero size AT the low
 *     edge, not at its own position;
 *   - a rectangle entirely off the HIGH edge collapses to zero size AT ITS
 *     OWN position, which is still outside the bounds.  The emitted origin
 *     can therefore exceed the high bound.  The origin is never pulled back
 *     from the high side. */
void BrScissorSet(int32_t x, int32_t y, int32_t w, int32_t h);

/* 0x10032A42 -- write viewport slot (BrG_6C6654+1)&0x1F from (x,y,w,h) and
 * emit G_MOVEMEM for it.  When fScissor is non-zero it first calls
 * BrScissorSet with |w| (note: with the ORIGINAL h, not |h|).
 *
 * A negative w means a mirrored viewport: BrG_6C1174 is set, w is negated for
 * the centre computation, and vscale.x keeps the negative sign.  BrG_6C3364
 * additionally negates vscale.x in BOTH the negative- and positive-w paths,
 * so setting it flips an already-mirrored viewport back. */
void BrViewportSet(int32_t x, int32_t y, int32_t w, int32_t h,
                   int32_t fScissor);

/* 0x10032C38 -- same, but the viewport always covers the whole screen
 * (BrG_0A81C0 x BrG_0A81C4) regardless of the arguments.
 *
 * The x/y/w/h arguments are used ONLY for the optional scissor; the doubling
 * applied to them afterwards is dead code in the original and is preserved
 * only as a comment here. */
void BrViewportSetFull(int32_t x, int32_t y, int32_t w, int32_t h,
                       int32_t fScissor);

/* 0x10032DF2 -- re-emit G_MOVEMEM for the CURRENT slot without touching it
 * (no ring advance).  Used to restore the viewport after something else
 * clobbered it. */
void BrViewportReEmit(void);

/* 0x1003348E, 0x10033493 -- empty in this build. */
void BrGfxNopA(void);
void BrGfxNopB(void);

/* 0x10033780 -- guarded HUD-text open; idempotent while already open. */
void BrHudTextBegin(void);
/* 0x100337AE -- close what BrHudTextBegin opened. */
void BrHudTextEnd(void);
/* 0x100337E9 -- BrHudTextEnd, then run both per-record passes.  The loop
 * bound is a hardcoded 1 in this build, so it visits record 0 only; the
 * stride 0x15C is the parallel-array stride noted in the contract. */
void BrHudDrawAll(void);

/* 0x10033838 -- close the display list, fill in the OSTask, record the
 * high-water marks, and hand the list to BrG_B501D0; then flip BrG_6C65EC.
 *
 * When BrG_6C6664 is zero the whole submission path is skipped and the
 * counter is merely incremented -- i.e. the FIRST call after the counter is
 * cleared drops its frame. */
void BrFrameEnd(void);

#endif /* SLICE2_18_H */
