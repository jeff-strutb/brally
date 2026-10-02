/* br_dl.c -- the display-list machine.  See br_dl.h for what this is and how
 * it was established.  Every address literal is from orig/BRGlide.dll, which
 * CONVENTIONS.md names as the reference; where the D3D build's handler is a
 * different function the divergence is called out at the site.
 *
 * WHAT IS TRANSCRIBED AND WHAT IS NOT
 * ----------------------------------------------------------------------
 * Transcribed from the disassembly, opcode by opcode:
 *   the loop (0x10023C90), the dispatch table (0x100A9A58), the load-time
 *   patch pass (0x10019040), and the handlers for
 *   0x01 0x03 0x04 0x06 0xB1 0xB6 0xB7 0xB8 0xB9 0xBC 0xBD 0xBF
 *   0xDC 0xDD 0xDE 0xDF 0xE1 0xE2 0xE3 0xE4 0xED 0xF2 0xF6 0xF7 0xF8
 *   0xFA 0xFB 0xFC.
 *
 * CLIPPING has since been read.  PART 2 below is the DRIVER 0x1001EE70 only;
 * the seven per-plane routines, the interpolator 0x1001F200 and the 64-node
 * pool are slice1_03.c's, under their D3D addresses -- grepping the Glide
 * ones finds nothing.  See the section header for the mapping.
 *
 * LIGHTING has since been read; it is PART 4 below, and the note that used to
 * stand here -- "a lighting pass exists and has not been found" -- is
 * withdrawn.  It was not found because it is not a pass: it is a second
 * G_VTX HANDLER, installed over the first.
 *
 * NOT transcribed, and flagged as DEVIATION where it shows:
 *   - TEXTURE_GEN's s/t generation.  0x10022600 and 0x10022BF0 are the two
 *     lit transforms the geometry mode selects when G_TEXTURE_GEN is set;
 *     they do the same lighting as 0x10021C70 (they call the same
 *     0x10022AC0) and additionally derive s/t from the normal.  The lighting
 *     is here; the s/t derivation is not, and pDl->cVtxTexGen counts the
 *     vertices that would have had it so the gap is visible rather than
 *     silent.  Nothing in this port sets G_TEXTURE_GEN today.
 *   - 0x1001E380, the 914-byte rectangle emitter both untextured fill-rect
 *     opcodes call.  Its four opening CLAMPS are read (they are what pin
 *     which scissor global is min and which is max) and its emission is not.
 *     The four arguments 0xE1 and 0xF6 compute for it are recorded in
 *     pDl->rectMinX..rectMaxY; the clamp and the two Glide triangles are not
 *     performed.
 *
 * FOURTEEN OPCODES ARE TRANSCRIBED TWICE IN THIS TREE, stated here rather
 * than left to be found.  br_dlglide.c independently ports 0xDC 0xDD 0xDF
 * 0xE1 0xE2 0xED and br_dlcmd.c independently ports 0x04 0xB1 0xBF 0xF6 0xF7
 * 0xFA 0xFB 0xFC, all under the same original addresses this file uses.  Two
 * host definitions of one original address is the hazard CONVENTIONS.md's
 * "Aliased storage" section names.  They now AGREE -- the seven places where
 * they did not are the subject of the opcode audit this pass acted on -- but
 * agreeing is not the same as being one object, and the end state is that one
 * of each pair goes.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_16.h"   /* br_globals: its objects */
#include "br_dl.h"

/* The clip planes, the interpolator and the node pool are slice1_03's --
 * see PART 2 on why this file must not own a second copy of them. */
#include "slice1_03.h"

/* The routines this file and slice2_16.c BOTH used to transcribe.  Same
 * original function, one host body -- see br_dlshared.h. */
#include "br_dlshared.h"

#include <string.h>

/* slice1_05.c owns 0x100306C0 (D3D) == 0x10029D70 (Glide), which shared.csv
 * pairs as one shared function.  Declared by prototype rather than by
 * including slice1_05.h, the way br_font.c declares BrRdpSetCombineLERP, so
 * this file does not drag in a dozen unrelated models.  Do NOT re-implement
 * it here: one original address must have one host definition. */
/* BrMat4Mul: prototype in br_funcs.h */

#include "br_vec.h"      /* BrVec3 -- see br_dl_normalise below */

#include <math.h>

/* ==================================================================== */
/* helpers                                                              */
/* ==================================================================== */

/* The list is in host order by the time the interpreter sees it, so a command
 * is two host u32s.  Read them byte-wise anyway: CONVENTIONS.md forbids
 * overlaying a struct on a foreign buffer, and a display list is exactly
 * that. */
/* (port-only br_dl_w removed) */


/* (port-only br_dl_be32 removed) */


/* (port-only br_dl_putw removed) */


/* 0x1001EC30's sign fold: a 12-bit field above 0x800 is negative.  0x1001E720
 * does the same thing with `shl 20 / sar 20` on all four of its corner fields;
 * written as the explicit fold because C99 leaves a signed right shift
 * implementation-defined. */
/* NOT 0x1001EC30 -- an annotation pass attached that address here because
 * the comment above opens with it.  0x1001EC30 is the 178-byte 0xF2
 * handler, which is br_dl_settilesize below; this is a six-line helper it
 * and 0x1001E720 both use. */
/* (port-only br_dl_s12 removed) */


/* `fistp dword ptr [0x105CE310]` under MSVC's startup control word: round to
 * NEAREST, TIES TO EVEN.  Out of range -- and NaN -- stores the x87 integer
 * indefinite 0x80000000.
 *
 * This is NOT __ftol (0x1007C8A0 / MSVCRT `_ftol` at 0x10074560), which
 * truncates toward zero and yields 0 out of range.  Both appear in this
 * binary, a few hundred bytes apart, and the quarter-pixel snap uses the
 * FIRST: 0x100220D4 is a bare `fistp`, with no control-word change anywhere
 * in 0x10022070 or its callers.  The port used to add +/-0.5 and truncate,
 * which is ties-AWAY-from-zero and differs from the original on every exact
 * half -- e.g. 0.125 * 4 == 0.5 snaps to 0 here and to 1 that way.
 *
 * rint() honours the current rounding mode, which is the same ties-to-even.
 * br_dlcmd.c's br_dlcmd_fistp is the identical helper for the same `fistp`;
 * the two files are separate translation units transcribing separate original
 * functions, so this is a duplicated LEAF, not a duplicated address. */
/* (port-only br_dl_fistp removed) */


/* Read a float out of a host-order 32-bit pattern without aliasing. */
/* (port-only br_dl_f32 removed) */


/* 0x10023B10's free-list threading; defined with the rest of the clipper. */
void br_dl_clip_reset(void);

/* ==================================================================== */
/* addressing -- see the header on why this is a table and not a cast    */
/* ==================================================================== */

/* (port-only BrDlAddRegion removed) */


/* (port-only BrDlResolve removed) */


/* ==================================================================== */
/* state                                                                */
/* ==================================================================== */

/* (port-only BrDlInit removed) */


/* @n64 0x802244A8 located */
/* (port-only BrDlSetViewport removed) */


/* ==================================================================== */
/* handlers                                                             */
/* ==================================================================== */

typedef const uint8_t *(*BrDlHandler)(BrDl *, const uint8_t *);

/* 0x10021240 -- 228 of the 256 table slots point here.  BRD3D's copy is
 * 0x100243D0 and slice5_60.c ports it as BrGbiCall100243D0; the whole
 * function is `add eax,8`, so the shared part is the step, and it is named
 * once in br_dlshared.h rather than written as a bare 8 in both files. The
 * counter is this port's, not the original's. */
/* WHAT IT DOES: the do-nothing handler that most of the drawing-command
 * table points at: it counts the command as unhandled and steps over it. The
 * N64 command set is far larger than the game actually uses, so the great
 * majority of the 256 slots land here. */
/* @implements 0x10021240 glide br_dl_skip */
const uint8_t *br_dl_skip(const uint8_t *p)
{
    return p + 8;
}

/* Table-compatible do-nothing handler for the PORT dispatch, which calls every
 * slot as (pDl, p). The byte-exact original (br_dl_skip above) takes one arg;
 * this two-arg wrapper does the same step (p + 8) so the 256-slot table is
 * type-uniform. Called as s_aTable[op](pDl, p): br_dl_skip's 1-arg form would
 * mis-read pDl as p, so the table must hold this version. */
/* (port-only br_dl_skip_h removed) */


/* ---- 0x01 G_MTX  (0x10021080, SHARED) ------------------------------- */
/* (port-only br_dl_mtx removed) */


/* ---- 0x03 G_MOVEMEM  (0x10023810, SHARED) --------------------------- */
/* (port-only br_dl_movemem removed) */


