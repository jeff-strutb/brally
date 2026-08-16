# Conventions

Rules this port follows. They exist because breaking each one has already cost
real time on this project, and most of them are not obvious.

## Files and types

- Modules are `port/src/<name>.c` + `port/include/<name>.h` + `port/tests/test_<name>.c`.
- **Reuse the shared types; never redefine them.** `br_vec.h` (`BrVec3`),
  `br_vecd.h` (`BrVec3d` — a *separate* double-precision type, never fold it into
  `BrVec3`), `br_mat.h` (`BrMat4`, row-major, row-vector convention),
  `br_phase.h` (`BrPhase_`, the canonical 0xC8 object), plus `br_seg.h`,
  `br_pool.h`, `br_bits.h`, `br_slots.h`, `br_span.h`, `br_crt.h`.
- COM/vtable types are named for the interface (`BrDPlayVtbl`, `BrDSoundVtbl`),
  never a generic `BrComVtbl`. Two incompatible `BrComVtbl` definitions once made
  a pair of headers unable to share a translation unit.
- Before naming anything, grep `port/include/` for the address. One address
  accumulated four names; the fix is always to reuse, never to coin a fifth.

## Transcription

- **Preserve original behaviour, including bugs.** Where you deliberately depart,
  comment the line beginning `DEVIATION:`. Several genuine defects are reproduced
  on purpose — an acceleration solver that never sums Z, a conflict pass that
  wipes its own results, a lap timer that loses a second on exact values.
- Keep the original's argument order even when it is inconsistent. `BrMat4Copy`
  takes source first while every vector routine takes destination first;
  `BrVec3dCross` puts the output third while `BrVec3Cross` puts it first.
  Preserve and document — do not harmonise.
- x87 operand order is **not** apparent without tracing the stack through every
  `fxch`. Trace it. If you cannot resolve a function confidently, leave it out and
  say so. A wrong-but-plausible function links cleanly and fails only at runtime,
  which is the worst outcome available here.
- Comparison polarity: `fcom`/`fcomp` + `test ah,<mask>` sets C0/C3 for
  **unordered** as well, so NaN takes the *true* side. Write these as negated
  comparisons (`!(a >= b)`), never the tidy positive form.
- Read float constants out of the binary rather than assuming them. This is cheap
  and has repeatedly been load-bearing — a dial rendered as a degenerate sliver
  until its two radii turned out to be different constants.

## Portability

- Decode integers **byte-wise**, never by struct overlay. Some payloads are
  big-endian.
- **Byte offsets are 32-bit-only.** On LP64 every pointer widens and struct tails
  shift. Never overlay a struct on a file image or foreign buffer, and never
  allocate an original size literal — `0xC8` under-allocates the phase object by
  104 bytes on a 64-bit host. Use `sizeof`.
- No Win32 types or calling conventions in portable code.
- C99 throughout, `-Wall -Wextra`, zero warnings. Compile-time assertions use the
  negative-array-size trick (`port/tests/test_layout.c`), since C99 has no
  `_Static_assert` and the tree stays on one standard.

## Tests

Assert **behaviour and invariants**, not volume. A test that encodes an
expectation rather than a property of the code is worse than none: three separate
assertions have failed here for that reason — "majority of pixels opaque" against
a transparency-keyed image, a triangle-count threshold against a low-poly model,
a "scissor rects are always in bounds" claim against an asymmetric clamp. In each
case the code was right and the test was wrong.

Prefer mathematical identities (a cross product is perpendicular to both inputs),
round-trips (swap twice restores the original), boundary conditions actually
present in the original, and aliasing behaviour where the original permits it.

## Facts not to re-derive

- Microcode is **F3DEX**: `G_VTX` has `n` in bits[15:10], `v0+n` in bits[7:1].
  Plain F3D's `16n-1` low-byte layout is wrong for this game.
- `0x1007DFE0` is `operator new` (`_nh_malloc(size,1)`) and does **not** zero.
  `0x1007D350` is `malloc`; `0x1007DE40` is `operator delete`.
- `0x1007C8A0` is `__ftol`: truncates toward zero, returns the **low dword** of a
  64-bit `fistp`, so out-of-range yields **0**, not `0x80000000`.
- `0x1007DB00` is `floor` (sets x87 RC toward -inf), not trunc.
- `0x10008B80` and `0x100378A0` are **stubs** in this build despite being called
  with real arguments.
- Anything at or above `0x1007CC40` is statically linked MSVC CRT. Do not port it.
- Entity/car records are stride `0x2B68`; the parallel array is `0x15C`.
  `car+0x1030` is speed in mph; `car+0x10AC` is a struct-of-arrays.
