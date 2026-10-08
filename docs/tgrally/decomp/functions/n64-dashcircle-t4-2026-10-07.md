# N64 dashcircle t4

*Recorded 2026-10-07.*

> BrPaintDashCircle 0x802528F8 T4 e8f896a6 (66d9b9, gate 718/0): uopt assigns spill SLOTS first-fit in web-number order; naming each point's coordinates (px, py) before its parity test renumbers the expression webs and fixes the slots

**DONE 2026-10-07, commit e8f896a6, image gate 718/0.** From eebbc5's x5 (DIFF 4: parity spill slot 0x50 vs ROM 0x4c).

Lever (read from the instrSP `spillcand/spilltemp/spillreuse` log, then one edit): uopt gives every uncoloured expression web a spill slot first-fit in WEB-NUMBER order (`forbid=` marks slots held by interfering webs), even webs that never get stored. Expression webs are numbered by first occurrence in the ucode. Simulating first-fit by hand showed the ROM's slots (parities 1-2, 3-4, 5/8 at 0x4c, 6-7 at 0x44) need each point's x and y expressions numbered before its parity. Source: `px = (x >> 1) + cx; py = (y >> 1) + cy; if ((px & 1) && px >= ax ...) BrFillPoint(px, py, c);` per point. The two "unused filler" frame words (u0, u1) were exactly px and py.

**How to apply:** when the residue is only WHICH stack slot a spilled temp uses, dump the instrSP spill records for the proc (group by `slots=0` restarts), simulate first-fit with the forbid patterns, and find the numbering that gives the ROM slots; then name the expressions in that order in source. Unused frame fillers in a draft may be the original's named temporaries. Related: [n64-aiscancorridor-wip-2026-10-07](n64-aiscancorridor-wip-2026-10-07.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).
