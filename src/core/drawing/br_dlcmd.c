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
static uint32_t br_dlcmd_w(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read a float out of a host-order 32-bit pattern without aliasing. */
static float br_dlcmd_f32(uint32_t v)
{
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
}

/* `fistp dword ptr [...]` under MSVC's startup control word: round to
 * NEAREST, ties to EVEN.  Out of range -- and NaN -- stores the x87 integer
 * indefinite, 0x80000000.
 *
 * This is NOT __ftol, which truncates and returns 0 out of range; the two
 * appear within a few hundred bytes of each other in this binary (0x10074560
 * is _ftol and is what 0x1001E380 uses) and confusing them has cost time
 * here before.  rint() honours the default rounding mode, which is the same
 * ties-to-even. */
static int32_t br_dlcmd_fistp(double v)
{
    double r = rint(v);
    /* Written as a negated conjunction so NaN takes the indefinite side --
     * x87 does the same, and `r < min || r > max` would let NaN through. */
    if (!(r >= -2147483648.0 && r <= 2147483647.0))
        return (int32_t)0x80000000;
    return (int32_t)r;
}

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
/* MATCHING STATUS (2026-08-22): NOT MATCHABLE FROM C ALONE.  The original
 * snaps screen coordinates with a bare `fistp [0x105CE310]; fild` pair -- no
 * fnstcw/fldcw around it, so ROUND-TO-NEAREST -- which no VC5 C construct
 * produces: float->int casts emit __ftol calls (truncating), and /QIfist
 * does not exist in VC5 (warning D4002).  The original source used inline
 * __asm (or a hand-asm module) for the snap.  Matching this function needs
 * an __asm-hybrid mechanism (SM64 GLOBAL_ASM-style) plus: /Oy- (the
 * original keeps an EBP frame), a ONE-argument signature (arg at [ebp+8] is
 * p; w1 is used directly as the source pointer), the 0x68-stride vertex
 * array as a GLOBAL at 0x105CE318, and the combined matrix as float[16] at
 * 0x105D1760.  The port body below is deliberately armored (resolver hook,
 * clamps, counters) and is NOT the matching shape. */
/* @implements 0x10021A20 glide BrDlCmdVtx */
const uint8_t *BrDlCmdVtx(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w0 = br_dlcmd_w(p);
    uint32_t w1 = br_dlcmd_w(p + 4);
    int n  = (int)((w0 >> 10) & 0x3Fu);
    int v0 = (int)((w0 >> 16) & 0xFFu);
    const uint8_t *pSrc;
    const float *m = &pS->combined.m[0][0];
    int i;

    pS->cVtxLoads++;

    /* 0x10021A49 `jle 0x10021C5F` -- the jump target IS the `lea eax,[esi+8]`,
     * so this path shares the one exit.  Note the source pointer is never
     * even dereferenced here, which is why an unresolvable w1 with n == 0 is
     * not an error. */
    if (n <= 0)
        return p + 8;

    pSrc = pS->sink.pfnResolve
         ? pS->sink.pfnResolve(pS->sink.pUser, w1, (size_t)n * 0x20u)
         : NULL;
    if (pSrc == NULL) {
        /* DEVIATION: the original dereferences w1 unconditionally and would
         * fault.  A test must not be able to crash, so the port counts and
         * advances -- and still returns p + 8, which is the property under
         * test. */
        pS->cVtxUnresolved++;
        return p + 8;
    }

    for (i = 0; i < n; ++i) {
        BrDlVtx *pV;
        double cx, cy, cz, cw;
        float x, y, z;
        int32_t oc = 0;
        int k = v0 + i;

        /* DEVIATION.  The original has NO bound: `edi` starts at
         * 0x105CE318 + 0x68*v0 with v0 an unchecked byte and the loop just
         * walks, so a malformed list writes past the array.  br_dl.c clamps
         * for the same reason.  Counted so a test can tell "clamped" from
         * "did not happen". */
        if (k >= BR_DL_VTX_COUNT) {
            pS->cVtxClamped++;
            break;
        }
        pV = &pS->aVtx[k];

        x = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x00));
        y = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x04));
        z = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x08));

        /* 0x10021A55..0x10021AF0.  Row-vector: out = v * M, translation in
         * row 3 -- y multiplies m[4..7], z multiplies m[8..11].
         *
         * THE SUMMATION ORDER IS THE ORIGINAL'S AND IS NOT THE OBVIOUS ONE.
         * Each component is built as
         *     fld m[4+j]; fmul y      -- st0 = m[4+j]*y
         *     fld m[8+j]; fmul z      -- st0 = m[8+j]*z, st1 = the above
         *     faddp st(1)             -- st1 += st0, so (m4*y + m8*z)
         *     fld x;      fmul m[0+j]
         *     faddp st(1)             -- + m0*x
         *     fadd m[12+j]            -- + m12
         * i.e. ((m4*y + m8*z) + m0*x) + m12, with x LAST of the three
         * products.  Float addition is not associative; this is transcribed,
         * not tidied. */
        cx = (((double)m[4] * y + (double)m[8]  * z) + (double)m[0]  * x) + m[12];
        cy = (((double)m[5] * y + (double)m[9]  * z) + (double)m[1]  * x) + m[13];
        cz = (((double)m[6] * y + (double)m[10] * z) + (double)m[2]  * x) + m[14];
        cw = (((double)m[7] * y + (double)m[11] * z) + (double)m[3]  * x) + m[15];

        pV->cx = (float)cx;
        pV->cy = (float)cy;
        pV->cz = (float)cz;
        pV->cw = (float)cw;

        /* 0x10021AF2..0x10021B0D: five verbatim dword copies. */
        pV->s  = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x0C));
        pV->t  = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x10));
        pV->n0 = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x14));
        pV->n1 = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x18));
        pV->n2 = br_dlcmd_f32(br_dlcmd_w(pSrc + 0x1C));

        /* 0x10021B10..0x10021BA4, the seven frustum tests, inlined here and
         * called out of line by the lit transforms (br_dl.c's
         * br_dl_outcode == 0x10022120).  Every one is
         *     fld <expr>; fcomp 0.0 (0x10077410); fnstsw ax; test ah,1
         * and `test ah,1` is C0 == STRICTLY LESS THAN.  An x87 UNORDERED
         * compare sets C0 as well, so NaN takes the true side -- which is
         * what `!(v >= 0)` reproduces and what `v < 0` would too, but the
         * negated form is the house style because it survives someone later
         * rewriting the operand order.  The bit ORDER is the original's. */
        if (!((double)pV->cw >= 0.0))                    oc |= BR_DL_CLIP_W;
        if (!((double)pV->cz + pV->cw >= 0.0))           oc |= BR_DL_CLIP_NEAR;
        if (!((double)pV->cw - pV->cz >= 0.0))           oc |= BR_DL_CLIP_FAR;
        if (!((double)pV->cx + pV->cw >= 0.0))           oc |= BR_DL_CLIP_LEFT;
        if (!((double)pV->cw - pV->cx >= 0.0))           oc |= BR_DL_CLIP_RIGHT;
        if (!((double)pV->cy + pV->cw >= 0.0))           oc |= BR_DL_CLIP_BOTTOM;
        if (!((double)pV->cw - pV->cy >= 0.0))           oc |= BR_DL_CLIP_TOP;
        pV->outcode = oc;

        pS->cVtxWritten++;
        pSrc += 0x20;

        /* 0x10021BA7 `jne` -- anything outside skips the projection entirely,
         * so x/y/oow/r/g/b keep whatever the PREVIOUS use of this slot left
         * there.  The clipper is what reads such a vertex, and it reads only
         * cx/cy/cz/cw/s/t/n0/n1/n2, all of which were just written. */
        if (oc != 0)
            continue;

        {
            /* 0x10021BAD.  `fld 1.0 / fdiv cw / fstp [ebp-4]` -- the spill is
             * a real rounding to float, and the value is then reloaded twice
             * (once from [ebp-4] for X, once from the vertex's +0x20 for Y),
             * so both axes see the same float. */
            float invW = (float)(1.0 / (double)pV->cw);
            double sx, sy;

            pV->oow = invW;

            sx = ((double)pS->vpScaleX * invW) * pV->cx + pS->vpTransX;
            sy = ((double)pS->vpScaleY * pV->oow) * pV->cy + pS->vpTransY;
            pV->x = (float)sx;
            pV->y = (float)sy;

            /* 0x10021BE7: r/g/b are the source's trailing three floats,
             * copied verbatim.  For an UNLIT model they are the Vtx's last
             * three bytes scaled by 1/128, not a colour at all -- br_dl.h's
             * BrDlColourScale exists to say which of the two a consumer is
             * looking at. */
            pV->r = pV->n0;
            pV->g = pV->n1;
            pV->b = pV->n2;

            /* 0x10021BF9..0x10021C48, the quarter-pixel snap, twice.
             *   fld x; fmul 4.0; fstp [ebp-4]        <- rounds to float
             *   fld [ebp-4]; fistp [0x105CE310]      <- ties to EVEN
             *   fild [0x105CE310]; fstp [ebp-4]      <- back to float
             *   fld [ebp-4]; fmul 0.25; fstp x
             * 0x105CE310 is a global, so it survives the call and is
             * modelled; it ends holding the LAST value written, which is the
             * last transformed vertex's Y. */
            {
                float q = (float)((double)pV->x * 4.0);
                pS->snapScratch = br_dlcmd_fistp((double)q);
                q = (float)pS->snapScratch;
                pV->x = (float)((double)q * 0.25);
            }
            {
                float q = (float)((double)pV->y * 4.0);
                pS->snapScratch = br_dlcmd_fistp((double)q);
                q = (float)pS->snapScratch;
                pV->y = (float)((double)q * 0.25);
            }

            pS->cVtxTransformed++;
        }
    }

    /* 0x10021C5B..0x10021C5F: `mov esi,[ebp+8]` reloads the ARGUMENT (esi was
     * clobbered as the outcode accumulator) and `lea eax,[esi+8]` returns it
     * plus eight.  Same exit as the n == 0 path. */
    return p + 8;
}

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
static void br_dlcmd_finish_vtx(BrDlCmd *pS, BrDlVtx *pV)
{
    float u;

    pV->tmu1[2] = pV->oow;
    pV->tmu0[2] = pV->oow;

    u = (float)(((double)pV->s * pS->texScaleS) * pV->oow);
    pV->tmu1[0] = u;
    pV->tmu0[0] = u;

    u = (float)(((double)pV->t * pS->texScaleT) * pV->oow);
    pV->tmu1[1] = u;
    pV->tmu0[1] = u;
}

