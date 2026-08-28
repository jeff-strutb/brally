# Fresh-batch generators — decision logic

Standalone transforms (`tools/gen_fresh.py`). Do **not** copy this file
into `ghidra_to_match.py` blindly: fold the *high-value* ones into
`_refine_candidates` as one candidate per function (charret is orig-gated
and folds in `refine_function`, next to callconv). Proven against
BRGlide.dll orig bytes. Residue: 184 unmatched refine rows
(`python3 tools/ghidra_to_match.py --residue`). Untranscribed: 564
`build/ghidra_decomp/<VA>.c` with no work file and no tree MATCH.

Four recurring idioms from the fresh DLL batches (VC5-IDIOMS-fresh{1,2,3}.md).
Each generator yields `(label, mutated_source)` in the `_refine_candidates`
style. `--validate` scores `transform_*` with `ghidra_to_match._score_source`,
opts `/O2`, `/Od`, `/O2 /Oy-`.

`--from-decomp` isolates the transform (wrap of `build/ghidra_decomp`).
`--pool untrans` is the same wrap on VAs that never got a work file.
Work-file scores are the residue as it sits (some VAs already hand-fixed).

Hit-rate that matters is from-decomp.

## Verdict

| generator | prey (decomp) | moved | MATCH | worse | fold? |
|---|---|---|---|---|---|
| **stringops** | 48 / 184 residue; **138 / 564** untrans | **27 + 98** | **2 + 24** | 5 + 3 | **FOLD** |
| **charret** | 7 / 184; 9 / 564 | **4 + 4** | **1 + 1** | 0 | **FOLD** (orig-gated) |
| i64glob | 2 / 184; 4 / 564 (1 fire) | 1 | 0 | 0 | **SKIP** as a search; seed the decl |
| ucharbx | 13 / 184; 19 / 564 | 4 + 8 | 0 | 5 + 5 | **SKIP** as combined; CONCAT gated on orig `8a; 53` |

stringops is the high-prey win. Untrans is where MATCH lands (those VAs
were never wrapped). Residue work already contains strcat on 0x1006FF50 /
0x10055AF0, char return on 0x10054390, `__int64` on 0x1002E186, unsigned
char on 0x10027B60 / 0x1006A650.

---

## 1. stringops — FOLD

Ghidra explodes `/Oi` `repne scasb` / `rep movsd` / `rep stosd` into a
`0xffffffff` (or signed `-1`) scan + dword/byte copy, or a counted
`for (i = N; i != 0; i--)` zero/copy. Wrap's `_strcpy_sub` requires
`src = walker + -len` with no intervening statements and no dest-scan;
`_strlen_sub` requires `u = 0xffffffff` and misses signed `i = -1`;
`_memset_sub` requires `n >> 2` PLUS a residual byte loop. Residue and
untrans still have walker-rewind strcpy, strcat (dest scanned first),
`if (i != -2)` strlen, dword-only stosd, and known-size memcpy.

`extern int s_*` loads the first dword (`mov r,[s]; push r`); orig
`push offset s` is `extern char s_*[]`. Reuses `gen_structural.strarr`
for the char[] / strcpy / strcat rewrite, then adds strlen / memset /
memcpy.

Distinguisher: orig `or ecx,-1; f2 ae; not ecx; shr ecx,2; f3 a5`
(strcpy/strcat), `or ecx,-1; f2 ae; not ecx; dec ecx; je` (strlen),
`xor eax,eax; mov ecx,N; f3 ab` (memset), `mov ecx,N; f3 a5` with no
preceding scasb (memcpy).

New ops that MATCH on their own (not just strarr):

