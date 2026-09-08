# Structural generators — decision logic

Standalone transforms (`tools/gen_structural.py`). Do **not** copy this file
into `ghidra_to_match.py` blindly: fold the *high-value* one into
`_refine_candidates` as one candidate per function. Proven against
BRGlide.dll orig bytes. Residue: 195 unmatched refine rows
(`python3 tools/ghidra_to_match.py --residue`).

Four recurring idioms from the structural batch (VC5-IDIOMS-dll.md). Each
generator yields `(label, mutated_source)` in the `_refine_candidates`
style. `--validate` scores `transform_*` (all edits of that generator)
with `ghidra_to_match._score_source`, opts `/O2`, `/Od`, `/O2 /Oy-`.

`--from-decomp` isolates the transform (wrap of `build/ghidra_decomp`).
Work-file scores are the residue as it sits (some VAs already hand-fixed).

## Verdict

| generator | prey (decomp) | moved | MATCH | worse | fold? |
|---|---|---|---|---|---|
| **strarr** | 29 / 195 | **19** | **1** | 3 | **FOLD** |
| looppeel | 2 / 195 | 0 | 0 | 0 | **SKIP** — peel wall |
| wordbswap | 1 / 195 | 0 | 0 | 1 | **SKIP** — n=1, no move |
| floatproto | 3 / 195 (1 fire) | 0 | 0 | 1 | **SKIP** as a search; seed the decl |

Hit-rate that matters is from-decomp (work already contains strcat on
0x1006FF50 / 0x10055A40, float proto on 0x1006EBC0, BrRca or-shift on
0x10031960). Work-only strarr still moved 9 / 25 = 36%.

---

## 1. strarr — FOLD

Ghidra explodes `/Oi` `repne scasb`/`rep movsd` into a `0xffffffff` scan +
dword/byte copy. `_strcpy_sub` at wrap time requires `src = walker + -len`
with no intervening statements and no dest-scan; residue still has
walker-rewind (`walker = walker + -len; dest = D`) and strcat (dest scanned
with `i = -1`). `extern int s_*` loads the first dword (`mov r,[s]; push r`);
orig `push offset s` is `extern char s_*[]`. Proven MATCH 0x1006FF50 (93→0
from-decomp) and 0x10008E60 (already in `_refine_candidates` as `(o) strarr`,
one s_* at a time — this generator also rewrites the exploded loops).

Distinguisher: orig `or ecx,-1; f2 ae; not ecx; shr ecx,2; f3 a5`.

### Fold recipe

One candidate, not a search. Seed from the wrapped TU. Do not write
`ghidra_work` from this module.

```
def _strarr_rewrite(src):
    import gen_structural as gs
    new, labels = gs.transform_strarr(src)
    if new != src:
        yield ('strarr', new)
```

`transform_strarr` applies every `extern int s_*` → `extern char s_*[]`,
every DAT used as a whole-object string address (`(char *)&DAT` without
`+`, or `fopen`/`strcpy`/`sprintf` arg), then every exploded strcpy/strcat.
Per-site yields (`strarr:NAME`, `strcpy:N`, `strcat:N`) stay available for
a climb; they are not the fold.

Do **not** retype `(char *)&DAT + i` (struct/table walk, 0x1006A330) or
sscanf 3rd+ `&DAT` out-params (0x10031030 `&DAT_1186c960`).

### Measured delta (from-decomp wrap)

Scored 2026-08-27. 29 prey of 195 residue. 19 moved, 1 MATCH, 3 worse.

| VA | before | after | delta | what |
|---|---|---|---|---|
| 0x1006FF50 | 93 | **0** | **-93** | strcat×2 + `extern char DAT[]` |
| 0x10055A40 | 134 | 2 | **-132** | `s_seasondesc_dat[]` + mode `char[]` |
| 0x100367C0 | 59 | 9 | **-50** | strcpy + `DAT_10ac40a8[]` |
| 0x10060A30 | 150 | 84 | **-66** | `s_SAVING_LAST_LAP_INFO[]` |
| 0x100583C0 | 177 | 126 | **-51** | three `s_DDraw_*[]` |
| 0x10031030 | 59 | 11 | **-48** | `s__u__x__d__d[]` |
| 0x1003FDA0 | 115 | 77 | **-38** | strcpy×2 + dest `char[]` (twins 0x1003FE80 / 0x10040040 same) |
| 0x1005A480 | 44 | 7 | **-37** | `s_Paint_damage_d_bmp[]` |
| 0x100038A0 | 58 | 33 | **-25** | strcpy + `DAT_1021c9b0[]` |
| 0x100087D0 | 82 | 64 | **-18** | `s_CleanupName_*[]` |
| 0x10038DA0 | 81 | 65 | **-16** | strcpy (keep intervening `iVar2 = *(int *)`) |
| 0x100392E0 | 81 | 65 | **-16** | strcpy twin of 0x10038DA0 |
| 0x10036A30 | 161 | 151 | **-10** | `s__s___s[]` / `s__s_s[]` |
| 0x10008990 | 58 | 56 | **-2** | `s_ReadPod_*[]` |
| 0x10035400 | 185 | 183 | **-2** | `s_Could_not_create_DirectPlay_*[]` |
| 0x100356B0 | 130 | 129 | **-1** | `s_Could_not_select_service_*[]` |
| 0x10008930 | 17 | 16 | **-1** | `s_GetNumForName_*[]` |

