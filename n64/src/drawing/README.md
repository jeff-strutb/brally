# drawing — display-list building, sprites, fonts, image blitting

Confirmed in the Top Gear Rally ROM: **12 functions, 528 bytes (0.12% of the 457,392-byte `.text`)**, of which **1 are byte-exact**.

The source that produced these is shared with the PC decomp and lives in:

  - `src/core/drawing/br_dl.c`
  - `src/core/drawing/br_dlglide.c`
  - `src/core/drawing/br_gbitexscan.c`
  - `src/core/drawing/br_img.c`
  - `src/core/drawing/br_imgblit.c`
  - `src/core/drawing/br_tex3d.c`
  - `src/core/drawing/br_textmode.c`
  - `src/core/drawing/br_uiimg.c`

N64-only code for this area, once recovered, belongs in this folder. Per-function detail is in `n64/config/functions_tgr.csv`.
