/* WHAT IT DOES: write a car's state as a difference from a previously sent
 * one, so a routine update costs far fewer bits. The facing goes out in full,
 * but position, height and one other field send only their low bits plus a
 * two-bit hint about how far the high part moved. That hint is deliberately
 * coarse: a jump of three or more steps cannot be reconstructed, a limitation
 * of the original format, not of this transcription. */
/* @implements 0x10006BA0 glide BrCarStateEncodeDelta
 * @cpp_symbol _BrCarStateEncodeDelta
 *
 * The C transcription (slice2_12.c, tagged at the D3D twin 0x10006830) is
 * shape-exact except for the writer's thiscall: 0x1006D0B0 is `this` in ecx
 * with BOTH arguments on the stack and callee-cleaned (`ret 8`) -- a C++
 * member function, so the calling TU was C++.  Under C the __fastcall shim
 * that fakes it emits a dead `xor edx,edx` at every one of the ~18 write
 * sites (the whole size gap).  This file is that TU's shape: the writer is a
 * declared-not-defined native-thiscall class method, the quantisers stay
 * extern "C" cdecl, and the body is the slice2_12.c transcription verbatim.
 * Result under VC5 /O2 /GX /MD: 714 -> 84 reloc-masked diff bytes; the shim
 * is gone and every write site's `mov ecx,pBs; push n; push v; call` matches.
 *
 * STATE 2026-09-10: NOT A MATCH.  The whole 84-byte residue is ONE thing --
 * the original pushes `nBits` BEFORE the inner quantiser call (in-place
 * right-to-left argument construction), while VC5 evaluates the value first
 * and pushes the constant late.  The consequent [esp+0x1c]-vs-[esp+0x18]
 * pBs-load offset is a downstream effect of the same push, not independent.
 * This is EXACTLY the push-early-vs-narrow-shift mutual exclusion the sibling
 * 0x10006510 documents: the `int16_t q` assignment is what gives the narrow
 * `sar ax,8`, and any form that makes the value a pure argument expression
 * (which is what pushes nBits early) widens the shift to `movsx; sar r32`
 * instead -- MEASURED here: the pure-expression form is 729 diffs under VC5.
 * The two are not simultaneously reachable under VC5/SP3/VC6.
 *
 * ‼ VC4.2 (tools/msvc42, cl 10.20.6166) REPRODUCES THE ANOMALY on this second
 * family member: with pure-expression arguments it emits `push 8` before the
 * pack call AND the narrow `sar ax,8` + `movsx` TOGETHER -- the VC5-impossible
 * combination.  This is independent confirmation of the sibling's finding
 * that the bitstream net family is a VC4.2-compiled C++ object linked into an
 * otherwise-VC5 DLL ([[vc42-is-the-real-compiler]]).  It is still NOT a
 * byte-match under VC4.2 at /O2 /Ox /O1 /O2y: register-blind it is 292 insns
 * against the original's 303 (an ~11-insn structural gap) plus prologue
 * register-save order (ebp) and edx/ecx allocation.  Matching this function
 * needs the VC4.2 toolchain wired into the image build for this TU (an
 * architecture decision, not a spelling), plus the `!= 0.0f` global-zero
 * compare idiom for the six tail bit-writes.  Do NOT re-grind VC5 spellings;
 * the wall is the compiler, proven twice.
 *
 * STATE 2026-09-12 (this file now carries the VC4.2-optimal spellings):
 * static-const-float zero (BR_FZERO) for the six tail writes, pure-expression
 * args at the four Q15 sites and both Q7 sites, a named `next` sum in the
 * delta-code guard, code-first `|` at the f78 site, and the LAST tail write
 * as a statement if/else (VC4.2 tail-duplicates its call+epilogue exactly as
 * the original does, 0x379/0x38c).  Under VC4.2 /O2: 298/302 insns, 913/925
 * bytes, 6 divergence regions -- residue is the prologue push order, three
 * 1-2-insn scheduling transpositions (push 2/8 vs the field load, lea+cmp vs
 * sub+cmp in the inlined guard, or-destination) and sites 1-5's bool diamond
 * layout (ours set-then-clear `mov 1; je; xor`, orig `jne; mov 1; jmp; xor`;
 * the ==-inverted ternary and the if/else-assign forms are DEAD -- both
 * canonicalise back, and the inverted form costs +15 B).  ‼ The VC5 score of
 * THIS file regressed 84 -> ~140 by these spellings; that is expected and
 * irrelevant -- the VC5 form is proven impossible, this file's target
 * compiler is VC4.2.  Attribution is NOT yet proven to the byte-exact
 * standard; it rests on the idiom pair plus this 298/302 convergence.
 */
#include <stdint.h>

class BrBitStream {
public:
    void WriteBits(int value, int nBits);   /* 0x1006D0B0, declared only */
};

