# Top Gear Rally Remastered

Top Gear Rally's menus (or Boss Rally's, the player's choice) with Boss Rally's engine running the races, on macOS,
Windows, Linux and the iPhone. Nothing here is byte-matched; both games are the
native ports (`ports/tgrally`, `ports/brally`) built as libraries, with what
Remastered adds laid over them (`tree.sh`).

## How it fits together

| piece | what |
|---|---|
| `tree.sh` | the two ports as Remastered builds them: each port, Remastered's own files over it (`brally/`, `tgrally/`, at the paths they take in the port), its changes to the port's files applied (`patch/brally`, `patch/tgrally`, one unified diff a file, no fuzz). `make ROOT` builds a tree, `save ROOT` turns an edited tree back into files and patches, `check ROOT` says whether it would. A patch that stops applying stops the build: the port moved under it |
| `brally/`, `patch/brally` | Boss Rally as Remastered has it: the race that fills any window, the races run for Top Gear Rally's menus (`race_handoff.c`, `host_race.h`), split screen and the players' controls (`race_pads.c`), Top Gear Rally's Rumble Pak (`rm_rumble.c`), the pause menu's Remaster Options and frame rate (`pause_options.c`), its own menus as the app's (`race_handoff.c`, `br_menus_run`), their Remaster Options page (`src/core/menus/rm_menuopts.cpp`, `rm_menuicons.c`) and their 16:9 layout (`src/core/drawing/rm_menuwide.c`), the music hooks (`audio.c`) |
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
| `tools/` | `mixcheck.sh`, the music's routing in `mux.c` checked against computed values; `paintcheck.py`, the paint shop by pointer, headless (below) |
| `ios/` | the iPhone app: `build.sh`, the xcodegen project, and in `Sources` the Swift host under the multiplexer (the Metal layer, events, Core Audio, the save folder) and the touch controls |
| `icon.png` | the app's icon (the `.icns` on macOS, `icon.ico` beside the exe on Windows) |

Each race is handed to Boss Rally (`host_race.h`,
`tgrally/src/racing/handoff.c`, `brally/platform/common/race_handoff.c`). There
is no loading screen: the track is loaded in the race's first frames (under a
tenth of a second), which fade in from black (`br_boot.c`, its loading state);
the seconds before the start are the race's own fly-by and countdown. Boss
Rally's replay after a finish is not run (`race_handoff.c`): the race goes back
to the menus that started it.

