# Structural generators 2 — decision logic

Standalone transforms (`tools/gen_structural2.py`). Do **not** copy this file
into `ghidra_to_match.py` blindly: fold the *high-value* ones into
`_refine_candidates` as one candidate per function. Proven against
BRGlide.dll orig bytes. Residue: 184 unmatched refine rows
(`python3 tools/ghidra_to_match.py --residue`).

Three recurring idioms from the structural batch (VC5-IDIOMS-dll2.md). Each
generator yields `(label, mutated_source)` in the `_refine_candidates`
style. `--validate` scores `transform_*` (all edits of that generator)
with `ghidra_to_match._score_source`, opts `/O2`, `/Od`, `/O2 /Oy-`.

`--from-decomp` isolates the transform (wrap of `build/ghidra_decomp`).
Work-file scores are the residue as it sits (some VAs already hand-fixed).

## Verdict

| generator | prey (decomp) | moved | MATCH | worse | fold? |
|---|---|---|---|---|---|
| **retnotemp** | 9 / 184 | **7** | **5** | 1 | **FOLD** |
| **ge0** | 6 / 184 | **5** | 0 | 0 | **FOLD** |
| lebound | 3 / 184 | 2 | 0 | 0 | **SKIP** as combined — n=3, 0 MATCH |

Hit-rate that matters is from-decomp. Work already contains the folded
spelling on 0x1006BAA0 / 0x1006B6E0 / 0x1006BB10 (retnotemp) and
0x1006E130 (ge0) / 0x100378C0 (lebound). Work-only retnotemp still moved
2 / 4 residue = 50%; ge0 moved 4 / 5 fired = 80%.

---

## 1. retnotemp — FOLD

Ghidra prints `i = f(...); return i != 0;` (and `== 0`, and
`return (uint)(i != 0)`). Orig of the call-in-the-return expression is
`neg; sbb; neg` (`f7 d8 1b c0 f7 d8`); the temp form is
`xor r,r; test eax; setne r; mov eax,r` (+3). `== 0` is `neg; sbb; inc`
(`f7 d8 1b c0 40`). Proven MATCH 0x1006BAA0 / 0x1006B6E0 (75 B) /
0x1006BB10 / 0x1006B4F0 / 0x10058F90 (12 B wrapper). Adjacent statements
only; a load + intervening store (0x1006B530, orig `setne`) is not prey.

### Fold recipe

One candidate, not a search. Seed from the wrapped TU. Do not write
`ghidra_work` from this module.

```
def _ret_notemp_rewrite(src):
    import gen_structural2 as gs
    new, labels = gs.transform_ret_notemp(src)
    if new != src:
        yield ('retnotemp', new)
```

`transform_ret_notemp` folds every adjacent `tmp = call(...); return tmp
!= 0;` (and `== 0`, and the `(unsigned int)(tmp != 0)` wrapper) into
`return call(...) != 0;`. The unused `int tmp;` decl stays (/Od slots).
Per-site yields (`retnotemp:ne:TMP`, `retnotemp:eq:TMP`) stay available
for a climb; they are not the fold.

Do **not** fold a non-call assign (`i = tab[k]; return i != 0`) or a temp
that is stored in between (0x1006B530).

### Measured delta (from-decomp wrap)

Scored 2026-08-27. 9 prey of 184 residue (+ extras). 7 moved, 5 MATCH, 1
worse.

| VA | before | after | delta | what |
|---|---|---|---|---|
| 0x1006BAA0 | 15 | **0** | **-15** | `return FUN_1006b790(...) != 0` |
| 0x1006B6E0 | 15 | **0** | **-15** | twin |
| 0x1006BB10 | 15 | **0** | **-15** | `== 0` (`neg; sbb; inc`) |
| 0x1006B4F0 | 15 | **0** | **-15** | `== 0` |
| 0x10058F90 | 6 | **0** | **-6** | 12 B `return FUN_10058e20() != 0` |
| 0x10002EB0 | 56 | 53 | **-3** | `(uint)(i != 0)` after IAT call |
| 0x10002F10 | 56 | 53 | **-3** | twin of 0x10002EB0 |

Unmoved: 0x10002830 23→23 (CONCAT arg still dominates). Worse: 0x10002F70
99→100 (other CD branches; hill-climb rejects +1). Do not revert a
sound-wrapper MATCH — the +1 is a different function.

The five MATCH VAs are already in the tree; from-decomp isolation is the
proof the temp was the whole wall (15 diffs of `setne` vs `neg; sbb`).
Residue still open: 0x10002EB0 / 0x10002F10 at 53.

---

## 2. ge0 — FOLD

