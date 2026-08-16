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
