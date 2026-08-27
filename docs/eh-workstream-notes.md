# EH workstream — SEH vs C++ (2026-08-27)

The `cxx-eh-frame-wall` note treated `push -1` / `fs:[0]` as unreachable
from C. That is only half true. VC5 C **does** emit that frame from
`__try/__except` / `__try/__finally`. Only `__CxxFrameHandler` (object
unwind / `new T`) needs `.cpp`. No try/catch exists in BRGlide.dll.

Stayed on BRGlide.dll EH-prologue functions. Did not run `--refine`. Did
not edit `tools/`, `include/`, `src/`, or any `build/*_work/` EXE dir.

## Proof: VC5 C `__try` reproduces the frame

Scored with `_score_source` against orig bins. A C `__try/__except`
compiles `/O2` to:

```
push ebp; mov ebp, esp
push -1
push offset scopetable          ; reloc
push offset _except_handler3    ; reloc (IAT thunk 0x10074AE6)
mov eax, fs:[0]
push eax
mov fs:[0], esp
sub esp, 8
push ebx; push esi; push edi
mov [ebp-0x18], esp             ; __except only (esp restore)
mov [ebp-4], 0                  ; trylevel
```

That is byte-identical (reloc-masked) to Glide `0x10074770` through the
trylevel store. `__try/__finally` is the same frame minus the esp save,
plus an outlined finally call after `mov [ebp-4], -1`.

`__except(1)` plants a 6-byte filter (`mov eax,1; ret`) and the loop's
`js` displacement misses by 1. The orig filter is
`FrameUnwindFilter(GetExceptionInformation())` (13 B); that displacement
is inside the 64-byte map slice, so the filter spelling is required even
though the filter bytes themselves sit in the next map entry.

## Classification

`build/match/orig/*.bin` starting `6a ff` or `64 a1`: **80 functions,
97,204 / 480,853 of `.text` (20.2%)**. Every one pushes a thunk
`mov eax, FuncInfo; jmp 0x10074566` and `0x10074566` is
`jmp [MSVCRT!__CxxFrameHandler]`. Magic `0x19930520`, **nTryBlocks = 0
on all 80** (unwind-only, not try/catch).

Those 80 miss the actual C SEH functions: they start `push ebp; mov
ebp,esp; push -1`. Found via the single `.text` xref to
`MSVCRT!_except_handler3` (IAT `0x118f0540` → thunk `0x10074AE6`). Three
functions push that thunk; all three are the CRT vector iterators.

| class | handler | count | bytes | C-reachable? |
|---|---|---:|---:|---|
| SEH `__try` | `_except_handler3` via `0x10074AE6` | 3 | 290 map-sliced (480 with outlined tails) | **yes — matched** |
| C++ unwind | `__CxxFrameHandler` via 10-B thunk | 80 | 97,204 | no — needs `.cpp` /GX |
| IAT thunk | `jmp [_except_handler3]` | 1 | 6 | **yes — matched** |

False positive: `push -1` as `WaitForSingleObject(..., INFINITE)` never
had `fs:[0]` in this corpus (0 bins).

### SEH (C-reachable) — matched `/O2`, 0 diffs

TUs in `build/ghidra_work/`. Map splits the compiler-outlined filter /
finally / second epilogue into sibling VAs; those siblings are not
independent C. Compiling the parent reproduces them. Complete DLL span
also 0 diffs reloc-masked.

| VA | map size | complete | name | source | diffs |
|---|---:|---:|---|---|---:|
| 0x10074AE6 | 6 | 6 | `_except_handler3` thunk | dllimport tail jmp | **0** |
| 0x100747E0 | 22 | 22 | `__FrameUnwindFilter` | `if (code==0xE06D7363) terminate(); return 0` | **0** |
| 0x10074770 | 64 | 106 | `__ArrayUnwind` | `__try` / `__except(FrameUnwindFilter(GetExceptionInformation()))` | **0** |
| 0x100746C0 | 115 | 176 | `__ehvec_dtor` | `__try/__finally` + success flag, walk back | **0** |
| 0x10074800 | 111 | 160 | `__ehvec_ctor` | `__try/__finally` + success flag, walk forward | **0** |

