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
  until its two radii turned out to be different constants; and an analog stick
  scaled by 1/80 for months where `.rdata` says 1/70.
  - `tools/constcheck.py` now does this over the whole tree, so the check is
    one command and the DENOMINATOR is reportable. It matches an address-
    annotated float declaration three ways (address in the trailing comment,
    address baked into the name, bare literal with an address comment) and
    `--selftest` validates it against four known answers, a negative control
    and three recall cases before it will be believed.
  - Two of its rules were wrong on the way and both are recorded in its
    docstring. Accepting `.text` as constant data made every *instruction*
    annotation "resolve", so 257 of 1274 lines were reported wrong against
    machine code read as IEEE754. And keying the load width on the `f` suffix
    is unsound in BOTH directions here, because this tree deliberately widens
    float constants to double literals to model the 53-bit registers.

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

- **The x87 runs at 53-BIT precision, not 80-bit, so a C `double` is an EXACT
  model of these registers.** `0x10091A48` in `BRD3D.dll`'s `.rdata` is the
  CRT's stored x87 control word and holds **`0x027F`**:

  | field | bits | value | meaning |
  |---|---|---|---|
  | IM DM ZM OM UM PM | 0-5 | `111111` | every exception masked |
  | PC | 8-9 | `10` | **53-bit mantissa** |
  | RC | 10-11 | `00` | round to nearest even |

  It is not merely a constant sitting there. Six sites in the CRT read the
  LIVE control word and compare it against `0x027F` before doing anything —
  `1007E554`, `1007E604`, `1007E6B8`, `1007E774`, `1007FA59`, `10085A7E`, each
  `fnstcw [esp]` / `cmp word ptr [esp], 0x27F` / `fldcw [0x10091A48]` only if
  it differs. So the CRT asserts at run time that the process runs at 53-bit,
  and `fldcw` from that address is the only absolute control-word load in the
  image. `0x027F` is the MSVC default; the x87's own power-on default is
  `0x037F` (64-bit), which this build never installs. BRGlide has no such site
  because it imports the CRT from MSVCRT.DLL rather than linking it, which is
  the same reason ~29 CRT functions are absent from its map — same CRT, same
  default.

  Consequences, because this is the part that keeps being got wrong:
  - An x87 intermediate that is *not* spilled is a `double`, exactly. It is
    not an approximation and there is no "last ulp" difference to waive.
  - Computing such an intermediate in `float` is a REAL defect, not a
    tolerable one, because it rounds where the original does not.
  - **But preserve the spills.** Where the original STORES an intermediate to
    a 32-bit slot, the port must round to `float` there too. The job is to
    model the register width, not to widen everything: widening a value the
    original spills is the same bug pointing the other way.
  - Stated limit: PC controls the significand only. The registers keep a
    15-bit exponent, so `double` and the x87 still diverge on overflow and in
    the denormal range. Nothing in this game's arithmetic goes near either,
    but that is the boundary of the claim.

  **The waivers are not the cause, only where someone noticed.** `slice2_21.c`
  carries about 200 float arithmetic operators in float context, uses `double`
  nowhere at all, and has no precision note of any kind — larger than
  `slice3_44.c` and `slice3_42.c` together, and with nothing to remove. So
  removing a waiver does not remove the defect class; it removes the
  permission slip. The files that never wrote one are not the safe ones.

  **And the spill model may be invisible to the suite that covers it.** When
  `slice3_44.c` and `slice3_42.c` were converted, eight separate spill
  decisions were each reversed in turn and every suite stayed green —
  including `test_collresp`'s slope trace, which was bit-identical under all
  eight, though a one-ulp perturbation of `qDot.f00` does move it. The cause
  was the FIXTURES: on "nice" values the two readings agree exactly, so the
  inputs have to be searched for. Three of the eight turned out to be
  provably equivalent (halving a float is exact; `(float)(1.0/(double)x)` and
  `1.0f/x` agree on all 8,388,608 significands in [1,2)); the other five were
  real gaps and are now pinned by bit-pattern assertions on searched inputs.
  Before believing a numeric transcription is guarded, mutate it — and if it
  survives, find out whether the mutation is equivalent or the fixture is
  blind, because those are opposite conclusions.

  **This had been re-derived, wrongly, at least seven times** — as a file-wide
  waiver in `slice3_44.h`, `slice3_42.c`, `slice1_05.h` and `slice2_16.h`, and
  per-site in `slice1_07.c`, `slice6_73.c` and `slice2_19.c`. Every one said
  80-bit or "64-bit mantissa", called the difference "the last ulp", and three
  called it "unavoidable in portable C". All three claims are false, and the
  cost was that the waivers licensed exactly the defect they were describing.
  The check is one `fldcw` operand. The generalisable part: **a difficulty
  asserted file-wide is never re-examined**, because it reads as a limit of
  the medium rather than as a claim about this program.

- Microcode is **F3DEX**: `G_VTX` has `n` in bits[15:10]. **The rest of this
  entry was wrong and is corrected below** — `v0+n` is NOT in bits[7:1], and
  the `16n-1` layout is NOT absent.
  - Measured in `testdata/bb.rca` and `testdata/ce.rca`: every `G_VTX` is
    `0x04<<24 | (n<<10) | (16n - 1)` with bits[23:16] zero. `0x040079DF`
    is n=30 and 479 = 16*30-1; `0x040081FF` is n=32 and 511; `0x0400207F`
    is n=8 and 127. So the low ten bits ARE the F3D byte-length field.
  - The consumer agrees: the `G_VTX` handler `0x10021A20` (Glide) takes `n`
    from bits[15:10] and the destination index from **byte 2 of w0**
    (`mov cl, byte ptr [ebp-2]`), i.e. bits[23:16]. Nothing reads bits[7:1].
  - Cost of the old claim: `br_f3d.c`'s `end = (w0>>1)&0x7F; if (end > 64)`
    aborts the walk at every `G_VTX` with n >= 9, so `test_f3d` reports 76
    triangles for `bb.rca` where there are **1820**, and 471 for `ce.rca`
    where there are **1079**. Its assertions pass because they test ratios.
- **The PC builds run a software RSP/RDP, not a native 3D path.** One
  interpreter, `0x10023C90` (Glide) / `0x10024A90` (D3D) — the same 29 bytes
  in both — walks host-order F3D commands through a 256-entry table at
  `0x100A9A58` (Glide) / `0x100A79F0` (D3D). **Both builds handle exactly the
  same 28 opcodes**; 18 of the 28 handlers are byte-identical shared code and
  8 diverge (`0x04 0x06 0xB1 0xBF 0xDC 0xE2 0xED 0xF8`). The other 228 slots
  point at one 8-byte "return p+8" stub. The text path in `br_font.c` feeds
  this same interpreter — there is no second one. See `port/include/br_dl.h`.
  - The opcode is byte **3** of the command in memory, because the loader
    (`0x10019040`) byte-swaps the whole list into host order first.
  - Handlers return the NEXT command. `0xE4` is 24 bytes, `0xDC` is `8*w1`,
    `0x06` returns w1, `0xB8` returns the popped address or NULL.
  - `0xF5 0xF3 0xF0 0xFD 0xBB 0xBA` have **no handler** and are skipped at
    draw time. Texture setup happens at load time instead.
  - The combiner is a **closed set**: `0x1001E7A0` is a chain of exact
    equality tests on the `(w0,w1)` pair — ten patterns plus a default.
    Render mode (`0x10021270`) is the same shape, nine values plus a
    bit-tested fallback. Neither is open-ended.
