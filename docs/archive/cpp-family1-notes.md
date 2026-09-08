# C++ family 1 — 201 B `new Phase` UI installers (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not `@implements`-tag these in `src/` this session.

These are **not destructors**. Unwind is `operator delete` (maxState=1):
`new Phase` if the ctor throws. Bodies are the C `BrOptEnsureObj` /
`BrPhaseActivateSlot` sequence; the C tree compiled them as 32-byte stubs.
The missing 12–23 diffs were the EH frame.

## Matched: 14 / 14, all four pieces 0

2,836 / 480,853 of BRGlide.dll `.text` (0.59%). 13×201 B identical
instruction stream (relocs differ: slot, enter hook, handler). 0x1003E370
is 223 B with two extra global copies.

| VA | size | slice | score |
|---|---:|---|---|
| 0x1003C7B0 | 201 | slice2_25 | **0** |
| 0x1003C880 | 201 | slice8_83 | **0** |
| 0x1003D220 | 201 | slice2_25 | **0** |
| 0x1003D620 | 201 | slice2_25 | **0** |
| 0x1003E660 | 201 | slice2_26 | **0** |
| 0x1003E810 | 201 | slice2_26 | **0** |
| 0x1003E8E0 | 201 | slice2_26 | **0** |
| 0x1003EA70 | 201 | slice2_26 | **0** |
| 0x1003EB40 | 201 | slice2_26 | **0** |
| 0x1003EF90 | 201 | slice3_31 | **0** |
| 0x1003F060 | 201 | slice3_31 | **0** |
| 0x1003F340 | 201 | slice3_31 | **0** |
| 0x1003F540 | 201 | slice3_31 | **0** |
| 0x1003E370 | 223 | slice4_50 | **0** |

Shared pattern covered **all 14**. Sidecar (FuncInfo / 11 B unwind /
10 B handler) matched as soon as `new Phase` had a declared ctor.

## Class layout

```
+0x00  void *vtbl          ; ctor-set, this TU does not store it
+0x04  void (*pfnEnter)(Phase *)   ; cdecl, one arg (the phase)
+0x08  void *pfnHook
+0x0C  int f0C             ; set 1 on just-built path
+0x10  pad 0x58
+0x68  int f68             ; set 1 on just-built path
+0x6C  pad 0x5C
sizeof = 0xC8
```

Compile-time `typedef char chk[...]` on offsetof. Same 0xC8 object as
`br_phase.h` BrPhase_.

## Proven source

```cpp
class Phase {                 // sizeof 0xC8
    void *vtbl;
    void (*pfnEnter)(Phase *);
    void *pfnHook;
    int f0C;
    char _[0x58];
    int f68;
    char _rest[0x5C];
    Phase();                  // DECLARED, not defined
    // no dtor
};

int Ctl::Activate()           // thiscall member — unused `this` is push ecx
{
    Phase *p = g_slot;
    if (p == 0) {
        p = new Phase;        // push 0xC8; ??2; ctor; trylevel 0 then -1
        g_slot = p;
        g_cur = p;            // both written even when p is NULL
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);  // re-read slot; cdecl add esp,4
        g_cur->f0C = 1;            // re-read cur
        g_cur->f68 = 1;            // re-read cur again
    } else {
        g_cur = p;
    }
    return 1;                 // AFTER the if/else, not inside both arms
}
```

0x1003E370 only extra, before the `if`:

```cpp
g_dst1 = g_src1;
p = g_slot;
g_dst2 = g_src2;
```

No named temps (`int t0 = g_src1`). Temps keep eax/ecx live and recolor
the post-call flag stores (18 diffs, `mov edx,[g_cur]` vs `mov eax,[g_cur]`).

## Idioms (solved once)

- **These are `new T` installers, not dtors.** Unwind action is the 11-byte
  `mov eax,[ebp-0x10]; push eax; call operator delete; pop ecx; ret`.
- **Ctor declared, not defined.** Visible empty ctor inlines the vptr store
  and drops EH (maxState collapses). Same as 0x10040D10's "declare member
  dtors, do not define".
- **Do not declare `~Phase()`.** A dtor would add a dtor call to the unwind
  action; orig is operator delete only.
- **`return 1` after the if/else.** `return 1` inside both arms — or
  `if (p) { existing; return 1; } new` — CSE's the constant 1 into the flag
  stores (`89 r/m,eax` vs orig `c7 r/m,1`). Shared return lets `/O2`
  duplicate the EH epilogue and keeps the `c7` immediates.
- **Thiscall member, not a free function.** `push ecx` after the EH node is
  unused `this`, then overwritten with the `new` pointer (`mov [esp],eax`).
  A free `int f()` does not emit that `push ecx`.
- **Re-read `g_slot` for the enter call, `g_cur` for each flag store.**
  An enter hook that re-points `g_cur` stamps the flags on the hook's phase.
- **Two extra copies: write `g_dst = g_src`, do not name temps.**

Unwind: magic `0x19930520`, maxState=1, nTryBlocks=0, unwind[0] toState=-1.
Handler: `mov eax, FuncInfo; jmp __CxxFrameHandler`.
