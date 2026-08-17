/* br_dlglide.h -- RESPONSIBILITY: the display-list opcodes whose entire
 * effect is a change of GLIDE raster state.
 *
 * Seven of the twenty-eight slots in the dispatch table at 0x100A9A58 do no
 * geometry at all.  They bind or re-aim a texture, move the clip window, fill
 * a screen rectangle, or park a scalar for a later handler to read.  Every one
 * of them ends either in a `glide2x.dll` import or in a call through one of
 * the two runtime hook slots at 0x118ED1CC / 0x118ED1D0.  That is the boundary
 * this module owns; the geometry half of the table is br_dl.c's.
 *
 * Slot assignments read out of orig/BRGlide.dll at 0x100A9A58, not inferred:
 *
 *     0xDC -> 0x1001E2E0    30 B    bind texture
 *     0xDD -> 0x1001E300    32 B    re-aim that texture at a new address
 *     0xDF -> 0x1001EB30    17 B    park w1 in 0x105D17C4
 *     0xE1 -> 0x1001E720    73 B    fill rect, INTEGER corners
 *     0xE2 -> 0x1001EBC0    97 B    set scissor, INTEGER corners
 *     0xED -> 0x1001EB50   103 B    set scissor, 10.2 corners
 *     0xF2 -> 0x1001EC30   178 B    set tile size  (DELEGATED -- see below)
 *
 * THE CALLING CONVENTION, and it is the property most worth testing
 * ----------------------------------------------------------------------
 * Every handler takes ONE argument -- a pointer to the 8-byte command, w0 at
 * +0 and w1 at +4 -- and returns the address of the NEXT command.  Six of the
 * seven return `arg + 8`; 0xDC returns `arg + 8*w1`, because a single 0xDC is
 * written over a whole texture-setup run at load time (br_dl.h documents that
 * chain).  Getting a return wrong desynchronises everything after it in the
 * list, so test_br_dlglide.c asserts the return of all seven explicitly.
 *
 * WHY THIS MODULE EXISTS ALONGSIDE br_dl.c, WHICH ALSO HANDLES THESE OPCODES
 * ----------------------------------------------------------------------
 * br_dl.c already carries a transcription of all twenty-eight opcodes.  Four
 * of these seven agreed with the original.  THREE DID NOT.  ALL THREE HAVE
 * SINCE BEEN FIXED IN br_dl.c, re-derived from the same bytes rather than
 * copied from here, each with a discriminating test and a reinstate-the-bug
 * mutation (test_br_dl.c's MUTATIONS KILLED table).  The three are kept below
 * because they are the derivation, not because they are still open:
 *
 *   1. 0xE2 and 0xED are DIFFERENT DECODES of the same command, and br_dl.c
 *      routes both to one function using the 0xED reading.  0x1001EB50 takes
 *      `(w >> 14) & 0x3FF` and `(w >> 2) & 0x3FF` -- 10.2 fixed point with the
 *      fraction dropped.  0x1001EBC0 takes `(w >> 12) & 0xFFF` and
 *      `w & 0xFFF` -- plain 12-bit integers.  So under br_dl.c an 0xE2 command
 *      was decoded with the wrong shifts and the wrong masks; it now has one
 *      handler per slot, br_dl_scissorE2 and br_dl_scissorED.  This is the
 *      exact relationship CONVENTIONS.md already records for 0xE1 against
 *      0xF6, and it is confirmed independently in BRD3D.dll, whose 0xE2
 *      (0x1001CE70) and 0xED (0x1001CDA0) split the same way at the same two
 *      shifts.
 *
 *   2. NEITHER scissor handler's Y is stored as it arrives.  Both compute
 *      `screenHeight - uly` and `screenHeight - lry` (0x100A7518 is the
 *      grSstWinOpen height) before storing, because Glide's window origin is
 *      at the BOTTOM.  br_dl.c stored the raw fields, so its scisULY/scisLRY
 *      were in the opposite vertical direction from the original's globals and
 *      arrived in the opposite order -- what the original calls the MINIMUM Y
 *      comes out of the LR field, not the UL one.  It now stores
 *      scisMinX/scisMinY/scisMaxX/scisMaxY with the flip applied.
 *
 *   3. 0xE1's four fields are SIGN-EXTENDED 12-bit integers (`shl 20 / sar 20`
 *      with no mask), and w0 carries the LOWER-RIGHT corner while w1 carries
 *      the UPPER-LEFT -- stock G_FILLRECT packing, and the opposite way round
 *      from 0xE2/0xED.  br_dl.c masked with 0xFFF (so a negative corner became
 *      a large positive one) and named w0's fields `ul`.  It also dropped the
 *      `+1` on lrx and the `-1` on the flipped lry that 0x1001E720 applies
 *      before calling 0x1001E380.  All three are fixed; the window is recorded
 *      as BrDl.rectMinX..rectMaxY, and the same three defects were present on
 *      0xF6 because the two share one arm there.
 *
 * br_dl.c was deliberately not edited when this module was written: it was
 * under concurrent work, and CONVENTIONS.md's rule is to declare and wire
 * rather than to reach into someone else's module.  It is no longer under
 * concurrent work and the three were fixed there.
 *
 * NO STORAGE IS ALIASED, AND THE DUPLICATION IS STILL OPEN.  br_dl.c's
 * `BrDl.scisMinX..scisMaxY` (renamed from scisULX..scisLRY when the flip was
 * put in) and this module's four clip-window fields model the same four
 * original globals, and they are NOT the same host object -- see
 * CONVENTIONS.md, "Aliased storage: a link-clean bug".  The two readings now
 * AGREE, which removes the harm; it does not remove the duplicate.  Nothing
 * in the tree writes one and reads the other, and nothing should start: the
 * end state is that one of the two definitions goes, not that both are kept
 * in step.  The same holds for 0xE1, 0xDC, 0xDD and 0xDF.
 *
 * WHAT IS NOT TRANSCRIBED, and it is declared rather than faked
 * ----------------------------------------------------------------------
 *   0x1001E380  914 bytes, the rectangle emitter both 0xE1 and 0xF6 call.  It
 *               clamps its four arguments against the four clip-window globals
 *               and then builds two Glide triangles.  Only its CLAMP is
 *               understood here (it is what pins the meaning of the four
 *               globals, see BrDlGl below); the emission is not transcribed.
 *               0xE1 therefore reaches this module's `pfnFillRect` hook with
 *               the arguments 0x1001E720 computes, UNCLAMPED, and a consumer
 *               that wants the clamp has the four fields to do it with.
 *   0x100284E0  what 0x118ED1CC holds at run time -- issues grTexSource and
 *               friends for texture `id`.
 *   0x100285E0  what 0x118ED1D0 holds -- re-downloads that texture from a new
 *               address.  Also called from 0x100287E0, 0x10028BB0 and
 *               0x1006E220, so it is not private to 0xDD.
 *   0x1001EC30  is transcribed, in br_dl.c, as `br_dl_settilesize`.  0xF2 is
 *               therefore DELEGATED here -- see BrDlGlSetTileSize.
 *
 * THE HOOK SLOTS ARE NOT IMPORTS
 * ----------------------------------------------------------------------
 * 0x118ED1CC and 0x118ED1D0 are .data words, filled at run time by 0x10029B50
 * with 0x100284E0 and 0x100285E0 (read off 0x10029B7A and 0x10029B84).  Before
 * that installer runs they are zero and the original would call through NULL.
 * This port does not: a NULL hook is DECLINED and counted in `cNullHook`, so a
 * run reports that it reached an uninstalled hook instead of pretending the
 * bind happened.  That is a frontier, not a placeholder -- nothing downstream
 * behaves as though the texture were bound.
 */
