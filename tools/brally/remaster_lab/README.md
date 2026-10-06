# Remastered lab scripts

Working scripts from the Remastered environment work (late September 2026),
kept for their method rather than as maintained tools:

- `track/`: the track catalogue (what the .trk files hold, about 470 prop
  types across 8 tracks), texture and instance censuses, contact sheets, the
  guard-rail tools and the collision and reachability probes.
- `bake/`: Blender helpers used while baking Poly Haven and Sketchfab models
  (LODs, UVs, materials, attribute probes).
- `textures/`: the texture sheet and tagging sweep.
- `meshy/`: the Meshy image and model generation clients.
- `view/`: a frame viewer and log tail used during headless runs.

Run from the repository root; scratch output goes to build/brally/remaster/lab/.
The maintained pipeline is ports/brally-wasm/tools (remaster_*.py) and the
recipes under ports/common/models.
