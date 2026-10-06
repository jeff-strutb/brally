# Vc42 is the real compiler

*Recorded 2026-09-11.*

> VC++ 4.2 (cl 10.20.6166) staged at tools/toolchains/msvc42; it breaks a codegen wall VC5 can't, but the cross-check says BRGlide is dominantly VC5 - 4.2 is at most a few mixed-in TUs, NOT a project-wide swap

** 2026-09-25 REFUTED for BRGlide: 0x10006BA0 is byte-exact under plain VC5 -- see [encodedelta-vc5-t4-2026-09-25](../levers/encodedelta-vc5-t4-2026-09-25.md). The "push-early vs narrow" anomaly was an int-vs-unsigned writer parameter, not the compiler.**

**2026-08-31. TITLE IS OVERSTATED - corrected by the cross-check below.**
Read together with [vc42-not-brglide](vc42-not-brglide.md) (concurrent session, same day).

**What is solid:** Visual C++ 4.2 (Microsoft 32-bit C/C++ Optimizing Compiler
Version 10.20.6166, June 1996) is staged at `tools/toolchains/msvc42/` (bin/include/lib,
gitignored like msvc5; separate C1.EXE/C1XX.EXE/C2.EXE backends). It compiles
and scores via `tools/brally/vc42_probe.py`. On BrCarStateEncode 0x10006510 (IN
BRGlide) it reproduces `push <nBits>` BEFORE the quantiser call + `sar ax,8`
narrow + `movsx` + clean C++ thiscall - the exact combination
src/brally/core/cpp/0x10006510.cpp documents as IMPOSSIBLE under VC5/VS97SP3/VC6
(18 spellings x 3 front ends). That anomaly is real and in the shipped DLL.

**What the bulk cross-check FOUND (tools/brally/vc42_probe.py, this session):**
- **0 functions newly match byte-exact under VC4.2.** Not one.
- **Dozens of VC5-exact functions REGRESS under VC4.2** (46 in one 12-file
  sample; 35 more in the slice2_12/slice1_02 pair). Audio, net, pad, the
  quantiser leaves - all match VC5 exactly, 1+ byte off under VC4.2.
- Display-list + car-draw TUs (br_dl_*, BrCarDraw*) get FEWER diffs under
  VC4.2 but still do NOT match - fewer-diffs != right compiler.
Concurrent session ran the same check on br_drawcar.c: all 4 VC5-exact
functions break under 4.2. See [vc42-not-brglide](vc42-not-brglide.md).

**Reconciliation (best current read, NOT proven):** BRGlide.dll is
overwhelmingly MSVC 5.0 (850+ byte-exact matches). BrCarStateEncode's
VC5-impossible shape is the one hard anomaly - most likely a single
VC4.2-compiled object (legacy net/bitstream code) linked into an otherwise
VC5 DLL, OR a VC5 spelling still undiscovered. Do NOT switch BRGlide work to
VC4.2. Keep VC5 the default. `tools/brally/vc42_probe.py` is the tool to re-check any
specific TU per-binary.

**Two VC4.2 idioms recorded** (in case a confirmed 4.2 TU turns up - e.g. an
EXE via config/brally/binaries.csv): pure-expression arg gives push-early+narrow-sar
+movsx together; `!= 0.0f` literal folds to `test [mem],0x7fffffff` so compare
against a `static const float` to force the x87 `fld;fcomp;fnstsw;test ah,0x40`.
See [byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md), [vc42-not-brglide](vc42-not-brglide.md).

**2026-09-10 - SECOND family member confirms the anomaly (0x10006BA0
BrCarStateEncodeDelta, committed src/brally/core/cpp/0x10006BA0.cpp).** Routing it to
the C++ lane (native-thiscall `BrBitStream::WriteBits` member, quantisers
extern "C") removed the C build's dead `xor edx,edx` shim: **714 -> 84
reloc-masked diff bytes under VC5 /O2 /GX /MD**. The residual 84 is ENTIRELY
the push-early-vs-narrow-shift mutual exclusion (the sibling's exact wall): the
`int16_t q` assignment keeps the narrow `sar ax,8` but pushes nBits late; the
pure-expression form pushes early but widens the shift (measured 729 under
VC5). **VC4.2 + pure-expression reproduces push-early AND narrow TOGETHER on
this second member** - independent confirmation the bitstream net family
(Encode/EncodeDelta, likely Decode/DecodeDelta 0x10007230/0x10007750) is a
VC4.2 C++ object.  It is still NOT byte-exact under VC4.2 at /O2 /Ox /O1
/O2y: register-blind 292 insns vs the original's 303 (~11-insn structural gap)
+ prologue reg-save order (ebp) + edx/ecx allocation. **The score reported by
vc42_probe/cpp_score is POSITIONAL (628) and hugely inflated by the early
prologue shift - read the register-blind instruction diff, not that number.**
Turning this family into matches is an ARCHITECTURE decision (wire tools/toolchains/msvc42
into the image build for these specific TUs), not a spelling grind - the wall
is the compiler, now proven on two members. Do not re-grind VC5 spellings.
PROJECT RE-AFFIRMED 2026-09-10: do not lower the T3 standard to absorb such
residue ([do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)).