/* ==================================================================== */
/* PART 4 -- LIGHTING                                                   */
/* ==================================================================== */
/* WHERE IT IS, AND WHY IT WAS NOT FOUND BY LOOKING FOR A PASS
 * ----------------------------------------------------------------------
 * There is no lighting pass.  There is a second G_VTX HANDLER, and the
 * geometry mode installs it over the first:
 *
 *   0x1001FD40 (0xB6 clear) and 0x100211E0 (0xB7 set) both end in
 *   `call 0x1001FD70`, and 0x1001FD70 does nothing but push renderer state
 *   and then WRITE THE DISPATCH TABLE:
 *
 *       0x100A9A68 == 0x100A9A58 + 4 * 4   -- slot 0x04, G_VTX
 *       0x100A9D1C == 0x100A9A58 + 0xB1*4  -- slot 0xB1, G_TRI2
 *       0x100A9D54 == 0x100A9A58 + 0xBF*4  -- slot 0xBF, G_TRI1
 *
 *   Those three addresses were the whole mystery.  0x100A9A68 had been read
 *   as a free-standing "triangle routine pointer" (br_dl.h says so, in the
 *   BR_DL_CC_DECAL note); it is a table slot, and the routines it holds are
 *   vertex transforms.  The static image of the table has 0x10021A20 in it,
 *   which is why the unlit transform is the one everybody found.
 *
 *   The selection, transcribed from 0x1001FE0D:
 *
 *     ZBUFFER     LIGHTING  TEXGEN  TEXGEN_LIN    slot 0x04
 *     -------     --------  ------  ----------    ---------
 *        1           0         0        -         0x10021A20   unlit
 *        1           1         0        -         0x10021C70   lit
 *        1           1         0        -         0x100221D0   lit, DECAL
 *        1           -         1        0         0x10022600   lit + texgen
 *        1           -         1        1         0x10022BF0   lit + texgen
 *        0           0         -        -         0x10023110   unlit
 *        0           1         -        -         0x10023360   lit
 *
 *   and SHADING_SMOOTH picks the triangle pair (0x1001ECF0/0x1001FA30 vs
 *   0x1001FEF0/0x10020CF0 with a z-buffer, 0x10020900/0x10020D70 vs
 *   0x100203F0/0x10020D30 without).  0x1001E8FB additionally swaps 0x10021C70
 *   and 0x100221D0 when the DECAL combiner row is selected -- and ONLY those
 *   two, so it can never install a lit routine over an unlit one.
 *
 *   0x10021C70, 0x100221D0 and 0x10023360 are the same 1019/1059/1019 bytes
 *   with different x87 scheduling: identical constant use (three 0x10077418,
 *   three 0x10077420, the same nine 0x105CE2xx slots), so ONE transcription
 *   covers all three.  0x10022600 and 0x10022BF0 call the lighting out of
 *   line at 0x10022AC0 instead of inlining it, and add texgen.
 *
 * THE ARITHMETIC IS ALSO ALREADY HALF-PORTED, UNDER D3D ADDRESSES.
 *   BRD3D 0x10022350 is slice2_16.c's BrGbiLightVertex and is byte-for-byte
 *   the same 301-byte routine as Glide 0x10022AC0, down to the "lights off"
 *   fallback reading three NON-CONTIGUOUS globals.  Its BrGbiLightState maps
 *   one-for-one onto the three float triples below.  The two differ in
 *   exactly one thing: D3D clamps at 1.0f (0x1008F3C4) and divides by 255 in
 *   its setup, Glide clamps at 255.0f (0x10077418) and does not divide,
 *   because a Glide iterated colour is 0..255.  Glide is the reference, so
 *   this file carries 0..255 and BrDlColourScale() says so out loud.
 *
 * WHAT THE INPUT ACTUALLY IS.  Measured, not assumed: over every vertex the
 * two retail models reach -- 864 in bb.rca, 931 in ce.rca -- the magnitude of
 * the Vtx's trailing three signed bytes is 127.0 +/- 0.6, every single one.
 * They are unit normals.  The colours this port has been rendering are
 * normals painted as colour; the file does NOT hold lit colours. */

/* --- 0x100344D0, and a deliberate second copy -------------------------
 * The normalise 0x10021DB8 calls is BRD3D 0x1003AE50 -- config/shared.csv
 * pairs the two as ONE shared 141-byte function -- and slice2_21.c ALREADY
 * PORTS IT, as BrVec3NormaliseGuard (not BrVec3Normalise: slice1_09 owns that
 * name for the unguarded 0x10074180).  CONVENTIONS.md says to reuse, and the
 * first attempt here did: `extern void BrVec3NormaliseGuard(BrVec3 *)`.
 *
 * It does not link.  slice2_21.c needs BrSqrtF, whose only definition is in
 * slice4_53.c, which needs roughly the whole game; adding it to
 * build.d/test_br_dl.deps turns a five-object test into an unlinkable one.
 * So this is a SECOND HOST COPY of one original function, on purpose, and the
 * mitigation is that it is stated here rather than discovered later.  It is
 * arithmetically identical, including the two things that are easy to get
 * wrong and that br_dl_light_setup depends on:
 *
 *   - the sum order is y*y + z*z + x*x (the original spills y and z and
 *     squares them from the stack first);
 *   - a length of EXACTLY zero yields (0, 0, 1), not a zero vector and not
 *     the input.  0x1003450D compares against 0x100775F4 == 0.0f and tests
 *     C3, which an unordered compare also sets, so a NaN length takes the
 *     same arm.  `!(len != 0.0f)` for that reason.
 *
 * br_dl.c already carries one other such copy for the same kind of reason:
 * br_dl_outcode is 0x10022120, which slice2_16.c ports as BrGbiClipCodes.
 * Those are the only two, and both are pure leaf arithmetic with no state. */
/* WHAT IT DOES: scales a direction so it is exactly one unit long, with one
 * important safety valve: a direction of zero length -- or one that is not a
 * number -- comes out pointing straight along the third axis rather than
 * staying zero. The lighting setup relies on that. */
/* @implements 0x100344D0 glide br_dl_normalise */
/* BrSqrtF: prototype in br_funcs.h */
void br_dl_normalise(BrVec3 *pV)
{
    /* Same locals as BrVec3Normalise: x on x87, y/z integer-homed.
     * Scale-out: pV->x * k is a memory operand; y/z copy-assign k.
     * Zero arm is the else so scale is fall-through (orig jne-to-zeros).
     *
     * WAS 49 bytes, 4 over / 2 instructions over, RAW 3+1.  The front
     * half and the zero arm are exact; the whole gap is ONE EXTRA x87 STACK
     * SLOT in the scale-out.  After the `fdivr` the original makes three
     * copies of the reciprocal and multiplies each by a MEMORY operand
     * (`fld st(0); fld st(1); fxch st(1); fmul [esi]; ... fmul [esi+4]; ...
     * fmul [esi+8]`); we load pV->x into st first (`fld [esi]`) and do that
     * one product register-to-register, which leaves a fourth value to
     * `fstp st(0)` at the end.  Only the X term differs -- y and z already
     * use the memory fmul.
     * Probed and DEAD, all three byte-identical to the old spelling: `pV->x
     * *= len` compound assignment, `pV->x * len` operand order, and keeping
     * the dead `x` local alive past the branch.  COMPILE VARIANT CHECKED,
     * this one is genuinely /O2: /O2 49 diffs, /O2 /Op 128, /O2 /Oy- 117,
     * /Od 122.
     *
     * CLOSED 2026-09-04 -- it was the `ptr[0]` ranking (docs/VC5-IDIOMS.md):
     * `pV->x` is an offset-0 operand and VC5 ranks that above the register
     * copy of the reciprocal, so it `fld`s the field and multiplies by
     * st(i); y and z at +4/+8 lose that ranking and become memory operands
     * like the original's.  The scale-out reads the vector through a
     * pointer displaced PAST THE END (`q = (float *)pV + 3`, indices -3..
     * -1) so no term is index 0; VC5 folds the displacement back and the
     * bytes are identical.  It must stay MULTI-USE: displacing only the x
     * read is 4 B short (0+2), `+1`/`+2` (which leave y or z at index 0)
     * are 46/52 diffs.  Declared up here or assigned after the divide is
     * the same bytes. */
    float x = pV->x;
    float y = pV->y;
    float z = pV->z;
    float len;
    float *q = (float *)pV + 3;
    len = BrSqrtF(y * y + z * z + x * x);
    if (len != 0.0f) {
        len = 1.0f / len;
        q[-3] = len * q[-3];
        q[-2] = len * q[-2];
        q[-1] = len * q[-1];
    } else {
        pV->x = 0.0f;
        pV->y = 0.0f;
        pV->z = 1.0f;
    }
}

/* --- 0x10021C70's prologue (0x10021C70..0x10021E0F) -------------------
 * Rebuild the derived light state.  Guarded by 0x105D17D0, which G_MTX
 * (modelview only), G_MOVEMEM light and G_MOVEWORD LIGHTCOL all clear. */
/* Port helper: works out the light the next batch of vertices will be
 * shaded by (colour, direction pulled into model space, ambient) and caches
 * it.  In the original this is the prologue of 0x10021C70's single body,
 * which is transcribed whole in br_dlvtx_lit.c (BrDlVtxLit). */
/* (port-only br_dl_light_setup removed) */