- **The wheel ground probe keys the collision grid on the WORLD point**, like
  its straight-down sibling, and there is no defect there. This entry used to
  say the opposite in bold — that `0x10068070` (D3D `0x1006F0C0`) keys on the
  wheel's BODY-LOCAL mount offset and can therefore never find ground on any
  track — and it is kept, corrected, because two independent passes reached
  that conclusion and the mechanism is a general trap.
  - The claim rested on `[esp+0x18]`/`[esp+0x1C]` appearing twice: once as the
    spill of `f78.x`/`f78.y` at `0x10068091`, once as the reload pushed to the
    acquire at `0x100680DC`. **Same displacement, different ESP.** The reload
    sits between `call 0x1006D9D0` and its `add esp,0xc`, so esp is still 0xC
    lower there and the two displacements name `R-0x30`/`R-0x2C` — the OUTPUT
    of the transform at `0x1006DA20` — not `R-0x24`/`R-0x20`, the mount.
  - Four independent confirmations: all three callees end in a bare `ret`
    (cdecl, so the caller's `add esp,0xc` is real and must be counted); the
    mount slots are REUSED for the hit point at `0x10068206`, so "the same
    slots are reloaded" could not have held anyway; every other read in the
    function agrees with one consistent labelling; and `0x1006F0C0` is
    byte-identical, so the D3D build cannot arbitrate — it has the same code.
  - `g_brPhysWheelGridWorldKey` was the switch that let the "defect" be
    measured. It is now vestigial and changes nothing.
  - **The refutation was available the whole time and was explained away.**
    The claim predicted total failure of wheel ground contact on every track;
    the shipped game plainly works. That was noticed, and answered with "then
    the contact must live in `0x10067C30`'s unported callees" rather than
    treated as evidence against. A reading that requires the shipped game to
    be broken needs *more* evidence than one that does not, and this one had
    less — one displacement match, taken at face value.
  - Generalising: **a stack displacement means nothing without the ESP it is
    relative to.** Before claiming two `[esp+N]` with the same `N` are the same
    slot, walk every push/pop/`sub esp`/`add esp` between them, and check
    whether the callees in between are cdecl or stdcall — that decides where
    the argument cleanup happens, and therefore where esp is.
- The car's rigid body is built by D3D `0x10062C50` / `0x10063000` with
  IMMEDIATES, not table data: chassis mode 1, dim (3.5, 2.0, 1.5), **mass
  1000**; wheels mode 2, mass 0; chassis gravity force `(0,0,-15450.75)` and a
  per-wheel `(0,0,-103.005)`. `-15450.75 == 1575 * 9.81` and
  `-103.005 == 10.5 * 9.81`, so the units are metres/kg/s — but the chassis
  mass is 1000, not 1575, so the chassis sees **15.45 m/s^2**. Spring rate
  (body `+0x1B8`) is `(20 - n*-4) * 16000` with `n` at car `+0xE94`; damper
  rate (body `+0x1BC`) is the immediate `-3000`. Force-list node layout is
  `{next, kind, f[3], r[3]}`, 0x20 bytes, built by `0x100746E0` whose LAST
  argument becomes `kind`.
- **`0x10074B20` / Glide `0x1006DD80` is a 3x3 matrix subtract, not a repeated
  vec3 one**, and `slice3_44.h` currently says the opposite *as a documented
  preserved bug*, which is the shape that survives review. `mov eax, edi` is at
  `0x10074B38`, one instruction **before** the outer loop's jump target
  `0x10074B3A`, so the cursor is never reset and the nested 3x3 loops walk nine
  consecutive floats. The two `sub` instructions inside the outer loop
  recompute the same two base offsets every time, which is what makes it look
  like a reset. Confirmed by its real caller: the collision impulse solver
  `0x10065C80` hands it a 3x3 identity with `1/mass` on the diagonal and a 3x3
  built from three chained `BrMat3Mul`s. Generalising: **a loop's reset
  instruction only resets if it is inside the loop** — check the jump target,
  not the reading order.
- **`0x10008D60` is one byte, `c3`** — a bare `ret`. Every "this only drives a
  printf, so the function is a test" argument in this tree rests on it, and
  `0x10066D70` shows why that inference is unsafe: the return value really does
  only reach the stub, and the function still writes `save.angVel` and calls
  `BrRbQuatDerivative`. **A dead return value says nothing about side effects.**
- The car's collision box (`car+0x340..0x34C` == `body+0x1DC..+0x1E8`) is
  **loaded from the disc, not built by the constructor**. Glide `0x1005BCC0`
  sets it to `(0, 0, 2.0, 0)` at `0x1005BD40/42/48/4E` and `0x10059A80` copies
  a car-data record's `+0x10..+0x1C` over it; `0x10063B80` fills that record
  from `0x10B73668 + 24*(...)`, an address inside `.data`'s **virtual** range
  but far past its `0x41E00` raw bytes. Two of the three extents being zero is
  what makes `0x10067C30`'s reciprocals infinite and the whole OBB collision
  chain inert in this port. See `port/include/br_collresp.h`.
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
- **The bulk game data is NOT in the POD.** The disc's `BossRally.pod` holds one
  entry and is a leftover. Tracks, cars, textures, art and sound are plain files
  in plain directories on the CD (`TRACKS/`, `CARS/`, `CARGFX/`, `IMAGES/`,
  `PAINT/`, `SFX/`), played from the CD; `DATA1.CAB` is 125 KB of setup stub and
  installs nothing of the game. `tools/extract_iso.py --list` walks the real
  ISO 9660 tree: 2111 files, 116 MB.
- `.trk`: a raw **big-endian** N64 memory image with a `0x230`-byte header on the
  front. File offset 0 is N64 `0x80025C00`, so the payload at file `0x230` is
  N64 `0x80025E30` — which every shipped file also stores at header `+0x84`.
  Loader `0x100311C0`; header swap+relocate `0x10031B80`; payload passes
  `0x100314D0`. Do not infer the header layout — `0x10031B80` states it, one
  unrolled reversal per field, and it skips `0x80..0x83` because that field is
  an RGBA quad.
- `guLookAtF` (`0x100309A0`) and `guPerspectiveF` (`0x10030930`, **7 args**) are
  **not** stock libultra. `BrAtan2` takes **x first** and is a bisection accurate
  to ~0.01 rad — do not substitute `atan2f`.
- Command byte `0xE1` is FILL RECTANGLE with **integer** corners here.
- **The font is not a file**, and each build carries its own copy. Do not go
  looking on the disc for a font asset — there is not one.
  - `BRD3D.dll`: **IA8** (high nibble intensity, low nibble alpha), one byte
    per texel, four wide strips — `0x100946C8`/`0x1009B4C8` (pitch **704**,
    **40** rows) and `0x100A22D0`/`0x100A4170` (pitch **392**, **20** rows),
    indexed by column. `0x10073820` registers **106** textures through the
    constructor at `0x118AA0AC` with fmt **3** (IA), siz **1** (8b);
    `0x10018590` draws; `0x100193C0` measures.
  - `BRGlide.dll`: **AI44** — the nibbles are the **other way round** (high
    alpha, low intensity). Two blocks of 54 fixed-stride windows,
    `0x1007B618` stride `0xA00` pitch **64** and `0x1009D218` stride `0x280`
    pitch **32**, indexed by **class**, not column. `0x1006C790` makes just
    **two** textures (64x64 and 32x32, format **4** =
    `GR_TEXFMT_ALPHA_INTENSITY_44`) and `0x10015B10` re-points one of them per
    glyph with an extra `0xDD` before each `0xDC`; `0x10016980` measures.
  - The extents are arithmetic in **both**: every block butts exactly against
    the next object in the image.
  - The two blobs are the **same font**: for all 53 renderable classes in both
    sizes, every Glide texel is the D3D texel **nibble-swapped**, at the
    **same row**. So the bottom-up storage is a property of the font, not a
    D3D artefact. The metrics (class map, both offset tables) and all four
    shading ramps are byte-identical — and the ramps are **nibble-replicated**,
    so they cannot be used as evidence about the format either way.
  - `port/src/br_font.c` reads both and works out which from the image.
- The **only** behavioural divergence between the two emitters is that
  `0x100B8C90` ("detail") is read by the D3D build and **not** by Glide. It is
  the same nine bytes in the width routines, which is why `0x100193C0` is 207
  bytes and `0x10016980` is 198.
- The Glide emitter is `0x10015B10`..`0x100166FA`, **3050 bytes**, then two jump
  tables and their two `0x4A` index tables ending at `0x100167FA` — so it is
  very slightly **bigger** than the D3D one, not a third of its size.
  `config/functions_glide.csv` used to split it at `0x10015F0B`, a jump target
  **mid-flow**; it no longer does (see the rebuild note below). Treat a
  suspiciously small Glide extent as a split, not a measurement.
- `tools/dumpasm.py` **honours** an explicit size argument and prints a NOTE
  when it disagrees with the map. (It used to discard it silently, which is how
  asking for a wrong 1019 bytes and receiving exactly 1019 read as
  confirmation.)
- `0x10019140` is **not a function** despite `config/functions.csv` listing it
  as 254 bytes: it is `0x10018590`'s two jump tables plus their two 0x4A-byte
  index tables, sitting in `.text` after the function ends.
- **Menu navigation runs on the control flag bits at +0x1C, and the "step"
  global is a decoy.** `0x100AB3DC` is added to the selection cursor
  `0x10AA286C` only by the two `+0x10` arms, in `0x10047A60` and `0x10048530`
  — and `0x10` is the DISABLED bit, so a row carrying it cannot be selected in
  the first place. An ordinary menu row (place flags `0x102001`) has neither
  `0x1000` nor `0x10`; it moves because the input handler `0x100603A0`
  **increments or decrements the cursor directly** (`10059613` / `10059628` in
  BRGlide, the only two writers outside the clamp). Wiring the step and nothing
  else leaves every ordinary row inert, which reads as the port being dead
  rather than the seam being wrong.
  The bits: `0x02` activate, `0x08` inert, `0x10` disabled, `0x20` current,
  `0x800` skip, `0x1000` page-frame ordinal arm, `0x2000` record the index in
  the phase's `+0xBC`. slice6_71's `0x1004F700` computes
  `flags = fAutoSave ? 0x102001 : 0x102011` — the clearest single statement of
  what `0x10` means anywhere in the corpus.
- **Map extents are wrong in BOTH directions.** `config/functions.csv` gives
  `0x10047A60` 161 bytes; it is 587, and the Glide map has it right. That is
  the mirror image of the font emitter, where the Glide map was the short one.
  Check the extent against the other build before trusting either.
- The UI style pool `g_aBrUiStyle` starts at **0x100AB418**, not 0x100AB438.
  `0x10047A60` reads `0x100AB418` and `0x100AB428` as hit-test rectangles four
  int32 deep, which pins the lower bound slice3_39.h had flagged as its
  weakest claim. `0x100AB408` is very likely another entry; nothing reads it,
  so it is not in the table.
- The style pool is **not what gets drawn**, and the table immediately above it
  is. `0x100AB568` (Glide `0x100AAD08`) is **145 entries of 24 bytes**,
  `{ int32 image; int32 rect[4]; int32 flags }`, and `0x100AB418 + 21*16 ==
  0x100AB568` so the two abut exactly. A control's background is
  `spriteTable[w1E20C]` blitted at `__ftol(x), __ftol(y)` with the entry's own
  width and height (`0x10047930` -> `0x10058380` -> `0x10001320`, a 16-bit
  software blit, colour-keyed when the entry's `+0x14` has bit 0). Every entry's
  rect starts at (0,0), so the "source rect" is always the whole bitmap. The
  bitmaps are **BMP files on the disc** (`images\...`, loaded by `0x10056260`
  into the stride-8 table at `0x10AC53E8`), so the port has the geometry and
  not the pixels. `port/include/br_uispr.h` owns the table;
  slice3_32.h's `BrScrRectEnt` is the same object typed and left without
  storage, and slice3_39.c's `g_BrSprRect46` / `g_BrSprRect48` are entries 46
  and 48 of it.
- **The menu font is not `br_font.c`'s font.** `br_font.c` recovers the
  DISPLAY-LIST font out of `.data`; the menus draw text by blitting one sprite
  per character out of `images\type_gry|wit|mid|yel.bmp` — sprites **2, 3, 4
  and 0x34** — using the metric tables at `0x100AC6E4` / `0x100ACB5C`.
  `0x1005B2B0` walks the string, `0x1005B730` maps the text box's kind byte
  `+0x2B64` (0/1/2/4) onto those four sheets, and `0x1005B7A0` uses sprite 5,
  `bignums.bmp`, for the large digits. So a **caption's colour is a font sheet,
  not a tint**, and a selected menu row is recoloured rather than given a
  background: `0x10048180`'s not-current tail forces the kind byte to 1 and its
  current arm leaves whatever the screen's `+0x0C` hook set.
- **A caption has no baseline offset at all.** `0x1005B2B0` hands every glyph
  the text box's `+0x414` unchanged as the destination top-left, and
  `0x10047EB0` sets `rcTop = __ftol(ctl->y)` and `rcBottom = rcTop + height`
  from the measure method. The caption's cell top IS the control's y and its
  cell height IS the measured height. The x is the box's `+0x410`, which
  `BrTextBoxCentreX` has already centred in the style rectangle when a2 bit 0
  is set -- which every menu builder passes.
- `config/functions.csv` (the **D3D** map) is a good index, **not ground truth**.
  It is still the output of `tools/funcmap.py`, whose extents run "from one
  start to the next". Measured against flow analysis (`tools/funcmap2.py`,
  `config/functions_d3d_flow.csv`), 2,451 of its 2,632 entries are exactly
  right and **181 are not**: 49 are not functions at all (jump tables such as
  `0x10019140`, `0x1000C074`, `0x100292BC`; and mid-flow labels such as
  `0x100331FF`, `0x100334D7`, `0x100312BB`), 15 are truncated, 95 are over-long
  by a whole following function, and 22 run on into trailing tables or padding.
  If a listing starts mid-instruction, skip it and say so.
- `config/functions_glide.csv` **has been rebuilt** by `tools/funcmap2.py` from
  call/pointer evidence plus flow, and every byte of `.text` is now accounted
  for as code, switch table, padding or data. Prefer it. The same tool's D3D
  output is `config/functions_d3d_flow.csv`; `config/functions.csv` has **not**
  been replaced, so the two D3D maps disagree — see above for by how much.

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
| `0x105D17A4` / `0x105D17B4` / `0x105CE2D0` | `BrDl.prim[0..2]` vs `BrDl.lightOff[3]`, **both in br_dl.h** | **Was a live bug, now resolved.** These three globals are 0xFA's R/G/B destinations (`0x1001EA80`) AND the numlights==0 fallback colour `0x10022AC0` copies at `0x10022BCC`. Modelled apart, `lightOff` had **no writer anywhere in the port**, so every unlit-but-lighting-enabled vertex came out black. `lightOff` is gone; `br_dl_light_vertex` reads `prim` |

Also still open, and the reason it is: `0x11750338` / `0x117554A0` cannot be
made one object on this host. `BrFxRecord` is 32 bytes but `BrCollPlane` holds
three `BrVec3 *` and widens under LP64, so the 600-record array cannot carry
both views. The fix is to make `BrCollPlane` store vertex INDICES; until then
`g_pBrCollGrid` stays NULL, which every consumer already guards.

**...and the second half of that was a wrong diagnosis, which is worth keeping
because of the shape of the error.** "Until then `g_pBrCollGrid` stays NULL"
reads as *the pointer width is what keeps the grid empty*. It is not. The
pointers land in `g_pBrCollVerts`, a genuine host `BrVec3 *` aimed at the track
image, which `br_track.c` has already byte-swapped in place — host-order floats
on a 12-byte stride, i.e. exactly `BrVec3`. A pointer into a host array
survives LP64 perfectly well.

What actually kept it NULL is much duller: **nothing in `port/src` ever DEFINED
it.** Four test files declared their own storage and the host had none, so
`br_phys.c`'s "a NULL grid means no ground" guard was taken on every probe of
every run. `port/src/br_collgrid.c` gives the two globals storage and binds the
five source tables (all five are .TRK header fields: `+0x0C`, `+0x14`, `+0x94`,
`+0x20`, `+0x24`), and the ground probe then reads the track's real triangles.

The ALIAS is still open and this changes nothing about it: `0x11750338` is two
host objects. Making `BrCollPlane` index-based is still the right end state,
because it is what would let that storage *be* `g_BrFx1750338` rather than sit
beside it. It just was not the thing standing between the port and the ground.

Generalising: "X is blocked on Y" is a claim about causation, and the cheap way
to check it is to ask whether anyone has ever tried X. Nobody had.

## 0x10AA2904, and the sweep that found 350 more

`tools/aliasmap.py` does the address-keyed sweep this section had been doing by
hand: it binds host declarations to original addresses through three channels
(an address in the declaration's own trailing comment, an address baked into
the identifier, and a lead comment), gates every candidate on the shipped
images' section table, and reports each address whose declarations come from
more than one OWNER. **350 addresses in this tree have more than one host
model.** Read its docstring before using the list -- it measures 4/5 on the
five known instances and says which one it misses and why.

Three things it got wrong on the way, all worth keeping:

  - Gating on `config/globals.csv` looked obviously right and drove recall to
    3/5. That file is the set of addresses `globals.py` could decode a
    reference to, and it is **missing 0x105D17A4 and 0x1184C088** -- two of the
    five calibration cases. Both misses were "no host object here", the
    dangerous direction. The section table is complete by construction; use it.
  - The declarator regex parsed `float primR;` as type `float prim`, name `R`,
    because a greedy type and an optional-whitespace separator let the type eat
    all but the last letter. It reported the calibration case under three
    nonsense names and still said PASS.
  - Grouping by field NAME reported thirteen "models" of one phase vtable --
    a struct every field of which is commented with the struct's own vtable
    address is annotated, not aliased. An alias is two OWNERS.

The Glide/D3D pairing it needs is DERIVED, not guessed: for a `body`-matched
pair in `shared.csv`, an offset carrying a 32-bit fixup in BOTH relocation
tables holds the same object under two numbers. 2,157 pairs, a clean bijection,
and it independently rediscovers both pairings this file already records --
`0x106C0964 <-> 0x106E79F4` and `0x10AA2904 <-> 0x10AC5C5C`.

**0x10AA2904 is resolved.** It had NINE declarations and SIX with real storage.
Four were `BrPhase_ *` under three type aliases (`BrPhase`, `BrOptObj`) and
they were four separate objects:

| host object | instance | what it did |
|---|---|---|
| `BrUiNav::pAA2904` | `brally.c` `g_nav` | what the frame loop READ |
| `BrPhaseCtx::pAA2904` | `br_wire78.c` `g_phaseBase` | what slice2_26/slice3_31/br_phaseact WROTE |
| `g_brPAA2904` | `slice2_25.c` | what slice2_25/slice4_50/slice5_63 WROTE |
| `BrS71Globals::pAA2904` | `slice6_71.c` `g_brS71` | what 0x10038F30 READ |

`port/include/br_phasecur.h` owns the one slot; `brally.c` binds it to
`g_nav.pAA2904`; the other three reach it as `BR_PHASE_CUR` / `g_brPAA2904`.
No cast is involved -- this is NOT the `BrPhase_`/`BrUiPhase` case, and the
two models that DO need a cast were deliberately left alone (below).

Measured, not asserted. `./build/brally -keys root ".dj"` before: the row's
action ran, `phase=` never moved, and the harness printed which slot each name
resolved to -- three different ones. After: `** PHASE CHANGED`, on every one of
the five rows whose action is ported. Reinstating just the host's one-line bind
reproduces the old behaviour exactly, which is how the causal claim was checked
rather than argued.

**And that mutation SURVIVES `tools/regress.sh`.** 131 suites, 0 failures, with
the split fully reinstated -- because no suite links `port/host`. The unit-level
assertions added here pin the module and both leaf ranges; the host's bind is
pinned only by running the harness. A green suite is not evidence that the
front end is wired.

Still two objects, and deliberately: `BrScrGlobals::pAA2904` is `BrPhaseFull *`
and `BrUiGlobals`/`BrMenuState` model the same dword as a bare `int32_t`.
`BrPhaseFull` is a separate struct with the same layout as `BrPhase_`, and
merging them means merging `BrUiPage`/`BrUiPage_` too. That is the retyping job,
not this one.

A pattern worth generalising from these: the aliases cluster where one packet
names a global positionally (`g_i0AC300`) and another names it semantically
(`g_BrCamMode`). Neither pass can find the other by grepping its own name, and
both are right about the address.

**The prim/lightOff row is the same pattern INSIDE ONE HEADER**, which is the
part worth keeping. Both names are in `br_dl.h`, twelve lines apart, and the
comment on `lightOff` even lists the three addresses -- so the evidence was
never hidden, it was just never compared. The tell that would have caught it
without any comparison at all: **a field with no writer.** `lightOff` was read
in exactly one place and assigned in none, which a grep for the symbol shows in
one line. Before believing a struct models the original's state, check that
every field is written by something.

## Duplicate FUNCTIONS, and why the list of them was mostly wrong

The aliased-storage section above is about one address under two data names.
The same thing happens to CODE, and it is harder to see: two modules
transcribe one original function under the two BUILDS' addresses, so neither
can find the other by grepping its own number.

Nineteen candidates were produced by grouping `config/ported.csv` through
`config/shared.csv`'s Glide/D3D pairing. **Eight of them were not duplicates
at all, and the reason is a property of the pairing data that will keep
producing false positives.**

- **`matched_by = body-dup:N` is not a pairing.** It means the D3D function is
  byte-identical to N BRGlide functions and *the evidence does not say which*.
  crossdiff writes the same `glide_va` on all of them, so N distinct D3D
  functions collide on one number. Five of the nineteen were this — the
  10-byte, 17-byte, 36-byte and 184-byte stubs at `0x10019810` (three-way),
  `0x1001EB10`, `0x1002EC2C`, `0x1003E9B0` and `0x10040900` (five-way). Every
  one is several real, distinct functions.
- **`matched_by = shape` is a similarity, not a match.** `0x10022350` /
  `0x10022AC0` (light one vertex) is classed `shared` on a shape match and the
  two builds genuinely differ: Glide clamps the result to **255.0f**
  (`0x10022B35 mov eax,0x437F0000`) and D3D to **1.0f** (`0x3F800000`),
  because Glide's iterated colour runs 0..255 and D3D's runs 0..1. That row
  should be `renderer`, like the font pair at `0x1006C790` which is already
  classed correctly.
- **A bare address is not self-describing, and neither is a bare grouping.**
  Two of the nineteen — `0x1001CC00` (BrRallyMain vs a fill-colour handler)
  and `0x1001C7A0` (a rect handler vs a car-table remove) — were an artefact
  of grouping on the NUMBER and ignoring the build tag. `shared.csv` has both
  rows and both are right: glide `0x1001CC00` pairs with d3d `0x10079820`,
  and d3d `0x1001CC00` pairs with glide `0x1001E9F0`. The same number names
  two different functions in the two images. This is `manifest.py`'s defect
  (6) reappearing in a different tool.

The audit that found these also checked all 772 manifest claims for a repeated
address, and for a repeated address+build, with `uniq -d`: **none**. So a
duplicate in this tree never looks like two `@implements` lines on one number.
It only ever appears through the pairing step, which is exactly why the
pairing data's weak classes have to be read before the list is believed.

**The eleven that were real, and what each cost.** Three of them DISAGREED,
and in every case the copy that looked better was the wrong one:

| addresses | the disagreement | which was right |
|---|---|---|
| `0x10021570`/`0x10021510` (0xE4) and `0x100219D0`/`0x10021B80` (0xE3) | br_dl.c shifted 0xE4's corners RIGHT by two and left 0xE3's alone; the bytes do the opposite — 0xE4 shifts nothing and 0xE3 shifts LEFT by two | slice2_16.c. br_dl.c's own comment said "the integer form multiplies by four" and the code under it did neither |
| `0x10045520`-family (the phase activate body) | slice2_26.c guarded the two flag stores with `if (pCtx->pAA2904 != NULL)` as a "memory safety" DEVIATION; slice3_31.c stored unconditionally, as the original does | slice3_31.c — the UNGUARDED one |
| `0x10028B50`/`0x10029410` (the texture seam) | br_tex3d.c guarded the plant with `*ppStart != NULL`; the original stores through the global unguarded, and so did slice2_16.c. The guard was also DEAD: the run start is written on the same transition that makes `state` non-zero | slice2_16.c |

Two more were latent rather than live: `swap_u16_run` took an UNSIGNED count
where the original's guard is `test ecx,ecx / jle` (a negative count is a
no-op, not four billion swaps), and `BrHookCallC`/`BrHookSetC` read and wrote
through a `BrHooks *` argument that **the original does not have** —
`0x10034C66` is `mov [0x106C0964],eax`, a plain global store, and the note
elsewhere in this tree explaining the pair as `__thiscall` with the `this`
dropped was describing a struct the game has no trace of.

