# Top Gear Rally on the Game Boy Advance (proof of concept)

The N64 game's main menu and a race, on the GBA: the menu's carousel of
rows with their turning, lit icons, its music and sounds; and the desert
track as the game draws it, replayed from the recorded race's camera, with
its music, its car sounds and its race HUD. The hot code is ARM assembly in IWRAM; C is left for start-up
and the frame's bookkeeping, and as the reference each assembly piece is
checked against.

## What it is made from

Everything comes from the game, through the native port (`ports/tgrally`)
running a recorded race:

| Recording | Port switch | What it holds |
|---|---|---|
| world | `TGR_WORLDDUMP=FILE` | the triangles as drawn, the camera each frame, the textures |
| memory | `TGR_RAMDUMP=FILE@FRAME` | game memory in the race: the track's objects, the playing module, the text printer's tables and font |
| sound | `TGR_SNDDUMP=FILE` | the effect voices each retrace, the music player's state |
| HUD | `TGR_HUDSTATE=FILE` | what the race HUD reads each frame, and the car's dial |

plus the cartridge itself (the music modules, the dial's images, the menu's
sounds).  The menu comes from the same two recordings, the world and the
memory, taken on the main menu while `scripts/menu_tour.txt` turns to every
row:

```
TGR_WORLDDUMP=$D/menu.wd TGR_RAMDUMP=$D/menu.ram@880 \
  build/tgrally/null-soft/tgrally --headless --shots /tmp/s --shot-at 99999999 \
  --script ports/tgrally-gba/scripts/menu_tour.txt
```

```
D=build/tgrally/gba
TGR_WORLDDUMP=$D/desert.wd TGR_SNDDUMP=$D/desert.snd TGR_HUDSTATE=$D/desert.hst \
  build/tgrally/null-soft/tgrally --headless --shots /tmp/s --shot-at 99999999 \
  --script tools/tgrally/n64box_scripts/arc_desert_sunny_car0.txt
```

## Building

```
ports/tgrally-gba/build.sh RAM WORLD F0 F1 ROM SOUND HUD MENU_RAM MENU_WORLD
```

`F0..F1` are the race's retraces to replay (3800 6047 for the desert run).
The tools convert the recordings (`tools/convert.py` the world,
`tools/sound.py` the sound, `tools/hud.py` the HUD, `tools/menu.py` the
menu, with `tools/models.py` reading the game's models) and the ROM is linked by
`tools/gbalink.py` with the host's clang (`--target=armv4t-none-eabi`).
With no arguments it rebuilds from the last conversion. `MAINDEF=-DRASTER_C`,
`MAINDEF=-DFRONT_C` and `MAINDEF=-DMENU_C` build the C references in place of
the assembly; `MAINDEF=-DMENU_FIXED_DT=N` holds the menu at its Nth frame (a
row turned at the 3rd), so two builds can be compared.

`host/build.sh` builds `gbarun`, a headless mGBA harness: screenshots, raw
video, a WAV of the sound, a per-address cycle profile, memory dumps, a held
camera frame for comparisons, and the pad pressed on given frames
(`GBARUN_KEYS`).

## Playing it

It starts on the main menu.  Left and right turn the carousel; A or START on
Championship, Arcade, Time Attack or Practice starts the race (the rows the
proof of concept does not have, Paint Shop, Load/Save and Options, stay put).
In the race, B or START goes back to the menu.

## The pieces

| File | |
|---|---|
| `gba/front.s` | the visibility walk, culling (cells past the screen's edges, triangles facing away), the vertex transform, near-plane clip, perspective subdivision, the depth buckets |
| `gba/raster.s` | the textured and flat rasterizers and the bucket walk |
| `gba/sound.s` | the game's module player (BrModRowRead, BrModTick) and a 13379 Hz stereo mixer on the sound FIFOs, with the recorded effect voices |
| `gba/hud.s` | the race HUD (racehud.c) and the text printer under it (textstate.c), as hardware sprites |
| `gba/menu.s` | the menu's icons: each vertex to the screen and lit, each triangle facing the camera to the depth buckets with its texture at its brightness |
| `gba/menu.c` | the menu (mainmenu.c BrMainMenu on frontbuttons.c BrMenu): the carousel, the pad, the sounds, the screen wipe (BrFadeStep), the text as sprites |
| `gba/main.c` | start-up, the menu and the race in turn, the frame, and the C references |

Each assembly piece is checked to give pixel-identical screens to its C
reference on a set of held camera frames.