/* --- 0x10022AC0 (== BRD3D 0x10022350 == slice2_16's BrGbiLightVertex) --
 * One vertex.  `pN` is the normal (source floats +0x14/+0x18/+0x1C), `pOut`
 * the three colour floats the caller writes to the vertex's +0x5C/+0x60/+0x64
 * -- the clip node's +0x1C/+0x20/+0x24, which is why the clipper carries the
 * LIT colour and not the normal across a cut edge. */
/* WHAT IT DOES: shades one vertex. With no lights it just copies the current
 * primitive colour. Otherwise it measures how squarely the surface faces the
 * light; surfaces facing away get plain ambient, and the rest get ambient
 * plus a share of the light's colour, capped at full brightness. Colours
 * here run 0 to 255, not 0 to 1. */
/* @implements 0x10022AC0 glide br_dl_light_vertex */
/* The original takes TWO arguments -- the source vertex record (normal at
 * +0x14/+0x18/+0x1C, already float) and the output vertex (colour at
 * +0x1C/+0x20/+0x24) -- and reads every light value from absolute globals;
 * the port's BrDl pointer is a port invention.  Graded /O2 /Op: t and each
 * v round through memory.  Layout facts: the two early arms are ELSE arms
 * (lights off, then facing away), so both land after the lit path; they
 * copy as dwords; the clamp is a float ?: that VC5 lowers to a punned
 * 0x437F0000 select. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                  /* nLights      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* lightScale   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* lightDir     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* lightAmb     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* unlit colour */
#define BR_DL_LV_PUN(d, s) (*(int *)&(d) = *(const int *)&(s))
/* BrDlLvIn, BrDlLvOut: br_dl.h */

void br_dl_light_vertex(const BrDlLvIn *pIn, BrDlLvOut *pOut)
{
    float t, v;

    if (DAT_105ccfd0 != 0) {
        t = (pIn->n[1] * DAT_105ce220 + pIn->n[2] * DAT_105ce224)
            + pIn->n[0] * DAT_105ce21c;
        if (t >= 0.0f) {
            v = t * DAT_105ce210 + DAT_105ce228;
            pOut->c[0] = (v > 255.0f) ? 255.0f : v;
            v = t * DAT_105ce214 + DAT_105ce22c;
            pOut->c[1] = (v > 255.0f) ? 255.0f : v;
            v = t * DAT_105ce218 + DAT_105ce230;
            pOut->c[2] = (v > 255.0f) ? 255.0f : v;
        } else {
            BR_DL_LV_PUN(pOut->c[0], DAT_105ce228);
            BR_DL_LV_PUN(pOut->c[1], DAT_105ce22c);
            BR_DL_LV_PUN(pOut->c[2], DAT_105ce230);
        }
    } else {
        BR_DL_LV_PUN(pOut->c[0], BrGbiRectG_5D17A4);
        BR_DL_LV_PUN(pOut->c[1], BrGbiRectG_5D17B4);
        BR_DL_LV_PUN(pOut->c[2], BrGbiRectG_5CE2D0);
    }
}
/* The port helper below keeps its BrDl signature under another name. */
#define br_dl_light_vertex br_dl_light_vertex_port
/* Port form (BrDl state, 3-component loop, the two port-only counters). */
/* (port-only br_dl_light_vertex removed) */


/* --- 0x1001FD70's table write, as a query -----------------------------
 * See the section header for the transcription. */
/* WHAT IT DOES: picks which of the six vertex-processing routines the next
 * batch of vertices goes through, based on the geometry switches currently
 * set -- whether the depth buffer is on, whether lighting is on, whether
 * texture coordinates are being generated, and whether decal mode is in
 * force. The port reports the original's address rather than installing a
 * function, so the choice stays checkable. */
/* @t4-pass 0x1001FD70 1 2026-09-07 probes 50 bytes 365 insns 80 regions 2 rows 42 census yes  (tools/crank.py) */
/* @t4-pass 0x1001FD70 2 2026-09-07 probes 50 bytes 365 insns 80 regions 2 rows 42 census yes  (tools/crank.py) */
/* @implements 0x1001FD70 glide BrDlVtxRoutine */
/* The port kept only the tail of this function -- the routine SELECTION --
 * and turned it into a value-returning query. The original takes no
 * arguments, returns nothing, and does three more things first:
 *
 *  - it reads the geometry mode from a global (0x105D17C8) and XORs it
 *    against the PREVIOUS mode (0x105D17CC) three separate times, driving a
 *    Glide state setter off each group of changed bits. Each block re-reads
 *    the mode afterwards, because the setter may change it.
 *  - it INSTALLS the chosen vertex routine into the dispatch slot at
 *    0x100A9A68 rather than returning it,
 *  - and it installs a pair of triangle handlers into 0x100A9D54 /
 *    0x100A9D1C, chosen by bit 0x200, on every one of the four exits.
 *
 * That is 53 of the 80 instructions and all three of the calls
 * tools/claimcheck.py flagged as "orig calls 3, port 0".
 *
 * RESIDUE (6+6 regnorm, -6 bytes, 80 instructions against 80): the original
 * loads the OLD mode first and xors the new one INTO it
 * (`mov ecx,[old]; mov eax,[new]; xor ecx,eax`), leaving the new mode alive
 * in eax; ours loads the new mode first and needs a copy before the xor. Two
 * of the three blocks then inherit the pairing, and one setter call has its
 * store scheduled after the push instead of before.
 * PROBED DEAD: flipping the xor operands (`geo ^ g_brDlGeoOld`) is
 * byte-identical -- VC5 canonicalises it. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105D17C8  the mode being applied  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105D17CC  the mode last applied   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x105D17D8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105CE2E0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105CDA04  picks the decal lighter */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A9A68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x100A9D54 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x100A9D1C */

/* BrGlSetCullMode: prototype in br_funcs.h */
/* BrGlSetCullSide: prototype in br_funcs.h */
/* BrGlSetDepthFn: prototype in br_funcs.h */

/* The eight routines the two slots select between. */
/* BrDlVtxLitDecal: prototype in br_funcs.h */
/* BrDlVtxLit: prototype in br_funcs.h */
/* BrDlVtxPlain: prototype in br_funcs.h */
/* BrDlVtxGenLin: prototype in br_funcs.h */
/* BrDlVtxGen: prototype in br_funcs.h */
/* BrDlVtxNoZLit: prototype in br_funcs.h */
/* BrDlVtxNoZ: prototype in br_funcs.h */
/* BrDlTri1Z: prototype in br_funcs.h */
/* BrDlTri2Z: prototype in br_funcs.h */
/* BrDlTri1ZFlat: prototype in br_funcs.h */
/* BrDlTri2ZFlat: prototype in br_funcs.h */
/* BrDlTri1NoZ: prototype in br_funcs.h */
/* BrDlTri2NoZ: prototype in br_funcs.h */
/* BrDlTri1NoZFlat: prototype in br_funcs.h */
/* BrDlTri2NoZFlat: prototype in br_funcs.h */

void BrDlVtxRoutine(void)
{
    uint32_t geo;

    /* 2026-09-24: the mode globals are re-read in every test (no cached
     * copy until the install), the cull mode is an if/else of two calls,
     * and the cull-side store is shared through a goto. */

    if ((((*(uint32_t *)&DAT_105d17cc) ^ (*(uint32_t *)&BrGbiRectG_5D17C8)) & 0x10000u) != 0) {
        if (((*(uint32_t *)&BrGbiRectG_5D17C8) & 0x10000u) != 0)
            grFogMode(2);
        else
            grFogMode(0);
    }
    if ((((*(uint32_t *)&DAT_105d17cc) ^ (*(uint32_t *)&BrGbiRectG_5D17C8)) & 0x3000u) != 0) {
        int32_t m = 0;

        g_brDlCullMode = m;
        if (((*(uint32_t *)&BrGbiRectG_5D17C8) & 0x1000u) != 0)
            m = 2;
        else if (((*(uint32_t *)&BrGbiRectG_5D17C8) & 0x2000u) != 0)
            m = 1;
        else
            goto side;
        g_brDlCullMode = m;      /* one store, shared by both set arms */
side:
        grCullMode(m);
    }
    if ((((*(uint32_t *)&DAT_105d17cc) ^ (*(uint32_t *)&BrGbiRectG_5D17C8)) & 1u) != 0) {
        if (((*(uint32_t *)&BrGbiRectG_5D17C8) & 1u) != 0) {
            (*(int32_t *)&BrGlDepthFuncShadow) = 1;
            grDepthBufferFunction(1);
        } else {
            (*(int32_t *)&BrGlDepthFuncShadow) = 7;
            grDepthBufferFunction(7);
        }
    }
    geo = (*(uint32_t *)&BrGbiRectG_5D17C8);

    /* ---- 0x1001FE0D: install, do not return ------------------------- */
    if ((geo & 1u) != 0) {
        if ((geo & 0x20000u) != 0) {
            (*(void * *)&g_brGbi0A79F0[4]) = (*(int32_t *)&BrGbiRectG_5CDA04) ? (void *)BrDlVtxLitDecal
                                          : (void *)BrDlVtxLit;
        } else {
            (*(void * *)&g_brGbi0A79F0[4]) = (void *)BrDlCmdVtx;
        }
        if ((geo & 0x40000u) != 0) {
            /* Assigned then overridden -- the original stores the LIN
             * routine unconditionally and replaces it. */
            (*(void * *)&g_brGbi0A79F0[4]) = (void *)BrDlVtxGenLin;
            if ((geo & 0x80000u) == 0)
                (*(void * *)&g_brGbi0A79F0[4]) = (void *)BrDlVtxGen;
        }
        if ((geo & 0x200u) != 0) {
            (*(void * *)&g_brGbi0A79F0[191]) = (void *)BrDlCmdTri1;
            (*(void * *)&g_brGbi0A79F0[177]) = (void *)BrDlCmdTri2;
        } else {
            (*(void * *)&g_brGbi0A79F0[191]) = (void *)BrDlCmdTri1FlatZ;
            (*(void * *)&g_brGbi0A79F0[177]) = (void *)BrDlCmdTri2FlatZ;
        }
    } else {
        (*(void * *)&g_brGbi0A79F0[4]) = (void *)BrDlVtxNoZLit;
        if ((geo & 0x20000u) == 0)
            (*(void * *)&g_brGbi0A79F0[4]) = (void *)BrDlVtxNoZ;
        if ((geo & 0x200u) != 0) {
            (*(void * *)&g_brGbi0A79F0[191]) = (void *)BrDlCmdTri1NoZ;
            (*(void * *)&g_brGbi0A79F0[177]) = (void *)BrDlCmdTri2NoZ;
        } else {
            (*(void * *)&g_brGbi0A79F0[191]) = (void *)BrDlCmdTri1Flat;
            (*(void * *)&g_brGbi0A79F0[177]) = (void *)BrDlCmdTri2Flat;
        }
    }
}

