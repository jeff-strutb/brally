# Big five t1 lanes

*Recorded 2026-09-04.*

> Handoff for the five-lane T1 intake of the largest untouched BRGlide functions (0x10011FA0, 0x100706D0, 0x1000CBA0, 0x1005D770, 0x100131E0): what exists on disk and the lane state.

Started 2026-09-04. The project lead asked for five of the LARGEST
functions not being worked, one lane each, byte-exact, filed per rule 6.

**Targets (largest T1 with a C-reachable prologue; EH-prologue rows
0x100498A0/0x100038F0/0x1004CBA0/0x1002F790 screened out; giants and
0x10019A70 excluded per project rule 11):**

| VA | B | module | intended TU | note |
|---|---|---|---|---|
| 0x10011FA0 | 4500 | drawing | src/core/drawing/br_framedrive.c | per-frame scene/frame driver; calls 0x1000EAF0 twice, 0x1000A110 per car |
| 0x100706D0 | 4145 | controls | src/core/controls/br_inputpoll.c | per-frame input poll; d3d twin 0x100773F0 documented in slice3_45.c/.h |
| 0x1000CBA0 | 3971 | drawing | src/core/drawing/br_objdl.c | per-object DL emitter called from br_scenedl.c line ~1279 |
| 0x1005D770 | 3858 | driving | src/core/driving/br_ctlai.c | BrCtlAiBody (already declared in br_ctlstep.c); ~700-line transcription in include/br_ai.h |
| 0x100131E0 | 3217 | drawing | src/core/drawing/br_viewsplit.c | unreferenced anywhere; view hand-off step, stride 0x2E0F0 records |

Run 1 stopped on a run limit after reading only (no files). Run 2 stopped on
the limit too but left real work (see "State entering run 3" below).

**How to apply:** at session start check `git status --short src/` and
`git log` for the five VAs / TU names above; whatever exists on disk is the
lane's state (files survive a stopped run, nothing else does). T1 rows cannot be held in `lane_claims.csv` (claim_lane.py drops
any VA not in report.csv), so coordination is by the TU filename only.

**Why:** the reading of these listings is expensive and should not be repeated.

## 2026-09-05 10:00-11:00 (by hand, one function, project lead's instruction: NO workers)

Run 4 stopped at a run limit. Then: ONE function only, by hand.
Picked **0x100706D0 BrInputPoll** (closest: 1185/1185 insns, -5 B, regnorm 3+3).
Landed b0c695a: **4145/4145 B, 1185/1185, regnorm 0+0, 2 bytes left**, from
two source facts (both in docs/VC5-IDIOMS.md, de73f99): block-scope the
address-taken DIMOUSESTATE so the fidiv temp packs into its slot (frame
0x118->0x110); `int32_t *pPrevAx = &g_brInMouse[prev].ax` for the three
copy-pasted reads. The 2-byte residue (keyboard arm: orig loads the vtable
BEFORE the index store) is C++-front-end behaviour: as a .cpp the site is
byte-exact and the mouse site regresses; ~25 C probes dead, all listed in the
TU header. **Open lead: a C1XX spelling for the mouse accumulate would move
the file to the C++ lane and finish it.** Do not re-run the listed probes.
Other four: BrFrameDraw 4503/4500 regnorm 4+3; BrCtlAiBody 3854/3858 20+18;
BrSnapInterpDraw 3520/3217 154+189 (a1e4192 was a regression probe, reverted);
BrObjDlBuild 3980/3971 124+69. All committed; nothing uncommitted of ours.

## State entering run 3 (2026-09-05 00:02 MDT)

Run 2 also stopped on the limit but this
time it left real work. Four of the five TUs now EXIST; the names below are
the real ones, not the placeholders above:

| VA | TU | name | state |
|---|---|---|---|
| 0x10011FA0 | src/core/drawing/br_framedrive.c | BrFrameDraw | committed (b568422, 71463a7), 4512/4500 B |
| 0x100706D0 | src/core/controls/br_inputpoll.c | BrInputPoll | UNCOMMITTED on disk, 4160/4145 B, /O2 /Op |
| 0x1005D770 | src/core/driving/br_ctlai.c | BrCtlAiBody | committed (abfe2c6), 3856/3858 B, regnorm 25+33 |
| 0x100131E0 | src/core/drawing/br_snapinterp.c | BrSnapInterpDraw | UNCOMMITTED, scored /Od 11146 B - the /Od row means it is NOT yet shaped like the original; treat the opt column as a defect signal |
| 0x1000CBA0 | (none) | - | never written; start fresh |

 Two lanes proved a real idiom already: 71463a7 - **seven pause-menu string
picks are if/else pairs with the CALL IN BOTH ARMS, not ternaries; VC5
tail-merges the call and the setne form cost 26 REGNORM rows.** Merge that
into docs/VC5-IDIOMS.md when the run settles.
