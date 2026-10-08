# N64 huddialdraw

*Recorded 2026-10-06.*

> BrHudDialDraw 0x80237980 (racehud.c) T4 by hand 2026-10-06, 017b732a, gate 675/0 (session 6c2550). Levers: gbi _g homes set the frame, locals reused across branches decide colours (one web per variable), compound -= flips cfe operand order, goto into an if body for a direct cache branch

DONE: T4 017b732a, image gate 675 placed / 0 bytes. Drafts and the exact body in build/tgrally/n64/search/80237980/ (exact_h59_6c2550.c). DIFF went 811 -> 0.

Levers proven (all hand-derived from the listing, forcing used only to confirm):
- Every gbi macro `_g` block local gets its own frame home, no sibling reuse. The frame shows how many gbi blocks precede a block's locals; rev/q/tip/base live in an inner block opened after the first 7 commands.
- uopt colours a VARIABLE as one web over all its occurrences (disconnected regions too). Reusing one local in two branches gives both the same register: the bar's needle count/lit/end reuse the lamp's `ly` (a2 from the argument preference). Splitting a variable (separate `dh` for the 3/4 dial height) gives different registers.
- Locals assigned just before a call and passed (lx/ly/lw/lh) are coloured straight into a1..a3 and v0/v1. That changes ugen's argument evaluation order, the ring rotation, and the as1 schedule of later stack args. ROM v0/v1 "temporaries" for a 5th argument were such locals.
- cfe emits the heavier operand of a commutative op first (ties: mpy left first, sub right first + swp). The ONLY way to get a light variable first against an inline difference was the compound assignment `a -= rev * (rest - max) / k` (probe: tools/toolchains/ido53/cc -S on small TUs).
- uopt save = sum(contrib) / nocs, nocs = n < 3 ? n : 2 + (n-2)/4 with n = occurrence records + live blocks; double webs get x2; ties go to the lower web number. Using `a` (not the rest CSE) as the outer operand made a's piece and rest tie at 1.5, a (lower number) first: the ROM's f0/f2/f12.
- `goto read;` from the key-change block into `if (frame != cached) { read: ... }` puts the cache branch straight to the draw (no ujp block), and gives PRE a fall-through edge for the lamp-address prelude (sw v0,0x48 = its home).
- Write a sum as C + (A + B) to get emission (A + B) + C: uopt/cfe emit the simpler operand of each add first.
- Indexed global-array stores keep a never-stored global pointer in one web across a loop; `*dst++` stores kill it.
- `int` not `unsigned` for a value converted to float (no 0x4f800000 fix-up); a constant arm first when the ROM's fall-through is the constant (`if (flag != 0) v = 0x40; else v = rand`).