/* The three-way decision both triangle handlers make, byte for byte the same
 * in each: 0x1001ED35..0x1001ED5C and 0x1001FA7A..0x1001FAB1 and again at
 * 0x1001FBEC..0x1001FC1B.
 *
 * NOTE THE ASYMMETRY, preserved because it is visible: the AND is formed from
 * the SECOND and THIRD outcodes and only then tested against the first, while
 * the OR is formed the other way round.  Same value either way; the original's
 * register pressure made the order explicit and br_dl.c preserves it too. */
static void br_dlcmd_tri(BrDlCmd *pS, int i0, int i1, int i2)
{
    BrDlVtx *a, *b, *c;
    int32_t and3, or3;

    pS->cTriIn++;

    /* DEVIATION: the original indexes 0x105CE318 with raw command bytes and
     * has no bound at all.  The indices come from the load-time pass, which
     * halved them, so a well-formed list cannot exceed the array. */
    if (i0 >= BR_DL_VTX_COUNT || i1 >= BR_DL_VTX_COUNT || i2 >= BR_DL_VTX_COUNT)
        return;

    a = &pS->aVtx[i0];
    b = &pS->aVtx[i1];
    c = &pS->aVtx[i2];

    and3 = b->outcode & c->outcode;
    if ((a->outcode & and3) != 0) {
        pS->cTriRejected++;
        return;                         /* 0x1001ED3B / 0x1001FA8C / 0x1001FBF6 */
    }

    or3 = b->outcode | c->outcode | a->outcode;
    if (or3 != 0) {
        pS->cTriClipped++;
        if (pS->sink.pfnClipTri)
            pS->sink.pfnClipTri(pS->sink.pUser, a, b, c);
        return;
    }

    br_dlcmd_finish_vtx(pS, a);
    br_dlcmd_finish_vtx(pS, b);
    br_dlcmd_finish_vtx(pS, c);
    pS->cTriDrawn++;
    if (pS->sink.pfnDrawTri)
        pS->sink.pfnDrawTri(pS->sink.pUser, a, b, c);
}

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
/* WHAT IT DOES: draws one triangle from three corner points already loaded.
 * If all three are off the same side of the screen the triangle is dropped; if
 * any of them is off the screen it is handed to the trimmer; otherwise its
 * texture coordinates are finished and it goes to the card. */