**Remaster Options.** Top Gear Rally's options screen has them as rows of its
carousel after Units, each with its own turning icon (Widescreen the PC's
monitor, 4:3 when off and 16:9 when on; Music the monitor for the PC's
soundtrack or the console for the N64's; Show FPS Time Attack's stopwatch;
View Distance the Coastline course's own, its lighthouse included):
up or down (a swipe on the iPhone) or A takes the next choice
(`tgrally/src/menus/rmoptions.c`). Boss Rally's pause menu has a Remaster
Options row before its quits. It opens a page of that name in the menu's own style: one
row an option, its choices side by side, the one in force in the row's
colour and the others faded, then Back. Up and down move, left and right
step through the choices at once; Return takes the next, and Back or Escape returns to
the pause menu. A tap or a click chooses a row or a choice. The choices go to
both games and are kept (`platform/options.c`, `remaster.cfg` in the save
folder; `RM_OPT_<NAME>=N` sets one for a check):

| option | choices | default |
|---|---|---|
| Widescreen | On: a race and the menus fill the window (`TGR_FLAG_ANY_ASPECT`, `BR_FLAG_ANY_ASPECT`); Off: the games' 4:3, boxed | On |
| Music | Boss Rally: its CD; TGR (USA): Top Gear Rally's pieces with the USA cartridge's front-end tune; TGR (PAL/Japan): the same with Europe's and Japan's (one tune; the race pieces are the same in all three) | TGR (USA) |
| Show FPS | the frames a second at the bottom centre, in either game | Off |
| View Distance | Unlimited: a race sees the whole world, nothing pops in as it comes into range (`BR_FLAG_VIEW_UNLIMITED`, `rm_view.h`); Original: the game's 400 | Unlimited |
| Menus | TGR (USA), Boss Rally, or TGR (PAL/Japan): Top Gear Rally's as Japan's cartridge has them (below) | TGR (USA) |

**Regions.** Remastered builds Top Gear Rally twice: from the USA cartridge
and from Japan's (`link.sh`: the second library's entry points `tgrp_*`,
`platform/rm_tgrp.h`; it needs both ROMs). Menus TGR (PAL/Japan) is Japan's:
its title (the start screen, as Japan has it), its pictures, its cars' real
liveries, its Load/Save with the cartridge's EEPROM (Game Pak) rows beside
the Controller Pak's. Its text is English: where Japan's screens print
Japanese the library is built with Europe's English (`TGR_RM_EN`: the text
arms of Japan's source take Europe's, layout and all), but for the weather
symbols beside a course's name, which are pictures. The two libraries keep
their own state (every file built `-fno-common`, so no variable is shared at
the link) and one Controller Pak file: each reads it again when the other has
written it (`platform/os/si.c`, `pak_fresh`). Each library carries what it
takes from the other cartridge at ROM 0x800000 (`tools/regionextras.py`): its
front-end tune, and its three cars. Japan replaced Type-RS, Type-M3 and the
Helmet with Type-PJ, Type-TR and Type-MN; Remastered offers both of each pair,
the other cartridge's beside this one's in the car select and unlocked with
it (`tgrally/src/menus/rmregion.c`), raced as Boss Rally's own (its cars 3, 6
and 9, and 13, 14 and 15).

**Cheats.** Europe's and Japan's two, in both libraries and on the first
player's controls (above all what each N64 button does): on the main menu,
Gear Up and Gear Down held and Change View pressed three times opens every
car, course and weather (`tgrally/platform/os/rmcheat.c`); in a race, Gear Up,
Gear Up, Steer Right, Accelerate, Change View, Steer Right finishes it there,
the first player first and the second second (`race_handoff.c`, `finish_now`).

**Menus.** Top Gear Rally's options screen has a Menus row after Show FPS
(the console its icon); up goes to the other Top Gear Rally's, down to Boss
Rally's. The other Top Gear Rally's run while this one's game thread waits,
and come back the same way (`main.c`, `other_from_usa`); the wait does not
count as menu time (`handoff.c`, `clock_resume`). Boss Rally's puts its own
front end in front:
its main menu, its championship, time attack and quick race, its own races,
while Top Gear Rally's game thread waits as it does for a race
(`tgr_menus_away`, `main.c` `menus_engine`, `br_menus_run`), its music
playing on.  Boss Rally's Options has a Remaster Options row before Back,
opening a page in its own style: the wrench top left, a row an option saying
the choice in force (Widescreen, Music, Show FPS, View Distance, Menus), Back, and top right
the machine whose music plays (the PC or the N64; with View Distance the row
chosen, Race Setup's picture of Coastline, whole at Unlimited and without its
lighthouse end at Original).  Return or a click takes
a row's next choice; Menus there hands back to the Top Gear Rally that
handed over, on its options screen.  Kept in `remaster.cfg`: launched with Boss Rally's menus, Top Gear
Rally goes straight to its main menu (no intro) and hands over on its first
frame, which is where the player comes back to.  In Boss Rally's menus:
its races are driven with the app's controls (`race_pads.c`, as the handed
races are); its Quit, and its pause menu's Quit Game, close the app as the
window's close does, after its own saves (`rm_quit`); a touch on the iPhone
clicks where it lands. On the iPhone, where an app does not close itself and
one screen is in one pair of hands, its main menu has neither Multiplayer nor
Quit and its pause menu no Quit Game (`rm_phone`; `RM_PHONE=1` shows them so
for a check).

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
The menus' screen camera sees as much more as the window shows (`view.c`,
`tgr_view_menu_lens`), so a carousel item slides in and out from the window's
own edges. A screen change's wipe draws its bars whole every frame, from the
screen's edges (`drawing/screenflash.c`): the cartridge drew only each frame's
new strip and left the rest in its frame buffers, which a window wider than
4:3 placed apart from the wipe.
The renderer hashes the texture memory once a load rather than once a
primitive (`rcp.c`, `tile_texture`): the icons are thousands of one-colour
triangles, and the console's 8000 hashed 4 KB each, some 30 ms a frame.

