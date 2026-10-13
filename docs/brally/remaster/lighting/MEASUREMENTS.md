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

## The track for rays: BVH and acceleration structure (2026-10-12)

DESIGN.md 8.1's world representation, built and verified without running
the game. `platform/light/light_bvh.c` builds a bounding volume hierarchy
over `light_game_track()`'s triangles: binned SAH (16 bins a axis,
traversal and triangle cost 1, leaves of at most 4), 32-byte nodes stored
depth first (an interior node's first child follows it, so the traversal
stacks only the far child), the triangles copied in leaf order with their
track numbers beside them, and a 16-byte surface record a triangle (object;
material, albedo and emission left zero for the materials work).
`render/metal/brr_metal_rt.m` uploads it as four buffers and, where the
device has `supportsRaytracing`, builds a primitive acceleration structure
over the same triangles in track order (compacted after the build), so both
paths name a triangle by the same number. `brr_metal_rt.h` holds the Metal
traversal (`rt_closest`, `rt_any`) and the intersector's
(`rt_hw_closest`, `rt_hw_any`), and `brr_rt_library` compiles them without
fast math (the slabs need IEEE infinities). The renderer exposes the world
as `brr_metal_rt_world()`, built the first time a pass asks for it on a
track; nothing traces rays yet.

The triangle test is Woop, Benthin and Wald's watertight test with one
change, found by the brute-force comparison: in plain float a small, thin
triangle far from the ray can lose two of its edge functions to
cancellation, and a ray 100 m away "hit" a millimetre sliver (the soup
scene, 2 rays in 20,000). The paper falls back to double precision for an
exact zero; the GPU has none. The edge functions are instead computed as a
difference of products with Kahan's algorithm (one fma each side), whose
relative error is at most two units in the last place (Jeannerod, Louvet
and Muller 2013): the sign is the exact sign and zero only when the exact
value is, which is also what makes the test watertight. Every multiply-add
in the traversal is an explicit fma on the CPU and the GPU, and the C is
built with `-ffp-contract=off`, so the two agree bit for bit.

Instruments (`ports/brally/tools/lighttest/run.sh`, which builds and runs
all three): `bvh_test` (the BVH against brute force on the CPU),
`rt_gpu` (the GPU against the CPU, through the renderer's own
`brr_rt_update`), `rt_compile` (the shader source at each language
version). Scenes: Coastline's whole track (`BR_LIGHT_TRACKOBJ`, 11,364
triangles, kept under `build/` as game data), and three synthetic ones:
soup (20,000 triangles of every size and orientation, overlapping, 1 km from
the origin), terrain (a 100 x 100 heightfield at 2 km sharing every edge,
with a flat terrace and a wall) and rooms (700 stacked 4 m boxes with shared
faces, every other box's triangles duplicated, 500 needles, 500 points and
500 slivers 40 m long and a millimetre wide). A million rays a set: random
(origins in the scene's box, directions uniform) and surface (a point on a
triangle by area, cosine-weighted about its normal on a random side, tmin a
millimetre: how GI's rays leave a hit). Any-hit gets a segment of half to
one and a half times the closest hit's distance.

Counting ties: the brute force tests every triangle in order and keeps the
first at the smallest distance; the BVH finds them in another order. A
difference counts as a tie only when both triangles are hit by the same
test at the distances reported and the two hit points are the same point to
the precision of the coordinates (one unit in the last place of the point's
largest coordinate). Every other difference fails the test.

### CPU against brute force, 1,000,000 rays a set

| scene | build | nodes | depth | SAH | closest: identical / ties / mismatches | any-hit | nodes, triangles a ray | one thread |
|---|---|---|---|---|---|---|---|---|
| Coastline, random | 11.3 ms | 11,487 | 17 | 8.36 | 999,998 / 2 / 0 | 100% | 5.1, 0.53 | 9.9 Mrays/s |
| Coastline, surface | | | | | 999,989 / 11 / 0 | 100% | 14.4, 2.71 | 3.7 Mrays/s |
| soup, random | 26.2 ms | 39,127 | 18 | 73.5 | 1,000,000 / 0 / 0 | 100% | 38.5, 1.72 | 1.3 Mrays/s |
| soup, surface | | | | | 1,000,000 / 0 / 0 | 100% | 51.1, 3.33 | 1.0 Mrays/s |
| terrain, random | 17.5 ms | 20,099 | 14 | 19.5 | 1,000,000 / 0 / 0 | 100% | 10.3, 1.23 | 5.2 Mrays/s |
| terrain, surface | | | | | 1,000,000 / 0 / 0 | 100% | 15.2, 2.27 | 3.4 Mrays/s |
| rooms, random | 8.1 ms | 9,049 | 18 | 321.5 | 730,107 / 269,893 / 0 | 100% | 128.7, 46.1 | 0.38 Mrays/s |
| rooms, surface | | | | | 496,499 / 503,501 / 0 | 100% | 170.0, 62.0 | 0.30 Mrays/s |

Coastline's ties are all at the identical distance (shared edges). Rooms'
are its duplicates and shared faces: 267,740 and 501,504 at the identical
distance, 2,153 and 1,997 a fraction of a micrometre apart (9.5e-7 m at
most, coplanar triangles whose computed distances differ in the last
places). The tree's invariants (every triangle in exactly one leaf, inside
its box, every child inside its parent) hold in every scene. Coastline's
tree is 980 KB with its surface records. As a second check of the triangle
test itself, the brute force's distances agree with Moller-Trumbore in
double precision within 1e-4 on every ray except 8 of rooms' random set
(origins that land exactly on a face with tmin 0, where any answer near
zero is right).

