# Top Gear Rally Remastered

Top Gear Rally's menus with Boss Rally's engine running the races, on macOS,
Windows, Linux and the iPhone. Nothing here is byte-matched; both games are the
native ports (`ports/tgrally`, `ports/brally`) built as libraries.

## How it fits together

| piece | what |
|---|---|
| `ports/tgrally/link.sh`, `HOST=lib` | Top Gear Rally as `libtgrally.a`, its `main()` as `tgr_main` |
| `ports/brally/link64.sh`, `HOST=lib` | Boss Rally as `libbrally.a`, its `main()` as `br_main` |
| `platform/thunks.c` | each game's calls to the host for the window, input, sound and folders, sent on to `mux_tgr_*` or `mux_br_*` |
| `platform/mux.c` | the one real host (`host/macos`, `host/windows`, `host/null`, the iPhone's Swift) shared between the two |
| `platform/main.c` | the process: Top Gear Rally on the main thread (on the iPhone, `rm_main` on a thread of the app's), Boss Rally started on its own thread at the first race |
| `link.sh` | builds both libraries, makes each one object (its thunks inside, only its entry points global: the two games share some hundred function names), links the executable |
| `package_app.sh` | `Top Gear Rally Remastered.app` (macOS) |
| `ios/` | the iPhone app: `build.sh`, the xcodegen project, and in `Sources` the Swift host under the multiplexer (the Metal layer, events, Core Audio, the save folder) and the touch controls |
| `icon.png` | the app's icon (the `.icns` on macOS, `icon.ico` beside the exe on Windows) |

The remastered flags work as in the two ports. With Top Gear Rally's
Remastered profile (the default) `TGR_FLAG_BR_RACES` hands each race to
Boss Rally (`host_race.h`, `ports/tgrally/src/racing/handoff.c`,
`ports/brally/platform/common/race_handoff.c`) and `TGR_FLAG_ANY_ASPECT`
fills any window shape; the window opens at 16:9, and its race fills it
while the menus keep the N64's 4:3 inside it. Tab (or a pad's View button)
switches Top Gear Rally to the Original profile: its own races, 4:3. During a
Boss Rally race, Tab is Boss Rally's own Toggle Remaster.

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
Landscape only; the games draw into a 16:9 area centred on the screen
(Remastered: a race fills it, menus keep 4:3 inside it). Touch drives the game
in front (`mux_front_game`), and the fingers on the glass are let go when the
other comes in front:

| | |
|---|---|
| in a race | a finger right of the brake zone holds the accelerator; sideways steers (full lock 90 points either way, the neutral point sliding with the finger past it), a wheel drawn under the finger turning with it; lift to coast. A finger on the left 30% of the screen (a pedal is drawn there) brakes and, the car stopped, reverses; it lets the accelerator go while held. In Boss Rally's races it is that game's reverse (the gamepad's button 2), which drives backwards at any speed, slowing the car first; in this game's own it is B until the car is below 3 km/h (`tgr_race_speed`), then A with the stick pulled back (this game reverses only from first gear) |
| menus, pause menu | tap an item: the carousel's arrows step it, the item between them is A, a list row moves the cursor there and chooses it, the button prompts (Select, Go Back, OK, Cancel, Exit) press their button; a tap on a screen with nothing to choose is A. Swipe: the content follows the finger (left brings on the next item, up the row below); a value row (a volume) goes up with the finger |
| anywhere | two-finger tap: START; three-finger tap: the sound's low-pass filter, off / 8 / 7 / 6 / 5 kHz (5 at first, kept between runs; `TGR_AUDIO_LOWPASS=Hz` on the desktop) |

In Boss Rally's races its pause menu reads the keyboard: a swipe is an arrow
key (the same way round), a tap Return, a two-finger tap Escape. The menus name
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
