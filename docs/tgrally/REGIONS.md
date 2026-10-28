# Top Gear Rally: the Europe and Japan cartridges

The decomp targets three cartridges from one source tree. USA is the base;
Europe and Japan are sub-variants held to the same standard: every function
byte-exact against that region's ROM.

| region | ROM | code | build date |
|---|---|---|---|
| `usa` | `Top Gear Rally (USA).z64` | NGRE | 25 Aug 1997 |
| `eur` | `Top Gear Rally (Europe).z64` | NTRP | 25 Sep 1997 |
| `jpn` | `Top Gear Rally (Japan).z64` | NTRJ | 16 Oct 1997 |

`Top Gear Rally (Asia) (En).z64` (NGRX) is kept for completeness and is not a
target.

## What the regions change

From the cartridges themselves and the TCRF page:

- **Title screen.** Japan has its own logo; Europe keeps the USA logo. Europe
  and Japan drop the Midway lines from the copyright text.
- **Title and menu music.** Europe and Japan play an up-tempo title tune in
  place of the USA's mellow one.
- **Intro demo.** USA drives the Desert track; Europe and Japan drive Mountain,
  Japan's cut longer with quick edits at the end.
- **Saving.** Europe and Japan add EEPROM saving beside the Controller Pak.
- **Cars (Japan).** Real-livery decals on every car, a white Milk Truck, and
  three cars replaced: Type-RS by Type-PJ, Type-M3 by Type-TR, Helmet by
  Type-MN. Boss Rally later carried all three.
- **Text (Japan).** Some Japanese text; KM/H and MILE/H; H-Brake; handling
  types Slow/Normal/Quick; Slip/Grip and Soft/Hard; the laps and volume
  instructions removed; the car colour resets after the lap-times screen.
- **AI (Europe).** Faster computer cars on Desert and Jungle.
- **Video (Europe).** PAL, 50 Hz.

## Layout

Each ROM is one resident segment at 0x80200000 from ROM 0x1000.
`tools/tgrally/region.py` holds the bounds:

| region | .text end (ROM) | .data end (ROM) | .bss |
|---|---|---|---|
| usa | 0x70AB0 | 0xAD400 | 0x802AC400 + 0xD67B0 |
| eur | 0x72A80 | 0xAF0F0 | 0x802AE0F0 + 0xD6A40 |
| jpn | 0x72530 | 0xADB30 | 0x802ACB30 + 0xD6C70 |

## How the tools handle a region

```bash
.venv/bin/python tools/tgrally/regionmap.py eur        # once per map change
TGR_REGION=eur .venv/bin/python tools/tgrally/n64build.py
TGR_REGION=eur .venv/bin/python tools/tgrally/n64build.py --diff 0x8021D6A8   # a USA address
```

- Every address in `src/tgrally` stays a USA address. `regionmap.py` pairs
  each USA function with its region home (equal bodies with addresses
  blanked, in link order; calls from unchanged code outrank the order) and
  each data address by walking the paired bodies' `%hi/%lo` pairs. The result
  is `config/tgrally/<region>/addrmap.csv`.
- `n64build.py` grades against the region ROM through that map, compiles
  with `-DTGR_REGION_USA`, `-DTGR_REGION_EUR` or `-DTGR_REGION_JPN`, and writes
  its ledger to `build/tgrally/n64-<region>/verify.csv`. A USA function the
  region does not have is `ABSENT`.
- A body the region changed is written once, with `#if` on the region macro
  where it differs. A function only a region has is tagged with its region
  address (`@implements 0x80XXXXXX tgr Name` under `#if`), once the tools
  learn region-only tags.

## Where it stands (2026-10-28)

The USA source, unchanged, against each region (functions):

| region | EXACT | DIFF | ABSENT | new game fns | new libultra fns |
|---|---:|---:|---:|---:|---:|
| eur | 676 / 735 | 58 | 1 | 11 | 8 |
| jpn | 651 / 735 | 82 | 2 | 14 | 9 |

## Plan

1. **Region layer** (done): `region.py`, `regionmap.py`, `n64build.py` and
   `n64rom.py` take `TGR_REGION`.
2. **Region-only functions**: tags for functions the USA does not have, the
   rest of the N64 tools (`n64box.py`, `n64link.py`, `pair.py`, `cover.py`,
   `n64strs.py`, `n64t3.py`) on `region.py`.
3. **Transcription**, by hand, one function at a time: each DIFF body under
   `#if`, each new game function, until both ledgers are all EXACT; then the
   whole-image check per region.
4. **Port** (`ports/tgrally`): a region flag that builds from that region's
   source and ROM data, always at 60 Hz (the PAL timing is in the decomp, never
   run).
5. **Remastered**: the Menus option offers Boss Rally, Top Gear Rally (USA),
   Top Gear Rally (Europe) and Top Gear Rally (Japan). A region changes the
   content (title screen, music, demo, cars, text), not the game logic.
   Packaging Remastered requires all three ROMs; building the port requires
   only the chosen region's.
