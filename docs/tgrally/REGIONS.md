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
- A body the region changed is written once, with `#if TGR_REGION_EUR` (or
  `_JPN`) where it differs and the USA code in the `#else`.
- A function only a region has is tagged with that region's own address,
  `@implements eur:0x80XXXXXX tgr Name`, and defined under `#if
  TGR_REGION_EUR`; the other regions' tools skip the tag. `@t3` tags take the
  same form. A USA function a region does not have is left out of that
  region's build with `#if !TGR_REGION_EUR`; until it is, the region's ledger
  lists it `ABSENT`.
- Data only a region has is named by its region address, `eur_D_80XXXXXX`
  (`func_`, `jtbl_` and `L_` likewise), or listed in
  `config/tgrally/<region>/symbols_tgr.csv` (name, region address). Neither is
  translated.
- `config/tgrally/<region>/addrmap_pins.csv` (usa,va) holds the pairings the
  map rules get wrong, by hand: `regionmap.py` takes a pinned function out of
  its sequence pairing and pairs it as pinned, and a pinned data address
  overrides its votes. An empty `va` says the region has no counterpart.
- Every other N64 tool reads `TGR_REGION` too: `n64tiers.py` (the region's
  counts, with denominators), `n64image.py`, `n64link.py`, `n64box.py`,
  `n64t3.py`, `n64strs.py`, `pair.py`, `cover.py`. A region's own records
  (`fenced_tgr.csv` for its extra library functions, `t3_live.csv`,
  `whole_image.csv`) live in `config/tgrally/<region>/`; addresses on a
  region tool's command line and in its records are the region's.
- `n64box.py` translates the libultra entry points it models and the data
  it touches through the map. Europe's box runs at 50 Hz (PAL) and models the
  cartridge's 4 Kbit EEPROM (`osEepromProbe`, `osEepromRead`,
  `osEepromWrite`; the long read and write run for real, waiting on the timer
  queue `osContInit` creates).

```bash
TGR_REGION=eur .venv/bin/python tools/tgrally/n64tiers.py     # Europe's counts
TGR_REGION=eur .venv/bin/python tools/tgrally/n64image.py     # Europe's M2 image
```

## Where it stands (2026-10-28)

The USA source, unchanged, against each region (functions):

| region | EXACT | DIFF | ABSENT | new game fns | new libultra fns |
|---|---:|---:|---:|---:|---:|
| eur | 676 / 735 | 58 | 1 | 11 | 8 |
| jpn | 651 / 735 | 82 | 2 | 14 | 9 |

## Plan

Two independent tracks. Neither waits on the other.

**Decomp: one effort per region.** Europe and Japan are each their own
byte-exact effort, tracked like the USA decomp (counts with denominators, a
status line in the README).

1. Region layer (done): `region.py`, `regionmap.py`, `n64build.py` and
   `n64rom.py` take `TGR_REGION`.
2. Tags for functions only the region has; the rest of the N64 tools
   (`n64box.py`, `n64link.py`, `pair.py`, `cover.py`, `n64strs.py`, `n64t3.py`)
   on `region.py`; a per-region count for the progress reports.
3. Hand transcription, one function at a time: each DIFF body under `#if`,
   each new function, until the region's ledger is all EXACT; then the
   whole-image check.

**Port (done 2026-10-29).** `TGR_REGION=eur|jpn ports/tgrally/link.sh` builds
the port from that region's decomp source and needs only that region's ROM
(`ports/tgrally/PORT.md`, Regions). Each build is checked against n64box
running its own cartridge: all 63 scripts identical for the USA, Europe and
Japan (`TGR_CHECK=1`). Europe runs at its console's 50 Hz.

**Remastered: after the decomps.** Its Menus option will offer Boss Rally, Top
Gear Rally (USA), Top Gear Rally (Europe) and Top Gear Rally (Japan), its
Top Gear Rally menus built from that region's source; packaging needs all three
ROMs; the game logic runs at 60 Hz. It waits because the menus a region
changes are code, not only assets: the title screen's code loads seven models
in the USA, six in Europe (the Midway one dropped) and a different set of two
in Japan. Only the menu music is a pure asset swap (the module the main menu
loads: USA 0xEBC00, Europe 0xEB310, Japan 0x102B50). Races are Boss Rally's,
whose cars include Japan's three (Type-PJ, Type-TR, Type-MN for Type-RS,
Type-M3 and Helmet); the attract demos are Boss Rally's too.
