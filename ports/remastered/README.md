# Top Gear Rally Remastered

Top Gear Rally's menus (or Boss Rally's, the player's choice) with Boss Rally's engine running the races, on macOS,
Windows, Linux and the iPhone. Nothing here is byte-matched; both games are the
native ports (`ports/tgrally`, `ports/brally`) built as libraries, with what
Remastered adds laid over them (`tree.sh`).

## How it fits together

| piece | what |
|---|---|
| `tree.sh` | the two ports as Remastered builds them: each port, Remastered's own files over it (`brally/`, `tgrally/`, at the paths they take in the port), its changes to the port's files applied (`patch/brally`, `patch/tgrally`, one unified diff a file, no fuzz). `make ROOT` builds a tree, `save ROOT` turns an edited tree back into files and patches, `check ROOT` says whether it would. A patch that stops applying stops the build: the port moved under it |
| `brally/`, `patch/brally` | Boss Rally as Remastered has it: the race that fills any window, the races run for Top Gear Rally's menus (`race_handoff.c`, `host_race.h`), split screen and the players' controls (`race_pads.c`), the pause menu's Remaster Options and frame rate (`pause_options.c`), its own menus as the app's (`race_handoff.c`, `br_menus_run`), their Remaster Options page (`src/core/menus/rm_menuopts.cpp`, `rm_menuicons.c`) and their 16:9 layout (`src/core/drawing/rm_menuwide.c`), the music hooks (`audio.c`); shelved, built but off: the lighting (`platform/light`, the Metal renderer's lit passes) and the textures (`light_surf.c`, `brr_metal_surf.*`, the sets in `ports/common/surfaces`) |
| `tgrally/`, `patch/tgrally` | Top Gear Rally as Remastered has it: the race that fills any window (`view.c`, `gfx/rcp.c`), the hand-over of the races and of the menus (`src/racing/handoff.c`), touch (`os/touch.c`), the frame rate (`drawing/frameloop.c`), the music's mute (`audio/mixer.c`) |
| `platform/options.c` | the Remaster Options: kept in the save folder (`remaster.cfg`), applied to both games |
| `tools/menuicons.py` | Boss Rally's Remaster Options page's two machines, into `brally/src/core/menus/rm_menuicons.c`: the PC from `ports/common/assets/retro_pc.svg`, the N64 the hand-built console drawn in that drawing's oblique, its greys a real console's (`ports/common/assets/n64console.png`) |
| `platform/controls.c` | the two players' controls: kept in the save folder (`controls.cfg`), read by both games |
| `tools/handicons.py` | the options screen's monitor and console icons, built by hand in a few hundred triangles of flat colour as the game's own icons are, their sizes, heights and colours measured off renders of a monitor and a console model, into `tgrally/platform/os/rm_icons.c` |
| `ports/tgrally/link.sh`, `HOST=lib` | Top Gear Rally as `libtgrally.a`, its `main()` as `tgr_main` |
| `ports/brally/link64.sh`, `HOST=lib` | Boss Rally as `libbrally.a`, its `main()` as `br_main` |
| `platform/thunks.c` | each game's calls to the host for the window, input, sound and folders, sent on to `mux_tgr_*` or `mux_br_*` |
| `platform/mux.c` | the one real host (`host/macos`, `host/windows`, `host/null`, the iPhone's Swift) shared between the two |
| `platform/main.c` | the process: Top Gear Rally on the main thread (on the iPhone, `rm_main` on a thread of the app's), Boss Rally started on its own thread at the first race |
| `link.sh` | makes the tree (`OUT/tree`), builds both libraries in it, makes each one object (its thunks inside, only its entry points global: the two games share some hundred function names), links the executable |
| `package_app.sh` | `Top Gear Rally Remastered.app` (macOS) |
| `music.sh` | Boss Rally's CD audio as FLAC, the PC's music, into the app (`package_app.sh`, `ios/build.sh`) |
| `tools/` | the lighting's tests (`lighttest/`, run against `build/remastered/tree`; `surfparity.sh` the textures' surface maps against `texregion.py`), `lightcheck.sh`, `texmat_h.py`; `mixcheck.sh`, the music's routing in `mux.c` checked against computed values |
| `ios/` | the iPhone app: `build.sh`, the xcodegen project, and in `Sources` the Swift host under the multiplexer (the Metal layer, events, Core Audio, the save folder) and the touch controls |
| `icon.png` | the app's icon (the `.icns` on macOS, `icon.ico` beside the exe on Windows) |

Each race is handed to Boss Rally (`host_race.h`,
`tgrally/src/racing/handoff.c`, `brally/platform/common/race_handoff.c`).

**Remaster Options.** Top Gear Rally's options screen has them as rows of its
carousel after Units, each with its own turning icon (Widescreen the PC's
monitor, 4:3 when off and 16:9 when on; Music the monitor for the PC's
soundtrack or the console for the N64's; Show FPS Time Attack's stopwatch):
up or down (a swipe on the iPhone) or A takes the other choice
(`tgrally/src/menus/rmoptions.c`). Boss Rally's pause menu has a Remaster
Options row before its quits. It opens a page of that name in the menu's own style: one
row an option, its two choices side by side, the one in force in the row's
colour and the other faded, then Back. Up and down move, left and right
choose at once; Return takes the other choice, and Back or Escape returns to
the pause menu. A tap or a click chooses a row or a choice. The choices go to
both games and are kept (`platform/options.c`, `remaster.cfg` in the save
folder; `RM_OPT_<NAME>=N` sets one for a check):

| option | choices | default |
|---|---|---|
| Widescreen | On: a race and the menus fill the window (`TGR_FLAG_ANY_ASPECT`, `BR_FLAG_ANY_ASPECT`); Off: the games' 4:3, boxed | On |
| Music | PC: Boss Rally's CD; N64: Top Gear Rally's pieces | N64 |
| Show FPS | the frames a second at the bottom centre, in either game | Off |
| Menus | Top Gear Rally's or Boss Rally's (below) | Top Gear Rally |

**Menus.** Top Gear Rally's options screen has a Menus row after Show FPS
(the console its icon); taking it puts Boss Rally's own front end in front:
its main menu, its championship, time attack and quick race, its own races,
while Top Gear Rally's game thread waits as it does for a race
(`tgr_menus_away`, `main.c` `menus_engine`, `br_menus_run`), its music
playing on.  Boss Rally's Options has a Remaster Options row before Back,
opening a page in its own style: the wrench top left, a row an option saying
the choice in force (Widescreen, Music, Show FPS, Menus), Back, and top right
the machine whose music plays (the PC or the N64).  Return or a click takes
a row's other choice; Menus there hands back to Top Gear Rally's options
screen.  Kept in `remaster.cfg`: launched with Boss Rally's menus, Top Gear
Rally goes straight to its main menu (no intro) and hands over on its first
frame, which is where the player comes back to.  In Boss Rally's menus:
its races are driven with the app's controls (`race_pads.c`, as the handed
races are); its Quit, and its pause menu's Quit Game, close the app as the
window's close does, after its own saves (`rm_quit`; on the iPhone, where an
app does not close itself, they go back instead); a touch on the iPhone
clicks where it lands.

