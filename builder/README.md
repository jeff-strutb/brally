# Rally Builder

A native app for macOS and Windows that builds Boss Rally or Top Gear Rally
from the player's own copy of the game. Players download it from the
repository's Releases page; nothing here ships game data.

1. Choose the game and the folder to build into.
2. Provide the dump: the retail Boss Rally CD as BIN/CUE, or the Top Gear
   Rally (USA) ROM. The builder shows the MD5 it expects and the file's own,
   and builds only from a match.
3. The build: the game's code ships inside the builder; the data comes from
   the dump.
4. The result: the game, or the reason the build stopped.

The result needs neither the dump nor this tree. Boss Rally gets the data
track's files (BRGlide.dll's initialised data is read from there at start-up)
and the 12 CD audio tracks as FLAC; Top Gear Rally gets the cartridge from ROM
0x70AB0 on as `romdata.bin`.

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, the cue sheet and ISO 9660 (Joliet) reader, a FLAC encoder, icon conversion, the build itself (`build.c`) |
| `src/mac/` | the AppKit front end and the app's Info.plist |
| `src/win/` | the Win32 front end, its manifest and resources (the game executables ride along as RCDATA) |
| `build.sh` | builds both builders into `build/builder/dist/` |
| `release.sh` | builds and publishes them as a GitHub release |
| `version.py` | the release version: the lower of the two decompilations' M2 progress, two decimals, rounded down |
| `check_payload.py` | proves the shipped game executables carry none of the games' data |

## Building

```sh
builder/build.sh                 # build/builder/dist/RallyBuilder-VERSION-{macOS.zip,Windows.exe}
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is the two ports built without their data: `ports/brally`
with `IMAGE=runtime` (macOS Metal, universal; Windows, software renderer) and
`ports/tgrally` with `ROMDATA=file` (likewise). Building needs Xcode's clang,
mingw-w64 for Windows, and the dumps under `reference/` that the ports
already build from (`check_payload.py` reads them to prove their absence).

Both front ends take arguments for a scripted run:
`--game br|tgr --dest DIR --bin FILE --cue FILE --rom FILE`; on Windows
`--build` starts the build at once. `RB_SNAPSHOT=DIR` makes the macOS app write
each page it shows to DIR as a PNG.
