# C++ family 2 — `new T` activate + screen builders (2026-08-27)

Harness: same as Task 1 (`build/cpp_work/<VA>.cpp`, `cl /O2 /GX /MD`,
`python3 tools/cpp_score.py --va <VA>`). Do not `@implements`-tag these in
`src/` this session.

## Matched: 6 activate hooks, 1,264 / 480,853 of `.text` (0.26%)

All four cpp_score pieces 0 on each (function `.text`, FuncInfo, unwind
action, handler thunk).

| VA | size | prologue | score |
|---|---:|---|---|
| 0x1003E9B0 | 184 | ResetBuf none; TailFn; shared return | **0** |
| 0x1003F260 | 212 | `g_dst = g_src` | **0** |
| 0x1003E0E0 | 214 | `ResetBuf(&g_buf)` | **0** |
| 0x1003E730 | 214 | `ResetBuf(&g_buf)` | **0** |
| 0x1003D140 | 219 | `ResetBuf(&g_buf); PrepFn()` | **0** |
| 0x1003E250 | 221 | `p = slot; g_c20 = 0; g_c24 = 0` | **0** |

### Proven source (thiscall member, maxState=1, op-delete)

```cpp
class Phase {               // sizeof 0xC8, ctor DECLAREd, no dtor
    void *vtbl;             // +0
    void (*pfnEnter)(Phase *); // +4  cdecl
    void *pfnHook;          // +8
    int f0C;                // +0xC
    char _[0x58];
    int f68;                // +0x68
    Phase();
};

int Ctl::Activate()
{
    Phase *p = g_slot;
    // optional prologue (copy, ResetBuf, zeros) — see per-VA
    if (p == 0) {
        p = new Phase;          // operator_new(0xC8) + ctor, trylevel 0/-1
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);   // re-read slot; cdecl, add esp,4
        g_cur->f0C = 1;             // re-read cur
        g_cur->f68 = 1;             // re-read cur again
    } else {
        g_cur = p;
    }
    // 0x1003E9B0 only: TailFn();
    return 1;                       // AFTER the if/else — not inside both arms
}
```

Polarity is `if (p == 0) { new-path } else { existing }` with **one**
`return 1` after the statement. `if (p != 0) { existing; return 1; } new`
inverts to a short `je` and CSE's `1` into the flag stores (`89` vs `c7`).
`return 1` inside both arms does the same CSE. Shared `return 1` lets `/O2`
duplicate the EH epilogue and keeps `c7 41 0c 01` / `c7 42 68 01`.

0x1003E9B0 adds `TailFn();` before that `return 1` (12-byte cdecl,
`neg/sbb/neg` of a callee, result discarded).

0x1003E250: load slot **then** the two zero stores (`p = g_slot; g_c20 = 0;
g_c24 = 0; if (p == 0)`). Compiler interleaves `test` between the stores.

Unwind action (11 B): `mov eax, [ebp-0x10]; push eax; call ??3; pop ecx; ret`.
Handler: `mov eax, FuncInfo; jmp __CxxFrameHandler`. FuncInfo magic
`0x19930520`, maxState=1, nTryBlocks=0, unwind[0].toState=-1.

`push ecx` after the EH node is the thiscall `this` slot, overwritten with
the `new` pointer (`mov [esp], eax`). Unused `this` is still required —
a free `int Activate()` does not emit `push ecx`.

Do not declare `~Phase()`. A dtor would add a dtor call to the unwind
action; orig is operator delete only.

## Big builders: sidecar MATCH, `.text` not 0

| VA | size | maxState | news | `.text` | FuncInfo | unwind×N | handler |
|---|---:|---:|---:|---|---|---|---|
| 0x100439B0 | 3746 | 23 | 1×0x348 + 22×0x1E214 | 2846 diffs | **MATCH** | **23/23** | **MATCH** |
| 0x10044860 | 2439 | 15 | 1×0x348 + 14×0x1E214 | 1841 diffs | **MATCH** | **15/15** | **MATCH** |
| 0x10045EF0 | 1834 | 11 | 1×0x348 + 10×0x1E214 | 1369 diffs | **MATCH** | **11/11** | **MATCH** |

8,019 combined orig bytes. The C 24-diff rows were stub-vs-EH (32 B
recomp). `/GX` now emits the frame: one FuncInfo, maxState = number of
`new`s, every unwind `toState=-1`, every action `push; call ??3; pop; ret`.
That is the 24-diff payoff. Do not C-sweep these.

Bodies: slice3_33.c as C++ (`new Screen` / `new Ctl`, globals not an
injected ctx, no NULL-safety returns). Ctor of Screen is 0x100418C0, Ctl
is 0x10040B10 (DECLARE, do not define). f34/f38 are virtual thiscall at
vtbl +0x34 / +0x38 (13 dummy virtuals before f34). BrStrGet is cdecl 1
arg; f34 is thiscall 4 stack args — `/O2` interleaves `push style; push
1; push 1; push id; call BrStrGet; add esp,4; push eax; call [vtbl+0x34]`.

### `.text` wall — do not re-probe

First divergence is the 0/1-register pair:

```
orig:    xor eax,eax; xor ebx,ebx; mov ebp,1    ; ebx=0, ebp=1
recomp:  xor eax,eax; xor ebp,ebp; mov ebx,1    ; ebp=0, ebx=1
```

Then `mov [edi+0x12], bx` vs `bp`, `aF6C[i]=1` via ebp vs ebx, `cmp esi,ebx`
vs `ebp`, and the `xor eax; mov cx; cmp; sete al; test al; je` null-check
vs a flags-preserving `cmp; jne`. Size gap ≈ 8 B × nNews (sete sequence).
Mapped probes that did **not** flip ebx/ebp: local `int z = 0`, `int iA=0`
inits, `fail = (p==0)` / `volatile fail`, swapping `f12=0` vs `aF6C=1`,
sibling enter hooks in the same TU, `float fRow = 0`. Fresh lead only.

0x10045EF0 also parks the phase in ebp (`mov ebp, [esp+0x1c]` before
esi/edi pushes). 439B0/44860 use edi.

## What not to do

- Do not define Phase/Screen/Ctl constructors or dtors in the TU.
- Do not `return 1` inside both activate arms.
- Do not invert to `if (slot != 0)`.
- Do not C-sweep the 80 `__CxxFrameHandler` functions.
- Do not tag `src/` until a filing session.
