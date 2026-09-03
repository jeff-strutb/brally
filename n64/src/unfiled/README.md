# unfiled — located, but its PC twin still lives in an unfiled address batch (a sliceN file) rather than a named module

Confirmed in the Top Gear Rally ROM: **111 functions, 9,092 bytes (1.99% of the 457,392-byte `.text`)**, of which **10 are byte-exact**.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/slice1_01.c`
  - `src/core/slice1_05.c`
  - `src/core/slice1_06.c`
  - `src/core/slice1_07.c`
  - `src/core/slice1_08.c`
  - `src/core/slice1_09.c`
  - `src/core/slice2_11.c`
  - `src/core/slice2_13.c`
  - `src/core/slice2_14.c`
  - `src/core/slice2_15.c`
  - `src/core/slice2_16.c`
  - `src/core/slice2_17.c`
  - `src/core/slice2_18.c`
  - `src/core/slice2_19.c`
  - `src/core/slice2_20.c`
  - `src/core/slice2_21.c`
  - `src/core/slice2_22.c`
  - `src/core/slice2_23.c`
  - `src/core/slice2_24.c`
  - `src/core/slice2_26.c`
  - `src/core/slice3_31.c`
  - `src/core/slice3_39.c`
  - `src/core/slice3_40.c`
  - `src/core/slice3_41.c`
  - `src/core/slice3_42.c`
  - `src/core/slice3_44.c`
  - `src/core/slice3_45.c`
  - `src/core/slice4_50.c`
  - `src/core/slice4_52.c`
  - `src/core/slice4_53.c`
  - `src/core/slice5_61.c`
  - `src/core/slice5_63.c`
  - `src/core/slice6_72.c`
  - `src/core/slice6_74.c`
  - `src/core/slice6_78.c`
  - `src/core/slice7_80.c`
  - `src/core/slice7_81.c`
  - `src/core/slice8_83.c`
  - `src/core/slice8_84.c`
  - `src/core/slice8_85.c`
  - `src/core/slice8_87.c`
  - `src/core/slice8_89.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
