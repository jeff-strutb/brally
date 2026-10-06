/* WHAT IT DOES: pack a car's full physics state into the compact form sent
 * over the network -- each float narrowed to the fewest bits that still
 * describe it, and a bitmask saying which of them are non-zero so the
 * receiver knows what was sent. */
/* @t3 0x10006510 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1018/1018 insns 350/350 rows 0+0 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * Residue: the push-early/narrow-shift argument schedule at ~30 write sites
 * (identical multiset, no rows). Dossier, the three-front-end map and the
 * ledger are in the block below. Do not reopen before the end-grind.
 */
/* @implements 0x10006510 glide BrCarStateEncode
 * @cpp_symbol _BrCarStateEncode
 *
 * NOT YET A MATCH -- 151 reloc-masked diff bytes, all from ONE residue: at
 * each of ~30 write sites the original pushes nBits BEFORE the quantiser
 * call (in-place right-to-left argument evaluation) AND shifts the returned
 * value at 16-bit width (`sar ax,8; movsx ecx,ax`).  Six controlled
 * experiments (build/brally/win32/match/sched.cpp probes, 2026-08-29) pin VC5's rules:
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
 * 2026-08-30: VS97 SP3 (vs97sp3 @ archive.org; staged tools/toolchains/msvc5/bin-sp3,
 * C1XX/C2 dated 1997-11-03) and VC6 RTM 12.00.8168 (vs6.iso @ archive.org;
 * staged tools/brally/msvc6/) both apply EXACTLY the same two rules -- assignment
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
 *
 * Gate A (t3.py, 2026-09-13): insn gap 0, rows 0+0, four masked regions,
 * no lost-sync -- the whole residue is the push-early/narrow-shift schedule
 * and its register colouring, nothing missing. The three passes below are
 * the sessions documented above plus a 2026-09-13 option pass (/Gi, /G5,
 * /Op, /Ob0, /Oa, /Ow, /Gf, /Gy, /GF, /Zp1: all 151 except /Ob0, worse).
 *
 * @t4-pass 0x10006510 1 2026-08-29 probes 15 bytes 1018 insns 350 regions 4 rows 0 census yes  (build/brally/win32/match/sched.cpp: six controlled push/narrow experiments, 15+ spellings)
 * @t4-pass 0x10006510 2 2026-08-30 probes 54 bytes 1018 insns 350 regions 4 rows 0 census yes  (18 spellings x 3 front ends: VC5 RTM, VS97 SP3, VC6 RTM; struct returns; D3D twin check)
 * @t4-pass 0x10006510 3 2026-09-13 probes 11 bytes 1018 insns 350 regions 4 rows 0 census no  (option sweep incl. /Gi -- the flag that flips commutative canonicalisation elsewhere is inert here)
 */
class BrBitStream {
public:
    void WriteBits(int value, int nBits);   /* 0x1006D0B0, declared only */
};

extern "C" {
/* BrFixPackS16Q15Neg: prototype in br_funcs.h */
/* BrFixPackU24Q13: prototype in br_funcs.h */
/* BrFixPackS16Q7: prototype in br_funcs.h */
/* BrFixPackS16Q8: prototype in br_funcs.h */
/* BrFixPackS8Q3: prototype in br_funcs.h */
/* BrFixPackS6Q7Neg: prototype in br_funcs.h */
/* BrFixPackU8Angle: prototype in br_funcs.h */
/* BrFixPackS24Q1: prototype in br_funcs.h */
/* BrFixPackU8Range: prototype in br_funcs.h */
/* BrFixPackLevel: prototype in br_funcs.h */
}

/* VC5 lowers `!= 0.0f` to one `fcomp; fnstsw; test ah,0x40` -- C3 covers
 * EQUAL and UNORDERED, so NaN reads as zero, same as the original. */
static __inline int BrIsNonZero(float v)
{
    return v != 0.0f;
}

#define BrSar16(v, n)  ((short)((v) >> (n)))
#define BrSar8(v, n)   ((signed char)((v) >> (n)))

/* BrCarState: slice1_02.h (via br_coretypes.h) */

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

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006D0B0: the original calls BrBitStreamWriteBits_1006D0B0 by address */
inline void BrBitStream::WriteBits(int a1, int a2)
{
    BrBitStreamWriteBits_1006D0B0((void *)this, (unsigned int)a1, (unsigned int)a2);
}
