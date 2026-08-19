/* slice4_51.c -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * See slice4_51.h for what is here, what is not, and why -- in particular
 * for the packet/name mismatch that makes work/slice4/agent51.asm disagree
 * with the `WANTED AS` lines it carries.
 */
#include "slice4_51.h"

#include <string.h>

#include "slice2_13.h"   /* BrDPlay4Obj, BrDPlayGetState, BR_DP_E_* */
#include "slice2_16.h"   /* BrGbiState */

/* =====================================================================
 * 0x1003D0B0 -- IDirectPlay4::GetSessionDesc into a Global* buffer
 * ===================================================================== */

/* Byte offset 0x58 of the IDirectPlay4A vtable.  slice2_13.h stops naming
 * slots at 0x64 and covers 0x28..0x60 with the opaque `aSlots10[15]`; 0x58
 * is entry 12 of that.  The slot is reached through slice2_13's type rather
 * than through a second, incompatible vtable declaration of this packet's
 * own -- a previous round produced exactly that clash. */
#define BR_DP4_GETSESSIONDESC_SLOT ((0x58 - 0x28) / 4)

typedef int32_t (*BrDPlay4GetSessionDescFn)(BrDPlay4Obj *pThis,
                                            void *pvData,
                                            uint32_t *pcbData);

static BrDPlay4GetSessionDescFn BrDPlay4GetSessionDesc(BrDPlay4Obj *pObj)
{
    BrDPlay4GetSessionDescFn pfn;
    void *pv = pObj->pVtbl->aSlots10[BR_DP4_GETSESSIONDESC_SLOT];

    /* DEVIATION: copied rather than cast.  C99 does not define object-to-
     * function pointer conversion; the original is machine code and has no
     * such distinction.  memcpy keeps the port warning-free everywhere. */
    memcpy(&pfn, &pv, sizeof pfn);
    return pfn;
}

int32_t BrSub1003D0B0(struct BrDPlay4Obj *pObjIn, void **ppvOut)
{
    BrDPlay4Obj              *pObj = (BrDPlay4Obj *)pObjIn;
    BrDPlay4GetSessionDescFn  pfn  = BrDPlay4GetSessionDesc(pObj);
    BrDPlayState             *pSt  = BrDPlayGetState();
    uint32_t                  cb   = 0;
    void                     *pv;
    int32_t                   hr;

    /* Sizing call: lpData = NULL.  The original reuses its own first
     * argument slot on the caller's stack as the size out-parameter. */
    hr = pfn(pObj, NULL, &cb);
    if (hr != BR_DP_E_BUFFERTOOSMALL)
        return hr;          /* GOTCHA: equality, so even S_OK bails out */

    /* DEVIATION: the original calls GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,
     * cb) + GlobalLock here and GlobalUnlock(GlobalHandle(p)) +
     * GlobalFree(GlobalHandle(p)) on the way out.  slice2_13 already models
     * those two exact pairs as BrDPlayOs::pfnAlloc / pfnFree, and its caller
     * (0x1000C670) frees this very buffer through pfnFree, so allocating
     * through anything else would mismatch the free. */
    pv = pSt->os.pfnAlloc(cb);
    if (pv == NULL)
        return BR_DP_E_OUTOFMEMORY;

    hr = pfn(pObj, pv, &cb);
    if (hr >= 0) {
        *ppvOut = pv;       /* only ever written on success */
        pv = NULL;
    }

    if (pv != NULL)
        pSt->os.pfnFree(pv);

    return hr;
}

/* =====================================================================
 * 0x1003E070 -- `call 0x1005FF60` / `jmp 0x1005FFF0`
 * ===================================================================== */

/* WHAT IT DOES: refreshes both sets of input edges for this frame --
 * keyboard keys and controller buttons -- so the menus can tell a fresh
 * press from a held one. */
/* @implements 0x1003E070 d3d BrFn1003E070 */
void BrFn1003E070(void)
{
    BrMenuSub1005FF60();
    BrMenuSub1005FFF0();
}

/* =====================================================================
 * 0x10021560 -- the tile-rectangle sink
 * ===================================================================== */

/* .rdata literals, read out of orig/BRD3D.dll rather than guessed. */
#define BR_GBI_RECT_C_ONE    1.0f    /* 0x1008F3C4 -- becomes w */
#define BR_GBI_RECT_C_HALF   2.0f    /* 0x1008F3CC -- screen extent / this */
#define BR_GBI_RECT_C_FIXED  4.0f    /* 0x1008F3D0 -- 10.2 fixed point */
#define BR_GBI_RECT_C_UVSCL  8.0f    /* 0x1008F3D4 -- scissor -> texcoord */

