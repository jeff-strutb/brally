# C++ family 5 — small/mid EH (installers, ctors, stack-dtor) (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not `@implements`-tag these in `src/` this session.

## Scoreboard (10 / 10 have sidecar; 1 / 10 is 0-diff `.text`)

3,396 / 480,853 of BRGlide.dll `.text` (0.71%) in this batch.

| VA | size | kind | `.text` | FuncInfo | unwind | handler |
|---|---:|---|---|---|---|---|
| 0x1003F700 | 284 | `new Phase` extra-prologue | **0** | **MATCH** | **MATCH** | **MATCH** |
| 0x1003F410 | 293 | two `new Phase` | 82 | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x1003F130 | 293 | two `new Phase` | 82 | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x10004900 | 309 | stack-dtor | 178 | **MATCH** | **MATCH** | **MATCH** |
| 0x10041B60 | 309 | Phase ctor, two `new NameList` | 2 | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x1003D7D0 | 340 | join + `new Phase` | 199 | **MATCH** | **MATCH** | **MATCH** |
| 0x10004FD0 | 360 | stack-dtor + ring | 230 | **MATCH** | **MATCH** | **MATCH** |
| 0x10004AD0 | 362 | stack-dtor 10-arg | 235 | **MATCH** | **MATCH** | **MATCH** |
| 0x100051C0 | 368 | stack-dtor + ring | 230 | **MATCH** | **MATCH** | **MATCH** |
| 0x10040B10 | 478 | BrCtl ctor, `__ehvec_ctor` | 295 | **MATCH** | **MATCH** (27 B ehvec_dtor) | **MATCH** |

Shared installer pattern covered **1 / 4** `.text` (0x1003F700) and **4 / 4** sidecars
(F700, F130, F410, 3D7D0). Stack-dtor / ctor sidecars all landed on the first
compile that spelled the object lifetime.

## Shared installer pattern (family 1/2, this batch)

thiscall member, ctor DECLARED not defined, no `~Phase()`, `new Phase`
(0xC8). Unwind is the 11-byte `mov eax,[ebp-0x10]; push eax; call ??3; pop; ret`.
`push ecx` after the EH node is unused `this`.

```cpp
int Ctl::Activate()
{
    Phase *p = g_slot;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);   // re-read slot
        g_cur->f0C = 1;             // re-read cur
        g_cur->f68 = 1;
        // optional just-built tail
        return 1;
    } else {
        g_cur = p;
        return 1;
    }
}
```

### 0x1003F700 — MATCH, all four pieces

Prologue stores (mode=2, ResetBuf, zeros/ones, 0xFF) **before** the slot test
force `mov esi,1`. Flag stores are `mov r/m, esi` not `c7`. Just-built tail
(three calls + hook at `obj+8`) lives **inside** the new arm, so each arm
`return 1` (duplicated EH epilogue). That is why this 284 B function CSE's 1
into the flags where the 201 B family must not.

### Two-new twins (0x1003F130 / 0x1003F410) — sidecar MATCH

Second `new Phase` runs only on the just-built path of the first. Second
writes **only its own slot + f0C** (no `g_cur`, no f68). Trylevel for the
second new is `mov [esp+N], esi` (1) then `-1`. maxState=2, both toState=-1.

`.text` first diff @ +0x1E: orig `test eax,eax; mov esi,1; jne existing`,
recomp `test; jne` and loads esi=1 after the first enter. Existing path is
`mov eax,1` vs orig `mov eax,esi`. Same CFG; the hoist of the 1-register
across the branch did not fire. `int one = 1` folds. Do not permute.

### 0x1003D7D0 — sidecar MATCH

Same installer after a join preamble (`g_inGame`, mode 2/3, inlined strlen
`repne scasb`, word at `+0x1E164` via `jbe`). Orig early `return 1` is
`jmp` to one shared epilogue (`75 74` short to activate). Recomp inlines the
EH epilogue on the first early return (near `0f 85`, +16 B), so the short
`jne` cannot form. Sidecar is the 11-byte op-delete.

## Phase ctor 0x10041B60 — sidecar MATCH, 2 SIB diffs

Two `new NameList` (0x6594, ctor DECLARED). maxState=2, both unwind `??3`.
`+0x04` is **not** written. `nPages` / `iPage` / `fBC` are **words**.
`vtbl = &g_vtbl` emits `c7 03 <reloc>`; `vtbl = g_vtbl` (pointer load) is
`a1; 89 03` (18 diffs). `memset(aFlags, 0, 0x50)` with `#pragma intrinsic`
lands the `rep stosd` (count in ecx first). BrStrGet / ErrShow are C++
linkage so trylevel=-1 after the second new stays. sprintf is `/MD`
dllimport (`FF 15`).

Remaining 2 diffs, both SIB encoding of the same address:

```
orig:    lea ecx, [eax+esi+4]     ; 8D 4C 30 04
recomp:  lea ecx, [esi+eax+4]     ; 8D 4C 06 04
```

