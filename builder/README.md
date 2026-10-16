# Rally Builder

A native app for macOS and Windows that builds a game from the player's own
copy of it: Boss Rally (`ports/brally`), Top Gear Rally (`ports/tgrally`), or
Top Gear Rally Remastered (`ports/remastered`: Top Gear Rally's menus, with Boss
Rally's engine running the races). Players download it from the repository's
Releases page; nothing here ships game data.

1. Which game.
2. What to build for (Remastered on macOS): the Mac or the iPhone (an .ipa,
   signed with the player's own Xcode signing when the Mac has a code-signing
   certificate with a provisioning profile for it, else unsigned, with a
   warning in red; it runs on the iPad too). Boss Rally and Top Gear Rally build
   for the Mac; on Windows, every game builds for Windows, and this page is
   skipped.
3. The dumps the game needs and the folder to build into: the Top Gear Rally
   (USA) ROM, the retail Boss Rally CD as BIN/CUE, or both for Remastered. The
   builder shows the MD5 it expects and each file's own, and builds only from a
   match.
4. The build: the code ships inside the builder; the data comes from the dumps.
5. The result: the app, the .ipa or the folder, or the reason the build stopped.

The result needs no dump nor this tree: it carries the cartridge from ROM
0x70AB0 on as `romdata.bin`, the CD's data track as `disc` (BRGlide.dll's
initialised data is read from there at start-up) and its audio tracks as FLAC
in `music/cd` (Boss Rally's soundtrack, and Remastered's PC music). Linux is not
built yet (no Linux window host in the ports).

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, the cue sheet and ISO 9660 (Joliet) reader, the FLAC encoder (`flac.c`), the disc icon's conversion (`icon.c`), the build itself (`build.c`) |
| `src/mac/` | the AppKit front end, the app's Info.plist, and `ios_sign.sh` (finds the player's Xcode signing and signs the iPhone build with it) |
| `src/win/` | the Win32 front end, its manifest and version resources |
| `build.sh` | builds both builders into `build/builder/dist/`: the macOS app, and a Windows zip of the exe with the three games' executables and icon beside it in `games\` |
| `release.sh` | builds and publishes them as a GitHub release |
| `version.py` | the release version: the average of the two decompilations' M2 progress, two decimals, rounded down |
| `check_payload.py` | proves the shipped game executables carry none of the games' data |

## Building

```sh
builder/build.sh                 # build/builder/dist/RallyBuilder-VERSION-{macOS,Windows}.zip
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is the three games built without their data (`ports/brally` with
`IMAGE=runtime`, `ports/tgrally` with `ROMDATA=file`, and `ports/remastered`
built from them): on macOS drawn with Metal, universal, with Remastered's iPhone
app beside them (unsigned, `ios/build.sh
--payload`); on Windows drawn with Vulkan, with an OpenGL 3.3 build of each
beside it for a PC without a Vulkan driver (the builder picks). Building needs Xcode's
clang, the iOS SDK and xcodegen, mingw-w64 for Windows, and the dumps under
`reference/` that the ports already build from (`check_payload.py` reads them
to prove their absence).

Both front ends take arguments for a scripted run:
`--game br|tgr|remastered --dest DIR --bin FILE --cue FILE --rom FILE`, and on macOS `--target mac|ios`;
on Windows `--build` starts the build at once. `RB_SNAPSHOT=DIR` makes the macOS app write
each page it shows to DIR as a PNG.