/* Port-only: it asks BrDlVtxRoutine for a VALUE, and the original's form
 * installs rather than returns. Nothing in the matching build calls it. */

/* (port-only BrDlColourScale removed) */


/* --- 0x10022120, and it is now ONE body -------------------------------
 * 0x10021A20 inlines these seven tests; 0x10021C70 calls them.  The tests
 * themselves used to be written out here AND again in slice2_16.c as
 * BrGbiClipCodes (BRD3D 0x10022DC0), and the two disagreed about NaN for as
 * long as both existed.  Both are gone; br_dlshared.c holds the one copy and
 * carries both addresses.  This is now nothing but the BrDlVtx field
 * ordering, which is this file's own business. */
/* (port-only br_dl_outcode removed) */


/* --- 0x10022070, and the identical tail 0x10021BAD..0x10021C48 ---------
 * Perspective divide, viewport, quarter-pixel snap, colour store.  The lit
 * transforms call it with the colour in three registers; the unlit one
 * inlines it and uses n0/n1/n2. */
/* WHAT IT DOES: turns a transformed vertex into a screen position: divides
 * through by depth to get perspective, applies the viewport scale and
 * offset, stores the vertex's colour, and snaps the result to the nearest
 * quarter of a pixel -- the resolution the hardware rasteriser works at. */
/* port-only body; the Glide match is src/core/drawing/br_dlproject.c */
/* br_dl_project: the placed body is br_dlproject.c */

/* ---- 0x04 G_VTX ----------------------------------------------------
 * Both transforms, selected exactly as 0x1001FD70 selects them.  The unlit
 * body is 0x10021A20 (Glide-only; D3D 0x10021BD0); the lit body is
 * 0x10021C70 and its three equivalents. */
/* (port-only br_dl_vtx removed) */


/* ---- 0x06 G_DL  (0x10021020, Glide-only) ---------------------------- */
/* (port-only br_dl_calldl removed) */


/* ---- 0xB8 G_ENDDL  (0x10021060, SHARED) ----------------------------- */
/* (port-only br_dl_enddl removed) */


/* ---- 0xB6 / 0xB7 geometry mode  (0x1001FD40 / 0x100211E0, SHARED) --- */
/* (port-only br_dl_geoclear removed) */

/* (port-only br_dl_geoset removed) */


/* ---- 0xB9 G_SETOTHERMODE_L  (0x10021210, SHARED) -------------------- */
/* (port-only br_dl_othermodeL removed) */


/* ---- 0xBC G_MOVEWORD  (0x100239C0, SHARED) -------------------------- */
/* (port-only br_dl_moveword removed) */


/* ---- 0xBD G_POPMTX  (0x100211B0, SHARED) ---------------------------- */
/* (port-only br_dl_popmtx removed) */


/* ==================================================================== */
/* PART 2 -- the clipper                                                */
/* ==================================================================== */
/* 0x1001EE70 (607 B) is the triangle submitter that drives seven per-plane
 * routines and a shared interpolator.  Only the DRIVER is here.  The plane
 * routines, the interpolator and the 64-node pool are slice1_03's -- see
 * below on why that matters -- so this section is the part that had no host
 * definition, and nothing else.
 *
 * WHERE THE REST OF IT ALREADY LIVED
 * ----------------------------------------------------------------------
 * BRGlide 0x1001F0D0 / 0x1001F2B0 / 0x1001F3F0 / 0x1001F530 are BRD3D
 * 0x1001D810 / 0x1001D9F0 / 0x1001DB30 / 0x1001DC70, which slice1_03.c ports
 * as BrClipPlaneW / WPlusF04 / WMinusF04 / WPlusF08, with 0x1001F200 ==
 * 0x1001D940 == BrClipLerpVert and the pool as BrClipPoolInit.  Grepping the
 * GLIDE addresses finds none of that; grepping the D3D ones finds all of it.
 * slice1_04.h even wrote down the three-line integration for the remaining
 * three planes and the reason -- forking the pool would be "a correctness
 * hazard, not just duplication".  Those three are now in slice1_03.c.
 *
 * THE LIST.  0x1001EE70's prologue takes three BrDlVtx*, adds 0x40 to each,
 * and links them a->b->c->a, keeping `{ head, count }` in two stack slots at
 * ebp-8 / ebp-4 which it passes to every plane routine by address -- exactly
 * slice1_03's BrClipList.  So a clip NODE is `&vtx->f40`, and that is what
 * pins slice1_03's positional field names: f04/f08/f0C = clip x/y/z,
 * f10/f14 = s/t, f18 = clip w, f1C/f20/f24 = the Vtx's trailing bytes.
 *
 * THE POOL.  0x10023B10 threads 0x105CCFF0..0x105CD9C8 downward in steps of
 * 0x28 -- 64 nodes -- and leaves the LOWEST as the head of the free list at
 * 0x105CDA00.  Every free site tests `0x105CCFF0 <= p < 0x105CD9F0` first, so
 * only pool nodes are recycled and the three vertex-resident seeds are
 * silently dropped.  The storage is here because this file is what makes the
 * pool exist at all in the port; the LIST is slice1_03's, one object.
 *
 * THE PLANES, and the order, which is observable.  The seven bodies are
 * identical apart from the two-instruction distance expression:
 *
 *   0x1001F7B0  fld cz ; fadd cw   ->  cz + cw   NEAR    called 1st
 *   0x1001F2B0  fld cw ; fadd cx   ->  cw + cx   LEFT    called 2nd
 *   0x1001F3F0  fld cw ; fsub cx   ->  cw - cx   RIGHT   called 3rd
 *   0x1001F670  fld cw ; fsub cy   ->  cw - cy   TOP     called 4th
 *   0x1001F8F0  fld cw ; fsub cz   ->  cw - cz   FAR     called 5th
 *   0x1001F530  fld cy ; fadd cw   ->  cy + cw   BOTTOM  called 6th
 *   0x1001F0D0  fld cw            ->  cw        W       called 7th
 *
 * Same seven half-spaces as br_dl_vtx's outcode bits, in a DIFFERENT order --
 * and Sutherland-Hodgman's output vertex order depends on it, so the order is
 * preserved rather than tidied.
 *
 * THE POLARITY.  Each routine does `fcomp [0x10077410]`, and 0x10077410 reads
 * 0x00000000, so the threshold is plain zero; then `fnstsw ax / test ah,1`,
 * i.e. C0, and the jump on C0 goes to the "outside" arm.  C0 is set for
 * unordered as well as less-than, so a NaN distance is OUTSIDE.  slice1_03
 * keeps that by writing the INSIDE test as `d >= 0.0f`. */

/* The pool storage.  Static, not per-BrDl, because the original's is one
 * global block and slice1_03's free list is one global list: a per-instance
 * copy would be the aliased-storage bug CONVENTIONS.md describes.  A second
 * live BrDl shares it, which is what the original does too. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x105CCFF0 */
static BrClipVert s_aClipSeed[3];                 /* the three &vtx->f40 */

/* 0x10023B10's free-list threading, delegated. */
/* WHAT IT DOES: empties the pool of spare vertices the triangle clipper
 * borrows from, putting every one of them back on the free list ready for
 * the next frame. */