/* The order is the original's: bit 0 is checked first and bit 10 last. */
const uint32_t BrGbiRectRenderState[BR_GBI_RECT_RS_COUNT] = {
    0x0Eu,   /* D3DRENDERSTATE_ZWRITEENABLE     */
    0x0Fu,   /* D3DRENDERSTATE_ALPHATESTENABLE  */
    0x13u,   /* D3DRENDERSTATE_SRCBLEND         */
    0x14u,   /* D3DRENDERSTATE_DESTBLEND        */
    0x16u,   /* D3DRENDERSTATE_CULLMODE         */
    0x17u,   /* D3DRENDERSTATE_ZFUNC            */
    0x18u,   /* D3DRENDERSTATE_ALPHAREF         */
    0x19u,   /* D3DRENDERSTATE_ALPHAFUNC        */
    0x2Cu,   /* D3DRENDERSTATE_TEXTUREMAG       */
    0x2Du,   /* D3DRENDERSTATE_TEXTUREMIN       */
    0x1Bu    /* D3DRENDERSTATE_ALPHABLENDENABLE */
};

static BrGbiRectState g_BrGbiRect;

BrGbiRectState *BrGbiRectGetState(void)
{
    return &g_BrGbiRect;
}

/* 0x100A79E8 and 0x104C5174 hold raw display-list words that this function
 * reads as floats.  Punned through memcpy so the port has no aliasing UB;
 * the bit pattern is exactly what the original's `fld dword ptr` sees. */
static float BrGbiRectBitsToFloat(uint32_t u)
{
    float f;

    memcpy(&f, &u, sizeof f);
    return f;
}

/* DEVIATION: the original dereferences the device pointer unconditionally
 * once the dirty mask is non-zero.  A NULL device is impossible there and
 * merely fatal here, so the two vtable calls are guarded.  Everything else
 * -- the shadow copies, the counter resets, the clearing of `dirty` -- runs
 * exactly as before, so no observable state differs when a device is set. */
static void BrGbiRectSetRenderState(BrGbiRectState *pSt, uint32_t state,
                                    uint32_t value)
{
    if (pSt->pDev != NULL)
        pSt->pDev->pVtbl->SetRenderState(pSt->pDev, state, value);
}

/* 0x100218CF..0x10021AEE -- flush the batch, then the deferred states. */
static void BrGbiRectFlush(BrGbiRectState *pSt)
{
    int i;

    if (pSt->cIndices != 0) {
        if (pSt->pDev != NULL)
            pSt->pDev->pVtbl->DrawIndexedPrimitive(pSt->pDev,
                                                   4u,   /* D3DPT_TRIANGLELIST */
                                                   3u,   /* D3DVT_TLVERTEX     */
                                                   pSt->pvVertices,
                                                   pSt->cVertices,
                                                   pSt->pwIndices,
                                                   pSt->cIndices,
                                                   0xCu);

        /* GOTCHA: the loop bound is re-read from 0x104C5190 on every
         * iteration, and the counter is only zeroed afterwards -- so a
         * callee that shortened the array mid-loop would be honoured.  The
         * pointers themselves are re-read from a walking cursor, not the
         * reloaded count, so only the bound is volatile. */
        for (i = 0; i < pSt->c4C5190; ++i) {
            unsigned char *p = (unsigned char *)pSt->ap4C0BC0[i];
            int32_t        v = -1;

            /* DEVIATION: the original stores a dword at +0x68 by overlay.
             * Written byte-wise so the port stays endian-agnostic, which
             * this project requires. */
            p[0x68] = (unsigned char)((uint32_t)v & 0xFFu);
            p[0x69] = (unsigned char)(((uint32_t)v >> 8) & 0xFFu);
            p[0x6A] = (unsigned char)(((uint32_t)v >> 16) & 0xFFu);
            p[0x6B] = (unsigned char)(((uint32_t)v >> 24) & 0xFFu);
        }

        pSt->cIndices  = 0;
        pSt->cVertices = 0;
        pSt->c4C5190   = 0;
    }

    for (i = 0; i < BR_GBI_RECT_RS_COUNT; ++i) {
        if ((pSt->dirty & (1u << i)) != 0) {
            BrGbiRectSetRenderState(pSt, BrGbiRectRenderState[i],
                                    pSt->aPending[i]);
            pSt->aShadow[i] = pSt->aPending[i];
        }
    }

    pSt->dirty = 0;
}

