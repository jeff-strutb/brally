# Top Gear Rally (N64, 1997) — menu system analysis

Analysed 2026-08-16 against `Top Gear Rally (USA).z64`
(8 MB, `NGRE`, CIC-NUS-6102, IPL3 crc32 `90BB6CB5`).
Tooling: `tools/n64rom.py`.

Same studio, same lineage as the PC game: Boss Game Studios wrote both, and the
strings prove shared source — N64 `ERROR: Track header size mismatch! (%d!=%d)`
vs PC `Error: Track header size mismatch(%d != %d)`; both emit **F3DEX** display
lists (the ROM carries `RSP Gfx ucode F3DEX.NoN 1.21`); the PC disc ships N64
`.ci4`/`.lut4` textures verbatim.

## 1. ROM layout

**One resident segment. No overlays.** The entry stub at `0x80200000` zeroes
`0x802AC400 + 0xD67B0`, sets `sp = 0x803168D0`, and jumps to `0x8021E5C4`.

| | rom | vram | size |
|---|---|---|---|
| `.text` | `0x001000`–`0x070AB0` | `0x80200000` | 446 KB, **883 functions** |
| `.data`/`.rodata` | `0x070AB0`–`0x0AD400` | `0x8026FAB0` | 247 KB |
| `.bss` | — | `0x802AC400` | 858 KB |
| assets | `0x0AD400`–`0x7DF75A` | — | 164 zlib records |

Checked every decompressed asset record for MIPS prologues and `jr $ra`: **zero
code in any of them.** All 883 functions are in the one segment.

## 2. Compiler

**SGI IDO (`cc`), not GCC.** Four independent signatures:

1. Loads hoisted *above* the stack adjustment (`0x802111E0` starts
   `lui $t6 / lw $t6 / addiu $sp,$sp,-0x38`). GCC never schedules across the
   prologue.
2. Frame layout is `[outgoing args][saved regs][locals]` — saved regs low
   (`ra` at `sp+0x54` in a `0x100`-byte frame), which is IDO. GCC puts them at
   the top.
3. **951 branch-likely instructions** (`beql`/`bnel`/`bnezl`/…, ~1.1% of the
   88k decoded). GCC 2.7.x for MIPS does not emit these.
4. Float constants materialised through a GPR (`lui $at,0x41a0; mtc1 $at,$f4`
   for `20.0f`) instead of a `.rodata` load.

5.3 vs 7.1 has to be pinned by matching, as in every N64 decomp. 1997 + F3DEX
1.21 points at IDO 5.3 / SDK 2.0 as the first thing to try.

libultra is linked in normally (`bcopy`/`bzero` with MIPS III `ld`/`sd` at
`0x80256270`, `osPiStartDma`, message queues) — those get matched from published
SDK sources rather than reversed. zlib 1.0.4 likewise (`0x8023E000`–`0x80242000`,
22 functions, error strings intact).

## 3. How the menu system works

Three layers. All three are small and all three are data-driven.

### 3.1 `MenuItem` — 20 bytes, static, in `.data`

```c
typedef struct {
    const char *label;   /* +0x00 rodata string, may carry %XY colour codes */
    u32   flags;         /* +0x04 */
    void *icon;          /* +0x08 runtime: decompressed icon, 0 in ROM */
    u32   iconRomStart;  /* +0x0C key for the load + for de-duplication */
    u32   iconRomEnd;    /* +0x10 */
} MenuItem;
```

A screen is a **NULL-terminated array of `MenuItem*`**. Every screen in the game
is one of these tables sitting in `.data`; 52 items across 20 tables were
recovered by scanning for the shape. Examples:

| table | items |
|---|---|
| `0x80272360` | Championship, Arcade, Time Attack, Practice, Paint Shop, Load/Save, Options |
| `0x802723B8` | One Player, Two Players |
| `0x802723DC` | BGM, SFX, Units, Controller Configuration, Save/Load Configuration, Credits |
| `0x80272240` | Sunny, Fog, Rain, Snow, Night |
| `0x802725F4` | TYPE A/B/C/D, STEERING WHEEL |
| `0x80272078` … `0x80272178` | car setup: Type 1-3, Manual/Automatic, Slippy/Normal/Grippy, Softer/Normal/Harder |

