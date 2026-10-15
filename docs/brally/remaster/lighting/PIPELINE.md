# Remastered lighting: the pipeline as built (2026-10-13)

This supersedes the sky, clouds, glare, exposure, night and sun shadow
sections of DESIGN.md and platform/light/NIGHT.md. The capture seam,
G-buffer replay, BRDF, lamp clusters and shadows, volumetric fog and
TAA/MetalFX described there are unchanged. Its ray tracing (DESIGN.md 3.1,
8.1) and the reflections' traced misses (8.4) are not: since 2026-10-14
nothing traces rays while the race runs, on any device (see "No ray
tracing" below).

The reference is Forza Horizon 6. Since FH3, Playground has photographed its
skies as HDR timelapse domes. A captured dome is both the sky you see and the
light the scene gets from it; there is no procedural atmosphere (GDC 2017,
Wood, "Shoot for the Sky"). The renderer is built the same way.

## Sky and its light (light/light_sky.c, render/metal/brr_metal_env.h, .m)

- A skymap is an equirectangular 8K Radiance `.hdr` from Poly Haven (CC0),
  one per track environment and condition, never shared between tracks:
  `ports/common/skies/skies.txt` names them (35), `fetch.sh` downloads them
  into `ph8k/` (gitignored, 2.9 GB). `BR_SKYDIR` overrides the folder,
  `BR_SKYMAP=file` the map for every weather.
  - Conditions: clear, fog, storm (the game's rain), snow, night. Fog, storm
    and snow maps are overcast captures; night maps are real night skies,
    their moon or brightest source taken as the moon.
  - A map that isn't a "puresky" one (land removed by Poly Haven) carries its
    photographer's trees and buildings over the horizon. skies.txt's fourth
    column is how high they reach (read on each map's horizon band); under
    it the loader rebuilds the sky from the sky just above, fading to the
    horizon's mean. Maps whose land stands over about 23 degrees (a bridge,
    a pine forest) aren't used.
  - Night maps lose the camera's lens glare about the moon (165 x the sky at
    a degree, 9 x at 8): a ring-by-ring median profile subtracted, stars kept.
- The sun's (at night the moon's) bearing is each track's own: the direction
  its art was lit from, its baked vertex light fitted against its faces'
  normals (`BR_LIGHT_BAKEFIT`, light_game.c k_track_bearing; the table
  below). The game lights its cars from one direction on every track
  (BrFrameTintSetup's (15, 10, 20), 33.7 degrees).
- The sun is found in the map. Its disc is integrated over solid angle to
  give the directional light's illuminance and colour, then painted out with
  the surrounding sky, so it is counted only once and casts shadows.
- Units:
  - Clear: the sun 100,000 lux facing it.
  - Fog, storm, snow: the sun's and sky's light on level ground together
    10,000, 6,000 and 18,000 lux (dense fog 5 to 15 klx, a rain overcast 2
    to 10 klx, a bright snow overcast 15 to 25 klx).
  - Night: the moon 0.5 lux facing it.
  - The map is turned about the vertical so the sun keeps the track's
    designed bearing.
- Below the horizon the map is replaced by a 0.2-albedo ground lit by the sun
  and sky; the track itself is the ground.
- Diffuse light: nine irradiance spherical-harmonic coefficients
  (Ramamoorthi and Hanrahan 2001).
- Specular light: a 2048 RGB9E5 cube (the sky drawn) and a GGX-prefiltered 256 cube with 5 mips (to
  16 per face), sampled by filtered importance sampling (Karis 2013, Krivanek
  and Colbert 2008), times E(n.v, r) from the existing table.
- The sky behind the scene is the cube plus an analytic sun disc whose
  radiance is its illuminance divided by its solid angle.
- The painted backdrop:
  - Its land and sea are lit as albedo by the map's light.
  - Its aerial perspective follows Koschmieder at 3 km, using the weather's
    visibility.
  - Its painted sky gives way to the map. A flat panel (mask 128) counts as
    sky above the horizon and as sea below it.

## Air (render/metal/brr_metal_vol.h)

- One visibility per weather drives both the volumetric fog near the camera
  and the painted backdrop's aerial perspective (Koschmieder, 3.912 / V).
  Before, the volume used 1 km for rain and 700 m for snow while the backdrop
  used 8 and 10 km.
  | weather | visibility |
  |---|---|
  | sunny, night | 20 km |
  | fog | 250 m (thick fog, WMO 200 to 500 m) |
  | rain | 4 km (moderate rain) |
  | snow | 1.5 km (light to moderate snow) |
- The air's ambient in-scatter is the mean radiance over the whole sphere,
  the skymap's harmonics' constant term (sh[0] Y00 / pi), which counts the
  ground below as well as the sky. It was the level-ground illuminance over
  pi, the upper hemisphere's alone: 0.75 stop too bright under a uniform
  overcast, which with 1 km visibility turned rain into a white wash.

## Global illumination

- **Baked when a track loads (2026-10-14), not traced while racing.** The
  probes had been a 16 x 8 x 16 volume 8 m apart moved with the camera, a
  quarter traced each frame: past 64 m the lookups fell to its edge probes,
  and every 8 m driven a row of new probes switched on at its far edge, so
  the sky's occlusion under walls and banks swept in ahead of the car, worst
  in snow where nearly all the light is the sky's (in a 60-frame snow burst
  the frame-to-frame darkening's 95th percentile 35,600 pixels, 12,400
  baked). Now brr_gi_bake places a probe 8 m apart wherever a track surface
  needs one, over the whole track (Coastline: 104,425 probes over 267 x 271
  x 12 points, 61 MB), and traces 64 rays a probe in 12 passes, each a bounce
  more (the first pass direct light only: with no probes to read yet, its
  hits had taken the open sky's light and lit the tunnel 10 to 40 times
  over). 280 to 430 ms on an M4 Pro, again only when the track, the sun, the
  sky or its map changes. The distance atlas is 8 x 8 half floats a probe.
  Left out of the bake: the track's lamps (gathered as the game draws them,
  each would have meant another bake) and the cars' lamps; both still light
  everything directly. The bounce's colour is the track's albedo as known at
  the bake (talb_learn learns it during the race: the sunny embankment's
  bounce a little less warm, within 4 to 9 levels of 255 on the mean frame).
