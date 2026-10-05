# Feedback subsurf only for generated

*Recorded 2026-09-30.*

> RULE 2026-09-30 -- the Subdivision step of the car route fixes lumpy generated (Meshy) surfaces only; authored models (Poly Haven) use their own LODs, else Collapse only

the project lead (2026-09-30): "We don't smooth to make the world 'round', we smooth to make models look correct for what they are. If our car models weren't so 'mumpy' it wouldn't be necessary." And: "you tell me" -- decide from the source's quality and the Forza target instead of asking.

**Rule:** Subdivision Surface (the car route's Collapse 200k -> Subsurf L3 -> Collapse) is ONLY for generated models whose surface is faceted/lumpy (Meshy cars, Meshy landmarks). Authored/photoscanned-and-retopologised models (Poly Haven) keep their surface: use the asset's own hand-made LOD objects (`<asset>_<variant>_LOD<n>`) when one fits the triangle budget, with its own normals; otherwise Blender Collapse Decimate from the nearest finer LOD + smooth_by_angle. Never hand-rolled mesh ops ([remaster-car-meshy-pipeline](remaster-car-meshy-pipeline.md)).

**Why:** subdividing clean assets adds nothing, rounds intended hard edges, and cost 12-25 GB per job (helped crash the machine, [heavy-jobs-crash-machine](heavy-jobs-crash-machine.md)). Measured: authored-LOD route on searsia_lucida 10 s / 1.7 GB vs 7.5 min / 12 GB.
**How to apply:** `ports/macos/tools/remaster_env_bake.py` defaults SUBSURF=0; set SUBSURF=3 only for a generated model. Related: [remaster-environment-sourcing](remaster-environment-sourcing.md), [remastered-look](remastered-look.md).