/* VC4.2 folds `!= 0.0f` to test [mem],0x7fffffff; a static const float
 * forces the original's x87 fld/fcomp/fnstsw form (vc42 idiom #2). */
static const float BR_FZERO = 0.0f;

extern "C" {
short BrFixPackS16Q15Neg(float);   /* 0x10006950 */
int   BrFixPackU24Q13(float);
short BrFixPackS16Q7(float);
int   BrFixPackS24Q1(float);
int   BrFixPackU8Range(float);
int   BrFixPackLevel(float);
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

/* `q` is the 16-bit lvalue that makes VC5 shift in AX: only an assignment to
 * a `short` narrows a promoted `>>` to `sar ax,N`.  A (short) cast, a short
 * parameter or a compound `q >>= N` all widen first (`movsx; sar r32`).
 * Plain `>>` on a signed value is arithmetic on every compiler this tree
 * targets. */
static __inline int32_t BrCarStateDeltaCode(uint32_t cur, uint32_t ref,
                                            uint32_t hiMask, uint32_t step)
{
    uint32_t code;
    uint32_t hiRef, hiCur;

    if (((cur ^ ref) & hiMask) == 0) {
        code = 0;
    } else {
        hiRef = ref & hiMask;
        hiCur = cur & hiMask;

        {
            uint32_t next = hiRef + step;
            if (next == hiCur)
                code = step;                /* exactly one step up */
            else
                code = (hiRef < hiCur) ? 2 * step   /* more than one step up */
                                       : 3 * step;  /* at or below the reference */
        }
    }

    return (int32_t)(code & (3 * step));
}

extern "C"
void BrCarStateEncodeDelta(BrBitStream *pBs, const BrCarState *pCur,
                           const BrCarState *pRef)
{
    int32_t cur, ref;
    int16_t q;

    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f00) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f04) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f08) >> 8, 8);
    pBs->WriteBits(BrFixPackS16Q15Neg(pCur->f0C) >> 8, 8);

    /* f10: 17-bit quantity, 12 bits sent plus a 2-bit code on 0x1F000. */
    ref = (int32_t)((uint32_t)BrFixPackU24Q13(pRef->f10) >> 7);
    cur = (int32_t)((uint32_t)BrFixPackU24Q13(pCur->f10) >> 7);
    pBs->WriteBits(BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);

    ref = (int32_t)((uint32_t)BrFixPackU24Q13(pRef->f14) >> 7);
    cur = (int32_t)((uint32_t)BrFixPackU24Q13(pCur->f14) >> 7);
    pBs->WriteBits(BrCarStateDeltaCode(cur, ref, 0x1F000, 0x1000) | (cur & 0xFFF), 14);

    /* f18: 15-bit SIGNED quantity, 9 bits sent plus a code on 0x7E00. */
    ref = BrFixPackS16Q7(pRef->f18) >> 1;
    cur = BrFixPackS16Q7(pCur->f18) >> 1;
    pBs->WriteBits((cur & 0x1FF) | BrCarStateDeltaCode(cur, ref, 0x7E00, 0x200), 11);

    /* f78: 24-bit signed quantity, 7 bits sent plus a code on 0xFFFF80. */
    ref = BrFixPackS24Q1(pRef->f78);
    cur = BrFixPackS24Q1(pCur->f78);
    pBs->WriteBits(BrCarStateDeltaCode(cur, ref, 0xFFFF80, 0x80) | (cur & 0x7F), 9);

    pBs->WriteBits((int32_t)((uint32_t)BrFixPackU8Range(pCur->f7C) & 0xFFu), 6);
    pBs->WriteBits((int32_t)((uint32_t)BrFixPackLevel(pCur->f80) & 0xFFu), 2);
    pBs->WriteBits((int32_t)((uint32_t)BrFixPackLevel(pCur->f84) & 0xFFu), 2);

    /* Open-coded in the original (`fld; fcomp; fnstsw; test ah,0x40; jne`).
     * VC5 gives 0 for a NaN here, which is what the original does; a strict
     * IEEE compiler would give 1, so this is a byte-fidelity choice. */
    pBs->WriteBits((pCur->f88 != BR_FZERO) ? 1 : 0, 1);
    pBs->WriteBits((pCur->f8C != BR_FZERO) ? 1 : 0, 1);
    pBs->WriteBits((pCur->f90 != BR_FZERO) ? 1 : 0, 1);
    pBs->WriteBits((pCur->f94 != BR_FZERO) ? 1 : 0, 1);
    pBs->WriteBits((pCur->f98 != BR_FZERO) ? 1 : 0, 1);
    if (pCur->f9C != BR_FZERO)
        pBs->WriteBits(1, 1);
    else
        pBs->WriteBits(0, 1);
}
