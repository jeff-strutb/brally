# Framedraw 0x10011fa0 state

*Recorded 2026-09-09.*

> 0x10011FA0 BrFrameDraw (4,500 B) state 2026-09-09 -- 4501/4500 B, 1+1 rows after the hMir-first lever; two residues left with dead lists in the file header; what was asked and answered about >2000 B functions.

**2026-09-09, the project lead asked "can you byte-exact any functions > 2000 bytes today?"**
Answer given: no >2000 B row closed by me; the two 2-diff rows were
0x1005FF00 BrRaceGateStep (closed byte-exact the same day by ANOTHER session,
commit 243bb2c: name both add operands, later-declared symbol is the
two-address destination) and 0x100706D0 BrInputPoll (front-end wall, T3
Gate 0+A+B PASS, still untagged). Everything else >2000 B is hundreds to
thousands of bytes off.

**What moved: 0x10011FA0 BrFrameDraw** (src/brally/core/drawing/br_framedrive.c)
4503 B / 1377 insns / 4+3 rows -> **4501 / 1376 / 1+1** (commits 18819d1,
38096bd). Lever: `hMir = wMir >> 2` FIRST in the mirror block. Mechanism
(new idiom, tail of docs/brally/VC5-IDIOMS.md): register OCCUPANCY decides a
cross-block global-load CSE; the crank's levers never reorder statements
inside a nested block.

**Residue (dead lists in the file header, ~35 probes):**
1. loop top +0x131: original forms `pV` then reads the car index through it
   after all six constant pushes; we fold the first read into the scaled
   index form. Solved consumers of the same records (0x10014C00, 0x100140B0,
   0x10017F80) fold the FIRST evaluated read -- so in the original the index
   read is not pV's first use. Every pointer/read spelling tried is inert;
   `pV` before the trace call reads through esi but hoists the lea (4502, 3+2).
2. +0xaf `add edx,eax` vs ours `add eax,edx` (aViews->x + aViews->w): the
   0x1005FF00 named-operands lever is INERT here in every declaration
   order; w has a second use (`0x130 - w`) before the add.

**Why:** the project lead wants the count moved on the big rows; this is the
closest structural row left among >2000 B and it is one fold from exact.
**How to apply:** do not re-run the header's dead lists; next lead for (1)
is a source shape where the car-index read is not the pointer's first
evaluated use (a hidden earlier use), or the N64 twin's statement order.
Related: [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md), [parked-is-not-walled](../triage/parked-is-not-walled.md),
[big-five-t1-lanes-2026-09-04](../log/big-five-t1-lanes-2026-09-04.md), [resume-state](../log/resume-state.md).

**CLOSED 2026-09-09 (later the same day): @t3 CERTIFIED, commit 28ca80f.**
4500/4500 B, 0+0 rows, 2 regions. The other session's X7 lever (pV moved
ABOVE the trace call; the clobber forces the read through the pointer)
closed site 1 and logged passes 4-5; I added the @t3 tag once its lane
claim was released. Do not reopen unless the project lead names it (rule 12).

**Same day, 0x1005D770 BrCtlAiBody (commit a147060): raw 45+43 -> 36+33.**
Two R4 sites closed: the vTarget copy via a const field pointer (the
BrInputPoll fold idiom -- and the dossier's corpus MISS was an artefact of
querying a 12-insn run; the 2-insn fold IS proven), and the ladder's second
induction as a named pointer local (br_track.c rule). NOT certifiable:
R1-R3 are unpaired allocation classes, A3 blocks Gate A regardless of row
count. Ladder TAIL still open (back-edge rotation, old-level inc/store/dec).