Track records are bigger structs (`0x17C` stride, `0x80270854`+) that **embed a
`MenuItem` as their first 20 bytes** — Desert, Mountain, Coastline, Strip Mine,
Jungle and the five Mirror variants.

Observed `flags`: `0x02` on actions that commit (Save/Load/Reset), `0x08` on
Two Players (needs a second pad), `0x10` on every Mirror track, `0x43` on the
two volume sliders, `0x03` on Units. Bit meanings not yet pinned.

Each item owns an icon stored as its own zlib record. All the menu icons live in
one contiguous ROM run, **`0x1F1E70`–`0x211C10`** (~130 KB compressed). Several
already came out in `extracted/n64/menu_art/` — this analysis now supplies their
names: `1E48D0`=Type 1, `1E5790`=Type 2, `1E6650`=Type 3, `1E7500`=Manual,
`1EA220`=Normal tyres, `1EC960`=Softer, `201270`=Units.

### 3.2 `0x8020AD5C` — the generic menu screen

One 5,408-byte **blocking modal loop**. Callers hand it a screen and get back a
result code; it owns the frame loop, the input polling and the drawing in
between.

```c
int Menu(const char *title,      /* a0 */
         int nItems,             /* a1 */
         MenuItem **items,       /* a2 */
         int *selected,          /* a3  in/out */
         int, int, int, int,     /* stack: 0,0,0,0 */
         int r, int g, int b);   /* stack: 0x40,0x40,0x40 */
/* -> 1 = item chosen, 2 = backed out, 5 = ... */
```

Seven call sites. The main menu (`0x802111E0`) is the model for all of them:

```
0x8020C408(items, &nItems)     load + de-dup every item icon, count items
0x80271FB0 = &bgDescriptor     background image + panel geometry
0x80271FB4..0x80271FC4 = 25, 14, 280, 32, 20.0f     layout
v0 = Menu("TOP GEAR RALLY", nItems, items, &sel, ...)
if (v0 == 1) goto jumptable[sel]      /* 7 entries at 0x802A7FD0 */
```

The dispatch is a plain bounds-checked jump table in `.rodata` immediately after
the label strings. Item loading (`0x8020C27C`) walks the array, and when two
items share an `iconRomStart` it reuses the already-decompressed texture instead
of loading it twice.

Screens: main menu `0x802111E0`, 1P/2P `0x80210FC8`, options `0x80211A3C`,
pad config `0x802170C8`, season/load `0x80209434`, track select `0x80211D70`,
car select `0x8020D004`. Car select is the odd one out — it draws real 3D car
models (`0x802260A0`, `0x80220438`, `0x802244FC`) behind the same item list.

### 3.3 `0x8022E4E0` — the text renderer

4,076 bytes, one function, called through `0x8022F5DC` (**411 call sites** —
this is how the entire game draws text). It builds F3DEX/RDP commands inline
(`gDPPipeSync 0xE7000000`, `gDPSetOtherMode 0xBA001402`, combiner
`0xFC317E02 / 0x51FEF3FA`) into the display-list head at **`0x8028A858`**.

API:

| | |
|---|---|
| `0x8022F4F8` | align = left |
| `0x8022F504` | align = centre |
| `0x8022F5D0` | set width / font size (`0x803519D8`) |
| `0x8022F5DC` | `DrawString(x, y, str)` — measures, applies alignment, renders |
| `0x8022F720` | measure string width |

Two fonts, chosen by requested width: **small** (< 25) with glyphs at
`0x8028C070`, char height 20; **large** with glyphs at `0x8028BDF0`, char
height 40. Width tables at `0x802A187C` (`0x188` B) and `0x802A17A0`
(`0x2C0` B). All uncompressed in `.data`.

**Inline colour escapes.** Strings carry `%XY` where X and Y are colour letters:

