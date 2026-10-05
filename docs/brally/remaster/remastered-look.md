# Feedback remastered look

*Recorded 2026-10-01.*

> The project lead's taste for the Mac port's Remastered lighting -- Forza Horizon clean high-key look; dry asphalt never reflects

2026-09-29 the project lead judged the first Remastered pass "dirty, not super high quality" and pointed at Forza Horizon 6 screenshots as the bar: clean high-key exposure, near-white luminous skies and backlit haze, long crisp shadows filled with sky blue, colour-true grading, deep-blue (never black) night skies with glowing tail lights and wet glossy roads.

Then: "asphalt should only be reflective when it's wet and based on how wet it is. Asphalt isn't reflective at all when dry."

**Why:** realism judged against modern reference games; muddy grades, grain, greyed tonemaps and dry-road sheen read as cheap.
2026-09-30: the same Forza bar applies to EVERY Remastered asset, including the ~470 track prop types (trees, vines, buildings, signs) -- "really high quality wherever we get them". So no stylised low-poly libraries, no upscaled 32px N64 textures on rebuilt geometry.

2026-09-30 night: "oversharpened, dirty, gritty... FH6 is smooth, a hyper-realistic toon look." Measured cause: the photoscanned ground materials at full contrast and full res (black aggregate pixels, 0.9 normals, POM to 30 m) raised pixel-level detail ~5x (worse at night: headlights graze it); TAA+motion blur smear that grit into "ghost" streaks. The project lead had called it ghosting and it kept "coming back" because each fix patched TAA/MB edges while more micro-detail kept being added. Fixed in host_fx.m: scan detail at 80% contrast near, easing over 8-60 m to 25% (asphalt) / 35% with a coarser mip; normals 0.35 fading 10-60 m; POM 6-20 m; asphalt brightness from a wide same-surface average (kills the 1999 texture's baked streaks). PROJECT: a fade "too close" is a visible band -- fades must be far and gradual. The asphalt scan itself is fine (no Meshy needed). FH6 asphalt = even dark grey, pale pebbles visible up close only, crisp markings.

2026-10-01 night (PROJECT): FH6 night = much higher contrast between lit and unlit, with unlit BLACKER IN TONE (neutral), not just darker and never blue; still readable. Measured: our blue night came from the PBR Neutral tonemap toe (subtracts min channel; near-black slightly-blue grey turned saturated blue), not the rig; fixed with a hue-preserving toe below x 0.08. Also night bloom 0.3 laid sky blue over darks (now 0.07). Original game's night light tints are near-neutral (dir DD,EE,FF; amb 3C,39,36). Headlights: warm, lit right ahead of the bumper, wide; spill above cutoff lights trees; host_env trees pass albedo in ga (alpha 1) so compfs lights them with lamps.

**How to apply:** no reflections or sun highlights on dry roads; scale wet effects by a wetness amount per weather; prefer neutral (hue-preserving) tonemapping, clean chroma, bright air over grey fog. Compare against the project lead's references, not the original game. Related: [mac-remastered-lighting-2026-09-29](mac-remastered-lighting-2026-09-29.md).
