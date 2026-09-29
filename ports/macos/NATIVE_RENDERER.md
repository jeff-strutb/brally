# Spec: a Mac-native renderer and frame loop for the playable port

Status: **phases 1-2 done** (spec written 2026-09-29; phases the same day,
see section 5). Scope: the playable Mac
build, `build/wasm/brally` (the 32-bit lane, `ports/macos/wasm/`). The decomp
(`src/`, `include/`) is not touched: everything here is port code.

## 1. Why

Today the port runs the decompiled game unchanged on top of emulations of
Win32, Glide, DirectInput and DirectSound (`ports/macos/wasm/host/`). The
renderer is a Glide-to-Metal translator (`host_glide.m`): it replays each
Glide call as Metal state, draws at 640x480 and scales up, and the game's own
1999 loop decides when frames happen. That is the wrong layer to fix quality
at. The measured costs:

| | now | target |
|---|---|---|
| frame start -> on screen (60 Hz, windowed) | 48-65 ms, and which one depends on start-up timing | <= 22 ms p90 |
| missed refreshes | frequent queue drift, no recovery | 0 in 10,000 frames |
| render resolution | 640x480 upscaled | the window's drawable size |
| Mac pointer vs game cursor | game cursor trails by a frame | cursor drawn at the latest pointer position |

Do not tune `host_glide.m`'s pacing by relaunching the game with different
constants. That was tried at length on 2026-09-29 and is the approach this spec
replaces. Design from the source, measure once with the instrument in section 6,
then decide.

## 2. Goals and non-goals

Goals:
1. **The port owns the frame loop.** Each frame is started by a display-timed
   loop, just in time for the refresh it targets. The game's fixed 30 Hz
   simulation is kept exactly as it is.
2. **A Metal renderer that consumes the game's display lists** (N64 GBI
   commands) directly. It replaces the Glide leaf handlers, not the Glide API
   calls. It renders at native resolution.
3. **Native input.** Mac events and GameController feed the game's input
   records directly.
4. **Behaviour identical to the original** wherever it is game logic:
   simulation, AI, timers, replays, ghosts, what is drawn and in what order.

Non-goals:
- 60 Hz simulation. The game is built on a fixed 30 Hz tick. Physics writes
  snapshots, replays and ghosts are sampled at 30/s, and the AI and control code
  carry per-tick constants. A 60 Hz tick would change handling and break
  ghosts, replays and multiplayer. The display already runs at 60 by the
  original's own design: it interpolates between snapshots (section 4.3).
- Audio. It is gated on a user decision (`ports/README.md`,
  `MUSIC-DECISION-PENDING.md`). Ask before writing any.
- The native 64-bit lane (`build.sh`, `ports/macos/metal/`). That lane is later
  work. Its `br_gfx_metal.m` 3D path already batches GBI-level draws
  (`BrGfx3dDraw`) and is prior art to borrow from, not the target.

## 3. The seam: native overrides of game functions

The 32-bit lane compiles every `src/core` function to wasm32 and translates it
to C (`w2c.py`) at its original address. Its only host seam today is the
import table: Glide, Win32, COM and CRT calls go to `host/*.c`. A native
renderer needs a second seam.

**Build first:** an override mechanism in `w2c.py` and `build_wasm.sh`.
- A native C or Objective-C function in `ports/macos/wasm/native/` declares
  which original function it replaces, e.g. `/* @replaces 0x1001FA30 BrDlCmdTri2 */`.
- `w2c.py` then points that address's slot in the dispatch table, and every
  direct call to it, at the native body instead of the translated one. Its
  existing call-site conventions (`sites.csv`, `load_conv`) already know each
  function's ABI.
- A native body reads and writes game memory through `W_P`/`W_LD`/`W_ST`
  (guest address + host base), exactly as `host/*.c` does. It can call back into
  translated game functions through the existing `w_icall*` thunks.
- An override is a port item like any other: it is listed, it has a reason, and
  removing it restores the translated body. Add a checker that fails the build
  if an `@replaces` names an address that is not placed (`placement.csv`).

This is the only new mechanism. Everything below is overrides plus new host
modules.

## 4. Architecture

### 4.1 Frame loop (new `native/frame.m`)
- A `CAMetalDisplayLink` (macOS 14+; fall back to `CVDisplayLink`) on its own
  thread supplies each refresh's target presentation time.
