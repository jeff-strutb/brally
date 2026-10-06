# Tgr n64 viability

*Recorded 2026-09-03.*

> N64 Top Gear Rally decomp spike (2026-08-25) - IDO -O2, standard F3DEX ucode, 195 strings shared verbatim with BRGlide.dll; highly viable, string-anchored function pairing is the bridge

Spike run 2026-08-25 on `reference/tgrally/Top Gear Rally (USA).z64` (8 MB,
valid z64, entry 0x80200000). Boss Rally (PC) is Boss's own engine; TGR N64
(1997) is the original - Kemco owned only the "Top Gear" name. No other decomp
of it exists anywhere (GitHub/decomp.me searched; only this project's repo).

**Findings - all favorable:**
1. **Compiler: IDO -O2.** Windowed capstone stats over the code segment (ROM
   0x1000 - ~0xC0000, uncompressed; assets are deflate-compressed - zlib
   `inflate 1.0.4` lives in-engine): branch-likely instructions dense in every
   window (up to 507/32KB), delay-slot nop-fill mostly 0 - 25%. GCC 2.7 emits
   ~no branch-likely. IDO = best tooling support (recompiled IDO, decomp.me,
   permuter, m3c all turnkey). IDO 5.3 vs 7.1 undetermined - settle via
   decomp.me trial.
2. **Graphics microcode is standard Nintendo F3DEX.NoN 1.21** (string in ROM).
   The "Boss custom ucode" reputation is WDC/Stunt Racer-era (BOSS ZSort);
   it does NOT apply to TGR's gfx. Custom *audio* ucode still possible,
   unchecked.
3. **Retail ROM is stripped** - no source paths, no asserts, no @(#) strings.
   No free module names from this build.
4. **195 strings shared verbatim with BRGlide.dll**, incl. highly distinctive
   debug printfs: "granting technically-earned lap %d/%d to %d", "Bad Final
   Matrix: in=%d (%s), s=%f", "VAR SAVE OVERFLOW", "airplanePathLeft = %08x",
   gate-tracking lines. Shared source proven. **String-anchored function
   pairing** is the bridge: same string referenced on both sides = function
   correspondence with zero matching work.

**PC-side transferability numbers** (measured from reference/brally/orig/BRGlide.dll IAT +
call graph): 74.7% of .text (343,696 B, 1,808/2,140 fns) never touches an
import - the shared-lineage engine body. Glide submission layer is only 35
fns / 14,360 B (3.1%). Of 512 matched fns, 460 are in the portable region.

**Prototype ROM** (`Top Gear Rally (Prototype).z64`, added 2026-08-25): NOT an
early dev build - embedded build stamps say proto=15Aug97, retail=25Aug97
(10 days apart, near-final master). Same IDO -O2, same F3DEX 1.21, stripped,
blank header. Marginal value: (a) second IDO compile of ~same source at
shifted addresses → proto↔retail twin table, same role as shared.csv
(pointer-vs-constant disambiguation, function boundary confirmation; 62%
identical 16-byte chunks); (b) a few debug guards retail dropped ("Mtx pool
ran dry", "CELL SHADOW GFX/VTX BUF OVERFLOW"). No symbols/asserts/paths.

**Round-trip test RUN 2026-08-25 (commit 2ed53ab).** IDO 5.3 + 7.1 recomp
staged in-repo at `tools/toolchains/ido/` and `tools/toolchains/ido53/` (gitignored; decompals
v1.2 macOS universal - runs natively). Flags for TGR: `-O2 -mips2
-non_shared -G 0`. Test subject: BrVarSave (PC 0x100608F0, 118 B ~ N64
0x8022adcc / ROM 0x2bdcc, 164 B), located via the "VAR SAVE OVERFLOW"
string anchor (string→code: single addiu hit; boot segment maps ROM 0x1000
→ RAM 0x80200000 linearly). Results:
- **N64 matching is cheap**: hand-recovered C matched the 41-insn body
  except 2 entry quirks (guard-load CSE / beql rotation + one spill slot
  0x80 vs home 0x8c) - identical output from IDO 5.3 and 7.1 and three
  source spellings; permuter territory, not a wall.
- **Cross-witness works**: N64 disasm resolved the PC file's documented
  deviation (original uses plain sprintf, not snprintf) and confirmed
  struct {ptr,size}, 0x50 buffer, arg order, fatal call.
- **LIMIT - the pivot's dream is only half-true**: correct source does NOT
  auto-match on PC. BrVarSave went 96→95 diffs; both PC compiles have
  identical structure (intrinsic rep movsd/movsb memcpy, lagging-pointer
  walk) with different register assignment - the regalloc wall class.
  N64 truth removes SOURCE uncertainty, not VC5 CODEGEN uncertainty.
  So N64-first pays on the structurally-wrong class (recomp_size <<
  orig_size, the class that pays per [inlined-helper-match-class](../../brally/decomp/triage/inlined-helper-match-class.md)),
  and does nothing for regalloc-walled rows.

**ADOPTED INTO THE WORKFLOW (2026-08-25).** The matching grind runs a
"1) machine crank 2) hand resolve 3) repeat" cadence across sessions. The
N64 witness is now part of step 2: when a function is blocked on WHAT THE
SOURCE SAYS (structurally-wrong class), find its N64 twin via a shared
debug string or callgraph and read the MIPS. Never consult it for
regalloc-classed rows - proven useless there. The active matching session
(brally-f0) was briefed with the full recipe via SendMessage; future
sessions get it from this note.

** BULK RUN EXECUTED 2026-09-03 (commit 1debdab) - two claims above are
now CORRECTED, and the harness exists.** `tools/tgr/` compiles the PC decomp's
C with the staged IDO and searches TGR's .text for each function, which pairs
AND matches in one step. 252/255 source files cross-compile (the unlock was
shims: `tools/tgr/include/` libc + Win32 typedefs, declarations only - the
Win32 headers alone blocked 182 files). Denominator: **TGR .text = 457,392 B /
883 functions.** Result with ZERO hand-matching: **22 EXACT (964 B, 0.21% of
.text), 175 SHAPE, 197 located (13,440 B, 2.94% of .text; 22.3% of functions).**

- **CORRECTION 1 - string anchoring is a SEED, not the bridge.** Only 101
  literals >=6 chars are shared and just **7** are code-referenced on the N64
  side; the distinctive ones (credits) sit in pointer tables. The "195 strings
  → string-anchored pairing is the bridge" claim above overstates the usable
  yield by ~30x. What actually pairs functions in bulk is the compile-and-
  search above. The anchors' real value is INDEPENDENT VALIDATION: they and
  the opcode-multiset search agree on every address where both fire
  (BrTexSizeShift 80217614, BrVarSave 8022ADCC) and disagree nowhere.
- **CORRECTION 2 - IDO 5.3 vs 7.1 is NOT a lever, question closed.** 23 EXACT
  vs 22 over the same 1,753 functions. Don't spend a decomp.me trial on it.
- **The residue is one sweepable class:** 43 functions sit at opcode-multiset
  distance 0 - structurally identical, differing only in commutative operand
  order and register allocation. **IDO preserves source operand order where
  VC5 canonicalises it**, so these are a SOURCE-ORDER ORACLE for the PC side,
  not an N64 wall (BrVec3Dot's summation grouping - the documented PRECISION
  CAVEAT in br_vec.c - is resolved by its N64 twin). Next bulk lever is an
  operand-order permuter, not hand work.

**Decision (project lead + analysis):** not a parallel second grind. Next step if
pursued: splat the ROM, IDO preset, round-trip 2 - 3 functions already matched
on PC (known answers) through the N64 diff harness to test whether
MIPS-first source recovery beats x86-first for the portable region. N64→PC
payoff: second codegen witness (signedness/width/op-order visible on MIPS)
+ string-anchored naming for unfiled slice functions.