- The irradiance probes' rays that miss the track take the skymap's radiance
  in their own direction (the prefiltered cube at GGX roughness 0.5), so the
  bounce light sees the sun's side and the horizon's glow. Before, a miss
  took one radiance looking up and one looking down.
- The probes store irradiance as spherical harmonics to order 2 (Ramamoorthi
  and Hanrahan 2001: 9 coefficients a probe, projected without bias from the
  rays, convolved with the cosine). gi.sh measures, over 256 normals against
  the closed form for a sky gradient, a worst error of 0.14% (the 8 x 8
  octahedral texels they replaced: 5.66%, from their folds, and 2.3% at
  16 x 16). The truncation on a lamp-lit box's first bounce is 3.0%; the
  estimator is within 0.04% of that field's own projection. 14.5 MB where the
  atlases took 20.5. The distances stay an octahedral atlas.

- **The track's albedo, learned from the frames** (brr_metal_talb.h).
  Every triangle's surface record held no colour, so GI's bounces and the
  reflections shaded everything as grey 0.3. Now the track's triangles are
  drawn from the frame's camera into an ID target, and one G-buffer pixel in
  16 a frame (rotating) whose point lies on its ID's triangle (2 cm + 0.2%)
  sums its albedo onto it (until 2026-10-14 a ray from the eye found the
  triangle; the rasterised mapping agrees with it to within 0.2 of 255 on
  the median in 110 checks a weather). Kept on disk a track and weather
  (the save folder's lighting/talb-*.bin), so the next load's probe bake
  has the colours. Each triangle's mean is blended into its
  record, a quarter a frame. Car paint and glass now mirror the yellow
  embankment beside them, where they mirrored grey.
  - `BR_LIGHT_TALBCHECK` maps every pixel on the CPU's BVH and compares each
    triangle's mean with the learned record: worst channel off by a median
    0.9 and a p90 of 3.3 of 255, 116 checks.
  - The weather's saturation measured again with it: the cars 1.15 sunny,
    0.95 rain against the art (1.16 and 0.96 before), so the factors stand.

- **Probes in the solid:** the track's faces wind outward, so a probe
  under the ground or inside a hill sees back faces. One whose rays meet back
  faces past 25% moves through the nearest one, and one within 1 m of a front
  face moves off it, at most 0.45 of the spacing (probe relocation, Majercik
  et al. 2021). A probe still buried is left out of the lookups. The buried
  probes had lit Coastline's tunnel with the sunlit hillside outside it (a
  dark diamond on its wall, the sky's light inside). gi.sh `buried` checks
  the classification and the CPU fold.
- **Reflection hits** (until 2026-10-14, when the reflections stopped
  tracing) inside the probes' volume took their light (sky, bounce) and the
  lamps of their light grid cube, each behind a shadow ray: sunny mean +0.39
  stops against the frame's own pixel, night +0.05.

  Leaving the buried probes out brightened the light near the ground
  wherever the sky alone lights it, so the weather's saturation was measured
  again: fog 1.74 (was 2.0), rain 0.45 and its cars 1.84 (were 0.63 and 2.0);
  art against lit then land 0.94 to 1.03, cars 0.93 to 1.12 in all four.

## No ray tracing (2026-10-14)

The project lead's rule: nothing traces rays while the race runs, on any
device (most iPhones have no hardware ray tracing; the techniques without it
look as good). The track's BVH remains only for the probes' bake when a
track loads (in shader code, every GPU) and the diagnostics
(`BR_LIGHT_SUNCHECK`, `BR_LIGHT_TALBCHECK`); the hardware acceleration
structure and intersector are gone. Before, the BVH world, and with it GI and
every reflection (the screen-space march included), was built only on GPUs
with hardware ray tracing (M3, A17 Pro and later).

- **Reflections** march the screen as before; what leaves the screen reads a
  local cube (brr_metal_rcube.h): the track drawn around the eye every frame,
  6 faces of 512, lit as the traced hits were (the learned albedo, the sun
  through the whole track's shadow map, the baked probes), the sky behind
  it, each texel's distance from the eye in alpha. The ray is looked up
  through those distances (24 steps from 0.2 to 300 m, the crossing refined
  by its secant: Szirmay-Kalos et al. 2005, distance impostors), so the bank
  beside a wet road is found where the reflected ray meets it rather than
  where the eye's direction would (the cube centred 1 m and more above the
  road). On the rain's start grid, road pixels against the traced build: 0.08%
  off by more than 16 of 255 (9.6% without the distance lookup); sunny and
  night frames within 0.11% throughout. Forza Horizon's cars reflect a
  dynamic cube the same way. `BR_LIGHT_VIEW=16` shows the reflected light
  alone, `17` the cube toward each pixel's point (the frame again, cars aside,
  where its faces are right).
