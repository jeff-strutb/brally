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
/* RE-TARGETED to the GLIDE original 0x100215C0 (1,032 B / 239 insns).  The
 * D3D-shaped body that used to live here modelled a pointer-reached state
 * block, a null guard and a deferred-state flush that the Glide function has
 * none of: it reads flat globals, calls nothing but the triangle submitter,
 * and takes its four edges as UNSIGNED (orig converts each with the
 * lo/hi=0 `fild qword` pair, which is MSVC's unsigned->double sequence).
 *
 * BUILD FLAG: this translation unit needs `/O2 /Op`.  Every int->float
 * conversion in the original is followed by `fstp dword [tmp]; fld dword
 * [tmp]` -- MSVC 5.0's round-to-float idiom, which /O2 alone never emits (it
 * keeps the value in the x87 register).  /Op also stops `/ 2.0f` and
 * `/ 4.0f` being strength-reduced to a multiply; the two divisors are still
 * modelled as unfoldable externs because the original divides by memory.
 * Under `/O2 /Op` this body is 1,034 B / 240 insns against 1,032 / 239 with
 * a single surplus `fxch` -- see docs note.  Under plain /O2 the rounds are
 * absent and it is 1,014 B / 225 insns.
 *
 * @implements 0x100215C0 glide BrGbiCall10021560 -- NOT TAGGED: one fxch out. */
/* --- BRGlide.dll flat globals this function reads directly ------------ */
#define BR_GBI_RECT_C_RGB   255.0f   /* 0x10077418; the else-arm literal is
                                      * 0x437F0000 = 255.0f in the original */
extern float        BrGbiRectK_HALF;    /* 0x10077414 -- not foldable */
extern float        BrGbiRectK_FIXED;   /* 0x10077408 -- not foldable */
extern int          BrGbiRectG_A7514;
extern int          BrGbiRectG_A7518;
extern float        BrGbiRectG_A9A54;
extern float        BrGbiRectG_5D17C4;
extern int          BrGbiRectG_5CDA04;
extern float        BrGbiRectG_5CCD44;
extern float        BrGbiRectG_5CD9F4;
extern float        BrGbiRectG_5CCCF8;
extern float        BrGbiRectG_5D17A4;
extern float        BrGbiRectG_5D17B4;
extern float        BrGbiRectG_5CE2D0;
extern int          BrGbiRectG_5D17C8;
extern int          BrGbiRectG_18ED198;
extern int          BrGbiRectG_186C954;
extern int          BrGbiRectG_186C950;
extern int          BrGbiRectG_18EC988;