- `%%` → literal `%`; `%i`, `%n` → single-char controls.
- X selects the **primary** colour, Y the **secondary** (the combiner blends the
  two, giving two-tone / drop-shadow text).
- Each letter indexes a 43-entry jump table — `0x802A9D60` for X, `0x802A9E0C`
  for Y — covering `'O'`(0x4F) through `'y'`(0x79), plus `'0'`, `'1'`, `'5'`
  handled separately. Unknown letters fall through to a no-op.
- Confirmed mappings: `r` = `0xBE0000`, `o` = `0xCD5F00`, `O` = `0xFF7800`,
  `y` = `0xFFF500`; `w`, `g`, `b`, `p`, `Y` are the remaining live entries.
  Default before any escape is `0xE6E600`.

So `%ryPAINT SHOP` is "PAINT SHOP, red on yellow", `%wwSelect` is flat white.
Every user-visible string in the ROM is plaintext with these escapes — around
770 of them, including debug prints that leak real identifiers
(`sizeof(UltraCarHeader)`, `sizeof(Vehicle)`, `sizeof(Enemy)`,
`savedRecordingLength`, `lapTimeFinal`, `decal_width`, `err_code`).

## 4. Module inventory (all 883 functions)

| module | vram | fns | bytes |
|---|---|---:|---:|
| front-end screens (results / season) | `80200000`–`8020AD5C` | 37 | 44,380 |
| **generic menu driver + helpers** | `8020AD5C`–`8020D004` | 7 | 8,872 |
| car select / paint car select | `8020D004`–`80210000` | 1 | 16,200 |
| main menu / 2P / options | `80210000`–`80214000` | 16 | 15,088 |
| controller-pak + pad config UI | `80214000`–`80218000` | 20 | 14,024 |
| gfx / 2D sprite / asset loader | `80218000`–`8022E4E0` | 256 | 91,100 |
| **text renderer + font** | `8022E4E0`–`8022F800` | 14 | 5,152 |
| HUD / race UI / track | `8022F800`–`8023E000` | 51 | 62,640 |
| zlib | `8023E000`–`80242000` | 22 | 14,048 |
| heap / misc | `80242000`–`80244D84` | 19 | 10,484 |
| paint shop (livery editor) | `80244D84`–`80254000` | 66 | 63,644 |
| libultra + libc *(see §10 — not all library code)* | `80254000`–`80270000` | 374 | 111,760 |

The menu proper — driver, screens, text, item tables — is **~95 functions and
~110 KB**. libultra + zlib (396 fns, 126 KB) come from published sources. That
leaves the asset loader and the 2D sprite path as the only substantial
unclassified dependency.

## 5. Verdict

**Yes, decompilable — and this is a notably favourable N64 target.**

In its favour:

- One resident segment, no overlays, CIC-6102. A splat config is close to
  trivial: one `code` segment at rom `0x1000` / vram `0x80200000`, then a run of
  `bin` records from `0x0AD400`.
- IDO, which is the exact toolchain the mature N64 decomp ecosystem is built
  around (splat, `ido-static-recomp`, `m2c`, `asm-differ`, decomp.me).
- 883 functions total, of which 396 are library code that gets matched rather
  than reversed. The real target is ~490 functions.
- Every string is plaintext, including debug prints carrying variable and type
  names — far better naming material than the PC DLL offered.
- The menu is **data-driven and separable**: static tables + one blocking driver
  + one text renderer. Its dependencies are the display-list builder, the ROM
  asset loader, the controller read, and the font — nothing entangled with the
  race code.
- No existing public TGR decomp to conflict with — greenfield.

Against:

- Byte-matching still needs the right IDO version pinned, and IDO's scheduling
  makes matching finicky. If the goal stays "portable C that runs" rather than
  "byte-identical", this cost disappears.
- The record-bundle format inside the compressed assets is only partly decoded
  (each bundle opens with a descriptor table — this is why
  `extracted/n64/menu_art/` had to guess image geometry; **that caveat is
  fixable**, the table is there).
