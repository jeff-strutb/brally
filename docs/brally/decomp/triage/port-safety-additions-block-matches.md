# Port safety additions block matches

*Recorded 2026-08-21.*

> The port's defensive additions - bounds guards, bounded string helpers, reordered wrapper signatures, NULL checks - are a large and systematic match-blocking class.

**Found 2026-08-21 while taking slice2_24 from 1/7 to 21/33.** The single
biggest blocker in that packet was not compiler behaviour. It was code the
PORT added that the original never had.

Four distinct sub-classes, all of which fully resolve:

1. **Bounds guards on table reads.** The original indexes blind:
   `mov eax,[g]; movsx cx, byte [eax+tab]`. The port wrapped every lookup in
   `(i < 4u) ? tab[i] : 0`, which adds a `cmp`/`jae` and costs the match. Nine
   caption setters fell to deleting the guard.
2. **Bounded string helpers.** `BrStrCopy(dst, sizeof dst, src)` where the
   original has MSVC's INLINE `strcpy` (`repne scasb` + `rep movsd/movsb`).
   Write `strcpy` and let /O2 inline it.
3. **Wrapper signatures in a different argument order.** `BrItoa10(dest, size,
   value)` vs the original's real `_itoa(value, dest, radix)`. A different
   callee with a different push sequence can never match. Restore the CRT
   signature; `#ifdef _MSC_VER` to `_itoa` and supply a small implementation
   for the macOS side.
4. **NULL checks the original does not make** - on vtable pointers especially.

**These are safe to remove HERE, and say why in the source.** Every buffer in
that packet is a known size holding a value of known maximum length, so the
bounded forms were never doing work. Do not delete a guard without checking
that; the point is faithfulness, not bravado.

**Also learned in the same pass:**

- **Branch polarity is load bearing.** `if (flag == 0) return const;` inverts
  the jump against an original that `je`s to the constant path. Read which
  side falls through. (BrMenuCap0950.)
- **Branches must yield the FIELD, not the index,** where the original
  specialises the constant case. `e = c ? 0 : i;` then one `tab[e].f08` makes
  VC5 keep a generic index and load once at the join; the original folds the
  zero case into an absolute `mov eax,[base+8]`. Name the field in each
  branch. (BrMenuText15A0.)
- **Deferred loads need an inner block.** Naming `pText`/`pVtbl` at the top of
  a function makes VC5 compute them BEFORE an intervening call and carry them
  across it, costing a spill. Declaring them in a `{ }` block after the call
  is what defers them. This made 0A50/0AC0 go from size-exact-but-wrong to
  exact.

Related: [matching-progress](../log/matching-progress.md), [divergence-class-triage](divergence-class-triage.md),
[thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md), [untagged-functions-are-free-matches](untagged-functions-are-free-matches.md).


## 2026-09-03: the GLOBALS-STRUCT parameter is the concrete form of this
*(six byte-exact matches in one pass, all from the same fix)*

src/brally/core/slice2_23.c's port bodies take `(BrUiObj *pObj, BrUiGlobals *pG)`
and reach every global through `pG->...`. The originals are cdecl with ONE
argument and direct global addresses. That single difference is the whole
gap -- six extra `mov r,[r+disp]`, an extra push, an extra call, ~130
diffs on a 147-byte function.

**The fix is already a convention in that file**, from an earlier session:
the port body keeps its name but loses its `@implements`, gaining the
comment `port-only body; Glide match is src/brally/core/generated/<VA>.c`, and a
separate per-VA TU carries the tag with direct externs. Look for that
comment next door before assuming a function is unclaimed. Byte-exact this
way, all first compile: 0x100386B0, 0x100381D0, 0x100391F0, plus the
0x10039270 / 0x10039350 / 0x10039510 triplet.

**Screening gotcha that cost time:** these functions are tagged by their
D3D VA, so `git grep "implements <glide VA>"` finds NOTHING and they look
unclaimed. Check build/brally/win32/match/report.csv (which resolves d3d tags to glide
VAs) instead, or you will write a duplicate TU as I did twice.

**Bookkeeping:** matching one in the cpp lane leaves the slice's d3d tag
claiming the same VA, so the residue carries a phantom `diff` row forever.
Converting those six stale tags dropped the C residue 359 -> 350 with no
image change. Do the cleanup in the same commit as the match.


