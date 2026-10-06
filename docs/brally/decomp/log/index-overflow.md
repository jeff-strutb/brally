# Index overflow

> Index lines moved out of the notes index on 2026-10-04 to keep it under the read limit -- targets/clusters, superseded walls, Mac/64-bit port and remaster state, older N64 sessions

Verbatim index lines (each links its topic file):

**Targets / clusters (done or in flight)**
- [BrRaceStep CERTIFIED T3](bracestep-wall.md) - 0x10019A70 (11,223B giant), oracle found+fixed a real bug. [BrFrameDraw @t3](framedraw-0x10011FA0-state.md); [BrFfb cluster @t3](brffb-cluster-2026-09-12.md); [COM vtable levers](com-vtable-levers-2026-09-10.md) - "structural wall" wasn't.
- [SetVideo.exe COMPLETE](setvideo-exe-complete.md) - 42/42, image 0 diff. [EXE decomp state](exe-decomp-state.md) - CRT linkage FF15 vs E8. [Tiny-stub census](tiny-stub-census.md); [collresp cluster](collresp-cluster-2026-09-05.md); [save-seam hand-off](save-seam-handoff.md).
- [Render frontier draining](render-frontier-draining.md) / [F3D emitter cluster](render-f3d-emitter-cluster.md) - blocked on scene-entity model. [Twenty rows 2026-09-13](session-2026-09-13-twenty-rows.md); [big-five T1 lanes](big-five-t1-lanes-2026-09-04.md); [T1 intake method](t1-intake-lane-method.md).
- [T3 lane on the largest](t3-lane-largest-2026-09-09.md) / [pool-refresh method](pool-refresh-method-2026-09-10.md) - you don't find pre-qualified rows.

**SUPERSEDED (kept for T4 lessons only - verdict changed under A5)**
- [BrTex3dExpand wall / doubling lever](brtex3dexpand-wall-broken.md) ([doubling](brtex3dexpand-doubling-lever-2026-09-10.md)) - 0x100250D0 now T3. [scenedl 0x1000EAF0 state](scenedl-0x1000EAF0-state.md) - now T3; keep the RE dossier.
- [T3 frontier map](t3-frontier-map-2026-09-10b.md) / [Gate-A distance survey](gate-a-distance-survey-2026-09-10.md) - keep the 4-class taxonomy + survey-script pattern. [Five largest 2026-09-13b](five-largest-2026-09-13b.md) / [largest-to-t3 survey](largest-to-t3-survey-2026-09-12.md) - several now T3; keep the x87 class + levers.
- [Mac Remastered lighting PoC 2026-09-29](mac-remastered-lighting-2026-09-29.md) - uncommitted host_fx.m, ~ toggle, weather 4 = night rain; port "sky corruption" in shots was a capture race (fixed)
- [Mac app + native music 2026-09-29](mac-app-and-music-2026-09-29.md) - package_app.sh self-contained .app; music DECIDED (AVAudioEngine CD + libopenmpt N64); bare-return/stub traps
- [Mac sound effects 2026-09-29](mac-sfx-2026-09-29.md) - DS mixer + sound.m; mmio/bare-return/empty-stand-in defects; brbox COMTRACE diff to verify
- [Native 64-bit portable core](native-64bit-portable-core-2026-09-30.md) - ports/brally: plays, multiplayer, Metal/Vulkan/soft, Windows build (2026-10-03); render bugs: copy host_glide.m semantics first
- [macOS 32-bit wasm lane](macos-port-32bit-wasm-lane.md) - full-boot port via wasm32->C->native, interim; native 64-bit later.
- [Mac missing-import = site attribution](mac-missing-import-site-attribution.md) - "host import not implemented" mid-game: sites.csv filed under a C draft; plus %C CD-check fix (2026-09-30)
- [obj_cpp is not scratch](obj-cpp-is-not-scratch.md) - deleting build/brally/win32/match/obj_cpp silently unplaces C++ bodies; Mac boot dies on Phase::Phase (2026-09-30)
- [N64 XM sample provenance](n64-xm-sample-provenance-2026-09-29.md) - donors traced; 8 better-quality sources in reference/tgrally/XM/ + manifest; 12 unsourced
- [N64 remaster renderer](n64-remaster-renderer-2026-09-29.md) - APPROVED stereo remaster method (Jungle done); event log + A440 retune + NAM guitars + own synth; mistakes to avoid
- [N64 session 2026-09-29](n64-session--2026-09-29.md) - VA split with peer, 17 T4 + 5 T2, parked list, claims file
- [N64 zlib + libultra lode 2026-09-29](n64-zlib-lode-2026-09-29.md) - zlib 1.0.4 (28 fns) and libultra (gu/libc -O3, os/io -O1) byte-exact from their own source; grep ROM for library strings first
- [N64 coverage scripts LANDED](n64-coverage-scripts-2026-09-28.md) - 38ffda50: 49 driver-made scripts, T4 reach 302→343/403; what is still unreached
- [Remastered load path 2026-09-30](remaster-load-path-2026-09-30.md) - prefetch + parallel loaders (host_load.m), 18.8 s → 40 ms; 30 fps stall-debt trap; 4.6 s car-setup pause is the original sfx wait, leave it
- [Remastered lighthouse 2026-10-01](remaster-lighthouse-2026-10-01.md) - landmark animation = specials table (rigid spin/fly-past); tower + rotating optic plan; .blend via Remesh convert_format_only
- [Remastered skies 2026-09-29](remaster-skies-2026-09-29.md) - 35 per-track/weather panoramas; host_sky.m + compfs pano; 1280px per 60deg copy x6; committed
- [Verify display on screen](native-64bit-portable-core-2026-10-03-addendum.md) - BR_SHOTS shows the target, not the screen; screencapture fullscreen + window for aspect work

## Moved from the notes index 2026-10-05 (verbatim)
**C++ lane**
- [C++-lane T3 filing workflow](cpp-lane-t3-filing-workflow.md) - file→cpp_sweep→--qualify→@t4-pass→@t3. [class cracks](cpp-lane-class-cracks-2026-09-12.md) - /Od intake vein; [EH lane](session-2026-09-13b-cpp-eh-lane.md) - 5 T4 ctors + 4 T3; [vcall family lode](cpp-vcall-family-lode.md).
- [CarState decode BYTE-EXACT](carstate-decode-cpp-2026-09-12.md) - family first. [C++ owns top C targets](cpp-lane-owns-top-c-targets.md) / [retire the C twin](cpp-twin-retire-chore.md) / [vptr hoist = C++](frontend-screen-vptr-hoist.md).
- [C++ EH screening](cxx-eh-frame-wall.md) / [thiscall](cxx-thiscall-wall.md) - RESOLVED via the C++ lane (0x1003FA00), not a wall.
**Corpus**
- [Corpus query tool](corpus-query-tool.md) - a MISS is a result. [External corpus LIVE](ext-corpus-2026-09-13.md) - 1588/3956 byte-exact; C2/ext names NEVER in-tree. [ext2 candidates](corpus-candidate-verdicts-2026-09-13.md); [CRT verdicts](crt-corpus-verdicts-2026-09-13.md); [VC5 idiom dictionary](vc5-idiom-dictionary.md).