| VA | before | after | what |
|---|---|---|---|
| 0x10038490 | 30 | **0** | `strlen:cmp` (`i = -1; … if (i != -2)`) |
| 0x10038550 | 30 | **0** | strlen:cmp twin |
| 0x100387C0 | 30 | **0** | strlen:cmp twin |
| 0x10023900 | 19 | **0** | `memcpy:imm` |
| 0x10033C90 | 38 | **0** | memcpy:imm |
| 0x100418C0 | 10 | **0** | `memset:imm` (counted dword stosd) |
| 0x10055AF0 | 320 | **0** | strcpy×2 + strcat×2 + memset 0x104 + `char[]` |

### Fold recipe

One candidate, not a search. Seed from the wrapped TU. Do not write
`ghidra_work` from this module.

```
def _stringops_rewrite(src):
    import gen_fresh as gf
    new, labels = gf.transform_stringops(src)
    if new != src:
        yield ('stringops', new)
```

`transform_stringops` applies strarr first (so memcpy does not steal
scasb copies), then every remaining strlen / memset / memcpy. Per-site
yields (`strarr:NAME`, `strcpy:N`, `strcat:N`, `strlen:cmp:N`,
`memset:imm:N`, `memcpy:imm:N`) stay available for a climb; they are
not the fold.

Do **not** convert a stride-loop inner copy (0x100013F0 `(n >> 1)` 2-D
blit). Do **not** treat a comparison-only scasb as strcpy (0x10040A90
strlen-for-compare; wrap already emits `strlen`).

### Measured delta (from-decomp wrap)

**Residue** (48 prey of 184, extras included). 27 moved, 2 MATCH, 5 worse.
Scored 2026-08-27.

| VA | before | after | delta | what |
|---|---|---|---|---|
| 0x10055AF0 | 320 | **0** | **-320** | strcpy/strcat + memset 0x104 + `char[]` |
| 0x1006FF50 | 93 | **0** | **-93** | strcat×2 + `extern char DAT[]` (strarr) |
| 0x100368A0 | 312 | 181 | **-131** | strcpy + `DAT[]` |
| 0x1003A580 | 264 | 168 | **-96** | strcpy×2 + strlen + memset 32 |
| 0x10060A30 | 150 | 84 | **-66** | `s_SAVING_LAST_LAP_INFO[]` |
| 0x100583C0 | 177 | 126 | **-51** | three `s_DDraw_*[]` |
| 0x100367C0 | 59 | 9 | **-50** | strcpy + `DAT_10ac40a8[]` |
| 0x10031030 | 59 | 11 | **-48** | `s__u__x__d__d[]` |
| 0x1003FDA0 | 115 | 77 | **-38** | strcpy + dest `char[]` (twins 0x1003FE80 / 0x10040040) |
| 0x100298C0 | 134 | 115 | **-19** | memcpy:imm |
| 0x10038DA0 | 81 | 65 | **-16** | strcpy (keep intervening `iVar2 = *(int *)`) |
| 0x10054390 | 296 | 288 | **-8** | strlen:cmp ×2 (charret is the rest) |
| 0x1006C4D0 | 305 | 295 | **-10** | memset 15 dwords |
| 0x100299A0 | 300 | 294 | **-6** | memcpy 0xaa / 0x80 dwords |

Worse (do not revert `char[]` — format strings still want offset push;
other residue dominates): 0x100023F0 72→74, 0x10032530 44→54,
0x10035533 102→105, 0x1005A080 281→285 (memcpy:shr size of `h*w*4 &
0x3fffffff` — hill-climb rejects), 0x1006FCE0 70→71.

**Untranscribed** (138 prey of 564). 98 moved, **24 MATCH**, 3 worse.
71% hit-rate. Closest non-MATCH: 0x100358F0 at 1 / 225.

MATCH VAs: 0x100034C0 0x100035E0 0x100036F0 0x10003760 0x100061E0
0x10006250 0x10023900 0x10030F50 0x10031140 0x10033C90 0x100357E0
0x10038420 0x10038490 0x10038550 0x10038580 0x100385F0 0x10038650
0x10038750 0x100387C0 0x1003AF60 0x1003B970 0x100418C0 0x10055AF0
0x1006FF50.

