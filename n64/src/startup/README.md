# startup — boot, init and the main loop

Confirmed in the Top Gear Rally ROM: **10 functions, 372 bytes (0.08% of the 457,392-byte `.text`)**, of which **0 are byte-exact**.

A further 8 function(s) in this address range are `inferred` — unlocated, but bracketed by confirmed members of this module. Not counted as confirmed.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/startup/br_bootfrontier.c`
  - `src/core/startup/br_gamestep.c`
  - `src/core/startup/br_mainloop.c`
  - `src/core/startup/br_objlife.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
