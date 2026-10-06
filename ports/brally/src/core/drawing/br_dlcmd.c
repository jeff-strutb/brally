/* br_dlcmd.c -- nine display-list opcode handlers, transcribed.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * One C function per slot of BRGlide's 256-entry dispatch table at
 * 0x100A9A58, written from the disassembly rather than modelled.  The
 * contract, the divergences from br_dl.c, the evidence that 0xF6 is fixed
 * point, and the 0x1001E9F0 / RallyMain address trap are all in br_dlcmd.h;
 * this file carries the instruction-level notes.
 *
 * Every handler here returns `p + 8`.  That is a RESULT, established path by
 * path below, not an assumption -- three of the twenty-eight handlers in this
 * table return something else.
 */
#include "br_dl.h"   /* br_globals: its objects */
#include "br_dlcmd.h"

#include <math.h>
#include <string.h>

/* ==================================================================== */
/* helpers                                                              */
/* ==================================================================== */

/* The list is in host order by the time the interpreter sees it (BrDlPatch
 * byte-swapped it at load), so a command is two host u32s.  Read them
 * byte-wise anyway: CONVENTIONS.md forbids overlaying a struct on a foreign
 * buffer, and a display list is exactly that. */
/* (port-only br_dlcmd_w removed) */


/* Read a float out of a host-order 32-bit pattern without aliasing. */
/* (port-only br_dlcmd_f32 removed) */


/* `fistp dword ptr [...]` under MSVC's startup control word: round to
 * NEAREST, ties to EVEN.  Out of range -- and NaN -- stores the x87 integer
 * indefinite, 0x80000000.
 *
 * This is NOT __ftol, which truncates and returns 0 out of range; the two
 * appear within a few hundred bytes of each other in this binary (0x10074560
 * is _ftol and is what 0x1001E380 uses) and confusing them has cost time
 * here before.  rint() honours the default rounding mode, which is the same
 * ties-to-even. */
/* (port-only br_dlcmd_fistp removed) */


/* ====================================================================
 * 0x10021A20 -- G_VTX, opcode 0x04.  584 bytes, Glide-only.
 *
 * The UNLIT vertex transform, and the one the dispatch table holds at link
 * time.  0x1001FD70 replaces slot 0x04 with a lit variant as soon as a
 * geometry mode arrives (br_dl.h, BrDlVtxRoutine); this module transcribes
 * the link-time occupant only.
 *
 * Command decode, from 0x10021A2B..0x10021A4F:
 *     w0 is spilled to [ebp-4] and `cl` is read from [ebp-2], i.e. BYTE 2 --
 *     the destination index v0.  n is (w0 >> 10) & 0x3F, which is F3DEX's
 *     gSPVertex packing (br_f3d.h has the evidence that this build is F3DEX
 *     and not F3D).  `jle` on the masked count: n == 0 jumps straight to the
 *     epilogue, so a zero-count G_VTX is a no-op that still advances 8.
 *
 * w1 is already a host pointer in the original (BrDlPatch's G_VTX arm ran it
 * through the vertex cache), and the loop steps it by 0x20 -- eight floats:
 * x, y, z, s, t, n0, n1, n2, which is what BrVtxExpand writes. */
/* WHAT IT DOES: loads a batch of model corner points into the renderer's
 * working set of vertices, moving each one from the model's own space into the
 * camera's, working out where it lands on screen, and noting which edges of
 * the view it falls outside so later triangles can be thrown away or trimmed.
 * This is the plain version used when the model is not being lit; a lit
 * version takes over the same slot once lighting is switched on. */
/* The matching arm (byte-exact) is BrDlCmdVtx in br_dlvtx_cmd.c: the snap
 * is inline asm there, as the original has it.  This is the port body. */
/* BrDlCmdVtx: the original (0x10021A20) is in br_dlvtx_cmd.c; this file kept a
 * port-era model of it for a test harness. */

/* ==================================================================== */
/* triangles -- 0xBF 0x1001ECF0 (378 B), 0xB1 0x1001FA30 (696 B)        */
/* ==================================================================== */

/* Complete one vertex's texture coordinates, which is the last thing both
 * triangle handlers do before submitting.  Inline at 0x1001ED71..0x1001EDBA
 * per vertex, and separately 0x1001FCF0 (70 bytes), which 0xB1 calls for the
 * SECOND triangle's third vertex only.  The two are
 * the same operation; 0x1001FCF0 takes the vertex twice, once as the record
 * and once as `&v->f40`, and reads s/t through the second at +0x10/+0x14 --
 * which is +0x50/+0x54 from the record, i.e. exactly s and t.
 *
 * Both TMUs get the same values: the writes are paired (+0x38 then +0x2C,
 * +0x30 then +0x24, +0x34 then +0x28). */
/* (port-only br_dlcmd_finish_vtx removed) */


/* The three-way decision both triangle handlers make, byte for byte the same
 * in each: 0x1001ED35..0x1001ED5C and 0x1001FA7A..0x1001FAB1 and again at
 * 0x1001FBEC..0x1001FC1B.
 *
 * NOTE THE ASYMMETRY, preserved because it is visible: the AND is formed from
 * the SECOND and THIRD outcodes and only then tested against the first, while
 * the OR is formed the other way round.  Same value either way; the original's
 * register pressure made the order explicit and br_dl.c preserves it too. */
/* (port-only br_dlcmd_tri removed) */


/* 0x1001ECF0 -- G_TRI1, opcode 0xBF.  378 bytes, Glide-only.
 *
 * The three index bytes are read as cl=[esi+6], al=[esi+4], dl=[esi+5] and
 * scaled by 104 (`lea` x2 then `shl 3`, i.e. *13*8), which pins the vertex
 * stride at 0x68 independently of br_dl.h.  The push order at 0x1001ED53 and
 * again at 0x1001EE15 puts byte 6 first under cdecl, so the triangle is
 * (p[6], p[5], p[4]).
 *
 * All three exits return p + 8.  Two of them read the argument back off the
 * stack at DIFFERENT displacements -- [esp+0x24] at 0x1001ED61, where three
 * arguments for the clipper are still pushed, and [esp+0x18] at 0x1001EE5D,
 * where they are not.  Same argument; the displacement alone does not say so.
 */
