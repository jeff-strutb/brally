# Remastered lighting: design

Status: design, 2026-10-12. Nothing here is implemented yet. Each phase in
section 12 ends committed, with its measurements in `MEASUREMENTS.md` beside
this file.

Scope: the 64-bit port, `ports/brally`, which Top Gear Rally Remastered
(`ports/remastered`) and the release builder ship on the Mac, the iPhone and
iPad, and Windows. The 32-bit lane (`ports/brally-wasm`) is deprecated as of
this design. Its experimental lighting pass (`host_fx.m` and its notes in
`docs/brally/remaster/`) is prior art: its lessons carry over, its code does
not.

Decisions already taken by the project lead (2026-10-12):

- the target is the 64-bit port; the wasm lane is deprecated;
- T0 is measured by forcing the T0 path on the M4 Pro and the iPhone 15 Pro
  Max, reported as indicative until a real T0 device is measured;
- T1 and T2 are told apart by a startup GPU calibration, never by model
  strings;
- the tonemapper is Khronos PBR Neutral.

## 1. Ground rules, as they apply here

- **Game logic is untouched.** Nothing under `ports/brally/src` (the game's
  code) or `src/brally` changes. The pipeline lives in
  `ports/brally/platform` (and a new `platform/light/` module). It reads game
  memory and never writes it.
- **Original is unchanged.** The current Glide path stays exactly as it is,
  and its frames are pixel-identical with the new code present and the flag
  off (section 11 checks it on every scripted scene).
