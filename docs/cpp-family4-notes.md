# C++ family 4 — small/mid EH (201 B installers + three other shapes)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not `@implements`-tag these in `src/` this session.

## Matched: 11 / 11, all four pieces 0

2,416 / 480,853 of BRGlide.dll `.text` (0.50%).

The family-1 thiscall `new Phase` installer covered **8 / 11**. The other
three are a stack-dtor, a DirectInput `new T`, and a table-driven `new Node`
loop (still maxState=1).

| VA | size | kind | score |
|---|---:|---|---|
| 0x10004C80 | 167 | stack-dtor, cdecl 2-arg | **0** |
| 0x1003CA10 | 201 | family-1 installer | **0** |
| 0x1003D2F0 | 201 | family-1 installer | **0** |
| 0x10071FC0 | 213 | refcount + `new DiDev` 0x54 | **0** |
| 0x10058D40 | 215 | `delete` list, loop `new Node` 0x14 | **0** |
| 0x1003D3C0 | 216 | installer + shared NetFn tail | **0** |
| 0x1003D930 | 222 | 3 stores then installer + pfnHook | **0** |
| 0x1003F610 | 239 | ResetBuf + MusicFn then installer | **0** |
| 0x1003E4A0 | 244 | ResetBuf + PrepFn; extra cdecls on new path | **0** |
| 0x1003ED90 | 244 | CD-gated installer | **0** |
| 0x1003DC20 | 254 | installer + host first-time tail | **0** |

Sidecar (FuncInfo magic `0x19930520`, maxState=1, nTry=0, unwind toState=-1,
11 B unwind action, 10 B handler) matched on every VA.

## Shared installer (8 of 11)

Same source as `docs/cpp-family1-notes.md`. Ctor DECLARED, no dtor, thiscall
member, `if (p == 0) { new Phase; … } else { g_cur = p; } return 1` AFTER
the if/else.

Prologue extras sit before the slot test (or, for a shared tail, after both
arms). Do not invert `if (p != 0)`. Do not `return 1` inside both arms.

| VA | extra |
|---|---|
| 0x1003CA10 / 0x1003D2F0 | none (201 B twins) |
| 0x1003D3C0 | after both arms: `if (!a && !b && (m==0 \|\| m==1)) NetFn();` |
| 0x1003D930 | `p = slot; g_host=1; g_kind=2; g_flag=0;` then activate; then `g_hookOwner->pfnHook = HookFn` |
| 0x1003F610 | `ResetBuf; MusicFn(3, 0x200020); p = slot; g_track=3;` (store interleaved after the test) |
| 0x1003E4A0 | `ResetBuf; g_mode=1; PrepFn;` extra 3 cdecls **only on the new path**; `return 1` still after the if/else |
| 0x1003ED90 | see CD polarity below |
| 0x1003DC20 | see host-tail return below |

## Idioms (solved once)

- **CD / large-vs-small polarity.** Orig `test; je far` to a small fail
  block at the end. Spell `if (CdCheck() != 0) { success; return 1; }
  ResetBuf(GetStr(0xD)); return 0;`. `if (CdCheck() == 0) { fail; return 0; }
  success` inverts to a short `jne` and 144 diffs. Alloc-fail `return 0`
  still shares the xor-eax epilogue past the GetStr.
- **Live-eax retest (0x1003DC20).** Orig `mov ecx,[g_inited]` then, on the
  already-inited path, `test eax,eax; je` of the still-live `g_host`. Nested
  `if (host) { if (!inited) {…} HostGo(); }` CSE's the second test and
  reuses eax for `g_inited` (47 diffs). Spell the first-time path with
  `HostGo(); return 1;` and a **second** top-level `if (g_host) HostGo();`
  so eax stays live across the inited load. `/O2` merges the two HostGo +
  epilogues into orig's `jmp` / `test eax` diamond.
- **Volatile dword load of a byte-used int (0x10004C80).** Orig
  `mov ecx,[g]; and cl,0xf; or cl,0xd0`. Plain `int g` shrinks to
  `mov cl,[g]` (1 diff). `volatile int g` keeps the dword load. Ctor/dtor
  of the 0x214 stack `Buf` DECLARED; unwind is `lea ecx,[ebp-0x220]; jmp
  dtor`. Free cdecl (no unused-this `push ecx`).
- **DirectInputCreateA is not dllimport (0x10071FC0).** Orig `E8` to a JMP
  thunk. MessageBoxA **is** dllimport (`FF 15`). `if (++refcount != 1)
  return 1;` then `if (DirectInputCreateA(hinst, 0x500, &pDI, 0) < 0)
  { MessageBoxA(hwnd, GetStr(0x127), GetStr(0x126), 0x10); return 0; }`.
  `new DiDev` (0x54), **no** NULL-return after new, then `p->Init(hwnd)`.
- **Table walk at `.h` (0x10058D40).** `esi` starts at `&g_tab[0].h`
  (`[esi]=h`, `[esi-4]=w`). `for (d = &g_tab[0].h; (int)d < (int)&g_tab[7].h;
  d += 2)` — the `(int)` compare is orig `jl`, not pointer `jb`. `delete`
  the old head (DECLARE `~Node()`), then `new Node` per row whose
  `w*h*6 <= (g_mem << 20)`. maxState=1: one live `new` at a time. `f8=0x10`,
  `fC=0`; append via thiscall `ret 4`.

Unwind actions: op-delete 11 B for every `new T` VA; stack-dtor 11 B
`lea ecx,[ebp-0x220]; jmp dtor` for 0x10004C80. Handler
`mov eax, FuncInfo; jmp __CxxFrameHandler` on all 11.

## What not to do

- Do not invert `if (p == 0)` / `if (CdCheck() != 0)` (large path is
  fall-through).
- Do not `return 1` inside both installer arms.
- Do not dllimport `DirectInputCreateA` or `operator new`.
- Do not define Phase/Buf/Node/DiDev ctors or dtors in the TU.
- Do not C-sweep these. `6aff` in the first ~0x20 bytes → `.cpp`.
