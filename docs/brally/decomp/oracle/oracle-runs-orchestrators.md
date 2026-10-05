# Oracle runs orchestrators

*Recorded 2026-09-16.*

> 2026-09-15: the A5 equivalence oracle can now behaviourally certify orchestrator functions (inputs = an object graph, not scalars) via valid-state seeding + emulator/resolution extensions + A5 made authoritative in t3.py. This is how a colouring-walled giant becomes T3.

** The A5 oracle (tools/t3b_verify.py) now runs functions whose input is an
OBJECT GRAPH, not just scalar-arg leaves. This is the path to T3 for a giant that
byte-shape gates false-negative on colouring residue.** Built 2026-09-15 doing
BrRaceStep 0x10019A70 (see bracestep-wall); all changes committed, no regressions.

**Why random seeding fails for these and what to do.** A leaf takes scalars --
a random int is a valid int, so random seeding works. An orchestrator takes no
args; its input is ~200 globals, many holding POINTERS to structs. Random bytes
in a pointer global point nowhere, the function walks garbage down branches the
compiler proved unreachable, and the two compilations diverge on paths that never
run with real state -- FALSE DIFFs. FIX = valid-state seeding: pointer globals
null-safe (BSS->0), the scalar state/flag globals that gate control flow varied
per seed. tools/oracle_profiles.py holds a per-VA `Profile(bss_fn)`; t3b_verify
auto-applies it. A profile is a decision about a function's input SHAPE, not a
lowering of standard -- a bad world surfaces as spurious DIFFs, never false
EQUIVALENTs.

**A5 is now AUTHORITATIVE (t3.py gates).** A clean EQUIVALENT/EQUIV-MODULO-FP
verdict SUPERSEDES byte-shape gates A1-A4 (they are proxies for "residue looks
like compiler choices"). They decide only when the oracle is UNCLASSIFIED. DIFF
still fails. This ends the false-negative where a behaviourally-equivalent giant
fails A1/A2/A4 on colouring residue that never changes behaviour.

** THE ORACLE FINDS REAL BUGS THAT STATIC REVIEW + "STRONG EVIDENCE" MISS.**
On BrRaceStep it caught an inverted branch (g_226A44 transcribed `==0`, the orig
`je` skips the loop when zero so it is `!=0`) -- invisible to "every symbol
resolves / strings match / dispatch looks right". Method to localise a DIFF:
compare the ORDERED sequence of writes to real in-image globals between the two
sides; the first divergence, mapped to the writing instruction, is the bug. Watch
for non-bugs that desync a positional write trace: an intermediate double-store
(orig `g=idx+1; if(...)g=0;` vs a single `g=` -- behaviourally identical) shifts
every later write; fix the source to match the store shape first, then the trace
realigns and the REAL divergence surfaces.

**Emulator/resolution extensions that made it run (tools/, committed).**
x87emu: indirect call + jmp, 64-bit CRT helpers (allmul 0x10074680, alldiv
0x100748B0, aulldiv 0x10074610), memmove import (IAT 0x118F04FC), rep string ops,
memory-destination arithmetic, 16-bit regs, setCC, on-demand disasm (the shared
.text index is a LINEAR sweep -> entries after embedded data are missed),
indirect-callee boundary modelling (compare the SLOT sequence, NOT args -- arity
unknown), garbage-pointer tolerance (identical wild touches are seed noise; compare
only real in-image globals + return + dispatch). t3b_env/reloc_fill: resolve the
naming-convention symbols (g_<HEX> -> 0x10000000+HEX, sub_/m_<HEX> -> full addr,
C++-mangled ?g_/?m_), string constants (Image.find_bytes), CRT imports
(Image.imports parses the IAT), and $L jump-table self-labels (same-.text-section
-> va+offset). t3b_verify: obj_cpp added to the obj scan; parse_signature strips
`extern "C"` and uses `git grep --untracked` (so a not-yet-committed cpp-lane file
is found).

See [cpp-lane-t3-filing-workflow](../cpp-lane/cpp-lane-t3-filing-workflow.md), [t3-certified-standard](../rules/t3-certified-standard.md),
[byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md).