- **The track's albedo** is learned by rasterisation (above).
- **GI** is baked when the track loads (above).

## Sky light where the sky is hidden

- **Rough specular from the probes:** past roughness 0.6 (where the
  reflections stop tracing) the sky's prefiltered cube gives way, by 0.8, to
  the probes' irradiance about the mirror direction over pi (Unreal's Lumen
  reuses its diffuse radiance cache for rough reflections the same way).
  Coastline's tunnel walls had been lit by the open sky: on its left wall the
  sky's specular alone was 0.0041 of the 0.0040 frame.
- **Specular occlusion** of the lobes the reflections never trace (rougher
  than 0.45, and car paint's base under its traced coat): the sky's cube times
  the probes' irradiance about the mirror direction over the open sky's. A car
  in the tunnel had been a pale mirror of the sky. Open-air frames unchanged.
- **The air's sky light** inside the probes' volume is theirs, (E up + E
  down) / 2 pi, not the sky's harmonics (a tunnel's air glowed white).
- **The track's lamps are lights** in every weather (texmat `lamp`: a
  tunnel's, a street's; light_world): each lamp triangle on the track an
  emitter at the luminance the frame gives its surface (BRR_LAMP_L times its
  texels' mean). Coastline's tunnel lamps cluster to about 5,300 cd each.
  The start lights are not on the track (they move and switch) and stay
  emitters only.
- **Day exposure adapts in a tunnel:** the incident meter, until the frame
  meters more than 1 EV darker than it predicts; then the excess, up to 8 EV,
  at the night's rates (3 EV/s darker, 1 brighter). Open frames meter -0.5
  to +0.6 EV from the incident meter in all four day weathers (the weathers'
  compensation aside) and never adapt; Coastline's tunnel meters 5.5 to 7.9
  under it. `BR_LIGHT_EVLOG` logs it, `BR_LIGHT_EVSNAP` adapts at once (a
  headless run's virtual clock runs slower than the race's).

## Surfaces

- **Ambient occlusion:** GTAO (render/metal/brr_metal_ao.h; Jimenez et al.
  2016, XeGTAO's slice integral).
  - Half resolution, 2 slices of 4 steps per side, 1.5 m radius, a 3x3
    depth-aware filter and a bilateral upsample.
  - The multi-bounce fit is applied to the sky's diffuse, and Lagarde's
    specular occlusion to its specular.
- **Smooth normals** (light/light_smooth.c): angle-weighted per frame across
  draws (Thurmer and Wuthrich 1998), split at a 50 degree crease, read by a
  lit-only vertex stage from a parallel buffer.
- **Material albedo:**
  - Natural surfaces tagged in texmat.csv (asphalt, concrete, sand, dirt,
    rock, grass, foliage, snow, ice) take one chroma per texture times the
    texel's luminance. Asphalt and concrete keep their texture's hue (forced
    grey, the road lost the art's warm grey).
  - This removes the 1999 dither noise and the baked vertex tints.
  - The start-light panel is `blackmetal`, albedo 0.05.
- **Dry ground** has roughness 0.9, so it shows no reflection.
- **Wet ground** (Lagarde 2013): porous ground (soil, sand, grass, rock)
  darkens to 0.55 of its albedo and glosses only to roughness 0.6; sealed
  ground (asphalt, concrete, metal: G-buffer code 124) keeps a film,
  darkening to 0.85, roughness 0.35, Forza Horizon 6's blurred wet sheen;
  puddles (roughness 0.05) only in sealed ground's low spots. Before, every
  upward face went to 0.1 and a third of it to mirror puddles.

## Exposure and image

- **Incident meter (ISO 2720):** EV100 = log2(E / 2.5), where E is the light on
  level ground. The exposure is 1 / (1.2 x 2^EV100).
- **Compensation +1.5 EV, measured:** FH6 shows sunlit asphalt at display
  0.18 to 0.19, and that is the level that lands it there through PBR
  Neutral's toe.
- **Night exposure, metered on the GPU:**
  - The frame's centre-weighted mean log luminance, with its brightest tenth
    held to at most 2.5 stops over mid-grey (a camera's highlight weighting,
    so a pool filling the bumper view keeps its texture).
  - It adapts at 3 EV/s toward darker and 1 EV/s toward brighter, never
    brighter than a floor of EV100 -0.5.
  - It adapts on the game's clock (`BR_VCLOCK`'s under a script). On the
    wall clock, a scripted run's night frames came out up to 6% apart from
    run to run.
- **Night targets, measured from FH6's night chase frames** (101 and s26):

  | element | FH6 | Coastline |
  |---|---|---|
  | low-beam pool | 0.04 to 0.6, texture kept | 0.09, texture kept |
  | road 30 to 50 m | 0.017 to 0.06 | 0.06 |
  | unlit road | 0.007 to 0.011 | 0.004 |
  | moonlit hills | (unlit ground above) | 0.025 |

  The moon is 0.5 lux facing it. The earlier pool target of 0.55 to 0.88 was
  wrong: at EV100 -2 it clipped the pool to flat white.
- **Glow:** an emitter's light past the screen's cap goes to an emission
  target (the G-buffer's lamps and lit windows, the cars' lenses), up to 128
  x the display's white, which the first bloom level adds: a lamp keeps its
  colour on screen and gets its glow about it. (At the cap alone lamps read
  as flat colour; at 1000 x a signal head's bloom swamped it white.)
- **Light-source surfaces** (start lights, lit windows, flares, lenses) show
  at most one stop over the display's white in their brightest channel, with
  their hue kept. This is Playground's rule that emissives sit "a few stops"
  over the scene's light. At physical luminance a signal head is thousands of
  times the night's exposure, and every colour reaches white. The light these
  sources cast stays physical.
- **White balance:** auto, to the light on level ground by day. At night it
  is daylight, so the moonlit sky stays blue and the lamps stay white.
- **Post:** bloom, then Khronos PBR Neutral, then the weather's saturation
  in Oklab (land and cars apart: the G-buffer's material), then sRGB. The
  factors are measured: the game's own art's saturation against the lit
  frame's on the same virtual-clock frames, weighted by the art's chroma,
  two steps (sunny 1.37 and 2.56, fog 2.0 and 2.5, rain 0.63 and 2.0, snow
  1.96 and 2.67). A car's clear coat mirrors the pale photographed sky and
  lost twice the land's colour.
- **The storm's exposure:** 2.4 stops under the day's (-0.9 EV against +1.5).
  Forza Horizon 6's rain frames sit at a median display luminance of 0.03 to
  0.08; the incident meter alone made a 6,000 lux storm as bright as a clear
  day (0.26 to 0.44).
- **The painted backdrop** gets no aerial perspective: its painting carries
  the artist's own, and adding Koschmieder's on top greyed its sea.
- **Flat decals** (a tire mark: a white texture, the mark in its vertex
  colour) take the game's combined colour as albedo and its alpha as display
  space coverage, 1 - (1 - a)^2.2.

- **Each track's sun and moon bearing** (toward the light, from +X toward
  +Y). `BR_LIGHT_BAKEFIT` fits each track's baked vertex light
  against its faces' normals (the menu index at bossrally.cfg byte 1344, the
  unlock masks at 1376 and 1380 set to reach all twelve):

  | env | tracks | baked bearing (degrees) | R2 |
  |---|---|---|---|
  | 0 | 0, 6 | 14, 58 | 0.48, 0.61 |
  | 1 | 1, 7 | 26, 17 | 0.44, 0.51 |
  | 2 (Coastline) | 2, 8 | 21, 18 | 0.35, 0.41 |
  | 3 | 3, 9 | 78, 83 | 0.37, 0.34 |
  | 4 | 4, 10 | 94, 125 | 0.34, 0.35 |
  | 5 | 5, 11 | 19, 7 | 0.28, 0.25 |

  Every track's sky is turned to its own (checked on all twelve by day and
  three by night: the sun or moon found in the map at the track's bearing to
  0.1 degree); a track change within an environment reloads it.

## Lights that are lights

- **Car lamps:**
  - The lamps follow the matrix the game drew each car with this frame
    (g_BrCarMtxSlot). The physics' state runs one or two ticks ahead: at
    50 m/s the lights trailed 0.9 to 1.9 m behind the car shown.
  - The lamps sit on the body's outer face (the box over all four
    full-detail body lists), so a car never lights itself.
  - Tail and reversing lenses light exactly where the car's texture paints
    them (carlamp.csv, G-buffer codes 112 and 116).
  - Rear lamps follow UN ECE R7's distribution: a Gaussian 18 degrees across
    by 6 degrees up and down. Tail 5 cd, stop 80 cd, reverse 300 cd.
  - Each lamp's lens is drawn as an emitter with L = I(v) / (A cos). It uses
    the same candela that light the scene.
