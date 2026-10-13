# Remastered lighting: measurements

Every number here comes from an instrument in the tree, with the command
that made it. Design: `DESIGN.md`.

## Phase 0: verification and the capture seam

### Facts settled from the code (2026-10-12)

- **Weather.** `BrRaceDifficultySet` (0x100627B0, `br_racestart.c`) sets four
  flags from the weather in the order Top Gear Rally's race set-up does
  (sunny, fog, rain, snow, night): 0 sunny, 1 fog, 2 rain (the storm), 3
  snow, 4 night. Night sets the night flag and not the rain flag: a dry
  night. The 32-bit lane's "4 = rain at night" note was wrong.
- **Reverse.** The physics has no gearbox (`br_cardrive.c`). Reverse is the
  car's control word bit 0x20000, which `br_carstep.c` turns into a negative
  drive torque (car+0xE68); a ghost or replay writes the negative torque
  itself (`br_carghostapply.c`). So: reverse lamps on while the drive torque
  is negative and the car is not moving forward; brake lamps as DESIGN.md
  2.3. The same for the player, the AI and ghosts.
- **The seam.** No handler wrappers are needed (the game itself rewrites
  table slots 4, 177 and 191 when the geometry mode changes, so wrappers
  installed once would be lost). Every display-list triangle, clipped or
  not, already tells `glide.c` its view before it is drawn (`br_dlview.h`),
  and arrives with its pixels and 1/w. The capture rebuilds the world point
  from them and the game's view x projection (`platform/light/light_capture.c`).
- **The backdrop.** The no-Z vertex routines set 1/w to 1/65535 after
  projecting (the painted sky and horizon). Those triangles have no world
  position; the capture keeps them apart as the backdrop.

### The capture, measured

`BR_VCLOCK=0.25 BR_HEADLESS=1 BR_LIGHT_CAPTURE=1 BR_LIGHT_STATS=1`, script
`20_quickrace_drive` (sunny), headless build `build/brally/light-null`:

| | |
|---|---|
| camera triangles a frame | 179 to 549 (median 520 on the opening straight, 232 later) |
| mirror triangles a frame | 0 (this camera has no mirror) |
| backdrop triangles a frame | 51 to 57 |
| largest reprojection error | 0.10 px (the rebuilt world point projected again, against where the game drew it) |
| world extent seen | x 811..1438, y 121..1175, z 0..26 (units) |

The game draws a few hundred triangles a frame: the scene is very light,
which is what DESIGN.md 8.1 counts on for the BVH.

![Frame 1,000: the game's frame, the captured triangles through its camera, and from above](phase0-capture.png)

*Frame 1,000 of `20_quickrace_drive` (`BR_LIGHT_DUMP=1000:f.obj`, a
software-renderer build, then `ports/brally/tools/lightviz.py`): the game's
frame; the 518 captured triangles drawn again through the game's camera, lit
flat by a test sun; the same triangles from behind and above the car.*

### The game never notices it

`ports/brally/tools/lightcheck.sh SCRIPT` (headless, `BR_VCLOCK=0.25`): the
script run with `BR_FLAG_LIGHTING=0` and `=1`, the game's data area dumped
every 100 frames, compared with `dumpdiff.py --state`.

| script | result |
|---|---|
| 20_quickrace_drive (sunny) | identical, 16 dumps |
| 27_quickrace_snow | identical, 17 dumps |
| 28_weather_rain | identical, 26 dumps |
| 29_weather_storm | identical, 26 dumps |
| 33_weather_fog | identical, 26 dumps |
| 38_views_keys | identical, 34 dumps |
| 21_quickrace_finish | identical except `g_aBrSfxChan` / `g_aBrSfxChanApplied` from frame 6600 |

The sound channels' records differ between two runs with the lighting OFF
in exactly the same place (frame 6600, the same two globals), so that is
existing run-to-run variation in the sound state, not the lighting.
`lightcheck.sh` leaves those two out and says so.

## Phase 1: first light (2026-10-12)

The Metal backend relights the camera's 3D view (`brr_metal.m`): each 3D
triangle writes a G-buffer beside the game's colour (albedo with the
vertex colour's brightness, the game's baked light, taken out; world
position; face normal), the frame's 3D triangles make a 2048^2 sun shadow
map, and one lighting pass computes the sun (with 3x3 filtered shadows),
the sky and the ground's light in lux, a GGX highlight, the camera's white
balance, an exposure metered from the frame's mean log luminance (EV100,
K = 12.5) and Khronos PBR Neutral. 2D, the mirror and the backdrop keep the
game's pixels. A headless Metal build (`HOST=null RENDER=metal`) takes the
shots without a window.

![Frame 1,000, Original and the new lighting](phase1-first-light.png)

