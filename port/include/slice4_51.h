/* slice4_51.h -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * This packet is a "close the link" packet: every entry point here is one an
 * already-ported module calls by name.  The names and signatures below are
 * therefore fixed by the callers, not chosen here.
 *
 *   0x1003D0B0  BrSub1003D0B0       (slice2_13, slice2_25)
 *   0x1003E070  BrFn1003E070        (slice2_23)
 *   0x10021560  BrGbiCall10021560   (slice2_16)
 *
 * ---------------------------------------------------------------------
 * PACKET / NAME MISMATCH -- read this before trusting work/slice4/agent51.asm
 * ---------------------------------------------------------------------
 * The listing handed to this pass is mis-paired with the `WANTED AS` names:
 * the address column of work/undefined_resolved.txt is shifted by one row
 * against the name column inside each alphabetically-sorted name family.
 * Concretely:
 *
 *   wanted BrSub1003D0B0    listing given: sub_1003CE80   (a DIFFERENT
 *                           function, already named BrSub1003CE80 elsewhere)
 *   wanted BrOptFn10056FF0  listing given: sub_100558A0   (already named
 *                           BrOptFn100558A0 in slice2_25.h)
 *
 * The bodies used here were re-read from asm/10030000.asm at the address the
 * NAME refers to, which is the only pairing consistent with how the callers
 * use these symbols (see the note on BrSub1003D0B0 below).
 *
 * NOT in this file, and why:
 *   0x1007C8A0  BrFtolTrunc     -- already implemented, in port/src/br_crt.c.
 *                                  Duplicating it is forbidden by the contract.
 *   0x10056FF0  BrOptFn10056FF0 -- skipped, see the tail of slice4_51.c.
 */
#ifndef SLICE4_51_H
#define SLICE4_51_H

#include <stddef.h>
#include <stdint.h>

#include "slice1_03.h"   /* BrClipVert -- the 0x28-byte clip node */

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * 1. 0x1003D0B0 -- IDirectPlay4::GetSessionDesc into a Global* buffer
 * ===================================================================== */

struct BrDPlay4Obj;

/* 0x1003D0B0
 *
 *   hr = pObj->vtbl[0x58](pObj, NULL, &cb);        // GetSessionDesc, sizing
 *   if (hr != DPERR_BUFFERTOOSMALL) return hr;     // NOTE: even S_OK bails
 *   pv = GlobalLock(GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, cb));
 *   if (!pv) return E_OUTOFMEMORY;
 *   hr = pObj->vtbl[0x58](pObj, pv, &cb);
 *   if (hr >= 0) { *ppvOut = pv; pv = NULL; }
 *   if (pv) { GlobalUnlock(GlobalHandle(pv)); GlobalFree(GlobalHandle(pv)); }
 *   return hr;
 *
 * The interface is IDirectPlay4A: byte offset 0x58 is GetSessionDesc, which
 * agrees with the offsets slice2_13.h already pinned down (0x08 Release,
 * 0x10 Close, 0x24 DestroyPlayer, 0x64 Receive).
 *
 * GOTCHA: the sizing call's result is compared for EQUALITY with
 * DPERR_BUFFERTOOSMALL, so a driver that answered S_OK for a zero-size
 * request would make this return S_OK having written nothing to *ppvOut.
 *
 * GOTCHA: *ppvOut is left untouched on every failure path -- the caller in
 * slice2_13 relies on that and initialises its own local to NULL.
 *
 * GOTCHA: the size out-parameter is the CALLER'S first argument slot, reused
 * as scratch.  That is invisible from C but it means the original clobbers
 * `pObj` on the stack; nothing reads it again, so nothing here depends on it.
 *
 * SIGNATURE CONFLICT (reported, not resolved here): slice2_13.c declares
 *   int32_t BrSub1003D0B0(BrDPlay4Obj *pObj, void **ppvOut);
 * and slice2_25.h declares
 *   void    BrSub1003D0B0(BrDPlay *pDPlay, BrDPSessionDesc **ppDesc);
 * The int32_t form is the one implemented: the original really does return
 * the HRESULT in eax, and slice2_13's caller tests it (`< 0`).  slice2_25
 * merely ignores the result, which is source-compatible at the call site but
 * NOT at the declaration, so slice4_51.h and slice2_25.h must not be
 * included into the same translation unit until integration picks one. */
int32_t BrSub1003D0B0(struct BrDPlay4Obj *pObj, void **ppvOut);

/* =====================================================================
 * 2. 0x1003E070 -- two-call thunk
 * ===================================================================== */

/* 0x1003E070 is ten bytes: `call 0x1005FF60` then `jmp 0x1005FFF0`, i.e. a
 * tail call.  Both targets are already ported, in slice3_39.c, and both take
 * no arguments and return none, so the tail call carries nothing across. */
