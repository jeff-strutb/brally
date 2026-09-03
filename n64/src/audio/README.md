# audio — music and sound-effect mixing, sequencing, output

Confirmed in the Top Gear Rally ROM: **14 functions, 572 bytes (0.13% of the 457,392-byte `.text`)**, of which **2 are byte-exact**.

A further 8 function(s) in this address range are `inferred` — unlocated, but bracketed by confirmed members of this module. Not counted as confirmed.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/audio/br_audio.c`
  - `src/core/audio/br_mix.c`
  - `src/core/audio/br_musiccmd.c`
  - `src/core/audio/br_sfx.c`
  - `src/core/audio/br_sfxsrc.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
