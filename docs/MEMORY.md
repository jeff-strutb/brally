# Memory index

Read `docs/VC5-IDIOMS.md` before matching any function. Query the tree for
coverage counts; do not trust a number in prose.

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