/* @implements 0x10023B10 glide br_dl_clip_reset */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
static void br_dl_clip_reset(void)
{
    int i;
    BrClipVert *c;

    DAT_10b73530 = ((void (*)(void *))BrFramePresent);
    DAT_10b73534 = ((funcptr)FUN_10023aa0);
    DAT_10b7352c = ((funcptr)BrPodNop);
    /* the free list over the 64-entry pool (0x105CCFF0..0x105CD9C8 in the
     * original): built from the last entry down, so it starts at entry 0 */
    c = 0;
    for (i = 63; i >= 0; i--) {
        s_aClipPool[i].pNext = c;
        c = &s_aClipPool[i];
    }
    g_pClipFree = c;
    DAT_105ccfe8 = 0;
    DAT_100a9a50 = 1;
    (*(int *)&BrGlBlendLatch) = 0;
}

/* The seven planes in 0x1001EE70's CALL order. */
typedef void (*BrDlClipPlaneFn)(BrClipList *);
static const BrDlClipPlaneFn s_apClipPlane[7] = {
    BrClipPlaneWPlusF0C,    /* 0x1001F7B0  NEAR   */
    BrClipPlaneWPlusF04,    /* 0x1001F2B0  LEFT   */
    BrClipPlaneWMinusF04,   /* 0x1001F3F0  RIGHT  */
    BrClipPlaneWMinusF08,   /* 0x1001F670  TOP    */
    BrClipPlaneWMinusF0C,   /* 0x1001F8F0  FAR    */
    BrClipPlaneWPlusF08,    /* 0x1001F530  BOTTOM */
    BrClipPlaneW            /* 0x1001F0D0  W      */
};

/* --- 0x1001EE70's output stage ---------------------------------------
 * Identical arithmetic to the tail of br_dl_vtx plus the s/t scaling of
 * br_dl_finish_vtx, written out here because the original writes it out here
 * too (0x1001EF82..0x1001F065) rather than calling either. */
/* WHAT IT DOES: turns one vertex produced by clipping into a finished screen
 * vertex, ready to be handed to the rasteriser: perspective divide,
 * viewport, quarter-pixel snap, colour, and texture coordinates divided
 * through by depth for both texture units. This is why a triangle cut by the
 * edge of the screen keeps its shading -- the clipper carries the already-
 * lit colour across the cut, not the surface normal. */
/* NOT TAGGED: this is the PORT's per-vertex emitter, factored out of
 * 0x1001EE70; the original has no such function.  The matching-build
 * transcription of 0x1001EE70 is BrDlClipTriZ in br_dltrim.c, where the
 * whole trimmer is one function as the original has it.  The tag used to sit
 * here and scored 288 bytes against the original's 607, which is the port's
 * factoring showing up as a permanent 246-diff row. */
/* (port-only br_dl_clip_emit removed) */


/* --- 0x1001EE70 ------------------------------------------------------- */
/* (port-only br_dl_clip_tri removed) */


/* ---- triangles  (0xBF 0x1001ECF0, 0xB1 0x1001FA30 -- both Glide-only) */

/* 0x1001ED83: s and t are scaled by two globals (0x118ED1A4 / 0x118ED1A8 --
 * the tile's texel-to-Glide-unit factors, written by the texture binder,
 * which has not been read) and then by 1/w.  Held at 1.0 here. */
/* (port-only br_dl_finish_vtx removed) */


/* (port-only br_dl_tri removed) */


/* (port-only br_dl_tri1 removed) */


/* (port-only br_dl_tri2 removed) */


/* ---- 0xDC bind texture  (0x1001E2E0 Glide, 30 B; D3D 0x1001BD70, 149) */
/* (port-only br_dl_bindtex removed) */


/* ---- 0xDD re-aim texture  (0x1001E300, SHARED) ---------------------- */
/* (port-only br_dl_retarget removed) */


/* ---- 0xDE / 0xDF  (0x1001EB10 SHARED / 0x1001EB30 Glide-only) ------- */
/* (port-only br_dl_setDE removed) */

/* (port-only br_dl_setDF removed) */


/* ---- rectangles ------------------------------------------------------
 * Four opcodes, two coordinate conventions, and this is where the RDP
 * dialect diverges from stock F3D:
 *
 *   0xF6  fill rect, 10.2 corners, 8 bytes   (0x1001E320, SHARED)
 *   0xE1  fill rect, INTEGER corners, 8 B    (0x1001E720, SHARED)
 *   0xE4  texture rect, 10.2, 24 BYTES       (0x10021570, SHARED)
 *   0xE3  texture rect, INTEGER, 8 bytes     (0x100219D0, SHARED)
 *
 * CONVENTIONS.md already records "0xE1 is FILL RECTANGLE with integer
 * corners here"; the table pins that -- 0x100A9A58 + 0xE1*4 holds
 * 0x1001E720, whose only difference from the 0xF6 handler is `sar 0x14`
 * where the other has `sar 0x16` and an `and 0x3FF`.
 *
 * ALL FOUR PUT THE LOWER-RIGHT CORNER IN w0 AND THE UPPER-LEFT IN w1.  This
 * is stock RDP packing and the file used to get it backwards on the
 * untextured pair while getting it right on the textured pair, which is the
 * strongest evidence available that the untextured arm was the wrong one.
 * Read off the disassembly:
 *
 *   0x10021570 (0xE4) pushes, last-first,  (w1>>12, w1&0xFFF, w0>>12,
 *     w0&0xFFF, tile) into 0x100215C0 -- so w1 is the FIRST corner.
 *   0x1001E320 (0xF6) and 0x1001E720 (0xE1) push, last-first,
 *     (w1 hi, H - (w0 lo) - 1, (w0 hi) + 1, H - (w1 lo)) into 0x1001E380,
 *     whose four opening clamps are max, max, min, min -- so the w1 fields
 *     are the MINIMA (upper-left) and the w0 fields the maxima.
 *
 * THE THREE FIELD DECODES, each verbatim:
 *   0xE1  shl 20 / sar 20            signed 12-bit integer, no mask.  A -8
 *                                    corner is -8; masking it with 0xFFF
 *                                    makes it 4088.
 *   0xF6  shl 20 / sar 22 / and 0x3FF   net (w >> 2) & 0x3FF -- the sign
 *                                    extension is masked straight off again,
 *                                    so 0xF6's corners are UNSIGNED.
 *   0xE3/0xE4  and 0xFFF, and 0xE3 additionally shifts each field left two
 *                                    on the way in, so both reach 0x100215C0
 *                                    in 10.2 and both are unsigned.
 *
 * AND THE UNTEXTURED PAIR DO NOT PASS THE CORNERS ON RAW.  They flip Y
 * against 0x100A7518 and adjust both maxima by one:
 *     0x1001E380(ulx, H - lry - 1, lrx + 1, H - uly)
 * (`inc edi` at 0x1001E74E / 0x1001E363, `dec edx` at 0x1001E753 /
 * 0x1001E368).  That window is recorded in pDl->rectMinX..rectMaxY. */

/* (port-only br_dl_rect removed) */


/* WHAT IT DOES: draws a solid-colour rectangle whose corners were given in
 * quarter-pixel units. The corners are unsigned in this form. The port
 * records the resulting screen window but does not itself paint the pixels. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1001e380: prototype in br_funcs.h */

/* WHAT IT DOES: draws a solid-colour rectangle whose corners were given in
 * QUARTER-pixel units, flipping the vertical coordinates because the display
 * list counts rows from the bottom and the screen counts from the top. The
 * whole-pixel twin is br_dl_fillE1. */
/* @implements 0x1001E320 glide br_dl_fillF6 */
const uint8_t *br_dl_fillF6(const uint8_t *p)
{
    int H, ulx, uly, lrx, lry;
    unsigned w0, w1;

    w1 = *(const unsigned *)(p + 4);
    w0 = *(const unsigned *)p;
    H = BrGbiRectG_A7518;
    uly = ((int)(w1 << 20) >> 22) & 0x3FF;
    lry = ((int)(w0 << 20) >> 22) & 0x3FF;
    lrx = ((int)(w0 << 8) >> 22) & 0x3FF;
    ulx = ((int)(w1 << 8) >> 22) & 0x3FF;
    BrGlRectFill(ulx, H - lry - 1, lrx + 1, H - uly);
    return p + 8;
}

/* WHAT IT DOES: draws a solid-colour rectangle whose corners were given as
 * whole pixels. Unlike its quarter-pixel twin br_dl_fillF6 these corners are
 * SIGNED, so a corner off the left of the screen really is negative. */
/* @implements 0x1001E720 glide br_dl_fillE1 */
const uint8_t *br_dl_fillE1(const uint8_t *p)
{
    int H, ulx, uly, lrx, lry;
    unsigned w0, w1;

    w1 = *(const unsigned *)(p + 4);
    w0 = *(const unsigned *)p;
    H = BrGbiRectG_A7518;
    uly = (int)(w1 << 20) >> 20;
    lry = (int)(w0 << 20) >> 20;
    lrx = (int)(w0 << 8) >> 20;
    ulx = (int)(w1 << 8) >> 20;
    BrGlRectFill(ulx, H - lry - 1, lrx + 1, H - uly);
    return p + 8;
}

/* WHAT IT DOES: draws a textured rectangle straight onto the screen -- the
 * command behind heads-up display panels and menu artwork -- with its
 * corners given in quarter-pixel units. It swallows three commands' worth of
 * data, because the texture coordinates follow it. */
