# Brtex3dexpand t3

*Recorded 2026-09-16.*

> 2026-09-16: 0x100250D0 BrTex3dExpand (8480B, the largest ex-T2) CERTIFIED T3 by a fresh hand transcription + A5 oracle EQUIVALENT. The byte-grind walls in the older brtex3dexpand notes are now MOOT (A5 supersedes byte-shape).

** 0x100250D0 BrTex3dExpand is T3 (commit b04b40a7).** Fresh hand transcription
from the disasm (discarded the permuter-tuned body + 500-line dead-lever comment),
filed at `src/brally/core/drawing/br_tex3d_expand.c`, certified by the A5 oracle
EQUIVALENT across 64 valid-state seeds. This SUPERSEDES the byte-grind framing in
[brtex3dexpand-doubling-lever-2026-09-10](../levers/brtex3dexpand-doubling-lever-2026-09-10.md) and [brtex3dexpand-wall-broken](brtex3dexpand-wall-broken.md):
those documented a colouring/allocation wall that never mattered for T3 - A5 is
authoritative and byte-shape is behaviour-neutral ([t3-certified-standard](../rules/t3-certified-standard.md)).
Do NOT reopen for the byte grind unless the project lead names it for T4.

**The method (same as bracestep-wall / [cpp-lane-t3-filing-workflow](../cpp-lane/cpp-lane-t3-filing-workflow.md)):**
transcribe from the bytes → match_sweep → oracle profile → A5 EQUIVALENT →
2 honest @t4-pass ledger lines → t3.py --qualify emits @t3.

** REUSABLE: oracle profile for a BUFFER/format function (not global-graph like
BrRaceStep).** BrTex3dExpand's inputs are ARGS: dst/src are `ptr` (auto-buffers),
but the palette and tile-record base are typed `int` yet USED as pointers. Pin
them (arg hook) to BSS scratch addresses (0x10E00000 range = is_bss, routes through
bss_byte) and seed their fields in the bss hook. Tile-record shift fields
(+0x20/+0x24/+0x60/+0x64) become `1<<n` loop counts → seed them SMALL (2) or the
run never terminates; source-offset/stride fields (+0x08/+0x0c/+0x48) → 0 so the
source cursor stays in its buffer. Bound cbMax (arg1) to the 0x100-byte dst
buffer. Vary mode/format/sub/flags/mirror/interleave per seed to cover every arm.

** THE TRAP that cost the most (write this on every buffer profile): `zero_stack`
defaults to TRUE, which zeroes the arg BUFFERS, not just null-safety - so the
SOURCE buffer is all zeros and source-driven bugs are INVISIBLE.** Two injected
bugs (nibble index, blend shift) both passed as EQUIVALENT until I set
`zero_stack=False` (varied source) AND seeded the palette with distinct-per-index
bytes. ALWAYS run negative controls (inject a bug, expect DIFF) before trusting an
EQUIVALENT - a profile that seeds a dead world gives false EQUIVALENTs, exactly the
failure oracle-profiles.py warns about. Palette bugs need a varied palette:
uniform-0 palette makes every index read 0.

**Emulator gap fixed (tools/brally/x87emu.py, committed b04b40a7):** one-operand `mul`
and one-operand `imul` (edx:eax) - the MSVC `/255` magic-divide uses `mul`. Was
UNCLASSIFIED "unhandled mul edx" until added. No regressions (certified sweep 0 DIFF).

**Stale-obj trap (same as [cpp-lane-t3-filing-workflow](../cpp-lane/cpp-lane-t3-filing-workflow.md)):** 52 leftover permuter
probe objs (A1_*.obj, W_one_eq_iVar3.obj, ...) in build/brally/win32/match/obj_O2 carried the
BrTex3dExpand symbol at 9632 B and shadowed the real 8224 B obj via _obj_index's
setdefault → "substituted bytes bury <neighbour>". Delete every obj carrying the
symbol except the real <file>.obj.

Residue (T4-only, do not chase): frame sub esp,0x50 vs 0x68 (mine uses FEWER
slots), pinned-1 constant family, byte-slot nibble-merge widening (uVar19 is
LOAD-BEARING - dropping it regresses 73→128 rows), loop-rotation jmps.