Outlined siblings (do not match standalone): `0x10074733/739/750`
(dtor finally), `0x100747B0/7BD` (ArrayUnwind filter/handler),
`0x1007486F/878/88C` (ctor finally). `0x10074AEC` is the `terminate`
IAT thunk (`??3` is `0x1007456C`, `??2` is `0x10074572`).

thiscall element ctor/dtor is `__fastcall(void *)`. `int i` in
`__ehvec_ctor` is declared uninitialized; `for (i = 0; ...)` is inside
`__try` so the orig store order `[ebp-0x20] success, [ebp-4] trylevel,
[ebp-0x1c] i` holds. `while (--count >= 0)` on `__ehvec_dtor` is
required — `for(;;){--count; if(count<0)break;}` inverts to `jns` and
outlines the body past `ret` (44 diffs).

### C++ (defer) — 80 functions, all unwind-only

Handler evidence (every row): thunk `B8 <FuncInfo> E9 <rel32-to-0x10074566>`,
FuncInfo.magic = `0x19930520`, nTryBlocks = 0, nIPMapEntries = 0.

Unwind-action shapes:

| shape | fns | what the source is |
|---|---:|---|
| `push ptr; call operator delete` (`0x1007456C`) | 70 | `new T(...)` — delete the allocation if T's ctor throws |
| `lea ecx, [ebp-N]; jmp dtor` | 8 | stack object with a destructor |
| `push dtor; push count; push size; push p; call __ehvec_dtor` | 2 | ctor of an object with a member array |

No `try/catch` in this DLL. The `.cpp` workstream is object lifetime,
not exception handlers.

`6aff` = 56 fns / 58,006 B (frameless `/O2` C++ EH). `64a1` = 24 fns /
39,198 B (`mov eax,fs:[0]` first, then `push -1`; still C++).

#### Full C++ list (size order)

