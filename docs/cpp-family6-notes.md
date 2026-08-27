# C++ family 6 — large EH functions (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not C-sweep these. Do not `@implements`-tag in `src/`
this session. Do not permute register coloring.

Two classes:

1. **Page builders** (`new Screen` 0x348 + N `new Ctl` 0x1E214). Unwind
   is 11-byte `operator delete`. maxState = 1 + N. Same family as
   0x1004F8C0 / 0x1004DA00 / 0x100439B0.
2. **Stack-dtor pair** (named local `Stream` sizeof 0x760). Unwind is
   `lea ecx,[ebp-0x760]; jmp dtor` (dtor 0x10008D60 = empty `ret`).

## Page builders — sidecar landed, body is a coloring wall

Ctl() **declared, not defined**. Same idiom as family 3: without it,
`new Ctl` inlines the vptr store and maxState collapses to 1.

| VA | size | maxState | news | .text | FuncInfo | unwind | handler | first .text diff |
|---|---:|---:|---:|---|---|---|---|---|
| 0x1004F290 | 1570 | 9 | 1+8 | 1214 diffs | **MATCH** | **9/9** | **MATCH** | +0x19 `mov esi,arg; push edi` vs `push edi; mov edi,arg` |
| 0x100504A0 | 1558 | 9 | 1+8 | 1195 diffs | **MATCH** | **9/9** | **MATCH** | +0x19 same esi-vs-edi phase coloring |
| 0x1004BE00 | 3475 | 21 | 1+20 | 2579 diffs | **MATCH** | **21/21** | **MATCH** | +0x17 prologue (`sub esp,0xc` + ebx/ebp 0/1) |
| 0x1004AEE0 | 3862 | 24 | 1+23 | 2909 diffs | **MATCH** | **24/24** | **MATCH** | +0x17 same |
| 0x1004CBA0 | 3671 | 21 | 1+20 | 2557 diffs | **MATCH** | 18/21 | **MATCH** | +0x17; unwind[6..8] `[ebp-0x50]` vs `[ebp-0x10]` |
| 0x100498A0 | 3993 | 24 | 1+23 | 2876 diffs | **MATCH** | 21/24 | **MATCH** | +0x17; unwind[6..8] same `[ebp-0x50]` |

Sources: `build/cpp_work/<VA>.cpp`. 0x1004F290 / 0x100504A0 are full
Place/SetText/edit-box transcriptions (strlen+strcpy of the name field).
The four large ones have every `new` and every Place/SetText/hook from
the orig bytes; leftover is the coloring wall.

### 0x1004F290 — multiplayer name (full body)

fX=190, fY=130. StrGet 0x5C / 0x3C / 0x1E / 0xC / 0xC0. Edit box
0x200001 at (fX, 174), rect 0xBA/0x13C/0xAC/0xBC. Continue published
to a global **before** cCtl++. Player-name label does **not** bump cSel.
`if (strlen(name) <= 1u) strcpy(name, StrGet(0xC0)); strcpy(tbstr, name);`

Orig prologue (after EH `push ecx`):

```
push ebx; push ebp; push esi
mov esi, [esp+0x20]     ; phase
xor eax, eax
push edi
mov ax, [esi+0x10]
xor ebx, ebx
mov edi, 1
```

Recomp pushes edi with the other callee-saves and parks phase in edi.
Then every `[esi+…]` vs `[edi+…]` is a cascade. Same wall as family 3
ebx-vs-ebp on 0x1004F8C0. Do not permute.

### 0x100504A0 — create-game (full body)

Twin of 0x1004F290 with extra `ResetSlots()` (0x10051550) **before**
`flags[nPages]=1`. fX=195. StrGet 0x62 / 0x63 / 0x1E / 0xC / 0xC1.
Game-name label **does** bump cSel. Default-name copy goes straight
into the text box, not into the saved-name buffer:

```
src = (strlen(gameName) > 1u) ? gameName : StrGet(0xC1);
strcpy(tbstr, src);
```

Rect 0xC5/0x135/0xAC/0xBC. Orig `xor ebx,ebx` before `push edi` (still
esi = phase, edi = 1). Same coloring wall.

### Four large builders

All `int f(Phase *p)` cdecl, Screen fX=195 fY=130. Virtual Place at
vtbl+0x38, SetText at +0x34. `fild` of two int globals for the three
0x402001 rects (a7 = 0x78 / 0x52 / 0x54), then y `fsub` of -33.0.

| VA | extra prologue | page +4/+8 | list ctl (flags 0x3001) |
|---|---|---|---|
| 0x1004BE00 | `g_dst = g_src`; `sub esp,0xc` | 0x10039F30 / 0x10039F60 | no |
| 0x1004CBA0 | `g_dst = g_src`; `sub esp,0x4c` | same | yes, pfn04=0x10038150 |
| 0x100498A0 | `sub esp,0x4c` | no | yes, pfn04=0x10038150 |
| 0x1004AEE0 | `sub esp,0xc` | no | no |