/* @implements 0x1001ECF0 glide BrDlCmdTri1 */
#ifdef BR_MATCHING_BUILD
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
 * RESIDUE, both handlers (Tri1: 108 insns against 113, -49 bytes, 8+13
 * regnorm; Tri2: 202 against 199, -56, 33+30). Two allocator choices:
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
extern BrDlVtx g_aBrDlVtxPool[];    /* 0x105CE318, stride 0x68 */
extern float   g_brDlTexScaleS;     /* 0x118ED1A4 */
extern float   g_brDlTexScaleT;     /* 0x118ED1A8 */
extern void    BrDlClipTri(BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);  /* 0x1001EE70 */
extern void    BrDlDrawTri(BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);  /* 0x100729EA */

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
        (u_) = V(i).s * g_brDlTexScaleS * V(i).oow;                 \
        BR_DL_PUN(V(i).tmu1[0], (u_));                              \
        BR_DL_PUN(V(i).tmu0[0], (u_));                              \
        (u_) = V(i).t * g_brDlTexScaleT * V(i).oow;                 \
        BR_DL_PUN(V(i).tmu1[1], (u_));                              \
        BR_DL_PUN(V(i).tmu0[1], (u_));                              \
    } while (0)

#define BR_DLCMD_TRI(ia, ib, ic, u_)                                    \
    do {                                                                \
        if ((V(ia).outcode & (V(ib).outcode & V(ic).outcode)) == 0) {    \
            if ((V(ib).outcode | V(ic).outcode | V(ia).outcode) != 0) {  \
                BrDlClipTri(&V(ia), &V(ib), &V(ic));                     \
            } else {                                                     \
                BR_DLCMD_FINISH_VTX(ia, u_);                             \
                BR_DLCMD_FINISH_VTX(ib, u_);                             \
                BR_DLCMD_FINISH_VTX(ic, u_);                             \
                BrDlDrawTri(&V(ia), &V(ib), &V(ic));                     \
            }                                                            \
        }                                                                \
    } while (0)

