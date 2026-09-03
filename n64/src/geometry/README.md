# geometry — vector, matrix and quaternion math

Confirmed in the Top Gear Rally ROM: **24 functions, 1,756 bytes (0.38% of the 457,392-byte `.text`)**, of which **8 are byte-exact**.

A further 13 function(s) in this address range are `inferred` — unlocated, but bracketed by confirmed members of this module. Not counted as confirmed.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/geometry/br_bits.c`
  - `src/core/geometry/br_mat.c`
  - `src/core/geometry/br_vec.c`
  - `src/core/geometry/br_vecnorm.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