- **One flag, live.** `BR_FLAG_LIGHTING` joins `br_flags.h`: on in the
  Remastered profile, off in Original, overridable with
  `BR_FLAG_LIGHTING=0|1`, and switched live with the profile toggle (Tab, or
  the game controller's View button) without restarting the engine. GPU
  resources are made on first use and kept, so a switch costs one frame.
- **Measured, not tuned.** Every number in the phase reports comes from the
  instruments in section 11, not from relaunching and eyeballing.

## 2. What the survey found

### 2.1 The renderer today

The game draws through Glide 2 (`platform/common/glide.c`) onto a neutral
backend interface (`platform/render/brr.h`) with Metal, Vulkan, software and
null implementations. Glide's model is screen-space triangles: by the time a
triangle reaches the backend it is in pixels with 1/w, a vertex colour, one
texture and the colour and alpha combiner state. The Metal backend
(`brr_metal.m`) is one render pass per frame into an 8-bit BGRA colour target
with a 32-bit float depth target, scaled into the window at present. There is
no HDR, no G-buffer and no lighting.

### 2.2 Where geometry comes from

The game builds N64 display lists every frame and runs them through its own
interpreter (`ports/brally/src/core/drawing/br_gbi*.c`, `br_dl*.c`). The
interpreter dispatches each command through a 256-entry handler table,
`g_brGbi0A79F0` (originally 0x100A9A58), a writable global. Its matrix
handler (`BrGbiMatrix`) keeps the "projection" slot (0x105CCD00) apart from a
10-deep model-matrix stack (0x105CCD50) and multiplies them into the combined
matrix (0x105D1760). The 32-bit lane established that the projection slot
holds view x projection and the stack holds model-to-world matrices, so world
positions are the stack top times the source vertex, directly. The vertex
handlers see source vertices with positions, texture coordinates and either a
colour (unlit geometry: the track, baked vertex lighting) or a normal (lit
geometry: cars), plus the game's one directional and one ambient light.

The game culls: it only builds lists for what the camera can see. Anything
off screen exists only in the track's loaded instance lists and the car
models' lists, which the port can walk read-only at load.

### 2.3 Game state the lighting reads

| what | where | notes |
|---|---|---|
| drivers and cars | `g_aBrRaceDriver[20]` (0x10AF07F8, stride 0x80, car at +0x60) | 20 cars at most: the stress grid is 20 |
| car transform | car+0x00, 4x4: rows forward, left, up, translation | Z-up world, about 1 unit to the metre |
| car velocity | car+0x1024 | |
| brake / handbrake / grid hold | brake torque car+0xE6C (negative when held), drive torque car+0xE68 against the direction of travel | the rule the 32-bit lane verified; the same for player and AI |
| reverse | not yet located | phase 2 finds it in `br_carstep.c` |
| car model | car+0x29C4 (the .rca record; LOD 0 body list at +0x8024) | no lamp positions in the data (2.4) |
| wheels | car+0x70+0x40*i, records +0x994/+0x57C/+0x370/+0x788 | surface byte +0x1A0, contact +0x1B4 |
| weather | `g_brCarPhysWeather` (0x104B15E8): 0 sunny, 1 fog, 2 storm, 3 snow, 4 night | whether 4 is wet or dry is written both ways in the old notes; phase 0 settles it from the code |
| lightning | timer at 0x100A79CC | |
| track | `g_brCfgChosenTrack` (0x100B3014); the .TRK instance lists; the collision mesh (0x106EECE4/EC/6C) | |
| animated landmarks | the .TRK "specials" (header +0x164, 12 bytes each, count +0x224), rotated a fixed angle per 30 Hz step | the Coast lighthouse beam is instance 64/142, kind 2, 1.25 degrees a step, flagged self-lit (0x400) |
| time | 30 Hz simulation; `BrSnapInterpDraw` blends the two newest snapshots for each 60 Hz picture | animations read the simulation step, so replays match |

### 2.4 What is missing from the data

- **Lamp positions.** The .rca models have no lamp markers. The 32-bit lane
  guessed them from the model's bounding box. Here they come from the model
  itself: the lamp lenses are textured faces, so the port finds the faces
  whose texels are lamp art (bright, low-saturation for headlamps, red for
  tail lamps; a deterministic classifier run once per model at load, its
  result logged and checkable in the debug view), and takes each lens's
  centroid and normal. The bounding box stays as the fallback, logged.
- **Light definitions** for the tracks: none. Section 7.3 adds a per-track
  light manifest.
- **A sun direction**: none. The game lights with a fixed directional light
  per weather. The manifest carries the sun (or moon) per track and
  weather; the default is the 32-bit lane's agreed direction.

### 2.5 Tests and determinism already available

`tools/brally/brbox_scripts` (sunny drive, finish, replay, snow, rain, storm,
fog, views, championship), `ports/brally/tools/suite.sh` (headless, in
parallel), the virtual clock `BR_VCLOCK` (runs reproducible to the frame),
the Glide call log `BR_GLLOG` and frame traces `BR_TRACE_FRAMES`, named
frame dumps `BR_SHOT=frame:path`, and `BR_STATS` (frames drawn and shown).
Night, a full grid, a lighthouse sweep, an enclosed section, a lightning
strike and a camera cut need new scripts.

### 2.6 Build targets and hardware here

macOS (Metal, universal), iOS and iPadOS (Metal), Windows (Vulkan, software),
headless. This Mac: M4 Pro, macOS 27, Metal 4 (T2). The project lead's
iPhone 15 Pro Max: A17 Pro (T1). No T0 device: see the decisions above.

## 3. Tiers

### 3.1 Detection

At startup, from Metal alone:

1. `supportsFamily(MTLGPUFamilyApple9)` and `supportsRaytracing`: hardware ray
   tracing (A17 Pro and M3 onward). Without Apple 9 the GPU is T0, whatever
   `supportsRaytracing` says (A15, A16, M1 and M2 trace in shader code).
2. Apple 9 GPUs are T1 or T2 by a calibration run the first time the app
   starts on a GPU (cached under its registry ID): about 50 ms of a fixed
   compute kernel and a fixed batch of ray queries against a small built-in
   scene. T2 above a measured throughput threshold, set from the M3, M4 and
   M4 Pro numbers phase 0 records.

`BR_LIGHT_TIER=0|1|2` forces a tier (the forced-T0 measurements use it).

### 3.2 Matrix

| feature | T0 (A15/A16, M1/M2) | T1 (A17 Pro/A18/A19, M3/M4/M5) | T2 (Pro/Max/Ultra) |
|---|---|---|---|
| internal resolution | 50-67% of the output, MetalFX temporal upscale | 67-77% | 77-100% |
| direct lights | clustered; 8 shadowed lights by importance, the rest unshadowed | clustered + stochastic sampling (ReSTIR DI) with ray-query shadows for every light | as T1, more samples per pixel |
| sun shadows | 3 cascades, static cached, cars overlaid, contact shadows | 2 cascades near + ray-traced far, contact shadows | ray-traced, cascades only as a fallback |
| world representation | BVH of simplified track and car proxies, traced in compute | BVH of the real geometry, hardware ray queries | as T1 |
| GI cache | DDGI, 3 cascades, 1/8 of probes updated a frame | DDGI, 3 cascades, 1/4 a frame, + ReSTIR GI final gather (optional) | DDGI, 1/2 a frame, ReSTIR GI |
| reflections | screen space, then BVH (quarter res), then probes/sky | screen space, then hardware rays (half res), then probes | full-res hardware rays where rough < 0.3 |
| volumetrics | froxels 96x54x48 | 128x72x64 | 160x90x96 |
| denoise | temporal + 2 a-trous passes | temporal + 3 a-trous; MetalFX denoised upscaler where present | as T1 |
| target | 60 fps sustained | 60 fps | 60, 120 on ProMotion when within budget |

## 4. Frame and pass graph

Captured from the game's display list as it runs (section 5), each frame:

```
capture (CPU, during the game's own list run)
  -> upload: world-space vertices, per-object matrices (this and last frame), materials, lights
G-buffer pass (render, internal res, jittered)      depth32f, albedo rgba8, normal+rough+metal rgba8, motion rg16f, emissive r11g11b10f
Hi-Z build (compute)
light culling (compute)                             froxel cluster lists (screen) + world light grid (for GI and rays)
shadows                                             sun cascades (cached static + dynamic cars), spot/point atlas by importance; T1+: none for local lights
TLAS refit/build (T1+) / proxy BVH refit (T0)       cars, specials
direct lighting (compute)                           T0: clustered + shadow maps; T1+: ReSTIR DI with ray-query visibility
GI: DDGI probe update (compute)                     trace, shade hits (surface record + direct + previous cache), blend
GI: screen-space near field + world fallback        per pixel, half res
reflections (compute)                               SSR -> world trace -> probes/sky
denoise (compute)                                   per signal: temporal + a-trous
volumetrics (compute)                               froxel inject (all lights, shadowed), integrate, reproject
composite (compute)                                 direct + indirect + reflections + emissive + sky + fog
transparent pass (render)                           game's own blended geometry, clustered forward lighting, lit particles
upscale                                             MetalFX temporal (OS 13/16+), or the port's own TAA
post (compute)                                      histogram exposure, bloom, tonemap (PBR Neutral), per-weather grade (3D LUT)
2D pass (render, output res)                        the game's HUD, text and menus as Glide draws them, untouched
```

The 2D pass is after the tonemap and the upscaler: the HUD and menus never
see TAA, motion blur, exposure or grading.

### 4.1 G-buffer: deferred, stored, thin

Rejected: a visibility buffer. Its advantages are bandwidth on dense
geometry and many small triangles. This game draws tens of thousands of
large triangles a frame (phase 0 records the counts), and a visibility buffer
would need bindless access to every vertex stream and texture in the resolve,
for little gain.

Rejected: a tile-memory-only deferred pass (G-buffer in imageblocks,
memoryless attachments, lighting in the same render pass), which is Apple's
best case for plain deferred shading. Almost every later pass here (Hi-Z,
screen-space GI, reflections, ReSTIR reuse, denoising, TAA) reads neighbours
and history, so the G-buffer has to reach memory anyway.

Chosen: a thin stored G-buffer (four targets, 17 bytes a pixel with depth).
The tile memory still helps: the G-buffer pass's depth clears and its
transient targets (the game's decal passes, alpha-tested foliage) stay
memoryless where nothing reads them after the pass.

## 5. Getting the scene: the capture seam

The game's handler table is where the port joins. For each relevant command
(`G_VTX` and its lit, unlit, decal and no-Z variants, `G_TRI1/2`, `G_MTX`, the
texture and render-mode commands, `G_DL`), the port installs a wrapper that
first calls the game's own handler with the same command (so every game-side
effect happens exactly as before), then records what the command meant:
world-space vertices (stack top x source), normals, colour, texture
coordinates, the texture, the render mode, and the object it belongs to (the
stack top's identity, the car slot or the instance index). The wrappers are
installed once at start-up and record only while the flag is on.

With the flag on, `glide.c` drops the 3D triangles the game's own leaves
hand it (they are the same triangles, already captured in world space) and
keeps everything 2D: LFB writes, textured rectangles and the remaining
screen-space `grDrawTriangle` callers, which the 2D pass draws after the
tonemap.

At track load, the same decoder walks every instance list of the track (in
each instance's matrix), the car models' LOD lists, and the specials, to build
the world representation (section 8) and the lamp lists (2.4). Read-only.

**Motion vectors**: every captured object carries its matrix from the
previous frame (matched by car slot or instance index), and the camera's
previous view x projection. Both frames are interpolated snapshots, so the
vectors are what the eye sees.

**Camera cuts**: a jump in view x projection beyond what a 1/60 s motion
allows, or a change of the game's camera mode, starts every screen-space
history afresh (TAA, denoisers, reservoirs). World-space caches survive.

## 6. Shading model

- Linear HDR (RGBA16F lighting, R11G11B10F where precision allows).
- GGX distribution with height-correlated Smith visibility (Heitz 2014,
  "Understanding the Masking-Shadowing Function"); multiple-scattering energy
  compensation (Kulla and Conty 2017; Fdez-Aguera 2019 for the split-sum
  form); Burley diffuse with the Frostbite renormalisation (Lagarde and de
  Rousiers 2014, "Moving Frostbite to PBR").
- Physical units throughout (Lagarde and de Rousiers 2014): lux for the sun
  and moon, candela (with a profile) for spots, lumens for points, nits for
  area lights and emissives. Exposure is EV100 from a physical camera
  (aperture, shutter, ISO), adapted by a luminance histogram (section 10).
- Materials: the game has none. Albedo is the 1999 texture times the de-lit
  vertex colour (6.1). Roughness and metalness come from a deterministic
  classification of each texture at load (road, dirt, grass, rock, metal,
  glass, paint, water, lamp lens), each class a fixed dielectric or metal
  preset, written in a table in the code. Wetness (from the weather, standing
  water from a puddle mask over the road map) lowers roughness and darkens
  albedo (Lagarde 2013, "Water drop: wet surfaces"); dry asphalt stays matte
  (the project lead's rule).

### 6.1 De-lighting the 1999 data

The track's vertex colours are baked light times a tint. The method,
deterministic and the same for every track:

1. Per vertex, colour c = tint x baked, with baked assumed grey and smooth
   across a continuous surface.
2. baked is estimated as the luminance of c, low-passed over the vertex's
   connected surface (neighbours within the same texture and a crease angle,
   one fixed kernel width in metres).
3. tint = c / baked, clamped; albedo = texture x tint.
4. The weather's own darkening (the game multiplies night colours down, about
   0.15 in linear) is divided out by the known factor for that weather, read
   from the game's light records.

The car models are lit by the game at run time (normals plus its two
lights), so their vertex colours are already tints: used as they are.

## 7. Direct light

### 7.1 Light types

- **Sun and moon**: directional, through the sky model: the sun's colour and
  the sky's light come from the same atmosphere (Hillaire 2020, "A Scalable
  and Production Ready Sky and Atmosphere Rendering Technique": transmittance,
  multiple-scattering, sky-view and aerial-perspective LUTs). Weather sets the
  turbidity, the cloud cover term and the moon at night.
- **Spot**: inner/outer cone, and a profile texture (an IES file converted to
  a 2D angular map, or a cookie).
- **Point**: inverse square with a smooth range window (Karis 2013).
- **Rect and tube area lights**: linearly transformed cosines (Heitz, Dupuy,
  Hill and Neubelt 2016; Heitz and Hill 2017 for lines).
- **Emissives**: surfaces flagged self-lit in the data (the 0x400 flag), lamp
  lenses and manifest-marked windows and signs: shaded as emission, and
  injected into the GI cache as sources.

### 7.2 Game-driven lights

- **Headlights**: per lamp found in 2.4, a spot with an automotive low-beam
  profile: the asymmetric cut-off of UN ECE Regulation 112 (a sharp horizontal
  cut-off with a 15-degree step up on the kerb side, the hot spot just below
  the horizon, a wide spread), drawn as a profile texture from the
  regulation's test points, about 20,000 cd peak per lamp. High beam per the
  same regulation's driving-beam points. Shadowed, in the volumetrics, and
  reflected (8).
- **Tail, brake, reverse**: red and white emitters at the rear lamps, from
  the car's own state as its physics applies it (2.3): tail about 5 cd,
  brake about 80 cd, reverse about 300 cd white (the order of magnitude of
  FMVSS 108 / ECE R7 and R23 photometry). They light the road and the cars
  behind, not only glow.
- **Lighthouse**: a spot with a narrow beam profile on the beam instance's
  live matrix (a special: rotated by the simulation step, so replays match),
  its shaft through the volumetrics.
- **Street and track lamps, windows, signs, start gantry**: from the manifest.
- **Lightning**: a scene-wide directional flash while the game's lightning
  timer runs, its direction drawn from the timer's own value (so a replay
  strikes the same way), shadowed through the sun's cascades (T0) or rays.

### 7.3 The light manifest

`ports/common/lights/<track>.json`, one per track (mirror tracks share their
base track's file, mirrored). No game data in it; ours to ship.

```json
{ "track": "coast",
  "sun":  { "dir": [3, 1, 1.1], "lux": 100000, "kelvin": 5800, "weather": [0, 1, 2, 3] },
  "moon": { "dir": [-1, 2, 2],  "lux": 0.3,    "kelvin": 4100, "weather": [4] },
  "lights": [
    { "id": "lighthouse-beam", "type": "spot",
      "anchor": { "special": 64 }, "local": [0, 0, 18.2], "dir": [1, 0, 0],
      "kelvin": 6500, "candela": 2.0e6, "profile": "beam-lighthouse", "cone": [0.5, 2.0],
      "animate": "anchor", "weather": [1, 2, 4], "shadow": true },
    { "id": "harbour-lamp-3", "type": "spot",
      "anchor": { "instance": 112 }, "local": [0.4, 0, 6.1], "dir": [0, 0, -1],
      "kelvin": 3000, "lumens": 9000, "profile": "ies/streetlight-type3", "weather": [4], "shadow": true },
    { "id": "cafe-window", "type": "rect",
      "anchor": { "world": true }, "local": [412.0, -88.5, 2.3], "size": [1.6, 1.1], "dir": [0, 1, 0],
      "kelvin": 2900, "nits": 120, "weather": [4] }
  ] }
```

Anchors: `world`, `instance` (a track instance: the light moves with it if
the game moves it), `special` (an animated landmark's live matrix). `weather`
lists the conditions it is on in. A debug view (`BR_LIGHT_DEBUG=1`, or the
overlay's key) draws every light's position, direction, cone and range,
lists them with their importance and shadow slot, and edits a light live
and writes the file back.

### 7.4 Many lights

- **All tiers**: clustered light culling (Olsson, Billeter and Assarsson
  2012; froxel clusters 16x9x24, exponential depth slices, as in Persson
  2015, "Practical Clustered Shading"), one list feeding the opaque,
  transparent and volumetric passes; a world-space light grid feeding GI
  injection and ray hits.
- **Shadow budget (T0)**: lights ranked by importance (screen coverage x
  intensity x 1/distance^2, with hysteresis so slots do not flicker), the top
  8 get a shadow-atlas slot (spots: perspective; points: octahedral). Static
  geometry is cached per slot and only cars are redrawn each frame.
- **T1 and up**: stochastic many-light sampling in the ReSTIR DI family
  (Bitterli et al. 2020, "Spatiotemporal reservoir resampling"), as UE5's
  MegaLights does it (Narkowicz and Costa, SIGGRAPH 2025 Advances): a few
  candidate lights per pixel from the cluster list, weighted reservoir
  resampling, visibility by ray query against the TLAS, temporal and spatial
  reuse, denoised. The number of shadowed lights stops being a budget.
- **Sun shadows**: cascades with cached static pages (the static track is
  re-rendered only when a cascade's window moves past a texel snap), cars as
  a per-frame overlay, screen-space contact shadows for fine detail
  (Bend Studio's 2023 method). Virtual shadow maps (UE5) were considered: a
  track is a narrow ribbon seen from low and far, and three cascades cover it
  for a fraction of the page-management cost. T1 traces the far shadow by ray
  query; T2 traces the sun everywhere.

**Stress case**: 20 cars at night, 40 headlights and 40 rear lamps on, plus
the track's lamps, holds each tier's budget.

## 8. Global illumination

### 8.1 World representation: a BVH of the real track

Chosen: Metal's ray-tracing acceleration structures over triangle geometry.
One primitive acceleration structure for the static track (built once at
load), one per car model (instanced, refit when the game dents it), one per
special, and a per-frame instance acceleration structure.

- **T1 and up**: the real geometry, traced with hardware ray queries.
- **T0**: the same API, which A15, A16, M1 and M2 run in shader code, against
  simplified proxies (the track decimated by a fixed error bound at load,
  cars as a dozen-box hull).

Each primitive carries a surface record: albedo (the triangle's mean texel
times its de-lit tint), emissive and material class. That is what a ray
shades on hit, without texture lookups on T0.

Rejected: a global signed distance field clipmap (Lumen's software path). It
is the right answer for dense scenes. This game's track is low-poly (phase 0
records the counts), a BVH of it is a few megabytes, it is exact (thin walls
stay thin, so fewer leaks), and it needs no voxelisation pass when a car
moves. Rejected: sparse voxels: memory and leaks through thin geometry at any
resolution T0 can afford.

### 8.2 Radiance cache: DDGI cascades

Chosen: dynamic diffuse GI probes (Majercik et al. 2019, "Dynamic Diffuse
Global Illumination with Ray-Traced Irradiance Fields"; Majercik et al. 2021,
"Scaling Probe-Based Real-Time Dynamic GI for Production"): irradiance (8x8
octahedral texels a probe) and depth moments (16x16) for Chebyshev
visibility, probe relocation out of walls, classification (probes in solid
geometry or empty space off), in three camera-centred cascades (spacing about
1.5, 4 and 12 m, 32x32x8 probes each). A track is a ribbon, so the cascades
are long and flat.

- **Multi-bounce**: a ray's hit samples last frame's probes.
- **Sources**: direct light at each hit from every light of section 7, with
  its shadow (the sun's cascades or a shadow ray), emissives, and the sky.
- **Updates**: a rotating subset of probes a frame, with a hysteresis that
  drops where a probe's irradiance changes fast (a headlight passing, a
  lightning strike).

Rejected: radiance cascades (Sannikov 2023). Excellent in 2D and screen
space; in 3D world space the memory for the cascades is beyond T0's budget.
Rejected as the primary cache: a hashed world-space cache (SHaRC-style). It
learns from per-pixel path tracing, which T0 cannot afford every frame. It
stays a candidate for T2's final gather.

### 8.3 Near field, final gather, and leaks

- **Screen-space near field**: a short screen-space trace per pixel (half
  res) for contact GI; a ray leaving the screen continues in the BVH, then
  falls back to the probes.
- **T1 and up**: hardware ray queries for probe updates; optional ReSTIR GI
  (Ouyang et al. 2021) for the final gather on T2.
- **Thin walls and tunnels**: exact BVH geometry (rays do not pass through),
  Chebyshev visibility plus the 2021 normal-and-view bias, probes relocated
  out of geometry and switched off inside it, and the classification's
  backface count: a probe whose rays mostly hit backfaces is inside a wall
  and off.
- **Fast-moving lights**: per-probe adaptive hysteresis, plus the screen-space
  near field, which sees a headlight's pool on the road in the frame it
  appears.
- **Camera cuts**: the cache is in world space and survives a cut. Only the
  cascades' recentring and the screen-space histories restart.
- **Lightning**: its indirect light is the flash's intensity times each
  probe's sky visibility (stored with the probe), so a strike lights the
  scene's indirect in the same frame, with no update lag.

### 8.4 Reflections

One hierarchy, roughness-aware:

1. screen-space trace against Hi-Z (Stachowiak 2015);
2. world trace: the BVH (T0, quarter res, shading the surface record), hardware
   ray queries (T1+, half res; T2 full res below roughness 0.3);
3. probes and sky.

Above roughness 0.6 only the probes. Lamps are emissive geometry in the BVH,
so a wet road reflects headlights and brake lights off screen too.

### 8.5 Denoising

Per signal (direct from ReSTIR, diffuse GI, specular), albedo demodulated:
temporal accumulation with reprojection, disocclusion detection (depth and
normal), history clamped to the neighbourhood's variance (Salvi 2016); then
a-trous spatial filtering guided by variance (Schied et al. 2017, SVGF).
Where the OS has it (macOS and iOS 26 and later, checked at run time),
MetalFX's denoised upscaler replaces the last stage on T1 and up. Reported:
flicker (frame-to-frame luminance variance on a static camera) and ghosting
(a fast car's trail length), not only stills.

## 9. Participating media

Froxel volumetrics (Wronski 2014, "Volumetric Fog"; Hillaire 2015, "Physically
Based and Unified Volumetric Rendering in Frostbite"): density per weather
(fog, rain, storm, snow), height fog, the sky's aerial perspective, lit by
every shadowed light (the cluster lists, the sun's cascades, the shadow atlas
or ray queries), temporal reprojection. The headlight beams, the lighthouse
shaft, street lamps and sun shafts all come from this one pass. The game's
rain, snow, spray and dust are lit with the same light lists.

## 10. Image

- **Upscaling**: MetalFX temporal upscaling (macOS 13, iOS 16), jittered
  rendering (Halton 2,3) and per-object motion vectors (5). MetalFX's
  denoised upscaler and frame interpolation only where the running OS has
  them (26 and later; checked at run time, never assumed: there are reports
  of the denoised upscaler failing on an iPhone 15 Pro). The port's own TAA
  where MetalFX is absent (and on Vulkan).
- **Dynamic resolution**: a controller holding the GPU time, the last line of
  defence, never the plan.
- **Exposure**: a luminance histogram (Frostbite's method), EV100 with
  per-weather limits.
- **Bloom**: a physically based energy-conserving mip chain (Jimenez 2014,
  "Next Generation Post Processing in Call of Duty: Advanced Warfare"), low
  strength.
- **Tonemap**: Khronos PBR Neutral (the project lead's choice): hue-preserving,
  colours untouched below the highlight roll-off.
- **Grade**: a 3D LUT per weather, starting neutral. Grading is art direction:
  any non-neutral LUT is shown to the project lead first.

## 11. Verification

Built first (phase 0), used by every phase:

- **Reference path tracer**: a progressive, unbiased path tracer in the
  renderer over the same BVH, materials and lights (hardware ray queries here,
  a slow compute fallback elsewhere). Real-time GI, shadows and reflections
  are compared against it on fixed frames with FLIP (Andersson et al. 2020,
  implemented from the paper, no new dependency) and RMSE.
- **Scenes**: deterministic under `BR_VCLOCK` and scripts: day (sunny), night
  with the full grid, fog, storm with lightning, snow, an enclosed section
  (the leak test; phase 0 finds which track has one), the lighthouse sweep,
  and a camera cut.
- **GPU timing**: per-pass timestamps from Metal's GPU counters at encoder
  boundaries (Apple GPUs sample at stage boundaries), an on-screen overlay and
  a log, per tier.
- **Temporal stability**: frame-to-frame luminance variance on a static camera;
  a ghosting measure on a fast car.
- **Sustained iPhone run**: 20 minutes racing with `ProcessInfo.thermalState`,
  GPU time and frame rate logged. A run on the project lead's phone is a
  deploy: asked for each time.
- **Determinism**: a hash of the simulation state every tick (cars, drivers,
  RNG, race state) and the function trace, identical with the flag on and off
  over every script, and replays and ghosts byte-identical.
- **Original unchanged**: every script's frame dumps pixel-identical with the
  new code present and the flag off, against the commit before it.

Acceptance per tier is the brief's: 60 fps sustained within budget on the
stress scenes; GI and shadows within a stated FLIP threshold of the reference
on every scene (the threshold is set in phase 0 from the reference's own
convergence noise and recorded); no leaks in the enclosed-section test;
headlights, brake lights and track lights each shown casting shadows, lighting
the scene, in reflections and volumetrics, and contributing indirect light
(a debug view isolating one light's indirect contribution); Original
unchanged.

## 12. Budgets and memory

GPU time at the internal resolution, ms (T0 at 1280x720 internal on an
A15/A16 or M1, extrapolated from forced-T0 runs until a device is measured):

| pass | T0 | T1 | T2 |
|---|---|---|---|
| G-buffer + Hi-Z | 1.6 | 1.4 | 1.2 |
| culling + shadows | 1.6 | 0.8 | 0.6 |
| direct lighting | 1.6 | 2.4 | 2.4 |
| GI (probes + near field) | 2.2 | 2.6 | 3.0 |
| reflections | 1.2 | 1.6 | 2.0 |
| volumetrics | 1.0 | 1.0 | 1.2 |
| denoise | 0.8 | 1.0 | 1.0 |
| **lighting total (brief: <= 10 on T0)** | **10.0** | **10.8** | **11.4** |
| upscale + post + 2D | 1.8 | 1.6 | 1.6 |
| frame | <= 16.6 | <= 16.6 | <= 16.6 (8.3 at 120) |

Memory, MB:

| | T0 | T1 | T2 |
|---|---|---|---|
| G-buffer + histories at internal res | 60 | 90 | 160 |
| sun cascades + local shadow atlas | 40 | 30 | 40 |
| acceleration structures | 15 | 40 | 40 |
| DDGI (3 cascades) | 25 | 25 | 50 |
| volumetrics | 10 | 20 | 50 |
| sky LUTs, light lists, misc | 10 | 15 | 20 |
| **total** | **~160** | **~220** | **~360** |

These are estimates. Phase 0 and each phase replace them with measured
numbers in `MEASUREMENTS.md`. If a feature cannot meet its budget on a tier,
the report says so with the numbers and proposes the scaled-down equivalent.

## 13. Phases

Each phase ends verified and committed, with its measurements recorded.

0. **Verification and seam.** `BR_FLAG_LIGHTING` (live), the capture seam
   (recording only, the Original path untouched), the world representation
   and lamp extraction at load (with a debug dump), the determinism hash, the
   Original pixel check, GPU timing overlay and log, tier detection and
   calibration, the reference path tracer (sun and sky first), the new test
   scripts, the FLIP and stability metrics. Settles: weather 4, the reverse
   state, the enclosed section, triangle counts.
1. **PBR and HDR base.** G-buffer from the capture, de-lit albedo, materials,
   the sky and atmosphere, the sun with cached cascades and contact shadows,
   exposure, bloom, tonemap, grade, the 2D composite, basic TAA.
2. **Direct lights and shadows.** Clustered culling, spot, point, rect and
   tube lights, the manifest and its debug view, car lights from game state,
   the lighthouse, lightning, the shadow atlas by importance; ReSTIR DI on T1
   and up. The 20-car night grid at budget.
3. **GI.** DDGI cascades over the BVH, injection of every light, the screen
   near field, T1 hardware rays, denoising, the per-light indirect view, the
   leak test.
4. **Reflections.** The hierarchy and wet roads.
5. **Volumetrics.** Froxels, weather densities, lit particles.
6. **Upscaling and sustained performance.** MetalFX (and the OS 26 features
   where present), dynamic resolution, the thermal test mode and the
   20-minute run.
7. **Polish, and Windows.** Vulkan: the same algorithms where feasible (no
   hardware ray queries assumed: the T0 paths), else the documented fallback
   (Original lighting on Windows).

## 14. Open items for the project lead

Not decided here; asked when the phase reaches them:

- the sky: the physical sky and atmosphere replaces the game's painted sky
  texture in Remastered, and the 32-bit lane's photographic panoramas are not
  carried over (art direction);
- per-track sun directions in the manifest (art direction);
- any non-neutral weather grade (art direction);
- Windows: whether lighting on Vulkan is in scope, and if so whether the
  shaders are written twice (MSL and GLSL) or through a shader cross-compiler
  (a new dependency).

## References

- Andersson, Nilsson, Akenine-Moller et al. 2020. FLIP: A Difference Evaluator for Alternating Images. HPG.
- Bitterli, Wyman, Pharr et al. 2020. Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting. SIGGRAPH.
- Fdez-Aguera 2019. A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting. JCGT.
- Heitz 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. JCGT.
- Heitz, Dupuy, Hill, Neubelt 2016. Real-Time Polygonal-Light Shading with Linearly Transformed Cosines. SIGGRAPH.
- Heitz, Hill 2017. Real-Time Line- and Disk-Light Shading with Linearly Transformed Cosines. SIGGRAPH course.
- Hillaire 2015. Physically Based and Unified Volumetric Rendering in Frostbite. SIGGRAPH course.
- Hillaire 2020. A Scalable and Production Ready Sky and Atmosphere Rendering Technique. EGSR.
- Jimenez 2014. Next Generation Post Processing in Call of Duty: Advanced Warfare. SIGGRAPH course.
- Karis 2013. Real Shading in Unreal Engine 4. SIGGRAPH course.
- Kulla, Conty 2017. Revisiting Physically Based Shading at Imageworks. SIGGRAPH course.
- Lagarde 2013. Water drop: physically based wet surfaces. Blog series.
- Lagarde, de Rousiers 2014. Moving Frostbite to Physically Based Rendering. SIGGRAPH course.
- Majercik, Guertin, Nowrouzezahrai, McGuire 2019. Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields. JCGT.
- Majercik, Marrs, Spjut, McGuire 2021. Scaling Probe-Based Real-Time Dynamic Global Illumination for Production. JCGT.
- Narkowicz, Costa 2025. MegaLights: Stochastic Direct Lighting. SIGGRAPH Advances in Real-Time Rendering.
- Olsson, Billeter, Assarsson 2012. Clustered Deferred and Forward Shading. HPG.
- Ouyang, Liu, Kettunen, Pharr, Pantaleoni 2021. ReSTIR GI: Path Resampling for Real-Time Path Tracing. HPG.
- Persson 2015. Practical Clustered Shading. SIGGRAPH course.
- Salvi 2016. An Excursion in Temporal Supersampling. GDC.
- Sannikov 2023. Radiance Cascades: A Novel Approach to Calculating Global Illumination.
- Schied et al. 2017. Spatiotemporal Variance-Guided Filtering. HPG.
- Stachowiak 2015. Stochastic Screen-Space Reflections. SIGGRAPH course.
- Wright, Narkowicz, Kelly 2022. Lumen: Real-time Global Illumination in Unreal Engine 5. SIGGRAPH Advances.
- Wronski 2014. Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering. SIGGRAPH Advances.
- UN ECE Regulation No. 112 (motor vehicle headlamps, asymmetrical passing beam); No. 7 (position and stop lamps); No. 23 (reversing lamps). FMVSS 108.
- Apple: MetalFX `MTLFXTemporalScaler` (macOS 13, iOS 16); `MTLFXFrameInterpolator` and the denoised scaler (OS 26); Apple family 9 GPUs and hardware ray tracing (Tech Talk 111375).