/* RENDERER SLOT -- THIS IS THE D3D IMPLEMENTATION, AND THE GLIDE ONE IS NOT
 * TRANSCRIBED.  See "Renderer slots" in CONVENTIONS.md.
 *
 *     slot           the textured-rectangle drawer, 2 aligned callsites
 *     D3D    0x10021560   1,567 bytes   396 instructions   <-- this body
 *     Glide  0x100215C0   1,032 bytes   239 instructions   NOT PORTED
 *     config/shared.csv: class `renderer`, matched_by `slot`, similarity 0.306
 *
 * 0.306 is the highest similarity of the three renderer slots in this pass,
 * which is exactly why it needs saying out loud: the two DO share a shape --
 * both unpack four edges, flip Y and emit two triangles -- and the temptation
 * is to read that as "one routine, some constants moved".  It is not.  The
 * Glide body's four calls all land on 0x1001EE70, its own triangle submitter,
 * and br_dlshared.h already records that the two builds' rect handlers "end in
 * the same five-argument call (0x100215C0 in BRGlide)" -- the same slot, not
 * the same body.
 *
 * The `d3d` tag is ACCURATE. */
/* WHAT IT DOES: actually draws a screen rectangle. It converts the four
 * edges from the display list's pixel coordinates into the renderer's own
 * space, flipping the vertical axis and dividing through by the perspective
 * factor, builds the four corners with their texture coordinates, and hands
 * over two triangles. It also flushes any renderer settings that were queued
 * up but not yet sent. Note the tile number it is given is never used, in
 * the original or here. */
/* @implements 0x10021560 d3d BrGbiCall10021560 */
void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile)
{
    BrGbiRectState *pSt  = BrGbiRectGetState();
    BrGbiState     *pGbi = (BrGbiState *)pSt->pGbi;
    BrGbiRectVert   v0, v1, v2, v3;
    float           cx, cy, w2, h2, e;
    double          xLrs, xUls, yLrt, yUlt;
    float           z, w;
    float           u0, u1, vt0, vt1;

    /* GOTCHA: the fifth argument is loaded by neither the original nor this
     * port.  Only the four rectangle edges are read. */
    (void)tile;

    /* DEVIATION: the original has no such check; the state it reads lives at
     * fixed addresses that always exist.  Here it is a pointer the
     * integration has to wire, so a null one is a link-time mistake rather
     * than a rendering condition, and crashing on it helps nobody. */
    if (pGbi == NULL)
        return;

    /* Each of these is stored to a dword and reloaded before use, so every
     * one is rounded to float first; the divisions and subtractions that
     * follow stay in x87 registers.  Reproduced as float inputs feeding a
     * double expression -- which is not an approximation of the original but
     * an exact model of it, because the x87 here runs at 53-bit precision
     * (CRT control word 0x027F -- CONVENTIONS.md).  This note used to explain
     * the double as the closest C99 could get to 80-bit temporaries "without
     * assuming long double is x87"; `long double` would in fact have been
     * WRONG, modelling a 64-bit-mantissa control word this process never
     * runs in. */
    cx = (float)BrG_0A81C0;
    cy = (float)BrG_0A81C4;
    w2 = cx / BR_GBI_RECT_C_HALF;      /* stored as float in the original */
    h2 = cy / BR_GBI_RECT_C_HALF;      /* likewise */
    e  = BrGbiRectBitsToFloat(pGbi->f0A79E8);

    xLrs = (((double)((float)lrs / BR_GBI_RECT_C_FIXED) - w2) / w2) / e;
    xUls = (((double)((float)uls / BR_GBI_RECT_C_FIXED) - w2) / w2) / e;
    yLrt = (((double)(cy - (float)lrt / BR_GBI_RECT_C_FIXED) - h2) / h2) / e;
    yUlt = (((double)(cy - (float)ult / BR_GBI_RECT_C_FIXED) - h2) / h2) / e;

    z = (float)(BrGbiRectBitsToFloat(pGbi->f4C5174) / e);
    w = (float)(BR_GBI_RECT_C_ONE / e);

    /* v0 (uls, lrt)   v1 (lrs, ult)   v2 (uls, ult)   v3 (lrs, lrt) */
    v0.node.f04 = (float)xUls;  v0.node.f08 = (float)yLrt;
    v1.node.f04 = (float)xLrs;  v1.node.f08 = (float)yUlt;
    v2.node.f04 = (float)xUls;  v2.node.f08 = (float)yUlt;
    v3.node.f04 = (float)xLrs;  v3.node.f08 = (float)yLrt;

    v0.node.f0C = z;  v1.node.f0C = z;  v2.node.f0C = z;  v3.node.f0C = z;
    v0.node.f18 = w;  v1.node.f18 = w;  v2.node.f18 = w;  v3.node.f18 = w;

    /* Colours.  The two vertices whose x comes from `lrs` (v1, v2) take
     * BrGbiState.light.off, which slice2_16.h documents as the colour
     * 0x10022350 falls back to when lighting is off; the other two take the
     * unrelated triple this module owns.  With 0x104C0DC0 clear all twelve
     * become the literal 1.0f (0x3F800000). */
    if (pSt->f4C0DC0 != 0) {
        v0.node.f1C = pSt->aRgb4BBF04[0];
        v0.node.f20 = pSt->aRgb4BBF04[1];
        v0.node.f24 = pSt->aRgb4BBF04[2];
        v3.node.f1C = pSt->aRgb4BBF04[0];
        v3.node.f20 = pSt->aRgb4BBF04[1];
        v3.node.f24 = pSt->aRgb4BBF04[2];

        v1.node.f1C = pGbi->light.off[0];
        v1.node.f20 = pGbi->light.off[1];
        v1.node.f24 = pGbi->light.off[2];
        v2.node.f1C = pGbi->light.off[0];
        v2.node.f20 = pGbi->light.off[1];
        v2.node.f24 = pGbi->light.off[2];
    } else {
        v0.node.f1C = 1.0f; v0.node.f20 = 1.0f; v0.node.f24 = 1.0f;
        v1.node.f1C = 1.0f; v1.node.f20 = 1.0f; v1.node.f24 = 1.0f;
        v2.node.f1C = 1.0f; v2.node.f20 = 1.0f; v2.node.f24 = 1.0f;
        v3.node.f1C = 1.0f; v3.node.f20 = 1.0f; v3.node.f24 = 1.0f;
    }

    /* Texture coordinates: the latched TILE, not the rectangle.  These four
     * globals used to be called the scissor here, and on that reading "the
     * texture coordinates come from the scissor" was a standing GOTCHA in
     * slice4_51.h.  0x1001CF30 is opcode 0xF2 -- G_SETTILESIZE -- so they are
     * uls/lrs/ult/lrt, and a tile rectangle taking its texture coordinates
     * from the tile is not a gotcha at all.  See slice2_16.h. */
    u0  = (float)pGbi->tile.uls * BR_GBI_RECT_C_UVSCL;      /* 0x118AA080 */
    u1  = (float)pGbi->tile.lrs * BR_GBI_RECT_C_UVSCL;      /* 0x1182983C */
    vt0 = (float)pGbi->tile.ult * BR_GBI_RECT_C_UVSCL;      /* 0x11829838 */
    vt1 = (float)pGbi->tile.lrt * BR_GBI_RECT_C_UVSCL;      /* 0x118A9870 */

    v0.node.f10 = u1;  v0.node.f14 = vt1;
    v1.node.f10 = u0;  v1.node.f14 = vt0;
    v2.node.f10 = u1;  v2.node.f14 = vt0;
    v3.node.f10 = u0;  v3.node.f14 = vt1;

    if (pSt->dirty != 0)
        BrGbiRectFlush(pSt);

    /* GOTCHA: bit 0x1000 of the geometry mode (F3D G_CULL_FRONT) picks the
     * winding.  The two argument triples of the second branch are the exact
     * reverse of the first branch's, vertex for vertex. */
    if ((pGbi->geo.cur & 0x1000u) != 0) {
        BrGbiCall1001D420(&v3, &v0, &v1);
        BrGbiCall1001D420(&v0, &v2, &v1);
    } else {
        BrGbiCall1001D420(&v1, &v0, &v3);
        BrGbiCall1001D420(&v1, &v2, &v0);
    }
}