- The loop learns the latch lead from real presentation times (section 6):
  start the frame `lead` before the refresh it must make, read input, run due
  sim ticks, render, and present for that refresh.
- **Measured rule (2026-09-29, windowed, 60 Hz, this machine):** a frame
  submitted between refreshes R0 and R1 is shown at R2 at the earliest. The
  compositor latches just before R1. Frames submitted as little as 0.3 ms
  before R1 still made R2; some submitted 1.6-3.4 ms before R1 missed. Drawing
  a frame takes 1-2 ms (menus under 1). So start about 5-6 ms before R1, and
  expect input-to-screen of lead + 1 refresh, about 22 ms.
- **Miss recovery:** a frame that lands later than aimed means one more frame is
  queued, and it never drains while a frame goes out every refresh. Skip one
  refresh and widen the lead by 1 ms. After long clean stretches, narrow it
  slowly (never below 3 ms).
- Pitfall: Metal's `addPresentedHandler` blocks run on the **main run loop**,
  which the game spins only between frames. Their `presentedTime` values are
  exact, but they arrive late. Use them to learn, never to wait on. Command
  buffer completion handlers run on a Metal thread and are timely.
- Pitfall: `maximumDrawableCount = 2` makes `nextDrawable` wait for the
  current frame to be shown. The race then drops to one frame per 30 Hz tick.
  Keep 3; the loop, not the drawable pool, limits queue depth.
- Pitfall: fullscreen on this 5K display rendered a 5760x3240 drawable, missed
  refreshes (30 fps) and was not faster. Measure fullscreen again only once the
  renderer draws at native size on purpose.

### 4.2 How the game's loop maps onto it
- Menus: `BrMainLoopRun` (0x10019730) -> `BrAppFrame` (0x1001CF80) -> page
  frame -> 2D sprite blits -> `BrGlideFlipWait` (0x1001DD50, the only
  `grBufferSwap`). Menus draw one frame per `BrAppFrame`, 60/s, and poll the
  mouse every frame.
- Race: `BrRaceStep` (0x10019A70, certified T3; read it, do not edit it). Its
  time-sync loop at 0x1001C5EE runs one 30 Hz tick, then keeps calling
  `BrSnapInterpDraw` (0x100131E0) while the clock is behind the next tick. Each
  call blends the two newest snapshots by elapsed wall time
  (`t = ms * 0.03`) and draws through the frame driver `BrFrameDraw` (0x10011FA0), then swaps.
  So the number of draws per tick depends on how long each swap blocks.
- Design decision for the session: own this through overrides of the clock the
  loop reads (0x1006E280) and the swap (0x1001DD50), plus `BrSnapInterpDraw`'s
  `t`. Compute `t` from the target presentation time rather than wall time at
  draw. The alternative is a native replacement of the race frame driver. Pick
  after reading `BrRaceStep_10019A70.cpp` around 0x1001C583-0x1001C640; do not
  edit that file.

### 4.3 Renderer (new `native/render.m`, overrides of the Glide leaves)
The display-list machine stays game code: the loop 0x10023C90, dispatch table
0x100A9A58, load-time patch pass 0x10019040, matrices, geometry mode, lighting
maths. The **leaves** that turn its state into Glide calls get replaced.

| leaf | address | replaced by |
|---|---|---|
| unlit G_VTX (inlined projection) | 0x10021A20 `BrDlVtxPlain` | emit clip-space vertices (cx,cy,cz,cw, colour, s,t) to a batch; no CPU projection or quarter-pixel snap |
| lit G_VTX (lighting kept) | 0x10021C70, 0x10022600, 0x10022BF0, 0x100221D0 (decal), 0x10023110 (no-Z) | keep the game's lighting and matrix maths (call the translated body or port it); output clip space |
| projection | 0x10022070 `br_dl_project` | gone: the GPU projects |
| clip codes / CPU clipper | 0x10022120, 0x1001EE70 `BrDlClipTriZ` + plane routines | gone: the GPU clips |
| triangles | 0x1001FA30 `BrDlCmdTri2`, 0x1001FCF0 `BrDlVtxFinishTex` | index into the batch |
| render mode (alpha test, blend, depth, decal) | 0x10021270 `BrGlGbiCall` | pipeline-state key |
| colour combine | 0x1001E770 / 0x1001E7A0 | pipeline-state key; the mode words map to the combiner equations `host_glide.m` already implements in its shader |
| prim / env colour, fill rect, scissor, tile size | 0x1001EA80, 0x1001E930, 0x1001E720, 0x1001EBC0 / 0x1001EB50, 0x1001EC30 | uniforms / scissor in native pixels |
| texture bind / create / download | 0xDC 0x1001E2E0, 0xDD 0x1001E300, 0x100284E0, 0x10028420, 0x100272F0, 0x10027FB0, 0x100283C0, 0x100285E0, hook table 0x118ED1C0-0x118ED1DC | a Metal texture per game texture record, converted once from the game's source image, all mip levels, sampler state from the record |
| 2D sprite blits (menus, HUD) | 0x10001320, 0x100013F0, 0x10001440 (today: `grLfbWriteRegion`) | textured quads in 640x480 virtual space, drawn after the 3D pass |

