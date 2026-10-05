# Cxx eh frame wall

*Recorded 2026-08-27.*

> Functions with an MSVC C++/SEH EH frame (push -1 / fs:[0]) are unreachable from plain C - the eventual .cpp/SEH workstream, not the C tree. SCREEN big targets for this BEFORE assigning.

##  CLASS OPEN + INTEGRATED 2026-08-27 - 36 C++ EH functions now match

**UPDATE (later 2026-08-27): the "INTEGRATION TODO" below is DONE.** 36 C++ EH
functions are filed under `src/core/cpp/*.cpp` (tag: `@implements 0xVA glide
NAME` + `@cpp_kind` + `@cpp_symbol <mangled>`), swept by `tools/cpp_sweep.py`
into `build/match/report_cpp.csv`, and counted by `total.py`. Includes the
**8,349 B landmark 0x10056260 - largest match in the project.** 15,832 B of
C++ EH matched. cpp_score.py verifies all 4 pieces (.text body+frame, FuncInfo
.xdata magic 0x19930520, unwind action, handler thunk). See
[counting-reconciliation](../traps/counting-reconciliation.md).

##  CLASS OPEN 2026-08-27 - C++ /GX harness PROVEN, verification gap CLOSED

**tools/cpp_score.py exists and WORKS.** It compiles a .cpp TU with `cl /O2
/GX /MD` and scores ALL FOUR pieces of a C++ EH function - the .text body+frame,
the FuncInfo table (.xdata, magic 0x19930520), the __ehvec_dtor unwind action,
and the handler thunk - including the .xdata/.rdata parts match_sweep CANNOT
see. **0x10040D10 (97B virtual dtor) matched 0 diffs on all four independently.**
So the earlier verification-gap worry is RESOLVED: the C++ workstream IS
verifiable, but with cpp_score.py, NOT the C sweep. The 20%-of-.text C++ EH
class is now an OPEN, tractable, verifiable workstream - no longer a wall.

** 0x10056260 (8,349 B) MATCHED 0-diff on all four pieces 2026-08-27 - the
LARGEST match in the project (1.74% of .text in one function).** Source
build/cpp_work/0x10056260.cpp: 145 unrolled `operator new(0x104)` + inlined
strcpy, then `new Phase`(0xC8) and `new Obj400`(0x400). New C++ idioms (in
docs/cpp-family3-notes.md): operator new must NOT be dllimport (E8 thunk, not
FF 15); path buffers are scalar `operator new(0x104)` not `new char[N]`;
`new T` needs a DECLARED-not-defined ctor or maxState collapses; Trylevel=-1
after the last new is dropped if later calls are extern "C" (keep the C++-
linkage calls as C++). **NEW SUB-CLASS: frame-lands-but-body-is-coloring-wall.**
0x1004F8C0/0x100485B0/0x1004DA00: the C++ FRAME reproduces (FuncInfo + unwind
MATCH) but the BODY is a register-coloring wall (1131-2051 diffs, ebx-vs-ebp
for the constant 1) - same ceiling as the plain-C coloring walls, just inside
a C++ function. Classify, don't permute.

Key layout idiom: the match needed the real member layout (vptr + TextBox
boxes[3] each 0x438 at +0x2B5C + TextList at +0x3838) and **member dtors
DECLARED not defined** - otherwise the compiler inlines them and you lose the
__ehvec_dtor call. Harness needs: .cpp + /GX, mangled-symbol mapping in the
COFF read, and the 4-piece .xdata/.text scorer. Next targets (docs/
cpp-harness-notes.md): slice8_86 24-diff cluster (bodies already C-matched,
residue is the frame), 0x10056260 (8,349B, 230 diffs, 97% body-matched), the
8 stack-dtors. INTEGRATION TODO: a .cpp tree module + fold cpp_score into the
sweep as a second scorer for @implements'd .cpp files.

##  CLASS FULLY SCOPED 2026-08-27 (SEH-split pass) - reshapes the picture

**SEH IN VC5 C IS PROVEN - `__try`/`__except`/`__finally` reproduces the
push -1/fs:[0] frame byte-exact.** Matched 0x10074770 (__ArrayUnwind, __try/
__except) and 4 more EH helpers at 0 diffs /O2 (0x10074AE6 _except_handler3
thunk, 0x100747E0 __FrameUnwindFilter, 0x100746C0 __ehvec_dtor __try/__finally,
0x10074800 __ehvec_ctor __try/__finally). Key: the orig __except filter is
`FrameUnwindFilter(GetExceptionInformation)`, NOT a constant 1. **5 SEH
matches pending filing in build/ghidra_work/ (tree was live).**

**THE 80 `push -1`/`fs:[0]` FUNCTIONS ARE C++ UNWIND, NOT SEH - and they are
97,204 B = 20.2% of .text.** All 80 thunk to `__CxxFrameHandler` with
nTryBlocks=0 (unwind only, FuncInfo magic 0x19930520): 70 `new T` (op-delete
cleanup), 8 stack-object dtors, 2 member-array __ehvec_dtor. This is the real
C++ pile and it is LARGE - but see the reframe:

