# SPEC — convert the corpus finding into a match on 0x1000A110's byte-lane wall

Handoff written 2026-09-03. Self-contained: everything needed is here or
named. **Read `src/core/drawing/br_drawcar.c`'s file header before editing** —
it holds ~10 sessions of dead probes and this spec assumes you have not
re-run them.

## The target in one paragraph

`0x1000A110 BrCarDrawVehicle` (`src/core/drawing/br_drawcar.c`) is 18
register-blind multiset rows from shape-exact. **Ten of those 18 are ONE
defect at TWO sites** — the colour-pack byte lane in arm 1 and arm 3. The
other 8 rows are already proven dead (a float operand swap, two probes; a
`mov R,[A]` reload, the pCam entry). So this defect is the whole remaining
job on this function.

## Measured baseline — re-measure, do not trust these

    34 raw divergence regions
    orig 1,843 insns vs ours 1,835 (+6 pad)   -> EIGHT short
    orig 7,577 bytes vs ours 7,546            -> 31 short
    register-blind multiset: 13 MISSING / 5 EXTRA = 18 rows

    MISSING: and R,0xff x3 | or R,R x3 | mov byte ptr [esp+S],B x3 | xor R,R
             (+ fld/fadd pair and one mov R,[A] — those are the dead 3)
    EXTRA:   mov B,B | mov B,[esp+S] | mov [esp+S],R
             (+ the fld/fadd counterpart)

Commands:

    .venv/bin/python tools/match_sweep.py src/core/drawing/br_drawcar.c    # ~12s
    .venv/bin/python tools/divergence.py build/match/obj_O2/br_drawcar.obj \
        build/match/orig/0x1000A110.bin BrCarDrawVehicle --deltas
    .venv/bin/python tools/msetdiff.py build/match/orig/0x1000A110.bin \
        build/match/obj_O2/br_drawcar.obj BrCarDrawVehicle 25

## What the original does, and what we do

Read instruction-for-instruction (already in the dossier, do not re-derive).
The original materialises BOTH byte locals into their slots before colourA
and reads BOTH back with a dword-load + `and 0xff` widening:

    mov [esp+0x31],dl ; mov [esp+0x32],cl
    mov ecx,[esp+0x32] ; mov dh,al ; mov eax,[esp+0x31]
    and ecx,0xff ; and eax,0xff ; or edx,eax

…and then does the whole thing a SECOND time for colourB. We home only one
of the two and let VC5 forward the other out of a live byte register, so the
`top << 8 | pack[0]` merge collapses into two byte-lane moves
(`mov dh,al ; mov dl,al`) instead of `mov dh,al ; or edx,<widened>`.

Region 4/5 is arm 1 (orig 0x327..0x3c0, −37 B); region 6 is arm 3
(orig 0x3ca..0x427, −24 B). Those two arms are 61 of the byte deficit.

## THE NEW INFORMATION (this is why the spec exists)

`tools/corpus.py` indexes the ORIGINAL bytes of 1,036 byte-exact functions.
Asked about this wall it returned two facts nobody had:

1. **The widening run exists NOWHERE in the solved tree.** Neither
   `mov R,[esp+S]; and R,0xff; or R,R` nor
   `mov byte [esp+S],B; mov R,[esp+S]; and R,0xff` appears in any of the
   1,036 functions. Ten sessions were spent permuting spellings for a
   construct never proven to exist. **Stop guessing at it.**

2. **The closest proven relative is `BrGlRectFill` 0x1001E380**
   (`src/core/drawing/br_dlglide.c`, byte-exact), which emits the
   two-instruction widening FOUR times (+0xc7, +0x103, +0x11b, +0x125).
   Its source is four `uint8_t` locals — `bR bG bB bA` — that are
   **assigned on BOTH ARMS of an if/else** and then read **sixteen times**
   after the join (four vertices x four channels, as `(float)bR` …).

       .venv/bin/python tools/corpus.py show --va 0x1001E380 --at 0xc7 --len 20

**Hypothesis to test:** what makes VC5 home a byte local and read it back
widened is the SHAPE OF ITS LIVE RANGE — assigned on more than one control
edge, and/or many uses surviving past a join — **not** an explicit `& 0xFF`,
which is already proven inert at this site.