Rules:
- One `MTLRenderCommandEncoder` per frame. Batches break only on a
  pipeline-state change. Pipeline states are prebuilt from the finite set of
  mode words the game uses (enumerate them from the data and the existing
  Glide stream logs).
- Depth: the original's decal passes use depth EQUAL and need exactly the
  same depth for the same vertices. With GPU projection that holds naturally
  (same inputs, same vertex function, `invariant` position). Keep a test for it
  (section 7).
- Native resolution: the 3D pass renders at the drawable size, letterboxed
  4:3. 2D is authored in 640x480 and scaled. The game's screen-space constants
  (scissor, fill rects, the 2D layout) stay in 640x480 space and are scaled at
  the leaf.
- Glide-only behaviours the game relies on are reproduced once, at the leaf:
  the LOD-bias packing (`h_grTexLodBiasValue` documents it), ALPHA_8 and ARGB
  formats, clamp modes, and alpha test against the 8-bit combined alpha.

### 4.4 Input (new `native/input.m`, overrides)
- Keyboard: write the game's key state and edge records directly. The poll is
  `BrInputPoll` 0x100706D0: records at 0x118EE9D0 (keys),
  0x118EEE50 (mouse, stride 0x1C), joystick 0x118EEBF8.
- Menu cursor: the hit test (0x10055330) reads the point at `*(0x10AC5DD8)`.
  Write the latest pointer position there at frame start (no delta feedback).
  In menus, consider drawing the Mac arrow and hiding the game's cursor sprite
  for zero cursor latency; ask the user, since it changes the look.
- Controllers via GameController.framework into the joystick record.
- Keep what already works and is committed: pointer scoped to the window,
  cursor hidden over the game view by a tracking area, the activation click
  swallowed, buttons latched per poll, and Cmd-Q / close box -> WM_CLOSE.

## 5. Phases (each ends committed, with its measurement)

1. **Override seam** (section 3), plus one trivial override (e.g. the swap
   0x1001DD50 calling the existing host swap) proving dispatch-table and
   direct-call replacement. Byte-for-byte same behaviour: the Glide stream log
   (`BR_GLLOG`) is identical before and after.
   **Done 2026-09-29.** `w2c.py --native` reads `@replaces` tags from
   `ports/macos/wasm/native/`, points direct calls and the dispatch slot at
   `n_<Name>`, and writes `w2c_native.h` (prototypes with w2c's signature, plus
   `W_ORIG_<Name>` for the translated body); a tag whose address is not placed,
   or whose name is not the function placed there, stops the build. Overrides
   are listed at the end of `build/wasm/c/w2c_report.txt`. `native/frame.m`
   replaces the swap 0x1001DD50 (reached only through the hook table, so the
   dispatch path) and the clock 0x1006E280 (31 direct call sites in 17 TUs, including
   `BrRaceStep`). Generated C differs from the no-override build only in those
   call targets and the two slots. Runs are reproducible under the host's new
   virtual clock, `BR_VCLOCK=0.25` (brbox's model: 0.25 ms per main-thread clock
   read). With it, `20_quickrace_drive`: two runs without overrides and two with
   give identical `BR_GLLOG` for race frames 1400-1430 (3,776,344 lines, 1,338
   swaps) and menu frames 240-330 (182 lines), and identical function traces
   (10,184,004 entries, repeats folded, native names mapped). Without the virtual
   clock two runs of one build already differ in the race, since the draws per
   tick follow wall time.