/* The matching-build definition of 0x1001ECF0 sits below BrDlTriFlatZ, in
 * the index-form block; the port body is at the #else. */
/* Everything the port factored out is INLINE in the original, and there is a
 * lot of it: br_dlcmd_tri, br_dlcmd_finish_vtx three times over, and the state
 * pointer itself. 113 instructions against 18.
 *
 *  - ONE argument. The vertex pool is the absolute global at 0x105CE318 with
 *    a 0x68 stride (`lea` x2 then `shl 3`), so there is no BrDlCmd to pass.
 *  - The two texture scales are absolute FLOAT globals (`fmul dword ptr
 *    [0x118ED1A4]`), not fields, and not doubles.
 *  - NO counters, NO bound check on the index bytes and NO null test before
 *    either sink call -- all four are port additions. The reject arm is a
 *    bare `jne` to the epilogue.
 *  - Both sinks are direct calls.
 *
 * The AND/OR asymmetry is the original's and is preserved: the AND is formed
 * from the second and third outcodes and only then tested against the first,
 * while the OR is formed the other way round.
 *
 * Tri1 is BYTE-EXACT (2026-09-04) in the index form below BrDlTriFlatZ; the
 * pointer form here is what Tri2 still uses.  Tri2 after the __stdcall draw
 * fix: 624 B against 696, 527 diffs.  !! Tri2 in the SAME index form as Tri1
 * (two BR_DLCMD_TRI_I instantiations, indices assigned a, b, c per triangle,
 * with either shared or six distinct int locals) is WORSE: 743 B, +7 insns,
 * regnorm 17+10, and the frame differs from the first byte -- the original
 * `push ecx`es a u slot and then spills the first triangle's oc_a into the
 * dead ARGUMENT slot ([esp+0x18]) and the AND result into the u slot, with
 * `mov ebx,[esp+0xc]` holding p in ebx for the whole function; ours keeps p
 * in ebp and never spills.  The lever is the register pressure of the FIRST
 * triangle (why the original runs out of registers there), not the form.
 *
 * RESIDUE (pointer form, both handlers, before the index form): Tri1 108
 * insns against 113, -49 bytes, 8+13 regnorm; Tri2 202 against 199, -56,
 * 33+30. Two allocator choices:
 *   - the original keeps the SCALED BYTE OFFSET in a register and re-forms
 *     `base + offset` at every access (`mov edi,[ecx+0x105CE338]`,
 *     `lea eax,[ecx+0x105CE318]`), where this keeps the pointer;
 *   - and it reads all three index bytes UP FRONT
 *     (`xor ecx,ecx / xor eax,eax / mov cl,[esi+6] / mov al,[esi+4] /
 *      xor edx,edx / mov dl,[esi+5]`) before scaling any of them, where this
 *     interleaves each load with its own scaling.
 * PROBED: writing the whole thing in INDEX form (`pool[i].field`, no pointer
 * locals) gets the instruction count to 114 against 113 but costs 94 bytes of
 * SIB addressing -- worse overall, do not re-run. Naming the three indices in
 * the original's 6/4/5 read order moved 1 regnorm and is kept.
 *
 * Was -337 bytes and 18 instructions before the helpers came inline. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x105CE318, stride 0x68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118ED1A4 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118ED1A8 */
/* BrDlClipTri: prototype in br_funcs.h */
/* 0x100729EA is the glide2x grDrawTriangle import thunk (config/brally/fenced.csv),
 * and Glide is __stdcall: no `add esp` follows the call in any original.
 * Declared cdecl it costs an `add esp,0xc` after every draw. */
/* grDrawTriangle: prototype in br_funcs.h */
#include "br_dlview.h"
#define BrDlDrawTri BR_DL_TRI   /* grDrawTriangle, its view first (br_dlview.h) */

/* DWORD-PUN stores. The original writes each value ONCE to the temp and
 * then copies it to both TMUs with integer movs (`fstp dword [esp+0x10];
 * mov edi,[esp+0x10]; mov [v+0x30],edi; mov [v+0x24],edi`), and the oow
 * copy is integer movs too. Written as plain float assignments VC5 emits
 * `fst`/`fstp` straight to the two fields and the temp never gets a slot. */
#define BR_DL_PUN(dst, src)                                          \
    (*(uint32_t *)(void *)&(dst) = *(const uint32_t *)(const void *)&(src))

#define V(i) (*(i))

#define BR_DLCMD_FINISH_VTX(i, u_)                                  \
    do {                                                            \
        BR_DL_PUN(V(i).tmu1[2], V(i).oow);                          \
        BR_DL_PUN(V(i).tmu0[2], V(i).oow);                          \
        (u_) = V(i).s * DAT_118ed1a4 * V(i).oow;                 \
        BR_DL_PUN(V(i).tmu1[0], (u_));                              \
        BR_DL_PUN(V(i).tmu0[0], (u_));                              \
        (u_) = V(i).t * DAT_118ed1a8 * V(i).oow;                 \
        BR_DL_PUN(V(i).tmu1[1], (u_));                              \
        BR_DL_PUN(V(i).tmu0[1], (u_));                              \
    } while (0)

#define BR_DLCMD_TRI(ia, ib, ic, u_)                                    \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTriZ(&V(ia), &V(ib), &V(ic));                     \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX(ia, u_);                             \
                BR_DLCMD_FINISH_VTX(ib, u_);                             \
                BR_DLCMD_FINISH_VTX(ic, u_);                             \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* The clip node br_dl.h describes, overlaid on a vertex at +0x40: a link and
 * then cx cy cz s t. Only s and t are read here, and the callers hand over
 * `(char *)v + 0x40` -- the vertex's OWN node -- so the two arguments are two
 * views of one vertex. */
typedef struct BrDlClipSt {
    float aLink[4];          /* +0x00 next, +0x04..+0x0C  cx cy cz */
    float s, t;              /* +0x10 +0x14                        */
} BrDlClipSt;