## 2026-09-03 (later): the blocker is in slice3_32.c too, and the stale-tag
## cleanup is worth as much as the matches

Second file with the same shape: src/brally/core/slice3_32.c's port bodies also
take a globals-struct pointer the originals do not have. Same fix, same
result -- byte-exact first compile on 0x10041100, 0x10041160, 0x10040DD0,
0x10040D80, plus 0x10038E10 and 0x10039870 in slice2_23.c.

**Screen for it like this, it takes seconds:**
`grep -rn "Globals \*p" src --include='*.c' | awk -F: '{print $1}' | sort |
uniq -c` finds the files; then for each of that file's `diff` rows in
report.csv check `report_cpp.csv` for a `match` row on the same VA. Nine of
slice3_32.c's fourteen diffs were ALREADY matched in the C++ lane and were
just stale d3d tags; converting them to
`/* port-only body; Glide match is src/brally/core/cpp/<VA>.cpp */` dropped the C
residue by nine with no image change. Do this before assuming a file has
real work left -- across both files the cleanup removed 15 phantom rows
(residue 359 -> 337) while the actual matches numbered 8.


## The blocker has THREE forms, not one (all found 2026-09-03)

1. **A globals-struct PARAMETER** the original does not have
   (`BrUiGlobals *pG`, `BrUiNav *`): every global access becomes
   `mov r,[r+disp]` instead of an absolute address, plus an extra push per
   call. slice2_23.c, slice3_32.c, br_uinav.c.
2. **A shared HELPER the original inlines**, or writes out twice. 0x10041180
   has both axes spelled out where the port factors one per-axis routine;
   0x10039870's key lookup likewise.
3. **A temp added to "preserve" an order the compiler produces anyway.**
   0x100400E0's port body names the root-phase pointer so the load lands
   before an unrelated store - VC5 does that hoist itself, and the name
   costs the eax accumulator encodings. See [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).

All three read as careful, defensive port code. All three are the gap.
Screen with `grep -rn "Globals \*p" src --include='*.c'` for form 1; for
forms 2 and 3 just read the port body next to the asm - it usually
documents the original's behaviour correctly and only the SHAPE is wrong.


## 2026-09-03 (session 14): the particle-step family, and the ONE-LINE screen
## that finds form 1 without reading any source

Three more in src/brally/core/slice2_21.c, all form 1: BrPfxTick (0x10033BB0, 219 B),
BrPfxUpdateB0 (0x10033880, 315 B) - both BYTE-EXACT - and BrPfxUpdateB4AC
(0x100339C0, 398 B, parked one instruction short). The port bodies take
`(BrPfxPool *, const BrPfxEnv *, const BrCarFxEnv *, const BrPfxTickEnv *,
uint32_t *)`; every original takes NOTHING.

**The screen that decides form 1 in seconds, with no source reading: look at
the CALL SITE in the original.** `tools/brally/dumpasm.py <caller VA>` - if the call
is a bare `call rel32` with no pushes in front of it, the callee is
`void f(void)` however many parameters the port body declares. In this family
one disassembly of BrPfxTick settled the signatures of four functions at once
(`mov ecx,[esi]; call` for the three per-car helpers is __fastcall on the car
pointer alone; the three pool steppers take nothing). That is faster and more
certain than the `grep "Globals \*p"` screen, which only catches the
struct-named form.

**Two levers finished these once the parameters were gone; both are now in
docs/brally/VC5-IDIOMS.md:**
 - respell `array[idx].field` in EVERY statement - hoisting the record into a
   `Rec *p` local cost 19 bytes and ten instructions on a function whose
   statements were all already correct (regnorm 48+21 -> 18+8);
 - a redundant LEFT grouping paren around the already-implicit left group of a
   float sum, `(prod*dt + drift) + pos`, took the same function from 4 diffs to
   byte-exact. Permuting the summands does nothing (VC5 canonicalises
   commutative float addition) but this tree-preserving paren does.

**Placement convention confirmed:** the port body keeps its name, loses its
`@implements`, and gains `port-only body; Glide match is
src/brally/core/generated/<VA>.c`; the new per-VA TU under `src/brally/core/generated/`
carries the glide tag, direct externs, and its own callee prototypes. A
duplicate definition of the same symbol across the two TUs is fine - the
matching build is compile-only and image_build resolves by symbol name.
`tools/brally/match_sweep.py <new file>` auto-files and auto-commits a MATCH.