- **Headlamp levelling** (UN ECE R48's automatic levelling): each car's
  beam elevation is its body's pitch low-passed (0.6 s, the game's clock),
  azimuth and roll the body's. Over 53 s of the night race the bounce (3.7 Hz)
  falls from 0.197 to 0.032 degrees (84%), the worst swing in half a second
  from 3.84 to 0.78 degrees, against a cut-off 0.57 degrees down; the beam
  follows the road's slope closer than the body did (worst 0.72 against 2.73
  degrees). `BR_LIGHT_LEVELLOG` logs the player's raw and levelled aim.
- **Start lights:** the `lamp` textures emit 10,000 cd/m2 at a full texel
  (LED signal heads run 5,000 to 15,000), in the G-buffer and in their flares.
- A lamp's lens shows only on its own car's body as drawn: car paint within
  35 cm. A car the game hides, such as the player's in the bumper view, shows
  no lenses.
- **Car floors:** each car casts from a floor pan 0.2 m up. The 1999 cars
  are open shells with blended glass, so the sun shone through the cabin
  onto the road under them, a lit patch that came and went with the car's
  pitch and the camera (the shadows "coming in and out" in real time).
- **Removed fakes:**
  - The game's blob shadow under the cars (texmat `shadow`).
  - The white light pool the game lays on the road ahead of the cars at
    night (texmat E2B6CDC5, tagged `shadow`): the lamps light the road.
  - The lighthouse beam polygons (removed earlier).
