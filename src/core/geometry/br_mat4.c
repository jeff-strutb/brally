/* br_mat4.c -- geometry: 4x4 transforms applied to points.
 *
 * Responsibility: positions, orientations and the arithmetic that moves them.
 * br_mat.c and br_mat3.c build and combine matrices; this module applies one
 * to a point.
 *
 * Moved out of src/core/slice1_09.c (an address batch) unchanged. The
 * preamble below is carried over verbatim from that file, including the
 * matching-build renames that have nothing to do with this code: they decide
 * the set of names the translation unit sees, and trimming them changes the
 * compiler's view of the code.
 */
#ifdef BR_MATCHING_BUILD
/* slice1_09.h declares these cdecl; the originals are thiscall with stack
 * args.  Hide those prototypes so the matching bodies can use __fastcall
 * plus a struct-typed second argument (never register-eligible, so forced
 * onto the stack).  Same split as thiscall; do not redefine BR_THISCALL. */
#define BrBitStreamReadBits  BrBitStreamReadBits_cdecl
#define BrBitStreamInit      BrBitStreamInit_cdecl
#define BrBitStreamSkipBytes BrBitStreamSkipBytes_cdecl
#define BrBitStreamWriteU8   BrBitStreamWriteU8_cdecl
#define BrBitStreamWriteU24  BrBitStreamWriteU24_cdecl
#define BrBitStreamWriteU32  BrBitStreamWriteU32_cdecl
#define BrEntitySetIndex     BrEntitySetIndex_cdecl
#define BrEntityBindAux      BrEntityBindAux_cdecl
#endif
#include "slice1_09.h"
#ifdef BR_MATCHING_BUILD
#undef BrBitStreamReadBits
#undef BrBitStreamInit
#undef BrBitStreamSkipBytes
#undef BrBitStreamWriteU8
#undef BrBitStreamWriteU24
#undef BrBitStreamWriteU32
#undef BrEntitySetIndex
#undef BrEntityBindAux
#endif

#include <math.h>
#include <stddef.h>

/* 0x100747C0.
 * Written out longhand rather than with temporaries so that the write order
 * matches the original exactly: each output component is zeroed and fully
 * accumulated before the next one begins, and the translation row is added
 * to all three only afterwards. That ordering is observable when pOut
 * aliases pV. */
/* WHAT IT DOES: moves a point through a transform: rotates and scales it by
 * the matrix and then adds the matrix's translation. The awkward longhand
 * here is deliberate, because the original writes each result component out
 * before starting the next, which is visible if the caller passes the same
 * point as both input and output. */
/* @implements 0x1006DA20 glide BrMat4TransformPoint */
/* @implements 0x100747C0 d3d BrMat4TransformPoint */
void BrMat4TransformPoint(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    /* Orig is two counted loops (ebp=3 outer, esi=3 inner), not unrolled
     * products: `mov [eax],0`; inner `fld [v]; fmul [m]; add m,0x10; add v,4;
     * dec esi; fadd [eax]; fstp [eax]`.  `sub edi,eax` is pM-pOut so the
     * column pointer is `lea r,[edi+eax]` as eax walks the output.
     *
     * INDEXED, NOT CURSORS.  Hand-rolled walking pointers (`col = m; v = pV;`
     * bumped by `col += 4; v++`) reproduce this exactly to 7 bytes and then
     * stop: VC5 binds the copy-from-register cursor to ecx and the lea-derived
     * one to edx, where the original has them the other way round, and the lea
     * comes out `[eax+edi]` instead of `[edi+eax]`.  Swapping the assignment
     * order, swapping the declaration order and block-scoping the pair inside
     * the outer loop all fail (9 / 7 / 5 diffs) -- a previous note here called
     * this a register-allocation wall and told the reader not to grind it, and
     * that was WRONG.  Letting the compiler build both induction variables
     * itself, from plain `pv[j]` and `pM->m[j][i]` subscripts, is byte-exact:
     * the two cursors then come into existence in the order VC5 wants them
     * and pick up ecx/edx accordingly.  Semantics are unchanged -- `pv[j]`
     * re-reads the live vector every outer pass, exactly as the reloaded
     * cursor did, which is what keeps the aliasing case above honest. */
    float       *o  = (float *)pOut;
    const float *pv = (const float *)pV;
    int i, j;

    for (i = 0; i < 3; i++) {
        o[i] = 0.0f;
        for (j = 0; j < 3; j++)
            o[i] += pv[j] * pM->m[j][i];
    }
    pOut->x += pM->m[3][0];
    pOut->y += pM->m[3][1];
    pOut->z += pM->m[3][2];
}