**Two players.** The 1P/2P screen offers two players on a desktop (a keyboard
is there; the iPhone has no 1P/2P screen: Arcade and Practice go straight to
the course select with one player), and the race goes to
Boss Rally's engine split screen. In a 4:3 window the two views sit one above
the other with the HUD beside them, as the N64 lays them out; with Widescreen
on they sit side by side, each filling its half, each with its own HUD
(`br_framebegin.c`, `br_framedrive.c`, `br_gfx.c`: the HUD's draws for a view are
marked in the display list, `race_pads.c`, and placed in that view). Both views
draw the sky, where the original filled them flat (black under a clear sky).
`race_pads.c` keeps the second view's record beside each frame's snapshot,
which held one; Boss Rally's own PC game never raced two people on one screen.
Each player drives their own car with their own transmission, tyres and
suspension, chosen on Top Gear Rally's set-up screens. Both outcomes come back
to the results.

**Quit.** On a desktop Top Gear Rally's main menu has Quit after Options, the
console its icon (`tgrally/src/menus/rmmainmenu.c`): the app closes as its
window's close does (`rm_quit`), as Boss Rally's menus' Quit does. The
cartridge has none, and neither has the iPhone.

**Controls.** One set for both games: each action is one N64 button, the same
in Boss Rally's races and in Top Gear Rally's menus (Accelerate / Select is A,
Brake / Back B, Gear Up R, Gear Down L, Change View C-up, Pause / Start Start,
Up, Down and the steering the stick's four ways, Z and the other C buttons the
menus' own, Reverse A with the stick back). The options screen's controller
row opens Controls: a Player row (up, down or A: whose controls), a row an
action showing its key and its pad button (A, then the next key or button
pressed sets it, taken from any other action that had it; Escape leaves it),
and Reset (`tgrally/src/menus/rmcontrols.c`, `platform/controls.c`). The
first player drives with the keyboard and the first pad (on the iPhone, the
touch controls), the second with the keyboard and the second pad; the left
stick always steers. Defaults, no key doing two things: player 1 X
accelerates and selects, Z brakes and goes back, C reverses, the arrows steer
and move, W and Q change gear, I the view, K, J and L the other C buttons,
Space Z, Escape pauses; player 2 A, S and D, F and H steer, T and G up and
down, R and E gears, V the view, P pauses; on a pad A accelerates, B brakes,
X reverses, the d-pad steers and moves, RB and LB change gear, Y the view,
Menu pauses, LT Z. A file of the first controls (before version 2) is set
aside for these. Boss Rally's own screens that read keys (its menus, the
pause menu) are given the same controls (`race_handoff.c`, `pad_keys`).

**Prompts.** Every button the screens show is the first player's own: the
foot of a menu and a popup's buttons draw a key cap in the cartridge's
colours (A blue, B green) naming the key, or the pad button when a pad was
used last, and a line that says "press A" names it too
(`tgrally/src/menus/rmprompt.c`; Boss Rally's "Press ESC to exit" the same).
On the iPhone the prompts are touch's: the words are buttons to tap, a line
says tap or swipe (`RM_PROMPT=key|pad|touch` for a check). In a race Change View
steps through the views from the one on the screen: the car's own (where a race
starts), the chase view, then (one player on the screen) the bonnet, and round
again (`race_pads.c`).

