/* br_dlglide.c -- RESPONSIBILITY: the display-list opcodes whose entire effect
 * is a change of GLIDE raster state.  See br_dlglide.h for what this module is
 * and how each fact in it was established.
 *
 * Every address literal below is from orig/BRGlide.dll, which CONVENTIONS.md
 * names as the reference.  Where the D3D build's handler at the same table
 * slot is a different function, that is called out at the site.
 *
 * Transcribed here, opcode by opcode, from the disassembly:
 *     0xDC 0x1001E2E0   0xDD 0x1001E300   0xDF 0x1001EB30
 *     0xE1 0x1001E720   0xE2 0x1001EBC0   0xED 0x1001EB50
 * Delegated, because br_dl.c already has it and is correct:
 *     0xF2 0x1001EC30
 */

#include "br_dlglide.h"

#include <string.h>

/* ==================================================================== */
/* helpers                                                              */
/* ==================================================================== */

/* The list is already in host order by the time the interpreter sees it (the
 * loader 0x10019040 byte-swaps it), so a command is two host u32s.  Read them
 * byte-wise anyway: CONVENTIONS.md forbids overlaying a struct on a foreign
 * buffer, and a display list is exactly that. */
static uint32_t br_dlgl_w(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* `shl 20 / sar 20` -- sign-extend the low twelve bits.  Written as the
 * explicit fold rather than as a signed right shift, which C99 leaves
 * implementation-defined.  0x1001E720 uses this on all four fields; the
 * scissor handlers do NOT (they use plain masks), and that difference is
 * load-bearing. */
static int32_t br_dlgl_s12(uint32_t v)
{
    int32_t x = (int32_t)(v & 0xFFFu);
    return (x >= 0x800) ? x - 0x1000 : x;
}

/* ==================================================================== */
/* state                                                                */
/* ==================================================================== */

void BrDlGlInit(BrDlGl *pGl, int32_t cyScreen)
{
    if (pGl == NULL)
        return;
    memset(pGl, 0, sizeof(*pGl));
    pGl->cyScreen = cyScreen;
    /* The window as 0x1001E1E0 / 0x1001E200 leave it for a full-screen view:
     * minimum at the origin, maximum at the window height.  Those two writers
     * are outside this module's scope, so this is a starting value rather than
     * a transcription, and it is stated as such. */
    pGl->clipMinX = 0;
    pGl->clipMinY = 0;
    pGl->clipMaxX = 0;
    pGl->clipMaxY = cyScreen;
}

float BrDlGlGet5D17C4(const BrDlGl *pGl)
{
    float f;
    uint32_t v = pGl->w5D17C4;
    memcpy(&f, &v, sizeof(f));
    return f;
}

/* ==================================================================== */
/* opcode 0xDC -- bind a texture                                        */
/* ==================================================================== */
/* THE TRACE.  D3D's 0xDC (0x1001BD70) is 149 bytes and is NOT the same function; Glide is
 * the reference, so this is the 30-byte one.
 *
 *   1001E2E5  mov  eax,[esi]          w0
 *   1001E2E7  and  eax,0xFFFFFF
 *   1001E2EC  push eax
 *   1001E2ED  call [0x118ED1CC]       the hook -- NOT an import
 *   1001E2F3  mov  ecx,[esi+4]        w1
 *   1001E2F9  lea  eax,[esi+ecx*8]    the next command
 *
 * Note the order: the hook is called BEFORE w1 is read, and `add esp,4` sits
 * between the two.  Neither matters to the result here, but the read of w1
 * after the call is why a hook that rewrites the command would be observed. */
/* 0x1001E2E0 -- G_DL opcode 0xDC, 30 bytes.  Bind the texture named by the
 * low 24 bits of w0, through the hook slot at 0x118ED1CC, and return
 * p + 8*w1.  Trace above. */
const uint8_t *BrDlGlBindTexture(BrDlGl *pGl, const uint8_t *p)
{
    uint32_t w0 = br_dlgl_w(p);
    uint32_t w1;

    pGl->hTexture = w0 & 0x00FFFFFFu;
    pGl->cBind++;
    if (pGl->hook.pfnTexSelect != NULL)
        pGl->hook.pfnTexSelect(pGl->hook.pUser, pGl->hTexture);
    else
        pGl->cNullHook++;           /* frontier: 0x118ED1CC not installed */

    w1 = br_dlgl_w(p + 4);
    /* `lea eax,[esi + ecx*8]`.  One 0xDC stands in for the run of texture
     * setup commands it was written over at load time (br_dl.h has that
     * chain), so w1 is a length in COMMANDS and the step is 8*w1.
     *
     * Two properties preserved deliberately:
     *   - w1 == 0 returns p unchanged.  The original then re-executes the same
     *     command forever; this port reproduces the arithmetic and leaves the
     *     loop bound to the caller.
     *   - w1 == 1 returns p + 8, which is what the Glide font emitter emits.
     *
     * DEVIATION: the original's `lea` is 32-bit and wraps at 4 GiB; this is
     * host pointer arithmetic and does not.  No shipped list can reach that --
     * w1 is a run length of a handful of commands -- and wrapping a host
     * pointer is not something a port can do meaningfully. */
    return p + (size_t)8u * (size_t)w1;
}

/* ==================================================================== */
/* opcode 0xDD -- re-aim that texture                                   */
/* ==================================================================== */
/* THE TRACE.  Shared with D3D 0x1001BE10 (config/shared.csv, by body).
 *
 *   1001E305  mov  ecx,[esi]          w0
 *   1001E307  mov  eax,[esi+4]        w1
 *   1001E30A  and  ecx,0xFFFFFF
 *   1001E310  push eax                -- second argument
 *   1001E311  push ecx                -- first argument
 *   1001E312  call [0x118ED1D0]
 *   1001E31B  lea  eax,[esi+8]
 *
 * cdecl, so the last push is argument one: (w0 & 0xFFFFFF, w1).  w1 is passed
 * WHOLE -- it is an address, and masking it would be a real defect. */
/* 0x1001E300 -- opcode 0xDD, 32 bytes.  Re-aim that texture at a new source
 * address, through the hook slot at 0x118ED1D0, and return p + 8. */
const uint8_t *BrDlGlRetarget(BrDlGl *pGl, const uint8_t *p)
{
    pGl->hRetarget    = br_dlgl_w(p) & 0x00FFFFFFu;
    pGl->addrRetarget = br_dlgl_w(p + 4);
    pGl->cRetarget++;
    if (pGl->hook.pfnTexRetarget != NULL)
        pGl->hook.pfnTexRetarget(pGl->hook.pUser,
                                 pGl->hRetarget, pGl->addrRetarget);
    else
        pGl->cNullHook++;           /* frontier: 0x118ED1D0 not installed */
    return p + 8;
}

/* ==================================================================== */
/* opcode 0xDF -- park a scalar for the texture-rect helper             */
/* ==================================================================== */
/* THE TRACE.  Glide-only; the D3D 0xDF (0x1001CD80) is the same shape over
 * 0x104C5174 and is slice2_16.c's BrGbiSet4C5174.
 *
 *   1001EB30  mov  eax,[esp+4]        NOTE: no push precedes this, so the
 *                                     argument really is at +4 here, where
 *                                     the other six handlers see it at +8 or
 *                                     +0xC behind their own pushes.
 *   1001EB34  mov  ecx,[eax+4]        w1
 *   1001EB37  add  eax,8
 *   1001EB3A  mov  [0x105D17C4],ecx
 *
 * The store is a plain dword move -- the handler does not interpret it.  Its
 * one reader (0x1002171C, inside the texture-rect helper 0x100215C0) does
 * `fld [0x105D17C4] / fdiv [0x100A9A54]`, so the VALUE is a float; see
 * BrDlGlGet5D17C4. */
/* 0x1001EB30 -- opcode 0xDF, 17 bytes.  Store w1 in the global 0x105D17C4
 * and return p + 8. */
const uint8_t *BrDlGlSet5D17C4(BrDlGl *pGl, const uint8_t *p)
{
    pGl->w5D17C4 = br_dlgl_w(p + 4);
    pGl->cSet5D17C4++;
    return p + 8;
}

/* ==================================================================== */
/* opcode 0xE1 -- fill rectangle, integer corners                       */
/* ==================================================================== */
/* THE TRACE.  Shared with D3D 0x1001C7A0.  CONVENTIONS.md already records "command byte
 * 0xE1 is FILL RECTANGLE with integer corners here", and the dispatch table
 * pins it: 0x100A9A58 + 0xE1*4 holds 0x1001E720.  A sibling analysis of the
 * N64 ROM found that title never emits 0xE1 at all, so this is a PC-side
 * repurposing rather than a misread of stock F3D.
 *
 * The register trace, and the pushes are the whole point:
 *
 *   1001E722  mov esi,[esp+0xC]           the argument, behind push ebx/esi
 *   1001E726  mov edx,[0x100A7518]        H, the grSstWinOpen height
 *   1001E72D  mov ebx,edx                 ebx = H
 *   1001E72F  mov eax,[esi+4]             w1
 *   1001E732  mov ecx,[esi]               w0
 *   1001E734  mov edi,eax
 *   1001E736  shl edi,0x14 ; sar edi,0x14     edi = s12(w1)       == uly
 *   1001E73C  sub ebx,edi                     ebx = H - uly
 *   1001E73E  mov edi,ecx
 *   1001E740  shl ecx,0x14                    (ecx and edi both from w0)
 *   1001E743  shl edi,8
 *   1001E746  sar ecx,0x14                    ecx = s12(w0)       == lry
 *   1001E749  sar edi,0x14                    edi = s12(w0 >> 12) == lrx
 *   1001E74C  sub edx,ecx                     edx = H - lry
 *   1001E74E  inc edi                         edi = lrx + 1
 *   1001E74F  shl eax,8
 *   1001E752  push ebx                        H - uly
 *   1001E753  dec edx                         edx = H - lry - 1
 *   1001E754  push edi                        lrx + 1
 *   1001E755  push edx                        H - lry - 1
 *   1001E756  sar eax,0x14                    eax = s12(w1 >> 12) == ulx
 *   1001E759  push eax                        ulx
 *   1001E75A  call 0x1001E380
 *   1001E762  lea  eax,[esi+8]
 *
 * cdecl -- last push first -- so the call is
 *     0x1001E380(ulx, H - lry - 1, lrx + 1, H - uly)
 * which is (minX, minY, maxX, maxY) in Glide's bottom-up window, matching the
 * four clamps 0x1001E380 opens with.
 *
 * w0 CARRIES THE LOWER-RIGHT CORNER and w1 the upper-left -- stock G_FILLRECT
 * packing, and the OPPOSITE way round from G_SETSCISSOR below.  Getting that
 * backwards mirrors the rectangle.
 *
 * The sibling 0xF6 (0x1001E320) is the same function with `sar 0x16` and an
 * `and 0x3FF` on each field instead of `sar 0x14`: 10.2, divided by four, and
 * the sign masked away.  0xE1 does none of that -- its corners are signed. */
/* 0x1001E720 -- opcode 0xE1, 73 bytes.  Fill a screen rectangle whose four
 * corners are signed 12-bit INTEGERS, by calling the emitter 0x1001E380 with
 * (ulx, H - lry - 1, lrx + 1, H - uly); returns p + 8. */
const uint8_t *BrDlGlFillRect(BrDlGl *pGl, const uint8_t *p)
{
    uint32_t w0 = br_dlgl_w(p), w1 = br_dlgl_w(p + 4);
    int32_t  H  = pGl->cyScreen;
    int32_t  lrx = br_dlgl_s12(w0 >> 12);
    int32_t  lry = br_dlgl_s12(w0);
    int32_t  ulx = br_dlgl_s12(w1 >> 12);
    int32_t  uly = br_dlgl_s12(w1);

    pGl->cFillRect++;
    if (pGl->hook.pfnFillRect != NULL)
        pGl->hook.pfnFillRect(pGl->hook.pUser,
                              ulx, H - lry - 1, lrx + 1, H - uly);
    else
        pGl->cNullHook++;       /* frontier: 0x1001E380 is not transcribed */
    return p + 8;
}

/* ==================================================================== */
/* opcodes 0xE2 and 0xED -- set the clip window, two conventions        */
/* ==================================================================== */
/* THE TRACE.  Both Glide-only (neither is in config/shared.csv), and the two are
 * SAME handler over two coordinate conventions -- exactly the relationship
 * 0xE1 has with 0xF6.  The split is confirmed independently in BRD3D.dll,
 * whose 0xE2 (0x1001CE70) shifts by 12 and masks 0xFFF while its 0xED
 * (0x1001CDA0) shifts by 14 and masks 0x3FF, at the same two instructions.
 *
 * 0x1001EB50 (0xED), and 0x1001EBC0 (0xE2) differs only in the four
 * shift/mask pairs marked:
 *
 *   1001EB52  mov esi,[esp+0xC]           the argument, behind push ebx/esi.
 *                                         NOTE it is read BEFORE the third
 *                                         push (edi) at 1001EB5C, so +0xC is
 *                                         right and +0x10 would be wrong --
 *                                         a displacement means nothing
 *                                         without the esp it is relative to.
 *   1001EB56  mov edx,[0x100A7518]        H
 *   1001EB5D  mov edi,edx
 *   1001EB61  shr eax,0xE ; and eax,0x3FF     <-- 0xE2: shr 0xC / and 0xFFF
 *   1001EB69  mov [0x105D17BC],eax            minX = ulx
 *   1001EB70  shr ecx,2   ; and ecx,0x3FF     <-- 0xE2: no shift / and 0xFFF
 *   1001EB79  sub edi,ecx
 *   1001EB7B  mov [0x105CCFE0],edi            maxY = H - uly
 *   1001EB84  shr ecx,0xE ; and ecx,0x3FF     <-- 0xE2: shr 0xC / and 0xFFF
 *   1001EB8D  push edi                        H - uly
 *   1001EB8E  mov [0x105D17B8],ecx            maxX = lrx
 *   1001EB97  shr ebx,2   ; and ebx,0x3FF     <-- 0xE2: no shift / and 0xFFF
 *   1001EBA0  push ecx                        lrx
 *   1001EBA1  sub edx,ebx
 *   1001EBA3  push edx                        H - lry
 *   1001EBA4  push eax                        ulx
 *   1001EBA5  mov [0x105D17C0],edx            minY = H - lry
 *   1001EBAB  call 0x100729D2  ->  glide2x.dll!grClipWindow
 *   1001EBB0  lea  eax,[esi+8]
 *
 * cdecl: grClipWindow(ulx, H - lry, lrx, H - uly) == (minx, miny, maxx, maxy).
 *
 * THE SHIFTS ARE `shr`, NOT `sar`, and there is no sign fold on either form --
 * unlike 0xE1 three functions up.  Preserved.
 *
 * THE Y FLIP IS THE POINT.  An F3D scissor has uly ABOVE lry in a top-down
 * screen; Glide's origin is at the bottom, so both are subtracted from the
 * window height and the two swap roles.  Store them unflipped and the window
 * comes out inverted -- minY above maxY -- which is what test_br_dlglide.c
 * asserts against. */
/* The body both scissor opcodes share.  Deliberately NOT banner-tagged with
 * either address -- 0x1001EBC0 and 0x1001EB50 are two functions in the
 * original, and each is claimed below by the wrapper that IS it.  They
 * differ only in the shift and mask applied to each of the four fields. */
static const uint8_t *br_dlgl_scissor(BrDlGl *pGl, const uint8_t *p,
                                      int fFrac)
{
    uint32_t w0 = br_dlgl_w(p), w1 = br_dlgl_w(p + 4);
    int32_t  H  = pGl->cyScreen;
    int32_t  ulx, uly, lrx, lry;

    if (fFrac) {                        /* 0xED, 0x1001EB50: 10.2 */
        ulx = (int32_t)((w0 >> 14) & 0x3FFu);
        uly = (int32_t)((w0 >> 2) & 0x3FFu);
        lrx = (int32_t)((w1 >> 14) & 0x3FFu);
        lry = (int32_t)((w1 >> 2) & 0x3FFu);
    } else {                            /* 0xE2, 0x1001EBC0: integer */
        ulx = (int32_t)((w0 >> 12) & 0xFFFu);
        uly = (int32_t)(w0 & 0xFFFu);
        lrx = (int32_t)((w1 >> 12) & 0xFFFu);
        lry = (int32_t)(w1 & 0xFFFu);
    }

    pGl->clipMinX = ulx;                /* 0x105D17BC */
    pGl->clipMaxY = H - uly;            /* 0x105CCFE0 */
    pGl->clipMaxX = lrx;                /* 0x105D17B8 */
    pGl->clipMinY = H - lry;            /* 0x105D17C0 */

    pGl->cScissor++;
    if (pGl->hook.pfnClipWindow != NULL)
        pGl->hook.pfnClipWindow(pGl->hook.pUser,
                                pGl->clipMinX, pGl->clipMinY,
                                pGl->clipMaxX, pGl->clipMaxY);
    else
        pGl->cNullHook++;               /* frontier: no grClipWindow here */
    return p + 8;
}

/* 0x1001EBC0 -- opcode 0xE2, 97 bytes.  The scissor with plain 12-bit
 * integer fields: (w >> 12) & 0xFFF and w & 0xFFF. */
const uint8_t *BrDlGlScissorInt(BrDlGl *pGl, const uint8_t *p)
{ return br_dlgl_scissor(pGl, p, 0); }

/* 0x1001EB50 -- opcode 0xED, 103 bytes.  The scissor with 10.2 fields:
 * (w >> 14) & 0x3FF and (w >> 2) & 0x3FF. */
const uint8_t *BrDlGlScissorFrac(BrDlGl *pGl, const uint8_t *p)
{ return br_dlgl_scissor(pGl, p, 1); }

/* ==================================================================== */
/* opcode 0xF2 -- set tile size.  DELEGATED, and counted.               */
/* ==================================================================== */
/* This is a FRONTIER, not a placeholder: it does nothing, says so, and a run
 * reports how many 0xF2 commands reached it.  Nothing downstream behaves as
 * though the tile size had been set.
 *
 * The real transcription is br_dl.c's static br_dl_settilesize, verified
 * instruction by instruction against 0x1001EC30 by test_br_dlglide.c, which
 * drives it through BrDlRun rather than duplicating it here.  Duplicating it
 * would give one original address two host definitions -- the failure
 * CONVENTIONS.md's "Aliased storage" section is about. */
/* DELEGATED, NOT TRANSCRIBED.  The address is written without a leading
 * banner on purpose: tools/isported.py attributes a banner whose first
 * token is an address to the function beneath it, and this function is a
 * counted frontier, not a port.  Opcode 0xF2 is 178 bytes at Glide
 * 0x1001EC30 and its transcription is br_dl.c's br_dl_settilesize; this
 * entry advances eight, counts itself, and decodes nothing. */
const uint8_t *BrDlGlSetTileSize(BrDlGl *pGl, const uint8_t *p)
{
    pGl->cF2Delegated++;
    /* 0x1001ECD3 `lea eax,[esi+8]` -- the advance is the one thing this entry
     * may state on its own, because it is a property of the opcode's length
     * and not of the decode it declines to do. */
    return p + 8;
}

/* ==================================================================== */
/* the seven slots of 0x100A9A58 this module owns                       */
/* ==================================================================== */

BrDlGlHandler BrDlGlDispatch(unsigned op)
{
    switch (op) {
    case 0xDC: return BrDlGlBindTexture;    /* 0x1001E2E0 */
    case 0xDD: return BrDlGlRetarget;       /* 0x1001E300 */
    case 0xDF: return BrDlGlSet5D17C4;      /* 0x1001EB30 */
    case 0xE1: return BrDlGlFillRect;       /* 0x1001E720 */
    case 0xE2: return BrDlGlScissorInt;     /* 0x1001EBC0 */
    case 0xED: return BrDlGlScissorFrac;    /* 0x1001EB50 */
    case 0xF2: return BrDlGlSetTileSize;    /* 0x1001EC30 -- delegated */
    default:   return NULL;
    }
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int __stdcall grBufferNumPending(void);
void __stdcall grBufferSwap(int);

/* WHAT IT DOES: spin-wait until the Glide back-buffer is idle, then swap. */
/* @implements 0x1001DD50 glide BrGlideFlipWait */

void BrGlideFlipWait(void)

{
  int iVar1;

  iVar1 = grBufferNumPending();
  while (0 < iVar1) {
    iVar1 = grBufferNumPending();
  }
  grBufferSwap(1);
  return;
}

void __stdcall grAlphaBlendFunction(int, int, int, int);
void __stdcall grAlphaTestFunction(int);
void __stdcall grAlphaCombine(int, int, int, int, int);
void __stdcall grColorCombine(int, int, int, int, int);
void __stdcall grDepthBufferFunction(int);
void __stdcall grDepthMask(int);
void __stdcall grClipWindow(int, int, int, int);
void __stdcall grCullMode(int);
void __stdcall grDrawTriangle(const void *, const void *, const void *);
void __stdcall grBufferClear(uint32_t, uint32_t, uint32_t);

/* The live Glide scissor/window state 0x1001E380 clamps against and
 * restores on exit.  Written by the scissor handlers (0xE2/0xED). */
extern int32_t  BrGlClipMinX;   /* 0x105D17BC */
extern int32_t  BrGlClipMinY;   /* 0x105D17C0 */
extern int32_t  BrGlClipMaxX;   /* 0x105D17B8 */
extern int32_t  BrGlClipMaxY;   /* 0x105CCFE0 */
/* 0xFC's latched combine words -- the magic pair below means "flat prim
 * colour", selecting the float prim channels over the RGBA5551 fill bytes. */
extern uint32_t BrGlCombineW0;  /* 0x105D17AC */
extern uint32_t BrGlCombineW1;  /* 0x105D17B0 */
extern float    BrGlPrimR;      /* 0x105D17A4, 0..255 (NOT 0..1) */
extern float    BrGlPrimG;      /* 0x105D17B4 */
extern float    BrGlPrimB;      /* 0x105CE2D0 */
extern float    BrGlPrimA;      /* 0x105CD9F0 */
extern uint8_t  BrGlFillR;      /* 0x105CCD40, 0xF7's unpacked fill bytes */
extern uint8_t  BrGlFillG;      /* 0x105CCFD8 */
extern uint8_t  BrGlFillB;      /* 0x105D17A0 */
extern uint8_t  BrGlFillA;      /* 0x105CE208 */
extern uint32_t BrGlRectMode;   /* 0x105CE2D4, 0x504340 = triangle path */

/* glide2.h GrVertex, 0x3C bytes.  Only x, y, r, g, b, a, oow are written. */
typedef struct {
    float x, y;        /* +0x00, +0x04 */
    float z;           /* +0x08 (unwritten) */
    float r, g, b;     /* +0x0C, +0x10, +0x14 */
    float ooz;         /* +0x18 (unwritten) */
    float a;           /* +0x1C */
    float oow;         /* +0x20, always 1.0f */
    float tmuvtx[6];   /* +0x24 (unwritten) */
} BrGrVertex;

/* 0x1001E380 -- the 914-byte rectangle emitter (opcodes 0xE1/0xF6 land
 * here).  Clamp the corners into the live window, pick the colour source
 * by the latched combine words, then either draw two triangles (mode
 * 0x504340) or clip+grBufferClear.  Colour bytes are four SEPARATE
 * uint8_t locals -- the dword-read + and 0xff widening in the bytes is
 * VC5's normal codegen for reading them back, not a source-level mask. */
/* @implements 0x1001E380 glide BrGlRectFill */
void BrGlRectFill(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    uint8_t bR, bG, bB, bA;
    BrGrVertex vB, vA, vC, vD;

    /* x1's clamp reads through a temp -- x1 itself stays memory-homed
     * (orig: cmp via scratch ecx, slot-only store, every later use
     * re-reads the slot). */
    {
        int32_t t1 = x1;
        if (t1 < BrGlClipMinX) x1 = BrGlClipMinX;
    }
    if (y1 < BrGlClipMinY) y1 = BrGlClipMinY;
    if (x2 > BrGlClipMaxX) x2 = BrGlClipMaxX;
    if (y2 > BrGlClipMaxY) y2 = BrGlClipMaxY;

    if (BrGlCombineW0 == 0xFCFFFFFFu && BrGlCombineW1 == 0xFFFDF6FBu) {
        bR = (uint8_t)(int32_t)BrGlPrimR;
        bG = (uint8_t)(int32_t)BrGlPrimG;
        bB = (uint8_t)(int32_t)BrGlPrimB;
        bA = (uint8_t)(int32_t)BrGlPrimA;
    } else {
        bR = BrGlFillR;
        bG = BrGlFillG;
        bB = BrGlFillB;
        bA = BrGlFillA;
    }

    if (BrGlRectMode == 0x504340) {
        /* Vertex-major in C, B, A, D order with the conversions written
         * inline -- CSE gives each converted value one integer-register
         * home, and /Op serializes each fild through the one scratch
         * slot. */
        vC.x = (float)x1;  vC.y = (float)y2;  vC.oow = 1.0f;
        vC.r = (float)bR;  vC.g = (float)bG;
        vC.b = (float)bB;  vC.a = (float)bA;
        vB.x = (float)x2;  vB.y = (float)y2;  vB.oow = 1.0f;
        vB.r = (float)bR;  vB.g = (float)bG;
        vB.b = (float)bB;  vB.a = (float)bA;
        vA.x = (float)x1;  vA.y = (float)y1;  vA.oow = 1.0f;
        vA.r = (float)bR;  vA.g = (float)bG;
        vA.b = (float)bB;  vA.a = (float)bA;
        vD.x = (float)x2;  vD.y = (float)y1;  vD.oow = 1.0f;
        vD.r = (float)bR;  vD.g = (float)bG;
        vD.b = (float)bB;  vD.a = (float)bA;

        grAlphaBlendFunction(1, 5, 4, 0);
        grAlphaTestFunction(7);
        grAlphaCombine(1, 0, 0, 2, 0);
        grColorCombine(1, 0, 0, 2, 0);
        grDepthBufferFunction(7);
        grDepthMask(0);
        grClipWindow(x1, y1, x2, y2);
        grCullMode(0);
        grDrawTriangle(&vA, &vB, &vC);
        grDrawTriangle(&vA, &vD, &vB);
        grClipWindow(BrGlClipMinX, BrGlClipMinY, BrGlClipMaxX, BrGlClipMaxY);
        grCullMode(0);
        grAlphaCombine(3, 8, 1, 1, 0);
        return;
    }

    grClipWindow(x1, y1, x2, y2);
    grBufferClear(((((uint32_t)bR << 8 | bG) << 8 | bB) << 8) | bA,
                  0, 0xFFFF);
    grClipWindow(BrGlClipMinX, BrGlClipMinY, BrGlClipMaxX, BrGlClipMaxY);
}

void __stdcall grAlphaTestReferenceValue(int);
void __stdcall grDepthBufferFunction(int);

/* Glide-side blend/test shadow state.  The four blend shadows mirror the
 * grAlphaBlendFunction argument order exactly. */
extern uint32_t BrGlAtestFuncShadow;   /* 0x105CCC70 */
extern uint32_t BrGlBlendLatch;        /* 0x105D17D4 */
extern uint32_t BrGlBlendSrcRGB;       /* 0x105D17A8 */
extern uint32_t BrGlBlendDstRGB;       /* 0x105D1758 */
extern uint32_t BrGlBlendSrcA;         /* 0x105CCFD4 */
extern uint32_t BrGlBlendDstA;         /* 0x105CCFE4 */
extern uint32_t BrGlDepthFuncShadow;   /* 0x105CE2E0 */

/* 0x10021270 -- the Glide render-mode dispatcher (BrGbiCall slot; the D3D
 * twin 0x10020FA0 in slice5_60.c is a dirty-bit state machine, DIFFERENT
 * CODE).  766 bytes, an if/else chain on the mode word driving the
 * grAlphaTest/AlphaCombine/AlphaBlend/DepthBuffer state directly. */
/* @implements 0x10021270 glide BrGlGbiCall */
void BrGlGbiCall(uint32_t w1)
{
    grDepthMask(1);

    if (w1 == 0x00504F50u) {
        grAlphaTestReferenceValue(8);
        BrGlAtestFuncShadow = 6;
        grAlphaTestFunction(6);
        if (BrGlBlendLatch == 0) {
            grAlphaCombine(3, 8, 1, 1, 0);
            BrGlBlendSrcRGB = 1;
            BrGlBlendDstRGB = 5;
            BrGlBlendSrcA   = 4;
            BrGlBlendDstA   = 0;
            grAlphaBlendFunction(1, 5, 4, 0);
        }
        BrGlDepthFuncShadow = 2;
        grDepthBufferFunction(2);
        return;
    }
    if (w1 == 4u) {
        grAlphaTestReferenceValue(8);
        BrGlAtestFuncShadow = 6;
        grAlphaTestFunction(6);
        if (BrGlBlendLatch == 0) {
            grAlphaCombine(3, 8, 1, 1, 0);
            BrGlBlendSrcRGB = 1;
            BrGlBlendDstRGB = 4;
            BrGlBlendSrcA   = 4;
            BrGlBlendDstA   = 0;
            grAlphaBlendFunction(1, 4, 4, 0);
        }
        BrGlDepthFuncShadow = 2;
        grDepthBufferFunction(2);
        return;
    }
    if (w1 == 0x0C184240u) {
        grAlphaTestReferenceValue(0x80);
        BrGlAtestFuncShadow = 6;
        grAlphaTestFunction(6);
        return;
    }
    if (w1 == 0x00504240u) {
        grAlphaTestReferenceValue(8);
        BrGlAtestFuncShadow = 6;
        grAlphaTestFunction(6);
        if (BrGlBlendLatch == 0) {
            grAlphaCombine(3, 8, 1, 1, 0);
            BrGlBlendSrcRGB = 1;
            BrGlBlendDstRGB = 5;
            BrGlBlendSrcA   = 4;
            BrGlBlendDstA   = 0;
            grAlphaBlendFunction(1, 5, 4, 0);
        }
        return;
    }
    if (w1 == 3u) {
        BrGlBlendLatch = 0;
        return;
    }
    if (w1 == 1u) {
        BrGlDepthFuncShadow = 3;
        grDepthBufferFunction(3);
        grDepthMask(0);
        return;
    }
    if (w1 == 0u) {
        BrGlDepthFuncShadow = 1;
        grDepthBufferFunction(1);
        return;
    }
    if (w1 == 2u) {
        grAlphaCombine(3, 4, 1, 2, 0);
        BrGlBlendSrcRGB = 1;
        BrGlBlendDstRGB = 4;
        BrGlBlendSrcA   = 4;
        BrGlBlendDstA   = 0;
        grAlphaBlendFunction(1, 4, 4, 0);
        grDepthMask(0);
        BrGlBlendLatch = 1;
        return;
    }
    if (w1 == 0x0D1849D8u) {
        grAlphaCombine(3, 4, 1, 2, 0);
        BrGlBlendSrcRGB = 1;
        BrGlBlendDstRGB = 5;
        BrGlBlendSrcA   = 4;
        BrGlBlendDstA   = 0;
        grAlphaBlendFunction(1, 5, 4, 0);
        grDepthMask(0);
        BrGlBlendLatch = 1;
        return;
    }
    if (w1 & 0x1800u) {
        if (w1 & 0x10000u) {
            grAlphaTestReferenceValue(0x80);
            BrGlAtestFuncShadow = 6;
            grAlphaTestFunction(6);
            return;
        }
        grAlphaTestReferenceValue(1);
        BrGlAtestFuncShadow = 6;
        grAlphaTestFunction(6);
        if (BrGlBlendLatch == 0) {
            grAlphaCombine(3, 8, 1, 1, 0);
            BrGlBlendSrcRGB = 1;
            BrGlBlendDstRGB = 5;
            BrGlBlendSrcA   = 4;
            BrGlBlendDstA   = 0;
            grAlphaBlendFunction(1, 5, 4, 0);
        }
        grDepthMask(0);
        return;
    }
    if (BrGlBlendLatch == 0) {
        grAlphaCombine(3, 8, 1, 1, 0);
        BrGlBlendSrcRGB = 4;
        BrGlBlendDstRGB = 0;
        BrGlBlendSrcA   = 4;
        BrGlBlendDstA   = 0;
        grAlphaBlendFunction(4, 0, 4, 0);
    }
    BrGlAtestFuncShadow = 7;
    grAlphaTestFunction(7);
}

void __stdcall grGlideInit(void);
int  __stdcall grSstQueryHardware(void *);
void __stdcall grSstSelect(int);

void BrGl_1001DD70(void);
void BrGl_1001DD80(int32_t w, int32_t h);

extern void   *BrGlFlipHook;        /* 0x106B7AB8 -> 0x1001DD50 */
extern void   *BrGlFlipHook2;       /* 0x106B7ABC -> 0x1001DD70 */
extern uint8_t BrGlHwConfig[1];     /* 0x105CCBD8, grSstQueryHardware out */
extern int32_t BrGlHwType;          /* 0x105CCBDC */
extern int32_t BrGlHwParamA;        /* 0x105CCBD0 */
extern int32_t BrGlHwParamB;        /* 0x105CCC6C */
extern int32_t BrGlHwCfgE0;         /* 0x105CCBE0 */
extern int32_t BrGlHwCfgE4;         /* 0x105CCBE4 */
extern int32_t BrGlHwCfgE8;         /* 0x105CCBE8 */
extern int32_t BrGlHwCfgEC;         /* 0x105CCBEC */
extern int32_t BrGlScreenW;         /* 0x100A7514 */
extern int32_t BrGlScreenH;         /* 0x100A7518 */

/* 0x1001E080 -- Glide bring-up: install the flip hooks, init Glide, probe
 * the SST, derive two hardware parameters by board type, then hand the
 * screen size to 0x1001DD80.  (Renderer slot; the D3D twin 0x1001BAE0 in
 * br_objlife.c is DIFFERENT CODE.)
 *
 * ONE residue region of 3 bytes, and it is the documented CROSS-JUMPING
 * class (see docs/VC5-IDIOMS.md): the function has no frame, so both exits
 * are a bare `ret`, and our cl merges them -- a 6-byte near `je` to the
 * tail -- where the original duplicated the ret and jumped over it with a
 * 2-byte `jne`.  All 40 other instructions are byte-identical.
 * Probed and dead: early-return vs nested-if vs else-return vs explicit
 * trailing return; and all five sweep variants (/O2 wins at 66 diffs, so
 * /Od, /Od /Op, /O2 /Op and /O2 /Oy- are worse).
 *
 * NOW TAGGED, 2026-09-03.  It was withheld, which left the function
 * invisible to triage while a FALSE TWIN claimed the same address:
 * br_objlife.c's BrInstall_1001BAE0 carried `@implements 0x1001BAE0 d3d`,
 * and config/shared.csv maps that to Glide 0x1001E080.  The D3D function
 * is 26 bytes of two pointer stores; this one is 173 bytes of 3dfx
 * bring-up.  Same renderer slot, different code.  That tag is now
 * @d3donly. */
/* @implements 0x1001E080 glide BrGlInstall */
void BrGlInstall(void)
{
    BrGlFlipHook  = (void *)BrGlideFlipWait;
    BrGlFlipHook2 = (void *)BrGl_1001DD70;
    grGlideInit();
    if (grSstQueryHardware(BrGlHwConfig) == 0)
        return;
    grSstSelect(0);
    switch (BrGlHwType) {
    default:
        BrGlHwParamA = 1;
        BrGlHwParamB = 2;
        break;
    case 1:
        BrGlHwParamA = BrGlHwCfgE4;
        BrGlHwParamB = BrGlHwCfgE0;
        break;
    case 0:
    case 3:
        BrGlHwParamA = BrGlHwCfgE8;
        BrGlHwParamB = BrGlHwCfgE0;
        if (BrGlHwCfgEC != 0)
            BrGlHwParamB = BrGlHwCfgE0 + BrGlHwCfgE0;
        break;
    }
    BrGl_1001DD80(BrGlScreenW, BrGlScreenH);
    return;
}

#endif /* BR_MATCHING_BUILD */