/* WHAT IT DOES: finish one vertex's texture coordinates before it is drawn.
 * Both texture units get the same thing: s and t divided through by w -- which
 * is what multiplying by the vertex's stored 1/w does -- and scaled by the
 * bound texture's texel size, plus that 1/w itself as the third component so
 * the rasteriser can interpolate perspective-correctly. This is the same work
 * BR_DLCMD_FINISH_VTX above does inline; the original kept one out-of-line
 * copy for the vertices whose s and t come from a clip node. */
/* @implements 0x1001FCF0 glide BrDlVtxFinishTex */
void BrDlVtxFinishTex(BrDlVtx *v, const BrDlClipSt *pSt)
{
    float u;

    BR_DL_PUN(v->tmu1[2], v->oow);
    BR_DL_PUN(v->tmu0[2], v->oow);
    u = pSt->s * DAT_118ed1a4 * v->oow;
    BR_DL_PUN(v->tmu1[0], u);
    BR_DL_PUN(v->tmu0[0], u);
    u = pSt->t * DAT_118ed1a8 * v->oow;
    BR_DL_PUN(v->tmu1[1], u);
    BR_DL_PUN(v->tmu0[1], u);
}

/* The matching-build BrDlCmdTri1 is defined below BrDlTriFlatZ, in the
 * index-form block it shares with it. */

/* 0x1001FA30 -- G_TRI2, opcode 0xB1.  696 bytes, Glide-only.
 *
 * Two triangles, (p[2], p[1], p[0]) then (p[6], p[5], p[4]).  The first
 * triangle's reject path jumps to 0x1001FBAA and its clip path jumps there
 * too (0x1001FAB9) -- and 0x1001FBAA is where the SECOND triangle starts, not
 * the epilogue.  So neither outcome skips the second triangle, which is the
 * one thing about this handler that is not obvious from its shape.
 *
 * Four exits, all `lea eax,[ebx+8]`; ebx holds the argument throughout and is
 * never reused, which is why this handler needs no reload. */
/* WHAT IT DOES: draws two triangles from one command -- the packing the game
 * uses for most of its geometry, since flat surfaces come in pairs. Each is
 * dropped, trimmed or drawn on its own, and whatever happens to the first the
 * second is still considered. */
/* !! RESIDUE, 2026-09-05.  The body below is the POINTER form and is 624 B
 * against the original's 696 with 527 diffs.  Two measurements now say what
 * it needs, and both were made by scratch-compiling copies (build/probe/):
 *
 *  1. IT WANTS THE INDEX FORM, like its single-triangle sibling.  The
 *     original scales each command byte into the pool and reaches fields as
 *     `[reg + 0x105CE318+off]`; the pointer form loses that.  Rewritten as
 *     two BR_DLCMD_TRI_I invocations with per-triangle block-scoped indices
 *     (ia=p[2], ib=p[1], ic=p[0]; then p[6], p[5], p[4] -- the read order the
 *     original uses), it goes to 743 B / 464 diffs / 206 insns against 199,
 *     with only FOUR divergence regions: the prologue's register choice, the
 *     two s/t loads still in pointer form, and one `fmul` pair whose operands
 *     are the other way round (`fmul [texScale]` then `fmul [oow]` in the
 *     original).  A working copy is build/probe/tri2_indexform_KEEP.c.
 *
 *  2. !! IT CANNOT LIVE IN THIS FILE.  Adding that body here as a third user
 *     of BR_DLCMD_TRI_I UN-MATCHES BOTH of its siblings: 0x1001ECF0 goes from
 *     byte-exact to 22 differing bytes and 0x10020900 to 258.  The surrounding
 *     translation unit decides the codegen, so 0x1001FA30 needs its OWN .c
 *     with the macros duplicated verbatim -- the same duplicated-LEAF pattern
 *     br_dl.c and br_dlcmd.c already use for the quarter-pixel snap.
 *
 * So the next pass is: copy build/probe/tri2_indexform_KEEP.c to a new
 * src/brally/core/drawing/br_dltri2.c, move this tag to it, and work the four
 * regions.  Do NOT re-try the pointer form and do NOT put it in this file. */
/* 2026-09-09: the TAG moved to src/brally/core/drawing/br_dltri2.c (index form,
 * macros duplicated), as this note prescribes.  THE POINTER BODY BELOW IS
 * KEPT ON PURPOSE, untagged, under its own name: removing or renaming it
 * takes 0x10020900 BrDlCmdTri1NoZ from byte-exact to 31 diffs and
 * 0x10020D70 from 57 to 465 (measured 2026-09-09) -- the surrounding TU,
 * symbol names included, decides those functions' codegen.  Dead code in
 * every build; the port arm is the #else below. */
/* BrDlCmdTri2: the placed body is br_dltri2.c */

/* ====================================================================
 * 0x1001FF60 -- the FLAT-shaded triangle emitter, z-buffered.
 *
 * The same shape as BR_DLCMD_TRI above, with three differences, and they are
 * the whole function:
 *
 *  - it takes three vertex INDICES as ordinary int arguments, not a command
 *    pointer, so the four G_TRI*_FLAT handlers (0x1001FEF0, 0x100203F0,
 *    0x10020CF0, 0x10020D30) can share it after permuting the bytes
 *    themselves.  The arguments are read BACKWARDS -- `[esp+0xc]` first.
 *  - the clip arm calls its own six-argument helper, not BrDlClipTri: the
 *    trimmer has to be told the flat colour separately, because the vertex it
 *    would otherwise read it from is the one being overwritten.
 *  - the flat shading itself, which is why the function exists.
 *
 * THE COLOUR SAVE/RESTORE IS NOT A PORT ADDITION.  The vertex pool is shared
 * and b and c are still referenced by later commands in the same display
 * list, so their own r/g/b must come back after the draw.  Six dword locals
 * hold them and VC5 puts THREE OF THEM IN THE INCOMING ARGUMENT SLOTS -- the
 * frame is `sub esp,0xc`, three dwords only, because the three parameters are
 * dead the moment they have been scaled, and the float temp `u` shares the
 * arg-3 slot with the first of them.  The frame is already exactly right; do
 * not add a seventh local to "make room".
 *
 * ADDRESSING, and it is genuinely a SPLIT -- measured, three ways:
 *   - all-pointer form (`BrDlVtx *a = &pool[i]`):  +2 insns but -105 bytes,
 *     regnorm 47+45.  Loses the three `shl R,3` and 18 `mov R,[R+A]`.
 *   - all-index form (`pool[i].field` throughout): +11 insns, +119 bytes,
 *     regnorm 36+25.  Wins the `shl`s, loses 18 `mov [R+I],R` on the stores.
 *   - the split below: +4 insns, regnorm 12+8.
 * This is the OPPOSITE of what BrDlCmdTri1 needs, and the reason is the index
 * itself: there it is a command BYTE, so `pool[p[6]]` re-reads it at every
 * access and pays for SIB; here it is an int PARAMETER, so VC5 scales it once
 * into a register and reaches every field as `[reg + 0x105CE318+off]`. */
