# T3 frontier map

*Recorded 2026-09-10.*

> SUPERSEDED - the byte-Gate-A frontier snapshot is obsolete under A5; functions it named as blocked/unreachable (0x10067710, 0x1005D770 BrCtlAiBody) are now CERTIFIED T3. Keep the four compiler-decision-class taxonomy and the float-compare levers as durable analysis.

Re-ran `t3.py --qualify` over all 51 uncertified diff rows >= 500 B and
ranked by **A3 unpaired rows**, not size. Reusable script: iterate
report.csv `status == diff`, drop `t3.py --vas`, parse the `A1..A5` lines
plus `GATE B`. ~12 min.

** THE HEADLINE: the reachable frontier is blocked by FOUR compiler
decisions, not by fifty separate source bugs.** Every candidate within
reach of Gate A fails on one of these, so crack a class and several
functions move at once:

1. **thiscall shim `xor edx,edx`** - the whole slice2_12 bitstream family
   (0x10006BA0 BrCarStateEncodeDelta 925 B, 0x10006510, 0x10007230).
   0x10006BA0's ENTIRE residue is 18 of them (36 B = the whole size gap).
    MEASURED AND DEAD 2026-09-10: making `value` a 4-byte struct DOES
   kill all 18 (insns 302 = 302 exactly) but `nBits` then claims edx  - 
   `push I` x17 becomes `mov edx, I` x17, +52 B. Making BOTH args structs
   costs +18 insns for the temps (+70 B). __fastcall gives ecx alone only
   when NO other parameter is register-eligible, and any ineligible type
   (struct, float) just passes the register to the next parameter. **The
   family is unreachable in C - route it to the C++ lane.**
2. **x87 dup-vs-reload on a shared multiplicand** - orig `fld st(0); fmul
   mem`, ours `fld mem; fmul st(k)`. 0x10067710 BrCrRespWalk (4 unpaired,
   A2 and Gate B both PASS - it is ONE pair from certified), 0x100311C0
   BrTrackLoad, 0x1005D770.  On BrCrRespWalk all SIX permutations of the
   `dp.x/y/z` store order and both product hands are inert: the `pP->nx`
   CSE out of the dot product above survives every one. This is the fork
   whose classifier fold was reverted in 7f826ba - fix the source or park.
3. **literal pooling into a register** - 0x10034010 BrSpanBuildHull: 0x3F
   appears four times, VC5 pools it into edi and pays a `push ebx`. Same
   class as the zero-register compares (`cmp [g],ebx` vs our `mov r,[g];
   test r,r`) in 0x1005D770 and 0x1005F6C0.
4. **addressing-form choice for one field read** - 0x10020D70
   BrDlCmdTri2NoZ: A1/A2/A4/gate 0 all PASS, TWO unpaired rows, one
   region, +3 B.  Three more spellings dead 2026-09-10 (splitting the
   macro per site, hoisting the pointer above the first corner, dropping
   the `w_` temp for a double `pv_->oow` read): every `pv_->oow` costs
   +13 B / +6 insns and moves FIRSTDIV to 0 - it adds a stack slot.

**WHAT MOVED: 0x1005D770 BrCtlAiBody (3,858 B) - gates 0, A1, A2, A4, A5
now ALL PASS** (was A2 35/27 and A3 15). Two new levers, both on the tail
of docs/brally/VC5-IDIOMS.md:

- **Name a computed float bound in a (dead) local.** `if (v < a - -1.0f)`
  is canonicalised to `fld <bound>; fcomp v`; with the bound in a slot it
  becomes the original's `fld <bound>; fld v; fcompp` + `test ah,1`.
  Five rows. The lever is the SLOT, not the spelling - operand-hand swaps
  and the `!(v >= ...)` form are inert.
- **Put the CONSTANT first in a float ceiling test.** `if (k > 0.4f)`
  homes k and writes the ceiling back as `mov dword ptr [esp+S],
  0x3ecccccd`; `if (0.4f < k)` leaves the constant in st. Rows 28 -> 26
  at identical size - that is what took A2 under its limit.

BrCtlAiBody is parked at **A3 = 10 unpaired** over four sites (the k
spill, the bias-global zero compare, the two hold counters). ~26 probes
dead this session, all listed in the file's own comments - read them
before reopening. A dedicated `float kf` local does NOT stop the spill;
VC5 homes any named float there.

**Do not touch 0x100250D0 BrTex3dExpand** - a peer session committed three
times to it on 2026-09-10 (rows 70 -> 62). Its 29 unpaired rows are one
coherent byte-width class (orig works in 8-bit regs, we widen to 32).

Six rows pass gates 0/A1/A2/A3/A5 with a **0+0 multiset** and fail ONLY
A4's identical-order demand: 0x10060F40, 0x10058540, 0x10058900,
0x10029EC0, 0x10013FD0, 0x10039D20. That is the gate question the project lead
already answered - see [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md). Parked.

 The tree is SHARED and a peer was writing `src/brally/core/slice3_44.c` during
this session; the tree-wide match count moved 984 -> 986 with none of my
edits landing a match. Commit by pathspec, re-derive counts, never quote
one from earlier in the session ([counting-reconciliation](../traps/counting-reconciliation.md),
[parallel-session-clobber](../traps/parallel-session-clobber.md)).

Related: [gate-a-distance-survey-2026-09-10](gate-a-distance-survey-2026-09-10.md) (the earlier, size-ranked
survey), [calling-convention-screen](../triage/calling-convention-screen.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md),
[thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md), [cxx-thiscall-wall](../cpp-lane/cxx-thiscall-wall.md).
