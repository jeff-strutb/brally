# Unfiled swaths

*Recorded 2026-09-09.*

> The gray "unfiled 0x1007xxxx" treemap swath was a display bug (fenced CRT), not untried work; 0x100311C0 BrTrackLoad parked T2 at one loop-head scheduling fork

2026-09-09, "take a crack at the untried swaths" session.

**The gray 0x1007xxxx swath was never work.** 642 of its 644 todo functions
(7,417/8,046 B) are in `config/fenced.csv` - import thunks, `__aulldiv`,
`__alldiv`, `_DllMainCRTStartup`, `_CRT_INIT` - reproduced at link.
`tools/progressmap.py` only honoured `fenced_exe.csv`; fixed in 1677fc9 to
read the DLL's `fenced.csv` (purple, own group). Before chasing a gray
region on the map, check `fenced.csv` first.

**0x100311C0 BrTrackLoad (770 B) T1→T2, committed 9c6bf9e**
(`src/core/generated/0x100311C0.c`). Byte-exact to +0x1da (whole preamble:
strcpy/strcat intrinsics, all calls, sky-texture reads). Proven levers:

- The 1/0/0 unit vector must be a **struct of three floats assigned
  1.0f/0.0f/0.0f** - `int[3]` makes VC5 CSE the zero into a register;
  three scalar ints shrink the frame (compiler drops the aliased slots).
- The flag OR must go through `pb = base + 0x4c + i; pb[1] |= 0x20;`  - 
  this +0x4c pointer spelling is what flipped the whole-function
  allocation from ebp to ebx (param in ebx, orig prologue exact).

Residue = ONE fork at the instance-loop head: orig sinks all three
vector-init immediate stores BELOW the arg pushes and forms arg3 with
`add edx,esi`; ours interleaves. Downstream symptoms only (fdivr/reload
order, `fld st(0);fmul mem` first test, SIB base/index swap, unfolded
byte-OR RMW with orig's dead `lea +0x4c`). 6 probes dead-listed in the
file header. Corpus MISSES on both loop idioms. NOT reggap-0 (two insn
shapes differ) - not a t3.py candidate yet.

**0x10036B20 BrDpAddressBuild (805 B) T1→T2 SIZE-EXACT, committed**
(`src/core/generated/0x10036B20.c`): regnorm 2+3, insns 232/231.  THE
LEVER: spelling null pointers as bare `0` - a single `(void *)0x0` cast
anywhere flipped the whole tail block layout AND broke the strlen guards
(+11 B). Other proven levers: pObj/vt COM-call locals (byte-exact sibling
0x10036F40 is the corpus-proven template for the alloc-retry-cleanup
dance), strlen guards through a materialized unsigned temp, one count
variable incremented in place, one result variable across both vtable
calls. Residue = the either-or layout class (as 0x10036810): branch-3
cmpsb hoisted over stores with jne delayed 14 insns, tail jmp-vs-
fallthrough. 6 probes dead-listed in the file header.

**0x10032E40 (1,881 B) SCREENED OUT of hand intake**: 182 x87 ops, 32
fxch - the colouring-wall class MATCHING.md says to skip in a T1/T2
lane. Needs the T3 path, not transcription. Do not open it by hand.

**The swaths are DONE.** Every real function in both panels is now
matched, parked T2 with a documented residue, screened out (the fxch
wall, the C++ twins 0x10038F40/0x10039620/0x100393C0/0x1003AA10/
0x10038CA0, thiscall 0x10039990, odd-address split 0x10035533), fenced,
or claimed by a parallel session. Both T2 parks are one-fork residues in
scheduler/layout classes - candidates for the end-grind, not re-opens.

Parallel sessions were landing in the SAME clone mid-session (their
commits interleave; `filing.csv`/`globals_learned.csv`/`br_dplay.c` left
dirty for them). fileaudit "assigned but not moved" 16 vs baseline 11  - 
the 5 extras are their fresh matches, not this session's.
