# C++ /GX harness — proven 2026-08-27

80 functions (97,204 B = 20.2% of BRGlide.dll `.text`) thunk to
`__CxxFrameHandler` with FuncInfo magic `0x19930520`, `nTryBlocks = 0`
(unwind only, no try/catch). The C pipeline cannot emit that frame.
This file is the `.cpp` path.

## Result: 0x10040D10 is open

`python3 tools/cpp_score.py --va 0x10040D10` → **0 diffs on .text**.

| piece | orig VA | size | result |
|---|---|---:|---|
| function body | 0x10040D10 | 97 | **MATCH** `/O2 /GX /MD` |
| FuncInfo (`.xdata$x`) | 0x10079AE0 | 28 | **MATCH** magic/maxState/nTry/nIP/toState |
| unwind action | 0x10075060 | 27 | **MATCH** |
| handler thunk | 0x1007507B | 10 | **MATCH** |

Source: `build/cpp_work/0x10040D10.cpp`. Scorer: `tools/cpp_score.py`.
Do not `@implements`-tag it in `src/` until a later session files it —
this session does not edit `src/` or commit.

It is a **virtual destructor**, not a ctor. Workstream notes called it a
ctor because the unwind *action* is `__ehvec_dtor`; the body also *calls*
`__ehvec_dtor` on the success path (destroy array after `~TextList`).
Ctor of the same class is 0x10040B10 (478 B, starts `6aff`); scalar
deleting dtor is 0x10040CF0.

## Verification gap — honest answer

**A `.text` match of the function does not imply FuncInfo matches.**
They are different sections and the C sweep never sees the data.

| what | where | in the 97-byte orig bin? | match_sweep? | cpp_score? |
|---|---|---|---|---|
| dtor body + EH prologue + trylevel stores | `.text` 0x10040D10 | yes | yes | yes |
| handler thunk `mov eax, FuncInfo; jmp __CxxFrameHandler` | `.text` 0x1007507B (own map entry, 10 B) | no (`push offset` is a reloc) | only if tagged separately | yes |
| unwind action `__ehvec_dtor(...)` | `.text` 0x10075060 (own map entry, 27 B) | no | only if tagged separately | yes |
| FuncInfo + UnwindMap | `.rdata` / `.xdata$x` 0x10079AE0 | **no** | **no** | structural compare |

The handler thunk's `B8 <FuncInfo>` is a reloc, so even scoring the 10-byte
thunk `.text` does **not** prove FuncInfo *contents*. You have to parse
`.xdata$x`. Same class as the jump-table `.rdata` blind spot, bigger.

**The workstream is verifiable, but not with current C tooling.**
`match_sweep` is `.text`-only and the map splits the EH sidecar into
sibling VAs. `cpp_score.py` checks all four pieces. On 0x10040D10 they
all matched independently — that is the proof the class is open, not the
97-byte score alone.

Do not treat a future 0-diff `.text` row as a filed `@implements` match
until the sidecar check is green too.

## What the harness needs

1. **A `.cpp` TU.** `cl` picks the language from the extension. A `.c`
   with `/GX` is still C (`__try` is SEH, already matched). `/EHs` is VC6+;
   VC5 is `/GX`.
2. **`/O2 /GX /MD`** (also try `/Od /GX /MD` and `/O2 /Oy- /GX /MD`).
   `/MD` is the DLL's CRT; `.drectve` becomes `-defaultlib:MSVCRT`.
   Keep `#define _CRTIMP __declspec(dllimport)` if the TU includes CRT
   headers. `__CxxFrameHandler` and `??_M` (`__ehvec_dtor`) stay
   undefined externs in the `.obj` — `E8`/`jmp` relocs, same as orig.
3. **Mangled lookup.** `parse_coff_obj` leaves `??1BrCtl@@UAE@XZ` intact
   (`undecorate` only strips `_Name@N`). Match `??0` ctor / `??1` dtor /
   `??_G` scalar deleting. Tag the `.cpp` with `@implements`, `@cpp_kind`,
   `@cpp_symbol`.
4. **`.text$x` labels.** Unwind actions are `IMAGE_SYM_CLASS_LABEL` in
   `.text$x`, not typed `.text` functions. `parse_coff_obj` drops them.
   `cpp_score.py` reads them. FuncInfo lives in `.xdata$x` (magic
   `0x19930520`), not `.rdata` of the function COMDAT.
5. **Layout from displacements, not relocs.** Vtable, call targets,
   handler, dtor-pointer, `fs:[0]` are masked. `+0x2B5C`, `+0x3838`,
   count `3`, size `0x438`, trylevel `0`/`-1` are immediates and must
   be exact. Compile-time `typedef char chk[...]` on offsetof.
6. **Declare member dtors, do not define them.** A visible empty dtor
   inlines / unrolls and you lose `__ehvec_dtor`. Undefined `~TextBox()`
   forces the call.
7. **Do not sweep the 80 as C.** `pipeline_triage.py` already buckets
   `6a ff`. The `64 a1` half of the 80 is the same class (this dtor is
   one). Screen: `6a ff` / `64 a1` in the first ~0x20 bytes, handler
   thunk `B8 <FuncInfo> E9` to `0x10074566`.

`cpp_score.py` is standalone (own `build/match/obj_cpp/`). It reuses
`match_diff.parse_coff_obj` and `match_sweep.score` / `load_orig`. It
does **not** edit `match_sweep.py`. Folding `/GX` into the C sweep is a
later, serialized tools change.

