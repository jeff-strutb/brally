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
3. The dumps the game needs and the folder to build into: a Top Gear Rally
   ROM (the USA's, Europe's or Japan's cartridge: the build is that cartridge's
   game), the retail Boss Rally CD as BIN/CUE, or for Remastered the USA's and
   Japan's ROMs and the CD. The builder shows the MD5 it expects and each
   file's own, and builds only from a match.
4. The build: the code ships inside the builder; the data comes from the dumps.
5. The result: the app, the .ipa or the folder, or the reason the build stopped.

The result needs no dump nor this tree: it carries the cartridge's data (from
the end of its code on) as `romdata.bin`, the CD's data track as `disc`
(BRGlide.dll's initialised data is read from there at start-up) and its audio
tracks as FLAC in `music/cd` (Boss Rally's soundtrack, and Remastered's PC
music). Remastered carries two cartridges' data, `romdata.bin` (the USA's) and
`romdata-jpn.bin` (Japan's), each with the other's front-end tune and three
cars after it, made by the builder from the two ROMs as
`ports/remastered/tools/regionextras.py` makes them. Linux is not built yet (no
Linux window host in the ports).

Version 0.94. The builder and every game it builds run on macOS 12 or later
(Apple silicon and Intel) and Windows 10 or later (64-bit, with a Vulkan
driver or OpenGL 3.3); Remastered's iPhone and iPad app on iOS 16 or later.

## The games it builds

Each is a native port of a decompilation (`ports/README.md`): the game's own
code compiled for the machine, with a platform layer under it. Saves and
settings live in the player's user folder (Application Support on macOS and
iOS, `%APPDATA%` on Windows), so a rebuild never touches them. Full screen:
View > Enter Full Screen (Control-Command-F) on a Mac, Alt+Enter on Windows.

| game | source | what it is |
|---|---|---|
| Boss Rally | `ports/brally`, from `src/brally` (every function certified to behave as the retail game's, 78.6% of its bytes byte-exact) | the PC game as it shipped: its menus, keys, CD soundtrack and Quit Game; drawn with Metal on a Mac, Vulkan or OpenGL 3.3 on Windows |
| Top Gear Rally | `ports/tgrally`, from `src/tgrally` (byte-exact, every function, all three cartridges) | the cartridge the ROM is from: the USA's, Europe's (at its own 25 frames a second, PAL 50 Hz) or Japan's (its own title, text and real liveries), drawn as the N64's RDP drew it; saves on a Controller Pak kept as a file, and Europe's and Japan's settings on the cartridge's EEPROM. Beyond the cartridge: Options > Keys and Pad (the keys and gamepad buttons for each of the N64 pad's), Exit Game on the pause menu and Escape on the main menu (`ports/tgrally/PORT.md`) |
| Top Gear Rally Remastered | `ports/remastered`, both ports in one app | Top Gear Rally's menus and seasons with every race run by Boss Rally's engine, races at 60 ticks a second; Remaster Options (Widescreen, Music, Show FPS, View Distance, Menus); the menus of the USA cartridge, of Japan's in English (with its Game Pak saves), or Boss Rally's own; all six cars Japan and the USA differ by; Europe's and Japan's cheats; two players split screen; one set of controls for the menus and the races, every on-screen prompt naming your own; controller rumble (the Rumble Pak's, in every race); the paint shop by mouse and touch; no loading screens; touch on the iPhone and iPad (`ports/remastered/README.md`) |

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, the cue sheet and ISO 9660 (Joliet) reader, the FLAC encoder (`flac.c`), the disc icon's conversion (`icon.c`), the build itself (`build.c`) |
| `src/mac/` | the AppKit front end, the app's Info.plist, and `ios_sign.sh` (finds the player's Xcode signing and signs the iPhone build with it) |
| `src/win/` | the Win32 front end, its manifest and version resources |
| `build.sh` | builds both builders into `build/builder/dist/`: the macOS app, and a Windows zip of the exe with the three games' executables and icon beside it in `games\` |
| `release.sh` | builds and publishes them as a GitHub release |
| `version.py` | the release version: the average M2 progress over Top Gear Rally's three cartridges and Boss Rally, two decimals, rounded down, or its `RELEASE` while that is ahead |
| `check_payload.py` | proves the shipped game executables carry none of the games' data |

## Building

```sh
builder/build.sh                 # build/builder/dist/RallyBuilder-VERSION-{macOS,Windows}.zip
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is the games built without their data (`ports/brally` with
`IMAGE=runtime`, `ports/tgrally` with `ROMDATA=file` once for each cartridge,
`tgrally`, `tgrally-eur` and `tgrally-jpn`, and `ports/remastered` built from
them): on macOS drawn with Metal, universal, with Remastered's iPhone
app beside them (unsigned, `ios/build.sh
--payload`); on Windows drawn with Vulkan, with an OpenGL 3.3 build of each
beside it for a PC without a Vulkan driver (the builder picks). Building needs Xcode's
clang, the iOS SDK and xcodegen, mingw-w64 for Windows, and the dumps under
`reference/` that the ports already build from (`check_payload.py` reads them
to prove their absence).

Both front ends take arguments for a scripted run:
`--game br|tgr|remastered --dest DIR --bin FILE --cue FILE --rom FILE --rom-jpn FILE`, and on macOS `--target mac|ios`;
on Windows `--build` starts the build at once. `RB_SNAPSHOT=DIR` makes the macOS app write
each page it shows to DIR as a PNG.
