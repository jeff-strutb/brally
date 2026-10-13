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

## Verification suite (2026-10-12)

The lighting's maths checked by mathematics, without running the game:
`ports/brally/tools/lighttest/run_all.sh` builds a standalone Metal program
(`lighttest.m`) and prints PASS or FAIL with its numbers for every check
(69 now, about 12 s on the M4 Pro). The program includes the renderer
itself (`brr_metal.m`), so the shader source it compiles is the renderer's
own string expression (`k_src` with `BRR_LIT_SRC` and `BRR_SKY_SRC`, as
`brr_open` compiles it), the pipelines are the renderer's (`lit_init`), and
the C it calls (`lit_exposure`, the weather and sky tables) is the
renderer's. Its test kernels (`kernels.metal`) are appended to that source
and call the shader functions as they are; full-screen passes (the sky
tables, bloom, post) run through the renderer's own pipelines. Every result
is checked against a closed form or a CPU double-precision reference
(`ref.h`: the Khronos PBR Neutral reference ported to C, Earth's atmosphere
with Hillaire 2020's published constants, IEC 61966-2-1 sRGB, UN ECE R112's
photometric table). Every value read back from the GPU (9.15 million) is
also scanned for NaN and infinity.

### Results

Before: the shaders at efc443b0. After: this commit. Thresholds are set by
the precision of what is measured (Monte Carlo noise, half float storage).