#undef V
#define V(i) g_aBrDlVtxPool[i]

/* ONE POINTER, AND ONLY FOR THE STORES.  The finish block is the inlined
 * BrDlVtxFinishTex, whose first parameter is a BrDlVtx*, so its eight WRITES
 * and the `oow` it multiplies by go through a `lea`d pointer (`[eax+0x38]`),
 * while `s` and `t` -- which reach it as the SECOND parameter, a clip node at
 * vertex+0x40 -- stay folded into the scaled index (`fld [ebx+0x105CE368]`). */
#define BR_DLCMD_FINISH_VTX_I(i, u_)                                \
    do {                                                            \
        BrDlVtx *pv_ = &V(i);                                       \
        uint32_t w_;                                                \
        BR_DL_PUN(w_, V(i).oow);                                    \
        BR_DL_PUN(pv_->tmu1[2], w_);                                \
        BR_DL_PUN(pv_->tmu0[2], w_);                                \
        (u_) = V(i).s * DAT_118ed1a4 * pv_->oow;                 \
        BR_DL_PUN(pv_->tmu1[0], (u_));                              \
        BR_DL_PUN(pv_->tmu0[0], (u_));                              \
        (u_) = V(i).t * DAT_118ed1a8 * pv_->oow;                 \
        BR_DL_PUN(pv_->tmu1[1], (u_));                              \
        BR_DL_PUN(pv_->tmu0[1], (u_));                              \
    } while (0)

/* BrDlClipTriFlatZ: prototype in br_funcs.h */
/* BrDlClipTriFlatNoZ: prototype in br_funcs.h */

/* WHAT IT DOES: draws one flat-shaded triangle, z-buffered, from three
 * vertex-pool indices. It is dropped if all three corners are off the same
 * edge of the screen, handed to the trimmer if any one of them is off screen,
 * and otherwise all three corners are forced to the FIRST corner's colour --
 * which is what makes it flat -- and it goes to the card. The other two
 * corners' own colours are put back afterwards, because the vertex pool is
 * shared and later commands still expect to find them there. */
/* BYTE-EXACT 2026-09-04.  Was parked at 592 B / 268 diffs with the frame,
 * the two-epilogue shape, the AND/OR asymmetry and the finish blocks all
 * verified; the last 34 bytes were THREE separate source facts, none of
 * them in the old "OPEN" list:
 *   1. the finish macro read `V(i).oow` twice, once per TMU store, and the
 *      first store goes through the `pv_` POINTER -- VC5 cannot prove a
 *      store through a pointer does not alias the indexed global, so it
 *      reloaded (+3 `mov R,[R+A]`).  The original loads it ONCE: pun it
 *      into a dword temp and store the temp twice.  592 -> 561 B.
 *   2. the draw call is the glide2x grDrawTriangle thunk and Glide is
 *      __stdcall; declared cdecl, ours emitted `add esp,0xc` after the
 *      call.  The original's only `add esp,0xc` is the frame.  561 -> 558.
 *   3. the accumulator of the `&`/`|` chains is picked by the LOCALS'
 *      DECLARATION ORDER, not by the expression: `int32_t oc0, oc1, oc2;`
 *      copies oc2 into ebp and tests oc0; `oc2, oc1, oc0` -- the order the
 *      arguments are read -- copies oc0 and tests oc2, as the original.
 *      Re-associating the `&` is inert (VC5 canonicalises the chain).
 * The old "clip call spelled wrong" lead was a misread of fact 2's `add
 * esp`; the three float arguments push fine as `float`. */
/* @implements 0x1001FF60 glide BrDlTriFlatZ */
void BrDlTriFlatZ(int i0, int i1, int i2)
{
    float    u;
    uint32_t br, bg, bb;
    uint32_t cr, cg, cb;
    uint32_t t;
    int32_t  oc2, oc1, oc0;    /* declaration order picks the accumulator */

    oc2 = V(i2).outcode;
    oc1 = V(i1).outcode;
    oc0 = V(i0).outcode;

    if ((oc2 & (oc0 & oc1)) != 0) {
        return;
    }
    if ((oc0 | oc1 | oc2) != 0) {
        BrDlClipTriFlatZ(&V(i0), &V(i1), &V(i2),
                         V(i0).n0, V(i0).n1, V(i0).n2);
        return;
    }

    BR_DLCMD_FINISH_VTX_I(i0, u);
    BR_DLCMD_FINISH_VTX_I(i1, u);
    BR_DLCMD_FINISH_VTX_I(i2, u);

    BR_DL_PUN(br, V(i1).r);
    BR_DL_PUN(bg, V(i1).g);
    BR_DL_PUN(bb, V(i1).b);
    BR_DL_PUN(cr, V(i2).r);
    BR_DL_PUN(cg, V(i2).g);
    BR_DL_PUN(cb, V(i2).b);

    BR_DL_PUN(t, V(i0).r);
    BR_DL_PUN(V(i1).r, t);
    BR_DL_PUN(V(i2).r, t);
    BR_DL_PUN(t, V(i0).g);
    BR_DL_PUN(V(i1).g, t);
    BR_DL_PUN(V(i2).g, t);
    BR_DL_PUN(t, V(i0).b);
    BR_DL_PUN(V(i1).b, t);
    BR_DL_PUN(V(i2).b, t);

    BrDlDrawTri(&V(i0), &V(i1), &V(i2));

    BR_DL_PUN(V(i1).r, br);
    BR_DL_PUN(V(i1).g, bg);
    BR_DL_PUN(V(i1).b, bb);
    BR_DL_PUN(V(i2).r, cr);
    BR_DL_PUN(V(i2).g, cg);
    BR_DL_PUN(V(i2).b, cb);
}

