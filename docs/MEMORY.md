# Memory index

Read `docs/VC5-IDIOMS.md` before matching any function. Query the tree for
coverage counts; do not trust a number in prose.

## The standard (2026-09-06): T3 and T4, nothing between

Tiers are T1/T2/T3/T4 only; T3a/T3b are retired (CLAUDE.md rule 12). T4 is
byte-exact. T3 is functionally DONE but not byte-exact, and is decided only
by `tools/t3.py --qualify <VA>`: Gate 0 (purpose comment, no unfinished
markers), Gate A (five mechanical residue checks on the sweep object), Gate B
(the `@t4-pass` ledger in the file header: >= 3 passes of >= 10 compiles,
the last two moving nothing, one census-driven). Crossing Gate A is a
precondition, never the trigger to stop. Certified functions are parked
until the end-grind and never handed out (`tools/claim_lane.py`,
`tools/t4lane.py` exclude them). `tools/t4lane.py` picks a byte-exact
session's targets from the tree. 0x1000EAF0: gates 0 and A pass, one
zero-movement pass short of T3 (rule 11b).

## Open leads

### 0x10019A70 — the race step (last among the big targets)

11,223 bytes, 11,223 / 480,853 of BRGlide.dll `.text` (2.33%). One original
function, so one C function. The port splits it (Clock / Begin / Frame /
Lights) and that is fine for the port; it is not a matching twin.

Matching protocol:

1. Win the prologue. Original is `sub esp, 0x34; push ebx; push ebp; push
   esi; push edi` with `xor ebp,ebp` — ebp is a general register. A recomp
   that emits `push ebp; mov ebp,esp; and esp,-8` has an 8-byte-aligned
   local (`double` / `__int64` / Ghidra `float10`). Remove it. Until
   instruction one is `83 ec 34`, nothing downstream can match.
2. Grow as a progress bar. Recompile the whole function after each section
   and watch the first divergence march forward. The ~2% honestly annotated
   in `br_racestep.c` (script seed at 0x1001A97C) is a known-good island,
   not a claim on the address.
3. Gated on 131 distinct callees. Wrong stdcall vs cdecl or arity corrupts
   the call site. Do this after those functions are matched and prototyped.

No WIP twin is kept in the tree: regenerate the starting dump with the
ghidra pipeline when the callees are ready — with the current idiom
dictionary applied from the start (declaration forms, real float
prototypes), a fresh dump starts cleaner than the discarded draft did.
Do not tag the address until the bytes diff clean. Details in
`include/br_racestep.h`.

Prologue, 2026-08-24:

- A clock-only stub matched instruction one (`83 ec 34`) but only pushed
  esi/edi — not enough 0-stores across calls for ebp/ebx.
- The full Ghidra body with `float10`→`float` still emitted `and esp,-8`
  because empty `int f();` declarations promoted float args to double.
- After real BrVec3 prototypes: `sub esp, 0x3c; push ebx; push ebp; push
  esi; push edi`. Four-register push matches. Frame is 8 bytes large
  (`0x3c` vs `0x34`); one `fstp qword` remains. Next: find that call.