**Rumble.** Each player's pad shakes as Top Gear Rally's Rumble Pak did, in
Boss Rally's races too (`brally/platform/common/rm_rumble.c`). Boss Rally's
sound step is Top Gear Rally's carried to the PC: it still starts the surface
impacts' effects into Top Gear Rally's per-player pulse state, and its three
knocks (two cars colliding, the body's hardest hit, a wheel landing) are the
same bytes on the car, written with the same values. The sound step starts
Top Gear Rally's three knock effects there, and a thread runs
`BrRumbleUpdate`'s pulses every 10 ms, the tick of the mixer timer that ran
them on the N64: so many hundredths of a second on, so many off. Paused, in a
replay or the attract demos, in the season's ceremony and out of a race the
motors are off. The motor is the pad's heavy one at full strength, on or off
(XInput on Windows, Core Haptics on the Mac and the iPhone); on the iPhone the
touch controls have none, so only a game controller rumbles.

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

**Paint shop.** The mouse works it beside the pad, and on the iPhone a
finger (`tgrally/src/menus/rmpaintptr.c`, the pointer in
`tgrally/platform/os/touch.c`). The cursor goes where the pointer is (the
stick still moves it); the left button or the finger is A and the right
button B, given to the game's pad record with the cursor already placed, so
a click acts where it lands, and a press the game has acted on is not given
again until the button comes up. The brush paints while held, every texel on
the way however fast it moves; the line, rectangle and oval tools take two
clicks as before or a drag (the button coming up is the second click); B on
the decal picks the colour under it and elsewhere cancels. In a popup a
click chooses a choice and a second click on the chosen one takes it, the A
and B button pictures (with their words) are those buttons, the right button
is B, and the colour mixer's bars are dragged. On the iPhone a finger
presses after 60 ms or once it moves, so a second finger first makes a
two-finger tap: B where the first landed. `RM_POINTER=F:u,v,b;...` plays
the pointer in headless (u, v fractions of the screen, b the buttons, from
retrace F), `RM_PAINTLOG=1` logs each paint shop frame and `RM_PAINTDUMP=F`
prints the decal's texels; `tools/paintcheck.py` drives a brush stroke, a
dragged line, a style popup, B, the mixer and a pad-only session with them
and checks the texels and the state against the shop's own geometry.

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
| paint shop | the finger is the pointer: the cursor where it is, held it is A (paint, drag out a line or a shape, tap a tool, a colour or a popup's choice); a two-finger tap is B where the first finger landed |
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
each game drew and those the screen showed.

**60 ticks a second.** Boss Rally's races (the driving, the replays, the
demos, the ceremony) step 60 times a second where the game stepped 30
(`brally/include/rm_tick.h`, `race_handoff.c`: `RM_TICK=30` gives the game's
own, value for value). What the game wrote in terms of its 30 goes through
`rm_tick.h`: counts in ticks, shares taken each tick, steps added each tick;
its grip solver (`br_cardrive.c`), a projection sized for a 30th of a second,
runs 30 times a second as it did. A recording read from a file the game made
(the CD's intro, credits and ending, a PC ghost: "RGho") plays half a frame a
tick, blended; Remastered's own ghosts say "RG60". A network race runs at 60
too (its clock and the ticks its packets carry: `rm_ms_to_ticks`). Drawing is the game's own, a picture blended
between the last two ticks at whatever rate the display takes. Top Gear
Rally's menus run a frame a retrace (60 a second; they move by the time a
frame took, so as fast as they did at 30), and the pictures between those
frames are its display lists run again with their matrices, rectangles and
scissors blended toward the frame before (`tgrally/platform/gfx/rcp.c`,
`os/thread.c`; `RM_TWEEN=0` draws each frame once). `RM_TICK=30` runs Boss
Rally's races at the game's own 30 (a check against the original). `BR_TICKLOG=1` writes
each race tick's clock and cars, for comparing two runs.

```sh
SIMCTL_CHILD_RM_LOG=1 SIMCTL_CHILD_BR_HANDOFF_AUTOPILOT=1 xcrun simctl launch --console-pty DEVICE \
    com.strutb.tgrally --script $PWD/tools/tgrally/n64box_scripts/champ_race.txt --frames 100000000
```