- Car select renders 3D models, so a faithful port of *that* screen pulls in the
  model path. The other six screens do not.

## 6. As an optional alternate menu in the PC port

This lines up better than it has any right to. The port already has the three
things an N64 menu needs:

| N64 menu needs | port already has |
|---|---|
| F3DEX display-list consumer | `br_f3d` — display-list interpreter, retail geometry through Metal |
| CI4/CI8/RGBA16 texel decode | `br_n64tex` |
| zlib | system zlib; format already reversed in `tools/` |

What would have to be written is small and self-contained:

1. `MenuItem` tables — transcribe from `.data` (52 items, mechanical).
2. Icon assets — extract the `0x1F1E70`–`0x211C10` run, once the bundle
   descriptor is decoded.
3. Font — lift the two glyph sets and width tables from `.data`.
4. `%XY` colour-escape parser + `DrawString`/`MeasureString` (~4 KB of MIPS).
5. The driver `0x8020AD5C` re-expressed as a non-blocking state machine, or kept
   blocking behind the port's frame pump.

Estimate: the text renderer and driver are the only real work, roughly 14 KB of
MIPS between them. Both are single functions with clean interfaces.

The one architectural mismatch worth naming up front: the N64 driver is a
**blocking modal loop** that owns its own frame loop, whereas the PC front end is
built from **screen builders that construct a page of controls** and return
(`port/include/br_ui.h`). They are not the same shape, so this is an alternate
menu *implementation*, not a skin — it would sit beside `br_ui`, not on top of it.

## 7. Project framing

The end goal is a **unified decomp of both titles, with the PC build taking
precedence wherever they conflict** (user, 2026-08-16). The PC decomp lands
first; full N64 decompilation follows. But the ROM is a live reference in the
meantime, and cheap N64-side checks that save PC effort are wanted rather than
deferred — see §11 for one that paid off immediately.

Where the two differ, assume the 1999 PC build is the improvement. The N64 is
best at three things, because its debug strings survived and the PC DLLs' did
not: **naming** (what a function is for), **structure** (a prompt to re-read a
carving), and **existence** (that some behaviour ought to be there at all).
Constants, thresholds and layouts do **not** transfer across titles.

## 8. Phase 0: emulator as oracle (recommended before any N64 decomp)

The expensive unknowns here are **not** the logic — the item tables are static
and already recovered, and the driver is one function. The expensive unknowns are
the *asset* details: icon geometry, texture formats, TLUT association, font atlas
layout. A running emulator answers all of those for free, because the game tells
you: every icon reaches the RDP as
`gDPSetTextureImage` + `gDPSetTile` + `gDPLoadBlock` + `gDPSetTileSize`, which
carry format, bit depth, width and height explicitly.

The hook is already located: the display-list head is a single global,
**`0x8028A858`**. Breakpoint the menu driver, read the pointer, walk the list.

Practical setup on macOS/Apple Silicon: **ares** exposes a **GDB stub**, so a
short Python script can speak the GDB remote protocol over TCP directly — set a
breakpoint, read RDRAM, dump the DL and the referenced texture blobs each frame.
No emulator patching, no C++. (`HailToDodongo/ares-64` is a fork carrying extra
N64 debugging tools if the stock stub is not enough.)

What this yields, in rough order of value:

1. **Ground-truth display lists per menu frame** — which the port can feed
   straight into the existing `br_f3d` interpreter and Metal backend. This is the
   fastest possible path to "the N64 menu appears in our binary".
2. **Decompressed assets straight out of RDRAM** — sidesteps decoding the
   record-bundle descriptor entirely.
3. **Reference frames for pixel diffing** — a regression oracle that keeps a
   re-implementation honest instead of letting it drift.
4. **Observed input→state transitions** — enough to re-implement the driver
   behaviourally without matching it.

The honest limit: a DL capture is a *recording*, not a menu. Interactivity still
needs the item tables (have them) and the selection/dispatch logic (small, and
§3.2 documents it). So phase 0 gets the menu on screen and pins every asset
question; it does not by itself get you a working menu.