**REFRAME - the C++ pile is NOT 97KB from scratch. Most BODIES are already
C-matched; only the EH FRAME is missing.** 0x10056260 (8,349B) is 97% body-
matched (needs 2 unwind states). The slice8_86 cluster (0x100439B0 etc., ~24
diffs) has C-matched bodies; the residue IS the EH frame. So the workstream is
"add the EH frame to functions we mostly already did," gated on ONE tool.

**THE GATE: a .cpp + `cl /GX` harness.** `match_sweep.compile_variant` only
builds C. C++ EH needs a .cpp TU compiled /GX. Build that, then: (1) smallest
first 0x10040D10 (97B thiscall ctor, member-array unwind); (2) the slice8_86
24-diff cluster (bodies done); (3) 0x10056260 (8KB, body 97% done); (4) 8
stack-dtors; (5) high-maxState UI ctors (24-25 states). Full table:
docs/eh-workstream-notes.md. NOTE the verify blind spot: the FuncInfo/unwind
tables live in .rdata (magic 0x19930520), OUTSIDE the .text bytes the sweep
compares - a .text match won't prove the unwind data matches (same class as
the jump-table blind spot, bigger). Decide whether to verify those separately.

---

**VERIFIED 2026-08-26.** Confirmed live on BrUiBootPreLoopGate 0x10056260  - 
its first 0x100 bytes are `push -1; push <scopetable>; mov eax,fs:[0]; push
eax; mov fs:[0],esp`, and the remaining 8,093 bytes (97%) already match. The
230 "diffs" were ENTIRELY the EH prologue, unreachable from C. (Original
salvaged-lead text below.)

**SCREENING DISCIPLINE (learned the hard way 2026-08-26):** before assigning
any large DIFF function as a matching target, screen the orig bytes:
1. **EH check** - `b[0:2]==6a ff` (push -1) OR `64 a1` (mov eax,fs:[..]) in
   the first ~0x20 bytes = EH/SEH frame → NOT a C target, skip.
2. **Completeness** - rank by `recomp_size/orig_size`, NOT by report.csv
   `diffs`. The diff count only compares the OVERLAP; a mostly-unwritten stub
   (e.g. BrTex3dExpand 0x100250D0: recomp 944B of 8480B) looks deceptively
   close. Want 0.85 - 1.15 completeness for a "reconcile the diffs" target.
3. **Float wall** - x87 byte density (`d8..df`) high, or it's 0x1000EAF0 =
   documented float-scheduling wall (rule 11a). Skip.
Doing this survey WRONG cost a near-miss: 0x10056260 was picked as "the 8KB
prize, 230 diffs, 97% done" - then found to be pure EH. Screen first.

**Big EH-wall functions found 2026-08-26 (do NOT re-survey as C targets):**
0x10056260 BrUiBootPreLoopGate (8349B), 0x1004AEE0 (3862B), 0x100439B0
(3746B, only 24 diffs - the EH prologue), 0x1004DA00 (3394B), 0x10044860
(2439B, 24 diffs), 0x100485B0 (2389B), 0x10045EF0 (1834B, 24 diffs). Plus a
cluster of `64 a1`-entry (SEH) functions in the 2 - 3KB range. These are the
eventual .cpp / SEH-C workstream - see the completeness note in README scope.

**UNVERIFIED LEAD, salvaged from a killed worker (2026-08-20).** Reported while
working the slice3_31 / slice3_32 ACTIVATE routines; the worker's work was lost
before I could confirm it, so re-check the disassembly before relying on this.

The ACTIVATE routines show an MSVC C++ **`new`-expression exception-handling
frame**: `push -1`, `push <scopetable>`, an `fs:[0]` exception-registration
link, and a constructor called with the object in `ecx`. That is the shape MSVC
emits for `new T(...)` inside a function with an active EH scope.

**Why it is a wall:** plain C has no EH frame, no scopetable, and no
ctor-in-ecx calling convention. No amount of source shaping in C reaches this
prologue. It is NOT a register-allocation near-miss and NOT a struct-layout
problem - it is a different language's codegen.

**How to apply:** when objdiff shows `push -1` + `push <imm32>` +
`mov fs:[0]` at the top of a function you are trying to match, stop
immediately and record it. Do not spend attempts on it. These functions are
candidates for the eventual C++-compiled subset of the decomp, not for the C
tree. Add a note in the source at the function so nobody re-derives it.

Distinguish from [thiscall-via-fastcall](thiscall-via-fastcall.md): a bare ctor-in-ecx call with NO EH
frame is often reachable from C via `__fastcall`. It is the EH frame that
makes it unreachable.

Related: [divergence-class-triage](../triage/divergence-class-triage.md), [matching-progress](../log/matching-progress.md).
