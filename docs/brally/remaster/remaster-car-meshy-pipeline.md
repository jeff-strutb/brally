# Remaster car meshy pipeline

*Recorded 2026-10-01.*

> Remastered player car (Mac port) -- Meshy model pipeline rules and where the pieces live; RULE on decimation

RULE (2026-09-29): high-poly models go through Readington's established path -- raw (NOT remeshed) Meshy output, **Blender Collapse Decimate only**, keeping Meshy's own UVs and textures. Do not remesh via Meshy (`should_remesh` makes panels lumpy), do not hand-roll mesh processing (normal smoothing, AO splats), do not add the unwrap+rebake step (that is Readington's tree-only fix; on Meshy's seam-split soup it used 0.3-2% of the atlas).

**Why:** the project lead: "Use THAT, but to much higher quality than Readington since we don't need to support the web right now" -- after I burned rounds on remesh artifacts and a rebake that fragmented.

APPROVED 2026-09-30 (the project lead: "Soooo much better"): Collapse to target -> smooth_by_angle -> Subdivision Surface level 2 on a COPY, Data Transfer its normals to the body (subdividing the body itself warped the tail-light texture at every level, 2026-09-30; the project lead's subsurf idea). This fixed the glossy-paint ripples; Laplacian normal-fairing and more decimation targets did not. Subsurf of the full 3M needs 188 GB (killed). Matte viewers hide ripples: always judge with a glossy matcap. zsh does not word-split $v in loops: use ${=v}.

SOURCE MUST BE Meshy's .blend (opened with wm.open_mainfile), NOT the API GLB: the API only serves glb, and a GLB import carries seams/custom normals that decimate into lumps (2026-09-30 mumps). the project lead CANNOT get .blend from the web: I source it myself via the Remesh API, `{"input_task_id": <task>, "convert_format_only": true, "target_formats": ["blend"]}` -> model_urls.blend, 5 credits, no remesh (same welded Mesh_0/Material_0/packed Image_0-2 layout as the car's). Script: ports/common/models/lighthouse/tools/toblend.py (2026-10-01). Image-to-3D: car settings = geometry_resolution 2k, texture 4k, enable_pbr, no remesh, 35 cr; "Meshy 7" = ai_model meshy-7.1.

The Readington route: Collapse decimate -> `shade_smooth_by_angle` (45 deg) crease normals (NO weld: Readington welds only re-baked meshes; welding a Meshy model warped its tail-light texture 2026-09-30), geometry-only export; and no normal maps (Meshy's map is baked for the raw surface and lumps on any decimated level). Skipping the normals step shipped a "mumps" car on 2026-09-30 (kept verts carry raw normals 15-23 deg off). the project lead: "We solved these problems in Readington. Refer to that" -- read readington/docs/assumptions.md + scripts/blender-decimate.py BEFORE inventing.

RULE (same day): never name Meshy (or any AI generator) in tracked repo files, README included; say "the models" / "the source model". the project lead will cite them himself if he wants. The gitignored `ports/common/models/` may hold Meshy task ids and scripts.

**How to apply:** `ports/brally-wasm/tools/remaster_bake.py` (Blender, decimate only) -> `ports/brally-wasm/tools/remaster_car.py` (pack to .rcm + RGBA PNGs, fit body to game hubs) -> `ports/common/models/es/pack/`. Runtime: `host/host_car.m` + `native/car.m` (marker `BC000008 CA5Ennnn` in the display list via `@replaces` BrCarDrawVehicle/BrGbiMoveWord); shown only while `hfx_on` (~ toggle). Meshy key: `~/.config/readington/meshy.env`; Meshy API supports image-to-image and multi-image-to-3d (images as data URIs). Concepts must be the CONCEPT (Escort RS Cosworth for TYPE-ES) in Forza quality, plain paint, no logos/text, livery as a separate texture, wheels generated separately (body wheel-less). Related: [native-renderer-not-hacks](../port/native-renderer-not-hacks.md).
