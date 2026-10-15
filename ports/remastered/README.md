# Top Gear Rally Remastered

Top Gear Rally's menus with Boss Rally's engine running the races, on macOS,
Windows, Linux and the iPhone. Nothing here is byte-matched; both games are the
native ports (`ports/tgrally`, `ports/brally`) built as libraries, with what
Remastered adds laid over them (`tree.sh`).

## How it fits together

| piece | what |
|---|---|
| `tree.sh` | the two ports as Remastered builds them: each port, Remastered's own files over it (`brally/`, `tgrally/`, at the paths they take in the port), its changes to the port's files applied (`patch/brally`, `patch/tgrally`, one unified diff a file, no fuzz). `make ROOT` builds a tree, `save ROOT` turns an edited tree back into files and patches, `check ROOT` says whether it would. A patch that stops applying stops the build: the port moved under it |
| `brally/`, `patch/brally` | Boss Rally as Remastered has it: the lighting (`platform/light`, the Metal renderer's lit passes), the race that fills any window, the races run for Top Gear Rally's menus (`race_handoff.c`, `host_race.h`), the pause menu's Remaster Options and frame rate (`pause_options.c`), the music hooks (`audio.c`) |
| `tgrally/`, `patch/tgrally` | Top Gear Rally as Remastered has it: the race that fills any window (`view.c`, `gfx/rcp.c`), the hand-over (`src/racing/handoff.c`), touch (`os/touch.c`), the frame rate (`drawing/frameloop.c`), the music's mute (`audio/mixer.c`) |
| `platform/options.c` | the Remaster Options: kept in the save folder (`remaster.cfg`), applied to both games |
| `ports/tgrally/link.sh`, `HOST=lib` | Top Gear Rally as `libtgrally.a`, its `main()` as `tgr_main` |
| `ports/brally/link64.sh`, `HOST=lib` | Boss Rally as `libbrally.a`, its `main()` as `br_main` |
| `platform/thunks.c` | each game's calls to the host for the window, input, sound and folders, sent on to `mux_tgr_*` or `mux_br_*` |
| `platform/mux.c` | the one real host (`host/macos`, `host/windows`, `host/null`, the iPhone's Swift) shared between the two |
| `platform/main.c` | the process: Top Gear Rally on the main thread (on the iPhone, `rm_main` on a thread of the app's), Boss Rally started on its own thread at the first race |
| `link.sh` | makes the tree (`OUT/tree`), builds both libraries in it, makes each one object (its thunks inside, only its entry points global: the two games share some hundred function names), links the executable |
| `package_app.sh` | `Top Gear Rally Remastered.app` (macOS) |
| `music.sh` | Boss Rally's CD audio as FLAC, the PC's music, into the app (`package_app.sh`, `ios/build.sh`) |
| `tools/` | the lighting's tests (`lighttest/`, run against `build/remastered/tree`), `lightcheck.sh`, `texmat_h.py`; `mixcheck.sh`, the music's routing in `mux.c` checked against computed values |
| `ios/` | the iPhone app: `build.sh`, the xcodegen project, and in `Sources` the Swift host under the multiplexer (the Metal layer, events, Core Audio, the save folder) and the touch controls |
| `icon.png` | the app's icon (the `.icns` on macOS, `icon.ico` beside the exe on Windows) |

Each race is handed to Boss Rally (`host_race.h`,
`tgrally/src/racing/handoff.c`, `brally/platform/common/race_handoff.c`).

**Remaster Options.** Boss Rally's pause menu has a Remaster Options row
before its quits. It opens a page of that name in the menu's own style: one
row an option, its two choices side by side, the one in force in the row's
colour and the other faded, then Back. Up and down move, left and right
choose at once; Return takes the other choice, and Back or Escape returns to
the pause menu. A tap or a click chooses a row or a choice. The choices go to
both games and are kept (`platform/options.c`, `remaster.cfg` in the save
folder; `RM_OPT_<NAME>=N` sets one for a check):

| option | choices | default |
|---|---|---|
| Widescreen | On: a race fills the window (`TGR_FLAG_ANY_ASPECT`, `BR_FLAG_ANY_ASPECT`); Off: the games' 4:3, boxed | On |
| Music | PC: Boss Rally's CD; N64: Top Gear Rally's pieces | N64 |
| Lighting | Original: the game's picture; Remaster: the lighting (`BR_FLAG_LIGHTING`) | Remaster |
| Show FPS | the frames a second at the bottom centre, in either game | Off |

The window opens at 16:9 with Widescreen on and takes any shape the player
gives it; the games' wishes to lock its shape are not passed on. Nothing
switches on Tab.

**Music.** The N64's: Top Gear Rally's pieces, its title piece in the menus
and the course's under a race, its whole output raised 15.1 dB (x5.70) to
the PC's loudness (its five race pieces measured -30.6 LUFS by their
energy's mean, Boss Rally's eleven race tracks -15.5; the loudest piece then
peaks at -2.1 dBFS) and, under a race, following Boss Rally's music volume.
The PC's: Boss Rally's CD, its front end's track behind Top Gear Rally's
menus (Boss Rally boots behind them at launch, its effects silent there) and
the race's own pick under a race. A change crossfades over half a second; a
limiter holds the sum under full scale (`platform/mux.c`).

One game is in front at a time (`mux_front`): the keys, the mouse and the
pad are its, and only it draws. Top Gear Rally's window loop drains the real
event queue and passes on what belongs to Boss Rally; keys held when a game
leaves the front go up for it. The sound is one output at Top Gear Rally's
rate: its music plays on under a Boss Rally race and Boss Rally's effects are
added, resampled. On Windows Boss Rally draws into a child window over the
client area while it is in front (a Vulkan swapchain owns the window it
presents to); on macOS both draw into the window's Metal layer.

Saves: the app's folder (`Application Support/Top Gear Rally Remastered`
on macOS and the iPhone, `%APPDATA%\Top Gear Rally Remastered`), Boss Rally's in its
`Boss Rally` folder. Boss Rally's data is built in (`BRGlide.dll`'s, `DLL`)
and its data track is the CD root (`BR_CDROOT`, else
`reference/brally/data/disc`, else the app's `disc`).

## Building

```sh
HOST=macos RENDER=metal ports/remastered/link.sh          # build/remastered/macos-metal/remastered
ports/remastered/package_app.sh                           # ... /Top Gear Rally Remastered.app
CC=ports/brally/tools/wincc.sh HOST=windows RENDER=vulkan ports/remastered/link.sh
                                                          # build/remastered/windows-vulkan/remastered.exe
HOST=null ports/remastered/link.sh                        # headless (Linux has no window host yet)
ports/remastered/ios/build.sh [--sim] [--install [--device ID]]
                                                          # build/remastered/ios-metal (ios-sim-metal)
```

Check of the whole round trip, headless: Top Gear Rally's script starts a
Championship round, Boss Rally's autopilot drives it, and `RM_LOG=1` prints each
race handed over and the outcome that came back. (Most of the N64 scripts do not
reach the race they name here: with the races handed over the 1P/2P screen
offers one player, so their timed presses land elsewhere, often in Practice,
which never finishes.)

```sh
RM_LOG=1 BR_HANDOFF_AUTOPILOT=1 build/remastered/null-null/remastered --headless \
    --frames 100000000 --script tools/tgrally/n64box_scripts/champ_race.txt
```

## The iPhone

`ios/build.sh [--rom FILE] [--bin FILE] [--sim] [--install [--device ID]]`
builds `libgames.a` (`link.sh`, `HOST=ios`: both games, the multiplexer and
the process as `rm_main`), puts Boss Rally's data track in the app as
`Resources/disc` and the icon in its asset catalog, generates the Xcode project
with xcodegen into `build/remastered/ios-metal` and signs it (`--sim`: a
Simulator build; `--install` puts it on the first paired iPhone). It replaced
`ports/tgrally/ios` (the same bundle identifier, so it installs over it).
It runs on the iPhone and the iPad. Landscape only; the games draw into the
whole screen, clear of the iPhone's sensor housing at the sides, whatever its
shape (Remastered: a race fills it, its camera widened or heightened to the
shape, menus keep 4:3 inside it). Touch drives the game
in front (`mux_front_game`), and the fingers on the glass are let go when the
other comes in front:

| | |
|---|---|
| in a race | a finger right of the brake zone holds the accelerator; sideways steers (full lock 90 points either way, the neutral point sliding with the finger past it), a wheel drawn under the finger turning with it; lift to coast. A finger on the left 30% of the screen (a pedal is drawn there) brakes and, the car stopped, reverses; it lets the accelerator go while held. In Boss Rally's races it is that game's reverse (the gamepad's button 2), which drives backwards at any speed, slowing the car first; in this game's own it is B until the car is below 3 km/h (`tgr_race_speed`), then A with the stick pulled back (this game reverses only from first gear) |
| in a race | the pause button at the top centre (two bars) pauses it |
| menus, pause menu | tap an item: the carousel's arrows step it, the item between them is A, a list row moves the cursor there and chooses it, the button prompts (Select, Go Back, OK, Cancel, Exit) press their button; a tap on a screen with nothing to choose is A. Swipe: the content follows the finger (left brings on the next item, up the row below); a value row (a volume) goes up with the finger |
| anywhere | two-finger tap: START; three-finger tap: the sound's low-pass filter, off / 8 / 7 / 6 / 5 kHz (5 at first, kept between runs; `TGR_AUDIO_LOWPASS=Hz` on the desktop) |

In Boss Rally's races a tap on its pause menu chooses the row or the choice
it lands on (`br_race_tap`); a swipe is an arrow key (the same way round), a
two-finger tap Escape. The menus name
their tap targets as they draw (`ports/tgrally/platform/os/touch.c`);
`TGR_TOUCHLOG=1` logs each tap and what it met. The app's options (from the
first `--`) go to the process: a Simulator check runs the same round trip as
the headless one,

Frame rates: `TGR_STATS=1` and `BR_STATS=1` report each second the frames
each game drew and those the screen showed. On an iPhone 15 Pro Max
(2026-10-12) Boss Rally's races and demos showed 60 a second, and Top Gear
Rally's menus every frame the game draws (30 a second, the game's own rate).

```sh
SIMCTL_CHILD_RM_LOG=1 SIMCTL_CHILD_BR_HANDOFF_AUTOPILOT=1 xcrun simctl launch --console-pty DEVICE \
    com.strutb.tgrally --script $PWD/tools/tgrally/n64box_scripts/champ_race.txt --frames 100000000
```
