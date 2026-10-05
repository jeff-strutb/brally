# Boss Rally: Remastered presentation

- [heavy-jobs-crash-machine](heavy-jobs-crash-machine.md): 2026-09-30 a crash took the whole machine down: 4 parallel Blender bakes of 4-17M-tri Poly Haven trees + a full 14-way port build. Heavy jobs serial, memory-checked.
- [mac-remastered-lighting-2026-09-29](mac-remastered-lighting-2026-09-29.md): Experimental \"Remastered\" modern-lighting layer in the Mac 32-bit lane (host_fx.m), the ~ toggle, and the screenshot capture race it exposed
- [meshy-credit-budget](meshy-credit-budget.md): RULE -- Meshy textures/images cost 10-20 credits EACH at most; never plan tiling/upscale schemes that multiply credits
- [remaster-car-meshy-pipeline](remaster-car-meshy-pipeline.md): Remastered player car (Mac port) -- Meshy model pipeline rules and where the pieces live; RULE on decimation
- [remaster-environment-sourcing](remaster-environment-sourcing.md): Remastered track props/foliage sourcing plan (2026-09-30) -- Poly Haven first, Meshy for landmarks, exceptions list; where the files live
- [remaster-fh6-levelup-2026-10-05](remaster-fh6-levelup-2026-10-05.md): Remastered Mountain level-up toward Forza Horizon 6 (on main) - plan, measured grade targets, sources, tools
- [remaster-lighthouse-2026-10-01](remaster-lighthouse-2026-10-01.md): First Meshy landmark (Coast lighthouse) -- how the game animates landmarks (specials table), the two-part model plan, assets made so far
- [remaster-load-path-2026-09-30](remaster-load-path-2026-09-30.md): How Remastered assets load in the Mac port (prefetch + parallel, host_load.m), measured before/after, and two timing traps (30 fps stall debt, 4.6 s pre-race gap)
- [remaster-skies-2026-09-29](remaster-skies-2026-09-29.md): Remastered per-track, per-weather sky panoramas in the Mac port -- where sources, pack, loader and shader hook live; resolution and mapping
- [remastered-look](remastered-look.md): The project lead's taste for the Mac port's Remastered lighting -- Forza Horizon clean high-key look; dry asphalt never reflects
- [subsurf-only-for-generated](subsurf-only-for-generated.md): RULE 2026-09-30 -- the Subdivision step of the car route fixes lumpy generated (Meshy) surfaces only; authored models (Poly Haven) use their own LODs, else Collapse only
