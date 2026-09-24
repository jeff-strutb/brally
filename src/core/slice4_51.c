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

/* The Glide body of the textured-rectangle slot, 0x100215C0, is filed in
 * drawing/br_dltexrect.c. */

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

