/* @implements 0x10006510 glide BrCarStateEncode
 * @cpp_symbol _BrCarStateEncode
 *
 * NOT YET A MATCH -- 151 reloc-masked diff bytes, all from ONE residue: at
 * each of ~30 write sites the original pushes nBits BEFORE the quantiser
 * call (in-place right-to-left argument evaluation) AND shifts the returned
 * value at 16-bit width (`sar ax,8; movsx ecx,ax`).  Six controlled
 * experiments (build/match/sched.cpp probes, 2026-08-29) pin VC5's rules:
 *   pure expression arg  -> push-early BUT movsx-then-sar32 (wide shift)
 *   assignment-in-arg    -> sar ax + movsx (narrow) BUT push-late
 *     (any side effect in an arg makes VC5 pre-evaluate it before pushes;
 *      __inline helpers count as assignments -- the inliner's temp)
 *   short value PARAM    -> sar ax narrow BUT no movsx (pushes eax raw)
 * The original combines push-early WITH the narrow shift.  15+ spelling
 * probes (dead temps, comma forms, functional/ref casts, short/int param
 * permutations, /G3-/G6, /Za, /Os, /Op, /Ob, /GX on/off) show the two are
 * MUTUALLY EXCLUSIVE under the staged RTM front end: narrowing fires only at
 * an assignment, and any assignment in an argument forces pre-evaluation.
 * Corpus cross-check: 11 MATCHED functions carry the hoisted-constant-push
 * motif and every one passes PURE call expressions as arguments
 * (BrCountedNetSend 0x10004A40 is the clean witness), consistent with the
 * rule.  LEADING HYPOTHESIS: the shipped binaries (March 1999, VS97 SP3
 * era) were compiled by an SP-patched front end.  TESTED AND DISPROVEN
 * 2026-08-30: VS97 SP3 (vs97sp3 @ archive.org; staged tools/msvc5/bin-sp3,
 * C1XX/C2 dated 1997-11-03) and VC6 RTM 12.00.8168 (vs6.iso @ archive.org;
 * staged tools/msvc6/) both apply EXACTLY the same two rules -- assignment
 * pre-evaluation and assignment-gated narrowing -- byte-for-byte on the
 * two-form battery.  Also probed and negative: struct-by-value returns
 * (the member still promotes AND the return temp counts as a side effect),
 * global/reference/deref object expressions.  Full map: 18 spellings x
 * 3 front ends x 10+ flag sets.  TWIN CHECK 2026-08-30: the D3D build's
 * copy (0x100061A0) is shape-identical (push-early + sar ax + movsx) --
 * stable across both shipped binaries: real compiler output, not
 * post-processing.  Remaining hypotheses: a fourth compiler
 * (VC4.2-era static library?), or a source shape not yet conceived.
 *
 * The C transcription (slice2_12.c) is shape-exact except for 33 surplus
 * `xor edx,edx` -- the __fastcall dead-edx idiom faking the writer's
 * thiscall.  The writer 0x1006D0B0 is `this` in ecx with BOTH arguments on
 * the stack and callee-cleaned (`ret 8`): a C++ member function, so the TU
 * that called it was C++.  This file is that TU's shape: the writer is a
 * declared-not-defined class method (native thiscall), the quantisers stay
 * extern "C" cdecl, and the body is the slice2_12.c transcription verbatim.
 */
class BrBitStream {
public:
    void WriteBits(int value, int nBits);   /* 0x1006D0B0, declared only */
};

extern "C" {
short BrFixPackS16Q15Neg(float);   /* 0x10006950 */
int BrFixPackU24Q13(float);
short BrFixPackS16Q7(float);
short BrFixPackS16Q8(float);
signed char BrFixPackS8Q3(float);
signed char BrFixPackS6Q7Neg(float);
int BrFixPackU8Angle(float);
int BrFixPackS24Q1(float);
int BrFixPackU8Range(float);
int BrFixPackLevel(float);
}

/* VC5 lowers `!= 0.0f` to one `fcomp; fnstsw; test ah,0x40` -- C3 covers
 * EQUAL and UNORDERED, so NaN reads as zero, same as the original. */
static __inline int BrIsNonZero(float v)
{
    return v != 0.0f;
}

#define BrSar16(v, n)  ((short)((v) >> (n)))
#define BrSar8(v, n)   ((signed char)((v) >> (n)))

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

extern "C"
void BrCarStateEncode(BrBitStream *pBs, const BrCarState *pSrc)
{
    short       s;
    signed char c;

    pBs->WriteBits(s = BrSar16(BrFixPackS16Q15Neg(pSrc->f00), 8), 8);
    pBs->WriteBits(s = BrSar16(BrFixPackS16Q15Neg(pSrc->f04), 8), 8);
    pBs->WriteBits(s = BrSar16(BrFixPackS16Q15Neg(pSrc->f08), 8), 8);
    pBs->WriteBits(s = BrSar16(BrFixPackS16Q15Neg(pSrc->f0C), 8), 8);

    pBs->WriteBits((int)((unsigned int)BrFixPackU24Q13(pSrc->f10) >> 7), 17);
    pBs->WriteBits((int)((unsigned int)BrFixPackU24Q13(pSrc->f14) >> 7), 17);
    pBs->WriteBits(s = BrSar16(BrFixPackS16Q7(pSrc->f18), 1), 15);

    pBs->WriteBits((short)BrFixPackS16Q8(pSrc->f1C), 16);
    pBs->WriteBits((short)BrFixPackS16Q8(pSrc->f20), 16);

    pBs->WriteBits(c = BrSar8(BrFixPackS8Q3(pSrc->f28), 3), 5);
    pBs->WriteBits(c = BrSar8(BrFixPackS8Q3(pSrc->f2C), 3), 5);
    pBs->WriteBits(c = BrSar8(BrFixPackS8Q3(pSrc->f30), 3), 5);
    pBs->WriteBits(c = BrSar8(BrFixPackS8Q3(pSrc->f34), 4), 4);
    pBs->WriteBits(c = BrSar8(BrFixPackS6Q7Neg(pSrc->f38), 2), 4);
    pBs->WriteBits((int)((unsigned int)(unsigned char)BrFixPackU8Angle(pSrc->f3C) >> 4), 4);

    pBs->WriteBits(BrIsNonZero(pSrc->f4C), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f50), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f54), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f58), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f6C), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f70), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f74), 1);

    pBs->WriteBits(BrFixPackS24Q1(pSrc->f78), 24);
    pBs->WriteBits((int)((unsigned int)BrFixPackU8Range(pSrc->f7C) & 0xFFu), 6);
    pBs->WriteBits((int)((unsigned int)BrFixPackLevel(pSrc->f80) & 0xFFu), 2);
    pBs->WriteBits((int)((unsigned int)BrFixPackLevel(pSrc->f84) & 0xFFu), 2);

    pBs->WriteBits(BrIsNonZero(pSrc->f88), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f8C), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f90), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f94), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f98), 1);
    pBs->WriteBits(BrIsNonZero(pSrc->f9C), 1);
}