/* The decode is br_dlshared.c's BrDlsTileRectDecode, which carries this
 * address and BRD3D's 0x10021510. */
/* (port-only br_dl_texE4 removed) */

/* WHAT IT DOES: the same textured screen rectangle with its corners given as
 * whole pixels, scaled up to quarter-pixels on the way through. */
/* Likewise 0x100219D0 / BRD3D 0x10021B80. */
/* (port-only br_dl_texE3 removed) */


/* ---- 0xED / 0xE2 scissor  (0x1001EB50 / 0x1001EBC0, both Glide-only) --
 * TWO FUNCTIONS, TWO CONVENTIONS.  This file used to route both slots to one
 * handler using the 0xED decode, which silently divided 0xE2's corners by
 * four.  They are 103 and 97 bytes and differ in exactly the four shift/mask
 * pairs:
 *
 *   0x1001EB61  shr eax,0xE ; and eax,0x3FF   |  0x1001EBD1  shr eax,0xC
 *   0x1001EB70  shr ecx,2   ; and ecx,0x3FF   |              ; and eax,0xFFF
 *   0x1001EB84  shr ecx,0xE ; and ecx,0x3FF   |  0x1001EBE0  and ecx,0xFFF
 *   0x1001EB97  shr ebx,2   ; and ebx,0x3FF   |  ...
 *
 * Confirmed independently in BRD3D.dll, where the same two slots hold
 * 0x1001CDA0 (0xED: `shr 0xE / and 0x3FF` at 0x1001CDBB) and 0x1001CE70
 * (0xE2: `shr 0xC / and 0xFFF` at 0x1001CE8B).
 *
 * THE SHIFTS ARE `shr`, NOT `sar`: unlike 0xE1 the scissor corners are
 * UNSIGNED in both forms.  Preserved.
 *
 * THE Y FLIP IS THE POINT, and the roles of the two Y corners swap with it:
 *     0x105D17BC = ulx      = minX        0x105D17B8 = lrx      = maxX
 *     0x105D17C0 = H - lry  = minY        0x105CCFE0 = H - uly  = maxY
 * Established from the CONSUMER, not from the names: 0x1001E380 opens with
 * four clamps -- `jge` against 0x105D17BC, `jge` against 0x105D17C0, `jle`
 * against 0x105D17B8, `jle` against 0x105CCFE0 -- i.e. max, max, min, min.
 * The tail (0x1001EBAB) passes the same four to grClipWindow through the
 * thunk 0x100729D2 in the order (minx, miny, maxx, maxy).
 *
 * DUPLICATION, stated rather than left to be found: br_dlglide.c transcribes
 * these two addresses independently and correctly as BrDlGlScissorInt /
 * BrDlGlScissorFrac.  Two host definitions of one original address is what
 * CONVENTIONS.md's aliased-storage section is about; they no longer DISAGREE,
 * which is the part that was actively harmful, but the duplication is real
 * and outlives this pass.  Same for 0xE1 (BrDlGlFillRect), 0xDC, 0xDD and
 * 0xDF here, and for 0xF6/0xF7/0xFA/0xFB in br_dlcmd.c. */
/* (port-only br_dl_scissor removed) */


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* grClipWindow: prototype in br_funcs.h */

/* 0x1001EBC0 -- opcode 0xE2, 97 bytes.  Integer fields. */
/* WHAT IT DOES: sets the clipping window -- the region of the screen
 * anything drawn afterwards is confined to -- from whole-pixel corners,
 * flipping the vertical axis because the game's display list counts down the
 * screen and the renderer counts up. */
/* @implements 0x1001EBC0 glide br_dl_scissorE2 */
const uint8_t *br_dl_scissorE2(const uint8_t *p)
{
    int H, ulx, uly, lrx, lry, maxY, minY;

    H = BrGbiRectG_A7518;
    ulx = (int)((*(const unsigned *)p >> 12) & 0xFFFu);
    (*(int *)&BrGlClipMinX) = ulx;
    uly = (int)(*(const unsigned *)p & 0xFFFu);
    maxY = H - uly;
    (*(int *)&BrGlClipMaxY) = maxY;
    lrx = (int)((*(const unsigned *)(p + 4) >> 12) & 0xFFFu);
    (*(int *)&BrGlClipMaxX) = lrx;
    lry = (int)(*(const unsigned *)(p + 4) & 0xFFFu);
    minY = H - lry;
    (*(int *)&BrGlClipMinY) = minY;
    grClipWindow(ulx, minY, lrx, maxY);
    return p + 8;
}

/* 0x1001EB50 -- opcode 0xED, 103 bytes.  10.2 fields. */
/* WHAT IT DOES: the same clipping-window setter for the quarter-pixel form
 * of the command. These really are two separate routines in the original,
 * and routing both through one decode quietly divides one form's corners by
 * four. */
/* @implements 0x1001EB50 glide br_dl_scissorED */
const uint8_t *br_dl_scissorED(const uint8_t *p)
{
    int H, ulx, uly, lrx, lry, maxY, minY;

    H = BrGbiRectG_A7518;
    ulx = (int)((*(const unsigned *)p >> 14) & 0x3FFu);
    (*(int *)&BrGlClipMinX) = ulx;
    uly = (int)((*(const unsigned *)p >> 2) & 0x3FFu);
    maxY = H - uly;
    (*(int *)&BrGlClipMaxY) = maxY;
    lrx = (int)((*(const unsigned *)(p + 4) >> 14) & 0x3FFu);
    (*(int *)&BrGlClipMaxX) = lrx;
    lry = (int)((*(const unsigned *)(p + 4) >> 2) & 0x3FFu);
    minY = H - lry;
    (*(int *)&BrGlClipMinY) = minY;
    grClipWindow(ulx, minY, lrx, maxY);
    return p + 8;
}

/* ---- 0xF2 G_SETTILESIZE  (0x1001EC30, SHARED) -----------------------
 * The D3D twin is 0x1001CF30, which slice2_16.c ports as BrGbiSetTileSize
 * -- it was called BrGbiSetScissor until this pass.  Same 178 bytes, same
 * slot 0xF2 in both builds' tables; one function under two addresses, and
 * ONE BODY: br_dlshared.c holds the decode and carries both addresses. */
/* WHAT IT DOES: tells the renderer which rectangle of a texture the next
 * drawings will use, and works out that rectangle's width and height in
 * texture pixels. Sign is preserved throughout, so a negative span stays
 * negative. */
/* (port-only br_dl_settilesize removed) */


/* ---- 0xF7 fill colour, 0xF8 fog colour ------------------------------
 * 0x1001E9F0 IS NOT A RAW STORE.  110 bytes, and every one of them is a
 * decode: it expands the LOW RGBA5551 half of w1 into four separate BYTE
 * globals, which the rectangle emitter 0x1001E380 reads at 0x1001E441 --
 * `mov bl,[0x105CCD40] / mov al,[0x105CCFD8] / mov cl,[0x105D17A0] /
 * mov dl,[0x105CE208]` -- on the arm taken whenever the latched combiner is
 * not the prim-colour row.  So 0xF7 is the colour 0xF6 fills with, and
 * keeping the word verbatim leaves that decode unwritten.
 *
 * The three colour channels use one idiom three times, e.g. for red at
 * 0x1001E9F8..0x1001EA09:
 *     mov ecx,w1 ; shr ecx,8      cl = bits 15:8
 *     mov edx,w1 ; shr edx,0xD    dl = bits 15:13
 *     xor dl,cl ; and dl,7 ; xor dl,cl
 * The three-instruction tail is the standard bitfield merge
 * `a ^ ((a ^ b) & mask)` == `(a & ~7) | (b & 7)`, i.e. the 5->8 widening
 * (v << 3) | (v >> 2) assembled out of one register pair.  Green is the same
 * with shifts 3 and 8; blue is `and cl,0xFE / shl cl,2` -- an EIGHT-BIT
 * shift, so bits 6 and 7 fall off the end, which is what leaves room for the
 * low three from `shr edx,3`.
 *
 * Alpha is `and cl,1 / neg cl / sbb ecx,ecx / and ecx,0xFF`: bit 0 spread to
 * all eight, giving 0 or 255, never 0 or 1.
 *
 * br_dlcmd.c transcribes this address independently as BrDlCmdFillColour; see
 * the duplication note on the scissor above. */
/* WHAT IT DOES: sets the colour that solid-colour rectangles are filled
 * with. The command carries the colour packed into fifteen bits plus one bit
 * of transparency, and this expands it back out to four full bytes -- the
 * transparency bit becoming either fully solid or fully clear, never
 * anything between. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: sets the colour the next solid rectangle will be painted in.
 * The display list carries it as one packed 16-bit value; this unpacks it into
 * the separate red, green, blue and alpha bytes the renderer holds, widening
 * each 5-bit channel to 8 bits by copying its top bits into the low ones. */
