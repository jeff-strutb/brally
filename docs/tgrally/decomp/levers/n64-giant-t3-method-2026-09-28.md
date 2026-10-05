# N64 giant t3 method

*Recorded 2026-09-28.*

> How N64 giants reach T3 - the ROM's big functions are un-optimised IDO (over -Olimit) with register vars; frame must match slot-for-slot; oracle fixes (OS record/replay, stack ra mask, ROM literals)

2026-09-28, BrRaceTick 0x8020082C (22,448 B) certified T3 (first N64 T3). Method that worked:

- **-Olimit**: IDO silently leaves a function unoptimised when it exceeds -Olimit (n64build passes -w, so the uopt warning is hidden; compile without -w to see it). The ROM's giants are compiled THAT way (no strength reduction, i*0x2090 recomputed per access, no CSE across statements) - so use default -O2, NOT a raised -Olimit (raised = 4193 vs 5612 insns; default + `register` = 5546). Tell: loops with `slti at,zero,6` guards on constants.
- In that mode locals live in the frame; `register` locals get s0,s1,... in declaration order (floats f20..); temps (ternaries, switch values) take the next s-reg (s7). EVERY declared local, register ones included, takes a frame slot top-down in declaration order; block-scope register vars also take slots (no reuse). "Unused" ROM slots = register vars / declared-unused ints.
- The frame must match SLOT FOR SLOT for T3: callees write stack addresses into globals (zlib state via BrMusicLoadTrack) → A5 needs equal frame size; the frame is live across retraces → A7 digest reads it → locals must sit at the ROM offsets and hold the same values (e.g. a pointer stored only inside a branch; `y += 20` after the last print).
- Block order = source order still applies (if/else swaps, switch case order from the jump table).
- Tools added: n64t3 RecBox record/replay of OS calls (A5 of IO functions), `--cap N`, N64T3_VERBOSE; n64box digest masks code addresses on live stacks; n64link maps a .rodata prefix byte-exact in ROM to the ROM address (string pointers stored/passed); `--image` places only @t3-tagged bodies. New box scripts race_pause.txt (START pauses/selects, D-pad rows/volumes, B resumes) and arcade_timeup.txt.
- Pad record bits (pressed): A 0x10000, B 0x40020, Z 0x208000, START 0x4000, L 0x1000, R 0x102000, D-pad U8 D2 L4 R1, C: CU 0x100 CD 0x400 CL 0x800 CR 0x200. Box models ONE controller (2P races unreachable).
- Scratch debug scripts (grab job / replay sandboxes / watch writes / call-sequence diff / a7diff / a7bisect) were in the session scratch; re-create as needed.

Related: [n64-ido-levers-2026-09-27](n64-ido-levers-2026-09-27.md), [a5-a7-coverage-gap](../../../brally/decomp/oracle/a5-a7-coverage-gap.md), [t3-certified-standard](../../../brally/decomp/rules/t3-certified-standard.md).