0x1004BE00 / 0x1004AEE0: extra locals are the three rect temps
(`fA`/`fB`/`iA`) → `sub esp,0xc` → **every** unwind action is
`[ebp-0x10]` and matches. Contrast family 3's 0x1004DA00, which had
the same `sub esp,0xc` but three actions reading `[ebp+4]`.

0x1004CBA0 / 0x100498A0: orig `sub esp,0x4c` (list-fill temps). Three
middle unwind actions (the rect `new`s) read `[ebp-0x50]` (`8b 45 b0`)
vs recomp `[ebp-0x10]` (`8b 45 f0`). FuncInfo maxState / toState / the
other 18 or 21 actions / handler still match. Same extra-slot class as
0x1004DA00. Do not permute.

## Stack-dtor pair — FuncInfo + handler MATCH, unwind 1-byte wall

Both unwind actions:

```
lea ecx, [ebp - 0x760]     ; orig  8d 8d a0 f8 ff ff
jmp 0x10008D60             ; empty thiscall dtor (`ret`)
```

Recomp emits `[ebp-0x76C]` (`8d 8d 94 f8 ff ff`) — 0xC extra, the EH
node size. Mapped pads (`volatile char extra[0x30]` / `[0x34]`,
matching orig `sub esp,0x790` / `0x794`) did **not** flip it.

| VA | size | args | orig frame | .text | FuncInfo | unwind | handler |
|---|---:|---|---|---|---|---|---|
| 0x1002F790 | 2517 | cdecl 3; ctor uses arg1,arg2 | `sub esp,0x790` | 86 diffs | **MATCH** | 1 byte (`-0x76C` vs `-0x760`) | **MATCH** |
| 0x100038F0 | 3799 | cdecl 5 (`add esp,0x14` from 0x1002F790) | `sub esp,0x794` | 84 diffs | **MATCH** | same 1 byte | **MATCH** |

Ctor is 0x1006CDA0: thiscall, 2 stack args, `ret 8`. Writes
`this[0,4,8]=0`, `this+0xC=arg2`, `this+0x10=arg1`. DECLARE, do not
define. Dtor DECLARE, do not define (a visible empty dtor inlines the
unwind to `ret` and you lose the `lea; jmp`).

0x1002F790 **calls** 0x100038F0. Both are mutex/WaitForSingleObject
packet workers on the same 0x760 Stream (methods 0x1006CDD0 / CE00 /
CE50 / CE80 / CF80). Body is a jump-table on `(byte & 0xE0)`. Frame
first; the remaining body is a separate transcription, gated on the
unwind displacement.

## Proven idioms (this family)

- **Page builder sidecar is `Ctl()` declared.** maxState = `new` count,
  every toState=-1, every unwind 11-byte `??3`. Four of six landed
  100% of unwind actions (0x1004F290, 0x100504A0, 0x1004BE00,
  0x1004AEE0).
- **`sub esp,0xc` (three extra slots) can still have every unwind
  `[ebp-0x10]`.** 0x1004BE00 and 0x1004AEE0 prove it. 0x1004DA00's
  `[ebp+4]` three-pack is not automatic for this prologue.
- **`sub esp,0x4c` (list builders) parks three middle new-pointers at
  `[ebp-0x50]`.** Distinguisher `8b 45 b0` vs `8b 45 f0`. 0x1004CBA0 /
  0x100498A0. Classified; do not permute.
- **Edit-box name seed is two shapes.** 0x1004F290 copies into the
  saved-name buffer then into the text box. 0x100504A0 copies into the
  text box from either the buffer or `StrGet`. Both are
  `cmp ecx,1 / ja` on inlined `strlen` (`#pragma intrinsic`).
- **ResetSlots before flags=1** is 0x100504A0 only. Spell it C++
  linkage (not `extern "C"`) so later trylevel=-1 stores survive.
- **Stack object unwind is `lea ecx,[ebp-N]; jmp dtor`.** N = sizeof
  when the object sits immediately below the EH node in the funclet's
  ebp. A TU with only that local emits N+0xC. DECLARE ctor and dtor.
- **0x10008D60 is an empty thiscall dtor (`ret`), shared.** The next
  function at 0x10008D70 is a stdcall path-strip, not the dtor.
- **0x1002F790 → 0x100038F0 is cdecl 5 args**, not stdcall WndProc
  (survey.csv `wndproc` tag is a classification miss). Orig `ret` with
  `add esp,0x14` at the call site.

## What not to do

- Do not permute esi/edi (0x1004F290 / 0x100504A0) or ebx/ebp (the
  four large ones). The wall is classified.
- Do not dllimport `operator new`.
- Do not `new char[N]` for anything here.
- Do not define Stream/Screen/Ctl constructors or dtors in the TU.
- Do not C-sweep the 80. `6aff` in the first ~0x20 bytes → `.cpp`.
- Do not re-run mapped extra-slot probes on 0x1004CBA0 / 0x100498A0
  (`[ebp-0x50]` vs `[ebp-0x10]`) or mapped 0xC unwind pads on the
  stack-dtor pair.