Ghidra canonicalizes `x >= 0` to `-1 < x` (and rarely `x > -1`). Orig
`if (x >= 0)` is `test r,r; jl` (`85 xx 7c`); Ghidra's spelling is
`cmp r, -1; jle` (`83 xx ff 7e`). Pushes may sit between test and jl
(0x1006E0A0: `test eax; push esi; push edi; jl`). Proven as the opening
of 0x1006E130 (55→2; leftover is the SIB wall `[edi+esi]` vs `[esi+edi]`).

Prey requires orig `test; jl` — Ghidra prints `-1 < x` for both
spellings, and rewriting a real `x > -1` would move *away* from orig.
Char/short Ghidra temps (`cVar*` / `sVar*`) are skipped: their `-1 <` is
an ASCII window (`c - 0x20`, 0x100541B0 / 0x10054280), not this idiom.

### Fold recipe

One candidate, not a search.

```
def _ge0_rewrite(src):
    import gen_structural2 as gs
    new, labels = gs.transform_ge0(src)
    if new != src:
        yield ('ge0', new)
```

`transform_ge0` rewrites every `-1 < X` and `(X > -1)` to `(X >= 0)`,
keeping `(int)` casts (`(-1 < (int)param_1)` → `((int)param_1 >= 0)`).
Does not touch `i + -1 < bound` or `<<`.

### Measured delta (from-decomp wrap)

Scored 2026-08-27. 6 prey of 184 residue. 5 moved, 0 MATCH, 0 worse.

| VA | before | after | delta | what |
|---|---|---|---|---|
| 0x1006E130 | 55 | 2 | **-53** | `param_1 >= 0` (SIB leftover) |
| 0x10006460 | 38 | 3 | **-35** | `DAT_10226a28 >= 0` |
| 0x100356B0 | 130 | 105 | **-25** | HRESULT `iVar1 >= 0` ×2 |
| 0x100031D0 | 84 | 67 | **-17** | `(int)param_1/2 >= 0` |
| 0x1006E0A0 | 119 | 102 | **-17** | `param_1 >= 0` (twin of 0x1006E130) |

Unmoved: 0x10038250 46→46 (COM HRESULT after a call; 3-byte `cmp -1` vs
2-byte `test` is a mid-function length change that did not drop the
total). Closest non-MATCH: 0x10006460 at 3 / 100; 0x1006E130 at 2 / 71
(SIB — stop). Work already has `>= 0` on 0x1006E130 (no-op).

---

## 3. lebound — SKIP as a combined fold

Ghidra `if (n < K+1)` vs orig `if (n <= K)`. Distinguisher: orig
`cmp n, K; jg` (`83 xx K; 7f`, a push between cmp and jcc is legal —
0x100378C0 `cmp esi,8; push edi; jg`) vs `cmp n, K+1; jge`. Dual of
`_refine_candidates` (i) `X > C` → `X >= C+1`. Proven 0x100378C0
(`param_1 < 9` → `param_1 <= 8`). Wrapping parens required, so
`for (i = 0; i < 8; i++)` does not fire. Orig-guided `transform_lebound`
keeps only sites whose orig is `cmp K; jg/jle`. `cVar*` / `sVar*`
skipped (same ASCII window as ge0).

Prey: 3 / 184. 2 moved, 0 MATCH, 0 worse. 0x100378C0 15→13 (the `<= 8`
is right; `uType = 0` local + shredded table still dominate).
0x10060A30 150→148. 0x1006AEB0 182→182 (error-class). n=3 and 0 MATCH
is not worth a combined compile. The remaining lever on 0x100378C0 is
not this compare.

Do not fold as one always-on candidate. If you add anything, add it as
a one-site search next to (i) — hill-climb already knows how to reject
a no-op:

```
def _lebound_rewrite(src):
    import gen_structural2 as gs
    for label, cand in gs.gen_lebound(src):
        yield (label, cand)
```

That is optional. Not the recommendation.

---

## CLI

```
python3 tools/gen_structural2.py --dry-run
python3 tools/gen_structural2.py --validate
python3 tools/gen_structural2.py --validate --from-decomp
python3 tools/gen_structural2.py --va 0x1006BAA0 --from-decomp --gen retnotemp
python3 tools/gen_structural2.py --va 0x1006E130 --from-decomp --gen ge0
python3 tools/gen_structural2.py --self-test
```

`--from-decomp` never writes `ghidra_work`. `--validate` does not write
learnings.

## `_refine_candidates` signatures

```
gen_ret_notemp(src) -> ('retnotemp:ne:TMP' | 'retnotemp:eq:TMP', new_src)
gen_lebound(src)    -> ('lebound:X<=K', new_src)
gen_ge0(src)        -> ('ge0:lt:X' | 'ge0:gt:X', new_src)
```

`transform_ret_notemp` / `transform_ge0` are the folds. `transform_lebound`
is orig-guided when orig bytes are passed; without orig it applies every
parenthesized `X < C` (C ≥ 1) and is not the fold.