Work-only stringops still moved 11 / 42 = 26% (0 MATCH — the MATCH
spellings are already in those work files).

---

## 2. charret — FOLD (orig-gated)

Orig `mov al, 1; pop*; ret` (`b0 01 5b c3`) came from a `char` /
`BrBool` return, not Ghidra's `undefined4` → wrap `int`
(`b8 01 00 00 00 c3`). Also `xor al,al` / `or al,0xff` before the
pops (`0x10054390` `return (char)0xff`). Proven MATCH 0x10054390
(442 B, already in the tree) and **0x10069930** (189→0 from-decomp).

Distinguisher: walk back from `c3`/`c2` over pops and `add esp,imm8`;
the previous opcode is `b0` / `32 c0` / `0c ff`. Mid-function
`b0 xx 5*` counts (0x10054390 has both).

`_refine_candidates` only sees `src`. Fold in `refine_function` next
to callconv, gated on the orig bytes — an ungated `int`→`char` would
compile every `return 1` as `mov al,1` and burn the cand budget.

### Fold recipe

```
# inside refine_function, orig_bytes already loaded:
import gen_fresh as gf
if gf.char_width_orig(orig_bytes):
    _new, _ = gf.transform_charret(src, orig=orig_bytes)
    if _new != src:
        yield ('charret', _new)   # or adopt like callconv (only-if-better)
```

`transform_charret` retypes the defined function to `char` and rewrites
`return 0xff` to `return (char)0xff`. Per-type yields (`charret:char`,
`charret:uchar`) stay available for a climb; the fold is `char`.

Do **not** fire on fnstsw helpers whose AL is a status nibble
(0x10006A10 `df e0; b0 02` — Ghidra return is `ushort`, `_DEF_SIG`
already skips it). Do **not** fire on a work file already typed `char`
(0x10069A80 work).

### Measured delta (from-decomp wrap)

Scored 2026-08-27. 7 prey of 184 residue (+ extras); 9 of 564 untrans.
4 moved, 1 MATCH, 0 worse. Same four VAs in both pools (extras).

| VA | before | after | delta | what |
|---|---|---|---|---|
| 0x10069930 | 189 | **0** | **-189** | `char` + `return 1` / `return 0` |
| 0x10069DE0 | 414 | 320 | **-94** | `char` (work still `int` — 414→320 work too) |
| 0x10069A80 | 598 | 584 | **-14** | `char` (work already `char`, from-decomp wrap is `int`) |
| 0x10054390 | 296 | 288 | **-8** | `char` + `(char)0xff` (tree MATCH is the rest) |

Unmoved: 0x100695C0 (compile of the wrap still fails), 0x1006A080 /
0x10036080 (orig tail is not a byte ret after all). Work-only: 1 move
(0x10069DE0).

---

## 3. i64glob — SKIP as a search; seed the decl

Ghidra splits a 64-bit global into `DAT_lo` / `DAT_lo+4` and prints
`CARRY4` + two stores, or `CONCAT44(hi, lo)`, or `__allmul(lo, hi, …)`.
`extern unsigned __int64 DAT_lo` emits `__allmul`/`__aulldiv` without
`and esp,-8`. A *local* `__int64` is the 0x14 / `and esp,-8` trap
(0x10019A70). Frame of the proven site is 0x10. Proven 0x1002E186
(hand body 22 leftover is fild-slot packing, not the type).

Distinguisher: orig `add ecx,eax; adc eax,edx` on two consecutive
globals. Source: `CARRY4(DAT_lo, expr)` or `__allmul(DAT_lo, DAT_hi, …)`.

Prey: 2 residue / 4 untrans; **1 fire** (0x1002E186). From-decomp
**191→138** /O2 /Oy- after the compile fix (wrap types `__allmul`'s
`undefined8` as `double`; MSVC 5 C2520 on `unsigned __int64`→`double`
until that local is retyped and `__aulldiv(x, imm, 0)` is spelled `/`).
0x1006E280 is `LARGE_INTEGER` (QPC out-param) — not prey. Consecutive
int tables (0x10002460 `DAT_1021c654`..) are not 64-bit; the generator
requires CARRY4 / `__allmul(lo,hi)` / CONCAT pair, not mere +4.