/* 0x10020460 -- the same emitter with the depth buffer OFF.  Byte-for-byte
 * the body above except for two source facts, both read off the 37 bytes in
 * which the two originals differ: the indices are consumed i0, i2, i1
 * (`[esp+4]` is loaded first, where 0x1001FF60 loads `[esp+0xc]` first),
 * which also moves the dead-argument-slot temps down by 8; and the clip arm
 * calls the no-Z trimmer 0x10020690.  The outcode locals are declared in
 * the read order, as above, which is what picks oc1 as the `&`/`|`
 * accumulator and oc0 as the tested operand. */
/* WHAT IT DOES: draws one flat-shaded triangle with the depth buffer off,
 * from three vertex-pool indices.  Dropped if all three corners are off the
 * same edge of the screen, handed to the no-Z trimmer if any one of them is
 * off screen, otherwise all three corners take the FIRST corner's colour and
 * it goes to the card; the other two corners' own colours are restored
 * afterwards because later commands still read them from the shared pool. */
/* @implements 0x10020460 glide BrDlTriFlatNoZ */
void BrDlTriFlatNoZ(int i0, int i1, int i2)
{
    float    u;
    uint32_t br, bg, bb;
    uint32_t cr, cg, cb;
    uint32_t t;
    int32_t  oc0, oc2, oc1;    /* declaration order picks the accumulator */

    oc0 = V(i0).outcode;
    oc2 = V(i2).outcode;
    oc1 = V(i1).outcode;

    if ((oc0 & (oc1 & oc2)) != 0) {
        return;
    }
    if ((oc1 | oc2 | oc0) != 0) {
        BrDlClipTriFlatNoZ(&V(i0), &V(i1), &V(i2),
                           V(i0).n0, V(i0).n1, V(i0).n2);
        return;
    }

    BR_DLCMD_FINISH_VTX_I(i0, u);
    BR_DLCMD_FINISH_VTX_I(i1, u);
    BR_DLCMD_FINISH_VTX_I(i2, u);

    BR_DL_PUN(br, V(i1).r);
    BR_DL_PUN(bg, V(i1).g);
    BR_DL_PUN(bb, V(i1).b);
    BR_DL_PUN(cr, V(i2).r);
    BR_DL_PUN(cg, V(i2).g);
    BR_DL_PUN(cb, V(i2).b);

    BR_DL_PUN(t, V(i0).r);
    BR_DL_PUN(V(i1).r, t);
    BR_DL_PUN(V(i2).r, t);
    BR_DL_PUN(t, V(i0).g);
    BR_DL_PUN(V(i1).g, t);
    BR_DL_PUN(V(i2).g, t);
    BR_DL_PUN(t, V(i0).b);
    BR_DL_PUN(V(i1).b, t);
    BR_DL_PUN(V(i2).b, t);

    BrDlDrawTri(&V(i0), &V(i1), &V(i2));

    BR_DL_PUN(V(i1).r, br);
    BR_DL_PUN(V(i1).g, bg);
    BR_DL_PUN(V(i1).b, bb);
    BR_DL_PUN(V(i2).r, cr);
    BR_DL_PUN(V(i2).g, cg);
    BR_DL_PUN(V(i2).b, cb);
}


/* G_TRI1 in the same INDEX + pointer-finish form as BrDlTriFlatZ above.  The
 * three command bytes become int indices, every outcode and clip-call access
 * is `pool[i].field` off the scaled offset kept in a register (the original's
 * `lea [reg+0x105CE318]` re-forming), and the finish stores go through the
 * lea'd pointer with ONE oow load.  BYTE-EXACT 2026-09-04; the pointer form
 * this replaced (`BrDlVtx *a = &pool[p[6]]`) was parked at 322 B / 258 diffs.
 *
 * !! THE INDEX READ ORDER IS NOT THE SOURCE ORDER.  The original reads the
 * bytes 6, 4, 5 (ecx, eax, edx).  With int locals, `ia = p[6]; ic = p[4];
 * ib = p[5];` compiles to reads 4, 6, 5 -- VC5 swaps the first two -- and
 * `ic = p[4]; ia = p[6]; ib = p[5];` gives the original's 6, 4, 5.  With the
 * old pointer locals the source order WAS the read order.  Also measured:
 * `float u` declared before the ints is what puts the three in ecx/eax/edx
 * (after them: 380 B, one `mov R,R` copy); the declaration order of the
 * ints themselves is inert; naming the three outcodes in locals (the
 * BrDlTriFlatZ lever) is WORSE here (377 B, -3 insns, regnorm 5+8);
 * re-associating the `&` or permuting the `|` is byte-identical. */
#define BR_DLCMD_TRI_I(ia, ib, ic, u_)                                  \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTriZ(&V(ia), &V(ib), &V(ic));                     \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX_I(ia, u_);                           \
                BR_DLCMD_FINISH_VTX_I(ib, u_);                           \
                BR_DLCMD_FINISH_VTX_I(ic, u_);                           \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* WHAT IT DOES: the G_TRI1 display-list command -- draw one triangle from
 * the three vertex-pool indices packed in the command's bytes 6, 5 and 4.
 * It is dropped if all three corners are off the same screen edge, handed
 * to the trimmer if any corner is off screen, and otherwise each corner's
 * texture coordinates are finished and it goes to the card.  Returns the
 * pointer to the next 8-byte command. */
/* @implements 0x1001ECF0 glide BrDlCmdTri1 */
const uint8_t *BrDlCmdTri1(const uint8_t *p)
{
    float u;                    /* ONE slot, shared by all three vertices */
    int   ic = p[4];            /* source order 4, 6, 5 reads 6, 4, 5 */
    int   ia = p[6];
    int   ib = p[5];

    BR_DLCMD_TRI_I(ia, ib, ic, u);
    return p + 8;
}