#ifndef BR_DLGLIDE_H
#define BR_DLGLIDE_H

#include <stdint.h>

/* ---------------------------------------------------------------------
 * The two runtime hook slots, and the two Glide entry points
 * ---------------------------------------------------------------------
 * Argument orders are read off the push sequences, cdecl, so the LAST push is
 * the first argument.  They are not guesses:
 *
 *   0x1001E2EC  push eax                     eax = w0 & 0xFFFFFF
 *   0x1001E2ED  call [0x118ED1CC]            -> one argument
 *
 *   0x1001E310  push eax                     eax = w1, WHOLE, unmasked
 *   0x1001E311  push ecx                     ecx = w0 & 0xFFFFFF
 *   0x1001E312  call [0x118ED1D0]            -> (id, addr)
 */
typedef struct BrDlGlHooks {
    void *pUser;

    /* [0x118ED1CC], installed with 0x100284E0 by 0x10029B50.  Selects the
     * texture named by the low 24 bits of the 0xDC command's w0. */
    void (*pfnTexSelect)(void *pUser, uint32_t id);

    /* [0x118ED1D0], installed with 0x100285E0.  Re-downloads texture `id` from
     * `addr` -- the one-texture scheme the Glide font emitter uses. */
    void (*pfnTexRetarget)(void *pUser, uint32_t id, uint32_t addr);

    /* glide2x.dll!grClipWindow, reached through the thunk 0x100729D2.  Both
     * scissor handlers call it as (minx, miny, maxx, maxy) -- see the note on
     * the flip in BrDlGl. */
    void (*pfnClipWindow)(void *pUser, int32_t minx, int32_t miny,
                          int32_t maxx, int32_t maxy);

    /* 0x1001E380, the untranscribed rectangle emitter.  0xE1 calls it with
     * (ulx, H - lry - 1, lrx + 1, H - uly). */
    void (*pfnFillRect)(void *pUser, int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1);
} BrDlGlHooks;