### GPU against CPU, M4 Pro, 1,000,000 rays a set

The compute traversal: every closest hit identical to the CPU's in
distance, both weights and triangle, and every any-hit identical, on all
eight sets (8,000,000 rays, zero differences). GPU time from the command
buffer's start and end, best of 5, one thread a ray, 64 a threadgroup;
repeated runs vary by about 15%.

| scene, rays | compute closest | compute any | hardware closest | hardware any |
|---|---|---|---|---|
| Coastline, random | 670 to 770 Mrays/s | 880 to 1,080 | 2,550 to 3,160 | 4,870 to 5,100 |
| Coastline, surface | 350 to 540 | 560 | 2,610 to 2,700 | 3,890 to 3,920 |
| soup, random | 170 to 200 | 210 | 1,570 | 2,000 |
| soup, surface | 180 | 190 | 1,190 to 1,210 | 1,480 to 1,500 |
| terrain, random | 380 to 440 | 460 to 510 | 2,150 to 3,120 | 4,160 to 4,320 |
| terrain, surface | 360 to 680 | 760 | 2,550 | 3,240 to 3,330 |
| rooms, random | 42 | 60 | 1,260 | 2,610 |
| rooms, surface | 38 | 60 | 840 | 1,590 |

The hardware structure for Coastline: built in 10 ms (the first build in a
process 21 ms), 585 KB compacted. The intersector does its own arithmetic,
so it is compared, not required to be bit-identical. On Coastline it finds
the same triangle as the CPU for 999,995 and 999,978 rays and ties for the
rest; its distances on the same triangle are within 16 units of the hit's
coordinates (about 2 mm at 2 km). Every remaining difference, on all
scenes, is one `rt_gpu` explains from the exact answer (long double) for
the triangles involved, and the run fails on any it cannot: 2 segments
ending exactly at the hit (the intersector includes the end, the CPU
excludes it), 9 rays starting on a face with tmin 0 (rooms), 3 rays passing
within 1e-7 (barycentric) of an edge no other triangle shares (soup, where
the exact answer is a miss by 8e-8 and either side may round it), and 5
hardware hits on rooms' 40,000:1 slivers 1 to 2 mm from a ray's origin,
where the exact hit is behind the origin and the CPU correctly misses. None
on Coastline's surface rays or the terrain.

The shader source compiles, run through this Mac's Metal compiler at
language versions 2.4, 3.0, 3.1 and 3.2, without fast math, with and
without the intersector, with no warnings (`rt_compile`). The offline
compiler (`xcrun metal`) is not installed on this Mac (Xcode reports the
Metal Toolchain component uninstalled), so the `-sdk macosx` and `-sdk
iphoneos` offline compiles are still to do once it is. `brr_metal_rt.m` and
`light_bvh.c` compile for iOS 16 (arm64) and macOS 12 (x86_64), and the
game links (`HOST=null RENDER=metal ports/brally/link64.sh`, no warnings).