- `.rca`: N64 struct at file `0x8000`, N64 address `0x803C8000`.
- `guLookAtF` (`0x100309A0`) and `guPerspectiveF` (`0x10030930`, **7 args**) are
  **not** stock libultra. `BrAtan2` takes **x first** and is a bisection accurate
  to ~0.01 rad — do not substitute `atan2f`.
- Command byte `0xE1` is FILL RECTANGLE with **integer** corners here.
- **The font is not a file.** The glyph pixels are compiled into `BRD3D.dll`'s
  `.data`, as **IA8** (high nibble intensity, low nibble alpha), one byte per
  texel, in four strips: `0x100946C8` and `0x1009B4C8` (pitch **704**, **40**
  rows) and `0x100A22D0` and `0x100A4170` (pitch **392**, **20** rows). Every
  strip butts exactly against the next object in the image, so the extents are
  arithmetic, not guesswork. `0x10073820` registers them one glyph at a time
  through the backend texture constructor at `0x118AA0AC` with fmt **3** (IA)
  and siz **1** (8b); `0x10018590` draws them; `port/src/br_font.c` reads them.
  Do not go looking on the disc for a font asset — there is not one.
- `0x10019140` is **not a function** despite `config/functions.csv` listing it
  as 254 bytes: it is `0x10018590`'s two jump tables plus their two 0x4A-byte
  index tables, sitting in `.text` after the function ends.
- `config/functions.csv` is a good index, **not ground truth**: it invents entries
  (`0x100331FF`, `0x100334D7`, `0x100312BB` are mid-instruction), misses real ones,
  and 37 of 2,581 extents end mid-flow. If a listing starts mid-instruction, skip
  it and say so.

## Aliased storage: a link-clean bug

Two modules may model the same ORIGINAL address under two different host
symbol names. That links fine -- zero duplicate symbols -- and is still wrong,
because the original had ONE object and the port now has two, drifting apart
after the first write.

Known live instance: 0x117554A0 / 0x117554C8 / 0x117554D8 / 0x117554E0 /
0x11750338 are modelled by slice3_42.h as `BrFxRecord g_BrFx1750338[600]` plus
bare int32 pairs, and by slice6_73 as a 4-cell plane cache. The two views are
arithmetically consistent (600 == 4*150), so neither is wrong about the shape --
but exactly ONE must own the storage and the other must alias into it.

Do not treat "the link is clean" as evidence that shared state is shared. Grep
the address, not the symbol.

**Eight more instances found by auditing the provisional data block, seven of
them resolved.** All were found by grepping ADDRESSES across `port/` and then
looking for a *definition* under each name -- not by looking for duplicate
symbols, of which there were none.

| Address | The two (or three) names | Resolution |
|---|---|---|
| `0x10220D68` | `g_brNet220D68` vs `g_brNetLastFull.f78` | **Was a live bug.** `BrCarState` is 0xA0 and `0x10220D68 - 0x10220CF0 == 0x78` == `f78` == `BR_NET_STAMP`. Modelled apart, `g_brNetLastFull = *pState` never updated the "reference" the crossing test reads, so `BrNetCarStateSend` sent a full packet on *every* call. Now a macro in `slice2_11.h` |
| `0x100C12A0` | `g_ab0C12A0` (slice2_20) vs `g_aBrC12A0` (slice3_45, sized `[1]`) | One object, owned by slice3_45.c. **Size now pinned**: stride 89992 x 16 lands exactly on `0x10220B20`, the next referenced global |
| `0x100AC300` | `g_i0AC300` vs `g_Br0AC300` (slice2_19) | Storage in `br_data.c`; and the image says **1**, not 0. "Non-zero suppresses part 2" -- the shipped build suppresses it and the port did not |
| `0x100AA8B4` | `g_brMode0AA8B4` vs `g_BrCamMode` (slice2_19) | Storage in `br_data.c`, initial value **1** |
| `0x106C2CFC` | `g_f6C2CFC` vs `g_BrAnimDt` (slice2_19 **and** slice3_41 -- three names) | Storage in `br_data.c` |
| `0x106C661C` / `0x106C6624` | `g_i6C661C` vs `g_Br6C661C` | Storage in `br_data.c` |
| `0x10AA26F4` | `g_brAA26F4`+`g_brAA26F5` (two bytes) vs `g_aBrAA26F4[4]` (one dword) | One array, owned by slice5_63.c; the byte names are macros over `[0]`/`[1]` |
| `0x118AA0C4` | `g_pfn18AA0C4(void *)` vs `g_BrGfxSubmitB(uint32_t)` | **NOT resolved.** The two disagree about the ARGUMENT (host pointer vs 32-bit DL address). Needs an adjudication, not a cast |