const uint8_t *BrDlCmdTri1(const uint8_t *p)
{
    /* The three index bytes are read 6, 4, 5 -- the original's order, and it
     * is not the argument order -- and each is scaled in its own register
     * with an in-place `shl x,3`, so each needs its own named index. */
    BrDlVtx *a = &g_aBrDlVtxPool[p[6]];
    BrDlVtx *c = &g_aBrDlVtxPool[p[4]];
    BrDlVtx *b = &g_aBrDlVtxPool[p[5]];
    float    u;                 /* ONE slot, shared by all three vertices */

    BR_DLCMD_TRI(a, b, c, u);
    return p + 8;
}
#else
const uint8_t *BrDlCmdTri1(BrDlCmd *pS, const uint8_t *p)
{
    br_dlcmd_tri(pS, p[6], p[5], p[4]);
    return p + 8;
}
#endif

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
/* @implements 0x1001FA30 glide BrDlCmdTri2 */
#ifdef BR_MATCHING_BUILD
const uint8_t *BrDlCmdTri2(const uint8_t *p)
{
    BrDlVtx *a0 = &g_aBrDlVtxPool[p[2]];
    BrDlVtx *c0 = &g_aBrDlVtxPool[p[0]];
    BrDlVtx *b0 = &g_aBrDlVtxPool[p[1]];
    BrDlVtx *a1 = &g_aBrDlVtxPool[p[6]];
    BrDlVtx *c1 = &g_aBrDlVtxPool[p[4]];
    BrDlVtx *b1 = &g_aBrDlVtxPool[p[5]];
    float    u;

    BR_DLCMD_TRI(a0, b0, c0, u);
    BR_DLCMD_TRI(a1, b1, c1, u);
    return p + 8;
}
#else
const uint8_t *BrDlCmdTri2(BrDlCmd *pS, const uint8_t *p)
{
    br_dlcmd_tri(pS, p[2], p[1], p[0]);
    br_dlcmd_tri(pS, p[6], p[5], p[4]);
    return p + 8;
}
#endif

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
const uint8_t *BrDlCmdFillRect(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w0 = br_dlcmd_w(p);
    uint32_t w1 = br_dlcmd_w(p + 4);
    int32_t  h  = pS->cyScreen;               /* 0x100A7518 */
    int32_t  ulx, uly, lrx, lry;

    uly = (int32_t)((w1 >>  2) & 0x3FFu);     /* w1 bits 11:0, 10.2 */
    ulx = (int32_t)((w1 >> 14) & 0x3FFu);     /* w1 bits 23:12      */
    lry = (int32_t)((w0 >>  2) & 0x3FFu);     /* w0 bits 11:0       */
    lrx = (int32_t)((w0 >> 14) & 0x3FFu);     /* w0 bits 23:12      */

    pS->cRects++;
    if (pS->sink.pfnFillRect)
        pS->sink.pfnFillRect(pS->sink.pUser, ulx, h - lry - 1, lrx + 1, h - uly);
    return p + 8;
}

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
const uint8_t *BrDlCmdFillColour(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w1 = br_dlcmd_w(p + 4);
    uint8_t  hi, lo;

    hi = (uint8_t)(w1 >> 8);          /* bits 15:8  -- carries R<<3 */
    lo = (uint8_t)(w1 >> 13);         /* bits 15:13 -- carries R>>2 */
    pS->fillR = (uint8_t)((hi & 0xF8u) | (lo & 0x07u));

    hi = (uint8_t)(w1 >> 3);          /* bits 10:3  -- carries G<<3 */
    lo = (uint8_t)(w1 >> 8);          /* bits 10:8  -- carries G>>2 */
    pS->fillG = (uint8_t)((hi & 0xF8u) | (lo & 0x07u));

    /* `and cl,0xFE / shl cl,2` is an EIGHT-BIT shift: bits 6 and 7 of the
     * masked byte fall off the end, which is what leaves room for the low
     * three.  Reproduced by truncating to uint8_t after the shift. */
    hi = (uint8_t)((uint8_t)(w1 & 0xFEu) << 2);
    lo = (uint8_t)(w1 >> 3);          /* bits 5:3   -- carries B>>2 */
    pS->fillB = (uint8_t)(hi | (lo & 0x07u));

    pS->fillA = (uint8_t)((w1 & 1u) ? 0xFFu : 0x00u);

    return p + 8;                     /* 0x1001EA4E `add eax,8` */
}

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
#ifdef BR_MATCHING_BUILD
/* Literal: one stdcall into the driver with the raw dword at p+4. */
void __stdcall grFogColorValue(int);
const uint8_t *BrDlCmdFogColour(const uint8_t *p, BrDlCmd *pS)
{
    grFogColorValue(*(const int *)(const void *)(p + 4));
    return p + 8;
}
#else
const uint8_t *BrDlCmdFogColour(BrDlCmd *pS, const uint8_t *p)
{
    if (pS->sink.pfnFogColor)
        pS->sink.pfnFogColor(pS->sink.pUser, br_dlcmd_w(p + 4));
    return p + 8;
}
#endif

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
const uint8_t *BrDlCmdPrimColour(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w1 = br_dlcmd_w(p + 4);

    pS->primR = (float)(int32_t)((w1 >> 24) & 0xFFu);   /* 0x105D17A4 */
    pS->primG = (float)(int32_t)((w1 >> 16) & 0xFFu);   /* 0x105D17B4 */
    pS->primB = (float)(int32_t)((w1 >>  8) & 0xFFu);   /* 0x105CE2D0 */
    pS->primA = (float)(int32_t)(w1 & 0xFFu);           /* 0x105CD9F0 */

    if (pS->sink.pfnConstantColor)
        pS->sink.pfnConstantColor(pS->sink.pUser, w1);
    return p + 8;
}

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
const uint8_t *BrDlCmdEnvColour(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w1 = br_dlcmd_w(p + 4);
    const float k = 1.0f / 255.0f;    /* 0x10077400 == 0x3B808081 exactly */

    pS->envR = (float)((double)(float)(int32_t)((w1 >> 24) & 0xFFu) * k);
    pS->envG = (float)((double)(float)(int32_t)((w1 >> 16) & 0xFFu) * k);
    pS->envB = (float)((double)(float)(int32_t)((w1 >>  8) & 0xFFu) * k);
    pS->envA = (float)((double)(float)(int32_t)(w1 & 0xFFu) * k);

    return p + 8;
}

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
#ifdef BR_MATCHING_BUILD
extern int DAT_105d17ac;
extern int DAT_105d17b0;
void FUN_1001e7a0(int, int);
const uint8_t *BrDlCmdSetCombine(const uint8_t *p, BrDlCmd *pS)
{
    int w0, w1;

    w0 = *(const int *)(const void *)p;
    DAT_105d17ac = w0;
    w1 = *(const int *)(const void *)(p + 4);
    DAT_105d17b0 = w1;
    FUN_1001e7a0(w0, w1);
    return p + 8;
}
#else
const uint8_t *BrDlCmdSetCombine(BrDlCmd *pS, const uint8_t *p)
{
    uint32_t w0 = br_dlcmd_w(p);
    uint32_t w1 = br_dlcmd_w(p + 4);

    pS->combineW0 = w0;               /* 0x105D17AC */
    pS->combineW1 = w1;               /* 0x105D17B0 */

    if (pS->sink.pfnCombine)
        pS->sink.pfnCombine(pS->sink.pUser, w0, w1);
    return p + 8;
}
#endif

