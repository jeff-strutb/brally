# Frontend screen vptr hoist

*Recorded 2026-09-05.*

> Screen: a COM vtable load scheduled ABOVE a store to a global is C++ front-end (C1XX) output, never C; and C1XX folds field pointers that C1 keeps. Both proven on 0x100706D0 BrInputPoll, which is 2 bytes from exact under C and 1 region under C++.

Proven 2026-09-05 on 0x100706D0 BrInputPoll (src/core/controls/br_inputpoll.c,
b0c695a); the full write-up is the three entries at the tail of
docs/VC5-IDIOMS.md (de73f99) and the TU header's dead-probe list.

**The screen.** When a function's only residue is `mov R,[R]` (a vtable read)
one slot ABOVE `mov [global],R'`, VC5's C front end cannot produce it: a
whole-binary census finds three deref-then-global-store adjacencies in 2,146
functions and the other two are plain source order. Every C spelling that puts
the read first in source uses a local, and every local FLOATS to the top of
the block (13-14 diffs). Compiled as C++ (COM struct with virtual __stdcall
slots, `/O2 /GX /MD` like src/core/cpp/), the site is byte-exact: C1XX treats
the vptr load as non-aliasing. Build such a function as `.cpp` before probing C.

**The trap on the same function.** The mouse-accumulate site needs
`int32_t *pPrevAx = &g_brInMouse[prev].ax` under C (C1 keeps the pointer and
splits the first read from the kept address); C1XX folds every pointer
spelling tried (plain, field/record/const pointer, reference, value+pointer,
`register`, function-scope, arithmetic/cast/flat-int, struct via pointer) back
to the plain form. So the two front ends are mutually exclusive on this
function today. **Open lead:** a C1XX spelling for that site finishes the
function in the C++ lane. Do not re-run the probes listed in the header.

**Also proven there:** an address-taken aggregate's BLOCK SCOPE frees its slot
for a later compiler temp (frame 0x118 -> 0x110) - the 2026-09-03 "scope is
inert under /O2" note holds for scalars only.

Related: [big-five-t1-lanes-2026-09-04](../log/big-five-t1-lanes-2026-09-04.md), [cpp-vcall-family-lode](cpp-vcall-family-lode.md),
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