This is complementary to, not a substitute for, §5 — static analysis is the
cheaper source for *logic and tables*, the emulator is the cheaper source for
*assets and pixels*. Doing phase 0 first also de-risks any later decomp by
handing it a test oracle on day one.

## 9. Suggested next steps

Phase 0 (cheap, days, does not block the PC decomp):

1. ares + GDB stub, breakpoint the menu driver, dump the display list at
   `0x8028A858` and every texture it references. §7.
2. Replay those lists through the port's existing `br_f3d` + Metal path.
3. Use the dumped `gDPSetTileSize` data to fix the inferred geometry in
   `extracted/n64/menu_art/`, and to decode the record-bundle descriptor
   (first words of any record, e.g. `1E48D0.bin`: count, total size, then
   per-entry offsets).

Phase 1 (only if the PC decomp lands and this is still wanted):

4. Set up splat with the layout in §1 and let it split the single segment.
5. Pin the IDO version by putting `0x8022F4F8`/`0x8022F504` (12 and 16 bytes)
   and `0x8020C408` (88 bytes) on decomp.me and trying 5.3 then 7.1.
6. Decompile in this order: text renderer → item loader (`0x8020C27C`) → driver
   (`0x8020AD5C`) → main menu (`0x802111E0`). That is a runnable menu.

## 10. The N64 collision module — usable now, no N64 decomp required

Found while sizing the modules, and it is not a menu finding: the range
`0x8025C000`–`0x80260000` is **not** libultra as the §4 table implies. It is a
**~17 KB, 30-function collision / resting-contact module**, and its debug prints
survive with the quantities named:

| string | vram | function |
|---|---|---|
| `Stand Dist ` | `802AB687` | `0x8025D368` |
| `Stand Vel ` | `802AB69B` | `0x8025D368` |
| `Stand Point ` | `802AB6AF` | `0x8025D368` |
| `Wank CT1 case` | `802AB6C4` | `0x8025DFCC` |
| `Triangle Edge to CubeFace` | `802AB6D4` | `0x8025DFCC` |
| `Cube Edge to Triangle Face` | `802AB6F0` | `0x8025DFCC` |
| `Resistive collision %10.3f` | `802AB70C` | `0x8025DFCC` |
| `Standing on it's F'in Nose damnit` | `802AB728` | `0x8025E55C` |

This lands directly on the PC port's stated blocker — physics that is exact on
flat ground but diverges on a slope, with the only unported candidate being a
5,196-byte block of collision callees called four times per frame. What the N64
side supplies without decompiling anything:

- **The case taxonomy.** `Triangle Edge to CubeFace` / `Cube Edge to Triangle
  Face` says the car's collision volume is a **box tested against track
  triangles**, resolved by explicit edge-vs-face cases. That is a structural
  answer to what the missing block computes.
- **Named intermediates in the resting solver** — `Stand Dist`, `Stand Vel`,
  `Stand Point` in `0x8025D368` (2,384 B). A pitch response has to come out of
  a standing/resting contact solve, and here it is with its variables labelled.
- **A second implementation to read against**, in MIPS, which decompiles far more
  cleanly than optimised frame-pointer-omitted x86.

### 10.1 Call structure — the multiplicity is *inside* the solve

Traced statically, checking every call site for an enclosing backward branch in
both branch forms and `j` (there are no backward `j` anywhere in the chain):

```
0x8021C718   for (;;) RunCurrentState();       <- bare main loop, no pacing here
  0x8021C6F0   jalr g_stateFn                  <- set by 0x8021C6E4, 55 call sites
    0x8020082C   race tick (22 KB, one function, one `jr $ra`)
  for (i = 0; i < *(s32*)0x8028B7F0; i++)      <- entity loop, runtime count
    0x8022C9FC(entity)                          no loops; entity+0x60 -> car,
                                                car+0xED8 -> update fn
      jalr car->update   = 0x80226D7C | 0x802288B4
        0x8021F998                              call site not in a loop
          0x80221940                            has a 3-iteration loop, but it
                                                ENDS before the call below
            0x8025E55C   "Standing on it's F'in Nose damnit"   no loops
              +- 0x8025D368   Stand Dist / Vel / Point         no loops, straight line
              +- 0x8025DFCC   ONE loop, 0x8025E070..0x8025E4E0 (284 instrs)
```