/* The same three-corner body as BR_DLCMD_TRI_I, with the no-Z trimmer in the
 * clip arm.  Spelled as a second macro rather than a parameter because the
 * original's clip arm is a direct `call rel32` to a different address, not an
 * indirect call through anything. */
/* BrDlClipTriNoZ: prototype in br_funcs.h */

#define BR_DLCMD_TRI_I_NOZ(ia, ib, ic, u_)                              \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTriNoZ(&V(ia), &V(ib), &V(ic));                  \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX_I(ia, u_);                           \
                BR_DLCMD_FINISH_VTX_I(ib, u_);                           \
                BR_DLCMD_FINISH_VTX_I(ic, u_);                           \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* 0x10020900 -- G_TRI1 with the depth buffer OFF.  The same 380-byte body as
 * 0x1001ECF0 above, differing in 37 bytes that are entirely accounted for by
 * two source facts: the clip arm calls the no-Z trimmer 0x10020A80, and the
 * three command bytes are consumed 5, 6, 4 rather than 6, 4, 5.
 *
 * THE READ ORDER IS SET BY DECLARATION ORDER, AND THE FLOAT TEMP IS PART OF
 * IT.  `int ic, ib, ia` (values 4, 5, 6) gets all three byte loads right but
 * schedules `push edi` one slot early, ahead of the third `xor`; moving the
 * shared float temp `u` from the top of the block to BETWEEN ic and ib fixes
 * exactly that, and the function is byte-exact.  A float local costs no
 * register, so it does not look like it should touch the integer prologue --
 * but it takes part in the same declaration walk, and it is the only lever
 * that moved those two bytes (spare int locals, `unsigned` indices, one-line
 * declarations and separated assignments were all probed and are inert or
 * worse).  See docs/brally/VC5-IDIOMS.md, "a float local reorders the integer
 * prologue". */
/* WHAT IT DOES: the G_TRI1 display-list command with the depth buffer off --
 * draw one triangle from the three vertex-pool indices in the command's
 * bytes 6, 5 and 4.  Dropped if all three corners are off the same screen
 * edge, handed to the no-Z trimmer if any corner is off screen, otherwise
 * each corner's texture coordinates are finished and it goes to the card.
 * Returns the pointer to the next 8-byte command. */
/* @implements 0x10020900 glide BrDlCmdTri1NoZ */
const uint8_t *BrDlCmdTri1NoZ(const uint8_t *p)
{
    int   ic = p[4];            /* source order 4, 5, 6 reads 5, 6, 4 */
    float u;                    /* ONE slot, shared by all three vertices */
    int   ib = p[5];
    int   ia = p[6];

    BR_DLCMD_TRI_I_NOZ(ia, ib, ic, u);
    return p + 8;
}

/* 0x10020D70 -- G_TRI2 with the depth buffer off: the no-Z body twice, the
 * first triangle from bytes 0, 1, 2 and the second from bytes 4, 5, 6, each
 * consumed low byte first and drawn third-first (2, 1, 0 and 6, 5, 4).  The
 * one asymmetry is in the second triangle's draw arm: its LAST corner (byte
 * 4) is finished by the out-of-line BrDlVtxFinishTex against its own clip
 * node instead of inline, so that arm is a third macro. */
/* The second triangle's first two corners: the same finish, but the
 * `s * scale` product is NAMED so it is formed before the multiply by 1/w
 * (the flat three-term product canonicalises the other way round here --
 * `s * oow * scale` -- where in the first triangle it does not). */
#define BR_DLCMD_FINISH_VTX_I_N(i, u_)                              \
    do {                                                            \
        BrDlVtx *pv_ = &V(i);                                       \
        uint32_t w_;                                                \
        float    ts_;                                               \
        BR_DL_PUN(w_, V(i).oow);                                    \
        BR_DL_PUN(pv_->tmu1[2], w_);                                \
        BR_DL_PUN(pv_->tmu0[2], w_);                                \
        ts_ = V(i).s * DAT_118ed1a4;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[0], (u_));                              \
        BR_DL_PUN(pv_->tmu0[0], (u_));                              \
        ts_ = V(i).t * DAT_118ed1a8;                             \
        (u_) = ts_ * pv_->oow;                                      \
        BR_DL_PUN(pv_->tmu1[1], (u_));                              \
        BR_DL_PUN(pv_->tmu0[1], (u_));                              \
    } while (0)

#define BR_DLCMD_TRI_I_NOZ_TEX(ia, ib, ic, u_)                          \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTriNoZ(&V(ia), &V(ib), &V(ic));                  \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX_I_N(ia, u_);                         \
                BR_DLCMD_FINISH_VTX_I_N(ib, u_);                         \
                BrDlVtxFinishTex(&V(ic), (const BrDlClipSt *)(const void *)&V(ic).f40); \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

/* WHAT IT DOES: the G_TRI2 display-list command with the depth buffer off --
 * two triangles, from the vertex-pool indices in the command's bytes 2, 1, 0
 * and 6, 5, 4.  Each is dropped if all three corners are off the same screen
 * edge, handed to the no-Z trimmer if any corner is off screen, and otherwise
 * finished and sent to the card; the second triangle's last corner is
 * finished through the clip-node path.  Returns the pointer to the next
 * 8-byte command. */
/* Residue: the second triangle's second corner reads oow through the lea'd
 * vertex pointer (`mov ecx,[edi+0x20]`, 3 B) where ours keeps the scaled
 * index form (`[eax+0x105CE338]`, 6 B).  Same address.  Named-pointer
 * spellings that land the 3-byte form spill a slot (ib4/ib5 +9 B, extra
 * `push ecx`).  A3 pairs the two movs as pointer-vs-index.
 * Dead probes (fn.py): `pv_->oow` for that corner's w_ (spills, +13 B);
 * `(&V(i))->oow` (inert); separate ic2/ib2/ia2 locals (FIRSTDIV moves up);
 * pIb taken before the first triangle (+13 B, REGNORM 6+0); pIb taken
 * after the first triangle, function-scope or `register` (+9 B). */