/* ---------------------------------------------------------------------
 * State
 * ---------------------------------------------------------------------
 * The four clip-window globals, and WHICH IS WHICH is read off the consumer
 * rather than off the names.  0x1001E380 opens with four compares:
 *
 *   0x1001E38F  cmp arg0, [0x105D17BC] ; jge   -> arg0 = max(arg0, that)
 *   0x1001E3AA  cmp arg1, [0x105D17C0] ; jge   -> arg1 = max(arg1, that)
 *   0x1001E3C3  cmp arg2, [0x105D17B8] ; jle   -> arg2 = min(arg2, that)
 *   0x1001E3DC  cmp arg3, [0x105CCFE0] ; jle   -> arg3 = min(arg3, that)
 *
 * so the first two are the window's MINIMUM corner and the last two its
 * MAXIMUM, and the scissor handlers fill them as
 *
 *   0x105D17BC = ulx          (from w0's high field)
 *   0x105CCFE0 = H - uly      (from w0's low field)   <- the MAXIMUM y
 *   0x105D17B8 = lrx          (from w1's high field)
 *   0x105D17C0 = H - lry      (from w1's low field)   <- the MINIMUM y
 *
 * The Y flip is what makes that consistent: an F3D scissor has uly above lry
 * in a top-down screen, and Glide's origin is at the bottom, so subtracting
 * both from the window height swaps which one is the minimum.  Storing the
 * fields unflipped -- which is what br_dl.c does -- leaves minY above maxY,
 * i.e. an inverted window, and test_br_dlglide.c pins exactly that. */
typedef struct BrDlGl {
    BrDlGlHooks hook;

    /* 0x100A7518 -- the grSstWinOpen height every Y is flipped against.
     * (0x100A7514 is the width; nothing in this module reads it.) */
    int32_t  cyScreen;

    int32_t  clipMinX;      /* 0x105D17BC */
    int32_t  clipMinY;      /* 0x105D17C0 */
    int32_t  clipMaxX;      /* 0x105D17B8 */
    int32_t  clipMaxY;      /* 0x105CCFE0 */

    /* 0x105D17C4 -- 0xDF's payload.  0x1001EB30 stores the raw dword; its one
     * reader, 0x1002171C inside the texture-rect helper 0x100215C0, does
     * `fld [0x105D17C4] / fdiv [0x100A9A54]`, i.e. reads it as a FLOAT.  The
     * store is kept as bits and the float reading offered separately, because
     * the handler genuinely does not interpret it. */
    uint32_t w5D17C4;

    /* Last values seen, so a test can assert what the hooks were handed even
     * when no hook is installed.  NOT in the original. */
    uint32_t hTexture;      /* 0xDC: w0 & 0xFFFFFF   */
    uint32_t hRetarget;     /* 0xDD: w0 & 0xFFFFFF   */
    uint32_t addrRetarget;  /* 0xDD: w1, unmasked    */

    /* Counters -- not in the original; the port's only way to assert, and the
     * way an uninstalled hook stays visible instead of silent. */
    uint32_t cBind, cRetarget, cSet5D17C4;
    uint32_t cFillRect, cScissor;
    uint32_t cF2Delegated;  /* 0xF2 commands this module declined to decode */
    uint32_t cNullHook;     /* calls declined because the slot was NULL     */
} BrDlGl;

/* Zero the state and set the flip height.  There is no single original for
 * this: 0x100A7518 is written by the grSstWinOpen wrapper and the clip-window
 * globals by 0x1001E1E0 / 0x1001E200, neither of which is in this module's
 * scope.  The window starts at (0,0)..(cxScreen is not read, cyScreen). */
void BrDlGlInit(BrDlGl *pGl, int32_t cyScreen);

/* 0x105D17C4 as its reader sees it. */
float BrDlGlGet5D17C4(const BrDlGl *pGl);

/* ---------------------------------------------------------------------
 * The handlers.  One argument, returns the next command.
 * --------------------------------------------------------------------- */
typedef const uint8_t *(*BrDlGlHandler)(BrDlGl *pGl, const uint8_t *p);

