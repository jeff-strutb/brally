# Remastered Mac port (wasm build)

<p>
<img src="docs/mac-port-coastline-original.png" width="49%" alt="Coastline at 83 mph in the Mac port with Original on: the game's own picture and its 1999 car model">
<img src="docs/mac-port-coastline-remastered.png" width="49%" alt="The same moment with Remastered on: modern lighting, sun shadows and the high-detail model of the player's car">
</p>

*The same moment of a Quick Race on Coastline, Original (left) and
Remastered (right): press ~ to switch between them live.*

This is the 32-bit lane: the first way the game ran on a Mac, and where the
Remastered lighting, car, skies and music were built. The
[native port](README.md#native-port-64-bit-cross-platform) replaces it as the game
itself; this lane remains the home of those Remastered extras until they move
over, and the reference the native port was checked against.

The game runs natively on macOS: an arm64 Mac app with a window, Metal
rendering, and the Mac's keyboard, mouse and game controllers. It is rough but
working. It boots through the splash and loading screens to the main menu, runs
the front end (menus, race and car setup, all menu art), and drives races with
the textured track, cars, shadows and HUD on screen, with music and sound
effects: Quick Race, championships through to the race, and the instant
replay of the whole race at the flag. Quitting from the menu, with Cmd-Q or
with the close box exits cleanly.

- **Native resolution.** 3D is projected, clipped and rasterised by the GPU
  at the window's pixel size (2560x1920 in a 1280x960 window on a Retina
  display), not drawn at 640x480 and stretched. The menus' 640x480 art is
  scaled with whole pixels, so it stays crisp.
- **Resize and full screen, live.** Drag the window (it keeps 4:3), use the
  green button, View > Enter Full Screen or Ctrl-Cmd-F; the game redraws at
  the new size without a restart. Above 8 million pixels (full screen on a 5K
  display) it renders at that cap and scales up, to stay inside a frame.
- **Display-paced frames.** The port starts each frame just before the
  display's next refresh and presents 60 frames a second in menus, races and
  replays. The race's 30 Hz simulation is untouched; each picture blends the
  game's snapshots at the moment it reaches the screen. Frame start to screen
  is about 40 ms while macOS composites the window, down from 65 ms.
- **Game controllers.** Any controller macOS supports (Xbox, PlayStation,
  Switch Pro, MFi) appears to the game as a joystick: choose it in the
  game's options and bind it in its Controls menu, as on Windows.
- **PC or N64, on Tab.** Tab switches the game between its PC version and
  its N64 one, live, and the version's icon shows in the top right corner
  for a moment. For now that is the soundtrack: the PC one (the disc's CD
  audio) or the N64 one, with the pieces the N64 game uses for its title
  screen and each race track, looping forever as on the N64. Both play
  every cue at once and a switch crossfades to the other where it has got
  to, so the race's PC track stays the one picked at the start; the choice
  is remembered and the two are level-matched. The game's
  own music logic decides what plays when, as on Windows: the front-end
  track, a random track per race, the next track when one ends, the Options
  jukebox and the next/previous keys. When a race ends the title piece
  comes back (the original leaves the race's track playing under the
  menus).
