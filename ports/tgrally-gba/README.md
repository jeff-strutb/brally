# Top Gear Rally on the Game Boy Advance (proof of concept)

A race of the N64 game, replayed on the GBA: the desert track as the game
draws it, from the recorded race's camera, with its music, its car sounds and
its race HUD. The hot code is ARM assembly in IWRAM; C is left for start-up
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

plus the cartridge itself (the music module, the dial's images).

```
D=build/tgrally/gba
TGR_WORLDDUMP=$D/desert.wd TGR_SNDDUMP=$D/desert.snd TGR_HUDSTATE=$D/desert.hst \
  build/tgrally/null-soft/tgrally --headless --shots /tmp/s --shot-at 99999999 \
  --script tools/tgrally/n64box_scripts/arc_desert_sunny_car0.txt
```

## Building

```
ports/tgrally-gba/build.sh RAM WORLD F0 F1 ROM SOUND HUD
```

`F0..F1` are the race's retraces to replay (3800 6047 for the desert run).
The tools convert the recordings (`tools/convert.py` the world,
`tools/sound.py` the sound, `tools/hud.py` the HUD) and the ROM is linked by
`tools/gbalink.py` with the host's clang (`--target=armv4t-none-eabi`).
With no arguments it rebuilds from the last conversion. `MAINDEF=-DRASTER_C`
and `MAINDEF=-DFRONT_C` build the C references in place of the assembly.

`host/build.sh` builds `gbarun`, a headless mGBA harness: screenshots, raw
video, a WAV of the sound, a per-address cycle profile, memory dumps, and a
held camera frame for comparisons.

## The pieces

| File | |
|---|---|
| `gba/front.s` | the visibility walk, culling (cells past the screen's edges, triangles facing away), the vertex transform, near-plane clip, perspective subdivision, the depth buckets |
| `gba/raster.s` | the textured and flat rasterizers and the bucket walk |
| `gba/sound.s` | the game's module player (BrModRowRead, BrModTick) and a 13379 Hz stereo mixer on the sound FIFOs, with the recorded effect voices |
| `gba/hud.s` | the race HUD (racehud.c) and the text printer under it (textstate.c), as hardware sprites |
| `gba/main.c` | start-up, the frame, and the C references |

Each assembly piece is checked to give pixel-identical screens to its C
reference on a set of held camera frames.
