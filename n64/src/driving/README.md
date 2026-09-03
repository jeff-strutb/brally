# driving — car physics, collision, surfaces, AI drivers

Confirmed in the Top Gear Rally ROM: **5 functions, 224 bytes (0.05% of the 457,392-byte `.text`)**, of which **0 are byte-exact**.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/driving/br_carphys.c`
  - `src/core/driving/br_collresp.c`
  - `src/core/driving/br_collrespsolve.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