- **Remastered or Original, on ~.** The ~ key (left of 1) switches live
  between the game as it was and a remastered one: modern lighting,
  photoscanned ground, volumetric smoke, spray, rain and snow, track skies,
  temporal anti-aliasing and motion blur, all following the race's weather;
  every ES drawn as a modern high-detail car; and the N64 soundtrack
  re-recorded with modern instruments. See
  [Remastered lighting](#remastered-lighting),
  [Remastered car](#remastered-car) and
  [Remastered music](#remastered-music).
- **Sound effects.** The menu clicks, the engines, the start countdown and
  the race's hits and scrapes play as on Windows: the game still drives DirectSound, and the
  port mixes its buffers natively (DirectSound's volume, pan and pitch laws)
  and plays them through AVAudioEngine. Checked against the original game
  under brbox, the port starts and stops the same sounds on the same frames.
  Like the original, the effects mute while the app is in the background.
- **A self-contained app.** One command extracts everything the game reads
  from your disc image and ROM (the data track, the CD audio and the N64
  modules) and builds `Boss Rally.app` around it, with the Remastered
  car, skies, ground materials and music when they are on disk. Once
  built, the app needs none of them nor this tree: copy it anywhere and
  open it.

**How it works.** Every function in the verified M1 build (all T3 and T4
bodies) is compiled from the same `src/` tree the byte-exact build uses; no
decomp source is edited for the port. The game is 32-bit to the core: pointers
live in 32-bit fields and the original's data tables hold 32-bit code and data
addresses. So for now the port keeps that model exactly: each module is
compiled to wasm32 (which is ILP32, like Win32), the objects are translated to
C by `ports/macos/wasm/w2c.py`, which also acts as the linker, and that C is
compiled natively for arm64 against a 32-bit address space. The original
DLL's data sits at its original addresses, so every function lands at the
address the verified build gives it and the original's function-pointer tables
work unchanged. A small host layer (`ports/macos/wasm/host/`) answers the
Win32, DirectX, Glide and C runtime calls the game makes:

| Host file | What it stands in for |
|---|---|
| `host_app.m` | `main`, the window, keyboard and mouse, presenting frames |
| `host_glide.m` | Glide on Metal, modelled on the Voodoo: 16-bit W/Z depth, mip levels, filtering, LOD bias |
| `host_fx.m` | Remastered lighting: the G-buffer passes, shadows, ground materials, particles, weather, TAA and motion blur, tonemap and the ~ switch (not a stand-in; port-only) |
| `host_car.m` | the Remastered car: loads its model and draws it in Metal (not a stand-in; port-only) |
| `host_sky.m` | the Remastered skies: picks the track and weather's panorama and decodes it off the render thread (not a stand-in; port-only) |
| `host_win.c` | Win32: files (the disc image as the CD, saves), threads, timers |
| `host_dx.c` | DirectInput, DirectSound, DirectPlay as COM objects in game memory; DirectSound's buffers are mixed for real |
| `host_ear.c` | the EAR 3D sound engine the game loads by name |
| `host_crt.c` | the C runtime |
| `host_script.c` | replays `tools/brbox_scripts/` input scripts, for testing |

A second seam replaces game functions outright: a body in
`ports/macos/wasm/native/` tagged `@replaces 0xVA Name` takes that function's
direct calls and dispatch-table slot, and the build stops if the address is
not in the verified placement. That is how the Mac-native parts plug in:

| Native file | What it replaces |
|---|---|
| `native/frame.m` | the frame swap and the game clock: display-timed pacing |
| `native/render.m` | the display-list triangle leaves: GPU projection and clipping |
| `native/input.m` | reads the Mac's game controller for the DirectInput joystick |
| `native/window.m` | live resizing and full screen |
| `native/music.m` | the CD music backends (MCI and the EAR engine's CD channel): AVAudioEngine for the CD audio, libopenmpt for the N64 modules, the remastered soundtrack under ~, all at once, crossfaded |
| `native/sound.m` | plays the DirectSound mix through AVAudioEngine; replaces the WAV loader's data seek and three sound-bank wrappers the translation could not reach |
| `native/version.m` | the PC / N64 version: Tab, its icon, the remembered choice |
| `native/car.m` | the car draw (0x1000A110) and G_MOVEWORD (0x100239C0): with Remastered on, each ES's display lists are emptied and a marker puts the modern model in their place |
| `native/varblock.c` | the four state-snapshot wrappers (0x100609B0 to 0x10060A10) the translation inlined away, and BrRaceSaveLastLapInfo (0x10060A30): the last-lap snapshot the instant replay starts from, and the pause block's save and restore |

`ports/macos/NATIVE_RENDERER.md` is the design and records what each piece
measured. This 32-bit lane is interim; the
[native port](README.md#native-port-64-bit-cross-platform) is its successor.

## Remastered lighting

An optional modern lighting pass over the game's own picture, switched live
with **~** (the key left of 1). Original is the frame exactly as the game
draws it; Remastered relights that frame on the GPU. The game's rendering,
simulation and display lists are unchanged either way, and nothing under
`src/` is involved. The choice is remembered between launches (the default
is Remastered), a word in the top left corner says which one is on for a
moment after a switch, and the game never sees the key.

What Remastered adds:

- **Sun shadows.** Every opaque triangle the frame drew is drawn again from
  the sun into two shadow cascades, a sharp one around the car and one four
  times wider out to the distance, trees and fences cut out by their
  textures' alpha, and sampled with soft filtering. The sun stays fixed in
  the world as the camera turns.
- **Smooth surfaces with relief.** Normals are averaged across each
  continuous surface (real creases kept), so low-poly shapes stop lighting
  in flat facets, and each texture's own detail becomes bumps that fade
  with distance.
- **Photoscanned ground.** Six CC0 material sets from ambientCG (asphalt,
  grass, ground, gravel, rock, snow) are laid over the game's own colours
  in world space, with their normal, roughness and occlusion maps and
  parallax occlusion close up, so gravel and rock stand out of the surface
  and shadow themselves. Each surface is classified from the game's
  texture and its slope; white and yellow road markings stay. Asphalt
  loses the old textures' baked streaks and purple cast.
- **A map of the road.** The track's collision mesh, the same triangles
  and surface codes the wheels find, is drawn from above once per track,
  so the lighting knows the road in every weather: in snow it is ploughed
  tarmac with packed snow along its edges, and everything else lies under
  snow.
- **Ambient occlusion and bounce light.** Creases, wheel arches and wall
  bases darken, and nearby surfaces tint each other with their colour (one
  bounce, sampled in world space against the frame's own geometry).
- **Wet roads, only when wet.** Dry asphalt is matte: no reflections and
  no sun glints. In the wet, screen-space reflections and sun highlights
  scale with how wet the weather is: a faint sheen in fog, glossy roads in
  rain and storms, and puddles scattered over the track that mirror the
  cars and scenery.
- **Light shafts, bloom and air.** Light scattering from the sky around the
  sun, bright details glowing (lit windows and lamps at night), and distant
  surfaces taking on the colour of sunlit air.
- **Clean, colour-true grading.** A neutral tonemap (Khronos PBR Neutral)
  that leaves colours as they are until the highlights roll off, a colour
  balance per weather (cool and blue at night), and a colour-only filter
  that keeps the 1999 textures' colour speckle from being amplified.
- **Headlights, tail and brake lights.** At night, in storms and in fog
  every car's headlights throw a low-beam pattern on the road and scenery
  ahead of it in real time (a hot spot just under the horizon, a cutoff
  above, wide to the sides), with glints on wet surfaces and beams visible
  in the air. The lamps sit where each car's own model has them, and glow
  when they face the camera and nothing hides them. Tail lights glow dimly
  in the dark; the brake lights come on from the car's own controls as its
  physics applies them (the brake pedal, the handbrake, the grid hold), for
  the player and the computer drivers alike, in any light, with a red glow
  on the road behind.
- **Smoke, dust, spray, rain and snow.** The game's own puffs are replaced:
  each tyre makes smoke from its sideways slip on tarmac and dust (or
  powder in snow) from speed and slip on loose ground, read from the
  game's grip tables, as one volume lit by the sun and sky through its own
  depth. Every wheel throws spray in the wet and splashes through puddles.
  Rain and snow fall as streaks and flakes fixed in the world at four
  layers of distance, rain rings on standing water, and on the Remastered
  car rain beads on the paint and runs down the sides.
- **Tyre tracks.** Each wheel lays a track the lighting reads: pressed
  snow, darkened dirt, dry lines on a wet road.
- **Skies.** A panorama for each track and weather (35 of them) where the
  pack is installed (see [Remastered assets](#remastered-assets)), laid
  out with a photograph's proportions so the clouds are in view while
  driving; otherwise in clear weather a drawn sky with a visible sun and
  soft ray-marched volumetric clouds that drift; the hills and scenery the
  game paints along the horizon are kept. At night a deep-blue sky where
  the game leaves black.
- **Water.** The sea (painted into the game's backdrop or modelled) is
  shaded as moving water: waves that shrink with distance and calm to a
  mirror toward the horizon, the sky reflected with Fresnel falloff, and
  the sun's glitter.
- **Sharper surfaces.** 3D textures are sampled with 16x anisotropic
  trilinear filtering (mip chains generated where the game has none), so
  the road stays crisp into the distance.
- **Temporal anti-aliasing and motion blur, on the scene only.** Every 3D
  triangle is drawn with a sub-pixel jitter and the frames are resolved
  over time, following last frame's camera and each car's own motion, and
  the same motion blurs what moves. Both happen before the game's 2D is
  laid over the scene, so the HUD, text and menus never pass through them.
  A camera cut starts the history afresh.
- **HUD and menus untouched.** Anything the game draws in 2D keeps its
  exact colours.

It follows the race's weather:

| Weather | Look |
|---|---|
| Sunny | clean high-key daylight, crisp shadows filled with sky blue, light shafts, matte dry roads |
| Fog | real fog: distance and sky fade into grey, headlights on with visible beams, a faint damp sheen on the road |
| Storm | dim overcast, rain, wet roads with puddles and ripples, wheel spray, headlights on, lightning flashes light the scene |
| Snow | bright overcast, falling snow, a ploughed road through snow-covered ground, powder off the tyres, cold bounce light off the snow |
| Night | a clear, dry night: moonlight under a deep-blue sky, cool colour balance, headlights lighting the road, tail and brake lights glowing red behind every car, lit windows |

**How it works.** Alongside its colour, each frame now also records every
pixel's position in the world and its surface direction. The positions come
from the game itself: the display-list machine keeps the camera's view and
projection as one matrix, and its inverse takes each triangle corner the
game already projected back into the world (a Z-up world where one unit is
about a metre). At the swap, `host_fx.m` builds the shadow map, occlusion,
reflections, light and bloom from that record and presents the result. The
weather comes from the game's weather variable, and the storm's lightning
from its lightning timer.

**Cost.** About 20 ms of GPU per frame at 2560x1920 with everything on,
driving a Quick Race on Coastline behind the Remastered car on an Apple
Silicon Mac (`BR_GPUTIME=1`), which is more than a 60 Hz frame allows.
`BR_FX_PROF=1` breaks it down by pass. Full screen on a 5K display renders
more pixels and has not been measured.

**Limits.** Only what is on screen can cast shadows or appear in
reflections, since the game draws nothing off screen. The game's textures
and vertex colours already carry its own lighting; Remastered treats them
as the surface colour and lights on top.

## Remastered car

With Remastered on, every 4WD ES in the race (the default Quick Race car)
is drawn as a modern high-detail model of the car it stands for, a
mid-1990s Escort RS Cosworth rally car, instead of its 1999 model; ~ swaps
it back to the original on the next frame, with the rest of Remastered.
Nothing the game does changes: its physics, cameras, matrices and the
other cars are the same either way.

- **Body and wheels separately.** The body (about a million triangles) has
  no wheels; one wheel and tyre (250,000 triangles) is drawn four times on
  the game's own wheel transforms, so the wheels spin, steer and ride the
  suspension exactly as the original's do.
- **Placed by the game's hubs.** The body is scaled and seated so its brake
  discs sit on the game's four wheel hubs, which puts it at the original's
  size, wheelbase and ride height.
- **Paint and livery.** The model is painted one plain green with no badges
  or text; the red, white and blue ribbons are a separate livery texture
  projected onto the painted surfaces only, so a livery can change without
  touching the model.
- **Shading.** Metal and roughness maps, a clear coat on the paint, the
  light rig Remastered uses for the weather, and reflections of the scene
  itself (the previous frame, sampled along each reflection ray). The car
  casts sun shadows and receives them, and is anti-aliased with the scene.
- **Every ES in the race.** The player's car in its green, the others (the
  Quick Race opponent) in the opponent's orange-yellow; the livery is the
  original's own design for both, as in the game.
- **The livery.** The original's composition in red, white and blue (an
  arch over the doors with a sweep over each wheel, stripes fanning up the
  bonnet, a band over the roof, a pinstripe across the back), drawn afresh
  as smooth ribbons by `remaster_livery.py`: the 1999 textures draw it
  panel by panel for a tiny texture and break into blobs on real panels.
- **Damage.** The original dents a car by moving its model's own vertices
  in eight zones around it as it hits things. Every frame the Remastered
  car takes the same displacement, vertex for vertex, through a mapping
  `remaster_dent.py` makes, adds a finer crumple where it is dented, scuffs
  the paint through to primer and metal, and crazes the glass.
- **Levels of detail and culling.** About a million triangles out to 28 m (the
  chase camera), 300 thousand to 60 m, 20 thousand beyond and in the mirror; a
  car out of view is not drawn, and only its outer faces are. Both cars
  cost about 1.3 ms of GPU a frame at 2560x1920.
- **In the display list's order.** The game still builds the car's drawing
  commands, with its lists emptied, and a marker takes the model's place in
  the list, so it draws in the right order under whichever camera is current
  (the rear-view mirror too) and shares the scene's W-buffer depth.

**The models are not in git** (see [Remastered assets](#remastered-assets)).
They live in `ports/common/models/es/`:

| Folder | What is in it |
|---|---|
| `source/` | the full-density source models, body and wheel (about 3 million triangles each) |
| `concept/` | the concept images the models were made from, the prompts, and renders of the game's own ES from its `.rca` |
| `decimated/` | the source models after Blender's Collapse Decimate, with their own UVs and textures |
| `pack/` | what the port loads: meshes in the car frame, 8-bit RGBA maps, the livery, the shadow proxy |
| `tools/`, `archive/` | the scripts that made the sources, and earlier attempts |

Rebuilding the pack from the sources:

```bash
blender --background --python ports/macos/tools/remaster_bake.py -- ports/common/models/es/source/body_raw.blend ports/common/models/es/decimated body 1000000
```

```bash
blender --background --python ports/macos/tools/remaster_bake.py -- ports/common/models/es/source/wheel_raw.blend ports/common/models/es/decimated wheel 250000
```

The lower levels of detail are the same Blender step at lower targets, into
`decimated/lod1` (body 300000, wheel 20000) and `decimated/lod2` (body 20000,
wheel 3000). Then the pack, the windows at each level, the livery and the
dent maps:

```bash
python3 ports/macos/tools/remaster_car.py --body ports/common/models/es/decimated/body.glb --wheel ports/common/models/es/decimated/wheel.glb --out ports/common/models/es/pack --lods ports/common/models/es/decimated/lod1 ports/common/models/es/decimated/lod2
```

```bash
blender --background --python ports/macos/tools/remaster_glass.py -- ports/common/models/es/pack
```

```bash
GLASS_EDGE=0.08 GLASS_OUT=glass_lod1 blender --background --python ports/macos/tools/remaster_glass.py -- ports/common/models/es/pack
```

```bash
GLASS_EDGE=0.2 GLASS_OUT=glass_lod2 blender --background --python ports/macos/tools/remaster_glass.py -- ports/common/models/es/pack
```

```bash
python3 ports/macos/tools/remaster_livery.py ports/common/models/es/pack
```

```bash
python3 ports/macos/tools/remaster_dent.py testdata/disc/cars/es.rca ports/common/models/es/pack
```

Each level is made from the source's own `.blend` (a GLB import carries
extra seams and normals that decimate into lumps), Collapse-decimated to the
level's target and given smooth-by-angle normals.  Its reflections then
come from a subdivided copy: Blender subdivides a copy two levels and Data
Transfer gives the body that copy's normals.  The subdivided surface holds
long clean highlights where the generated one ripples under a glossy clear
coat, and only its normals are taken, because subdividing the body itself
moves it off the painted texture (the tail lights warp).  Nothing is
welded, and the source's normal map is not used: it matches only the
full-density surface.

## Remastered music

With Remastered on, the music is a remastered N64 soundtrack: Barry
Leitch's six Top Gear Rally pieces, every note, volume change and effect
taken from the ROM's own modules, played with modern instruments (sampled
grand piano, drum kit, orchestra, choir, electric guitars through an amp
model, synthesisers) and mixed in stereo. It plays over whichever version
Tab has picked, and ~ crossfades between it and that version's soundtrack
where each has got to. The game's own music logic still decides what
plays when: the title piece at the front end, one of the five race pieces
for each race's cue, each looping as the N64's do, and levelled with the
CD tracks. The instruments and their licences are listed in
`ports/common/music/CREDITS.md`. Without the soundtrack on disk, ~ leaves
the music alone.

## Remastered assets

Everything Remastered loads besides code lives in `ports/common/`, which
is kept out of git (it is large, and stored and packaged outside the
repository). `package_app.sh` copies each part into the app when it is
there; without it, Remastered falls back as listed.

| Folder | What is in it | App folder | Without it |
|---|---|---|---|
| `music/` | the remastered soundtrack: `remastered.json` (files, loop points, levels), six FLACs, `CREDITS.md` | `music/remastered/` | the version's own soundtrack |
| `models/es/` | the Remastered car: sources, decimated levels, the pack (see [Remastered car](#remastered-car)) | `remaster/` | the original car |
| `models/sky/` | the sky panoramas: `source/` pictures, one per track and weather, and the `pack/` `remaster_sky.py` makes from them | `sky/` | the drawn sky in clear weather, else the game's own |
| `models/materials/src/` | the six ambientCG sets, 2K JPEGs (colour, normal, roughness, occlusion, displacement), scaled to 1024 when packaged | `materials/` | procedural ground detail |
| `assets/` | the PC and N64 version icons (these two are tracked) | `assets/` | required |

The sky pack is rebuilt from its sources with:

```bash
python3 ports/macos/tools/remaster_sky.py
```

**Not there yet.**

- **No wheels or force feedback.** Game controllers work as a joystick;
  force-feedback wheels are not supported.
- **No network play.** DirectPlay answers as a machine with no connection
  available, so multiplayer cannot host or join.
- **Rough edges.** Expect visual differences from a real Voodoo card and
  untested corners of the game. `ports/macos/wasm/FINDINGS.csv` lists defects
  the port turned up in certified function bodies.

**Building.** On an Apple Silicon Mac:

1. Run `./setup.sh` with the reference data in place (see
   [Reference data](README.md#reference-data-you-supply-none-tracked-in-git)
   and [Reference ROM](README.md#reference-rom-you-supply-not-tracked-in-git)). The port
   needs what it stages: `orig/BRGlide.dll` and the disc extracted to
   `testdata/disc/`.
2. Run the matching sweep once, so `build/match/report.csv` exists. The port
   links the placement of the verified build, which is derived from it:

   ```bash
   .venv/bin/python tools/match_sweep.py
   ```

3. Install Homebrew's emscripten, libopenmpt and ffmpeg. The build uses only
   emscripten's LLVM (clang with the wasm backend), not the emcc driver; set
   `BR_WASM_LLVM` to use another wasm-capable LLVM `bin/` directory.
   libopenmpt is linked statically, so the built game depends on no
   Homebrew library; ffmpeg is only used when the app is packaged (encoding
   the CD audio).

   ```bash
   brew install emscripten libopenmpt ffmpeg
   ```

4. Build. This writes `build/wasm/brally`; later runs rebuild only what
   changed.

   ```bash
   sh ports/macos/wasm/build_wasm.sh
   ```

5. Or build the app instead, which runs step 4 itself. It reads
   `reference/brally/BossRally.BIN` (with its `.cue`),
   `reference/tgrally/Top Gear Rally (USA).z64` (or the paths given with
   `--bin` and `--rom`), and writes `build/app/Boss Rally.app`, about
   370 MB, or about 1.1 GB with all the [Remastered assets](#remastered-assets)
   in place (the remastered soundtrack is about 490 MB of it, the car
   180 MB, the skies 90 MB). Extraction runs once per set of sources; later
   packages reuse it.

   ```bash
   ports/macos/wasm/package_app.sh
   ```

   All the music stays lossless. The CD audio is copied from the disc
   image sector for sector into FLAC, the N64 soundtrack is the ROM's
   own modules played live by libopenmpt, and the remastered soundtrack is
   FLAC, so nothing is ever encoded to a lossy format.

**Running.** Open `build/app/Boss Rally.app`, or run the bare build from the
repo root (it reads the disc from `testdata/disc/` and the music from the
app's extract in `build/app/extract/music`):

```bash
build/wasm/brally
```

Saves go to `~/Library/Application Support/Boss Rally`. Environment variables
the host reads:

| Variable | Effect |
|---|---|
| `BR_ROOT` | repo root to load `orig/BRGlide.dll` and build outputs from (default: current directory) |
| `BR_CDROOT` | directory used as the game's CD (default `testdata/disc`) |
| `BR_HEADLESS=1` | no window; Metal still renders |
| `BR_SCRIPT=file` | replay a `tools/brbox_scripts/` input script; its `shot NAME` writes a PPM to `BR_SHOTS` (default `build/wasm/shots`) |
| `BR_SHOT_DIR`, `BR_SHOT_EVERY` | dump every Nth frame as a PPM |
| `BR_LOG=1` | log host calls to stderr |
| `BR_RES=WxH` | render at a fixed size instead of following the window |
| `BR_MAXPIX=N` | largest render target in pixels (default 8000000) |
| `BR_PACE=0` | turn the display-timed frame loop off |
| `BR_FRAMELOG=file` | per-frame timing log; `ports/macos/wasm/framelog.py file` reports latency, misses and frame rate |
| `BR_VCLOCK=ms` | virtual time (each clock read costs `ms`), so scripted runs repeat exactly |
| `BR_GLIDE3D=1` | draw 3D through the original Glide path, for side-by-side checks |
| `BR_PADFAKE=x,y,z,buttons` | a fixed controller state, for checks without a controller |
| `BR_MUSIC=0` | no music (headless runs are always silent) |
| `BR_MUSIC_DIR=dir` | where the bare build finds the soundtracks (`cd/`, `n64/`, and `remastered/`, else `ports/common/music`) |
| `BR_MUSICWAV=file` | record the music output to a file, for checks without listening |
| `BR_SFX=0` | no sound effects (headless runs are silent) |
| `BR_SFXWAV=file` | record the sound effects to a WAV file; headless, they are rendered offline in step with the game's clock (use `BR_VCLOCK` for game-time length) |
| `BR_SFXQUIET=1` | with `BR_SFXWAV`, record a windowed run without playing it |
| `BR_FX=0` / `BR_FX=1` | force Original or Remastered, overriding the remembered choice (headless runs default to Original) |
| `BR_FX_WEATHER=N` | light the scene as weather N (0 sunny, 1 fog, 2 storm, 3 snow, 4 night) whatever the race's weather |
| `BR_FX_SUN=x,y,z` | sun direction in the world (default 1,1,1.1, the game's own light direction) |
| `BR_FX_SHADOWR=m`, `BR_FX_AOR=m` | shadow-map half-width and occlusion radius, in metres |
| `BR_FX_DEBUG=N` | show one ingredient: 1 normals, 2 shadow, 3 occlusion, 4 world position, 5 bounce, 6 reflections, 7-8 shadow map, 9 the game's own colour, 10 invalid values, 11 water, 12 height bands, 13 road classification, 14 ground material weights, 15 surface classes (car, lit, sky), 16 headlight light |
| `BR_FX_NOSHAFT=1`, `BR_FX_NOSHADOW=1`, `BR_FX_NOBLOOM=1`, `BR_FX_NOLIGHTS=1`, `BR_FX_NOPFX=1`, `BR_FX_NOMAT=1` | turn light shafts, sun shadows, bloom, car lamps, particles or ground materials off, to measure their cost |
| `BR_FX_TAA=0`, `BR_FX_NOMB=1`, `BR_FX_SHUTTER=s` | temporal anti-aliasing off (FXAA instead), motion blur off, or the shutter as a fraction of a frame (default 0.5) |
| `BR_FX_STAT=1` | print shadow-caster counts, camera and weather, and GPU time per frame |
| `BR_FX_PROF=1`, `BR_GPUTIME=1` | GPU time of every Remastered pass, or of the whole frame, averaged and printed |
| `BR_FX_BRAKEPROBE=1` | print each car's speed, throttle, brake and brake-light level |
| `BR_FX_RMAPDUMP=file` | write the road map (the track's surfaces seen from above) to `file` as a PGM |
| `BR_SKY_DIR=dir` | load the sky panoramas from `dir` (default: the app's `Resources/sky`, else `ports/common/models/sky/pack`) |
| `BR_SAVEDIR=dir` | keep saves in `dir` instead of Application Support |
| `BR_FX_SHOTC=1` | with Remastered, `shot NAME` also writes the game's own colour to `NAME.c.ppm` |
| `BR_FX_TESTKEY=N,M,...` | post a real ~ key press at those swaps (a windowed check of the switch) |
| `BR_FX_AA=0` | with `BR_FX_TAA=0`, leave FXAA out too |
| `BR_CAR=0` | keep the original car in Remastered |
| `BR_REMASTER_DIR=dir` | load the Remastered car's pack from `dir` (default: the app's `Resources/remaster`, else `ports/common/models/es/pack`) |
| `BR_CAR_DEBUG=N` | show one ingredient of the car: 1 base colour, 2 paint mask, 3 occlusion, 4 normals, 5 roughness and metal, 6 diffuse, 7 reflections, 8 sun highlight |
| `BR_CARLOG=1` | print the followed car's position and its wheel hubs in the car's frame (the numbers `remaster_car.py --hubs` takes) |
| `BR_CAR_LOD=N` | draw the Remastered car at level of detail N (0 finest) whatever its distance |
| `BR_DENTLOG=1` | print the player's car's eight damage zones and how far its dents have moved the body |

The screenshots at the top of [README.md](README.md) were taken headless (the Remastered
one with `BR_FX=1 BR_RES=2560x1920`, scaled down to 1280x960, and a script
that starts a Quick Race, presses PgDn for the chase camera and holds the
throttle for ten seconds to the lighthouse):

```bash
BR_HEADLESS=1 BR_SCRIPT=menu.txt BR_SHOTS=. build/wasm/brally
```

with a `menu.txt` of `sleep 400`, `shot menu`, `quit`. Scripts can also
resize the window (`window W H`), toggle full screen (`fullscreen`), press
Mac shortcuts (`chord ctrl+cmd+f`) and drive the player's car along the
racing line to the flag (`autopilot on`). Further tracing aids
(`BR_TRACE_FRAMES`, `BR_GLLOG`, `BR_PICK`, `BR_GLSTAT`, `BR_SWAPLOG`,
`BR_MOUSELOG`, `BR_DUMP`) are documented where they are read, in
`ports/macos/wasm/host/`.
