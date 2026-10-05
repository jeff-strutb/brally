# Racestep hole

*Recorded 2026-09-10.*

> 2026-09-10 the race step 0x10019A70 (11,223 B, the largest function in the tree): its untranscribed hole went 5,094 -> 3,520 B, the address 54.6% -> 68.6%, and the 131-callee gate is SPENT -- all 64 callees of the hole already have symbols.

The project lead named the VA on 2026-09-10. project rule 11's "gated on 131 callee
signatures" is now **spent**: all 64 distinct callees of the untranscribed
block already have symbols in this tree -- 115 of its 116 call sites land on a
report.csv row and the last (0x100325B0) is the C++ lane's WM_DESTROY
teardown. What blocks the address now is transcription volume, not unknown
callees.

 **tiers.py reports this address as T1 "not started". That is wrong and has
been wrong for a long time.** It reads the absence of an `@implements` line,
and this address cannot carry one until 100% of it is transcribed (the note in
br_racestep.c explains why a partial claim was deliberately REMOVED). 54.6% of
it was already transcribed across five functions before this session. Never
plan a "largest untouched function" session off tiers.py alone -- grep the
tree for the VA first.

**Transcribed 2026-09-10 (three blocks, 1,574 B, all committed, port green):**

    BrRaceStepEffects  0x1001B261..0x1001B364   frame effects + fly-past
                                                arming scan          260 B
    BrRaceStepSpecials 0x1001B365..0x1001B402   the special-object loop and
                       0x1001B870..0x1001B886   its 3 rotation arms   181 B
    BrRaceStepFlyPast  0x1001B403..0x1001B86F   the spline walk and the
                                                basis it builds     1,133 B

Hole now 0x1001B887..0x1001C646 (3,520 B); the address is 68.6% transcribed.

**Reading rules that make this function legible -- learn them before opening
the listing:**

-  **ebp is the function's PINNED ZERO** for the whole of 0x10019A70
  (`xor ebp,ebp` at 0x10019A8E, re-zeroed at 0x1001B30C / 0x1001B363 / 0x1001B63C
  after a block borrows it). `cmp X, ebp` reads `X == 0`; `push ebp` reads
  `push 0`. Reading either as a live value invents defects.
- The frame CANNOT be won from a fragment: `sub esp,0x34` is decided by the
  WHOLE function's locals, so rule 11's "win the frame first" only becomes
  measurable once the last block is in. Do not chase it early.
- Jump tables sit PAST the function's extracted bytes (0x1001C678 is beyond
  the 11,223-byte end at 0x1001C647) -- read them from orig/BRGlide.dll
  through the section table, not from build/match/orig/<VA>.bin.
- `lea` chains that look like table indices are usually the 0x2B68 car-record
  stride or the 0x54 object-record stride. x1389 then scale-8 is 0x2B68.

 **TRAP I HIT: a `*/` at the end of a note inserted INTO the file's dossier
comment closes the block early and breaks the whole TU** (the rest of the
dossier then parses as code; the first non-ASCII character is what MSVC
reports). This is the trap already recorded in [resume-state](../log/resume-state.md) and I walked
straight into it -- always re-sweep after editing a dossier comment.

 **Next block's derivation is already in the file header**, including the
hazard: 0x10008D60 (BrPodNop) is called with FIVE arguments at 0x1001B27A /
0x1001B298 and with ONE at 0x1001B955, so the matching arm needs two
prototypes; picking one arity silently changes the caller's stack adjustment.

Related: [gate-a-distance-survey-2026-09-10](../log/gate-a-distance-survey-2026-09-10.md), [braceStep-wall](braceStep-wall.md),
[callee-saved-zero-web-class](../levers/callee-saved-zero-web-class.md).
