# C++ family 3 — large EH functions (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not C-sweep these.

## 0x10056260 — MATCH, all four pieces

8349 / 480,853 of BRGlide.dll `.text` (1.74%). Previously 230 diffs
(EH frame only in the C port). Highest-value match on the board.

| piece | orig VA | size | result |
|---|---|---:|---|
| function body | 0x10056260 | 8349 | **MATCH** `/O2 /GX /MD` |
| FuncInfo | 0x1007AD00 | 28 | **MATCH** magic/maxState=2/nTry=0/nIP=0/toState=-1,-1 |
| unwind[0] | 0x100765E0 | 11 | **MATCH** `push [ebp-0x10]; call operator delete; pop; ret` |
| unwind[1] | 0x100765EB | 11 | **MATCH** same |
| handler | 0x100765F6 | 10 | **MATCH** `mov eax, FuncInfo; jmp __CxxFrameHandler` |

Source: `build/cpp_work/0x10056260.cpp`.
`python3 tools/cpp_score.py --va 0x10056260 --opt "/O2 /GX /MD"`

Do not `@implements`-tag it in `src/` this session.

### What the source is

One cdecl function, no args. Not a ctor/dtor. Two `new T` (maxState=2,
both unwind `operator delete`, both toState=-1 = sequential not nested)
plus 145 scalar allocations with no EH:

1. `memset` 0x122 dwords at the image table, three word zeros, `xor ebx,ebx`
   (ebx stays 0 for every later NULL test).
2. `Tables64Clear()`.
3. **145×** `p = (char *)operator new(0x104); slot = p; strcpy(p, "images\\…")`.
   Unrolled. Inlined strcpy = `or ecx,-1 / xor eax,eax / repne scasb /
   not ecx / shr ecx,2 / rep movsd / and ecx,3 / rep movsb`. The next
   `push 0x104` software-pipelines into the current copy (7 of 145 store
   the pointer before loading the string — compiler schedule, not source
   order). Last copy interleaves `or eax,-1` into the movsb for the
   following `rep stosd` of the 0xB4-dword `-1` fill.
4. `new Phase` (0xC8, declared ctor). Trylevel 0 then -1. Store to both
   phase globals, `if (!p) return 0`, then `p->pfnEnter = fn`.
5. `if (!g_obj400) { o = new Obj400; /* 0x400, declared ctor */ … }`.
   Trylevel 1 then -1. `if (!o) ErrShow(1)` and **fall through**.
6. strcpy of the two save-file buffers, `RectTablesInit()`, `return 1`.

### Proven idioms (this function)

- **Scalar `operator new` is not dllimport.** Orig `call 0x10074572` (E8
  thunk to `??2`). `#include <new>` / `/MD` default is `FF 15` (6 bytes,
  shifts everything). Declare `void *__cdecl operator new(unsigned int);`
  and do not include `<new.h>`.
- **`new char[0x104]` is `??_U` (new[]).** The 145 path buffers are
  `operator new(0x104)` with no ctor and no EH state.
- **`new T` needs a declared, not defined, ctor.** An implicit/trivial
  ctor inlines to a vptr store and drops the EH region (maxState
  collapses). Same as 0x10040D10's "declare member dtors, do not define".
- **Trylevel=-1 after the last `new` is not free.** `/O2` omits it when
  every later call is `extern "C"` (nothrow). Orig still stores it
  (8 bytes). Spell `ErrShow` / `RectTablesInit` as C++ linkage so the
  compiler keeps the store. First seen here: 92 diffs, first @ +0x1FF5
  (`jne` disp 0x43 vs 0x3b) until that store returned.
- **`extern "C"` strcpy, `#pragma intrinsic(strcpy)`.** Inlines; a
  dllimport call does not.
- **Prologue is `push -1; push handler; mov eax, fs:[0]`** (the ctor
  schedule, not the 0x10040D10 dtor schedule). One local (`push ecx`) +
  ebx/esi/edi. ebp is not the frame pointer.

