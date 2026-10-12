# Rally Builder

A native app for macOS and Windows that builds Top Gear Rally Remastered
(`ports/remastered`: Top Gear Rally's menus, with Boss Rally's engine running
the races) from the player's own copies of the two games. Players download it
from the repository's Releases page; nothing here ships game data.

1. What to build for: the system the builder runs on. On macOS, the Mac or the
   iPhone (an .ipa, signed with the player's own Xcode signing when the Mac
   has a code-signing certificate with a provisioning profile for it, else
   unsigned, with a warning in red; it runs on the iPad too); on Windows, Windows. And the folder to
   build into.
2. The dumps: the Top Gear Rally (USA) ROM and the retail Boss Rally CD as
   BIN/CUE. The builder shows the MD5 it expects and each file's own, and
   builds only from a match.
3. The build: the code ships inside the builder; the data comes from the dumps.
4. The result: the app, the .ipa or the folder, or the reason the build stopped.

The result needs neither dump nor this tree: it carries the cartridge from ROM
0x70AB0 on as `romdata.bin` and the CD's data track as `disc` (BRGlide.dll's
initialised data is read from there at start-up). Linux is not built yet (no
Linux window host in the ports).

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, the cue sheet and ISO 9660 (Joliet) reader, the build itself (`build.c`) |
| `src/mac/` | the AppKit front end, the app's Info.plist, and `ios_sign.sh` (finds the player's Xcode signing and signs the iPhone build with it) |
| `src/win/` | the Win32 front end, its manifest and version resources |
| `build.sh` | builds both builders into `build/builder/dist/`: the macOS app, and a Windows zip of the exe with the game executables and icon beside it in `games\` |
| `release.sh` | builds and publishes them as a GitHub release |
| `version.py` | the release version: the average of the two decompilations' M2 progress, two decimals, rounded down |
| `check_payload.py` | proves the shipped game executables carry none of the games' data |

## Building

```sh
builder/build.sh                 # build/builder/dist/RallyBuilder-VERSION-{macOS,Windows}.zip
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is `ports/remastered` built without the games' data (`ports/brally`
with `IMAGE=runtime`, `ports/tgrally` with `ROMDATA=file`): on macOS drawn with
Metal, universal, with the iPhone app beside it (unsigned, `ios/build.sh
--payload`); on Windows drawn with Vulkan, with a software-rendered build beside
it for a PC without a Vulkan driver (the builder picks). Building needs Xcode's
clang, the iOS SDK and xcodegen, mingw-w64 for Windows, and the dumps under
`reference/` that the ports already build from (`check_payload.py` reads them
to prove their absence).

Both front ends take arguments for a scripted run:
`--dest DIR --bin FILE --cue FILE --rom FILE`, and on macOS `--target mac|ios`;
on Windows `--build` starts the build at once. `RB_SNAPSHOT=DIR` makes the macOS app write
each page it shows to DIR as a PNG.