**A three-way alias fell out of that one.** `0x106C0964` (D3D) is the same
dword as `0x106E79F4` (Glide) — `shared.csv` pairs all three of its accessors
`0x10034C51`/`0x10034C66`/`0x10034C73` with `0x1002E302`/`0x1002E317`/
`0x1002E324` as byte-identical — and the port had it under THREE host names:
`g_pfnStep` (br_gamestep.c), `g_brHook6C0964` (slice4_50.c) and
`BrHooks::pfnC` (slice1_05.c). br_gamestep.c owns it now. Identifying it also
names it: slice2_19.c gates the pad's two extra buttons on
`BrHookIsCurrent(g_BrPadHookFn)`, and `g_BrPadHookFn` is the literal
`0x1002C500`, which `shared.csv` pairs with BRGlide `0x10019A70` — **the race
step**. The test reads "is a race the thing currently running".

**Where the shared bodies live, and why not in either module.** `br_dlshared.c`
(the display-list routines), `br_bits.c` (`BrSwapU16Array`) and
`br_phaseact.c` (the activate sequence) are leaves. That is forced, not
tidiness: slice2_26.c drags in a dozen enter hooks so slice3_31's suite cannot
link it, and slice3_31.c is the same in reverse. A module that only one of the
two can link leaves two bodies however the code is written.

