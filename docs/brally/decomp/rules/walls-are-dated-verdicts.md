# Walls are dated verdicts

*Recorded 2026-09-21.*

> RULE 2026-09-13 - a "wall" is a verdict about the LEVERS TRIED AT THAT DATE, not a permanent do-not-touch; re-triage parked walls whenever a new lever class lands

**RULE (2026-09-13):** stop reading old wall notes as "we said there's
a wall here, so don't bother." A wall entry means "dead under every lever
known on its date" - nothing more. When a NEW lever class is proven, the
parked walls it could touch get RE-TRIAGED against it.

**Why:** the x87 operand-role class was parked across many sessions as
"scheduling wall, no spelling moves it" - all true - and then
[x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md) proved a lever NONE of those verdicts had
tested (TU state from preceding function definitions; byte-exact
demonstration on a 116 B shape). Every dated wall verdict is silent about
levers discovered after it.

**How to apply:**
- Old dead-lists stay dead for what they actually tested - never re-probe a
  listed SPELLING; that is no-token-thrashing, unchanged.
- But "parked/wall" is NOT a reason to skip a row when the session brings a
  lever the parking note predates. Check the note's date against the idiom
  dictionary's tail; anything newer than the note is untested on that row.
- Current re-triage queue for the TU-state lever (micro-at-states
  diagnostic, ~15 compiles at build/external/lab/lab.py, then TU
  composition/position in-tree): the five largest ([five-largest-2026-09-13b](../log/five-largest-2026-09-13b.md):
  0x1000E320, 0x10032E40, 0x10001CF0, 0x10068F80, 0x10068900), 0x1006D530,
  0x10029D70, 0x1002A050, 0x100271F0's and-before-byte-move, the fresh-xor
  rows (0x10002580), and the dirty-widen carriers (0x1006CE50, 0x1006CE80,
  0x10062B80) - all of whose parking notes predate the mechanism.
- Known limits, so re-triage is honest: rolled-loop ANCHOR residue
  (0x1006DD20 class) measured ordinal-INSENSITIVE - the TU-state lever does
  not apply there; and co-filing candidates need SAME best flag variant AND
  VA adjacency (test #1 on obb+carcol was null for exactly that reason).
- A diagnostic that zeroes a row at ANY pad state proves the SOURCE is
  correct - the row then needs placement/TU work, not probes; record that
  in the file header when found.

**2026-09-20 (project lead repeated the rule, sharper): "stop assuming old walls are
valid - it holds you back from delivering."** The failure mode called out:
reading a dossier ("do not probe") and STOPPING, instead of testing it against
the session's levers, and - worse - leaving behaviourally-EQUIVALENT rows
parked as T2 when they were one gate-run from a deliverable. Concrete:
screened the 21 reg 1-12 T2 rows; instead of grinding walls, ran
`tools/t3.py --qualify` and found 4 already PASS 0+A+B with A5 EQUIVALENT
(BrTex3dTexel 0x100271F0, BrSurfSetColourKey 0x100014A0, BrRaceCarPickIndex
0x1005C490, BrGfxDrawTexRect 0x10013FD0) - certified + committed on the spot,
T3 141->146. **Lesson: the deliverable next to a "wall" is often the T3 cert,
not a T4 respell. Run --qualify on any ledgered EQUIVALENT row before treating
it as blocked.** Several more (BrCollRespReset 0x10063DD0, BrCheatCodeScan
0x10040A90, BrDlsTileRectE4 0x10021570) pass 0+A+EQUIVALENT and only need fresh
crank Gate-B ledger passes at current numbers. BrMat3Mul 0x1006DD20's
ordinal-insensitive anchor (line above) DID hold on re-test - deference there
was correct; the rule is TEST, not blindly re-open.