/* 0xDC -- 0x1001E2E0 (Glide, 30 B; D3D 0x1001BD70, 149 B, NOT the same code).
 * Calls [0x118ED1CC] with the low 24 bits of w0, then returns `p + 8*w1`.
 * The multiply is `lea eax,[esi+ecx*8]`, so w1 == 0 returns p UNCHANGED and
 * the original spins forever on such a command; that is preserved. */
const uint8_t *BrDlGlBindTexture(BrDlGl *pGl, const uint8_t *p);  /* 0x1001E2E0 */

/* 0xDD -- 0x1001E300 (shared with D3D 0x1001BE10).  Calls [0x118ED1D0] with
 * (w0 & 0xFFFFFF, w1) -- note w1 is passed whole -- and returns p + 8. */
const uint8_t *BrDlGlRetarget(BrDlGl *pGl, const uint8_t *p);  /* 0x1001E300 */

/* 0xDF -- 0x1001EB30 (Glide-only; the D3D twin 0x1001CD80 is slice2_16.c's
 * BrGbiSet4C5174, whose global is 0x104C5174).  Stores w1 in 0x105D17C4 and
 * returns p + 8.  Named for the address it writes rather than for a guessed
 * meaning, following that twin. */
const uint8_t *BrDlGlSet5D17C4(BrDlGl *pGl, const uint8_t *p);  /* 0x1001EB30 */

/* 0xE1 -- 0x1001E720 (shared with D3D 0x1001C7A0).  FILL RECTANGLE with plain
 * SIGNED 12-bit integer corners, w0 carrying the lower-right and w1 the
 * upper-left.  Calls 0x1001E380 with
 *
 *     (ulx, H - lry - 1, lrx + 1, H - uly)
 *
 * and returns p + 8.  The only difference from the 0xF6 handler 0x1001E320 is
 * `sar 0x14` where that one has `sar 0x16` plus an `and 0x3FF` -- i.e. 0xF6
 * divides by four and masks the sign away, and 0xE1 does neither. */
const uint8_t *BrDlGlFillRect(BrDlGl *pGl, const uint8_t *p);  /* 0x1001E720 */

/* 0xE2 -- 0x1001EBC0 (Glide-only).  SET SCISSOR with plain 12-bit INTEGER
 * corners: `(w >> 12) & 0xFFF` and `w & 0xFFF`, unsigned, no sign fold.
 * w0 is the upper-left and w1 the lower-right.  Returns p + 8. */
const uint8_t *BrDlGlScissorInt(BrDlGl *pGl, const uint8_t *p);  /* 0x1001EBC0 */

/* 0xED -- 0x1001EB50 (Glide-only).  The SAME command in 10.2: the integer part
 * is `(w >> 14) & 0x3FF` and `(w >> 2) & 0x3FF`.  Returns p + 8. */
const uint8_t *BrDlGlScissorFrac(BrDlGl *pGl, const uint8_t *p);  /* 0x1001EB50 */

/* 0xF2 -- 0x1001EC30.  A FRONTIER, DELIBERATELY: this opcode is already
 * transcribed, correctly, as br_dl.c's static `br_dl_settilesize`, and again
 * under its D3D address 0x1001CF30 as slice2_16.c's BrGbiSetTileSize -- which
 * this file used to call BrGbiSetScissor and flag as misnamed; it has since
 * been renamed there, with the dispatch-table evidence at the site.  This
 * entry decodes NOTHING.  It advances the cursor by eight, counts itself in
 * cF2Delegated, and exists so that a caller installing this module's table
 * over slots 0xDC..0xF2 cannot silently lose the opcode.  Route 0xF2 to
 * br_dl.c's dispatcher -- BrDlRun -- for the real decode. */
const uint8_t *BrDlGlSetTileSize(BrDlGl *pGl, const uint8_t *p);
/* ...and note that line carries NO trailing address annotation, unlike the six
 * above it.  That is deliberate: an annotated address is one of the three ways
 * tools/isported.py decides a function IS the port of that address, and this
 * one is a counted frontier.  Annotating it would report 0x1001EC30 as ported
 * by a function that decodes nothing -- a false PORTED, which CONVENTIONS.md
 * names as the dangerous direction. */

/* The handler the original's table at 0x100A9A58 holds for `op`, or NULL for
 * every opcode this module does not own (which includes the 228 that fall to
 * the skip stub at 0x10021240 and the 21 that are br_dl.c's). */
BrDlGlHandler BrDlGlDispatch(unsigned op);

#endif /* BR_DLGLIDE_H */