- **Mis-tag fixed:** the windmill's sails (texmat AAC3ACFA) were tagged `lamp`
  and glowed; they are now `wood`.

## Sun shadows: a virtual shadow map (2026-10-14)

The sun's (and the moon's) shadow is a virtual shadow map in the manner of
Unreal Engine 5's (brr_vsm.h, brr_metal_vsm.m). It replaces the four
cascades, which `BR_LIGHT_VSM=0` still selects for comparison.

- **Why:** shadows came in as the car closed on them. `BR_LIGHT_SUNCHECK`
  traces every sun-facing pixel's ray to the sun through the track (the
  static casters' own triangles, so the trace is the truth) and counts the
  real shadow the map loses, by distance. The cascades lost 10% of it at
  55 to 110 m, 37 to 40% from 110 to 300 m and all of it past 300 m. Two
  causes: their bias (up to 12 texels of depth at a grazing sun, 1 to 1.7 m
  in the far cascade, against casters 0.3 m above their receivers: kerbs,
  walls, a hill's grazing shadow), and 22 cm texels from 110 m on.
- **Bias:** the receiver moved along its normal by 0.3 + sin(angle from the
  sun) texels (normal offset, Holbert 2011: a move of a texels gives a / cos
  of depth margin, and a bilinear comparison's neighbours lie within tan of
  a texel), each filter sample compared against the receiver's own plane
  (receiver plane depth bias, Isidoro 2006), half a texel more for
  rounding, nothing on the casters. On the cascades alone this halved the
  far loss with no more acne.
- **The map:** 8 levels in the sun's axes, texels from 1 cm to 1.28 m, each
  64 x 64 pages of 128 texels about the camera, a page's table slot its
  number modulo 64. A point takes the finest level whose texel is at most
  half its pixel's footprint and whose window holds it, dithered over half
  a level at each change. Pages are marked from the G-buffer, given a slot
  of a fixed pool (3969 on the Mac, 1953 on iOS; R32Float with a texel of
  gutter), drawn at most 120 a frame into a scratch depth atlas (the static
  track, from a light-space grid) and copied in. The sun is fixed for a
  track, so a page stays until it leaves its window or goes unneeded for 120
  frames. The moving casters get dynamic pages each frame, compared at the
  nearer depth. A page not drawn yet falls back a level, then to a track map
  drawn once (2048 square over the track), which is also the fog's sun.
- **Measured,** the same 150-frame sunny run, real shadow lost (cascades
  with the new bias, then the map): 55 to 110 m 3.1% to 0.1%, 110 to 200 m
  18.9% to 1.3%, 200 to 300 m 18.4% to 3.0%, past 300 m 100% to 1.9%; false
  shadow unchanged at 0.1%. Tracks 0 and 4 the same way (past 300 m 100% to
  7.0% and 1.1%). Night, rain, snow and fog render as before.
- **Cost:** the map's passes 0.36 ms a frame at 1280 x 720 on an M4 Pro
  (marking 0.17, the rest 0.02 to 0.05 each; `BR_LIGHT_VSMTIME`), against
  about 0.38 for the cascades' drawing; the lighting within run-to-run
  noise of the cascades' (the filter's taps follow the penumbra's width, a
  lookup's samples reuse the centre's page). At most 94 pages drawn in a
  frame; about 710 resident on track 2. `BR_LIGHT_VSMLOG` logs the counts.