Do not add it as a hill-climb *search* (prey=1, 0 MATCH). If you seed
one no-search candidate alongside stringops, it is one extra compile:

```
def _i64_rewrite(src):
    import gen_fresh as gf
    new, labels = gf.transform_i64glob(src)
    if new != src:
        yield ('i64glob', new)
```

Skip it until a batch with more add/adc global pairs. 0x1002E186 work
already has the decl.

---

## 4. ucharbx — SKIP as combined; CONCAT gated on orig

`unsigned char` locals feeding stores through branchy code emit
`mov bl; push ebx` (the zero/one-register pattern). Ghidra `bool bVar`
wraps to `int bVar`. CONCAT31(hi, byte) at a call site is the 0x10027B60
expand: orig loads maskOdd into ebx, then each earlier byte overwrites
`bl` and re-pushes ebx (`8a 9e xx; 53`, not `0f be` / `0f b6`).

From-decomp: 0x10027B60 **211→155** (CONCAT×8 + callee `unsigned char`
slots). 0x1006A650 **277→269** (`int bVar2` → `unsigned char`).
0x1006E9E0 **94→22** (work too — `bVar` is a live byte flag).

Combined fold is the wrong shape: 5 worse on residue from-decomp
(0x10071F00 21→23, 0x100154A0 109→112, 0x100541B0 162→168,
0x10054280 190→197, 0x1002E186 191→195) and 5 worse on untrans
(including 0x100250D0, the 8480 B callee — do not retype its CONCAT
internals). Hill-climb would reject those; a one-shot combined
candidate would not.

### Fold recipe (CONCAT only, orig-gated)

```
# inside refine_function:
import gen_fresh as gf
if gf._has_mov_bl_push_ebx(orig_bytes):
    new, labels = gf.transform_ucharbx(src)
    if new != src:
        yield ('ucharbx', new)
```

That hits 0x10027B60 and does not dump `int bVar*` across the corpus.
Leave bVar as a per-var climb yield (`ucharbx:bvar:NAME`) if a later
batch wants it; it is not the fold.

---

## CLI

```
python3 tools/gen_fresh.py --dry-run
python3 tools/gen_fresh.py --dry-run --pool untrans
python3 tools/gen_fresh.py --validate --from-decomp
python3 tools/gen_fresh.py --validate --pool untrans
python3 tools/gen_fresh.py --va 0x10055AF0 --from-decomp --gen stringops
python3 tools/gen_fresh.py --va 0x10069930 --from-decomp --gen charret
python3 tools/gen_fresh.py --va 0x1002E186 --from-decomp --gen i64glob
python3 tools/gen_fresh.py --va 0x10027B60 --from-decomp --gen ucharbx
```

`--from-decomp` / `--pool untrans` never writes `ghidra_work`.
`--validate` does not write learnings.

## `_refine_candidates` signatures

```
gen_stringops(src) -> ('strarr:NAME' | 'strcpy:N' | 'strcat:N' |
                       'strlen:cmp:N' | 'strlen:len:N' |
                       'memset:imm:N' | 'memset:ch:N' | 'memcpy:imm:N', new_src)
gen_charret(src, orig=None) -> ('charret:char' | 'charret:uchar', new_src)
gen_i64glob(src)            -> ('i64glob', new_src)
gen_ucharbx(src)            -> ('ucharbx:bvar:NAME' | 'ucharbx:concat:N' |
                                'ucharbx:proto:NAME', new_src)
```

Only `transform_stringops` (combined) and orig-gated `transform_charret`
are worth the compile as a no-search fold. i64glob / ucharbx stay in
this module until a wider add/adc or `8a; 53` batch shows up.
