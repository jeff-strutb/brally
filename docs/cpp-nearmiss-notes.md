# C++ near-miss push (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not `@implements`-tag these in `src/` this session.

## Scoreboard

| VA | size | kind | `.text` | FuncInfo | unwind | handler |
|---|---:|---|---|---|---|---|
| 0x1003F410 | 293 | two `new Phase` | **0** | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x1003F130 | 293 | two `new Phase` | **0** | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x10041B60 | 309 | Phase ctor, two `new NameList` | 2 | **MATCH** | 2/2 **MATCH** | **MATCH** |
| 0x1002F790 | 2517 | stack-dtor Stream | 86 | **MATCH** | **MATCH** | **MATCH** |
| 0x100038F0 | 3799 | stack-dtor Stream | 84 | **MATCH** | **MATCH** | **MATCH** |

Matched this batch: 293+293 = 586 / 480,853 of BRGlide.dll `.text` (0.12%).
Sidecar (FuncInfo / unwind / handler) is 5 / 5.

## 0x1003F410 / 0x1003F130 — MATCH, all four pieces

Orig first bytes after the unused-this `push ecx`:

```
mov eax, [g_slot]
push esi
test eax, eax
mov esi, 1            ;  BE 01 00 00 00  in the flags gap
jne existing          ;  0F 85 far
```

The previous sidecar-matched source (`return one` **inside both arms**) delayed
`mov esi,1` until after the first enter and rematerialized the existing path
as `mov eax,1` (B8) — 82 diffs, first @ +0x1E.

Shared `return one` **after** the if/else, with a live local assigned after
the slot load, hoists `mov esi,1` into that gap and both epilogues become
`mov eax, esi`. Flag stores stay `89 r/m, esi` (the live local, not `c7`).

```cpp
p = g_slot;
one = 1;
if (p == 0) {
    p = new Phase;
    g_slot = p;
    g_cur = p;
    if (p == 0)
        return 0;
    p->pfnEnter = EnterFn;
    g_slot->pfnEnter(g_slot);
    g_cur->f0C = one;
    g_cur->f68 = one;
    q = new Phase;
    g_slot2 = q;
    if (q == 0)
        return 0;
    q->pfnEnter = Enter2Fn;
    g_slot2->pfnEnter(g_slot2);
    g_slot2->f0C = one;
} else {
    g_cur = p;
}
return one;
```

Mapped miss: `return one` inside both arms (existing path `mov eax,1`,
esi=1 after enter). `int one = 1` at the top of the function folds the
same way. A dummy global store of 1 hoists esi but inserts `mov [g],esi`
before the `jne` (178 diffs).

Family-1 201 B still wants a shared **literal** `return 1` so flags stay
`c7`. These two-`new` twins want a shared **register** `return one` so
flags stay `89` *and* esi is live on the existing path. Same compiler,
opposite 1-CSE because of the second `new`'s trylevel=1 plus the extra
f0C store.

## 0x10041B60 — sidecar MATCH, 2 SIB diffs (wall)

Both remaining diffs are the same SIB base/index swap on the sprintf dest:

```
orig:    lea ecx, [eax+esi+4]     ; 8D 4C 30 04
recomp:  lea ecx, [esi+eax+4]     ; 8D 4C 06 04
```

and the list1 twin (`8D 44 32 04` vs `8D 44 16 04`). Pointer in eax/edx,
byte-offset induction in esi (`add esi,0x104; cmp esi,0x6590; jl`).
Same address; encoding only.

Operand-order / index-base swaps that did **not** flip the SIB byte
(still 2 diffs, same 320 B body):

- `off + 4 + (char *)pList0`
- `(char *)((int)pList0 + off + 4)`
- `(char *)(off + 4 + (int)pList0)`
- `(char *)(4 + (int)pList0 + off)`
- `(char *)pList0 + 4 + off`
- `&pList0->asz[0][off]`
- `&((unsigned char *)pList0)[off + 4]`

`pList0->asz[i]` (i-scaled, not esi+=0x104) is 122 diffs. Caching
`(char *)pList0` in a named pointer is 97 diffs (`lea ecx,[ebp+esi+4]`).
Do not re-probe. Allocator-internal SIB, same class as 0x10001440.

## 0x1002F790 / 0x100038F0 — unwind MATCH (was 1-byte miss)

Orig unwind both:

```
lea ecx, [ebp-0x760]     ; 8D 8D A0 F8 FF FF
jmp 0x10008D60           ; empty thiscall dtor (`ret`)
```

`sizeof 0x760` + `extra[0x30]/[0x34]` (matching `sub esp,0x790/0x794`)
emits `[ebp-0x76C]` — 0xC extra, the EH node. The unwind displacement is
**object-start from ebp**, which for 04C80 equals sizeof+0xC because the
object *is* the whole local area. Here 0x3C (2F790) / 0x40 (038F0) temps
sit **below** the object, so:

| | sizeof | extra below | `sub esp` | unwind |
|---|---:|---:|---|---|
| 0x1002F790 | **0x754** | 0x3C | `0x790` | `[ebp-0x760]` = 0x754+0xC |
| 0x100038F0 | **0x754** | 0x40 | `0x794` | same |

Ctor 0x1006CDA0 only writes `this[0,4,8]=0`, `this+0xC=arg2`,
`this+0x10=arg1` (external buffer). The 0x754 is the stack object's
allocated size, not the header. sizeof 0x14 with only extra[0x3C] parks
the object next to the EH node (`[ebp-0x20]`, 8-byte action). Do not
re-probe 0x760 pads.

`.text` first diff @ +0x29: orig `push ebx; push ebp; push esi; push edi;
xor ebp,ebp` then two 0-stores and ctor. Stub only `push esi` (Status
return). 86 / 84 diffs are that 144-byte stub vs the 2517 / 3799 orig
prefix, **not** a 2517-byte body 86 away. Partial loop/switch
transcriptions grew the frame (`sub esp,0x7B4`) and jumped to 400–800
diffs without moving the first-diff past +0x17.

Body is a `(byte & 0xE0)` jump table, player stride `0x96C` at
`0x117A9B88`, WaitForSingleObject / ReleaseMutex IAT in edi/ebx, and
0x1002F790 → 0x100038F0 cdecl 5. Gated on a full transcription that
keeps Stream at +0x3C in the `sub esp,0x790` region.

## Idioms (solved once)

- **Two-`new` installer 1-register:** live `int one` + shared `return one`
  after if/else. `mov esi,1` lands between `test eax,eax` and the far
  `jne`. Return inside both arms rematerializes the existing path as
  `mov eax,1` and kills the hoist. Opposite of the 201 B literal-shared-
  return / `c7` rule.
- **SIB `[base+index+disp]` vs `[index+base+disp]`** on
  `sprintf(list+4+off)` is not source-reachable from add-tree operand
  order. 2 diffs, classified.
- **Stack-object unwind displacement is object-start, not a size
  immediate.** When temps sit below the object, sizeof = unwind_disp - 0xC
  and extra = `sub esp` - sizeof. 0x1002F790: 0x754 + 0x3C. 04C80 is the
  extra=0 case (sizeof 0x214, unwind 0x220).

## What not to do

- Do not `return 1`/`return one` inside both arms on the two-`new`
  twins (that was the 82-diff miss).
- Do not re-probe 41B60 SIB operand order.
- Do not set Stream sizeof to the unwind displacement 0x760.
- Do not define Phase/NameList/Stream ctors or dtors in these TUs.
- Do not C-sweep these. `6aff` in the first ~0x20 bytes → `.cpp`.
- Do not tag `src/` until a filing session.
