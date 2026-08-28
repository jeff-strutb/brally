# C++ family 7 — EH worklist + two stack-dtor matches (2026-08-27)

Harness: `build/cpp_work/<VA>.cpp` + `python3 tools/cpp_score.py --va <VA>`.
`/O2 /GX /MD`. Do not C-sweep these.

Scanned `build/match/orig/*.bin` for `6aff` / `64a1` with FuncInfo magic
`0x19930520`: **80 / 80** C++ EH functions, 97,204 / 480,853 of BRGlide.dll
`.text` (20.2%). `src/core/cpp/` after this batch: **38 functions /
16,503 B** (17.0% of the C++ class, 3.43% of `.text`). **42 unmatched /
80,701 B** remain.

SKIP bodies (frame reproduces, register-coloring wall): 0x1004DA00,
0x100485B0, 0x1004F8C0. Same wall class: 439B0 / 44860 / 45EF0 / 4F290 /
504A0 / 4BE00 / 4AEE0 / 4CBA0 / 498A0.

## Matched this batch: 2 / 2, all four pieces 0

671 / 480,853 of BRGlide.dll `.text` (0.14%). Stack-dtors. Frame was
already sidecar-MATCH in family 5; residue was PutByte width, not the
EH frame. Same DECLARE-not-define dtor as 0x10056260 / 0x10004C80.

| VA | size | kind | score |
|---|---:|---|---|
| 0x10004900 | 309 | stack-dtor, cdecl 7-arg | **0** |
| 0x10004AD0 | 362 | stack-dtor, cdecl 10-arg | **0** |

Sources: `src/core/cpp/0x10004900.cpp`, `src/core/cpp/0x10004AD0.cpp`.

### 0x10004900

`PutByte(unsigned char)` (not `unsigned`). `volatile int g_id`.
`pkt.PutByte((unsigned char)((g_id & 0xf) \| flags \| 0xE0))`.
Orig `mov bl,[esp+0x248]; and al,0xf; or al,bl; or al,0xe0`.

### 0x10004AD0

Same PutByte width. `PutByte((unsigned char)(g_id \| a9))` — no `& 0xf`.
`if ((a8 & 0x3F) <= 2)` / `if ((a8 & 0x3F) == 4)` CSE's to
`mov ebp,esi; and ebp,0x3f`. A stored `kind = a8 & 0x3F` is 5 diffs
(`and esi,0x3f; mov ebp,esi`).

## Ranked remainder (cpp_score /O2 /GX /MD, then orig-only)

### Tractable (frame landed or frame-only; body is a type/CFG insight)

| VA | size | cpp diffs | first | sidecar | what |
|---|---:|---:|---|---|---|
| 0x1003DD20 | 407 | **32** | +0x16A | **4/4** | last installer. `new Phase` + host tail MATCH. Residue is `g_obj` in eax (`a1`) vs orig ecx (`8b 0d`) then `[ecx+8]` vs `[eax+8]`. Do not permute. Fresh lead: a type that occupies eax across the obj hook. |
| 0x10041B60 | 309 | **2** | +0xDE | **4/4** | Phase ctor SIB `lea ecx,[eax+esi+4]` vs `[esi+eax+4]`. Mapped; do not re-probe. |
| 0x1003D7D0 | 340 | 199 | +0x32 | **4/4** | join preamble. Orig `75 74` short to activate. `return 1` / `goto done` inlines the EH epilogue (`0f 85`). Flag+goto did not form the short jne. |

0x1003DD20 is the next byte pile: 407 B, C was 209 diffs (missing EH).
C++ conversion (56260 method: DECLARE ctor, no `<new>`, cdecl 1-arg
callees `push 0`) landed the frame, the `new`, and the 3DC20 live-eax
host tail (`goto` past `HostAgain` from the first-time arm).

### Stack-dtor walls (sidecar MATCH)

| VA | size | diffs | first | what |
|---|---:|---:|---|---|
| 0x10004FD0 | 360 | 190 | +0x20 | ring: orig `push ebx` / ring in ebx; recomp 3 callee-saves, ring in esi |
| 0x100051C0 | 368 | 235 | +0x17 | twin, flag 0x80 |
| 0x100038F0 | 3799 | 84 | +0x29 | Stream unwind `[ebp-0x760]` vs `[ebp-0x76C]` (family 6) |
| 0x1002F790 | 2517 | — | — | same unwind 1-byte wall; cpp_work timed out this run |
| 0x10063060 | 1104 | no cpp | — | thiscall `ret 4`, stack obj `sub esp,0x87c`, dtor 0x10008D60. C in br_cfgfile.c (797 diffs) |