Against DESIGN.md 12's acceleration structure budget (15 MB T0, 40 MB T1
and T2): Coastline's world is 1.6 MB with both representations. At the
measured rates a T1 frame's GI probe update (32x32x8 probes x 3 cascades,
a quarter a frame, 64 rays each: about 400,000 rays) costs about 0.15 ms of
hardware tracing or 0.7 ms in the compute BVH on this GPU, before shading.

## Soft sun shadows (2026-10-12)

Three changes to the sun's shadows (`brr_metal_lit.h`, `brr_metal.m`, the
arithmetic in `brr_shadow.h`), verified by arithmetic alone: the game was not
run for any of this. `ports/brally/tools/lighttest/run.sh` builds and runs
every check below (CPU programs, and the shader's own functions on this
Mac's GPU in compute kernels over synthetic maps, with Metal's validation
layer on); it needs no game data.

**Soft shadows from the sun's disc.** The sun is 0.535 degrees across, so a
blocker D metres above a surface casts a penumbra 2 D tan(0.2675 degrees)
wide, 9.3 mm per metre of D. Each cascade evaluation (PCSS, Fernando 2005,
for a directional light) searches for blockers over a disc as wide as the
penumbra of the farthest possible caster (the window reaches back toward
the sun to it, so the receiver's depth gives that distance; 16 gathers, 64
texels), takes the blockers' mean depth, and filters 24 hardware comparisons
over a disc of radius D tan(0.2675 degrees). For a straight edge that is the
share of the sun's disc in view: the physical profile, not only its width.
Both discs are Vogel (golden angle) discs turned per pixel by interleaved
gradient noise; widths are capped at 16 texels (about 35 cm in the nearest
cascade, a blocker 75 m up). The cascade blend is kept. The cascades'
constants carry metres per unit of depth (`LU.cdepth`) and the sun's angular
radius (`LU.csoft`).

**Contact shadows.** From each lit pixel, a march toward the sun over 0.5 m
in the G-buffer's world positions, one step per pixel the march crosses on
screen (at most 16, none when it stays inside its own pixel), jittered per
pixel. A step is shadowed when the surface seen at its pixel is nearer the
camera than the step by less than a thickness (two steps, plus about four
pixels of depth at 1080p) and stands off the receiver's own plane by 2 cm
(so a flat receiver never shadows itself). Hits fade out over the march's
last quarter, where the cascades take over; the sun's visibility is the
lesser of the two. The world to pixel map (`LU.scr`) is the inverse of the
sky's camera ray, checked against it below. `BR_LIGHT_VIEW=6` shows the
contact shadow alone, `BR_LIGHT_NOCONTACT=1` turns it off.

**The static cache.** Each cascade's window is an eighth wider than its
slice's bounding sphere and moves in steps of an eighth of its width (256
texels): anywhere within a step the sphere stays inside, so the matrix
holds still to the bit until the slice crosses a step. Its reach toward the
sun moves in 16 m steps. The track is drawn into a cache array only when a
cascade's matrix or the track changes. The moving casters are the frame's
opaque 3D less what lies on a track triangle (each corner within 0.05 of the
same one, `light_capture_ontrack`; the track's own shadow already holds
those): the cars, the start lights, what the race draws itself. Per frame
and cascade, the live map is restored from the cache where last frame's
movers were (a region blit; the whole slice after a redraw), the movers are
composited over the cache's depth in a scratch tile their size (the
cascade's matrix cropped to the tile: the same texel grid), and the tile is
blitted in. A cascade with no movers now or last frame costs nothing. The
blits for all the cascades share two encoders.

### The checks

`ports/brally/tools/lighttest/run.sh`, Apple M4 Pro.

