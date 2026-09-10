# Memory index

Query the tree for coverage (`tools/tiers.py`, `tools/total.py`). Do not trust
a number in prose. Procedure: `docs/MATCHING.md`. Idioms: `tools/corpus.py`.

## Open leads

### Port build is broken

`src/core/slice2_12.c` narrows `BrFixPackS16Q15Neg` to `int16_t` only under
`BR_MATCHING_BUILD`; clang stops on the port arm. Last green suite: 136/136
on 2026-08-27. Not a matching reorder — keep the port buildable.

### Colouring tail → T3, not a grind

~47 T2 rows have `reggap 0` (same instructions, registers differ). Proven
unreachable from source (permuter 0/95, refine 0/258, crank 4/536). Qualify
with `tools/t3.py --qualify`; park until the end-grind.

### 0x10019A70 — the race step (last among the big targets)

11,223 / 480,853 B of BRGlide.dll `.text` (2.33%). One original function, one
C function. Gated on 131 callee signatures. Win `sub esp, 0x34` first (ebp is
a general register, `xor ebp,ebp`; no 8-byte-aligned local). Do not tag until
the bytes diff clean. Details: `include/br_racestep.h`.

### 0x1000EAF0 BrSceneDlBuild — T3 2026-09-09, do not reopen

### 0x100250D0 BrTex3dExpand

Do not open unless named. `divergence.py --key 10`, never 6
(twelve near-identical channel arms; key 6 resyncs on the wrong copy).
A2 70 rows (limit 60), A3 34 unpaired, A4 243 B uncompared at key 6.