This is the same axis as the `x = a; if (c) x = b;` vs true-if/else entry
proved on 0x1000EAF0 (see `docs/VC5-IDIOMS.md`), arriving from the other
direction.

## Probes, in order

Our arm 3 currently gives `pack[0]`/`pack[1]` TWO SHORT LIVE RANGES —
assigned, read once for colourA, reassigned, read once for colourB. The
original reads them widened BOTH times. Every probe below attacks that.

1. **Screen first, it is free.** Before editing, ask the corpus about the
   rest of the missing rows — especially `xor R,R`, which in arm 1 is
   `xor ecx,ecx ; … ; mov ch,al` (a zeroed base then a byte lane):

       .venv/bin/python tools/corpus.py find --pattern 'xor R, R; mov B, B' --source
       .venv/bin/python tools/corpus.py find --from 0x1000A110 --at 0x327 --len 14 --source

2. **Function-scope byte locals assigned on EVERY arm.** The untried
   combination. Four distinct byte locals declared at FUNCTION scope, each
   assigned in all three arms, read after the joins. ‼ Distinct locals for
   arm 3's colourA *alone* is DEAD (byte-identical, session 10) and
   block-scoping is DEAD (measured) — the new element is function scope
   **plus** assignment on multiple edges. Do not conflate it with either.

3. **Hoist the pack assignment above the if/else** so one definition
   dominates both arms and every read is post-join — the BrGlRectFill shape
   stated minimally.

4. **Widen the use count.** BrGlRectFill's locals have sixteen reads. If 2
   and 3 both fail, the discriminator may be use COUNT, not edge count —
   check whether the original's arm reads each byte more times than ours
   does before inventing a spelling for it.

## Do NOT re-run (all measured, all in the dossier)

- `(pack[0] & 0xFFu)` / `(pack[1] & 0xFFu)` explicit widening — byte-identical
- arm 1 block-scoped `packA0`/`packA1` — un-merges but moves first divergence
  27 B EARLIER; net regression
- arm 3's colourA given its own byte locals — byte-identical
- `pack` as two scalars instead of `uint8_t pack[2]` — the ARRAY is
  load-bearing (VC5 never enregisters an array); keep it
- regions 2/3: swapping the `|` operands, adding `(uint32_t)` to the low
  term, splitting the top component out, hoisting it to a block temp,
  swapping the two globals' roles — all five byte-identical
- `topA` assigned FIRST in arm 3 — already in the tree, keep it
- the float operand swap and the pCam reload — both dead, both documented

## Verification, and the traps

After every probe: one-file sweep, then divergence + msetdiff. **Judge on the
multiset (13/5 baseline), not the region count** — the dossier records a probe
whose region count stayed flat through both its good and bad halves. Also:

- **Check the frame holds** (`sub esp,0x4c`). A frame collapse shows as
  first-divergence dropping toward +0x2 and invalidates everything after it.
- **Check the FIRST DIVERGENCE ADDRESS**, not just totals.
- ‼ **A spelling can improve every number this project measures and still be
  provably wrong** (0x1000EAF0 pass 24). Before accepting any control-flow
  change, disassemble the site and check the ARM ORDER against the original.
- ‼ **A byte or instruction total is not progress when a lost-sync gap
  exists.** Read `divergence.py`'s `NEVER COMPARED` line every time.

## Done / stop

- **Done** = multiset 13/5 → 0/0 and the sweep says MATCH. Commit
  immediately, on its own, with a pathspec (`git commit -m "…" -- <paths>`);
  parallel sessions run here and a bare commit takes their staged work.
- **Partial** = multiset drops but not to zero: keep it only if the FIRST
  DIVERGENCE also moves later or holds. Record the numbers in the file
  header.
- **Stop** at ~40 minutes or when three probes in a row are byte-identical.
  Write an honest residue note naming what you ruled out, add any proven
  idiom to `docs/VC5-IDIOMS.md`, and re-run `tools/corpus.py build` if the
  match count moved.

Realistic odds: this site has killed a lot of neighbouring ideas already.
The corpus finding is genuinely new information, but a lead is not a match.
