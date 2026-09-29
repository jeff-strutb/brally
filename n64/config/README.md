# n64/config: records for the Top Gear Rally decomp

| file | what it is |
|---|---|
| `symbols_tgr.csv` | name -> address for every named function or data symbol the N64 sources reference. Names of the form `func_80XXXXXX` / `D_80XXXXXX` resolve without an entry. |
| `fenced_tgr.csv` | library code linked into the ROM, outside the target (the N64 counterpart of the PC lane's static CRT). |
| `functions_tgr.csv` | the older cross-compile survey (`n64/tools/manifest.py`); not the tier record. |

## Why the fenced ranges are library code

**zlib 1.0.4, 0x8023EDB0-0x80242940.** Every function that references zlib's
own error strings ("invalid literal/length code", "incorrect header check",
...) sits in this range, and its call graph is closed: nothing inside calls game
code except the memory helpers zlib is configured with, and the game enters it
only through `uncompress` (0x80242890). The ROM carries the version string
`inflate 1.0.4`.

**libultra and its libc/libm, 0x802607AC-end of .text.** Nothing in this range
calls a function below it (checked over the whole call graph), it contains all
18 functions in the ROM that use COP0 or cache instructions (the OS kernel),
and it holds the recognisable library members: `memcpy`, `strlen`, `strchr`,
`sqrtf`, `sinf`, `cosf`, the `guMtx*` matrix helpers. IDO links archive
members after all game objects, which is why it sits at the end.