| VA | size | start | maxState | unwind | diffs | file |
|---|---:|---|---:|---|---|---|
| 0x10040D10 | 97 | `64a1` | 1 | ehvec-dtor |  |  |
| 0x10004C80 | 167 | `6aff` | 1 | stack-dtor |  |  |
| 0x1003E9B0 | 184 | `6aff` | 1 | op-delete | 33 | slice2_26.c |
| 0x1003C7B0 | 201 | `6aff` | 1 | op-delete | 20 | slice2_25.c |
| 0x1003C880 | 201 | `6aff` | 1 | op-delete | 12 | slice8_83.c |
| 0x1003CA10 | 201 | `6aff` | 1 | op-delete |  |  |
| 0x1003D220 | 201 | `6aff` | 1 | op-delete | 20 | slice2_25.c |
| 0x1003D2F0 | 201 | `6aff` | 1 | op-delete |  |  |
| 0x1003D620 | 201 | `6aff` | 1 | op-delete | 20 | slice2_25.c |
| 0x1003E660 | 201 | `6aff` | 1 | op-delete | 23 | slice2_26.c |
| 0x1003E810 | 201 | `6aff` | 1 | op-delete | 23 | slice2_26.c |
| 0x1003E8E0 | 201 | `6aff` | 1 | op-delete | 22 | slice2_26.c |
| 0x1003EA70 | 201 | `6aff` | 1 | op-delete | 23 | slice2_26.c |
| 0x1003EB40 | 201 | `6aff` | 1 | op-delete | 23 | slice2_26.c |
| 0x1003EF90 | 201 | `6aff` | 1 | op-delete | 19 | slice3_31.c |
| 0x1003F060 | 201 | `6aff` | 1 | op-delete | 19 | slice3_31.c |
| 0x1003F340 | 201 | `6aff` | 1 | op-delete | 19 | slice3_31.c |
| 0x1003F540 | 201 | `6aff` | 1 | op-delete | 19 | slice3_31.c |
| 0x1003F260 | 212 | `6aff` | 1 | op-delete | 30 | slice3_31.c |
| 0x10071FC0 | 213 | `6aff` | 1 | op-delete |  |  |
| 0x1003E0E0 | 214 | `6aff` | 1 | op-delete | 24 | slice4_53.c |
| 0x1003E730 | 214 | `6aff` | 1 | op-delete | 33 | slice2_26.c |
| 0x10058D40 | 215 | `6aff` | 1 | op-delete |  |  |
| 0x1003D3C0 | 216 | `6aff` | 1 | op-delete | 51 | slice2_25.c |
| 0x1003D140 | 219 | `6aff` | 1 | op-delete | 24 | slice4_50.c |
| 0x1003E250 | 221 | `6aff` | 1 | op-delete | 40 | slice2_26.c |
| 0x1003D930 | 222 | `6aff` | 1 | op-delete | 44 | slice2_25.c |
| 0x1003E370 | 223 | `6aff` | 1 | op-delete | 20 | slice4_50.c |
| 0x1003F610 | 239 | `6aff` | 1 | op-delete | 49 | slice3_31.c |
| 0x1003E4A0 | 244 | `6aff` | 1 | op-delete | 63 | slice2_26.c |
| 0x1003ED90 | 244 | `6aff` | 1 | op-delete | 62 | slice3_31.c |
| 0x1003DC20 | 254 | `6aff` | 1 | op-delete | 60 | slice2_25.c |
| 0x1003F700 | 284 | `6aff` | 1 | op-delete | 116 | slice3_31.c |
| 0x1003F130 | 293 | `6aff` | 2 | op-delete | 55 | slice3_31.c |
| 0x1003F410 | 293 | `6aff` | 2 | op-delete | 53 | slice3_31.c |
| 0x10004900 | 309 | `6aff` | 1 | stack-dtor |  |  |
| 0x10041B60 | 309 | `6aff` | 2 | op-delete | 224 | slice6_73.c |
| 0x1003D7D0 | 340 | `6aff` | 1 | op-delete | 127 | slice2_25.c |
| 0x10004FD0 | 360 | `6aff` | 1 | stack-dtor |  |  |
| 0x10004AD0 | 362 | `6aff` | 1 | stack-dtor |  |  |
| 0x100051C0 | 368 | `6aff` | 1 | stack-dtor |  |  |
| 0x1003DD20 | 407 | `64a1` | 1 | op-delete | 209 | slice2_26.c |
| 0x10040B10 | 478 | `6aff` | 1 | ehvec-dtor |  |  |
| 0x1004ABE0 | 760 | `64a1` | 4 | op-delete | 408 | slice6_71.c |
| 0x10043050 | 794 | `64a1` | 5 | op-delete | 278 | br_uipages.c |
| 0x10043370 | 800 | `64a1` | 5 | op-delete | 362 | slice6_71.c |
| 0x10043690 | 800 | `64a1` | 5 | op-delete |  |  |
| 0x10046620 | 908 | `64a1` | 6 | op-delete |  |  |
| 0x1004A840 | 923 | `64a1` | 7 | op-delete | 140 | slice4_52.c |
| 0x10048160 | 1091 | `64a1` | 7 | op-delete | 442 | slice6_73.c |
| 0x10052610 | 1091 | `64a1` | 7 | op-delete | 593 | slice6_72.c |
| 0x10063060 | 1104 | `64a1` | 1 | stack-dtor | 797 | br_cfgfile.c |
| 0x100469B0 | 1210 | `64a1` | 8 | op-delete |  |  |
| 0x1004F8C0 | 1498 | `6aff` | 9 | op-delete | 901 | slice6_72.c |
| 0x1004FEA0 | 1532 | `64a1` | 9 | op-delete |  |  |
| 0x100504A0 | 1558 | `6aff` | 9 | op-delete | 868 | slice6_71.c |
| 0x100458D0 | 1565 | `64a1` | 10 | op-delete | 24 | slice8_86.c |
| 0x1004F290 | 1570 | `6aff` | 9 | op-delete | 758 | br_uipages.c |
| 0x100451F0 | 1749 | `64a1` | 11 | op-delete | 24 | slice8_86.c |
| 0x10045EF0 | 1834 | `6aff` | 11 | op-delete | 24 | slice8_86.c |
| 0x10053590 | 1932 | `64a1` | 12 | op-delete |  |  |
| 0x10046E70 | 2114 | `64a1` | 13 | op-delete | 974 | slice6_73.c |
| 0x100485B0 | 2389 | `6aff` | 14 | op-delete | 1441 | slice6_71.c |
| 0x10048F10 | 2433 | `64a1` | 15 | op-delete | 1075 | slice6_73.c |
| 0x10044860 | 2439 | `6aff` | 15 | op-delete | 24 | slice8_86.c |
| 0x1002F790 | 2517 | `6aff` | 1 | stack-dtor |  |  |
| 0x100425E0 | 2659 | `64a1` | 17 | op-delete | 741 | br_uiroot.c |
| 0x100476E0 | 2679 | `64a1` | 17 | op-delete | 1648 | slice6_72.c |
| 0x10050AC0 | 2701 | `64a1` | 17 | op-delete | 1643 | slice6_72.c |
| 0x10052A60 | 2863 | `64a1` | 18 | op-delete |  |  |
| 0x1004E750 | 2877 | `64a1` | 18 | op-delete | 1301 | slice6_73.c |
| 0x1004DA00 | 3394 | `6aff` | 21 | op-delete | 1486 | slice6_73.c |
| 0x1004BE00 | 3475 | `6aff` | 21 | op-delete |  |  |
| 0x1004CBA0 | 3671 | `6aff` | 21 | op-delete |  |  |
| 0x100439B0 | 3746 | `6aff` | 23 | op-delete | 24 | slice8_86.c |
| 0x100038F0 | 3799 | `6aff` | 1 | stack-dtor |  |  |
| 0x1004AEE0 | 3862 | `6aff` | 24 | op-delete | 2368 | slice6_72.c |
| 0x100498A0 | 3993 | `6aff` | 24 | op-delete |  |  |
| 0x10051600 | 4109 | `64a1` | 25 | op-delete |  |  |
| 0x10056260 | 8349 | `6aff` | 2 | op-delete | 230 | br_uiboot.c |