CLI:

```
python3 tools/cpp_score.py --va 0x10040D10
python3 tools/cpp_score.py --va 0x10040D10 --src build/cpp_work/0x10040D10.cpp --list
```

`score_source(src_text, func_name, orig_bytes, opts, tag)` is the
`_score_source` twin (writes `r.cpp`, never `r.c`).

## Proven idiom (0x10040D10)

Class with a virtual dtor, a member array of 3 non-trivial objects of
size `0x438` at `+0x2B5C`, and a later non-virtual non-trivial member at
`+0x3838`. `/O2 /GX /MD` emits:

```
mov eax, fs:[0]; push -1; push handler; push eax; mov fs:[0], esp
push esi; mov esi, ecx
mov [esi], vtable
lea ecx, [esi+0x3838]
mov [esp+0xc], 0          ; trylevel = 0  (if ~list throws, unwind array)
call ~TextList
push ~TextBox; push 3; add esi, 0x2B5C; push 0x438; push esi
mov [esp+0x1c], -1        ; trylevel = -1
call __ehvec_dtor         ; stdcall ??_M, callee pops 16
mov ecx, [esp+4]; pop esi; mov fs:[0], ecx; add esp, 0xc; ret
```

Ctor of the same class uses the other prologue schedule (`push -1` first,
then `mov eax, fs:[0]`). Both forms are one compiler, one TU, per-function
scheduling — not a flag split. Unwind action (outlined, `[ebp-0x10]` is
`this` in the EH frame):

```
push dtor; push 3; push 0x438
mov eax, [ebp-0x10]; add eax, 0x2B5C; push eax
call __ehvec_dtor; ret
```

Handler: `mov eax, offset FuncInfo; jmp __CxxFrameHandler`.
FuncInfo: magic `0x19930520`, maxState=1, nTryBlocks=0, nIPMapEntries=0,
unwind[0] toState=-1.

## Next 3 targets

The 80 are object-lifetime, not try/catch. Three unwind-action shapes:
`operator delete` (70), stack-dtor `lea ecx,[ebp-N]; jmp dtor` (8),
`__ehvec_dtor` (2 — this was the small one; 0x10040B10 is the other).

### 1. slice8_86 24-diff cluster (op-delete, maxState 10–23)

| VA | size | start | maxState | report.csv today |
|---|---:|---|---:|---|
| 0x100439B0 | 3746 | `6aff` | 23 | 24 diffs, recomp 32 B |
| 0x10044860 | 2439 | `6aff` | 15 | 24 diffs, recomp 32 B |
| 0x100451F0 | 1749 | `64a1` | 11 | 24 diffs, recomp 32 B |
| 0x100458D0 | 1565 | `64a1` | 10 | 24 diffs, recomp 32 B |
| 0x10045EF0 | 1834 | `6aff` | 11 | 24 diffs, recomp 32 B |

Workstream notes: a C body compile left **only the EH frame** (24 diffs).
**Today's tagged C is not that body** — `src/core/slice8_86.c` holds 32-byte
port adapters (`BrPhaseEnterPlaceholder_*` calling `BrExt_*` if a host
ctx is set). The 24 diffs are stub-vs-prologue on a 32-byte overlap.

To match: copy the real bodies (Ghidra / the prior C transcription) into
`build/cpp_work/<VA>.cpp`, spell each unwind state as a C++ `new T` (all
five actions are `push; call operator delete` / `0x1007456C`), compile
`/GX`. One FuncInfo per function, maxState = number of live `new`s.

### 2. 0x10056260 BrUiBootPreLoopGate (8,349 B, 97% body, 230 diffs)

`src/core/startup/br_uiboot.c`, report.csv `diff /O2 orig=8349 recomp=256
diffs=230`. Two unwind states, both `push eax; call operator delete`.
The `new` of the 0xC8 phase object (concern E) plus one more allocation
(concern F, 0x400 singleton). Port already has the behaviour split across
helpers; matching needs **one** C++ function with those two `new`s so
the compiler emits maxState=2. Body was 97% C-matched — the 230 diffs
are the EH prologue plus the trylevel stores.

### 3. Stack-dtor eight

Unwind `lea ecx,[ebp-N]; jmp dtor` — a named local of class type, not
`new`. Smallest first: 0x10004C80 (167 B). Then 0x10004900 / 0x10004AD0 /
0x10004FD0 / 0x100051C0 (300–370 B), 0x1002F790 (2517 B), 0x100038F0
(3799 B, one 0x760-byte stack object, dtor 0x10008D60), 0x10063060
(1104 B, 797 diffs in `br_cfgfile.c`).

After those: the 201 B `new T` twins (~15 functions, maxState=1,
op-delete — cheapest op-delete proof, same 10-byte thunk as this dtor)
and the high-maxState UI ctors (0x100498A0 / 0x1004AEE0 maxState=24,
0x10051600 maxState=25).

## What not to do

- Do not re-derive 0x10040D10. The `.cpp` is the source; `/O2 /GX /MD`
  is the opt; all four pieces matched.
- Do not C-sweep the 80. `6aff`/`64a1` in the first ~0x20 bytes → `.cpp`.
- Do not confuse this with the 3 SEH helpers (`_except_handler3` via
  `0x10074AE6`) — those are C `__try` and already matched.
- Header edits stay serialized. This harness created no `include/` churn.
