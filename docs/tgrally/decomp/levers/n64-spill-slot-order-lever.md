# N64 spill slot order lever

*Recorded 2026-10-07.*

> IDO uopt spill-temp slots are first-fit in candidate BIT order (program order of first sight), forbidden by earlier candidates sharing a block; a pointer local set early moves an expression's bit forward. Solved BrSkidStep 0x8023B418 T4 15938f9f (2026-10-06, d7b110)

uopt `spilltemps` (instr7/uopt.c f_spilltemps) walks spill candidates in bit-position order (newbit `pos`, = order uopt first meets the expression in the code), and gives each the first slot (frame top - 124 - 4n for a 0xF0 frame) not held by an EARLIER candidate present in any of the same blocks (block bitset at node+0x15c, set by codemotion). Later candidates never forbid.

Lever: to move an expression's slot, change where it is first met. BrSkidStep: f's spill was 0x64 vs ROM 0x70 because car+4k (slot 0x70) shares f's blocks. `side = (BrVec3 *)car->mtx0[1];` before the loop makes car+0x10 the second candidate; it takes 0x70 and f (not co-present with it) reuses it. The pointer variable itself copy-propagates away (no code).

Diagnostic tooling: private copy build/tgrally/ext/instrD7 (instr7 + `CDX_SPILLCO=1` prints `spillin cand/blk` and `spillco cand/blk/prior`); replay script build/tgrally/n64/search/8023B418/spillsim_d7.py reproduces the slots and tests hypotheses (remove/move a candidate) before writing source. Also: n64alloc Grader with TGR_TRACE_CC=instr7 cc gives spillcand/spilltemp lines.

Related: [n64-trackdrawsetup-2026-10-06](../functions/n64-trackdrawsetup-2026-10-06.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).

**BrFrameTintSetup 0x80218D5C T4 7d5cb059 (same session):**
- as1 shares `lui $at` between stores only for a symbol DEFINED in the TU (even tentative `unsigned int D[4];`), two words at a time (D/D+4, D+8/D+12); extern = a lui per store. ROM pairs of shared $at = define the array in the file.
- `lui rA; lbu rB, lo(rA)` with rA a uopt reg ≠ rB = ugen `lbu rA; move rB, rA` merged by as1: two webs, the second an int CONVERSION of the byte (uchar promoted in `x << 24` without an `(unsigned int)` cast). `(unsigned int)uchar` folds into the load; plain promotion keeps a CVT web. The extra webs drop priority (fixed colour order vs constants) and frame.
- Frame model that held here: frame = align8(0x18 + ugen temps + ugen area + uopt locals/spill); an alignment pad sits at 0x18, shifting ugen spill slots up by 4.

**BrCarTrackLocate 0x8021EB50 T4 e551d523 (same session, 231 -> 0):**
- as1 load order follows source LINES: three assignments on one line let as1 issue them in reverse (z, y, x) while the colours keep x first.
- cfe: `call() + (expr)` where expr has an ARRAY INDEX -> call hoisted into an 8-byte frame temp and ADD(expr, tmp) (call second); with pointer derefs only, no temp and the call stays first. Probe it (scratch probe2 pattern).
- A variable is ONE live range in IDO uopt: reusing a variable for an unrelated later value drags it onto the earlier colour; split values need their own variable (count frame words to see which locals exist).
- Same literal spelled two ways (1000.f / 1000.0f) = two constants, neither hoisted.
- `return 0` placed last via goto = ROM's beq-to-epilogue with v0=0 in the delay slot.

**BrWeatherStep 0x8023A1C8 T4 574432d6 (same session, 317 -> 0, weather.c):**
- A ROM "car" pointer living in a LOW frame slot (below the locals) = a uopt strength-reduced temp from `D_8031B760[n]`, not a source variable (source vars get homes in declaration order at the top).
- Writing through a pointer (`w[k]`) makes later stores to an address-taken local (v) kill forwarding of w[k]; indexing the global (`D[n][k]`) keeps the stored values in registers for the following adds.
- **Pointer stores alias globals; indexed-global stores don't.** With `D[n].p[i][k]` uopt PRE hoisted the loop-bottom reload of the count global into the if-arms (an extra spill candidate = +1 frame slot). A row pointer `pp = D[n].p` makes the last store kill it, as in the ROM.
- A copy-propagated pointer's address is built in uopt pass 2, so it lands AFTER the LICM'd invariants in the preheader; writing the invariants as statements just before the loop (sdx = dx * K) restores the ROM order (address first).
- ROM registers f0/f2/f14 at several load sites of a local array = one float variable per element reassigned at each site (x = cur[0] ...).
- Spill-slot count is part of the frame: diagnose with instrD7 `CDX_SPILLCO` (spillcand forbid=XXXX shows the co-present set); script build/tgrally/n64/search/8023A1C8/cdx.py (takes the _full.c).

**OPEN BrCrImpulseSolve 0x8025BBB8 (collresp.c) 302 -> 208 (blind 27), draft build/tgrally/n64/search/8025BBB8/d7b110_best.c:** levers that worked: loop counters declared last (no frame words at the top), a separate `r = D + rest` keeps the param in its home, cfe evaluates the RIGHT product of `A*B - C*D` reversed (write `b->cur[12] * nb[1]` to get nb1 first), `vc[k] + b->cur[k+3]` for cur-first adds. uopt promotes never-address-taken local arrays (tt[]) to registers and writes them back before the next call; their colour/write-back order = first sight (a surviving early tt[2] reference reorders them). Tools: scratch rb.py (register-blind diff), uc/dump.py (uopt x.O ucode per line: Rmt = promoted register, Mmt = memory).

**OPEN BrHudArrowDraw 0x80233880 (racehud.c) 342 -> 88 (blind 21), draft build/tgrally/n64/search/80233880/d7b110_best.c + car_h_dent_fields.patch.** It is the car dent pass (model->dl[k][j] G_VTX walk). Levers: defaults set inside the out-of-range arm (uopt hoists them; set before the if, uopt deletes the equal-value arm stores); reassign the short param in place (`amount <<= 2`) to keep its register; a value used as an int after a 16-bit wrap spelled `(e) << 16 >> 16` (uopt splits sll/sra and CSEs the sll); `else if (d < -lim) X; else Y;` gives the ROM's trailing `b` to the next address; branch TO a later block for one case + inline default = `switch`; `w = (w >> 10) & 0x3f; while (w--)` (variable reuse) gives the count copies. Allocation left (dl/const 8 swap, word/counter registers, compare operand order). Peer lever (5c4f1d): the uopt block after a call closes once it would pass 24 loads of locals/params.
