/* br_mat4.c -- geometry: 4x4 transforms applied to points and directions.
 *
 * Responsibility: positions, orientations and the arithmetic that moves them.
 * br_mat.c and br_mat3.c build and combine matrices; this module applies one
 * to a point or a direction.
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

/* 0x1003B3F0. Moved from src/core/slice2_21.c (an address batch) unchanged.
 * POSITION IN THE TU IS LOAD-BEARING: it must sit FIRST in this file, ahead
 * of both BrMat4TransformPoint4 and BrMat4TransformPoint -- anywhere else
 * the allocator drops a fxch (this residue) or costs one of the other two
 * its byte-exact match. */
/* WHAT IT DOES: puts a direction through a transform. Unlike a point, a
 * direction is only rotated and scaled and never moved, so the transform's
 * position part is deliberately left out.
 * RESIDUE (8 bytes, RAW/REGNORM 0+0): pV->x/y/z read once each into locals
 * closes 85 diffs to 8 (the original keeps each component live across its
 * three uses instead of reloading from memory); what remains is a pure
 * fxch/store-order permutation at +0x32. Probed and inert (declaration
 * order/type qualifiers/register hint/parenthesization/pointer-vs-array
 * matrix access/statement order, 20 variants, all byte-identical). Corpus
 * MISS at +0x32 len 12: not proven anywhere in the solved tree. */
/* @t4-pass 0x10034A70 1 2026-09-13 probes 10 bytes 115 insns 49 regions 1 rows 0 census yes  (hand, fn.py variants: x/y/z declaration order and type, register/const qualifiers, parenthesization, pointer-vs-array m[][] access; corpus MISS at +0x32 len 12) */
/* @t4-pass 0x10034A70 2 2026-09-13 probes 10 bytes 115 insns 49 regions 1 rows 0 census no  (hand, fn.py variants: term order per row, output-statement order, (*pOut).x spelling, all inert) */
/* @t3 0x10034A70 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 115/115 insns 49/49 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Pure fxch/store-order permutation at +0x32, register-blind gap 0+0 --
 * VC5 keeps x/y/z live across their three uses in a different schedule than
 * the original; 20 spellings tried, none move it. Do not reopen before the
 * end-grind (CLAUDE.md rule 12). */
/* @implements 0x1003B3F0 d3d BrMtxXfmDir3 */
/* @implements 0x10034A70 glide BrMtxXfmDir3 */
void BrMtxXfmDir3(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM)
{
    float x = pV->x, y = pV->y, z = pV->z;
    pOut->x = pM->m[0][0] * x + pM->m[1][0] * y + pM->m[2][0] * z;
    pOut->y = pM->m[0][1] * x + pM->m[1][1] * y + pM->m[2][1] * z;
    pOut->z = pM->m[0][2] * x + pM->m[1][2] * y + pM->m[2][2] * z;
}

/* 0x1003B2A0 -- signature deliberately matches slice2_18.h's XSLICE
 * declaration (a bare `const float *` matrix) so the two link. Moved from
 * src/core/slice2_21.c (an address batch) unchanged. */
/* WHAT IT DOES: puts a point through a transform -- moving, rotating and
 * scaling it in one step -- and keeps the fourth component, which is what the
 * perspective divide later needs. */
/* @implements 0x1003B2A0 d3d BrMat4TransformPoint4 */
/* @implements 0x10034920 glide BrMat4TransformPoint4 */
void BrMat4TransformPoint4(float pOut[4], const BrVec3 *pV, const float *pM)
{
    /* Orig is four unrolled columns, not a j<4 loop (66 B vs 157 B). x/y/z
     * cached in locals: the original keeps each live across its four uses
     * instead of reloading pV->x/y/z from memory each time. */
    float x = pV->x, y = pV->y, z = pV->z;
    pOut[0] = pM[0] * x + pM[4] * y + pM[8]  * z + pM[12];
    pOut[1] = pM[1] * x + pM[5] * y + pM[9]  * z + pM[13];
    pOut[2] = pM[2] * x + pM[6] * y + pM[10] * z + pM[14];
    pOut[3] = pM[3] * x + pM[7] * y + pM[11] * z + pM[15];
}

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