void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile)
{
    BrGbiRectVert v[4];
    float cy, w2, h2;
    float fLrs, fLrt, fUls, fUlt;
    float xLrs, xUls, yLrt, yUlt;
    float u0, u1, vt0, vt1;

    (void)tile;

    /* The four edges are pre-divided by the fixed-point scale BEFORE the
     * half-extent block, two before and two after: that is what starts two
     * fdivs at +0x77 ahead of the /HALF pair and makes the whole divider-
     * pipeline block (+0x55..+0xd3) instruction-identical to the original.
     * Post-restructure the chains subtract the pre-divided temps. */
    fLrs = (float)(unsigned int)lrs / BrGbiRectK_FIXED;
    fLrt = (float)(unsigned int)lrt / BrGbiRectK_FIXED;

    /* h2 BEFORE w2: the spill-slot homes follow computation order, and the
     * whole sub/div drain downstream keys off which of the two lives in
     * [esp+4].  h-first: 129 masked diff bytes at exact 1,032-byte length;
     * w-first: 547 (though its +0x55..+0xd3 prefix matches exactly).  All
     * other placements of the setup lines are inert (9 probed, all tie). */
    cy = (float)BrGbiRectG_A7518;
    h2 = cy / BrGbiRectK_HALF;
    w2 = (float)BrGbiRectG_A7514 / BrGbiRectK_HALF;

    fUls = (float)(unsigned int)uls / BrGbiRectK_FIXED;
    fUlt = (float)(unsigned int)ult / BrGbiRectK_FIXED;

    xLrs = ((fLrs - w2) / w2) / BrGbiRectG_A9A54;
    yLrt = ((cy - fLrt - h2) / h2) / BrGbiRectG_A9A54;
    xUls = ((fUls - w2) / w2) / BrGbiRectG_A9A54;
    yUlt = ((cy - fUlt - h2) / h2) / BrGbiRectG_A9A54;

    v[3].node.f04 = xLrs;  v[3].node.f08 = yLrt;
    v[1].node.f04 = xLrs;  v[1].node.f08 = yUlt;
    v[0].node.f04 = xUls;  v[0].node.f08 = yLrt;
    v[2].node.f04 = xUls;  v[2].node.f08 = yUlt;

    v[2].node.f0C = BrGbiRectG_5D17C4 / BrGbiRectG_A9A54;
    v[1].node.f0C = v[2].node.f0C;
    v[0].node.f0C = v[2].node.f0C;
    v[3].node.f0C = v[2].node.f0C;

    v[2].node.f18 = BR_GBI_RECT_C_ONE / BrGbiRectG_A9A54;
    v[1].node.f18 = v[2].node.f18;
    v[0].node.f18 = v[2].node.f18;
    v[3].node.f18 = v[2].node.f18;

    if (BrGbiRectG_5CDA04 != 0) {
        v[0].node.f1C = BrGbiRectG_5CCD44 * BR_GBI_RECT_C_RGB;
        v[3].node.f1C = v[0].node.f1C;
        v[0].node.f20 = BrGbiRectG_5CD9F4 * BR_GBI_RECT_C_RGB;
        v[3].node.f20 = v[0].node.f20;
        v[0].node.f24 = BrGbiRectG_5CCCF8 * BR_GBI_RECT_C_RGB;
        v[3].node.f24 = v[0].node.f24;

        v[2].node.f1C = BrGbiRectG_5D17A4;
        v[1].node.f1C = BrGbiRectG_5D17A4;
        v[2].node.f20 = BrGbiRectG_5D17B4;
        v[1].node.f20 = BrGbiRectG_5D17B4;
        v[2].node.f24 = BrGbiRectG_5CE2D0;
        v[1].node.f24 = BrGbiRectG_5CE2D0;
    } else {
        v[2].node.f1C = BR_GBI_RECT_C_RGB;
        v[1].node.f1C = BR_GBI_RECT_C_RGB;
        v[0].node.f1C = BR_GBI_RECT_C_RGB;
        v[3].node.f1C = BR_GBI_RECT_C_RGB;
        v[2].node.f20 = BR_GBI_RECT_C_RGB;
        v[1].node.f20 = BR_GBI_RECT_C_RGB;
        v[0].node.f20 = BR_GBI_RECT_C_RGB;
        v[3].node.f20 = BR_GBI_RECT_C_RGB;
        v[2].node.f24 = BR_GBI_RECT_C_RGB;
        v[1].node.f24 = BR_GBI_RECT_C_RGB;
        v[0].node.f24 = BR_GBI_RECT_C_RGB;
        v[3].node.f24 = BR_GBI_RECT_C_RGB;
    }

    u0  = (float)BrGbiRectG_18ED198 * BR_GBI_RECT_C_UVSCL;
    u1  = (float)BrGbiRectG_186C954 * BR_GBI_RECT_C_UVSCL;
    vt0 = (float)BrGbiRectG_186C950 * BR_GBI_RECT_C_UVSCL;
    vt1 = (float)BrGbiRectG_18EC988 * BR_GBI_RECT_C_UVSCL;

    v[3].node.f10 = u0;  v[3].node.f14 = vt1;
    v[1].node.f10 = u0;  v[1].node.f14 = vt0;
    v[0].node.f10 = u1;  v[0].node.f14 = vt1;
    v[2].node.f10 = u1;  v[2].node.f14 = vt0;

    if ((BrGbiRectG_5D17C8 & 0x1000) != 0) {
        BrGbiCall1001D420(&v[3], &v[0], &v[1]);
        BrGbiCall1001D420(&v[0], &v[2], &v[1]);
    } else {
        BrGbiCall1001D420(&v[1], &v[0], &v[3]);
        BrGbiCall1001D420(&v[1], &v[2], &v[0]);
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

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_10ac5bb4;
extern int DAT_10ac5e50;
extern int DAT_10ac6050;
extern int DAT_10ac610c;
extern int DAT_10ac6114;
extern int * DAT_10ac6730;
extern int DAT_10ac6734;
extern int DAT_10ac6738;
extern int DAT_10ac673c;
int FUN_10037720();


/* WHAT IT DOES: return 1 if live input or demo playback is active. */
/* @implements 0x10037780 glide BrInputOrPlaybackActive */

int BrInputOrPlaybackActive(void)

{
  int iVar1;
  
  iVar1 = BrFn1005FFD0();
  if (iVar1 < 0) {
    iVar1 = FUN_10037720();
    if (iVar1 == 0) {
      return 0;
    }
  }
  return 1;
}


int FUN_100038f0();
int BrDPlayRawSend();

/* WHAT IT DOES: send one small fixed command message -- a three-word packet
 * with a fixed command code and two arguments -- to a player, and only if
 * that player's channel is actually up. */
/* @implements 0x100371F0 glide FUN_100371f0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100371f0(int *param_1,int param_2,int param_3)

{
  struct { int cmd; int a; int b; } msg;
  
  if ((param_1 != (int *)0x0) && (*param_1 != 0)) {
    msg.a = param_2;
    msg.cmd = 0x60000008;
    msg.b = param_3;
    if (param_1[3] != 0) {
      FUN_100038f0(param_1,&msg,0xc,param_1[2],1);
    }
    return BrDPlayRawSend(*param_1,param_1[2],0,1,&msg,0xc);
  }
  return 0;
}


#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
int FUN_10036a30(int, int, LPCSTR, LPCVOID *, int);
extern char DAT_10ac4db0[];
extern int g_brAA288C;
int BrDPlayRawSend(int, int, int, int, void *, unsigned int);

/* WHAT IT DOES: send a message to one network player, taking care of the
 * global memory handle DirectPlay wants and freeing it afterwards. Refuses
 * and reports failure when there is no target or the network layer is
 * shutting down. */
/* @implements 0x100368A0 glide FUN_100368a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100368a0(HWND param_1, int *param_2, int param_3)
{
  HGLOBAL hMem;
  LPCSTR lpString;
  int result;
  int *pMem;
  LPCVOID local_4;

  pMem = 0;
  local_4 = 0;
  if (param_2 == 0 || *param_2 == 0 || g_brAA288C != 0) {
    return 0;
  }
  hMem = GlobalAlloc(0x42, 0xc9);
  lpString = GlobalLock(hMem);
  if (lpString == 0) {
    result = 0x8007000e;
  }
  else {
    strcpy((char *)lpString, DAT_10ac4db0);
    result = FUN_10036a30(*param_2, param_2[2], lpString, &local_4, param_3);
    if (result >= 0) {
      PostMessageA(param_1, 0x501, 0, (LPARAM)local_4);
      local_4 = 0;
      {
        int n = lstrlenA(lpString);
        n += 8;
        hMem = GlobalAlloc(0x42, n);
        pMem = GlobalLock(hMem);
        if (pMem == 0) {
          result = 0x8007000e;
        }
        else {
          *pMem = (param_3 != 0) + 0x60000000;
          lstrcpyA((LPSTR)(pMem + 1), lpString);
          result = BrDPlayRawSend(*param_2, param_2[2], 0, 1, pMem, n);
        }
      }
    }
  }
  if (lpString != 0) {
    GlobalUnlock(GlobalHandle(lpString));
    GlobalFree(GlobalHandle(lpString));
  }
  if (local_4 != 0) {
    GlobalUnlock(GlobalHandle(local_4));
    GlobalFree(GlobalHandle(local_4));
  }
  if (pMem != 0) {
    GlobalUnlock(GlobalHandle(pMem));
    GlobalFree(GlobalHandle(pMem));
  }
  return result;
}


extern char DAT_10ac3070[];

/* WHAT IT DOES: turn a DirectPlay error code into its printable name for the
 * log. A long comparison chain, and it returns a generic unknown string for
 * anything it does not recognise. */
/* @implements 0x100372B0 glide FUN_100372b0 */
/* auto-filed from ghidra --refine; transforms: as-is */

char *FUN_100372b0(int hr)
{
  if (hr <= (int)0x80004001) {
    if (hr != (int)0x80004001) {
      if (hr != (int)0x8000000A) goto unknown;
      return "DPERR_PENDING";
    }
    return "DPERR_UNSUPPORTED";
  }
  if (hr <= (int)0x80004005) {
    if (hr != (int)0x80004005) {
      if (hr != (int)0x80004002) goto unknown;
      return "DPERR_NOINTERFACE";
    }
    return "DPERR_GENERIC";
  }
  if (hr <= (int)0x80070057) {
    if (hr != (int)0x80070057) {
      if (hr != (int)0x8007000E) goto unknown;
      return "DPERR_OUTOFMEMORY";
    }
    return "DPERR_INVALIDPARAMS";
  }
  if (hr <= (int)0x8877000A) {
    if (hr != (int)0x8877000A) {
      if (hr != (int)0x88770005) goto unknown;
      return "DPERR_ALREADYINITIALIZED";
    }
    return "DPERR_ACCESSDENIED";
  }
  if (hr <= (int)0x8877001E) {
    if (hr != (int)0x8877001E) {
      if (hr != (int)0x88770014) goto unknown;
      return "DPERR_ACTIVEPLAYERS";
    }
    return "DPERR_BUFFERTOOSMALL";
  }
  if (hr <= (int)0x88770032) {
    if (hr != (int)0x88770032) {
      if (hr != (int)0x88770028) goto unknown;
      return "DPERR_CANTADDPLAYER";
    }
    return "DPERR_CANTCREATEGROUP";
  }
  if (hr <= (int)0x88770046) {
    if (hr != (int)0x88770046) {
      if (hr != (int)0x8877003C) goto unknown;
      return "DPERR_CANTCREATEPLAYER";
    }
    return "DPERR_CANTCREATESESSION";
  }
  if (hr <= (int)0x8877005A) {
    if (hr != (int)0x8877005A) {
      if (hr != (int)0x88770050) goto unknown;
      return "DPERR_CAPSNOTAVAILABLEYET";
    }
    return "DPERR_EXCEPTION";
  }
  if (hr <= (int)0x88770082) {
    if (hr != (int)0x88770082) {
      if (hr != (int)0x88770078) goto unknown;
      return "DPERR_INVALIDFLAGS";
    }
    return "DPERR_INVALIDOBJECT";
  }
  if (hr <= (int)0x8877009B) {
    if (hr != (int)0x8877009B) {
      if (hr != (int)0x88770096) goto unknown;
      return "DPERR_INVALIDPLAYER";
    }
    return "DPERR_INVALIDGROUP";
  }
  if (hr <= (int)0x887700AA) {
    if (hr != (int)0x887700AA) {
      if (hr != (int)0x887700A0) goto unknown;
      return "DPERR_NOCAPS";
    }
    return "DPERR_NOCONNECTION";
  }
  if (hr <= (int)0x887700C8) {
    if (hr != (int)0x887700C8) {
      if (hr != (int)0x887700BE) goto unknown;
      return "DPERR_NOMESSAGES";
    }
    return "DPERR_NONAMESERVERFOUND";
  }
  if (hr <= (int)0x887700DC) {
    if (hr != (int)0x887700DC) {
      if (hr != (int)0x887700D2) goto unknown;
      return "DPERR_NOPLAYERS";
    }
    return "DPERR_NOSESSIONS";
  }
  if (hr <= (int)0x887700F0) {
    if (hr != (int)0x887700F0) {
      if (hr != (int)0x887700E6) goto unknown;
      return "DPERR_SENDTOOBIG";
    }
    return "DPERR_TIMEOUT";
  }
  if (hr <= (int)0x8877010E) {
    if (hr != (int)0x8877010E) {
      if (hr != (int)0x887700FA) goto unknown;
      return "DPERR_UNAVAILABLE";
    }
    return "DPERR_BUSY";
  }
  if (hr <= (int)0x88770122) {
    if (hr != (int)0x88770122) {
      if (hr != (int)0x88770118) goto unknown;
      return "DPERR_USERCANCEL";
    }
    return "DPERR_CANNOTCREATESERVER";
  }
  if (hr <= (int)0x88770136) {
    if (hr != (int)0x88770136) {
      if (hr != (int)0x8877012C) goto unknown;
      return "DPERR_PLAYERLOST";
    }
    return "DPERR_SESSIONLOST";
  }
  if (hr <= (int)0x8877014A) {
    if (hr != (int)0x8877014A) {
      if (hr != (int)0x88770140) goto unknown;
      return "DPERR_UNINITIALIZED";
    }
    return "DPERR_NONEWPLAYERS";
  }
  if (hr <= (int)0x8877015E) {
    if (hr != (int)0x8877015E) {
      if (hr != (int)0x88770154) goto unknown;
      return "DPERR_INVALIDPASSWORD";
    }
    return "DPERR_CONNECTING";
  }
  if (hr <= (int)0x887703F2) {
    if (hr != (int)0x887703F2) {
      if (hr != (int)0x887703E8) goto unknown;
      return "DPERR_BUFFERTOOLARGE";
    }
    return "DPERR_CANTCREATEPROCESS";
  }
  if (hr <= (int)0x88770406) {
    if (hr != (int)0x88770406) {
      if (hr != (int)0x887703FC) goto unknown;
      return "DPERR_APPNOTSTARTED";
    }
    return "DPERR_INVALIDINTERFACE";
  }
  if (hr <= (int)0x8877041A) {
    if (hr != (int)0x8877041A) {
      if (hr != (int)0x88770410) goto unknown;
      return "DPERR_NOSERVICEPROVIDER";
    }
    return "DPERR_UNKNOWNAPPLICATION";
  }
  if (hr <= (int)0x88770438) {
    if (hr != (int)0x88770438) {
      if (hr != (int)0x8877042E) goto unknown;
      return "DPERR_NOTLOBBIED";
    }
    return "DPERR_SERVICEPROVIDERLOADED";
  }
  if (hr <= (int)0x8877044C) {
    if (hr != (int)0x8877044C) {
      if (hr != (int)0x88770442) goto unknown;
      return "DPERR_ALREADYREGISTERED";
    }
    return "DPERR_NOTREGISTERED";
  }
  if (hr <= (int)0x887707DA) {
    if (hr != (int)0x887707DA) {
      if (hr != (int)0x887707D0) goto unknown;
      return "DPERR_AUTHENTICATIONFAILED";
    }
    return "DPERR_CANTLOADSSPI";
  }
  if (hr <= (int)0x887707EE) {
    if (hr != (int)0x887707EE) {
      if (hr != (int)0x887707E4) goto unknown;
      return "DPERR_ENCRYPTIONFAILED";
    }
    return "DPERR_SIGNFAILED";
  }
  if (hr <= (int)0x88770802) {
    if (hr != (int)0x88770802) {
      if (hr != (int)0x887707F8) goto unknown;
      return "DPERR_CANTLOADSECURITYPACKAGE";
    }
    return "DPERR_ENCRYPTIONNOTSUPPORTED";
  }
  if (hr <= (int)0x88770816) {
    if (hr != (int)0x88770816) {
      if (hr != (int)0x8877080C) goto unknown;
      return "DPERR_CANTLOADCAPI";
    }
    return "DPERR_NOTLOGGEDIN";
  }
  if (hr != (int)0x88770820) {
    if (hr != 0) {
      goto unknown;
    }
    return "DP_OK";
  }
  return "DPERR_LOGONDENIED";
unknown:
  wsprintfA(DAT_10ac3070, "0x%08X", hr);
  return DAT_10ac3070;
}


extern int DAT_10ac40a8;

/* WHAT IT DOES: fill in the session description used when hosting -- copies
 * the player's chosen session name in, if they set one longer than a single
 * character, and clears the trailing field. */
/* @implements 0x100367C0 glide FUN_100367c0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_100367c0(char *param_1)

{
  if (strlen((char *)&DAT_10ac40a8) > 1) {
    strcpy(param_1, (char *)&DAT_10ac40a8);
  }
  *(int *)(param_1 + 0xc8) = 0;
  return;
}

#endif /* BR_MATCHING_BUILD */
