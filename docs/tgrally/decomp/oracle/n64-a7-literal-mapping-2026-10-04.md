# N64 a7 literal mapping

*Recorded 2026-10-04.*

> N64 A7 failures from string-literal pointers: linker HI16/LO16 pairing fix, file literal order = source order, branch flips; audit list of unmapped files

2026-10-04 (25bd31). A7 digest = .data/.bss with thread stacks blanked (n64box.ram_digest), so pure dead stack never fails A7; it fails when a pointer or stale value lands in game state.

Two string-pointer causes found and fixed:
1. n64link paired HI16/LO16 by offset; must be relocation-TABLE order (7f6a2bba). IDO schedules another literal's addiu between a lui and its own; all .rodata literals share one section symbol. Symptom: stored pointer 0x805Fxxxx with the ROM's low half.
2. n64link maps a file's literals onto the ROM only for the longest byte-exact PREFIX of its .rodata (n64link.rom_literals). IDO emits string literals in plain SOURCE TEXT order (tested), so a file whose branches are ordered differently from the original stores annex pointers. Fix = reorder exclusive if/else arms (negate the condition) until the prefix covers the strings; also `?:` operand order and block-scope static tables (rank.c place messages). BrCarSelect: 84 -> 1132 of 1232 bytes; BrRaceGateStep certified (A7 #10, 105 bodies).

Audit (2026-10-04) of files with unmapped strings: cpak.c, ctlconfig.c (0 mapped), decalpak.c 88/848, paintshop.c 556/896, racehud.c 33/240, ifacemem.c, loadsave.c 2164/2416, racetick.c 660/848, music.c, paintscreen.c, mainmenu.c, weatherselect.c, carview.c. Only matters for non-exact (T3) bodies that STORE a literal pointer. Possible linker upgrade: per-literal mapping (beware duplicate strings in the ROM, e.g. "Handling" x2).

Scratch tools that worked: a7ram.py (peer scratch), datadiff.py (stack-blanked digest region diff at a frame), a7one.py (single-script _image_worker), flip.py (swap if/else arms; BUG it had: rebuilds from the needle's first line, so the needle must START at the if line, or it drops the preceding statement).

THREAD-LOOP A5 (user-approved 2026-10-04): n64t3.THREAD_LOOPS = {0x80257D3C}. A never-returning thread is compared per loop iteration: live arrival at the ROM's outer loop head (lowest target of a backward `b`) opens a recording, the next closes it; original runs head->head, candidate from its own entry (OS call before its head = fail) to its 2nd head arrival; memory outside the thread frame + OS calls compared, no registers. Negative control (one constant) DIVERGENT. BrMusicThread certified (A7 #14, 109 bodies), and it caught a real bug: BrMixSfx takes sfxPos in a2. --cand drafts of a multi-function file fail to link (only the draft's own symbol is known) -> UNCOVERED; run controls in-tree and restore.
REGISTER-RESIDUE LEVER (BrPakMessage 433 -> 0): the ROM shares an s-reg between unrelated short live ranges (mode then text-y; msg then panel-y). Ours reassigned one variable (y = (y+30)>>1; x = (x+16)>>1). Give each its own variable / inline the CSE'd expression, then a filler to the ROM frame.
N64 M1 at 566/567 after A7 #14; last T2 = BrPaintShopScreen (peer 8ff319 took it).
