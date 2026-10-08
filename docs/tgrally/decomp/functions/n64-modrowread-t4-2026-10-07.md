# N64 modrowread t4

*Recorded 2026-10-07.*

> BrModRowRead 0x80256DEC T4 1505ad5d (6f73ff): no-color constant webs count as interferers; arpOn = 1 replaced a code-less load temp; index via the chain var

BrModRowRead (music.c) T4 1505ad5d, gate 733/0, 2026-10-07 by session 6f73ff, from bfa2d4's 3-word draft ([n64-modrowread-bfa2d4-2026-10-07](n64-modrowread-bfa2d4-2026-10-07.md)).

Two edits, both removing stand-ins:
- `D.arpOn = 1;` (was `n * 0 + 1`): the literal creates a constant web that uopt leaves uncoloured (trace decision=no-color, save ~0.8, nocs 12), emits no code, but still counts in numintf of every web it overlaps. That kept webs 34/33/172 (row ptr, offset, end) at intf 22 (the p1 = constrained set) after the next edit removed a web.
- Note block index `D_803787D0[inst - 1]` with `inst = D.smp = D.inst;`: the raw `ilod D.inst` no longer has two uses (chain cvtl + index), so its code-less CSE temp (web 148, save 30, lower web number) is gone and the sample-pointer CSE gets t0.

Either edit alone fails (b0a 241, b1 134); together EXACT. bfa2d4's "constant candidate would push table bases out of s0-s3" held only while web 148 also existed.

**How to apply:** when a code-less web wins a save tie on web number, look for a no-code web elsewhere that supplies the same interference (no-color constants, chain cvt temps) so the culprit can be removed. Webs are numbered by first occurrence, so a declared local does not get a low number (tested, and a pointer var loses cfe's indexed operand order). Tool: build/tgrally/n64/search/80256DEC/s6f/web.py (CDX_DETAIL_WEB dump), capuc.py for uopt ucode (Rmt length = register). Told ad0088 for AiScanCorridor ([n64-aiscancorridor-wip-2026-10-07](n64-aiscancorridor-wip-2026-10-07.md)).