2. **Frame loop** on the existing Glide renderer: port-owned pacing, the race's
   `t` from the target present time. Measure section 6. Accept at <= 22 ms p90,
   0 misses in 10k frames, race at 60 presents/s, race clock = wall clock.
   **Done 2026-09-29** (`native/frame.m`, analysed by `wasm/framelog.py`).
   Race, 10,000 frames windowed: 59.85 presents/s over 167 s, clock rate
   1.00000 game ms per presented ms, latency median 44.4 / p90 57.3 / max
   75.6 ms (was 65 ms, pacing off: `BR_PACE=0`), 103 late frames. The 22 ms
   target was reached (22-28 ms, frames shown one refresh after the latch)
   only while WindowServer composited one refresh deep. For most of the day
   it composited two or three deep: a standalone probe, presenting one clear
   per refresh, showed every frame two refreshes after its latch whatever the
   submit phase (0-13 ms before), with or without skipped refreshes, in a
   window or fullscreen (this display runs a scaled mode, so WindowServer
   always composites). `CAMetalDisplayLink` (`preferredFrameLatency` 1)
   targets presentation 2 refreshes after its deadline, so lead + 33 ms at
   best; `presentDrawable:atTime:` showed frames 4 refreshes after the
   deadline. So the loop learns the depth (a miss a skip does not cure sets
   it; 30 early frames lower it) and aims the race clock at it; the 103
   late frames are the depth changes. Remaining lever, untested: a display
   mode at the panel's native resolution, where fullscreen can bypass the
   compositor.
3. **Renderer, 3D**: leaves replaced; native resolution. Accept by visual
   parity (section 7) on the scripted scenes.
4. **Renderer, 2D**: sprite blits as quads; the Glide shim and LFB emulation
   are then dead for the menus.
5. **Input**: native records and controllers.
6. **Retire** what the overrides made dead in `host_glide.m`, keeping it
   buildable behind a switch until phase 3 has soaked.

## 6. The one measurement instrument

Per frame, log to a file (not stderr, which interleaves): frame start (input
read), submit, GPU complete (command buffer handler, Metal thread), presented
time (exact, arrives late), and the targeted refresh. Analyse offline:
refreshes from submit to shown, submit phase before the latch, misses, and
latency distributions. A version was used on 2026-09-29 (`BR_FRAMELOG`, not
committed); rebuild it inside the frame loop module. Every latency claim cites
this log: n, median, p90, max, and misses.

Test windows grab focus and a GPU. Say before running them if the user might be
playing, and never leave a test instance running.

## 7. Verification

- **Before/after comparisons** run under `BR_VCLOCK=0.25`, or two runs of the
  same build will not agree once a race starts (phase 1).
- **Game logic unchanged:** `tools/brbox_scripts/*` replayed through the port
  (`BR_SCRIPT`) reach the same checkpoints. The function-entry trace
  (`BR_TRACE_FRAMES`) matches the original's (`brbox`) outside renderer leaves.
  The race clock matches wall clock.
- **Rendering:** per scripted checkpoint, a framebuffer shot compared with the
  current Glide-path shot at 640x480 (same geometry, textures, blend and depth
  results). Differences are listed and explained, e.g. resolution and filtering.
  Include a decal/depth-EQUAL scene (car shadows on the Coastline start) and
  the start lights.
- **Pacing:** section 6 numbers for menus, a race and a replay.
- **Quit:** all three paths still exit in < 0.1 s.

## 8. Known facts to reuse, not rediscover (2026-09-29)

- Menus: 60 frames/s, one swap per `BrAppFrame`, mouse polled per frame, 2D
  only (no triangles).
- Race: 30 `BrAppFrame`/s, 2 presents per tick through `BrSnapInterpDraw`, 1-2
  ms of CPU per frame.
- `happ_frame()` (w2c emits it at `BrAppFrame` entry) is the per-frame host
  hook.
- The texture handle that texture creation returns is used by the font, shadow,
  badge, tachometer and panel textures. It was lost to a `void` declaration
  until 282f9ca1. Another 178 "void callee, value used" pairs remain unaudited.
  A native renderer that keys textures by record will show that class of bug as
  a wrong texture, not a crash.
- Metal presents: see the pitfalls in 4.1.
- Commits from 2026-09-29 to build on:
  - 80be3b52: depth quantisation, mipmaps, LOD bias;
  - 623b5e11 / 174dc31e: pointer;
  - 2bcf1f59: ExitThread signalling;
  - 359449f6: two frames in flight.