The penumbra, on the CPU twin of the shader (`shadowcpu.h`, step for step,
the hardware's gather and filtered comparison written out): the sun
overhead, 2 cm texels, a flat blocker D above a flat receiver over half the
map; 2,401 receivers 0.5 mm apart across the edge, each at 64 turns of the
discs. "Width" is the span where the mean over the turns is strictly between
0 and 1, less the one texel the hardware's bilinear comparison adds; "rms"
and "max" compare the mean with the physical profile (the share of the
sun's disc in view) seen through that one-texel comparison; "noise" is the
largest standard deviation over the turns at one receiver (what one pixel
can show before any temporal filter); "drops" counts places where one
turn's result gets darker moving away from the blocker.

| D | 2 D tan, mm | width, mm | off by, texels | rms | max | noise | drops |
|---|---|---|---|---|---|---|---|
| 0.05 m (contact) | 0.47 | 0.5 | 0.00 | 0.0000 | 0.0010 | 0.000 | 0 |
| 2 m | 18.68 | 18.5 | -0.01 | 0.0001 | 0.0010 | 0.007 | 0 |
| 5 m | 46.69 | 46.5 | -0.01 | 0.0001 | 0.0010 | 0.011 | 0 |
| 10 m | 93.38 | 92.5 | -0.04 | 0.0001 | 0.0006 | 0.013 | 0 |
| 20 m | 186.75 | 184.5 | -0.11 | 0.0001 | 0.0007 | 0.018 | 0 |
| 40 m | 373.50 | 369.5 | -0.20 | 0.0003 | 0.0020 | 0.026 | 0 |
| 60 m | 560.25 | 554.5 | -0.29 | 0.0004 | 0.0023 | 0.028 | 0 |

Every width is within a third of a texel of 2 D tan(0.2675 degrees) (the
outermost of 24 Vogel points sits at 0.99 of the radius, hence the slight
shortfall growing with D); the contact case is a hard edge, its whole
transition one texel (20.5 mm); receivers well inside the shadow are
exactly 0 and well outside exactly 1, at every turn.

The same functions on the GPU against the CPU twin, all 153,664 receivers
per D: the largest difference 7.1e-4 (the hardware's 1/256 texel sample
positions); no darkening away from the blocker on the GPU either; the GPU's
own penumbra widths the same within 1 mm. At 60 m, 2 receivers of
153,664 differ by one sample of 24: at the penumbra's outermost tip (shadow
under 4%) the GPU's blocker search missed the blocker's last texel column
that the CPU's caught, and took PCSS's "no blocker, fully lit" exit. That
exit is PCSS's own discontinuity, bounded by the shadow the search's gaps
can hide at the rim.

The cache, a simulated drive (`shadowtest.c`): 3,920 static triangles
(ground and 60 posts over 400 m), three cars as moving casters, four
cascades of 256^2 through `brr_cascade_fit`, `brr_shc_plan`, `brr_shc_rect`,
`brr_shc_tile`, every live map compared bit for bit with the scene drawn
from scratch with the same matrix, every frame:

| phase | static redraws, cascades 0..3 |
|---|---|
| camera still 20 frames, cars moving | 1 1 1 1 (the first frame only) |
| driving 0.3 m a frame (18 m/s), 300 frames | 20 11 5 1 |
| the track rebuilt | 1 1 1 1 |
| turning 1 degree a frame, 60 frames | 3 4 8 7 |

Live maps differing from brute force: 0 of 1,524. Redraws without a matrix
change: 0; matrix changes without a redraw: 0. The moving casters are in
the live maps (568 texels a frame nearer than the cached track). Slice
corners outside their window: 0. The cropped tile matrix puts every point
within 7.6e-6 texels of its map texel less the tile's corner.

On the GPU: the region and whole-slice restore blits are bit-exact (0
texels differ, 0 outside the region changed). The tile path against drawing
the movers straight into the slice (a dozen triangles over random cache
depths, the game's depth bias): 0 texels changed outside the rectangle;
6,338 inside differ, by at most 8.4e-6 in depth (1% of one texel's depth,
0.4 mm at that test's scale), from the rasteriser snapping corners to 1/256
texel at positions the crop moves by 1e-5 texel. No covered texel changes.

The world to pixel map against the sky's camera ray (2,000 random cameras,
viewports, screen maps, both row orders): the ray through the computed
pixel passes the point within 1.2e-5 rad.

The contact shadow against ray-traced truth (a 30 x 30 x 12 cm block on
flat ground, the sun 33 degrees up behind it, a 320 x 240 G-buffer ray
traced through the game's camera conventions): of 214 ground pixels the
block shadows within 0.30 m along the sun, 196 (91.6%) found; the 18 missed
are rays that only graze the block's corner between steps, or stay within
the 2 cm plane margin before leaving its side. Of 74,445 ground pixels with
nothing within 0.55 m, 2 shadowed: the method's silhouette fringe (a point
just behind the block's top edge cannot be told from one inside it by a
single depth layer; a fixed 0.25 m thickness gave 8, the thickness used
gives 2 with the same 196 found). GPU and CPU agree at all 74,659 ground
pixels.

The whole shader source, extracted as `brr_open` builds it, compiles with
the system's Metal compiler with no diagnostics and holds all 18 functions
the lit frame makes pipelines of; the LU struct is 560 bytes in the shader
and in C. Not done: the offline `metal` compiles for macOS and iOS. This
Mac's Xcode 27 ships the offline compiler as a separate component (the
Metal Toolchain, `xcodebuild -downloadComponent MetalToolchain`), not
installed; installing it is the project lead's call. `run.sh` uses it when
present.

`HOST=null RENDER=metal ports/brally/link64.sh` builds and links with no
warnings.

### Cost

Per lit pixel: the sun's visibility is 16 gathers and 24 filtered
comparisons a cascade evaluation (40 texture reads, against 9), twice in the
blend bands (the last tenth of each cascade); none on a face turned from the
sun; 16 where no blocker is in reach (most lit ground). One sincos a pixel;
the disc points are constants once the loops unroll (turned by a 2 x 2
rotation, 4 FMAs a point). The contact march: up to 16 reads of the 16-byte
position texel and a 4 x 4 transform each; none where the cascades already
shadow fully or the march stays inside one pixel.

Measured in compute kernels over a 1920 x 1080 G-buffer of ground seen from
2 m (four 2048^2 cascades split at the fit's distances, half the ground
under a 10 m blocker), M4 Pro, milliseconds over the G-buffer reads alone,
the range of the medians of six runs (other work was sharing the GPU):

| | ms at 1080p |
|---|---|
| sun, the previous 3x3 filter | 0.13 to 0.23 |
| sun, PCSS | 0.35 to 0.56 |
| contact march | 0.46 to 0.66 |
| sun and contact, as the lighting pass does them | 0.52 to 0.79 |

So the soft sun and contact shadows add about 0.4 to 0.6 ms at 1080p on
the M4 Pro over the 3x3 filter. An A15-class phone GPU has roughly a
quarter of the M4 Pro's throughput: an estimated 1 ms more at T0's 1280 x
720, against DESIGN.md 12's 1.6 ms for culling and shadows together. The
knobs if a device run shows it over budget: 12 and 16 samples instead of 16
and 24, and 8 contact steps.

The shadow update (the maps themselves): a height field of 11,250
triangles (as many as Coastline's track) and 36 moving triangles in a 276 x
180 texel rectangle, M4 Pro, the minimum of 41 runs (the work itself; the
medians, 0.5 to 0.9 ms, carry the other load), two runs:

| | min, ms |
|---|---|
| before: four maps drawn afresh each frame | 0.25 to 0.28 |
| cache, movers in 2 cascades | 0.13 to 0.14 |
| cache, movers in 4 cascades | 0.17 |
| cache, no movers | 0 |
| cache, a frame that redraws one cascade | 0.18 to 0.21 |

A depth pass costs its whole 2048^2 attachment however little it draws
(measured: 0.07 to 0.09 ms a slice, about the same for a 256^2 draw as for a
full one, whatever the load action), a 256^2 region blit 0.005 ms, and an
encoder about as much as the texels a small blit moves: hence the tiles and
the shared blit encoders. The simulated drive redraws cascade 0 once in 15
frames at 18 m/s, cascade 3 once in 300.

Memory: the cache doubles the cascades, 64 MB more (two 2048^2 x 4 arrays of
32-bit depth, 128 MB), plus a tile per cascade grown to the largest it has
needed (1 MB at 512^2). The live array alone was already over DESIGN.md
12's 40 MB for T0's sun and local shadows: T0 needs 1024^2 cascades or
16-bit depth (a quarter or half of this), a tier decision for the device
runs.

The CPU's share: the on-track test is one grid lookup and a point to
triangle distance per candidate in the centroid's cell, for a few hundred
triangles a frame.

### Open

- No frame of the game has been looked at with these changes (the project
  lead's rule for this work: verification by arithmetic). The first look
  should be `BR_LIGHT_VIEW=3` (the sun's visibility) and `=6` (contact
  alone) on Coastline, sunny, with `BR_LIGHT_STATS=1` for the cache's
  redraw and composite counts on a real drive.
- The penumbra's per-pixel grain (up to 0.028 standard deviation in the
  widest penumbrae) waits for the temporal filter DESIGN.md 4 plans.
- The offline compiles for macOS and iOS, once the Metal Toolchain is
  installed.
- `BR_LIGHT_GPU` times a pass's first encoder only; the shadow update is
  several encoders and blits, so its line there undercounts it.