- **Moving casters fixed on the way:** the ground's far triangles failed the
  5 cm "on the track" test (a drawn corner is rebuilt from its pixel and
  1/w, its error at most 0.022% of its distance from the eye) and ground at
  z = 0 sat on a hash cell's face, so 20 to 30 triangles up to 50 m across
  were redrawn every frame as movers (into the cascades' live map, the lamp
  tiles' invalidation, the map's dynamic pages: 250 a frame). The tolerance
  is now 5 cm + 0.05% of the distance and the hash's boxes are padded by
  1 m; the movers are the cars alone (4 dynamic pages a frame), the GPU's
  list still equal to the CPU's in all 94 checks. The cascades also left a
  trail of the car's shadow tips on the road; the map's dynamic pages are
  cleared each frame.

## Cost

- **Pixel budget:** the lit 3D renders at no more than 1920 x 1080 pixels,
  whatever the window. MetalFX upscales to the target, up to 3x per side on
  Apple silicon.
  - Before, a fixed 0.67 scale made a 5K screen cost three times a 1440p one.
  - At the budget, 2560 x 1920 takes about 12 ms of GPU a frame on an M4 Pro.
- **Smooth normals run on the GPU:** a GPU spatial hash in four dispatches.
  `BR_LIGHT_SMOOTHCHECK` compares them with light_smooth.c on the live frame:
  worst 0.04 degrees.
