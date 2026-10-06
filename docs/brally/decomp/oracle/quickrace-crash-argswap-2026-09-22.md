# Quickrace crash argswap

*Recorded 2026-09-22.*

> Quick Race crash root-caused to a swapped-argument transcription bug in BrRaceStep that A5 "certification" missed; found by differential call+arg trace

**2026-09-22: Quick Race crash-to-desktop FIXED - swapped call args in BrRaceStep 0x10019A70 (T3 giant).**

Root cause: the car-setup loop's `m_1006FCE0` (BrCarSlotSetup) call passed
its two int args in the wrong order - `m_1006FCE0(*(int*)(s+0x1aa0), i)`
instead of `m_1006FCE0(i, *(int*)(s+0x1aa0))`. The compiler emitted
`push edi/push eax` reversed vs the original's `push eax/push edi`.
BrCarSlotSetup forwards arg0 as the record **slot** to BrCarSlotLoad
(record = recbase 0x100bcdd0 + slot*0x15F88). Swapped, car 0 (i=0, id=1)
loaded its model into slot 1, leaving rec[0].model NULL; next frame
BrCarGfxSetColour (0x1002e79f, T4/byte-exact) dereferenced rec[0] and
faulted at 0x1002e7f1. Same commit also flipped a guard `!=0`→`==0`
(orig `jne` fires the call on `[esi-0x80]==0`). Fix commit: 2b922fdb.

**Why it matters (the project lead's core complaint, confirmed):** this is a
SEMANTIC arg swap that changed behaviour, yet BrRaceStep was A5-certified
T3. The A5 oracle's seeding gap never exercised this path, so
"certified" did not mean correct. See [t3-certified-standard](../rules/t3-certified-standard.md)
 -  A5 EQUIVALENT is only as good as the seeds; giants have paths A5 never
drives. Expect more such one-offs in the diff giants.

**Method that found it (use this, not guessing) - differential call+arg
trace:** BR_TRACE build stubs every annexable fn with a 24-byte
register/flag-transparent stub logging `va ecx edx a0 a1 a2 a3`; a
GetProcAddress crash filter dumps EIP+regs+globals to brally.log; a
BR_GOLDEN build relocates the ORIGINAL bytes into the annex + same
logging = a working reference. Diff the two traces. KEY: golden and
broken are different-length play sessions, so raw SequenceMatcher is
dominated by menu-frame-count noise - anchor on the deterministic race-
start region and diff per-entity call sequences + args. The lone
divergence in the setup path (car 0 → rec[1] vs rec[0]) named the bug.
Scripts in session scratch: brally_vm.py (readlog/writedll to the
running-VM VHD via vhdfat.py), golden.log/broken.log.

**Build gotcha (cost a full cycle):** BrRaceStep is a C++-lane row
(report_cpp.csv, opt O2y). image_build_t3.py reads the CACHED
`build/brally/win32/match/obj_cpp/<base>_sweep_<VA>_<ti>.obj` and does NOT recompile on
source edit. After editing a C++-lane .cpp, regenerate all 4 opt objs
first: `cpp_score.compile_cpp(src,'sweep_%08X_%d'%(va,i),DEFAULT_OPTS[i])`
for i in 0..3, THEN build. Related: [image-gate-builds-what-it-grades](../traps/image-gate-builds-what-it-grades.md).