*Frame 1,000 of `20_quickrace_drive`: Original (left), the new lighting
(right). The sun is the default direction, 19 degrees up, behind the gantry:
its shadow lies across the road.*

![Debug views](phase1-debug-views.png)

*`BR_LIGHT_VIEW` 1 albedo, 2 normals, 3 sun shadow, 4 the sun's angle on
each surface.*

| check | result |
|---|---|
| Original unchanged | frames 300, 1,000 and 3,000 byte-identical PNGs against the pre-lighting commit c8194344 (same Metal renderer, headless) |
| game state | `lightcheck.sh` with the lit Metal build: identical over `20_quickrace_drive` (16 dumps) |

Not yet: emissive lamps (the gantry's lights read as plain surfaces), the
sky (the game's painted backdrop is kept), shadows from what is off screen,
GPU timing.

## Phase 2: the cars' lamps (2026-10-12)

Every car in the race (`light_game.c` reads `g_aBrRaceDriver`: body frame,
velocity, drive and brake torque, control word, model extent) carries four
lamps (`light_scene.c`): low-beam headlights (about 20,000 cd, halogen
white, the UN ECE R112 asymmetric pattern in the shader) at night, in fog and
in rain; rear lamps (tail 5 cd red with the headlights, brake 80 cd, reverse
300 cd white) from the car's own brake and reverse state. Unshadowed for
now; the lamp positions are the faces of the model's extent (the lens faces
are the refinement).

Weather 4 is night: `28_weather_rain` runs it (`BR_LIGHT_STATS` prints
weather 4) and its frames are night (black sky, lit windows); the script's
"rain" comment is wrong.

Exposure: the lighting pass bins every 16th pixel's luminance into a
64-bin histogram; the mean log luminance between its 80th and 98.3rd
percentiles (Unreal Engine's auto-exposure defaults) sets EV100 (K = 12.5).
A plain mean failed at night: the moonlit dark pulled it to EV 1.4 and the
headlit road burned out; the histogram band meters EV 2.7 there. Night's
range is EV100 -2.5 to 7.5 (moonlight to a floodlit road).

![Sunny and night, Original and the new lighting](phase2-headlights.png)

## The lit frame as passes (2026-10-12)

Relighting the game's finished frame dropped or pasted back what the game
draws blended (the lit windows are alpha-blended decals over the walls: the
colour under each window is the wall's grey). A lit frame is now the game's
draws recorded and replayed (`brr_metal.m`, `brr_metal_lit.h`): G-buffer of
the opaque 3D; the sun's shadow map; HDR lighting with the game's
background (what it drew before its 3D) and painted backdrop behind; the
blended 3D forward-lit by the same lights in the game's order (additive
draws as emitted light); bloom (13-tap down, tent up, Jimenez 2014); post
(histogram exposure over lit surfaces only, Khronos PBR Neutral); then the
game's 2D, mirror and LFB writes over it as drawn. Glow is a texture
property decided when the texture is made (lamplight texels), about
100 cd/m2 at night. Each recorded draw keeps the texture object it was drawn
with (the game replaces its font pages within a frame).

Bugs the frames showed, each fixed at its cause: glyphs all the last font
page (texture identity, above); a black night (a sliver's zero normal made
NaN, which bloom spread over the frame: guarded normals, finite bloom
input); a dark exposure (the meter read the backdrop, whose level follows
the exposure: it reads lit surfaces only); the night's black background
fill drawn over the scene (draws before the first 3D are background now).

| check | result |
|---|---|
| Original unchanged | sunny and night frames 1,000 and 3,000 byte-identical to c8194344 |
| game state | `lightcheck.sh` with the lit Metal build, `28_weather_rain` (night): identical, 26 dumps |

![Sunny and night through the passes](phase2-passes.png)

## Sky, sun and atmosphere (2026-10-12)

The project lead chose the sun for Coastline (high, behind the start: option
B of `sun-options.png`) and a physical sky. `brr_metal_sky.h`: Hillaire 2020
(transmittance 256 x 64, multiple scattering 32 x 32, sky view 192 x 108,
Earth's Rayleigh, Mie and ozone constants), the sun's disc at 0.2675 degrees,
the sunlight reaching the ground and the sky's light on an up-facing
surface both read from the same atmosphere (a 3 x 1 reduction, read three
frames later), and the fog's colour the air's light at the horizon. The
weather sets the haze (Mie x1 sunny, x30 fog, x12 rain, x8 snow) and how much
sun the clouds let through. The game's painted backdrop stays in front: its
hills, coast and sea; its painted sky (blue or neutral in a texel's
neighbourhood, above the horizon) gives way. Daylight white balance is to
the sun's own light, so the sky stays blue.

Found on the way: the sky's rays came out upside down (the top of the screen
looked 24 degrees down): the game sets Glide's origin to the lower left, so
its viewport's rows run up; the capture uses the game's own coordinates
(right), the sky's rays now turn the target's rows back the same way.

| check | result |
|---|---|
| Original unchanged | sunny and night frames 1,000 and 3,000 byte-identical to c8194344 |
| game state | `lightcheck.sh`, `20_quickrace_drive`: identical |

![Sunny and night with the physical sky](phase2-sky.png)

## GPU time per pass (2026-10-12)

`BR_LIGHT_GPU=1` puts Metal timestamp counters on each lit pass's first
encoder (start of vertex work, end of fragment work) and prints the
milliseconds a frame each second. The GPU starts a pass while the one
before it is still shading, so a pass is timed from the later of its start
and the previous pass's end (the frame's first pass from the last frame's
end); the passes then add up to the frame's span. `BR_SIZE=WxH` sets a
headless run's target size.

Apple M4 Pro, headless, `BR_VCLOCK=0.25`, steady state, ms a frame:

| pass | sunny 1080p | night 1080p | sunny 4K | night 4K |
|---|---|---|---|---|
| sky tables | 0.07 | 0.07 | 0.07 | 0.07 |
| G-buffer | 0.31 | 0.36 | 0.98 | 1.14 |
| sun shadow | 0.02 | 0.02 | 0.02 | 0.02 |
| lighting | 0.62 | 0.46 | 2.40 | 1.74 |
| transparent | 0.02 | 0.04 | 0.03 | 0.07 |
| bloom down | 0.06 | 0.06 | 0.24 | 0.24 |
| bloom up | 0.06 | 0.06 | 0.12 | 0.12 |
| post | 0.13 | 0.14 | 0.48 | 0.48 |
| 2D | 0.04 | 0.03 | 0.07 | 0.07 |
| **total** | **1.34** | **1.25** | **4.42** | **3.96** |

At 640 x 480: 0.67 sunny, 0.66 night. The lighting pass is the largest and
grows with the pixels; the scene's few hundred triangles make the shadow
map almost free. The iPhone and tier budgets are measured on the devices
(each phone run needs the project lead's go-ahead).

| check | result |
|---|---|
| Original unchanged | sunny frames 1,000 and 1,600, night 1,000 and 3,000: byte-identical to c8194344 |

One of eight baseline runs started together drew night frame 3,000
differently; run again, the baseline and this build agree byte for byte.
That is run-to-run variation in the baseline under load, which the
Original check has to allow for by repeating a mismatch before reporting it.

## The whole track and the sun's cascades (2026-10-12)

**The track at load** (`light_game_track`, `light_game.c`): the track's
instance table (`g_brTrkHdr.aInstances`, 0x54-byte records, the table the
scene draw walks) gives each object's model matrix (an unrotated object,
flag 0x20 at +0x4D: its scale and position) and its display list at +0x44,
walked read-only (G_VTX, G_TRI1/2, G_DL, G_ENDDL) into world triangles in
the capture's space. Coastline: 673 objects (661 unrotated), 643 lists,
11,364 triangles, x -61..2039, y -10..2116, z -3..49.

Checked against what the camera draws (`BR_LIGHT_TRACKCHECK=1`: each captured
camera vertex tested for lying within 0.05 on a track triangle): 53% of the
camera's vertices lie on the track. The rest, at frame 1,000 of
`20_quickrace_drive` grouped into connected pieces (`BR_LIGHT_TRACKOBJ`,
`BR_LIGHT_DUMP`), are what the race draws itself: the twelve start-light
bulbs (3 x 4 hexagons of 18 faces, 1.5..3.6 up), the tyre trail and the cars.
Those stay this frame's captured casters.

**Cascaded sun shadows**: four 2048^2 cascades over the view to 300 m,
split at the practical-scheme distances (lambda 0.7), each fitted by its
slice's bounding sphere and moved in whole texels; reaching back toward the
sun to the track's extent, so what is behind or above the view casts (the
start gantry's beam across the road, frame 20,000 below). Bias slope-scaled:
one texel in the cascade's depth x (1 + tan theta), the receiver pushed one
to three texels along its normal; the first fit (a flat texel) showed acne
rings in `BR_LIGHT_VIEW=3`, this one none on the road and banks.

| check | result |
|---|---|
| Original unchanged | sunny 1,000 and 1,600, night 1,000 and 3,000: byte-identical to c8194344 |
| game state | `lightcheck.sh`, `20_quickrace_drive`: identical, 16 dumps |

Headless runs draw about 17 frames a race step, so a race frame is a high
renderer frame number: the green light is past frame 10,000, and frame
20,000 is 1.16 s into the race.

![Frame 20,000, 1080p: the gantry beam's shadow from off screen](phase3-cascades.png)