Also still open, and the reason it is: `0x11750338` / `0x117554A0` cannot be
made one object on this host. `BrFxRecord` is 32 bytes but `BrCollPlane` holds
three `BrVec3 *` and widens under LP64, so the 600-record array cannot carry
both views. The fix is to make `BrCollPlane` store vertex INDICES; until then
`g_pBrCollGrid` stays NULL, which every consumer already guards.

A pattern worth generalising from these: the aliases cluster where one packet
names a global positionally (`g_i0AC300`) and another names it semantically
(`g_BrCamMode`). Neither pass can find the other by grepping its own name, and
both are right about the address.

## Two models of one object, shifted

`BrUiScreen` (slice3_33.h) and `BrUiPage_` (slice6_73.h) describe the SAME
original object and do not start at the same place: `BrUiScreen` begins at
+0x10 and has no `pVtbl`/`pfn04`/`pfn08`; `BrUiPage_` begins at +0x00 and does.

So a pointer written through one and read through the other is off by three
fields, and every read lands in the wrong member. It compiles, it links, it
runs, and it silently produces wrong numbers -- the host harness reported
`cCtl=7` for two builders whose disassembly says 4 and 3, because both landed
on the same wrong offset.

This is worse than the aliased-storage hazard above, because there the two
views at least agreed about shape. Here they disagree about where the object
STARTS.

Rule: before reading a field out of a UI object, check which model the module
that WROTE it uses. If it is not the same model you are reading through, the
value is meaningless -- and the fix is to merge the models, never to trust
whichever number looks more plausible.

## Which binary is the reference: BRGlide, not BRD3D

`BRD3D.dll` is the Direct3D build. `BRGlide.dll` is the Glide build, and Glide
is the intended reference -- it was the mature target when this game shipped.

This was got wrong for a long stretch: `tools/dumpasm.py` defaulted to
`BRD3D.dll`, and agent briefs described that file as "the Glide build". Both are
now corrected, but work done before this point was read off the D3D build.

What that does and does not invalidate, measured rather than assumed
(`tools/crossdiff.py`, `config/shared.csv`):

- **1,712 functions are shared** between the two builds; those are the real
  decompilation target and reading either binary gives the same answer.
- Of the addresses this port references, **~80% are confirmed shared**.
- The rest split two ways, and the distinction matters: ~29 are the statically
  linked CRT, which is genuinely absent from the Glide build because Glide
  imports it from MSVCRT -- not a problem. The remainder are either truly
  D3D-specific or simply missing from `config/functions_glide.csv`, which has
  2,110 entries against the D3D map's 2,581. **A function absent from the Glide
  map is classified `d3d_only` even when it exists in the binary**, so that
  bucket is an upper bound on divergence, not a measurement of it.

Before trusting any renderer-adjacent port, check the address against
`config/shared.csv`. If it is not `shared`, re-derive it from `BRGlide.dll`.

### Measured divergence in the text path

The commit that introduced this section claimed "the glyph strips and offset
tables are byte-identical in both binaries". **The strips part was wrong**, and
wrong in an instructive way: the check matched a 1 KB prefix, and a glyph
strip's leading rows are blank, so it matched blank space anywhere in the file.
Both "large" strips even reported the same Glide address, which should have been
the tell.

Re-checked using each strip's DENSEST row instead of its prefix, down to a
16-byte signature: **zero matches**. The D3D glyph pixels are not in BRGlide at
all.

What IS shared is the metrics. Each of the four offset tables matches exactly
once, on non-blank content:

| table | D3D | Glide |
|---|---|---|
| large digits  | `0x100A6070` | `0x100A5978` |
| large letters | `0x100A60E0` | `0x100A59E8` |
| small digits  | `0x100A6150` | `0x100A5A58` |
| small letters | `0x100A61C0` | `0x100A5AC8` |

And the code genuinely differs:

| | D3D | Glide |
|---|---|---|
| text emitter | `0x10018590`, 2992 B | `0x10015B10`, 1019 B |
| width routine | `0x100193C0`, 207 B | `0x10016980`, 198 B |

A third the size is not a variant of the same function. The width routines are
close enough to be the same logic; the emitters are not.

**Lesson for any future cross-build check: match on the densest part of an
object, not its start, and count the matches. One match on dense content is
evidence; one match on a blank prefix is nothing.**