At Widescreen Boss Rally's menus fill the window: their picture is as wide
as the window at the game's height (854 at 16:9, up to twice 640), each
page's 640 columns centred in it, the pointer free across it.  The backdrop
every page shares is widened, not boxed: its two vertical bars are carried
on across the width at their own pitch (157) and alternating as they do,
only on the black that reaches the picture's edge, so nothing of the logo,
the panel or a screw changes; at 852x480 the centre 640 columns are the
4:3 picture's to the pixel but for those bars (`rm_menuwide.c`).

Top Gear Rally's menus at Widescreen keep their 4:3 layout at its size in the middle of
the window; a picture across the whole screen (a menu's background) is scaled
up to cover the window, and the title's 3D backdrop, which already reaches past
the 4:3 edges, simply shows more (`tgrally/platform/gfx/rcp.c`, `place_menu`).
The renderer hashes the texture memory once a load rather than once a
primitive (`rcp.c`, `tile_texture`): the icons are thousands of one-colour
triangles, and the console's 8000 hashed 4 KB each, some 30 ms a frame.

**Shelved: the lighting and the textures.** Their code stays and is built, but
they are always off and no menu offers them (`platform/options.c`; the
`lighting` and `textures` lines of `remaster.cfg` are ignored); the packaging
leaves their skies and texture sets out. `RM_OPT_LIGHTING=1` and
`RM_OPT_TEXTURES=1` (`BR_FLAG_LIGHTING=1`, `BR_FLAG_TEXTURES=1` for Boss Rally
alone) still turn them on for their checks, and `RM_SHELVED_ASSETS=1` packages
their assets (`package_app.sh`, `ios/build.sh`, `builder/build.sh`).

**Two players.** The 1P/2P screen offers two players (on a desktop always: a
keyboard is there; on the iPhone with a game controller), and the race goes to
Boss Rally's engine split screen. In a 4:3 window the two views sit one above
the other with the HUD beside them, as the N64 lays them out; with Widescreen
on they sit side by side, each filling its half, each with its own HUD
(`br_framebegin.c`, `br_framedrive.c`, `glide.c`: the HUD's draws for a view are
marked in the display list, `race_pads.c`, and placed in that view). Both views
draw the sky, where the original filled them flat (black under a clear sky).
`race_pads.c` keeps the second view's record beside each frame's snapshot,
which held one; Boss Rally's own PC game never raced two people on one screen.
Each player drives their own car with their own transmission, tyres and
suspension, chosen on Top Gear Rally's set-up screens. Both outcomes come back
to the results.

**Controls.** The options screen's controller row opens Controls: a Player row
(up, down or A: whose controls), a row an action (Accelerate, Brake, Reverse,
Steer Left, Steer Right, Gear Up, Gear Down, Change View) showing its key and
its pad button (A, then the next key or button pressed sets it; Escape leaves
it), and Reset (`tgrally/src/menus/rmcontrols.c`, `platform/controls.c`). The
first player drives with the keyboard and the first pad (on the iPhone, the
touch controls), the second with the keyboard and the second pad; the left
stick always steers. Defaults: player 1 Boss Rally's keys (arrows, right Ctrl
brakes, A and Z gears, Delete the view), player 2 I accelerates, K reverses,
J and L steer, O brakes, U and M gears, Y the view; on a pad A accelerates,
B brakes, X reverses, RB and LB change gear, Y the view. In a race Change View
steps through the views from the one on the screen: the car's own (where a race
starts), the chase view, then (one player on the screen) the bonnet, and round
again (`race_pads.c`).

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
reach the race they name here: the app's menus run to other timings, so their
timed presses land elsewhere, often in Practice, which never finishes.)
`RM_SCANS=F:S,...` holds key S (a scan code, hex) for four frames at frame F of a
scripted run (the Controls screen's checks).

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
shape, menus fill it around their 4:3). Touch drives the game
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