/* =====================================================================
 * Not done, and why
 * =====================================================================
 *
 * 0x1007C8A0  BrFtolTrunc
 *     Already implemented, in port/src/br_crt.c, declared in br_crt.h, and
 *     already used by slice2_21 / slice3_32 / slice3_40.  The contract
 *     forbids duplicating it and it is byte-for-byte the same MSVC __ftol
 *     the packet listed.  Nothing to do.
 *
 * 0x10056FF0  BrOptFn10056FF0
 *     Skipped.  Two independent reasons:
 *
 *     (a) The listing supplied in work/slice4/agent51.asm for this name is
 *         sub_100558A0, which is a DIFFERENT function and already has its
 *         own name (BrOptFn100558A0, slice2_25.h, and handed to a later pass --
 *         itself under a third address).  Implementing 0x100558A0's body
 *         under the name BrOptFn10056FF0 would wire the wrong screen into
 *         the options menu and would be undetectable at link time.
 *
 *     (b) The real 0x10056FF0 (1532 bytes, asm/10050000.asm) is an options-
 *         screen constructor: an SEH frame whose unwind index is stepped
 *         through fourteen partial-construction states, `operator new` of a
 *         0x348-byte object and then thirteen 0x1E214-byte widgets, each
 *         initialised through `pVtbl[0x38]` with eight arguments and
 *         labelled through `pVtbl[0x34]`, plus six handler addresses poked
 *         into fields of the widget.  Not one of those classes is modelled
 *         anywhere in the port, and the function has no direct caller -- it
 *         is only ever reached as a table entry.  Guessing thirteen widget
 *         layouts to produce a function nobody calls yet is exactly the
 *         "wrong-but-plausible" outcome the contract rules out.
 */