Worse (do not revert the `char[]` decl — format strings still want offset
push; other residue dominates): 0x100023F0 72→74, 0x10032530 44→54,
0x10035533 102→105.

Unmoved: 0x10008960 / 08A30 / 089F0 (s_* is a comparison operand, Ghidra
`>` inside the symbol), 0x10008DC0 / 08E10 (s_* + fopen `&DAT` already
address), 0x10027710, 0x10040A90 (scasb is strlen-for-compare, not copy).

Closest non-MATCH: 0x10055A40 at 2 / 165 (scattered — stop after filing
the `char[]`). 0x1005A480 at 7 / 91. 0x100367C0 at 9 / 71.

---

## 2. looppeel — SKIP

Orig 0x10039580: each switch arm is `xor eax,eax; mov ecx, TABLE;
cmp [ecx],key; je found; add ecx,0x24; inc eax; cmp ecx,END; jl header`.
Ghidra prints `while (*p != key) { p += 9; i++; if (p >= end) return 0; }`
or `do { if (*p == key) return i; ... } while (p < end)`. VC5 peels
**every** while/do/for/goto spelling of that shape to `jge; cmp; jne`
(VC5-IDIOMS-dll.md). Same family as the compacted-switch wall.

Prey: 2 / 195 (`_WHILE_HEAD` / `_DO_HEAD` with `*p ==`/`!=`, plus orig
`add ecx,0x24` + jl-back). 0x10039580, 0x10036080.

From-decomp: 103→103 and 92→92. Work: 125→125 and 92→92. Unique goto
labels (`LAB_scan_%d` % span_start) compile; they do not change bytes.

Do not fold. A refine candidate that cannot move the score is compile
waste. The remaining lever is not C (table-base integer constants are
already in the 0x10039580 work file).

---

## 3. wordbswap — SKIP

Orig 0x10031960: `xor eax,eax; xor ecx,ecx; mov al,[esi+15]; mov cl,[esi+17];
mov ah,[esi+14]; mov ch,[esi+16]; mov word [esi+14],ax; mov word [esi+16],cx`.
That is BrRcaFixupRecord's 16-bit BE reconstruct (`*(uint16_t *)(p+N) =
(uint16_t)(p[N+1] | (p[N] << 8))`), not a two-statement byte-pair swap
(those are the surrounding dword pairs; swaprot already covers them).

Ghidra: `*(ushort *)(p+0x14) = CONCAT11(p[0x14], p[0x15]);`. Wrap strips
`CONCAT11` to `/* CONCAT */(a,b)` (comma expr = `b` only).

Prey: 1 / 195 (0x10031960). Work already has the BrRca or-shift (44 diffs
/O2 — AL vs CL + loop CSE, mapped in VC5-IDIOMS-dll.md). Transform is a
no-op on work. From-decomp CONCAT→BrRca is 163→171 (worse); xor-zero
locals were 163→189. The 16-bit spelling is not the wall.

Do not fold. n=1 and the proven spelling is already in the work file.

---

## 4. floatproto — SKIP as a search; seed the decl if you fold anything else

Empty `int f();` + a float arg at the call site promotes to double
(`fstp qword`, `sub esp,8`, enough to force `and esp,-8`). Real
`int f(float)` is `push ecx; fstp dword`. Proven 0x10019A70 /
0x10018990 thunk (`fld dword [esp+4]; jmp __ftol`) / 0x1006EBC0 work.

Prey: 3 residue with orig `fstp dword` and an empty proto in the wrap;
1 fire (0x1006EBC0). Work already has `int FUN_10018990(float x);` (11
diffs /O2 — x87 scheduling wall, stop). From-decomp wrap names the
thunk `BrFtolArg` and leaves `int BrFtolArg();`; the transform yields
`int BrFtolArg(float);` and scores **68→78** /O2. Isolated worse because
the rest of the Ghidra body is still wrong (`FUN_1006e5c0()` without this,
`0x3f < x` vs orig `>= 0x40`). The decl is still the right bytes for
those two call sites.

Do not add it as a hill-climb *search* (prey=1, 0 moves). If you seed
one no-search candidate alongside strarr, it is one extra compile:

```
def _float_proto_rewrite(src):
    import gen_structural as gs
    new, labels = gs.transform_float_proto(src)
    if new != src:
        yield ('floatproto', new)
```

Skip it until a batch with more `int f();` + `*(float *)` call sites
(0x10019A70 is gated on 131 callees and is last among the big targets).

---

## CLI

```
python3 tools/gen_structural.py --dry-run
python3 tools/gen_structural.py --validate
python3 tools/gen_structural.py --validate --from-decomp
python3 tools/gen_structural.py --va 0x1006FF50 --from-decomp --gen strarr
python3 tools/gen_structural.py --va 0x1006EBC0 --from-decomp --gen floatproto
```

`--from-decomp` never writes `ghidra_work`. `--validate` does not write
learnings.

## `_refine_candidates` signatures

```
gen_strarr(src)      -> ('strarr:NAME' | 'strcpy:N' | 'strcat:N', new_src)
gen_loop_peel(src)   -> ('loopgoto:N' | 'loopdowhile:N' | 'loopimm:ADDR', new_src)
gen_word_bswap(src)  -> ('wordbswap:brrca:N' | 'wordbswap:N' | 'wordbswap:pair:N', new_src)
gen_float_proto(src) -> ('floatproto:NAME', new_src)
```

Only `gen_strarr` / `transform_strarr` is worth the compile.