/* @implements 0x1001E9F0 glide br_dl_fillcolour */
const uint8_t *br_dl_fillcolour(const uint8_t *p)
{
    unsigned w;

    w = *(const unsigned *)(p + 4);
    (*(unsigned char *)&BrGlFillR) = (unsigned char)(((w >> 8) & 0xF8) | ((w >> 13) & 7));
    w = *(const unsigned *)(p + 4);
    (*(unsigned char *)&BrGlFillG) = (unsigned char)(((w >> 3) & 0xF8) | ((w >> 8) & 7));

    {
        unsigned char c = (unsigned char)(*(const unsigned char *)(p + 4) & 0xFE);
        unsigned char d = (unsigned char)((*(const unsigned *)(p + 4) >> 3) & 7);
        (*(unsigned char *)&BrGlFillB) = (unsigned char)((unsigned char)(c << 2) | d);
    }

    w = *(const unsigned *)(p + 4);
    (*(unsigned char *)&BrGlFillA) = (unsigned char)((w & 1) ? 0xFFu : 0);
    return p + 8;
}

/* (port-only br_dl_fogcolour removed) */


/* ---- 0xFA prim colour, 0xFB env colour  (0x1001EA80 / 0x1001E930) ---
 * ONE UNPACK IS WRONG FOR ONE OF THEM.  Both take the same four bytes in the
 * same order -- R (bits 31:24), G, B, A -- and both `fild` each into a float.
 * There the two part company, and this file used to divide both by 255:
 *
 *   0x1001EA80 (0xFA)  fild qword [esp+4] ; fstp dword [dest]
 *                      -- four times, at 0x1001EAA0 / EABE / EADC / EAF3.
 *                      NO fmul anywhere in the 138 bytes.  Prim is 0..255.
 *   0x1001E930 (0xFB)  fild ; fstp scratch ; fld scratch ;
 *                      fmul [0x10077400] ; fstp dest
 *                      -- four times, and 0x10077400 is 0x3B808081, the float
 *                      nearest 1/255.  Env is 0..1.
 *
 * Three independent corroborations that 0..255 is the reading and not an
 * oversight: BR_DL_COLOUR_MAX (0x10077418) is 255.0f and is what the lit
 * colour is clamped to two functions away; 0x1001E380 passes all four of
 * 0xFA's destinations through _ftol (0x10074560) into single BYTES at
 * 0x1001E412..0x1001E43B, which only makes sense for 0..255; and the
 * "lights off" fallback at 0x10022BCC copies three of them straight into a
 * vertex colour in a pipeline whose ceiling is 255.
 *
 * The spill through a stack slot between 0xFB's fild and its fmul is a real
 * rounding to float.  Reproduced rather than folded, as br_dlcmd.c does.
 *
 * br_dlcmd.c transcribes both addresses independently as BrDlCmdPrimColour /
 * BrDlCmdEnvColour; see the duplication note on the scissor above. */
/* WHAT IT DOES: sets the primitive colour, the flat colour used where a
 * drawing is not taking its colour from a texture or from vertex shading.
 * The four channels are kept on a 0-to-255 scale, unlike the environment
 * colour below. */
/* port-only body; Glide match is src/core/generated/0x1001EA80.c */
/* br_dl_prim: the placed body is br_dlcmd.c */
/* WHAT IT DOES: sets the environment colour, the second flat colour the
 * pixel combiner can mix in. Unlike the primitive colour this one is scaled
 * down to a 0-to-1 range as it is stored, which is a real difference between
 * the two commands and not an oversight. */
/* @implements 0x1001E930 glide br_dl_env */
/* FOUR FACTS, all read off 0x1001E930, and together they are the whole 183
 * bytes (this landed byte-exact at /O2 /Op -- see the note on the variant
 * below, the report row's O2y was simply the wrong guess):
 *
 *  1. ONE ARGUMENT.  `mov eax,[esp+0xC]` after `sub esp,8` is arg1, and every
 *     read is `[eax+4]`; there is no BrDl parameter.  The four destinations
 *     are SEPARATE ABSOLUTE GLOBALS and they are not even contiguous
 *     (0x105CCD44 / 0x105CD9F4 / 0x105CCCF8 / 0x105CCC74), so `pDl->env[]`
 *     was never an array -- the accessor sub-case of the factored-helper
 *     screen.  The dispatch table therefore holds a one-argument function
 *     here, which is why the install below needs a cast in this arm.
 *  2. THE DWORD IS READ RAW, not through br_dl_w.  MSVC5 will not inline
 *     br_dl_w (more than one caller), so it emitted a `call` the original
 *     does not have.  Spell the load out: `*(const uint32_t *)(p + 4)`.
 *  3. IT IS RE-READ FOR EVERY COMPONENT.  The original has four separate
 *     `mov edx,[eax+4]`; hoisting it into a `v` local costs three of them.
 *  4. THE CONVERSION IS UNSIGNED.  `fild qword ptr [esp]` with the high dword
 *     zeroed is `(float)(unsigned)`; a `(int32_t)` cast gives `fild dword`
 *     and is wrong.  Note the top component has NO mask -- `>> 24` alone,
 *     which is only correct because the value is unsigned.
 *
 * DEAD PROBE, do not re-run: naming the conversion in a `float t` to try to
 * reproduce the `fstp [esp+0xC]; fld [esp+0xC]` round-trip.  That round-trip
 * is /Op's round-to-float on assignment, not a source temp -- at /O2 /Op the
 * form WITHOUT the temp is byte-exact and the form with it is 119 diffs. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
const uint8_t *br_dl_env(const uint8_t *p)
{
    const float k = 1.0f / 255.0f;    /* 0x10077400 == 0x3B808081 exactly */

    BrGbiRectG_5CCD44 = (float)(*(const uint32_t *)(p + 4) >> 24) * k;
    BrGbiRectG_5CD9F4 = (float)((*(const uint32_t *)(p + 4) >> 16) & 0xFFu) * k;
    BrGbiRectG_5CCCF8 = (float)((*(const uint32_t *)(p + 4) >> 8) & 0xFFu) * k;
    DAT_105ccc74 = (float)(*(const uint32_t *)(p + 4) & 0xFFu) * k;
    return p + 8;
}

/* ---- 0xFC G_SETCOMBINE  (0x1001E770 SHARED -> 0x1001E7A0 Glide-only)  */

/* The whole state model, in one table.  0x1001E7A0 is a chain of exact
 * equality tests on the pair, and this is that chain with the compare order
 * preserved -- the 0xFC317E02 row has two accepted w1 values, which is why
 * the table has a mask column rather than two rows. */
static const struct { uint32_t w0, w1, w1b; BrDlCombine id; } s_aCombine[] = {
    { 0xFCFFFFFFu, 0xFFFCF87Cu, 0xFFFCF87Cu, BR_DL_CC_SHADE        },
    { 0xFCFFFFFFu, 0xFFFE793Cu, 0xFFFE793Cu, BR_DL_CC_TEX          },
    { 0xFC567EACu, 0xFFFFF3F9u, 0xFFFFF3F9u, BR_DL_CC_TEX_SHADE_C1 },
    { 0xFCFF97FFu, 0xFF2DFEFFu, 0xFF2DFEFFu, BR_DL_CC_TEX_SHADE_A  },
    { 0xFCFFFFFFu, 0xFFFDF2F9u, 0xFFFDF2F9u, BR_DL_CC_TEX_SHADE_B  },
    { 0xFCFFFFFFu, 0xFFFF73B9u, 0xFFFF73B9u, BR_DL_CC_TEX_SHADE_CW },
    { 0xFC127E08u, 0xF3FFF2F8u, 0xF3FFF2F8u, BR_DL_CC_ENVMAP       },
    { 0xFC317E02u, 0x5FFEF3FAu, 0x51FEF3FAu, BR_DL_CC_DECAL        },
    { 0xFC127FFFu, 0xFFFFF838u, 0xFFFFF838u, BR_DL_CC_TEX_SHADE_C0 }
};

/* (port-only BrDlClassifyCombine removed) */


/* (port-only br_dl_combine removed) */


/* ==================================================================== */
/* the table -- 0x100A9A58                                              */
/* ==================================================================== */

static BrDlHandler s_aTable[256];
static int s_fTableReady;

/* (port-only br_dl_build_table removed) */


/* (port-only BrDlIsHandled removed) */


/* ==================================================================== */
/* 0x10023C90                                                           */
/* ==================================================================== */

/* (port-only BrDlRun removed) */


/* ==================================================================== */
/* 0x10019040 -- the load-time patch pass                               */
/* ==================================================================== */

/* (port-only BrDlPatch removed) */


/* ==================================================================== */
/* PART 3 -- reference rasteriser (NOT in the original)                 */
/* ==================================================================== */

/* (port-only br_ras_tri removed) */


/* (port-only BrDlAttachRaster removed) */


/* -- Ghidra-matched functions --------------------------- */
/* funcptr: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: display-list opcode: call a function pointer with the low 24 bits of the command word, advance by the stride. */
/* @implements 0x1001E2E0 glide BrDlOpDispatch1 */

unsigned int * BrDlOpDispatch1(unsigned int *param_1)

{
  (*DAT_118ed1cc)(*param_1 & 0xffffff);
  return param_1 + param_1[1] * 2;
}

/* WHAT IT DOES: display-list opcode: call a function pointer with the command word and its argument, advance by 2. */
/* @implements 0x1001E300 glide BrDlOpDispatch2 */