**Two pairs are deliberately still two bodies**, and the reason is recorded at
both sites: `0x10028B50`/`0x10029410` and `0x100293D0`/`0x10029E60` differ in
how they must REACH a display-list command. br_tex3d.c writes words byte-wise
into a possibly-unaligned `uint8_t *` — which the portability rules above
require of a foreign buffer — and slice2_16.c stores through a `BrGfxWords`
overlay. Sharing a body means picking one, and the byte-wise store changes
slice2_16's behaviour on a big-endian host while the overlay puts an unaligned
32-bit store in br_tex3d. The DISAGREEMENT between them is gone; the two
bodies are not.

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

- **~1,700 functions are shared** between the two builds; those are the real
  decompilation target and reading either binary gives the same answer.
- Of the addresses this port references, **~80% are confirmed shared**.
- The rest split two ways, and the distinction matters: ~29 are the statically
  linked CRT, which is genuinely absent from the Glide build because Glide
  imports it from MSVCRT -- not a problem. The remainder are either truly
  D3D-specific or simply missing from the Glide map. **A function absent from
  the Glide map is classified `d3d_only` even when it exists in the binary**,
  so that bucket is an upper bound on divergence, not a measurement of it.

**The exact match count is a property of BOTH maps, not of the binaries.** A
`crossdiff` pair only matches when the two maps agree about the function's
extent, so the number moves when either side is re-derived:

| D3D map | Glide map | matched |
|---|---|---|
| `functions.csv` | old glide map | 1,712 |
| `functions.csv` | **rebuilt** | 1,648 |
| **rebuilt** | old glide map | 1,656 |
| **rebuilt** | **rebuilt** | **1,739** |

The old pair's 1,712 is not evidence of correctness: both maps came from the
same tool, so the same extent bug applied to both binaries produces the same
wrong extent on both sides and the hashes still match. The mixed rows are lower
precisely because one side has been fixed. `config/shared.csv` as shipped is the
second row -- keyed to `config/functions.csv`, which has not been replaced -- so
its `d3d_only` bucket is currently **wider than the truth by roughly 90
functions**. Re-running `crossdiff` with `BR_MAP_D3D=config/functions_d3d_flow.csv`
gives the fourth row.

Glide's smaller function count is not, as was once assumed, a sign of a
deficient map: 2,140 against D3D's 2,818 on 82.7% of the `.text`, and the gap
is the statically linked CRT that Glide does not carry.

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

### ...and then BOTH of those corrections were themselves wrong

The section above is kept as written because the reasoning failure is the point.
Two of its conclusions did not survive contact with the Glide build:

**"The glyph pixels do not exist in BRGlide."** They do. They are the same
pixels **nibble-swapped**: D3D stores IA8 (intensity high, alpha low), Glide
stores AI44 (alpha high, intensity low). Every texel of all 53 renderable
classes matches its D3D counterpart under the swap. Searching for identical
bytes was the wrong test twice over -- first on blank prefixes, then on a format
difference. The right test was to search for the *transformed* bytes.

**"The Glide emitter is 1019 bytes, a third of D3D's 2992."** It is about 3050
bytes -- slightly LARGER. `config/functions_glide.csv` splits it in two at
`0x10015F0B`, which is a jump target in the middle of the function, and
`tools/dumpasm.py` silently ignored its size argument and used the map's extent.
Asking for 1019 bytes and receiving exactly 1019 read as confirmation. It was
the tool agreeing with the map, and the map was wrong.

`tools/dumpasm.py` now honours an explicit size and prints a NOTE when it
disagrees with the map.

**The rule that would have caught both: a measurement that merely agrees with
the thing you already believed is not evidence. Check it a second way, ideally
one that could produce a different answer.**

### What the two builds actually do differently

Exactly two behavioural differences, everything else compared item by item and
identical (all 17 preamble commands, the 16 combine tokens, the epilogue, the
space and pen advances, the tile width, the bounds test, the clamp arm, the
escape grammar, both colour tables):

1. `0x100B8C90` ("detail") is D3D-only -- it forces the small font when > 1.
   Those same nine bytes are the *entire* difference between the two width
   routines (207 vs 198), which is an independent second sighting of the same
   fact.
2. Glyph binding: D3D pre-registers 106 textures and emits one `0xDC`. Glide
   registers two (one per size) and emits a `0xDD` carrying the window address
   before each `0xDC` -- one texture, re-aimed.

Glide glyph windows are indexed by CLASS, not column: large at `0x1007B618`
stride `0xA00`, small at `0x1009D218` stride `0x280`. Both extents pin
arithmetically against the class map.

**All three quirks br_font.c documents hold in BOTH builds** -- the
one-column-wider tile, the +1 per space the width routine does not add, and the
clamp-at-zero-only that is not a scissor. The bottom-up storage claim is now
*stronger* than when it was made: two independently laid-out blobs with
independently pinned extents agree that row 0 is the bottom.


## The function maps, and how the sweep-derived one lied

`config/functions.csv` (D3D) and `config/functions_glide.csv` are now derived by
**recursive descent with flow-determined extents** (`tools/funcmap2.py`). The
previous sweep-derived D3D map is kept as `config/functions_d3d_sweep.csv` so
the two can be compared.

The old generator seeded entry points by **scanning `.text` linearly for `0xE8`
bytes**. x86 is not self-synchronising, so most `0xE8` bytes are operand bytes,
not call opcodes. That produced phantom functions, and one of them cost a
published false conclusion:

    0x100239FD   c1 e8 08     shr eax, 8

The `0xE8` there is the shift's ModRM byte. Read as a call, it invented an entry
at `0x10015F0B` — which is mid-flow inside the Glide text emitter — and split
that one 3050-byte function into "1019 + 2287". Asking the disassembler for 1019
bytes returned exactly 1019, which read as confirmation.

Two further faults: relocated dwords inside `.text` were promoted to entries
(most are SEH handler immediates and `lea`/`mov` displacements, not pointers),
and every extent was set to "bytes until the next start" — making an extent a
claim about the *next* function rather than about this one.

