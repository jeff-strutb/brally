# N64 decalpak transfer

*Recorded 2026-10-05.*

> BrDecalPakTransfer 0x80248F88 (image.c) T4 2026-10-06 e777ec00 (session eba7ae): the last as1 stall-fill residue was a text-x VARIABLE (tx = 114; save arm tx++) - a constant def through a variable puts li a1 ahead of the string la

**DONE 2026-10-06, commit e777ec00, image gate 677/0.** BrDecalPakTransfer (src/tgrally/drawing/image.c, 7.4 KB, the largest N64 T3).

**The closing lever (general, reusable):** ROM had SAVING's `li a1,0x73` in the slot before `bne op,9` where ours put the string's `lui a0`. Source that gives it: `tx = 114; if (op == OP_LOAD) Print("LOADING", tx, ty); else { tx++; Print("SAVING", tx, ty); }`. `tx++` is a *def* of a variable coloured a1, so uopt keeps `li $5,115` at the arm start, ahead of the `la $4` par; as1 then hoists it (first eligible target record). A literal, `x + 1`, or `x = 115` all const-propagate back into the par slot (la first). Found with a 10-line probe (pr/p3.c): when a constant argument lands out of order, try the argument as a variable modified in the arm (++/--/+=).

Kept from earlier sessions: case 7 head `ty = 0x7C; if (ty); top = 0xDB;` on one line (zero-footprint statement, [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md)-class; the natural two-statement form colours top into a1, DIFF 1176). Decal step reads m->parts[...] directly; mask index through a local; `row * stride` order.

Toolkit notes still valid: pipe.sh (cfe/uopt/ugen/as1 by hand, name uopt output .UO not .O on macOS), bd.py binasm dump, bx.py binasm edits, xd.py grade an .obj; as1 stall-fill = first target-block record whose dest is not live-in at the fallthrough (as1.c func_42a028 scans and accumulates skipped records' def/use).