void BrFn1003E070(void);

/* =====================================================================
 * 3. 0x10021560 -- the tile-rectangle sink
 * ===================================================================== */

/* The stack object 0x10021560 builds four of.  0x1001D420 is handed the base
 * of one of these and immediately does `+ 0x40` to reach the clip node, so
 * only the BrClipVert at +0x40 carries anything; the 0x40 bytes in front of
 * it are neither read nor written by either function.  The original's stack
 * stride is 0x6C. */
typedef struct BrGbiRectVert {
    uint8_t    abUnused[0x40];   /* +0x00..+0x3F -- never touched */
    BrClipVert node;             /* +0x40 -- pNext, then f04..f24 */
} BrGbiRectVert;

/* The clip-node float slots, as slice2_16.h already documents them for
 * 0x10022350 (dst[7..9] are +0x1C/+0x20/+0x24) and 0x10022DC0 (w is +0x18,
 * the coordinates are +0x04/+0x08/+0x0C):
 *
 *   f04 x   f08 y   f0C z   f10 u   f14 v   f18 w   f1C r   f20 g   f24 b
 */

/* --- the Direct3D device the state flush drives ---------------------
 * IDirect3DDevice2, pinned by two slots: byte offset 0x5C is SetRenderState
 * (the values passed are D3DRENDERSTATE_ZWRITEENABLE 0x0E, ALPHATESTENABLE
 * 0x0F, SRCBLEND 0x13, DESTBLEND 0x14, CULLMODE 0x16, ZFUNC 0x17, ALPHAREF
 * 0x18, ALPHAFUNC 0x19, TEXTUREMAG 0x2C, TEXTUREMIN 0x2D, ALPHABLENDENABLE
 * 0x1B) and byte offset 0x78 is DrawIndexedPrimitive, called with exactly
 * its seven arguments plus `this`.  Those two offsets are SetRenderState /
 * DrawIndexedPrimitive on IDirect3DDevice2 and on no other Direct3D device
 * interface, which is what fixes the version.
 *
 * Named BrD3DDev2* and not BrD3DDev* on purpose: the contract forbids a
 * generic COM vtable name. */
struct BrD3DDev2;

typedef struct BrD3DDev2Vtbl {
    void *aSlots00[23];                                   /* +0x00..+0x58 */
    int32_t (*SetRenderState)(struct BrD3DDev2 *pThis,
                              uint32_t dwState,
                              uint32_t dwValue);          /* +0x5C */
    void *aSlots24[6];                                    /* +0x60..+0x74 */
    int32_t (*DrawIndexedPrimitive)(struct BrD3DDev2 *pThis,
                                    uint32_t dptPrimitiveType,
                                    uint32_t dvtVertexType,
                                    void *pvVertices,
                                    uint32_t cVertices,
                                    uint16_t *pwIndices,
                                    uint32_t cIndices,
                                    uint32_t dwFlags);    /* +0x78 */
} BrD3DDev2Vtbl;

typedef struct BrD3DDev2 {
    const BrD3DDev2Vtbl *pVtbl;   /* +0x00 */
} BrD3DDev2;

/* Number of deferred render states.  Bit i of `dirty` guards entry i; the
 * codes are in BrGbiRectRenderState[]. */
#define BR_GBI_RECT_RS_COUNT 11

/* The render-state code bit i defers, in the original's order. */
extern const uint32_t BrGbiRectRenderState[BR_GBI_RECT_RS_COUNT];

/* NOT in the original: 0x10021560 reaches all of this through fixed
 * addresses.  slice2_16.h already gathers the GBI half into BrGbiState but
 * exposes no singleton, so the pointer to it lives here too.
 *
 * Field names are positional wherever the meaning is not established by a
 * call this packet can see. */
typedef struct BrGbiRectState {
    /* the slice2_16 aggregate: scissor, geo.cur (0x104C5178), light.off
     * (0x104C5154/0x104C5160/0x104C1690), f0A79E8, f4C5174 */
    struct BrGbiState *pGbi;

    BrD3DDev2 *pDev;                            /* 0x10277368 */
    uint32_t   dirty;                           /* 0x10277370 */
    uint32_t   aPending[BR_GBI_RECT_RS_COUNT];  /* 0x10277378 .. 0x102773A0 */
    uint32_t   aShadow[BR_GBI_RECT_RS_COUNT];   /* 0x102773F8 .. 0x10277420 */

    void      *pvVertices;   /* 0x104BC1A0  DrawIndexedPrimitive arg 3 */
    uint32_t   cVertices;    /* 0x104C5188  arg 4 */
    uint16_t  *pwIndices;    /* 0x104C4D50  arg 5 */
    uint32_t   cIndices;     /* 0x104C518C  arg 6, and the flush guard */

    void     **ap4C0BC0;     /* 0x104C0BC0  array walked after the draw */
    int32_t    c4C5190;      /* 0x104C5190  its length; SIGNED, tested > 0 */

    int32_t    f4C0DC0;      /* 0x104C0DC0  0 => every vertex colour is 1.0f */
    /* 0x104BBF04, 0x104C0BAC, 0x104BBEB8 -- the colour the two vertices that
     * take their x from `lrs` get.  The other two take BrGbiState.light.off,
     * which slice2_16.h already owns. */
    float      aRgb4BBF04[3];
} BrGbiRectState;