So the contact solve runs **exactly once per car per race tick, and there is no
fixed-timestep accumulator at any level** — not in the solve, not in the entity
loop, not in the race tick (the entity-loop call site has exactly one enclosing
backward branch, the entity loop itself), and not in the main loop, which is a
bare `for(;;)`. The N64 build does not substep. Frame pacing must block somewhere
inside the race tick; that has **not** been located, so nothing here establishes
that the entity loop runs at a fixed 1/30 s — only that nothing subdivides it.

All four case strings sit *inside* that single
284-instruction loop — `Wank CT1 case` `0x8025E1A4`, `Triangle Edge to CubeFace`
`0x8025E234`, `Cube Edge to Triangle Face` `0x8025E274`, `Resistive collision`
`0x8025E39C`. One call iterates candidate features internally.

Two shape claims worth carrying over (shapes, not quantities — the constants
above are evidence for the structure and are **not** adoptable across titles):

- **`0x8025E55C` is the parent of both**, not a sibling: the nose guard *calls*
  the resting solve and the case solve. If the PC block has a corresponding
  parent, that is where a clamp would live.
- The resting solve `0x8025D368` is **straight-line, no loops** — it resolves one
  contact, it does not iterate.

Cheap next step, independent of any N64 decomp: name the PC-side callees from
this taxonomy, and look for a parent above them rather than a peer beside them.

## 11. Track format — the two titles share it, and share the data

Prompted by a question from the PC side: is the N64's on-cart track format the
same raw big-endian N64 memory image the PC loader reads behind a `0x230`
header? **Yes — same format, same header constant, and for at least one track
the same data.**

The N64 track loader `0x8021DE5C` carries the counterpart of the PC's
`Error: Track header size mismatch` check, at `0x8021E124`:

```
lw    $a2, 4($s4)          ; header[+0x04]
addiu $at, $zero, 0x230    ; <- the same constant the PC loader checks
beq   $a2, $at, ...
```

Immediately above it, `header[+0x64]` is bounds-checked against `0x800`.

All five track records on the cart have `+0x04 == 0x230`:

| track | rom | size | `+0x00` | `+0x08` | `+0x0C` | `+0x10` |
|---|---|---:|---|---:|---|---:|
| Desert | `0x211C10` | 1,512,224 | `0x171320` | 9,226 | `0x800C5A40` | 7,877 |
| Mountain | `0x300500` | 1,086,296 | `0x109358` | 5,398 | `0x800ABF38` | 4,994 |
| Coastline | `0x3D3050` | 1,298,624 | `0x13D0C0` | 6,427 | `0x800B0930` | 6,535 |
| Strip Mine | `0x551180` | 1,419,368 | `0x15A868` | 7,155 | `0x800B91B8` | 6,503 |
| Jungle | `0x65C140` | 1,306,160 | `0x13EE30` | 6,301 | `0x800CBD58` | 6,104 |

`+0x00` is the total size and matches each record's decompressed length exactly.
Names come from the track descriptors at `0x80270854` (stride `0x17C`), whose
`+0x14` holds the track-data rom offset; the five Mirror variants reuse the same
records with flag `0x10`.

**Desert's `+0x08`/`+0x10` are 9,226 and 7,877 — the exact pair the PC loader
reports for its own `desert`.** Both numbers matching is not coincidence: the two
titles ship the same track asset, not merely the same container.

