# Remaster fh6 levelup

*Recorded 2026-10-05.*

> Remastered Mountain level-up toward Forza Horizon 6 (on main) - plan, measured grade targets, sources, tools

2026-10-05: on main (5c427e00 landscape, 09bb4e27 package_app.sh ships it); the terrain worktree is removed, generator data moved to main's build/terrain. Weathers tuned in REAL races (race setup WEATHER clicks: 1 night, 2 snow, 3 storm, 4 fog); BR_FX_WEATHER alone leaves the game's fog colour black, so never tune fog/night with it. the project lead: current look "nowhere close to FH6"; plan agreed ("Do it"):
1 lighting/grade, 2 ground cover + road edges, 3 trees/undergrowth, 4 rocks, 5 road surface, 6 upscaler/post. Cars excluded (generated later). Perf: >=60 fps always on his Mac (FH6 does 60 there without RT), 120 target.

Reference metric: FH6 Steam screenshots (app 2483190) pulled with curl (feedback-fetch-urls-directly); rural daylight set r004 r005 r011 r018 r049 r050 r064 r093 r105 r130 r134 r139 r141 -> p5 0.006, p50 0.104, p95 0.754, log-contrast 2.07, sat 0.40, green sat 0.52, green r/g 0.83. Scratch tools metrics.py / tune.sh (shoot variants 4-up, print metrics).

Done: sunny grade = Uchimura filmic (tmo uniform), exp 1.9, sun 3.4, toe 1.7, wb 1.07/1/0.88, asphalt albedo pulled to 0.11; sun dir default (3,1,1.1) in host_fx.m AND host_car.m (BR_FX_SUN is the DIRECTION; strength knob is BR_FX_SUNK). Grass = scanned leaf ribbons from Poly Haven grass_medium_02 (tools/remaster_grass_atlas.py -> textures/grass_blades), thatch floor under grass. Asphalt edge crumble from road-map signed distance (RG16F t_rmap). Patchy shoulder in kcomp. Other weathers: filmic too, exposures from real races (snow 0.7, fog 0.65, night 0.8, storm 0.95).

Trees: PlantCatalog (Sketchfab, CC-BY) Norway spruce x5, mountain hemlock x2, ferns x3 via fetch_sketchfab.py + assets.tsv. glTF has LOD0 only; artist LODs are in the source zip -> tools/remaster_env_addlods.py, then rebake (scratch rebake_lods.sh, budgets 55000/42000/30000). Scatter tables in remaster_terrain.py SCATTER_T (trees + new `under` ferns); runtime host_terrain.m small[] = <2.5 m plants drawn to 70 m. SNM raised to 64.

Perf (vsync off: BR_VSYNC=0 now honoured under BR_VCLOCK; before that every bench was capped by the 60 Hz display): 0.5 scale + MetalFX temporal ~11.4 ms; after grass/grade 12.3 ms. Temporal upscaler smears the car (motion vectors suspected, unverified).