| check | before | after | threshold |
|---|---|---|---|
| white furnace, GGX + diffuse, 12 roughnesses x 6 view angles, 1M samples a cell | max 1.794 (roughness 0.045, mu 0.03); 1.352 at roughness 0.25, mu 0.1 | max 1.0007; every cell 0.9990 to 1.0007 | <= 1.001 |
| GGX lobe energy vs double reference | 4.2e-3 (1.0042 at roughness 0.045) | 9.0e-6 | 2e-3 |
| diffuse albedo = 1 - E_spec(v) | not so coupled | 1.0e-3 | 2e-3 |
| reciprocity f(v,l) = f(l,v), 65,536 triples | 5.0e-6 | 6.7e-5 | 1e-4 |
| Fresnel at normal incidence | 0.039999 | 0.040000 | 0.04 +- 1e-4 |
| Fresnel vs Schlick, 0 to 89 degrees | 4.4e-5 | 6.4e-7 | 1e-3 |
| zenith transmittance vs closed form (tau 0.06624, 0.14675, 0.27615) | 0.9360 0.8628 0.7563, 3.1e-3 off | 0.9360 0.8638 0.7588, 3.1e-4 off | 2e-3 |
| transmittance table vs exact optical depth, 400 rays above the horizon | max dT 0.0051 | 0.0010 | 0.01 |
| sun at the ground vs 128,000 lux x exact T, 5 elevations | 5.2e-3 | 2.1e-3 | 1e-2 |
| zenith sky, sun overhead | 3,651 cd/m2 | 4,191 cd/m2 | 2,000 to 10,000 |
| zenith sky vs CPU reference, sun 90 / 54 / 30 / 10 / 5 degrees | -14.8 / -3.9 / -3.5 / -2.7 / -2.2% | -2.2 / -1.9 / -1.7 / -0.9 / -0.4% | 5% |
| sky light on the ground vs CPU reference | -2.8% at worst | -0.4% at worst | 5% |
| multiple scattering table vs CPU reference (Hillaire 5.5, converged) | 6.2% of the table's peak; 12% of a value | 0.38% of the peak; 1.3% of a value | 5% of the peak |
| f_ms (the series' ratio) | 0.332 max | 0.332 max | < 1 |
| (sky + sun at the ground) / (top x mu_s) | 0.51 to 0.97 | 0.51 to 0.97 | <= 1 / (1 - 0.3) |
| Rayleigh and Cornette-Shanks phases over the sphere | 1.000001, 0.999999 | same | 1e-3 |
| sky view mapping round trip | 0.040 degrees | same | 0.05 |
| sun disc integrated vs the sun's illuminance | 0.954 | 1.004 | 1% |
| PBR Neutral vs the Khronos reference, 200,000 colours | 1.6e-7 | same | 1e-5 |
| PBR Neutral: 0 to 0; monotonic (grey ramp; luminance of 64 hues x 2,000 scales) | yes | yes | |
| post: an HDR texel past half float (the sun's disc: 1.66e9 cd/m2) | black (inf, then NaN in the tonemap) | 249 249 249 (60,000 shows 248) | not black |
| exposure, 6 synthetic scenes (log-uniform, log-normal, sky and road, night, fog) | -0.002 to -0.014 EV | -0.001 to +0.001 EV | 0.05 EV |
| exposure, a uniform scene, 41 luminances over a stop | 0.250 EV | 0.031 EV | 0.05 EV |
| exposure clamps to the weather's range; an all-inf scene metered | not metered | 16.000; 16.000 | |
| R112 B50L 0.57U 3.43L (max 350 cd) | 200 | 127 | |
| R112 BR 1.0U 2.5R (max 1,750) | 285 | 238 | |
| R112 75R 0.57D 1.15R (min 10,100) | 19,975 | 19,036 | |
| R112 75L 0.57D 3.43L (max 10,600) | 16,706 | 1,163 | |
| R112 50L 0.86D 3.43L (max 13,200) | 16,706 | 9,684 | |
| R112 50R 0.86D 1.72R (min 10,100) | 19,978 | 19,978 | |
| R112 50V 0.86D 0 (min 5,100) | 19,666 | 17,066 | |
| R112 25L / 25R 1.72D 9L / 9R (min 1,700) | 9,577 / 13,135 | 4,802 / 13,166 | |
| R112 zone III, above the cut-off (max 625) | 12,019 at 5.95R 1.5U | 240 | |
| R112 zone IV (min 2,500) / zone I (max 2 x I(50R)) | 14,476 / 19,942 | 6,778 / 19,989 | |
| cut-off line (R112's steepest log gradient), left of V-V | 0.305U | 0.565D | 0.57D +- 0.1 |
| rear lamp: cd cos^1.5 | exact | exact | |
| bloom: constant image out = in | exact | exact | 2e-3 |
| bloom: delta impulse energy, float targets, away from the edges | 1.000000 | 1.000000 | 1e-4 |
| bloom: delta impulse energy, the renderer's half float targets, 512 x 256 | 0.9944 | 0.9944 | 1% |
| bloom down pass: NaN and inf input | finite | finite | |
| delit: albedo independent of the baked light (x1 to x0.12) | 1.0e-6 | 1.1e-6 | 1e-5 |
| delit: grey vertex colour gives 0.85 x lin(texture) | 0.7277 x (14.4% off) | 0.8500 x | 1e-3 |
| delit within 0 to 0.9 | yes | yes | |
| lin and srgb1 vs IEC 61966-2-1; round trip | 2.2e-7; 2.6e-7 | same | 1e-5; 1e-4 |
| facen: no face gives straight up; planes give their normals | yes | yes | |
| brdf, radiance and fog finite over 300,000 random and degenerate inputs | yes | yes | |

The run also prints the white albedo by roughness and view cosine and the
GGX lobe's own energy. Its loss, the multiple scattering a single-scattering
lobe drops, is 0.4% at roughness 0.25, 30% at 0.7 and 69% at 1.0, at normal
view.

The sunny sky's physical numbers: the sun at the ground 111,563 lux overhead
and 108,103 lux at Coastline's 54 degrees (128,000 lux at the top of the
atmosphere times its transmittance); the sky's light on an up-facing
surface 9,280 and 8,562 lux; the zenith sky 4,191 and 1,790 cd/m2. The
1,790 cd/m2 at 54 degrees is what Earth's constants give (the CPU reference
integrating them gets 1,824). One headlamp's pattern carries 946 lm.

The bloom chain conserves energy exactly in float targets away from the
frame's edges (1.000000 at 2048 x 1024). On a 512 x 256 frame its 8 x 4 level
reaches the edges and clamping loses 0.39%; the renderer's half float
targets lose another 0.17%. Neither is a defect.

### Defects found and fixed

Each was found by a failing check, its cause identified with a further
measurement, and fixed once. Original rendering is untouched: `k_src` (the
Original shaders) and every path outside the lit frame are unchanged. The
new BRDF table is drawn once at start-up into its own texture; with the lit
pipelines, all of `lit_init` takes about 21 ms on the M4 Pro.

1. **The BRDF reflected more than arrives** (`brdf`). The diffuse was
   weighted by 1 - F(h.v), which ignores what the specular lobe takes at
   grazing views: white reflected 1.794 at roughness 0.045 and view cosine
   0.03, and 1.352 at roughness 0.25 (the rain's wet road) and 0.1. The
   diffuse is now coupled to the lobe through its directional albedo E, as
   Kelemen and Szirmay-Kalos 2001 and Kulla and Conty 2017 couple them:
   (1 - E(n.l))(1 - E(n.v)) / (pi (1 - E_avg)). White then reflects what
   arrives, to the table's precision: every cell 0.9990 to 1.0007. E is a
   64 x 64 table made at start-up from the shader's own specular function
   (`brdf_lut`, sampling the visible normals, Heitz 2018), over the square
   root of the cosine and from the roughness floor up; E_avg is integrated
   from the table itself through the lookup `brdf` makes (`brdf_avg`). A
   closed-form coupling (Shirley et al. 1997, 1 - F(n.v)) was tried first
   and rejected on its own numbers: it lost half the light of rough surfaces
   at grazing views (white 0.49 at roughness 0.7, cosine 0.1: the road 20 m
   ahead).
2. **The GGX denominator lost precision on smooth lobes** (`brdf`):
   (n.h)^2 (a^2 - 1) + 1 cancels in float, and the lobe held 1.0042 of the
   light at roughness 0.045. Now 1 - (n.h)^2 is computed as |n x h|^2:
   1.0000, 9e-6 off the double reference everywhere.
3. **The transmittance table's edges** (`trans`, `atm_t`): its texel centres
   did not reach the unit range's ends, so the zenith seen from the ground
   read as 11.5 degrees off it, and even steps of 2.5 km missed 16% of the
   haze's 1.2 km layer: 3.1e-3 off the closed form. Now Bruneton's
   unit-range mapping, with steps growing with the square of the distance:
   3.1e-4, and the table within 0.0010 of exact everywhere above the
   horizon.
4. **The sky view undercounted the haze** (`atm_sv`): even 3.3 km steps
   straight up made the zenith under a high sun 14.8% dark. Steps now grow
   with the square of the distance, as Hillaire's implementation spaces
   them: 2.2%.
5. **The multiple-scattering table's directions** (`atm_ms`): 64 even
   directions straddled the horizon's cosine, where the light jumps (the
   sunlit ground below, the sky above), and missed the long paths just above
   it: 6.2% of the table's peak off a converged reference at 30 km. A CPU
   run of the shader's own sampling matched the GPU to four digits, so the
   integration was right and the directions were the error. Now 256
   directions split at the horizon, 32 cosines a side: 0.38%. The table is
   made only when the weather's haze changes.
6. **The sun's disc** (`skyrad`, `skyfs`, `pfs`): its soft rim kept 0.954 of
   the sun's illuminance; now divided back (1.004). Its centre, 1.66e9
   cd/m2, overflowed the half float HDR target to infinity, and the tonemap
   made that NaN: a black sun. The sky's output is now kept within half
   float's range, and the post pass clamps what it reads (an overflowed
   texel shows 249, the brightest).