unsigned int * BrDlOpDispatch2(unsigned int *param_1)

{
  (*DAT_118ed1d0)(*param_1 & 0xffffff,param_1[1]);
  return param_1 + 2;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1002bf50: prototype in br_funcs.h */

/* WHAT IT DOES: advance the 32-slot rect ring at 0x106E8818 (stride 0x10) and fill the
 * next slot with a doubled/mirrored screen rect; emit display-list command 0x03800010
 * pointing at the slot, and publish the slot's two 0x1FF fields. A negative width flips
 * the mirror flag at 0x106E8204; 0x1002BF50 is called first when param_5 asks for it. */
/* @implements 0x1002C0F3 glide BrDlRectCmdEmit */

void BrDlRectCmdEmit(int param_1,int param_2,int param_3,int param_4,int param_5)
{
  int *puVar1;
  int local_c;
  
  (*(int *)((char *)&g_aBrEntRecs + 0xB4)) = (*(int *)((char *)&g_aBrEntRecs + 0xB4)) + 1 & 0x1f;
  if (param_5 != 0) {
    if (param_3 < 0) {
      local_c = -param_3;
    }
    else {
      local_c = param_3;
    }
    BrSub_1003289F(param_1,param_2,local_c,param_4);
  }
  if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0) {
    param_1 = param_1 << 1;
    param_2 = param_2 << 1;
    param_3 = param_3 << 1;
    param_4 = param_4 << 1;
  }
  if (param_3 < 0) {
    if ((*(int *)&g_brRaceBeginDifficulty) != 0) {
      *(short *)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(param_3 * -2);
    }
    else {
      *(short *)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(param_3 << 1);
    }
    param_3 = -param_3;
    (*(int *)&BrG_6C1174) = 1;
  }
  else {
    if ((*(int *)&g_brRaceBeginDifficulty) != 0) {
      *(short *)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(param_3 * -2);
    }
    else {
      *(short *)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(param_3 << 1);
    }
    (*(int *)&BrG_6C1174) = 0;
  }
  *(short *)(&DAT_106e881a + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(param_4 << 1);
  *(short *)(&DAT_106e881c + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0x1ff;
  *(short *)(&DAT_106e881e + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0;
  *(short *)(&DAT_106e8820 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)((param_1 * 2 + param_3) * 2);
  *(short *)(&DAT_106e8822 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)((param_2 * 2 + param_4) * 2);
  *(short *)(&DAT_106e8824 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0x1ff;
  *(short *)(&DAT_106e8826 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0;
  puVar1 = (*(int * *)&g_BrGfxPtr);
  (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
  *puVar1 = 0x3800010;
  puVar1[1] = (int)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  DAT_106ed368 = (int)*(short *)(&DAT_106e881c + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  (*(int *)((char *)&g_aBrEntRecs + 0x18)) = (int)*(short *)(&DAT_106e8824 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1002bf50: prototype in br_funcs.h */


/* WHAT IT DOES: emit the full-screen rect: optionally emit an 0xE7000000 (pipe sync)
 * command and call 0x1002BF50 first, then fill the next ring slot with the doubled
 * screen size / half-size-times-4 rect and emit command 0x03800010 for it. The dead
 * parameter-doubling block under [0x106ED674] is the original's. */
/* @implements 0x1002C2E9 glide BrDlScreenRectEmit */

void BrDlScreenRectEmit(int param_1,int param_2,int param_3,int param_4,int param_5)
{
  int *puVar2;
  int *puVar1;
  int local_10;
  
  (*(int *)((char *)&g_aBrEntRecs + 0xB4)) = (*(int *)((char *)&g_aBrEntRecs + 0xB4)) + 1 & 0x1f;
  if (param_5 != 0) {
    puVar2 = (*(int * *)&g_BrGfxPtr);
    (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
    *puVar2 = 0xe7000000;
    puVar2[1] = 0;
    if (param_3 < 0) {
      local_10 = -param_3;
    }
    else {
      local_10 = param_3;
    }
    BrSub_1003289F(param_1,param_2,local_10,param_4);
  }
  if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0) {
    param_1 = param_1 << 1;
    param_2 = param_2 << 1;
    param_3 = param_3 << 1;
    param_4 = param_4 << 1;
  }
  (*(int *)&BrG_6C1174) = 0;
  *(short *)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(BrGbiRectG_A7514 << 1);
  *(short *)(&DAT_106e881a + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(BrGbiRectG_A7518 << 1);
  *(short *)(&DAT_106e881c + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0x1ff;
  *(short *)(&DAT_106e881e + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0;
  *(short *)(&DAT_106e8820 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(BrGbiRectG_A7514 / 2 << 2);
  *(short *)(&DAT_106e8822 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = (short)(BrGbiRectG_A7518 / 2 << 2);
  *(short *)(&DAT_106e8824 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0x1ff;
  *(short *)(&DAT_106e8826 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10) = 0;
  puVar1 = (*(int * *)&g_BrGfxPtr);
  (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
  *puVar1 = 0x3800010;
  puVar1[1] = (int)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  DAT_106ed368 = (int)*(short *)(&DAT_106e881c + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  (*(int *)((char *)&g_aBrEntRecs + 0x18)) = (int)*(short *)(&DAT_106e8824 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: emit display-list command 0x03800010 for the CURRENT rect-ring slot
 * (without advancing the ring) and publish its two 0x1FF fields -- the shared tail of
 * BrDlRectCmdEmit/BrDlScreenRectEmit as its own function. */
/* @implements 0x1002C4A3 glide BrDlRectCmdFlush */

void BrDlRectCmdFlush(void)

{
  int *puVar1;
  
  puVar1 = (*(int * *)&g_BrGfxPtr);
  (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2;
  *puVar1 = 0x3800010;
  puVar1[1] = (int)(&DAT_106e8818 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  DAT_106ed368 = (int)*(short *)(&DAT_106e881c + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  (*(int *)((char *)&g_aBrEntRecs + 0x18)) = (int)*(short *)(&DAT_106e8824 + (*(int *)((char *)&g_aBrEntRecs + 0xB4)) * 0x10);
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: emit the 21-command border/frame display list for the rect at
 * (param_1, param_2) sized (param_3, param_4): two passes of E1 edge-fill commands under
 * different othermode/fill settings, each coordinate packed x<<12|y into op and arg.
 * The 0xF7 fill-color's 0x10001 is a SHORT local set to 1 and doubled up as
 * `v | v << 16` -- Ghidra folded it, the /Od bytes still compute it (movsx x2). */
/* @implements 0x1002C50E glide BrDlBorderEmit */

void BrDlBorderEmit(unsigned int param_1,unsigned int param_2,int param_3,int param_4)

{
  short v;
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xb900031d; p_[1] = 0xf0a4000; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xba001402; p_[1] = 0x300000; }
  v = 1;
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xf7000000; p_[1] = v | v << 16; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xf8000000; p_[1] = 0xff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 - 1 & 0xfff; p_[1] = (param_1 - 1 & 0xfff) << 0xc | param_2 - 2 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + 1 + param_4 & 0xfff; p_[1] = (param_1 - 1 & 0xfff) << 0xc | param_2 + param_4 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 - 1 & 0xfff) << 0xc | 0xe1000000 | param_2 + param_4 & 0xfff; p_[1] = (param_1 - 2 & 0xfff) << 0xc | param_2 - 1 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + 1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + param_4 & 0xfff; p_[1] = (param_1 + param_3 & 0xfff) << 0xc | param_2 - 1 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xb900031d; p_[1] = 0x55004240; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xba001402; p_[1] = 0; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = 0xf8000000; p_[1] = 0xff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + 1 & 0xfff; p_[1] = (param_1 & 0xfff) << 0xc | param_2 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + param_4 & 0xfff; p_[1] = (param_1 & 0xfff) << 0xc | (param_2 - 1) + param_4 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + 1 & 0xfff) << 0xc | 0xe1000000 | param_2 + param_4 & 0xfff; p_[1] = (param_1 & 0xfff) << 0xc | param_2 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + param_4 & 0xfff; p_[1] = ((param_1 - 1) + param_3 & 0xfff) << 0xc | param_2 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + 3 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 - 2 & 0xfff; p_[1] = (param_1 - 3 & 0xfff) << 0xc | param_2 - 3 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + 3 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + 3 + param_4 & 0xfff; p_[1] = (param_1 - 3 & 0xfff) << 0xc | param_2 + 2 + param_4 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 - 2 & 0xfff) << 0xc | 0xe1000000 | param_2 + 3 + param_4 & 0xfff; p_[1] = (param_1 - 3 & 0xfff) << 0xc | param_2 - 3 & 0xfff; }
  { int *p_ = (*(int * *)&g_BrGfxPtr); (*(int * *)&g_BrGfxPtr) = (*(int * *)&g_BrGfxPtr) + 2; *p_ = (param_1 + 3 + param_3 & 0xfff) << 0xc | 0xe1000000 | param_2 + 3 + param_4 & 0xfff; p_[1] = (param_1 + 2 + param_3 & 0xfff) << 0xc | param_2 - 3 & 0xfff; }
  return;
}

