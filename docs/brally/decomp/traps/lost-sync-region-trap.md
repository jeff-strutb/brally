# Lost sync region trap

*Recorded 2026-09-03.*

> divergence.py used to STOP at an unresyncable block and still print a region total - every 0x100250D0 region map before 2026-09-03 covered only two thirds of the function

 MEASUREMENT TRAP, found and fixed 2026-09-03 (commit b72676b).
`tools/brally/divergence.py` searched only 400 instructions ahead for a resync. Past
that it printed `... lost sync at orig+X`, **stopped**, and still printed a
region total - so the number looked complete and was not.

On **0x100250D0 (BrTex3dExpand)** it lost sync at orig+0x15b8 at EVERY key
(6, 8, 10, 12, 14). Twelve sessions of that function's dossier quoted maps
measured on orig `0x0..0x15b8` and had never compared the last 2,920 bytes
(34%) - which hold its largest reliable per-block drift, -50 at 0x1a4c.

**How to apply:** the tool now re-anchors with a global KEY-gram index and
prints ` N lost-sync gap(s): B orig bytes (P%) were NEVER COMPARED`. Read
that line before quoting any region count, in this project's dossiers or your
own. Honest post-fix numbers for that function: 31 regions at `--key 10`
(was 20), 52 at `--key 6` (was 32).

**Why:** a gram index must mask exactly what the byte comparator ignores  - 
keeping branch targets in the key made the re-anchor overshoot 1,177 bytes,
because a ten-instruction window in this code nearly always contains a jcc.
And a stretch that stays uncompared is a *diagnosis* (no run of KEY matching
instructions exists there), not a tooling gap.

Same family as [instruction-count-padding-trap](instruction-count-padding-trap.md) - a tool's headline number
that quietly measured the wrong thing. See also [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md).