- **Moving casters run on the GPU** (brr_metal_cast.h): a kernel tests each
  opaque triangle against a GPU copy of the track's hash, appends the movers
  with their indirect draw's count, and widens each cascade's rectangle. The
  live shadow map is restored from the static cache over last frame's
  rectangle by a quad the GPU places. The CPU places only the cars' boxes
  (the cascades' reach, the lamp tiles' invalidation, their floor pans).
  `BR_LIGHT_CASTCHECK` compares with the CPU path: 180 of 182 frames exact,
  two at float rounding (one triangle on the 5 cm track tolerance, one
  rectangle edge by a texel).
- **Lamp clusters are built on the GPU** (brr_metal_cluster.h): one thread
  a light marks the columns, rows and slices it meets, then one thread a
  cluster ands the three masks and tests each light against the cluster's
  sphere. The CPU gives the camera's planes alone (light_cluster_geom).
  `BR_LIGHT_CLUSTERCHECK` compares with light_cluster_build: 119 of 119
  checks exact.
- **Lamp shadows are planned and drawn on the GPU** (brr_metal_plan.h): the
  plan (each lamp's importance from this frame's cluster coverage, tile
  sizes halved or dropped to fit the atlas, last frame's places kept, Morton
  packing, the cache against the cars' boxes) runs in one threadgroup, its
  cache kept on the GPU; the drawn tiles' track casters are one list of
  (tile, triangle); the pass is one render over the atlas, each triangle
  placed in its tile and clipped to it by the vertex stage, every draw's
  count the GPU's. `BR_LIGHT_PLANCHECK` steps light_shadow_plan on the same
  inputs every frame: night, rain and sun, 14,160 frames each, no tile
  differing, transforms within 7.8e-5, the track casters' lists equal in all
  116 checks a run; the frames against the CPU path within 4 of 255 on
  0.002% of pixels. CPU: the plan 0.009 to 0.004 ms a frame (its encoding),
  the tiles' casters 0.047 to none; the pass 0.06 ms more on the GPU.
  `BR_LIGHT_PLANCPU` forces the CPU's plan.
- The deferred lighting at 2560 x 1440: 3.70 ms a frame this morning, 4.30
  with the probes' specular and occlusion of the sky and the harmonics (the
  two lookups share their 8 probes' weights).
- `BR_LIGHT_CPU`: the casters 0.01 ms a frame (0.17 before). The lights
  stage, by part: lights 0.004, clusters 0.013, shadow plan 0.012, tiles'
  casters 0.004 (0.066 on the CPU in the same run). The rest is command
  encoding. GPU total unchanged (6.2 ms at 2560 x 1440).

## Fixed on the way

- **Reflection history:** the temporal reprojection read last frame's camera
  at a fixed 112 bytes from the end of the lighting's constants.
  - The skymap's harmonics, appended after it, turned that read into garbage,
    so every frame's history was dropped.
  - Wet roads then showed the one-sample trace's speckle.
  - It now reads LU.scr by its real offset.
- Blended draws now write the game's own depth (the W-buffer value). Before,
  the rasteriser's z was tested against the G-buffer's W-buffer depth, so
  flares, decals and windows passed or failed at random. This is the start
  lights' glow drawn behind the panel.

- **Reflection hits read the wrong triangle's corners:** a hit is numbered
  in the track's order, but the reflections read its corners from the BVH's
  leaf order, so its normal (and its sun and sky shading) came from another
  triangle. They now go through the leaf map (`BR_LIGHT_TALBCHECK` checks
  it against the track's corners: 0 of 11,364 wrong).

## Checks

```sh
sh ports/brally/tools/lighttest/sky.sh      # sun found, measured, painted out; SH against brute force
sh ports/brally/tools/lighttest/env.sh      # cube faces against Metal's sampling; GGX prefilter against the CPU
sh ports/brally/tools/lighttest/smooth.sh   # normals on a prism, flipped windings, a sphere
```

| check | result |
|---|---|
| synthetic sun's direction | within 0.01 degrees |
| synthetic sun's illuminance | within 0.1% |
| sky on level ground (synthetic) | pi, exact |
| real map's SH irradiance vs brute force | worst 2.8% |
| cube faces through Metal's sampling | 0.0007 |
| cube mips conserve the sphere's integral | within 2% |
| prefilter vs CPU lobe, roughness 0.25 to 1 | 0.3 to 1.6% |
| prism normals, radial | 6e-8 |
| sphere normals | within 0.34 degrees |

Original (lighting off) is pixel-identical to the previous build under
`BR_VCLOCK=1`. Without the virtual clock, headless races differ from run to
run, even for the same binary.

Diagnostics:

- `BR_LIGHT_VIEW`:
  - 1 albedo, 2 normals, 3 sun visibility
  - 9 sun, 10 sky diffuse, 11 lamps, 12 glow, 14 sky specular, 15 occlusion
- `BR_LIGHT_NOAO`, `BR_LIGHT_NOBLEND`, `BR_LIGHT_NOBLENDTEX=a,b`,
  `BR_LIGHT_BLENDLOG`
- `BR_LIGHT_LAMPLOG` now also logs each lamp's height over the track and its
  side vector.
- `BR_LIGHT_PICK=fx,fy` logs every 3D draw under that point of the frame
  (fractions of its width and height): texture hash, material, blend.
- `BR_LIGHT_NOBLENDTEX` takes texture hashes as well as indices.

## Packaging

- The Mac app carries the 8K maps in Resources/skies, the iPhone and iPad app
  the 4K ones (ios/build.sh; all 35 of skies.txt checked in the Simulator
  build, 716 MB).

## Open