## Page builders — frame landed, body is a coloring wall

Same class: `int f(Phase *ph)` cdecl, `new Page` (0x348, ctor 0x100418C0)
then N `new Ctl` (0x1E214, ctor 0x10040B10). Virtual `Place` at vtbl+0x38
(8 stack args, thiscall, callee pops), `SetText` at +0x34 (4 stack args).
maxState = number of `new`s, all toState=-1, all unwind `operator delete`.

Ctl vtable is 15 slots (dtor at +0, Place at +0x38). Declare them all;
displacements are immediates. Page is 0x348 with `apCtl[200]` at +0x18,
`fX`/`fY` at +0x338/+0x33C, `pOwner` at +0x340.

| VA | size | maxState | .text | FuncInfo | unwind actions | handler | first .text diff |
|---|---:|---:|---|---|---|---|---|
| 0x1004F8C0 | 1498 | 9 | 1131 diffs | **MATCH** | 9/9 **MATCH** | **MATCH** | +0x22 `mov ebx,1` vs `mov ebp,1` |
| 0x100485B0 | 2389 | 14 | 1776 diffs | **MATCH** | 13/14 (one `[ebp+4]` vs `[ebp-0x10]`) | **MATCH** | +0x00 prologue schedule |
| 0x1004DA00 | 3394 | 21 | 2051 diffs | **MATCH** | 18/21 (same slot) | **MATCH** | +0x00 prologue + missing `sub esp,0xc` |

Sources: `build/cpp_work/0x1004F8C0.cpp` (full Place/SetText body),
`0x100485B0.cpp` (fopen AutoSave.brf + namelist fill + 13 Ctl),
`0x1004DA00.cpp` (21 news; trailing Place args not all unique).

### Coloring wall (do not permute)

On 0x1004F8C0 the EH prologue matches through `push edi`. Instruction
+0x22 is `mov r32, 1`: orig **ebx**, recomp **ebp**. Orig then keeps
**Page in ebp** (whole function) and **ebx = 1 until the first Ctl**,
which overwrites it. Recomp swapped those two roles. The 1-register
feeds `flags[nPages]=1` and trylevel=1; after that every `[ebp+…]` vs
`[ebx+…]` is a cascade. Same wall as VC5-IDIOMS "allocator-internal
residue" — three source shapes compile to the same split.

Secondary lowering: orig materializes `if (p == 0)` as `cmp; sete al;
test al; je` because `xor eax,eax` / `mov cx, nPages` sits between the
compare and the branch. Recomp uses `cmp; jne` and skips the sete.
Not source-reachable without a type/shape insight; do not re-derive.

0x100485B0 / 0x1004DA00 also flip the prologue schedule (`mov eax, fs:[0]`
first vs orig `push -1` first) once extra locals / `fopen` appear. Both
schedules are one compiler (see cpp-harness-notes 0x10040D10). 0x1004DA00
orig has `sub esp, 0xc` (three extra slots); unwind actions that live in
those slots read `[ebp-0x10]`, ours `[ebp+4]`.

### What opened the frame on these three

`Ctl()` **declared, not defined**. Without it, `new Ctl` inlines the
vptr store, maxState=1 (only `new Page` is protected). With it,
maxState equals the `new` count and every unwind action is the 11-byte
`operator delete`. That is the 0x10056260 ctor idiom applied to the
page-builder family.

## What not to do

- Do not re-derive 0x10056260. The `.cpp` is the source; `/O2 /GX /MD`
  is the opt; all four pieces matched.
- Do not dllimport `operator new` on these TUs.
- Do not `new char[N]` for the path buffers.
- Do not C-sweep the 80. `6aff`/`64a1` in the first ~0x20 bytes → `.cpp`.
- Do not permute ebx/ebp on the page builders. The wall is classified.
