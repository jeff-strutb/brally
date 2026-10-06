# Store order and position levers

*Recorded 2026-09-10.*

> Two levers that paid on 2026-09-10 second wave -- statement/store ORDER read off the zero-stores, and position-in-TU (which rots silently); plus the exhausted thiscall screen.

Second wave of 2026-09-10, after the READY pool was emptied by the day's first
four sessions. Every remaining row is a FAIL row. Two levers paid.

This is the SMALL-ROW companion to [t3-frontier-map-2026-09-10b](../log/t3-frontier-map-2026-09-10b.md), which maps
the same day's rows >= 500 B into four compiler-decision classes. Read that one
for the big end of the pool; this one is what worked under ~350 B.

## 1. STATEMENT / STORE ORDER, read off the constant stores

The cheapest decisive screen on this tree right now. Where a function stores a
run of constants (zeros, -1.0f), the ORIGINAL'S STORE OFFSETS SPELL OUT ITS
SOURCE ORDER, and matching that order fixes the whole schedule.

- **0x10029EC0 BrMat4Frustum** went 286/288 B and 90/91 insns to 288/288 and
  91/91 at REGNORM 0+0 (positionally identical register-blind) purely by
  writing the matrix **column by column instead of row by row**. The proof was
  the zero-store sequence +0x10, +0x30, +0x04, +0x34, +0x08, +0x18, +0x0c,
  +0x1c, then -1.0 at +0x2c, then +0x3c = m[1][0], m[3][0], m[0][1], m[3][1],
  m[0][2], m[1][2], m[0][3], m[1][3], m[2][3], m[3][3] -- the four columns.
  Certified @t3 the same session.
- **0x100342B0 BrVec3Cross** went byte-exact on ONE probe by declaring the
  temporaries **z, y, x** instead of x, y, z. The original computes z's two
  products first; in that order the results land on the x87 stack as y, z, x,
  which is the order the closing `fstp [eax+4]/[eax+8]/[eax]` already wanted.
  Written x,y,z it was the same 33 instructions plus one `fxch st(1)`.
- **0x10054020 BrTextBoxCentreX** went byte-exact once the span-minus-width
  difference became **its own statement** (`t = a - b; v = t * 0.5f; v = v +
  left;`). One expression drops the original's two dead `fxch`. See also
  [declaration-order-tiebreak](declaration-order-tiebreak.md).

Corollary worth reusing: `(f+f)*n` and `f*n + f*n` are bit-identical (doubling
is exact) but only the second emits `fmul` before `fadd st,st`. When the
original's stream shows a product before a doubling, spell the product first.

## 2. POSITION IN THE TU -- rare, big, and it ROTS

`tools/brally/possweep.py` (committed 2026-09-10; lifts a function's comment block through its closing
brace and reinserts it ahead of every top-level definition, scoring each with
`fn.py --var`) swept ~25 rows. Position moved only 3 of them, but when it moves
it moves a lot:

- **0x1001DD00 BrVec3dCross**: REGNORM 8+8 -> 0+0 at the HEAD of br_vecd.c.
  Certified @t3; residue is a pA/pOut eax<->edx relabel plus one crossed
  commutative fold.
- **0x1006D530 BrRbQuatDerivative**: its dossier claimed 21 differing bytes at
  REGNORM 0+0; measured it was 163 at 27+24.  **A NEIGHBOUR EDIT HAD MOVED IT
  AND NOBODY RE-MEASURED.** It must sit immediately ahead of BrRbBuildMatrix.
  **Re-measure a dossier's numbers before trusting them** -- same class as
  [collresp-cluster-2026-09-05](../functions/collresp-cluster-2026-09-05.md).
- Position is completely inert on leaves with no register pressure (checked
  every slot for BrMat4Frustum and BrBitStreamReadS32).

Also: moving a function above a typedef it needs will not compile -- the sweep
must report those slots as skipped, not as a score.

## 3. THE THISCALL SCREEN IS EXHAUSTED

`0x10054020` was an undeclared thiscall: original entry `push ecx` with the
object in ecx, ours reading `[esp+4]`. Fixing it with `BR_THISCALL1` (one
argument, so `__fastcall` is exact) moved FIRSTDIV from +0x1 to +0x34.
A tree-wide screen over every uncertified diff row for that signature returns
exactly ONE more candidate, 0x10022AC0 br_dl_light_vertex, and that one is 79
bytes and a whole entry test away -- not a convention fix. Do not re-run this
screen. Compare [calling-convention-screen](../triage/calling-convention-screen.md), which is the `ret K` variant and
is also spent.

## 4. What is left, and why it is slow

The small FAIL rows split two ways and neither is quick:

- **Documented walls with dead lists**: 0x1006B080 / 0x10037FA0 / 0x10038000
  (byte argument homed by the struct wrapper -- C++ TU lane), 0x1006DD20 +
  0x10029D70 (IV anchored on the last array reference), 0x10018E90 (+2 pointer
  anchor), 0x10015550 (`neg`-built -100 multiple), 0x10039D20, 0x10058900.
- **Gate-A4 scheduling blocks**: 0x10058540, 0x10058900, 0x10060F40,
  0x10013FD0, 0x1002A050, 0x10063CC0. These sit at IDENTICAL instruction
  multisets (rows 0+0 or 1+0) and fail only A4's positional walk, by 2-17 bytes
  over the 32 B tolerance. They are exactly what rule 12 calls scheduling
  residue, and the gate still says no. **Do not widen it**
  ([do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)) -- fix the schedule or park.
  0x10063CC0 BrReplaySeek: ten spellings of a `v < 0` clamp all score 5; VC5
  folds a zero compare into the preceding `add`'s flags from everything
  reachable in C.

 `--qualify <VA>` PRINTS THE TAG FOR AN ALREADY-CERTIFIED ROW. 0x100643E0
looked like a free certification (gates 0+A and B both PASS) and was already in
`--vas`. Always filter against `t3.py --vas`.

## 5. A MALFORMED @t4-pass LINE SILENTLY LOSES THE WHOLE PASS

0x10032320 BrDpLobbyConnect carried `@t4-pass 2026-09-09 probes=9
result=-6B/raw0+1 census no`.  Gate B does not parse that shape, so t3.py
reported "no counted @t4-pass lines yet" and nine probes of work were invisible.
Restated in the counted form the gate now says the honest thing -- "thin passes
[1] not counted", 9 probes against the 10 it wants.

Sweep for the class with:

    grep -rhoE "@t4-pass [^*]*" src/brally/ | grep -vE "@t4-pass 0x[0-9A-Fa-f]{8} [0-9]+ [0-9]{4}-[0-9]{2}-[0-9]{2} probes [0-9]+ bytes [0-9]+ insns [0-9]+ regions [0-9]+ rows [0-9]+ census (yes|no)"

Most hits are prose inside a @t3 tag ("@t4-pass passes (ledger lines above...)")
and are harmless; as of 2026-09-10 that was the only real one left. Companion to
the duplicate-@t3-block sweep already in [resume-state](../log/resume-state.md).
