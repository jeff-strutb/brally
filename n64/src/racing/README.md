# racing — race state machine, laps, gates, timing, results

Confirmed in the Top Gear Rally ROM: **8 functions, 360 bytes (0.08% of the 457,392-byte `.text`)**, of which **0 are byte-exact**.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/racing/br_ai.c`
  - `src/core/racing/br_racebegin.c`
  - `src/core/racing/br_racestart.c`
  - `src/core/racing/br_replayon.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