/* @t4-pass 0x10020D70 1 2026-09-12 probes 12 bytes 688 insns 194 regions 1 rows 2 census yes */
/* @t4-pass 0x10020D70 2 2026-09-12 probes 10 bytes 688 insns 194 regions 1 rows 2 census yes */
/* @t3 0x10020D70 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 688/685 insns 194/194 rows 1+1 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one pointer-vs-index load of vertex.oow on the second
 * triangle's second corner (mov [edi+0x20] vs [eax+g_pool+0x20]);
 * identical insn count; named-pointer spellings spill a slot. Dead list
 * in the PARKED note above.
 * Do not reopen before the end-grind. */
/* @implements 0x10020D70 glide BrDlCmdTri2NoZ */
const uint8_t *BrDlCmdTri2NoZ(const uint8_t *p)
{
    int   ic = p[0];
    float u;                    /* ONE slot, shared by all six corners */
    int   ib = p[1];
    int   ia = p[2];

    BR_DLCMD_TRI_I_NOZ(ia, ib, ic, u);

    ic = p[4];
    ib = p[5];
    ia = p[6];
    BR_DLCMD_TRI_I_NOZ_TEX(ia, ib, ic, u);
    return p + 8;
}

/* WHAT IT DOES: draws one flat-shaded z-buffered triangle, permuting the
 * three vertex bytes according to a selector in the command. */
/* @implements 0x1001FEF0 glide BrDlCmdTri1FlatZ */
unsigned char *BrDlCmdTri1FlatZ(unsigned char *p)
{
    switch (p[7]) {
    case 0:
        BrDlTriFlatZ(p[6], p[5], p[4]);
        return p + 8;
    case 1:
        BrDlTriFlatZ(p[5], p[4], p[6]);
        return p + 8;
    default:
        BrDlTriFlatZ(p[4], p[6], p[5]);
        return p + 8;
    }
}

/* WHAT IT DOES: draws one flat-shaded triangle with the z-buffer off,
 * permuting the three vertex bytes according to a selector in the command. */
/* @implements 0x100203F0 glide BrDlCmdTri1Flat */
unsigned char *BrDlCmdTri1Flat(unsigned char *p)
{
    switch (p[7]) {
    case 0:
        BrDlTriFlatNoZ(p[6], p[5], p[4]);
        return p + 8;
    case 1:
        BrDlTriFlatNoZ(p[5], p[4], p[6]);
        return p + 8;
    default:
        BrDlTriFlatNoZ(p[4], p[6], p[5]);
        return p + 8;
    }
}

/* WHAT IT DOES: draws two flat-shaded z-buffered triangles from one
 * command, vertex bytes 0..2 then 4..6. */
/* @implements 0x10020CF0 glide BrDlCmdTri2FlatZ */
unsigned char *BrDlCmdTri2FlatZ(unsigned char *p)
{
    BrDlTriFlatZ(p[2], p[1], p[0]);
    BrDlTriFlatZ(p[6], p[5], p[4]);
    return p + 8;
}

/* WHAT IT DOES: draws two flat-shaded triangles with the z-buffer off,
 * vertex bytes 0..2 then 4..6. */
/* @implements 0x10020D30 glide BrDlCmdTri2Flat */
unsigned char *BrDlCmdTri2Flat(unsigned char *p)
{
    BrDlTriFlatNoZ(p[2], p[1], p[0]);
    BrDlTriFlatNoZ(p[6], p[5], p[4]);
    return p + 8;
}

#undef V
#define V(i) (*(i))

/* ====================================================================
 * 0x1001E320 -- G_FILLRECT, opcode 0xF6.  96 bytes; the body is shared with
 * the D3D build, where the same code sits at 0x1001BE30.
 *
 * 10.2 FIXED POINT.  The evidence, and the contrast with 0xE1's integers, is
 * in br_dlcmd.h; the mechanics are:
 *
 *     shl edi,0x14 / sar edi,0x16 / and edi,0x3FF
 *
 * `shl 20` then `sar 22` is a net arithmetic shift right of 2 on a
 * sign-extended 12-bit field -- the fixed-point divide.  `and 0x3FF` then
 * discards the sign extension, so the whole sequence is provably just
 * `(w >> 2) & 0x3FF`, and it is written that way here: the arithmetic shift
 * is unobservable and open-coding it would invite the reader to think it is
 * not.  The X field is `shl 8 / sar 22 / and 0x3FF`, a net >> 14, i.e. bits
 * 23:14 -- the integer part of a 10.2 value living in bits 23:12.
 *
 * Corner assignment, and it is the opposite of br_dl.c's untextured arm:
 * w0 carries the LOWER-RIGHT corner and w1 the UPPER-LEFT.  Read it off the
 * flips -- `sub ebx,edi` uses the w1 Y and becomes the fourth argument (the
 * MAXIMUM), `sub edx,ecx / dec edx` uses the w0 Y and becomes the second (the
 * MINIMUM).  With screen Y increasing downward and Glide's increasing upward,
 * only w1 = upper-left makes those two land the right way round.
 *
 * The X maximum gets `inc edi`, so it is exclusive; the Y pair gets the
 * `-1` on the minimum, which is the same exclusivity after the flip.  Both
 * spans are therefore (corner difference + 1) pixels. */
/* (port-only BrDlCmdFillRect removed) */


/* ====================================================================
 * 0x1001E9F0 -- G_SETFILLCOLOR, opcode 0xF7.  110 bytes.
 *
 * PAIRS WITH D3D 0x1001CC00.  Glide 0x1001CC00 is RallyMain, which is ported
 * as BrRallyMain; the two are different functions that happen to share a
 * number across the two images.  br_dlcmd.h has the full warning.
 *
 * The body is three copies of the bitfield-insert idiom
 *     dl = cl ^ (((cl ^ dh) & 7))      i.e.   (cl & 0xF8) | (dh & 7)
 * which is the standard 5->8 channel expansion `(v << 3) | (v >> 2)` written
 * so that both halves come out of one register pair.  Note it decodes the LOW
 * halfword of w1 only: a 32-bit fill colour is two RGBA5551 pixels and this
 * build reads one.
 *
 * Alpha is `and cl,1 / neg cl / sbb ecx,ecx / and ecx,0xFF` -- the classic
 * "spread bit 0 to all eight", giving 0 or 255, not 0 or 1.
 *
 * All four destinations are read by the rect drawer 0x1001E380 at 0x1001E441,
 * which is the arm taken whenever the latched combiner is NOT the prim-colour
 * row.  So 0xF7 is the colour 0xF6 fills with. */