**Scale of the damage, measured:** the README said 37 of 2,581 D3D extents ended
mid-flow. The real defect set is **181 of 2,632**, and truncation is the
*smallest* mode: 49 entries are not functions at all, 15 truncate, 22 run into
tables or padding, and **95 are over-long by a whole following function**. The
dominant failure was swallowing, not truncating — which is worse, because a
swallowed function looks like a large one rather than a broken one.

**Why the old `shared.csv` was not evidence of anything.** A cross-build hash
match requires *both* maps to agree about the extent. The same bug applied to
both binaries produces the same wrong extent on both sides, so it matched
anyway:

| D3D map | Glide map | matched |
|---|---|---|
| sweep | sweep | 1712 |
| sweep | flow  | 1648 |
| flow  | sweep | 1656 |
| flow  | flow  | **1739** |

Both-rebuilt is now shipped. `.text` is 100% accounted for in both binaries as
code, switch table, padding or data, with no undecodable bytes inside any
function.

## Renderer slots: one dispatch slot, two implementations, and only one ported

`config/shared.csv` has a class `renderer` with `matched_by = slot`. It means
the two builds put **genuinely different code** behind the same dispatch slot
-- crossdiff paired them by their aligned CALL SITES, not by their bodies.
These are not "the port picked the wrong constant". There are two
implementations and the tree contains one.

Four are known, all D3D-side, and the numbers are worth having in one place:

| slot | D3D | Glide | similarity | state |
|---|---|---|---|---|
| rectangle filler | `0x1001BE90` 1934 B / 534 ins | `0x1001E380` 914 B / 228 ins | 0.118 | D3D only |
| render mode | `0x10020FA0` 1392 B / 418 ins | `0x10021270` 766 B / 241 ins | 0.112 | D3D only |
| textured rect | `0x10021560` 1567 B / 396 ins | `0x100215C0` 1032 B / 239 ins | 0.306 | D3D only |
| font registration | `0x10073820` 291 B / 102 ins | `0x1006C790` 105 B / 33 ins | -- | **both**, `br_font.c` |

**They are different because the Glide build talks to `glide2x.dll` and the
D3D build does not.** This is checkable rather than inferred: the three
un-ported Glide bodies' calls resolve through the IAT to `_grDrawTriangle`,
`_grClipWindow`, `_grAlphaCombine`, `_grAlphaBlendFunction`,
`_grAlphaTestFunction`, `_grAlphaTestReferenceValue`, `_grCullMode`,
`_grDepthMask` and `_grDepthBufferFunction`. The D3D bodies build
`BrD3DTLVertex` records with `rhw` and `specular` and go through a device.
There is no constant to swap.

**DO NOT "fix" one of these by transcribing the Glide body over the D3D one.**
That silently swaps which renderer the tree implements and destroys work. The
D3D bodies are correct transcriptions of real functions; their `d3d` tags are
accurate, and a census of `d3d`-tagged claims surfacing them is the tag doing
its job, not a defect report.

### What the honest options are, and when each applies

`br_font.c` is the worked example of the good end state: **one file, both
bodies, both claimed, an explicit selector.** `BrFontRegisterPages`
(`0x1006C790`, glide) and `BrFontRegisterGlyphs` (`0x10073820`, d3d) sit
fifteen lines apart, and `BrTextEmitString` / `BrFontMeasure` go further and
share ONE body with an `fGlide` flag where the divergence is small enough.

