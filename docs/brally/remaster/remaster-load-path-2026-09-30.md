# Remaster load path

*Recorded 2026-09-30.*

> How Remastered assets load in the Mac port (prefetch + parallel, host_load.m), measured before/after, and two timing traps (30 fps stall debt, 4.6 s pre-race gap)

2026-09-30: race load with Remastered went from 18.8 s (first race frame → env/car/materials/sky ready) to 40 ms. Coast, quick race script, headless BR_FX=1.

- `host/host_load.m`: `hload_png` (ImageIO, pixel-identical to NSBitmapImageRep on 51 asset PNGs; ~110 ms each serial), `hload_for`/`hload_rows` (dispatch_apply), `hload_mips`/`hload_flush` (one blit + one wait per batch; never a queue+wait per texture).
- host_env.m: placements parsed in 64 parallel bands with strtof (bit-identical to the old sscanf on all 5 .env files); every texture/mesh of new models is one job; clip-texture coverage search uses an alpha histogram (exact). `henv_prefetch` (from hfx_tick) reads `placements/<ENV[g_brCfgChosenTrack]>.env` + models on a serial queue the moment the track is picked; load_track waits on the group and takes it if the counts match.
- host_car.m / host_fx.m: car model, fx shaders and ground materials load once in the background at the first Remastered swap (dispatch_once; readers use acquire on g_state).

**Traps:**
- The game's race frame limiter pays back stalls: after a long load stall the next frames run unpaced until caught up. A run without the stall shows exactly 30 fps in-race. That is correct pacing, not a regression.
- The ~4.6 s pause on the car-setup click is the ORIGINAL game's deliberate wait for the confirm sound effect to play before the race loads (project lead, 2026-09-30). RULE: leave it alone for now; not a load-time bug.

Related: [mac-remastered-lighting-2026-09-29](mac-remastered-lighting-2026-09-29.md), [remaster-environment-sourcing](remaster-environment-sourcing.md).
