# gamedata — save files, config, string resources, CRT helpers

Confirmed in the Top Gear Rally ROM: **5 functions, 168 bytes (0.04% of the 457,392-byte `.text`)**, of which **1 are byte-exact**.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/gamedata/br_crt.c`
  - `src/core/gamedata/br_obj.c`
  - `src/core/gamedata/br_strres.c`
  - `src/core/gamedata/br_track.c`
  - `src/core/gamedata/br_volume.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
