# Photo family literal lever

*Recorded 2026-10-05.*

> Five menu builders (0x100498A0, 0x1004AEE0, 0x1004CBA0, 0x1004BE00, 0x1004DA00; 18.4 KB) T3 -> T4 in one session: DAT_1007xxxx 'globals' were constant-pool float literals. VC5 scheduler priority rule measured with a hooked C2. Literal vs global changes alias edges.

DONE 2026-10-05 (session): commits 59a6ac06, 2a116ebf, 82a882dd (C-twin
retire), 06002f42. The "photo1 Pentium-pairing schedule wall" (726 dead probes,
certified T3 2026-09-12) was a wrong SYMBOL KIND, not a schedule.

**Lever:** an `fsub/fadd [addr]` whose address sits among other float constants in
.rdata (here 0x10077640..68: 0.5, 0.001, -19, -38, -57, -76, -95, -114, -133, -33, 19)
is a compiler LITERAL. VC5 emits `fy + 33.0f` as `fsub [-33.0]`. Reading it as an
`extern float DAT_...` gives identical bytes but a different dependence graph:
a global read gets may-alias edges from every earlier store through a pointer and
to every later one; a literal load gets none. Check .rdata neighbours before
spelling any float operand as a global ([tu-constant-pool-oracle-2026-09-15](../corpus/tu-constant-pool-oracle-2026-09-15.md)).

**VC5 list scheduler (measured, C2.EXE):** driver FUN_00417450 per region:
DAG build FUN_0043acf4, priority FUN_0043be83, list-schedule FUN_0043c94d
(pick FUN_0043caf6, ready insert FUN_0043ca68). priority = height<<13
+ 0x10000 if the tuple reads memory (bit 0x20) or is an x87 store (bit 0x40 on
FP tuple). Ready list sorted by priority desc, TIES BY IR ORDER (node+0x36).
height = max(edge latency + succ height) + 1. schedmd.c FUN_0045c651 is the
x87 stack pass, not the scheduler.

**Trace kit:** build/brally/win32/match/probe_bd4988/c2cap/hs/ (mk.py patches C2 into C2P.EXE
with hooks on 0x43c94d region start, 0x43ca68 ready insert, 0x43d0b8 issue,
call 0x41751a -> DAG dump of nodes + edges/latencies; run.sh SRC TAG compiles
via c2wrap /B2 with C2PATCH=1; dag.py LOG REGION MINSEQ prints a region's DAG).
The colour.c hook build is kept as C2P_colour.EXE. Any schedule "wall": trace the
region, compare priorities and IR order of the two contenders, then ask what
source makes the original's order fall out.

**C++ match bookkeeping:** after a .cpp goes T4, cpp_sweep the file, delete the
function's reloc_overrides rows, image gate. If tiers.py does not count it, a
stale C twin row in report.csv shadows it: replace the C body with a bare
prototype and switch the port spec `@drop/@after fn:` to `proto:` (precedent
3e5aeebb), re-sweep the TU, portgen, gate, fileaudit.
