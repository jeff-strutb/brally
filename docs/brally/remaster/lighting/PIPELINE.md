# Remastered lighting: the pipeline as built (2026-10-13)

This supersedes the sky, clouds, glare, exposure and night sections of
DESIGN.md and platform/light/NIGHT.md. The capture seam, G-buffer replay,
BRDF, sun cascades, lamp clusters and shadows, ray tracing, reflections,
volumetric fog and TAA/MetalFX described there are unchanged.

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

- The irradiance probes' rays that miss the track take the skymap's radiance
  in their own direction (the prefiltered cube at GGX roughness 0.5), so the
  bounce light sees the sun's side and the horizon's glow. Before, a miss
  took one radiance looking up and one looking down.
- The probes store irradiance as 8 x 8 octahedral texels, bilinearly
  sampled. For a smooth field this is within 1.4% on the axes and up to 6%
  at oblique normals (a CPU model reproduces it); gi.sh checks the cube path
  against the analytic one through the same storage (0.03%).

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
    texel's luminance. Asphalt and concrete are neutral.
  - This removes the 1999 dither noise and the baked vertex tints.
  - The start-light panel is `blackmetal`, albedo 0.05.
- **Dry ground** has roughness 0.9, so it shows no reflection.

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
- **Night targets, measured from FH6's night chase frames** (101 and s26):

  | element | FH6 | Coastline |
  |---|---|---|
  | low-beam pool | 0.04 to 0.6, texture kept | 0.09, texture kept |
  | road 30 to 50 m | 0.017 to 0.06 | 0.06 |
  | unlit road | 0.007 to 0.011 | 0.004 |
  | moonlit hills | (unlit ground above) | 0.025 |

  The moon is 0.5 lux facing it. The earlier pool target of 0.55 to 0.88 was
  wrong: at EV100 -2 it clipped the pool to flat white.
- **Light-source surfaces** (start lights, lit windows, flares, lenses) show
  at most one stop over the display's white in their brightest channel, with
  their hue kept. This is Playground's rule that emissives sit "a few stops"
  over the scene's light. At physical luminance a signal head is thousands of
  times the night's exposure, and every colour reaches white. The light these
  sources cast stays physical.
- **White balance:** auto, to the light on level ground by day. At night it
  is daylight, so the moonlit sky stays blue and the lamps stay white.
- **Post:** bloom, then Khronos PBR Neutral, then sRGB. There is no grade, no
  local exposure, no Purkinje tint and no lens flare.

## Lights that are lights

- **Car lamps:**
  - The lamps sit on the body's outer face, so a car never lights itself.
  - Rear lamps follow UN ECE R7's distribution: a Gaussian 18 degrees across
    by 6 degrees up and down. Tail 5 cd, stop 80 cd, reverse 300 cd.
  - Each lamp's lens is drawn as an emitter with L = I(v) / (A cos). It uses
    the same candela that light the scene.
- **Start lights:** the `lamp` textures emit 10,000 cd/m2 at a full texel
  (LED signal heads run 5,000 to 15,000), in the G-buffer and in their flares.
- A lamp's lens shows only on its own car's body as drawn: car paint within
  35 cm. A car the game hides, such as the player's in the bumper view, shows
  no lenses.
- **Removed fakes:**
  - The game's blob shadow under the cars (texmat `shadow`).
  - The white light pool the game lays on the road ahead of the cars at
    night (texmat E2B6CDC5, tagged `shadow`): the lamps light the road.
  - The lighthouse beam polygons (removed earlier).
- **Mis-tag fixed:** the windmill's sails (texmat AAC3ACFA) were tagged `lamp`
  and glowed; they are now `wood`.

## Cost

- **Pixel budget:** the lit 3D renders at no more than 1920 x 1080 pixels,
  whatever the window. MetalFX upscales to the target, up to 3x per side on
  Apple silicon.
  - Before, a fixed 0.67 scale made a 5K screen cost three times a 1440p one.
  - At the budget, 2560 x 1920 takes about 12 ms of GPU a frame on an M4 Pro.
- **Smooth normals run on the GPU:** a GPU spatial hash in four dispatches.
  `BR_LIGHT_SMOOTHCHECK` compares them with light_smooth.c on the live frame:
  worst 0.04 degrees.
- `BR_LIGHT_CPU` times what is left on the CPU: 1.2 ms a frame at night,
  mostly command encoding.

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

## Open

- Each track's sun bearing: only Coastline's designed bearing is known; the
  others keep the map's own.
- iOS packaging of the skymaps (4K for memory). The Mac app carries the 8K
  maps in Resources/skies.
- The probes' octahedral storage error at oblique normals (above): SH per
  probe would remove it.