> **Do not cite this table as independent confirmation of which field is which.**
> Which of `+0x08`/`+0x10` is faces and which is vertices comes *entirely* from
> the PC loader's interpretation. If that reading is wrong, this table inherits
> the error, and the two projects would be confirming each other in a circle that
> is invisible from inside either one.
>
> I attempted to settle it from the N64 side alone and **failed**. What was
> tried: consecutive header-pointer extents divided by each count at every
> plausible stride (one exact hit, `9226 x 8`, but only by counting from file
> offset 0, i.e. through the header — almost certainly coincidence); a stock
> `Vtx` signature scan (16-byte records, `flag` u16 zero) at all 13 pointer
> targets (no runs); and an index-bound test looking for an array of 9,226
> records whose fields stay below 7,877 (none).
>
> One genuinely independent observation did come out: the array immediately
> after the `0x230` header is **32-bit float** data (`0x446E546A` = 953.32,
> `0x443C31DB` = 752.78, `0xC232E4F7` = -44.72), not fixed-point `s16` vertices.
> Its extent divides evenly by neither count. So the record is float geometry
> behind the header, but the field naming remains the PC side's reading, unproven
> from here.

One structural difference worth noting: the PC maps to a *fixed* vram base, while
the N64 stores a per-track base at `+0x0C` (`0x800C5A40`, `0x800ABF38`, …).

This corrects a guess in `extracted/n64/README.md`, which listed these five
records as large menu-art bundles with an undecoded `[size, vaddr]` DMA table.
They are the five tracks, and the "DMA table" is the `0x230` track header.

Consequences for the PC work, which is where the value is:

- The PC track loader gains **five more files to validate against**, in a format
  it already parses.
- Shared track *data* means the collision **input** is the same asset in both
  titles. That does not prove the collision *code* is shared — but it is
  evidence of continuity in the data pipeline across the two years, which is the
  relevant question when deciding how far to trust N64-derived naming.

## 12. F3DEX dialect — the N64 is stock, which validates the PC's divergences

Prompted by the PC side's other offer: is the N64's F3DEX recognisably the same
dialect the PC's ported interpreter, clipper and texel expander already handle?

Command vocabulary emitted by the N64 build, recovered by finding every `lui`
that materialises a GBI command word:

```
geometry   B0 B1(G_TRI2) B3/B4(RDPHALF_2/1) B6 B7 B8 B9 BA BB BC BD BF
RDP        E4(TEXRECT) E6 E7 E8 E9 ED F0(LOADTLUT) F2 F3 F5
           F6(FILLRECT) F7 F8 F9 FA FB FC FD FE FF
```

That is **stock F3DEX with no custom commands.** `G_TRI2` (`0xB1`) and the
`RDPHALF_1`/`RDPHALF_2` pair confirm F3DEX rather than plain F3D, consistent with
the embedded `RSP Gfx ucode F3DEX.NoN 1.21`. The texrect counts line up exactly —
10 sites each of `0xE4`, `0xB4` and `0xB3` — which is textbook
`gSPTextureRectangle`.

**This independently confirms a divergence the PC decomp already documented.**
`CONVENTIONS.md` records that in the PC build, command `0xE1` is FILL RECTANGLE
with plain 12-bit *integer* corners, unlike the N64's `0xF6` with 10.2 fixed
point. Counting on the N64 side: **`0xF6` appears at 17 sites, `0xE1` at zero.**
The N64 uses the stock fill rect and never emits `0xE1` at all — so the PC's
`0xE1` really is a PC-side repurposing, not a misread.

Value to the PC work: the N64 producer is a **stock reference** for everything
the PC consumer handles. Where the PC's reading matches stock F3DEX, it is
corroborated; where it doesn't, the difference is a real PC divergence worth
recording rather than a candidate bug. The `0xE1` case is the worked example.

Method caveat, recorded because it bit me: an initial pass required the `lui` to
be followed by an `sw` of the same register within 8 instructions, and that
filter reported `0xF6` and `0xE4` as **absent**. They are not — the store is
simply further away. It also produces false positives in the opposite direction:
`0xBE`, `0xC8`, `0xCD` and `0xD2` "commands" in the text renderer are RGBA colour
words (`0xBE0000FF` red, `0xCD5F00FF` orange), not command bytes. Any
`lui`-immediate scan for GBI opcodes collides with colours and floats; cross-check
against the RDPHALF pairing counts before trusting a vocabulary list.