That works there for a reason that does not generalise: **the font blob
discriminates.** `BrFontLoad` decides `pFont->build` by looking at the image
it was handed, so both arms are reachable and both get exercised. There is no
project-wide "which renderer am I" flag, and inventing one for the three
slots above would give every Glide arm zero coverage forever -- which
CONVENTIONS.md already names as its own hazard ("a path that has never
executed is not tested"). A seam whose second arm cannot run is not a seam,
it is 3,000 bytes of unexercised transcription wearing one.

So the rule is:

- **Seam it** when the divergence is small and something in the DATA already
  says which build is in play. `br_font.c`'s `fGlide` and `pFont->build`.
- **Label it** when the two bodies are genuinely different code and nothing
  selects between them yet. State the slot, both addresses, both sizes, the
  similarity, which body is present and which is not, at the site.

**An accurate label plus a stated gap beats a hasty seam.** The thing that
must never be left is the third option -- a body that implements one build
under a comment describing the other, which is how the nine divergences this
section came out of survived a census.

## Grep BOTH builds' addresses before deciding something is unported

Four of the seven clipper planes, the attribute interpolator and the whole
64-node vertex pool were already ported in `port/src/slice1_03.c`. A pass sent
to port the clipper grepped the **Glide** addresses, found nothing, and wrote a
working duplicate — then discovered the existing code and threw its own away.

The reason the grep missed: `slice1_03.c` records the **D3D** addresses
(`0x1001D810` and friends), because that is what the reference binary was when
it was written. The Glide addresses (`0x1001F0D0` &c.) appear nowhere in the
tree. Same functions, different numbers, and no textual overlap at all.

`config/shared.csv` pairs them — that is what it is for. Before concluding an
address is unported, look it up there and grep the paired address too.

This is the same failure the project has now hit roughly thirty times in
another guise ("~27 functions already existed under another name"), with a new
twist: here the other name was the same name, under a different **build's**
address.

## A path that has never executed is not tested

The clipper had zero coverage for the whole life of this port, and every suite
was green, because both retail test models fit entirely inside the test
frustum. `clip=0` on every run read as "no clipping needed", not as "this code
has never run".

The fix was not a new assertion, it was a new **camera**. Driving the same
display lists with the model panned right takes `clip` from 0 to 306; a close-up
that crosses NEAR takes it to 512.

When a subsystem reports zero work done, check whether it is idle or unreachable
before believing the green.

## A "function pointer" that is really a dispatch-table slot

`br_dl.h` described `0x100A9A68` as "the triangle-drawing function pointer",
because the one writer anyone had read swapped two triangle routines through
it. It is dispatch-table slot **0x04 — G_VTX** (`0x100A9A58 + 4*4`), and
`0x1001FD70` rewrites slots 0x04, 0xB1 and 0xBF together from the geometry
mode.

So `G_LIGHTING` does not select a lighting *pass*. It selects a **second vertex
transform**, installed over the first. The static table image holds the unlit
one, which is why every reader found the unlit one and concluded lighting was
never applied.

The general shape is worth remembering: when a table slot is written at
runtime, the image tells you the DEFAULT, not the behaviour. Find the writers
before concluding what a slot holds.

## Measure the data before believing a hypothesis about it

"The file already holds lit colours" was a live and plausible explanation for
flat-looking geometry. It is false, and one measurement killed it: over every
vertex the two retail models reach -- 864 in one, 931 in the other -- the
magnitude of a Vtx's trailing three signed bytes is **127.0 +/- 0.6, every
single one**. They are unit normals. What the port had been rendering was
normals painted as colour.

That took one loop over data already in hand, and it settled a question that
had been open across three passes.

## A misreading dressed as a preserved bug is the durable kind

`0x10074B20` was documented as "three subtractions performed three times --
PRESERVED BUG: the outer loop resets the cursor", with a test case labelled
"the visible face of the preserved bug" asserting exactly that. Both were
wrong. It is a 3x3 matrix subtract: nine floats, each once.

The reset `mov eax, edi` sits at `0x1006DD98`; the outer loop's target is
`0x1006DD9A`, **one instruction later**, so the reset never runs as part of the
loop. And `0x10065C80` passes a 3x3 identity scaled by `1/mass`, which is
meaningless to a routine that touches three elements.

The failure shape is worth naming, because it beats review:

  1. a wrong reading explains an oddity, so it feels like insight;
  2. calling it a PRESERVED BUG makes it look like diligence rather than a
     claim, so nobody re-derives it;
  3. a test is written to pin it, and passes -- which converts the misreading
     into a regression guard defending itself.

**A claim that the original is WRONG must be held to a higher standard than a
claim that it is right, not a lower one.** The shipped game worked. Same rule
as the wheel-probe "defect", and this is its second appearance.

When preserving a bug, record the instruction addresses that establish it, so
the next reader can check the claim instead of inheriting it.

## Source precedence: Glide, then D3D, then N64 — and the N64 never wins a tie

Three binaries can answer a question about this game. They are NOT equal, and
the order matters whenever they disagree:

  1. **orig/BRGlide.dll** — THE reference. Glide was the mature target when the
     PC game shipped. Anything renderer-adjacent must come from here.
  2. **orig/BRD3D.dll** — the same 1999 game, other backend. Authoritative for
     the 1,809 functions classed `shared`, since either build gives the same
     answer there. For the rest it is the wrong renderer.
  3. **Top Gear Rally (N64, 1997)** — a DIFFERENT, EARLIER game by the same
     studio. Useful and non-authoritative.

**The PC game is the later and more developed product.** Where the two titles
differ, assume the PC build is the improvement and port the PC behaviour. The
N64 is two years of development behind it; a difference is far more likely to
be something they fixed than something they lost.

So the N64 ROM is for:
  - NAMING. Its debug strings survived ("Triangle Edge to CubeFace", "Stand
    Dist", "Standing on it's F'in Nose damnit"), and the PC builds' did not.
    A name tells you what a PC function is FOR.
  - STRUCTURE. If a PC block does not decompose the way the N64's sibling does,
    that is a signal you may be mis-carving it — a prompt to re-read, not a
    correction.
  - EXISTENCE. It can suggest that some behaviour ought to be present, which is
    worth searching the PC bytes for.

It is NOT for:
  - deciding what the PC code does. That must come out of BRGlide.dll.
  - supplying a constant, a formula, a threshold or a layout.
  - overriding a PC reading you have evidence for.

Overriding a PC reading with an N64 one needs a STRONG, STATED reason -- for
example the PC bytes being genuinely ambiguous while the N64's are not, and
even then the PC bytes must be consistent with what you adopt. Record the
reason at the site.

### A concrete lead from the N64 cross-read, and why the "4 vs 1" was a non-issue

The sibling analysis reported the N64 contact solve running ONCE per car per
frame, against this port's four-per-frame, and correctly warned that this is
not revision evidence. It is not, and the reason is worth recording because it
is a general trap when comparing two builds:

**The two counts iterate different axes.** `BrCarPhysAdvance`'s loop is
`t -= BR_CP_SUBSTEP` until the frame's time is consumed -- it subdivides TIME.
The N64's single call iterates candidate CONTACT FEATURES internally, with all
four of its case strings inside one loop. Neither number is the other's
denominator, so comparing them says nothing at all.

STRONGER, after the sibling re-checked its own claim one level up: there is NO
fixed-timestep accumulator ANYWHERE in the N64 chain -- not in the solve, not
in the entity loop, not in the race tick, and the main loop above it is a bare
spin with no gate. So it is not "we substep and they iterate a different axis",
it is "we substep and they do not". The PC build has an integration the earlier
title lacks entirely, which is exactly what the precedence rule predicts and
this time it is supported rather than merely un-contradicted.

Their stated limit, kept because it bounds the claim: they have not located
where the N64 blocks for frame pacing, so nobody can say its entity loop runs
at a fixed rate -- only that nothing subdivides it.

Worth noting HOW that arrived: the sibling had verified only the innermost
enclosing loop, realised this port was about to build on the unchecked step
above it, went and checked, and reported back that its own earlier claim was
understated. That is the behaviour that makes a cross-read worth having.

**The adoptable part was a SHAPE claim, and it points somewhere specific.** The
N64's nose guard is the PARENT of both the resting solve and the case solve --
it calls them, it does not sit beside them. So a corresponding PC guard would
be one level ABOVE the callees carved out of the position pass, not among them.

One level above 0x10067C30 is 0x1005A7A0 (ported in full), and one above that
is **0x1006F170 (Glide) / 0x10075F10 (D3D), 1,295 bytes, UNPORTED** -- named in
br_racestep.c as part of the car's control chain and never transcribed. That is
the first place to look for a pitch clamp, and it is a cheap check.

The second shape claim is a carving test rather than a lead: the N64's resting
solve is straight-line with no loops. A PC candidate for the same role that
contains a loop has probably swallowed a neighbour.

## A DETECTOR YOU HAVE NOT VALIDATED IS NOT EVIDENCE, AND IT FAILS TOWARD "MISSING"

Twice in one session this project dispatched agents to port functions the tree
already had, both times off a hand-rolled grep written minutes earlier and
never checked against a known answer.

  - "Which hook slots are installed?" scanned only slice8_84.c and slice8_85.c.
    Reported 48 of 108 filled, 60 NULL. Truth: 99 and 9. slice7_81.c,
    slice2_24.c, slice7_80.c and the host install hooks too.
  - "Is this address already ported?" matched a declaration ending
    `/* 0xADDR */` or a definition under a banner comment. Modules that name a
    function AFTER its address -- BrUiHook81_100450F0, BrMenuCap0730,
    BrOptToggle2F7C_C -- were invisible, so weeks-old ports read as missing.

One agent found 8 of its 10 addresses already ported AND wired; another found
6 of 8. The cost is not just wasted runs: slice7_81.c owns the storage for
thirteen globals, and a second leave routine would have cleared the wrong word.

Three rules fall out.

FIRST: these detectors fail toward "missing", and that is the dangerous
direction. A false "already ported" gets caught the moment someone looks for
the function. A false "missing" ends with a duplicate that links, runs, and
quietly fights the original for the same storage. When a scan reports work to
do, that is the answer to distrust.

SECOND: validate against a known answer BEFORE dispatching. Both scans would
have died instantly on one spot check -- grep a function you know exists and
confirm the detector sees it. That check costs one command. Not running it
cost two agents most of a run each.

THIRD: never encode one naming convention. tools/hookaudit.py now looks three
ways (annotated declaration, banner-comment definition, address-in-name) and
matches BOTH `->pXXXX =` and `.pXXXX =`, because the host writes its slots with
a dot and the first version only understood arrows.

AND: do not assume every hook is an action. +0x08 is the ACTION, called when
the ACTIVATE bit is set; +0x04 is the per-frame caption setter. A brief calling
a pfn04 caption setter an "action hook" sends an agent hunting a screen
transition that never existed. That was also done here. hookaudit.py prints
which slot each entry feeds, so the brief cannot get it wrong silently.

## ACCURACY FIRST. PLAYABILITY IS A CONSEQUENCE, NOT A TARGET

The model is MAME, not ZSNES. Correctness of the decompilation is the point;
being able to play the result is what falls out of getting it right.

This project spent a long stretch doing the opposite and the symptoms were
consistent, so they are worth naming as smells:

  - Standing in for an unported function so something visible happens. Four
    phase-enter placeholders are why "the menu will not navigate" was
    investigated for weeks as a bug in the menu, when the menu was fine and
    its destinations were fabrications.
  - Quoting "it builds", "N suites pass" or "16 of 16 builders run" as
    progress. Those mean nothing crashed, and that our own tests agree with our
    own code.
  - Chasing a visible symptom by wiring around a gap instead of decompiling the
    function the gap is made of. THE ENTRY POINT WENT UNREAD FOR WEEKS while
    the host hand-wrote a substitute for it, and every "nothing builds the root
    menu" note was that one absence seen from a different angle.

A FRONTIER IS NOT A PLACEHOLDER. A declared, counted, do-nothing callee at the
edge of the port is honest: it does nothing, says so, and a run reports what it
hit. A stand-in that returns a plausible value is a lie with a counter on it.
The test: does anything downstream behave as though the function worked? If
yes, it is a placeholder, and it will be mistaken for progress.

## A TEST THAT CANNOT FAIL IS WORSE THAN NO TEST -- SO MUTATE IT

Three found in this tree in three days:

  - a suite asserting RATIOS instead of counts passed while 96% of track
    geometry was being dropped.
  - a fixture left the deciding row zeroed, so it passed under both the right
    and the wrong reading of the argument.
  - the boot state machine's suite could not tell whether the continue flag was
    read BEFORE or AFTER the frame ran, because nothing could change the flag
    mid-call.

Writing the mutation is the only way to know. For every assertion that matters:
reinstate the bug it guards, confirm it FAILS, restore. Report the table.

BOUND ANY TEST THAT DRIVES A LOOP WHOSE TERMINATION IS THE PROPERTY UNDER TEST.
Inverting the main loop's suspend gate did not produce a failure, it produced a
HANG -- because the mutation reproduces the real defect faithfully and the real
defect IS a hang. A suite that hangs reports nothing and blocks the run.

And the bound must leave by a path the mutation cannot block. Two attempts
failed first: returning "queue empty" routed straight into the non-terminating
frame path, and setting the quit flag did nothing because the flag is only read
if the frame RUNS. Only injecting WM_QUIT worked -- the one exit that depends
on neither of the two things under test.

## VALIDATE A DETECTOR AGAINST A KNOWN ANSWER BEFORE YOU TRUST IT

Every scan written in this project has been wrong on first use, always in the
same direction -- under-reporting what the tree already has:

  - "which hook slots are installed" looked in two modules of the five that
    install hooks. Reported 48 of 108; the truth was 99.
  - "is this address ported" knew one of the three naming conventions here, so
    weeks-old ports read as missing. Two agents were briefed to re-port
    functions that already existed.
  - `grep -rl 0xADDR port/` counts a MENTION in a comment as a port. It is what
    produced "nine of eleven boot callees absent" when the answer is eleven.
  - tools/isported.py itself was wrong FOUR TIMES on one address before it was
    right: a `[^*]*` that cannot cross a line; a `.*?` that ran from a mention
    through a banner's end and attached the address to the next function; a
    greedy prefix that captured a different address from later on the same
    line; and counting this project's own frontier stubs as ports.

All four were caught by checking against an address whose answer was already
known. That step costs one command and is the only reason the tool is usable.

FAILING TOWARD "MISSING" IS THE DANGEROUS DIRECTION. A false "already ported"
is caught the moment someone looks for the function. A false "missing" ends as
a duplicate that links, runs, and quietly fights the original for the same
storage -- slice7_81.c owns thirteen globals, and a second leave routine would
have cleared the wrong word.

## STATE THE DENOMINATOR BEFORE STATING A PERCENTAGE

Three different coverage figures were reported here -- 65%, 36%, 26% -- each
computed before deciding what was being divided. 65% counted comment mentions.
36% used only the DLL's shared code. 26% included a 97 KB CD-autorun installer
that is not the game at all. The disc ships four PE images and only BRGlide.dll
is the game; that was not established until the executables were mapped.

A percentage without its divisor is not a measurement. And "ported" is a
ceiling on code TRANSCRIBED, never a floor on code CORRECT: this tree has 42
call sites reaching a placeholder or stub and 43 hole annotations, and no audit
of behavioural equivalence against the original has been done.

## EVERY COMPLETED FUNCTION GETS A PLAIN-ENGLISH DESCRIPTION

This is a preservation project, so the source has two audiences: someone
checking the transcription against the original, and someone trying to
understand how the game worked. The instruction traces serve the first and are
useless to the second.

So every transcribed function carries, above its `@implements` line, a short
paragraph in the form:

    /* WHAT IT DOES: one tick of the running game. Counts the frame, does the
     * frame's work, and reports whether the game should keep going. This is
     * the state the game sits in for as long as it is being played. */

RULES:
  - ONE short paragraph. If it needs two, the function probably needs naming
    better or splitting.
  - Say what it does FOR THE GAME, not what the instructions do. "Decides what
    resolution the game runs at" beats "writes 0x280/0x1E0 into six globals".
  - No addresses, no register names, no opcode numbers. Those belong in the
    technical banner underneath, which stays exactly as it is -- this does not
    replace the evidence, it sits above it.
  - Plain words. A reader who has never opened a disassembler should follow it.
  - If the function does something surprising, say so in a clause rather than a
    second paragraph: "...which also resets the sound, because that goes with
    the device."
  - Do not invent purpose. If what a function is FOR is genuinely unknown,
    write what it observably does and say the purpose is unclear. A confident
    guess in plain English is harder to catch than a confident guess in hex.

## A `void` RETURN TYPE IS WHERE A BEHAVIOUR GOES TO DIE

Forty-three routines in `slice3_31.c` -- everything the two `BR31_LEAVE`
macros generate, plus the hand-written leaves and the three gotos -- were
declared `void`. Every one of them ends `xor eax, eax`, and the header said
so, then explained the `void` with "no caller anywhere looks at the value".

The caller looks at the value.

    10048280  ff5608   call dword ptr [esi + 8]
    10048286  85c0     test eax, eax
    10048288  7508     jne  0x10048292      ; zero -> return 0 immediately

`0x10048180` dispatches the +0x08 ACTION hook and a zero makes it return 0 and
skip the rest of the row's frame -- the `[0x10AA33E4] = 0` store, the
`flags &= ~2` clear, the child loop and the vtable +0x08 draw. Since all
forty-three return 0, that early exit is what the shipped game does on every
menu action, and the port could not express any of it.

Three things kept it hidden, and each is the generalisable part:

  - **The bodies were right and the TYPE threw the answer away.** Reviewing
    the transcription of any one function finds nothing wrong with it. The
    defect lives in `BrPhaseHookFn_`, one line in `br_phase.h`, and in the two
    macros -- none of which is "a function".
  - **A macro spreads one decision over a whole family.** One `void` in
    `BR31_LEAVE` is thirty-one lost returns, and grepping for the symptom
    finds a macro invocation list, not thirty-one mistakes.
  - **The caller's arm had never executed.** `test_slice3_32.c` exercised
    +0x04's `-1`/`-2` sentinels and the +0x0C arm and never once installed a
    +0x08 hook, so `if (r == 0) return 0;` -- which was correct all along --
    had no coverage. `0x10048180` had been audited EQUIVALENT in the same
    round that flagged the leaves.

Rule: before writing `void` on a transcription of a function that ends by
setting eax, find the call site and check whether anything TESTS it. And when
a hook slot's host type is `void`, that is a claim about every function ever
stored in it, not about the one in front of you.

## A DOCUMENTED DEVIATION'S REASON IS A CLAIM, AND NOBODY RE-CHECKS IT

`slice3_39.h` transcribed 98 of `BrCharMapLookup`'s 784 records and explained
the other 686 as "string literals and pointers" whose codes would give "a
garbage character back". It passed review because it was a documented
deviation with a stated reason -- **which is worse than an undocumented one,
because it had been checked and cleared.**

Decoding the region killed both halves. It is not all strings: past the string
pool sit two ten-entry intensity ramps and a block of 0x20-byte descriptor
records, and the eight-byte record framing cuts them into perfectly ordinary
`(code, ch)` pairs. And nothing is garbage: these are fixed bytes in the
shipped image, returned deterministically, **identical in both builds** over
the whole 0..0xFF domain -- checked code by code, zero disagreements.

Fifteen of those accidental pairs carry a code the caller can deliver. The
loudest is `0x09`: **pressing Tab appends a character in the original and
appended nothing in this port.**

Two rules fall out.

FIRST: when a deviation says "the rest is garbage/unreachable/irrelevant",
that is the sentence to test, and the test is usually cheap -- here it was one
loop over bytes already on disk. The deviation's *existence* being documented
is not evidence; only its *reason* is, and a reason nobody re-derived is a
guess with a comment on it.

SECOND: state unreachability with the INSTRUCTION that enforces it. Code 0 is
genuinely unreachable -- `1005B6AB cmp eax, ebp / je 0x1005B71C` filters it
before the lookup -- and code 8 is diverted by `1005B6AF cmp eax, 8 / jne`.
That is checkable. "The remaining records would give garbage" was not.

The matching test defect: `test_slice3_39.c` asserted
`BrCharMapLookup(0x00) == 0`, which is the PORT's answer; the original returns
`0x54` ('T', out of the string "Time"). Its four neighbouring assertions were
correct, which is what made the wrong one look verified. **An assertion sitting
in a block of correct ones inherits their credibility and none of their
evidence.**
