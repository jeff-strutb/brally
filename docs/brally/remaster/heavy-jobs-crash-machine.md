# Feedback heavy jobs crash machine

*Recorded 2026-09-30.*

> 2026-09-30 a crash took the whole machine down: 4 parallel Blender bakes of 4-17M-tri Poly Haven trees + a full 14-way port build. Heavy jobs serial, memory-checked.

2026-09-30: running four `remaster_env_bake.py` Blender jobs at once (Poly Haven pine/fir/island trees: 4-17M triangles each, Collapse to 1M then Subdivision Surface -> ~4M, several GB RAM apiece) while a full `build_wasm.sh` (14 compile jobs) ran and other sessions were busy ran the 64 GB machine out of memory and crashed it. the project lead: "you overwhelmed the computer and crashed."

**Why:** the "use the 14 cores" rule (feedback-parallelize-long-runs) is for light CPU-bound jobs (compiles, oracle runs), not multi-GB-each Blender work; and other sessions share the machine.
**How to apply:** Blender bakes of big source meshes run ONE at a time (xargs -P 1); measure the first one's peak RSS (`/usr/bin/time -l`) before ever running two. Never start a full port build while a bake runs. Check `ps`/ListAgents for busy peers first. Related: [remaster-environment-sourcing](remaster-environment-sourcing.md).
