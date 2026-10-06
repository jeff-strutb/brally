# Five largest

*Recorded 2026-09-13.*

> 2026-09-13 '5 of the largest to T4/T3': three @t3 (0x100498A0 3993 B, 0x1004CBA0 3671 B, 0x1005C8B0 1951 B at 1951/1951 rows 0+0) and two T2 (0x100038F0 packet dispatcher, 0x10066D70 tip kick at 1774/1782, A1 gap 4 vs 3). The levers and walls, per function.

**Picked by size from `tiers.py --list`** after screening: the four largest
T1 rows are all C++ EH (prologue `6a ff`), so the C++ lane again.

- **0x100498A0 / 0x1004CBA0 (menu-page builders, @t3)**: `gen_menubuilder.py
  --partial` gives the skeleton; the hand blocks are a selector fill loop and
  the photo trio. The fill loop is `for (i = 0; i < n; i++)` with `i + 1` as the
  itoa argument -- VC5 CSEs it with the increment (`lea ebp,[eax+1]` ...
  `mov eax,ebp`); a `do { i++; ...}` is `inc`. A PLAIN virtual add call inside
  the loop is what C1XX hoists to a slot before the loop (the named
  pointer-to-member form is NOT it here). Two 32-byte buffers; on CBA0 the
  strcpy/strcat labels are the inlined intrinsics. Both certified on the
  photo1 pairing wall of the trio (0x1004AEE0.cpp dead list); ledgers were a
  120-order generated sweep + 22 options + corpus MISS.
- **0x1005C8B0 (car control step, @t3, 1951/1951, rows 0+0)**: thiscall
  method in the C++ lane (0x1005C6D0.cpp's object). Levers: `SQ(expr)` over
  the RAW differences (a CSE temp homed with `fst` and multiplied by its
  home), z difference first, the two squares as SEPARATE statements, the sum
  left UNNAMED in the condition (`fcom` keeps it for `best = ...`); the latch
  triple as an ARRAY (aggregate slot between the spilled scalars and the
  next aggregate); `p29C0->Ack(bit)` as the input record's own thiscall (its
  `this` is why the pointer is re-read after a call, and why there is no
  `add esp`); signed `char` latches so the 0x80 store constant is not the
  test mask; `int` flags so the `& 1` mask joins the 1-web; every float gate
  spelled then-arm-first (`x > K` before `x < K`). Residue = four
  register-colouring clusters (global load order, ext/flags eax<->edx,
  &f30/&fF24 ebx<->edi, one temp home).
- **0x100038F0 (packet dispatcher, T2, 4240 B ours incl. 312 B of switch
  tables that the cpp score counts)**: per-field slot globals + `slot*0x978`
  (a struct array CSEs the full address); `>= 2`-first decode arms; the
  timeout global read ONCE; pointer-walk reset loop with `do {} while (--n)`
  (a `for (i<8)` becomes three rep stosd). Wall: the original never hoists
  `t`/nMode/import addresses out of the packet loop; ours does. Compiling
  WITHOUT /Oi makes 0x2f..0x337 exact including the slot layout but the
  original's strcpy IS inline -- register pressure in case 0, not the loop.
  Slot order of scalars at /O2 is NOT name-keyed (full rename = identical)
  and not declaration-keyed.
- **0x10066D70 (tip kick, T2, 1774/1782, regnorm 0+4)**: replaces the port's
  loop form in the matching arm of br_collresp.c (the `#define Name
  Name_port` recipe). Levers: ONE wheel pointer assigned INSIDE the arm from
  a re-read of `child[k]`, test through the field, uninitialised (the
  original's block-1 skip path reloads garbage from the slot `best` shares);
  z term needs `(double)f1E8 - f1E4 * K`; abs as the two-read conditional
  inside the compare. Wall: the chassis dot `s` -- VC5 canonicalises every
  permutation/association/operand-kind identically (m loaded first), the
  original loads n first: 4 fxch, A1 gap 4 vs 3.0.

**Tools:** `cprobe.py` (scratch) = compile one option + divergence in ~2 s;
`cpp_score.score_source` for generated sweeps; `fn.py --var` writes to
`build/brally/win32/match/t3d/fn_<VA>_<tag>.c`. t3.py counts ledger lines only inside the
@implements comment block -- check the anchor line exists before pasting.

Related: [five-largest-2026-09-12b](five-largest-2026-09-12b.md), [largest-to-t3-survey-2026-09-12](largest-to-t3-survey-2026-09-12.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [callee-saved-zero-web-class](../levers/callee-saved-zero-web-class.md).
