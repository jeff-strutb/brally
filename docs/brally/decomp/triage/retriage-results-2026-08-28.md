# Retriage results

*Recorded 2026-08-28.*

> The structural re-triage PAID: 2 new byte-exact matches (741->743) plus a missing sweep variant (/O2 /Op) that made float-heavy functions unmatchable. Confirms 'coloring wall' verdicts were mostly port-safety/missing-code misdiagnoses.

**2026-08-28.** Eight workers on the top targets from
[residue-retriage-2026-08-28](residue-retriage-2026-08-28.md). Every one was confirmed structural, not
register allocation. Commit `7a3060f`.

## Matches landed: 741 -> 743 (`tools/brally/total.py`: 743 fns / 77,498 B)

- **BrNetReset 0x10005CD0**, 430 B. The port had aggregated loose globals into
  a struct - the original takes NO argument and reaches ~20 fields as flat
  globals (`mov eax,[0x10226a54]`), the tree went through `pNet->...`.
- **BrCarDrawWheels 0x10009C10**, 932 B. See the macro idiom below.

Both verified with `match_sweep.score`, not just the fnmatch harness;
per-file sweep moved report.csv 599 -> 601 MATCH.

##  MISSING SWEEP VARIANT: `/O2 /Op` (bulk lever)

`tools/brally/match_sweep.py` VARIANTS was `/O2`, `/Od`, `/O2 /Oy-`. **A TU the
original compiled with precise FP can never match under any of them.** Without
`/Op`, MSVC 5.0 keeps an int->float conversion in the x87 register; with it,
every conversion is followed by the round-to-float idiom
`fstp dword [tmp]; fld dword [tmp]`, and `/ 2.0f` stops being strength-reduced
to a multiply. Found on 0x100215C0: a large multiset gap under `/O2`, a
SINGLE surplus `fxch` under `/O2 /Op`. `('O2p', '/O2 /Op')` added.
Every float-heavy function in the residue was invisible to this before.

## New proven idioms (also in docs/brally/VC5-IDIOMS.md)

- **Macro vs function changes EVALUATION ORDER.** BrCarDrawWheels' two-word
  command append stores the first word *before* loading the global for the
  second. A function - even `__inline` - evaluates its arguments first, so VC5
  CSEs that load ahead of the guard test and cross-jumps the two arms into one
  append. It must be a macro. (Equivalent here because every argument at every
  site is a pure load or a constant.)
- **`v != 0.0f` is ONE `fcomp`/`test ah,0x40`**; `(v < 0.0f) || (v > 0.0f)` is
  two compares. Keep the two-compare form under `#else` for the port.
- Inline vs out-of-line helpers dominate: BrRcaFixup showed +76 `call`,
  +85 `push`, +77 `add esp,I` against 64 missing `mov B,[R+I]` - the original
  inlines its byte-swap as a MACRO at ~24 sites.

##  HARNESS BUG THAT HID FINISHED MATCHES

`tools/brally/fnmatch/fn.py` was not masking RELOCATION slots (zero in an unlinked
.obj, patched at link time). BrCarDrawWheels read as ~261 differing bytes when
it was already byte-exact. `match_sweep.score` has always masked them. Fixed.
**Any per-function harness must mask relocs or it will under-report matches.**

## Close, not landed (all improved, none matched)

| fn | state |
|---|---|
| BrRcaFixup 0x10030770 | REGNORM **0+0**, +6 B, exact insn count. Was retired as a coloring wall; it was port-safety-additions in its purest form. |
| BrScenePropsDraw 0x1001D1B0 | exact size + insns, 12 reloc-masked diff bytes |
| BrGbiCall10021560 0x100215C0 | one surplus `fxch` under `/O2 /Op`. **The tree body modelled the D3D twin while report.csv keys it to the Glide VA** - it needed re-targeting, not polishing. |
| BrCarStateEncode / ...Delta | REGNORM miss 0; blocked on 33 / 18 surplus `xor edx,edx` |
| BrHudDrawDial 0x100140B0 | REGNORM 18+13, RAW == REGNORM (zero register noise) |

## Near-miss verdicts (2026-08-29, by hand follow-up)

- **BrScenePropsDraw g8 / BrRcaFixup g7: PARKED at pure allocation.**
  g8 = 12 masked bytes, all ONE variable (recomp caches pList in edi; orig
  mutates one pointer in place + reloads the param slot). Walking-pointer
  respellings measured WORSE twice (+2 insns, firstdiv collapse) -- the
  residue is pressure-driven. g7 = 23 raw shapes, regnorm 0+0. Both are
  permuter-class; BUT permute.py --src CANNOT seed from a multi-function
  variant file (extractor grabbed FUN_100281c0 / BrTrackSwapRec28 instead of
  the target -- tooling TODO before seeding these).
- **BrCarStateEncode: the thiscall wall is BROKEN (`1db162d`)** -- the
  verdict confused the language for the limit: a __thiscall callee with
  stack args means the caller's TU was C++, and the project HAS a C++
  matching path. As C++ (writer = declared-not-defined class method) all 33
  `xor edx,edx` vanish; exact 1018-byte length, 151 masked diffs left, ALL
  from one uniform per-site idiom: VC5's C++ argument-list scheduling
  (in-place right-to-left pushes UNLESS an argument has a side effect) x
  the narrow-shift-needs-short-assignment rule. Six probes pinned the
  rules -- see src/brally/core/cpp/0x10006510.cpp banner and VC5-IDIOMS.md. The
  push-early + sar-narrow combination is PROVEN MUTUALLY EXCLUSIVE under the
  staged RTM C1XX.DLL (15+ probes + 11-matched-function corpus cross-check).
  FRONT-END HYPOTHESIS TESTED AND DISPROVEN 2026-08-30 (`5cbb6dd`): VS97 SP3
  AND VC6 RTM both downloaded (authorized by the project lead, archive.org), staged in-repo
  (tools/toolchains/msvc5/bin-sp3, tools/brally/msvc6), and both apply byte-identical rules to
  the two-form battery. The 151-byte residue survives every Microsoft C++
  front end from the game's window. Negative map now: 18 spellings x 3 front
  ends x 10+ flags (incl. struct-return members, object-expression forms).
  Remaining hypotheses: a prebuilt static library from a 4th compiler
  (VC4.2?), or an unconceived source shape. ALSO GAINED: SP3's LINK.EXE
  (Nov 1997) is staged -- closer to the shipping linker than RTM's; try it
  if image_build ever hits a linker-layout wall. tools/brally/cabx.py = spanning-
  cab extractor (MSZIP); LZX sets need cabextract (built in scratch). Same treatment applies to ...Delta g4 once
  Encode lands.

- **BrHudDrawDial g5: DIAGNOSED, next up.** 31 shapes, RAW == REGNORM (zero
  register noise), all x87: +7 `fmulp` / -4 `fmul st(i)` / -3 `fmul [esp+S]`
  +3 `fld` -- the float-DAG OPERAND-KIND class, i.e. exactly what the proven
  operand-kind ladder in docs/brally/VC5-IDIOMS.md (the 0x1000EAF0 fourth-pass
  entry) was built for. Start the next sitting there: read the ladder entry,
  map the ~8 sites via `fn.py 0x100140B0 --var g5 --detail regnorm`, apply
  ladder spellings site by site.

Variants live in `build/brally/win32/match/t3d/fn_<VA>_<tag>.c`. Harness:
[fnmatch-harness](../toolchain/fnmatch-harness.md). Method: [register-rotation-is-a-symptom](register-rotation-is-a-symptom.md).