BrGbiRectState *BrGbiRectGetState(void);

/* 0x10021560
 *
 * Builds four clip-space vertices from a screen-space rectangle and emits
 * two triangles through 0x1001D420, flushing any deferred Direct3D render
 * state first.
 *
 * The argument names are slice2_16.h's, which is binding; the LOWER-RIGHT
 * pair really does come first.  What the code does with them:
 *
 *   x_from(v) = ((v / 4.0f) - w2) / w2 / e     w2 = screenW / 2.0f
 *   y_from(v) = ((screenH - v / 4.0f) - h2) / h2 / e   h2 = screenH / 2.0f
 *   z         = f4C5174 / e
 *   w         = 1.0f / e
 *
 * with `e` the raw dword at 0x100A79E8 REINTERPRETED AS A FLOAT, and the
 * four literals read out of .rdata: 0x1008F3C4 = 1.0f, 0x1008F3CC = 2.0f,
 * 0x1008F3D0 = 4.0f, 0x1008F3D4 = 8.0f.  The /4 is the 10.2 fixed point the
 * rectangle arrives in; the y form is the screen-to-NDC flip.
 *
 * GOTCHA: `tile` is accepted and never read.  The original loads the other
 * four argument slots and leaves the fifth alone.
 *
 * GOTCHA: 0x100A79E8 and 0x104C5174 are typed uint32_t in slice2_16.h
 * because their setters (0x1001CD60, 0x1001CD80) copy a display-list word
 * into them verbatim.  Here they are used as FLOATS.  0x100A79E8 is 1.0f in
 * the shipped image and is a DIVISOR five times over -- a display list that
 * puts 0 there produces infinities, and there is no guard.
 *
 * GOTCHA: the texture coordinates do not come from the rectangle at all.
 * They come from the SCISSOR (BrGbiState.scissor), scaled by 8.0f, and they
 * are paired with the corners like this:
 *
 *      x from `lrs`  <-> u = scissor.ulx * 8      (0x118AA080)
 *      x from `uls`  <-> u = scissor.lrx * 8      (0x1182983C)
 *      y from `lrt`  <-> v = scissor.lry * 8      (0x118A9870)
 *      y from `ult`  <-> v = scissor.uly * 8      (0x11829838)
 *
 * i.e. the pairing is consistent for x/u only if `lrs`/`uls` are really the
 * upper-left/lower-right pair in that order, and it is inverted for y/v
 * either way.  Preserved exactly; no attempt is made to "fix" it.
 *
 * GOTCHA: the winding of both triangles is chosen by bit 0x1000 of
 * BrGbiState.geo.cur (F3D G_CULL_FRONT).  Set => (v3,v0,v1) and (v0,v2,v1);
 * clear => the exact reverse of each. */
void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile);

/* =====================================================================
 * Cross-slice dependencies
 * ===================================================================== */

/* XSLICE 0x1005FF60 -- already implemented in slice3_39.c. */
extern void BrMenuSub1005FF60(void);
/* XSLICE 0x1005FFF0 -- already implemented in slice3_39.c. */
extern void BrMenuSub1005FFF0(void);

/* XSLICE 0x100A81C0 -- screen width.  Name taken from slice2_18.h, which
 * already declares and defines it.  (slice3_39.h declares the SAME original
 * global under the second name g_Br0A81C0; that pre-existing duplication is
 * not made worse here.) */
extern int32_t BrG_0A81C0;
/* XSLICE 0x100A81C4 -- screen height, same provenance. */
extern int32_t BrG_0A81C4;

/* XSLICE 0x1001D420 -- clip and emit one triangle.  Each argument is the
 * BASE of a BrGbiRectVert; the callee derives the clip node itself with
 * `+ 0x40`.  Nothing in this slice implements it and no other packet claims
 * it, so the name is coined here in slice2_16.h's house style. */
extern void BrGbiCall1001D420(BrGbiRectVert *pA, BrGbiRectVert *pB,
                              BrGbiRectVert *pC);

#ifdef __cplusplus
}
#endif

#endif /* SLICE4_51_H */