/* ==================================================================== */
/* wiring                                                               */
/* ==================================================================== */

void BrDlCmdInit(BrDlCmd *pS, int32_t cyScreen)
{
    int i;

    memset(pS, 0, sizeof(*pS));
    for (i = 0; i < 4; ++i)
        pS->combined.m[i][i] = 1.0f;
    /* 0x118ED1A4 / 0x118ED1A8 are the texture binder's business; unity is
     * what br_dl.c holds them at and what an unbound tile behaves as. */
    pS->texScaleS = 1.0f;
    pS->texScaleT = 1.0f;
    pS->cyScreen  = cyScreen;
}

/* The nine slots this module owns, out of the 28 the table at 0x100A9A58
 * fills.  Everything else -- including the other nineteen -- answers NULL, so a
 * caller cannot silently get the wrong handler for a byte. */
BrDlCmdFn BrDlCmdLookup(unsigned op)
{
    switch (op) {
    case 0x04: return BrDlCmdVtx;
#ifdef BR_MATCHING_BUILD
    /* One-argument in the matching arm; the table's type is the port's. */
    case 0xB1: return (BrDlCmdFn)BrDlCmdTri2;
    case 0xBF: return (BrDlCmdFn)BrDlCmdTri1;
#else
    case 0xB1: return BrDlCmdTri2;
    case 0xBF: return BrDlCmdTri1;
#endif
    case 0xF6: return BrDlCmdFillRect;
    case 0xF7: return BrDlCmdFillColour;
    case 0xF8: return BrDlCmdFogColour;
    case 0xFA: return BrDlCmdPrimColour;
    case 0xFB: return BrDlCmdEnvColour;
    case 0xFC: return BrDlCmdSetCombine;
    default:   return NULL;
    }
}
