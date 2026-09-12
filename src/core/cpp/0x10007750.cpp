/* WHAT IT DOES: reads a car's state sent as a difference from an earlier
 * one. Facing comes through in full; position, height and one other field
 * arrive as low bits plus a two-bit hint that nudges the high part up one
 * step, up two, or down one. Everything the packet does not mention is left
 * as the caller had it, so the caller must seed the record from the
 * reference first. */
/* @implements 0x10007750 glide BrCarStateDecodeDelta
 * @cpp_symbol _BrCarStateDecodeDelta
 *
 * The C transcription (slice1_02.c, tagged at the D3D twin 0x100073E0)
 * diverges two ways (fn.py multiset 2026-09-12): the thiscall reader shim
 * (surplus push/add-esp pairs, missing `mov ecx,r` -- the family's C++-TU
 * evidence, see the 0x10006510 / 0x10006BA0 dossiers), and the delta-merge
 * helper compiled OUT OF LINE where the original inlines it at all four
 * sites (missing cmp/jne/jmp/and webs, surplus `call`).  This file is the
 * C++ TU's shape: native-thiscall reader declared only, extern "C" cdecl
 * quantisers, and the merge helper __inline so VC5 expands it as the
 * original does.  Body otherwise the slice1_02.c transcription verbatim.
 *
 * STATE 2026-09-12: 844/845 B, register-blind multiset 1+1 -- the ONLY
 * residue is f18's double: ours `shl R,1` in place, the original
 * `lea eax,[esi+esi]` into a fresh register.  Probed dead: arg-expression
 * form, assign-back-to-prev, fresh `bits` variable, signed operands,
 * *2u vs x+x (VC5 value-numbers them all to the in-place shift; the lea
 * needs the source register to stay live, and nothing after f18 reads it).
 * Levers that DID land here, for the family: per-arm `prev & keepMask`
 * with code==0 tested first (four ands, adds folded to the code register);
 * short-typed pack prototype so `>> 1` narrows to `sar ax,1`; the bare
 * lea double passed unmasked (the movsx inside the callee narrows). */
#include <stdint.h>

class BrBitReader {
public:
    uint32_t ReadBits(unsigned nBits);   /* thiscall, declared only */
};

extern "C" {
float   BrFixUnpackS16Q15Neg(int32_t v);
float   BrFixUnpackU32Q13(uint32_t v);
float   BrFixUnpackS16Q7(int32_t v);
float   BrFixUnpackS24Q1(uint32_t v);
float   BrFixUnpackU8Range(int32_t v);
float   BrFixUnpackLevel(int32_t v);
int32_t BrFixPackU24Q13(float v);
short   BrFixPackS16Q7(float v);   /* int32 in the C tree; short here so `>> 1` narrows to `sar ax,1` */
int32_t BrFixPackS24Q1(float v);
}

struct BrCarState {
    float f00, f04, f08, f0C;
    float f10, f14, f18, f1C, f20;
    float f24;
    float f28, f2C, f30, f34, f38, f3C;
    float f40, f44, f48;
    float f4C, f50, f54, f58;
    float f5C, f60, f64, f68;
    float f6C, f70, f74, f78, f7C, f80, f84;
    float f88, f8C, f90, f94, f98, f9C;
};

#define BR_ONE_128  128.0f

/* The delta code shared by f10, f14, f18 and f78: the transmitted word
 * carries a 2-bit code in its top bits and the new low bits underneath; the
 * code adjusts the retained high part by {0, +step, +2*step, -step} -- the
 * fourth case is a SUBTRACT.  The original never re-masks after the
 * add/subtract; that wraparound is load-bearing for the s16 fields, which is
 * why this is all unsigned arithmetic. */
static __inline uint32_t BrCarStateDeltaMerge(uint32_t prev, uint32_t bits,
                                              uint32_t codeMask, uint32_t step,
                                              uint32_t keepMask, uint32_t lowMask)
{
    uint32_t code = bits & codeMask;
    uint32_t hi;

    /* The original recomputes `prev & keepMask` INSIDE each arm, code==0
     * tested first -- four copies of the and, adds folded to the code
     * register. Hoisting it emits one and above the chain instead. */
    if (code == 0u)
        hi = prev & keepMask;
    else if (code == step)
        hi = (prev & keepMask) + step;
    else if (code == step * 2u)
        hi = (prev & keepMask) + step * 2u;
    else
        hi = (prev & keepMask) - step;

    /* Compound |= keeps the result in hi's register (`or esi,eax`), where a
     * fresh `hi | (bits & lowMask)` lands it in the masked-bits register. */
    hi |= bits & lowMask;
    return hi;
}

extern "C"
void BrCarStateDecodeDelta(BrCarState *pDst, const BrCarState *pRef,
                           BrBitReader *pReader)
{
    uint32_t prev, bits;

    pDst->f00 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f04 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f08 = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));
    pDst->f0C = BrFixUnpackS16Q15Neg((int32_t)((pReader->ReadBits(8) & 0xFFu) << 8));

    /* f10/f14: re-quantise the reference to unsigned Q13-in-24, drop the low 7
     * bits (`shr esi,7`, a LOGICAL shift) to get a 17-bit value, then merge
     * 12 transmitted low bits under a 2-bit page code. */
    prev = (uint32_t)BrFixPackU24Q13(pRef->f10) >> 7;
    bits = pReader->ReadBits(14);
    prev = BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu);
    pDst->f10 = BrFixUnpackU32Q13(prev << 7);

    prev = (uint32_t)BrFixPackU24Q13(pRef->f14) >> 7;
    bits = pReader->ReadBits(14);
    prev = BrCarStateDeltaMerge(prev, bits, 0x3000u, 0x1000u, 0x1F000u, 0xFFFu);
    pDst->f14 = BrFixUnpackU32Q13(prev << 7);

    /* f18: signed Q7-in-16, then `sar ax,1` -- an ARITHMETIC shift on the low
     * 16 bits only -- giving a 15-bit signed value; 9 transmitted low bits. */
    {
        short q;
        q = BrFixPackS16Q7(pRef->f18) >> 1;   /* sar ax,1: short lvalue */
        prev = (uint32_t)(int32_t)q;          /* movsx */
        bits = pReader->ReadBits(11);
        /* `lea eax,[esi+esi]`: the double is passed RAW -- the `movsx ax`
         * inside the unpack is what narrows it, so no mask here (the C tree
         * masks defensively; this TU matches the original's spelling). */
        prev = BrCarStateDeltaMerge(prev, bits, 0x600u, 0x200u, 0x7E00u, 0x1FFu);
        pDst->f18 = BrFixUnpackS16Q7((int32_t)prev + (int32_t)prev);
    }

    /* f78: signed Q1-in-24, kept whole; 7 transmitted low bits. */
    prev = (uint32_t)BrFixPackS24Q1(pRef->f78);
    bits = pReader->ReadBits(9);
    prev = BrCarStateDeltaMerge(prev, bits, 0x180u, 0x80u, 0xFFFF80u, 0x7Fu);
    pDst->f78 = BrFixUnpackS24Q1(prev);

    pDst->f7C = BrFixUnpackU8Range((int32_t)pReader->ReadBits(6));
    pDst->f80 = BrFixUnpackLevel((int32_t)pReader->ReadBits(2));
    pDst->f84 = BrFixUnpackLevel((int32_t)pReader->ReadBits(2));

    pDst->f88 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f8C = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f90 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f94 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f98 = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
    pDst->f9C = pReader->ReadBits(1) ? BR_ONE_128 : 0.0f;
}