7. **The exposure meter's bins** (`pfs`, `lit_exposure`): 64 bins of half a
   stop put a uniform scene up to 0.25 EV off. Now 512 bins of a sixteenth
   of a stop: 0.031 EV, and the six synthetic scenes within 0.001 EV.
8. **The headlamp pattern against R112** (`lamp`): the cut-off sat on the
   horizon, not 0.57 degrees below it where the regulation aims it
   (paragraph 6.2.2.1), and the near side was as bright as the kerb side:
   75L 16,706 cd (max 10,600), 50L 16,706 (max 13,200), zone III 12,019 (max
   625), and the cut-off's line (the steepest log gradient, R112's own
   definition) 0.305 degrees up. Now aimed 0.57 degrees down, the hot spot's
   spread narrower on the near side, and the cut-off a blend in log
   intensity: every test point and zone within the class B limits, the line
   within 0.005 degrees of the regulation's.
9. **De-lighting's neutral** (`delit`): the hue was linearised through the
   sRGB curve at half and scaled back by 4.0, a gamma-2 reading of the
   curve, so a grey vertex colour gave 0.7277 x the texture, not the 0.85
   the code intends. Scaled back by 1 / lin(0.5) = 4.672: 0.8500.
10. **brdf below the surface**: with the light behind the view (l = -v) and
    the eye below the surface, h = normalize(0) gave NaN. `radiance` turns
    the normal to the eye, so the game never met it, but `brdf` now returns
    nothing for a light or eye below the surface.

What the lit frame will show differently: albedo 17% higher before the
exposure meter (which adapts to it); wet and smooth surfaces darker at
grazing views (they no longer reflect more than arrives); headlamps dimmer
on the near side and cut 0.57 degrees lower; the clear sky's zenith brighter
under a high sun (up to 15%) and its light on the ground up to 2.7%
brighter; the sun's disc white and 5% brighter. None of it has been looked
at in a frame (the suite runs no game); the project lead reviews it in the
next frame dumps.

### Not covered here

- The offline compiles (`xcrun -sdk macosx metal -c` and `xcrun -sdk
  iphoneos metal -c` of the dumped source, in `run_all.sh`) need Xcode's
  Metal Toolchain, which is not installed on this machine (`xcodebuild
  -downloadComponent MetalToolchain`); the run reports them as SKIP. The
  runtime compile (the renderer's own way, here on macOS) passes.
- The white balance (inline in `lit_present`), the cascades' shadow
  lookups (checked only for finite output), and the GPU time of the
  changes.