and the twin on list1 (`8D 44 32 04` vs `8D 44 16 04`). Pointer + offset + 4;
allocator-internal base/index swap. Mapped spellings that did not flip it:
`pList0->asz[0]+off`, `(char *)pList0+off+4`, `&((char *)pList0)[off+4]`,
named `char *dst`. Do not re-probe those.

## BrCtl ctor 0x10040B10 — sidecar MATCH (the other `__ehvec_*`)

Unwind is 27 B `__ehvec_dtor(this+0x2B5C, 0x438, 3, ~TextBox)` — same action
as 0x10040D10's dtor sidecar. FuncInfo maxState=1, toState=-1. That is the
ctor/dtor pair for the 0x1E214 control.

`.text` first diff @ +0x18: orig `xor ebx,ebx` then POD stores interleaved
with `__ehvec_ctor` argument pushes; recomp runs **both** member ctors
(array then TextList) then the whole body. VC5 C++ still constructs members
before the body; orig software-pipelines independent POD stores into the
ehvec / list-ctor *setup*. Named fields (so stores do not alias the array
via `char *`) and a body that is only the pre-ehvec scalars did **not**
move those stores. Wall: do not permute. Layout (boxes @ +0x2B5C, list @
+0x3838, sizeof 0x1E214, pack(1) for `a012A` at +0x12A) is correct — the
sidecar displacements prove it.

## Stack-dtor four — sidecar MATCH

Unwind on all four: `lea ecx, [ebp-0x220]; jmp 0x10008D60`. 0x10008D60 is
one-byte `ret` (empty dtor). **Declare `~Pkt()`, do not define it** or the
call / unwind vanish. sizeof 0x214. Free cdecl (no `push ecx` after the EH
node) — not a thiscall member.

```cpp
class Pkt {
    char b[0x214];
public:
    Pkt();          // 0x1006CD80, DECLARE
    ~Pkt();         // 0x10008D60, DECLARE
    void Reset();   // 0x1006CDC0, ret
    void PutByte(unsigned);  // 0x1006CFA0, ret 4
    void Put24(unsigned);    // 0x1006D000, ret 4
    void Put32(unsigned);    // 0x1006D050, ret 4
};
```

| VA | what |
|---|---|
| 0x10004900 | Reset, Put24(0), flag byte `(g_id & 0xf) \| flags \| 0xe0`, 24-byte name, Send |
| 0x10004AD0 | InitPkt (0x10004C40), 10 cdecl args (D3D twin BrNetSend4760), kind=`a8&0x3f` |
| 0x10004FD0 | WaitForMultipleObjects on per-player 0x978 ring, flag 0x40, EncodeFull, SendTo |
| 0x100051C0 | same ring, flag 0x80, EncodeDelta(ref, state, pkt) |

Pkt must be **declared after** the ring/mutex work on 04FD0/051C0 or its ctor
runs too early. Ring stride 0x978, index at +0x55C, flags at +0x38, slots at
+0x58 stride 0xA0 — those displacements now match; remaining `.text` is
ebx-as-ring vs esi and a 4-byte extra frame. 04900 flag combine is
`and al,0xf; or al,bl; or al,0xe0` (byte ops after a dword load of g_id);
recomp still emits `and eax,0xf` / `and ecx,0xff`.

## Idioms (solved once)

- **Installer with live 1:** prologue stores of 1 before the slot test keep
  `esi=1` for the whole function. Flag stores become `89` not `c7`. `return 1`
  **inside** both arms (just-built tail cannot sit after the if/else). Opposite
  of the 201 B family's shared-return / `c7` rule — both are one compiler.
- **Two `new`s, second nested in the just-built arm:** maxState=2, both
  toState=-1, both unwind `??3`. Second object: own slot only, f0C only.
- **Phase ctor:** two `new NameList` (0x6594). Word fields at +0x10/+0x12/+0xBC.
  `vtbl = &symbol` not a loaded pointer. `#pragma intrinsic(memset)` for the
  20-dword `aFlags` fill. C++ linkage on post-`new` calls keeps trylevel=-1.
- **SIB `[base+index+disp]` vs `[index+base+disp]`** on `sprintf(list+4+off)`
  is not source-reachable from the spellings above. 2 diffs, classified.
- **Stack-dtor:** named local of class type, dtor DECLARED. Unwind
  `lea ecx,[ebp-0x220]; jmp empty_dtor`. Free function, not thiscall.
  Construct the local at the source point the orig constructs it.
- **BrCtl ctor sidecar is `__ehvec_dtor` of boxes**, not op-delete. Member
  array + later TextList. Body-store interleaving with ehvec setup is a wall.

## What not to do

- Do not define Phase/NameList/Pkt/TextBox/TextList ctors or dtors in these TUs
  (except the function under test).
- Do not invert `if (p == 0)` on the installers.
- Do not dllimport `operator new` (`new T` without `<new.h>` is the E8 thunk).
- Do not C-sweep these 10. `6aff` in the first ~0x20 bytes → `.cpp`.
- Do not re-probe the 2-diff SIB pair on 0x10041B60 or the ebx/esi hoist on
  0x1003F130 / 0x1003F410.
- Do not tag `src/` until a filing session.