/* (port-only BrDlCmdFillColour removed) */


/* ====================================================================
 * 0x1001EA60 -- G_SETFOGCOLOR, opcode 0xF8.  Glide-only.
 *
 * Nineteen bytes: load w1, call grFogColorValue through thunk 0x100729F6,
 * `lea eax,[esi+8]`.  It stores NOTHING -- there is no fog-colour global on
 * this path, the value lives in the Glide driver.  (br_dl.c keeps one; that
 * is its own model, not a transcription.) */
/* WHAT IT DOES: sets the colour that distant scenery fades towards. The colour
 * is passed straight to the graphics card and this build keeps no copy of it
 * of its own. */
/* @implements 0x1001EA60 glide BrDlCmdFogColour */
/* @n64 0x8023DF00 located */
/* Literal: one stdcall into the driver with the raw dword at p+4. */
/* grFogColorValue: prototype in br_funcs.h */
const uint8_t *BrDlCmdFogColour(const uint8_t *p)
{
    grFogColorValue(*(const int *)(const void *)(p + 4));
    return p + 8;
}

/* Forward declarations for unknown functions/globals */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* grConstantColorValue: prototype in br_funcs.h */

/* WHAT IT DOES: handle the display-list command that sets the primitive
 * colour. Unpacks the packed 32-bit value into four separate float channels
 * the renderer keeps, and hands the packed form straight to the 3dfx
 * constant-colour register. Returns the pointer to the next command. */
/* @implements 0x1001EA80 glide br_dl_prim */
const uint8_t *br_dl_prim(const uint8_t *p)

{
  unsigned int w1 = *(const unsigned int *)(const void *)(p + 4);
  BrGbiRectG_5D17A4 = (float)(w1 >> 0x18);
  BrGbiRectG_5D17B4 = (float)(w1 >> 0x10 & 0xff);
  BrGbiRectG_5CE2D0 = (float)(w1 >> 8 & 0xff);
  BrGlPrimA = (float)(w1 & 0xff);
  grConstantColorValue((int)w1);
  return p + 8;
}

/* ====================================================================
 * 0x1001EA80 -- G_SETPRIMCOLOR, opcode 0xFA.  138 bytes, Glide-only.
 *
 * Four `fild qword` conversions with the high dword explicitly zeroed, each
 * `fstp dword` straight into its global.  THERE IS NO SCALE: the stored
 * floats are 0..255.  Contrast 0xFB below, which multiplies by 1/255 -- the
 * two handlers sit 350 bytes apart and differ in exactly that.
 *
 * Corroboration that 0..255 is right rather than an oversight: br_dl.h
 * records 0x105D17A4 / 0x105D17B4 / 0x105CE2D0 as the lit transform's
 * "no lights at all" fallback colour, those are this handler's R/G/B, and
 * BR_DL_COLOUR_MAX -- the Glide build's iterated-colour ceiling -- is 255.0f.
 *
 * The tail is grConstantColorValue(w1) through thunk 0x10072996, passing the
 * RAW word.  port/src/gfx/metal/br_gfx_metal.m already records that Glide's
 * GrColor_t is R,G,B,A in byte order for this call. */
/* (port-only BrDlCmdPrimColour removed) */


/* ====================================================================
 * 0x1001E930 -- G_SETENVCOLOR, opcode 0xFB.  183 bytes.
 *
 * Same four bytes, same order, but each goes
 *     fild qword; fstp dword [scratch]; fld [scratch]; fmul [0x10077400]
 * and 0x10077400 is 0x3B808081 == the float nearest 1/255.  So env colour is
 * 0..1 while prim colour is 0..255.
 *
 * The spill through the stack slot between the fild and the fmul is a real
 * rounding to float; it is reproduced rather than folded because the source
 * values are small integers where it happens to be exact, and a later reader
 * should not have to re-derive that.
 *
 * `add eax,8` sits at 0x1001E9C4, in the MIDDLE of the function -- before the
 * alpha channel is even converted.  The return value is still p + 8; the
 * scheduler simply hoisted it. */
/* (port-only BrDlCmdEnvColour removed) */


/* ====================================================================
 * 0x1001E770 -- G_SETCOMBINE, opcode 0xFC.  36 bytes; the body is shared with
 * the D3D build, where the same code sits at 0x1001C7F0.
 *
 * Latch, then apply.  Both stores precede the call, and that ordering is
 * load-bearing: 0x1001E380 compares 0x105D17AC / 0x105D17B0 against the
 * prim-colour row to decide which of two colour sources a fill rectangle
 * uses, and 0x1001E7A0 can reach code that draws.
 *
 * The classification chain itself is 0x1001E7A0 and is NOT this function;
 * br_dl.c's BrDlClassifyCombine already models it and enumerates the ten
 * recognised (w0, w1) pairs. */
/* WHAT IT DOES: chooses the recipe by which texture, lighting and flat colour
 * are mixed together for everything drawn from here on. It remembers the
 * choice as well as applying it, because the rectangle filler later checks
 * which recipe is in force to decide where its colour comes from. */
/* @implements 0x1001E770 glide BrDlCmdSetCombine */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1001e7a0: prototype in br_funcs.h */
const uint8_t *BrDlCmdSetCombine(const uint8_t *p)
{
    int w0, w1;

    w0 = *(const int *)(const void *)p;
    (*(int *)&BrGlCombineW0) = w0;
    w1 = *(const int *)(const void *)(p + 4);
    (*(int *)&BrGlCombineW1) = w1;
    BrGlSetCombine(w0, w1);
    return p + 8;
}

/* ==================================================================== */
/* wiring                                                               */
/* ==================================================================== */

/* (port-only BrDlCmdInit removed) */


/* The nine slots this module owns, out of the 28 the table at 0x100A9A58
 * fills.  Everything else -- including the other nineteen -- answers NULL, so a
 * caller cannot silently get the wrong handler for a byte. */
/* (port-only BrDlCmdLookup removed) */