24-diff rows (0x100439B0, 0x10044860, 0x100451F0, 0x100458D0,
0x10045EF0, all `slice8_86.c`) are the EH frame only — bodies already
match in C. Same class as 0x10056260's 230 diffs (EH prologue +
trylevel stores; 97% of the 8,349 B already matched).

## C++ `.cpp` roadmap (do not attempt until a `.cpp` harness exists)

`match_sweep.compile_variant` always writes `r.c` / the source path as
C. C++ EH needs `cl /GX` (or `/EHs`) on a `.cpp` TU. No such path in
the tree yet.

1. **Harness.** Teach `_score_source` / `compile_variant` to accept
   `.cpp` and pass `/GX`. Keep `/MD` `_CRTIMP dllimport`. Do not sweep
   the 80 as C targets.
2. **Smallest first: 0x10040D10 (97 B).** thiscall ctor:
   `mov esi,ecx; mov [esi], vtable 0x10077680; lea ecx,[esi+0x3838];
   trylevel=0`. Unwind is `__ehvec_dtor(this+0x2B5C, 0x438, 3,
   0x10053EE0)` — member array of 3 × 0x438-byte objects. Once this
   ctor's frame matches, the 201 B `new T` family (maxState=1,
   op-delete unwind, 15 twins) is the same thunk shape.
3. **24-diff slice8_86 cluster.** Body is already C. Recompile those
   TUs as `.cpp` with the local objects / `new` the FuncInfo describes
   (maxState 10–23, each state → `operator delete`).
4. **0x10056260 (8,349 B, 230 diffs).** Two unwind states, both
   `push eax; call operator delete` (`0x1007456C`). The `new` of the
   phase object plus one more allocation. Port already has the
   behaviour; matching needs the two EH states spelled as C++ `new`.
5. **Stack-dtor eight** (0x10004C80, 0x10004900, 0x10004AD0,
   0x10004FD0, 0x100051C0, 0x1002F790, 0x100038F0, 0x10063060).
   `lea ecx,[ebp-N]; jmp dtor` — a named local of class type. 0x100038F0
   is the large one (3,799 B, one 0x760-byte stack object, dtor
   `0x10008D60`).
6. **High maxState UI ctors** (0x100498A0 / 0x1004AEE0 maxState=24,
   0x10051600 maxState=25). One `new` per constructed subobject. Gated
   on the small ctor matching first.

Screening: `6a ff` / `64 a1` in the first ~0x20 bytes **or**
`55 8b ec 6a ff` (SEH with ebp). The former is C++ in this DLL; the
latter is C `__try`. `pipeline_triage.py` currently buckets only `6a ff`
as `cxx-eh-frame` and would miss the three SEH helpers (starts `55`).
Do not re-derive; the 80 + 3 are enumerated above.