### Page-builder coloring walls (do not permute ebx/ebp or esi/edi)

maxState = 1 + N `new Ctl`. Sidecar usually MATCH once Ctl() is DECLARED.
First `.text` diff is prologue 0/1-register or esi-vs-edi phase.

| VA | size | maxS | cpp diffs | notes |
|---|---:|---:|---:|---|
| 0x1004ABE0 | 760 | 4 | no cpp | smallest builder |
| 0x10043050 | 794 | 5 | no cpp | C 278, br_uipages.c |
| 0x10043370 | 800 | 5 | no cpp | C 362 |
| 0x10043690 | 800 | 5 | no cpp | ghidra dump exists |
| 0x10046620 | 908 | 6 | no cpp | |
| 0x1004A840 | 923 | 7 | no cpp | C 140 (incomplete recomp 160) |
| 0x10048160 | 1091 | 7 | no cpp | |
| 0x10052610 | 1091 | 7 | no cpp | |
| 0x100469B0 | 1210 | 8 | no cpp | |
| 0x1004F8C0 | 1498 | 9 | 1131 | **SKIP** coloring |
| 0x1004FEA0 | 1532 | 9 | no cpp | |
| 0x100504A0 | 1558 | 9 | 1195 | esi-vs-edi |
| 0x100458D0 | 1565 | 10 | no cpp | C stub 24 |
| 0x1004F290 | 1570 | 9 | 1214 | esi-vs-edi |
| 0x100451F0 | 1749 | 11 | no cpp | C stub 24 |
| 0x10045EF0 | 1834 | 11 | 1369 | ebx/ebp |
| 0x10053590 | 1932 | 12 | no cpp | 2 Page |
| 0x10046E70 | 2114 | 13 | no cpp | |
| 0x100485B0 | 2389 | 14 | 1776 | **SKIP** coloring |
| 0x10048F10 | 2433 | 15 | no cpp | 2 Page |
| 0x10044860 | 2439 | 15 | 1841 | ebx/ebp |
| 0x100425E0 | 2659 | 17 | no cpp | C 741 |
| 0x100476E0 | 2679 | 17 | no cpp | |
| 0x10050AC0 | 2701 | 17 | no cpp | |
| 0x10052A60 | 2863 | 18 | no cpp | |
| 0x1004E750 | 2877 | 18 | no cpp | |
| 0x1004DA00 | 3394 | 21 | 2046 | **SKIP** coloring |
| 0x1004BE00 | 3475 | 21 | 2579 | |
| 0x1004CBA0 | 3671 | 21 | 2557 | unwind `[ebp-0x50]` |
| 0x100439B0 | 3746 | 23 | 2846 | ebx/ebp |
| 0x1004AEE0 | 3862 | 24 | 2909 | |
| 0x100498A0 | 3993 | 24 | 2876 | unwind `[ebp-0x50]` |
| 0x10051600 | 4109 | 25 | no cpp | largest builder |

### Ctor wall

| VA | size | diffs | what |
|---|---:|---:|---|
| 0x10040B10 | 478 | 295 | BrCtl ctor; POD stores vs `__ehvec_ctor` setup (family 5) |

## Byte pile

Unmatched C++ EH: 80,701 / 97,204 of the class (83.0%), 80,701 / 480,853 of
`.text` (16.8%). Almost all of that is page builders (coloring wall) plus
the two Stream stack-dtors (unwind displacement). The only sub-50-diff
body left is 0x1003DD20 (32) and 0x10041B60 (2 SIB).

## What not to do

- Do not permute ebx/ebp or esi/edi on page builders.
- Do not `PutByte(unsigned)` on Pkt thiscall byte args.
- Do not store `kind = a8 & 0x3F` when orig copies then ands.
- Do not dllimport `operator new`.
- Do not define Pkt/Phase ctors or dtors in these TUs.
- Do not C-sweep the 80.
