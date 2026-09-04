/* br_scenedl.c -- 0x1000EAF0, the frame's scene display-list builder.
 *
 * 9,354 bytes -- the second-largest function in BRGlide.dll after the race
 * step.  Called by the frame driver 0x10011FA0 (see include/br_drawcar.h);
 * emits the frame-global DL preamble (fog, combiner, othermode, texture
 * windows), walks the sorted object table at 0x106EED38 (0x54-byte records:
 * a 4x4 matrix, a DL pointer and flag words), transforms and range-checks
 * each object's matrix, then on the mirror/second pass batches the wheel
 * trail quads out of the per-wheel 500-entry rings at 0x10273690.
 *
 * Matching build only -- transcribed from build/ghidra_decomp/0x1000EAF0.c
 * against the disassembly of build/match/orig/0x1000EAF0.bin.
 *
 * ‼ TWENTIETH PASS (2026-09-03) -- WALL 1's TERM-3 FLIP IS CLOSED, and the
 * nineteenth pass's "done as a source problem" verdict is RETRACTED for it.
 * The lever was a POINTER-INDEX ASYMMETRY nobody had looked at, because
 * every pass had been probing the coefficient side.  Terms 1, 2 and 4 read
 * their object factor as `ptr[0]` off a dedicated pointer local; term 3
 * alone read `pPos[2]` -- the SAME pointer as term 1, at a NON-ZERO index.
 * VC5 ranks a `ptr[0]` operand and a `ptr[k!=0]` operand differently when it
 * decides which of a multiply's two memory operands gets the `fld` and which
 * gets the memory `fmul`, so term 3 alone came out object-first
 * (`fld [esi+0x38]; fmul [coef]`) where the original is coefficient-first.
 * A FOURTH pointer local, `float *pTz = pObj + 0xe`, with term 3 spelled
 * `pTz[0]`, makes all four terms symmetric and all four come out
 * coefficient-first, exactly as the original.
 *   Measured: register-blind multiset 48+54 -> 41+45 (SIXTEEN rows, the
 *   whole `4 fld [I]` / `4 fmul [R+I]` / `4 fmul [I]` / `3 fld [R+I]` block
 *   the nineteenth pass had catalogued as walls 1/2), instructions 6 short
 *   -> 4 short, bytes -15 -> -14, fn.py RAW 117+123 -> 109+113.
 *   ‼ MASKED REGIONS GO 19 -> 21 AND THAT IS NOT A REGRESSION -- read the
 *   eighth pass's warning above.  The two new regions are 0xe58 (the fourth
 *   row statement, now a small order-only region like its three siblings --
 *   all four sit at a flat delta +2 instead of the old +7/-3/-3 swings) and
 *   0x189c, the `mov edi,1` sink that re-opens on ANY allocation change in
 *   this function and has done so twice before.  Every other metric moves
 *   the right way.
 *   WHAT IS LEFT of the row block is COMPLETION ORDER only: orig finishes
 *   T2 before T1 (`fld C1; fld C2; fmul [esi+0x3c]` then `fld C3; fxch st(2);
 *   fmul [esi+0x30]`) and defers `fld C4` until after T3's fmul; we finish
 *   T1 first and hoist C4.  Same instruction multiset, four regions of two
 *   bytes each.
 * TWENTIETH-PASS PROBES, DEAD -- do not re-run.  Both were re-measured
 * under the staleness rule because the pTz fix moved the allocation, so the
 * seventeenth and eighteenth passes' verdicts on them were fair game:
 *   - TERM ORDER, three permutations (2134, 2314, 2341) chosen because the
 *     original COMPLETES the terms in order 2,1,3,4.  All three are
 *     BYTE-IDENTICAL to 1234.  This re-confirms the flat-float-sum
 *     canonicalisation for the fourth time in this file: only GROUPING
 *     reaches the scheduler, never summand order.
 *   - GROUPING, six forms.  `(T1+T2)+(T3+T4)` = 40+45 / -16 B;
 *     `T1+((T2+T3)+T4)` = 41+45 / -14 B (identical to no-grouping);
 *     `((T1+T2)+T3)+(T4)` and `((T1+T2)+(T3))+T4` = 42+45 / -9 B;
 *     `(((T1)+T2)+T3)+T4` = 40+47 / -20 B; and a REDUNDANT OUTER PAIR
 *     `(((T1+T2)+T3)+T4)` = 43+45 / -7 B / -2 insns, with a second and third
 *     nested pair inert beyond the first.
 *     ‼ THE OUTER PAIR IS THE TRAP AND IT IS WHY IT IS NOT IN THE TREE.  It
 *     is the best BYTE and INSTRUCTION result this function has ever read
 *     (-7 B, only two instructions short) and it is WORSE: it re-opens the
 *     scale block at 0xf2a as a -18-byte region with +20 straight back at
 *     0x11d3, i.e. it breaks the 8|4 batching the sixteenth pass won, and
 *     the recovered "instructions" are the scale block's extra loads.  22
 *     regions, REGNORM 43+45.  The eighteenth pass's warning that parens
 *     break the batching HOLDS at the new allocation; do not take a paren
 *     form in this file on a byte or instruction count alone.
 *
 * ‼ NINETEENTH PASS (2026-09-03) -- MEASURED STATE, and a full residue
 * accounting that says this function is DONE as a source problem.
 *   Measured: 2,322 vs 2,328 instructions (SIX short), 9,339 vs 9,354
 *   bytes (-15), 19 regions slot-masked / 28 raw, register-blind multiset
 *   39 missing / 33 extra, fn.py REGNORM 48+54.
 *   ‼ EVERY ONE of those 39+33 multiset rows maps to a wall already
 *   catalogued below -- this was checked row by row, not assumed:
 *     wall 4 (ring*4 CSE)  4 `mov R,[R+A]` / 3 `mov [R+A],R` / 1 `lea
 *                          R,[R*K]` / 1 `cmp R,[R*K+A]`, plus the 3
 *                          pDst-spill rows that the eleventh pass proved
 *                          are downstream of it  = 12 rows, and it is the
 *                          largest single wall left (~13 B, region 19).
 *     walls 1/2 (x87)      4 `fld [A]` / 4 `fmul [R+0x38]` (the term-3
 *                          operand-ranking flip) + the fxch/faddp spread.
 *     wall 3               `lea R,[R+R+0x70]`, `mov R,[R+0x50]`.
 *     wall 6 (slot packer) regions 1, 2, 4, 5, 6, 7 are a PURE two-way
 *                          swap of slots 0x20/0x24 (bSolo+idx vs pDst) --
 *                          zero byte delta each, wrong displacement byte.
 *     walls 9a/9b          the `dec`/`lea-1` and `mov R,6` rows, both
 *                          already recorded dead.
 *   There is NO missing-code row left: the six missing instructions are
 *   3x wall 4's spill + walls 1/3's schedule.  So the remaining work is
 *   register allocation and x87 scheduling, which is T3a by the
 *   playbook's own decision tree.  Anyone opening this file again should
 *   read that sentence first and pick a different function unless they
 *   have a NEW lever for wall 4 or wall 6 specifically.
 * NINETEENTH-PASS PROBE, DEAD -- do not re-run.  The row block's THREE
 * OBJECT POINTER LOCALS (`pTw`/`pTy`/`pPos`, used only by the four row
 * statements) deleted and every term spelled directly off pObj
 * (`pObj[0xc]`, `pObj[0xf]`, `pObj[0xe]`, `pObj[0xd]` -- the same four
 * displacements the original emits off esi).  This was the obvious
 * untried counterpart to the sixteenth pass's coefficient-symbol fix, on
 * the theory that the term-3 flip is an operand-KIND ranking: it is much
 * worse, REGNORM 48+54 -> 72+83, bytes -15 -> -25, instructions -6 -> -11.
 * VC5 CSEs harder without the pointer locals and loses five more
 * instructions.  The pointer spelling is load-bearing; keep it.
 *
 * STATE (2026-09-03, ninth pass): 2,322 vs 2,328 instructions -- SIX
 * SHORT -- 9,338 vs 9,354 bytes, 20 divergence regions slot-masked (28
 * raw).  Region and byte counts unchanged from the eighth pass.
 * ‼ RETRACTED, and it matters: the eighth pass recorded "2,328/2,328
 * instructions -- EQUAL".  That was `divergence.py` counting the COFF
 * function extent's 16-byte alignment padding -- six trailing nops the
 * extracted original does not have -- as code.  The tool is fixed
 * (commit a00add5); the real gap is SIX MISSING INSTRUCTIONS.  So the
 * eighth pass's "the instruction count is now exactly equal" is not
 * evidence that nothing is missing, and neither is any earlier statement
 * of the same shape in this file.  Wall 3's `lea R,[R + 0x70]` /
 * `fld [R + R + 0x54]` pairs and wall 1's fld hoist are where the
 * multiset says the six sit.
 * NINTH PASS found NO further constant defect.  The eighth pass's advice to
 * re-run the register-blind multiset was followed and `tools/msetdiff.py`
 * had to be fixed first (branch targets and reloc addends were compared
 * literally, so every reloc'd instruction paired as MISSING+EXTRA; 76 rows
 * of noise down to 37 real).  ‼ Two rows that looked like real defects --
 * `push A` vs `push 0` at 0xab (BrFloat12MaxAbs(DAT_106e9a38)) and at
 * 0x12c1 (the logger's format string DAT_100a5db4) -- are ARTEFACTS: both
 * sites carry a relocation and are already correct.  Do not "fix" them.
 * What the clean multiset leaves is 37 rows and all of them are already
 * mapped below: the fxch/faddp spread is walls 1 and 2, the four
 * `[R + A]` vs `[R*K + A]` pairs are wall 4, and `lea R,[R + R + 0x70]` /
 * `fld [R + R + 0x54]` are wall 3.
 * SIXTEENTH PASS (2026-09-03) ‼ WALL 2 IS CLOSED.  The scale block now emits
 * the original's 8|4 batching, exactly, for the first time in nine passes.
 * The lever was the row block's COEFFICIENT SPELLING, and the fix is the one
 * the original's source must always have had: all four coefficients as
 * ARRAY SYMBOLS (`DAT_106e9a38[k]`, `DAT_106e9a68[k]`, `DAT_106e9a58[k]`,
 * `DAT_106e9a48[k]`), not the mixed transcription that was here -- terms 1
 * and 3 as raw-address casts `*(float *)(0x106e9a38 + 4k)` and terms 2 and 4
 * through pointer locals.  A raw-address cast is a compile-time CONSTANT and
 * a symbol reference is a relocation; VC5 schedules the two differently, and
 * the mixed form was a transcription artefact, not a discovery.
 * Masked regions 20 -> 19, bytes 18 short -> 15, instructions unchanged at 6
 * short, and `pV3`/`pV1` go away (removing them is byte-neutral; `pView`
 * must stay, the logger reads it).
 *   ‼ THE TRADE, stated honestly: the register-blind multiset goes 35/29 ->
 *   39/33.  All eight of those rows are ONE defect at four sites -- under
 *   the symbol spelling VC5 emits term 3 object-first (`fld [esi+0x38];
 *   fmul [coef]`) where the original is coefficient-first (`fld [coef];
 *   fmul [esi+0x38]`).  That is worth taking: it swaps a wall that six
 *   passes of probes could not move for a localised operand-order defect,
 *   and it moves the region count and the byte count the right way.
 *   MEASURED EIGHTEENTH PASS, do not re-run, both under the staleness rule
 *   (this function's allocation moved when the coefficients became symbols,
 *   so its dead list was fair game again):
 *     - the REDUNDANT-PARENTHESIS axis on the row terms.  Six variants
 *       (parens around term 3, term 4, 3+4, 1+3, all four, and one outer
 *       pair): none improves the register-blind gap below 48+54, and most
 *       BREAK the 8|4 batching the symbol spelling had just won -- paren=3
 *       and paren=4 give 6|6, paren=34 gives 5|7, paren=13 gives 5|7.  The
 *       current no-paren form is the best of the seven.  The axis pays where
 *       the register-blind gap is already 0; here it is 102 rows.
 *     - WALL 4's byte-offset spelling, re-tested against the new
 *       allocation: `rbo = ring * 4` with all eight if-arm sites through
 *       `*(int *)((char *)base + rbo)` is BYTE-IDENTICAL to the plain index,
 *       exactly as in the eleventh pass.  The verdict HOLDS; wall 4 is not
 *       stale.
 *   MEASURED SEVENTEENTH PASS, do not re-run: the ROW TERM ORDER is not the
 *   lever for the leftover term-3 flip either.  Six permutations measured
 *   (1234, 2134, 1324, 3124, 1243, 2143); all keep the 8|4 batching and all
 *   leave the flip, the best being 1243/2143 at one register-blind row
 *   better (48+53 against 48+54) which is not worth reordering the natural
 *   reading for.  With factor order canonicalised and term order inert, the
 *   flip is an operand-ranking decision, not a spelling: T3 is the one term
 *   whose two operands VC5 ranks the other way round.
 *   MEASURED, do not re-run: the full 4-term spelling mask.  Only SSSS gives
 *   8|4; every mixed mask gives 6|6 (SSLS, LSSS, SSSL) and the old LSLS
 *   gives 5|7.  And the term-3 flip is NOT source-selectable -- swapping the
 *   factor order in the source at term 1, 2, 3, 4 or 3+4 is BYTE-IDENTICAL
 *   every time, so VC5 canonicalises x87 multiply operand order exactly as
 *   it does the integer one.
 *
 * FIFTEENTH PASS (2026-09-03) -- no region closed, two measurement facts
 * that change how this file must be read, and one new probe axis.
 *   ‼ THE ORIGINAL'S FOUR-TERM ROW IS LEFT-ASSOCIATED, which the source
 *   already is.  Read it off the bytes rather than guessing: at 0xda7
 *   `faddp st(2)` folds terms 1 and 2 together first (st0 holds T1, st2
 *   holds T2 at that point), so `(((T1 + T2) + T3) + T4)` is right and any
 *   right-associated variant is the WRONG SOURCE however it scores.
 *   ‼ AND ONE OF THEM SCORES SPECTACULARLY, WHICH IS THE TRAP: the fully
 *   right-associated form `T1 + (T2 + (T3 + T4))` reads DIFFS 4,669 ->
 *   2,490.  That is not a 47% improvement, it is TWO BYTES.  fn.py's DIFFS
 *   is a POSITIONAL byte compare with no alignment, and those two bytes
 *   moved the whole 0x11d3-0x23ec tail from delta -2 to delta 0, flipping
 *   ~2,200 spuriously-mismatching bytes to spuriously-matching ones.  It
 *   recovered two bytes and one instruction and nothing else.  **Compare
 *   DIFFS only between builds of the SAME size; when the size moves, rank by
 *   msetdiff rows, the instruction gap and the masked region map.**
 *   NEW PROBE AXIS, isolated A/B and real: a REDUNDANT OUTER PARENTHESIS
 *   PAIR around the four row expressions -- nothing else changed, same
 *   association -- moves the scale preload 5|7 -> 4|8, grows the recompile
 *   three bytes and takes the register-blind gap 40+46 -> 41+47.  So
 *   parentheses reach VC5's scheduler, not just its parser.  Do NOT tidy the
 *   parentheses in this file, and when an x87 schedule is one notch off try
 *   the same association with and without an outer pair.
 *   MEASURED THIS PASS, all four row associations, no outer parens:
 *     left `((T1+T2)+T3)+T4` = the tree, 4,669 / 5|7 / -18 B  (baseline)
 *     `(T1+T2)+(T3+T4)`                4,695 / 5|7
 *     `T1+(T2+(T3+T4))`                2,490 / 5|7 / -16 B  (SEE TRAP ABOVE)
 *     `(T1+T3)+(T2+T4)`                4,778 / 5|7
 *   None reaches the original's 8-preload; the association is not the lever.
 *   Also measured: the row block's peak x87 depth is 6 here against the
 *   original's 8 (both enter the scale block at depth 0), which is the same
 *   fact as wall 1's missing hoist notch stated as a number.
 *
 * FOURTEENTH PASS (2026-09-03) ‼ RETRACTS THE ELEVENTH PASS'S WALL-2
 * VERDICT.  That pass wrote "the 5-vs-8 preload is a scheduler constant, not
 * a pressure difference we can create".  IT IS NOT A CONSTANT.  Two facts,
 * both measured this pass:
 *   1. THIS FUNCTION ALREADY EMITS AN 8-PRELOAD, at the SECOND scale site
 *      (0x1300, `OUTM(k) = scale * OUTM(k)` x16), where it matches the
 *      original instruction for instruction.  So the compiler is willing.
 *   2. The first site's batch is FIXED AT 5 regardless of how many
 *      statements it contains -- measured at N = 10,11,12,13,14,15,16, the
 *      first run is 5 every time and only the second grows (5,5 / 5,6 / 5,7
 *      / 5,8 / 5,8 / 5,8 / 5,8).  So it is not a batch-size rule; something
 *      at that SITE costs three x87 slots.
 *   Ruled out as the cause: the helper boundary (flat 12 statements give the
 *   same 5), array identity (the same 12 written IN PLACE against one array
 *   still give 5), and extern-vs-defined storage for the two matrices
 *   (defining them here, output matrix first in address order as the stale
 *   comment above the declarations describes, changes nothing).
 *   ‼ WHAT IT IS: the preceding OUTM(12..15) row block -- i.e. WALL 1.  Move
 *   the scale statements ABOVE those four rows and the first batch goes
 *   5 -> 7.  Spell all four row terms as absolute literals (pV3[k] and
 *   pV1[k] rewritten as `*(float *)(0x106e9a68 + 4k)` / `(0x106e9a48 + 4k)`,
 *   matching the two terms that already read that way) and the first batch
 *   goes to EIGHT, at 0xea4, exactly as the original.
 *   SO WALL 2 IS DOWNSTREAM OF WALL 1, not an independent wall.  Fix the row
 *   block's operand kinds and the preload follows.
 *   That spelling is NOT in the tree, because it wrecks the rows while
 *   fixing the scale: byte diff 4,669 -> 4,742, masked regions 20 -> 22, and
 *   the row block itself picks up two SUSPECT resyncs (-190 drift at 0xe7c).
 *   Either half alone is worse still (pV3 only: 4,929; pV1 only: 5,020, and
 *   both give a 4|8 split).  ALSO PROBED AND WORSE, so do not re-run: the
 *   same rewrite with the now-dead `pV3`/`pV1` declarations DELETED, which
 *   was the obvious explanation for the row regression -- 5,027, worse
 *   still, so the dead locals are not the cause and the row block simply
 *   prefers the pointer spelling.  (`pView` must stay either way: the
 *   logger reads it through VIEW(k).)
 *   NEXT LEVER, and it is a real one: the ORIGINAL reads all four view terms
 *   absolutely (`fld [0x106e9a48]`, `fld [0x106e9a6c]`) and its pObj terms
 *   register-relative (`fmul [esi+0x34]`), so the operand KINDS are settled
 *   -- what is not settled is a row spelling that carries those kinds
 *   without costing the rows more than the scale block gains.  Do NOT re-run
 *   the ruled-out causes above (helper boundary, array identity, extern vs
 *   defined, dead-local removal).
 *
 * THIRTEENTH PASS (2026-09-03) re-tested this file's dead list under the
 * staleness rule that fell out of 0x1000A110 ("a dead verdict measured
 * against a wrong frame -- or any other changed allocation input -- is
 * stale").  This function's allocation last moved with the twelfth pass's
 * +0x70 pointer, so the pre-twelfth verdicts were fair game.  RESULT: both
 * re-tests HOLD, and one of them is now a SHARPER verdict than before.
 *   (a) re-measured ALONE rather than in the ninth pass's three-change
 *       bundle: `slot = head; if (--slot < 0)` against `slot = head - 1;
 *       if (slot < 0)` is BYTE-IDENTICAL.  So the `dec`/`jns` against
 *       `lea`/`test`/`jge` difference is not this spelling at all -- VC5
 *       canonicalises the two -- and the ninth pass's "20 -> 21 regions" was
 *       the other two changes in that bundle.  Do not re-run either form.
 *   (b)/wall 5 re-read at the bytes rather than probed blind: the join at
 *       0xad4 is a THREE-VALUE choice, not a spelling.  The original keeps
 *       cHead and base in registers across the param_2 join (its else-arm
 *       reloads both from their slots BEFORE the merge) and therefore has to
 *       read `i` from memory at the guard (`cmp [esp+0x1c],edx`); we keep
 *       `i` in a register and reload cHead/base after the merge.  Two of the
 *       three fit; the original picks the other two.  Probed this pass and
 *       DEAD: expressing the join through the locals (`DAT_1035fb8c = cHead
 *       + 1 + base; i = base;` instead of naming the globals three more
 *       times) moves the reloc-masked byte diff by ONE, 4,669 -> 4,670.
 *   Screens, both negative, do not repeat: this function's frame MATCHES
 *   (`tools/framescreen.py` does not list it), and its `(double)` modelling
 *   is real -- all 49 qword spills are `fstp qword ptr [esp]` varargs pushes,
 *   matched exactly in both streams, with no `fld qword` anywhere -- so the
 *   Glide-is-float lever does not apply here.
 *
 * TWELFTH PASS (2026-09-03) MOVED WALL 3 for the first time in six passes.
 * The wheel record's +0x70 field is now reached through its own pointer,
 * `float *pP = (float *)(wb + (int)pCar + 0x70)`, which is the SAME
 * expression the BrGroundProbeZ argument already used -- so VC5 CSEs the
 * two and addresses x as `[eax]` where it used to spell `[edi+0x70]`, and
 * the argument push reuses the register instead of computing a second sum.
 * Both x reads (x1 and x2) go through `pP[0]`; y and z stay `pW->y` /
 * `pW->z`, which is what the original does (`fld [eax]` / `fadd [eax]` for
 * x, `[edi+0x74]` for y, `[edi+0x78]` for z).
 *   Scoreboard, and read all of it before judging: reloc-masked byte diff
 *   4,727 -> 4,669; register-blind multiset 37 missing/31 extra -> 35/29;
 *   fn.py RAW 120+126 -> 108+114, REGNORM 41+47 -> 40+46.  Masked regions
 *   are FLAT at 20 (0x1c5f and 0x1c90 closed, 0x1fc8 opened) and the
 *   recompile is 2 bytes SMALLER than before (-16 -> -18).  Wall 3's rows
 *   `fld [R]`, `fadd [R]`, `fld [R + R + 0x54]` are gone from the multiset;
 *   what is left of it is one pair, `lea R,[R + 0x70]` (orig) against
 *   `lea R,[R + R + 0x70]` (ours), plus a new `mov R,[R + R + 0x50]` vs
 *   `mov R,[R + 0x50]` -- the original defers `lea edi,[eax+ecx]` until
 *   AFTER the vx/vy reads and we now materialise it before them.
 * TWELFTH-PASS PROBES, DEAD -- do not re-run.  Both are the same finding:
 *   (e) the probe argument spelled `&pW->x` (inline), and
 *   (f) `wb` deleted outright, `pW` defined by the whole four-term sum with
 *       `pP = &pW->x`
 *   each FLIP THE FRAME, not merely the address split: first divergence
 *   collapses +0x2b -> +0x15 (`push edi` moves ahead of the `cmp` and the
 *   `mov edi,1` sink re-opens) and the reloc-masked byte diff explodes to
 *   6,475 / 6,639.  (f) lands at -1 byte / +1 instruction, which looks like
 *   parity and is not: the whole allocation is different.  ‼ So the entry
 *   below is stronger than it reads -- it is not just that a single-use
 *   `wb` merges the four-term sum, it is that ANY spelling which makes the
 *   wheel pointer one sum rewrites the prologue.  Keep `wb` and keep it
 *   used twice.
 *   (g) sinking `pP` into a nested block after the sqrt is BYTE-IDENTICAL
 *       to declaring it with the other locals -- /O2 slot and materialisation
 *       order ignore scope, as the sixth-pass entry already says.
 *
 * ELEVENTH PASS (2026-09-03) closed no region but SETTLED both remaining
 * "shape" walls by reading the original harder, and it retires the framing
 * the tenth pass left behind:
 *   ‼ WALL 4 IS PER-ARM IN THE ORIGINAL, and that is the whole story.
 *   Grep the original for the two ring globals and the split is flat:
 *     if-arm  0x1c96..0x1e63 -- `lea edx,[ecx*4]` materialised once, then
 *             EIGHT `[edx + 0x1035faf0]` / `[edx + 0x1035f750]` sites
 *     else-arm 0x1e75..0x1f08 -- FIVE sites, all folded `[ecx*4 + abs]`,
 *             i.e. byte-identical to what we already emit
 *   The original therefore spells the two arms the SAME way and VC5 picked
 *   different addressing per arm on local pressure.  The dossier's "separate
 *   `rb2 = ring * 4`, 17 uses" probe was converting BOTH arms, which cannot
 *   be right at any spelling.  Re-probed this pass on the IF-ARM ALONE
 *   (`rbo = ring * 4` declared with the other locals, all eight if-arm sites
 *   through `*(int *)((char *)base + rbo)`, else-arm untouched): still folds
 *   to `mov eax,[ecx*4]`, and costs 5 bytes elsewhere (-16 -> -21, 2322 ->
 *   2321 insns).  DEAD -- do not re-run either half.
 *   ‼ THREE OF THE SIX MISSING INSTRUCTIONS ARE DOWNSTREAM OF WALL 4, not
 *   independent defects.  With edx pinned to ring*4 the original is a
 *   register short across the h1 test, so it memory-homes `pDst` in BOTH
 *   arms (`mov [esp+0x20],ebx` at 0x1cba and 0x1cc5) and reloads it at
 *   0x1d00 (`mov edi,[esp+0x20]`) to form `slot`.  We keep pDst in edi and
 *   emit none of the three.  Those are exactly the msetdiff rows
 *   `mov dword ptr [esp+S], R` x2 and `mov R, dword ptr [esp+S]` x1 -- so do
 *   NOT hunt them as a separate missing-store defect.  Region 20's -13 bytes
 *   is this same spill, not the `[edx+A]` encodings (ours are one byte
 *   LARGER per site: 7-byte SIB vs the original's 6-byte base+disp32).
 *   ‼ WALL 2 IS NOT THE HELPER BOUNDARY.  The tenth pass recorded that the
 *   original's 8|4 batching "is exactly that call boundary".  It is not:
 *   replacing BrRowScale8+BrRowScale4 with twelve flat `OUTM(k) = scale *
 *   VIEWS(k)` statements emits the IDENTICAL five-deep preload (five
 *   `fld [esp+0x10]`, `fxch st(4)`, ...), so the split is 5|7 with or
 *   without the helpers and grouping does not address it.  Also ruled out
 *   this pass: an x87 stack leak.  Simulating depth over both streams shows
 *   both enter the block at depth 0 (the preceding `fstp` of OUTM(15)
 *   drains), so the 5-vs-8 preload is a scheduler constant with the same
 *   pipeline shape (3 in flight, fxch/fmul/fstp identical modulo the depth
 *   offset), not a pressure difference we can create.  DEAD.
 * TENTH PASS (2026-09-03) added `divergence.py --deltas`, and it re-ranks
 * this function's twenty regions once and for all.  Byte drift per region:
 *   r4  (0xf2a, scale block)   -56, and r5 gives +58 straight back
 *   r13 (0x1dc7, trail append) -15
 *   r20 (0x23ec, tail fixup)   -10   r18 +9   r17 -8   r6/r7 +-7
 *   everything else moves 0..4 bytes.
 * So SIXTEEN of the twenty regions are worth a few bytes each and only two
 * blocks carry the shape: the x87 batching of the scale block (wall 2) and
 * the ring-index addressing form (wall 4). Grind those or none.
 *   r4/r5, read fresh: the source is already `BrRowScale8` then
 *   `BrRowScale4` and the original's 8|4 batching is exactly that call
 *   boundary -- orig tops the x87 stack up to EIGHT `fld [esp+0x10]` before
 *   the first fmul and starts the 4-group with a fresh four, while we start
 *   multiplying at five and then refill with SEVEN, merging the tail of the
 *   8-group with the whole 4-group into one batch.  Both do 12 loads for 12
 *   multiplies; only the batching differs, and the operand order is already
 *   right (`k * sr[i]`, i.e. `fld k; fmul mem`, confirmed against the
 *   bytes).  Nothing above the helper is source-visible: this is the x87
 *   scheduler choosing its refill point.
 *   r13, read fresh: the block is instruction-for-instruction ours, and the
 *   whole 15 bytes is wall 4 -- orig's `mov [edx + 0x1035faf0], eax` (byte
 *   offset in a register, absolute base as displacement) against our
 *   `mov [ecx*4], eax`, repeated across the block.
 * TENTH-PASS PROBES, DEAD -- do not re-run:
 *   (c) tail fixup (r20): a temp for the SUM (`vis = cHead + base;
 *       DAT_102e0ca0 = vis - DAT_102e0ca0;`) is BYTE-IDENTICAL to the
 *       parenthesised one-liner.  The dossier had only ever probed a temp
 *       for the GLOBAL; both are canonicalised, so the association really
 *       is not source-selectable here.  Entry 6b stands.
 *   (d) ‼ the seventh pass's drain-loop lever does NOT generalise to the
 *       trail-append loop.  Converting that loop's two FLAT ring arrays
 *       (7 + 10 sites, both arms together) to a byte-offset induction
 *       variable -- `rt = iCar << 4` at loop entry, `rt += 4` per wheel,
 *       every access `*(int *)((char *)base + rt)`, `ring` kept for the
 *       `ring * 500` record term, i.e. exactly the spelling that gained a
 *       region in the drain loops -- explodes it: 20 -> 64 masked regions,
 *       +11 instructions.  The drain loops' induction variable IS the loop
 *       counter; here `ring` is recomputed from two enclosing counters, so
 *       a byte-offset IV is a THIRD induction variable rather than a
 *       restatement of an existing one, and VC5 rebuilds the whole region
 *       around it.  Wall 4 stays open and this spelling is now closed.
 * NINTH-PASS PROBES, DEAD -- do not re-run:
 *   (a) 0x1e8d, else-arm of the trail append (`slot = head - 1`): orig
 *       emits `mov edi,edx; dec edi; jns` where we emit `lea edi,[eax-1];
 *       test edi,edi; jge` -- one decision, since `dec` sets SF and `lea`
 *       does not.  Probed together with reversing the guard to
 *       `if (DAT_1035f750[ring] != DAT_1035faf0[ring])` (orig loads f750
 *       FIRST, we load faf0 first) and sinking `head =` inside it, spelled
 *       `slot = head; if (--slot < 0)`: 20 -> 21 masked regions and the
 *       instruction count leaves EQUAL (2,329), because the copy-then-dec
 *       does not appear and instead the later `head = head + 1` flips from
 *       orig's `lea R,[R+1]` to `inc R`.  The lea/dec choice here is
 *       downstream of the allocation, not source-selectable at this site.
 *   (b) 0xb21/0xb28 (loop pre-header of the object walk): orig hoists the
 *       combiner constants into registers across the param_2 join -- one
 *       arm sets `esi,0xfffa` and `edi,1`, and `mov edx,6` is scheduled
 *       after the `cmp` at 0xb37 -- while we materialise `mov edx,6` in
 *       BOTH arms and never home the 1.  Same cause as wall 5 (the join at
 *       0xad4 keeping cHead/base in registers); it is one register-
 *       allocation decision, not four EMIT() spellings.
 * EIGHTH PASS: the ring-wrap test is `if (head >= 500)`, not `if (499 <
 * head)`.  Four sites; orig emits `cmp X,0x1f4; jl` and we were emitting
 * `cmp X,0x1f3; jle`.  Both spellings are semantically identical, so this
 * is a transcription defect in the CONSTANT, and it dropped the true byte
 * diff 3,855 -> 3,727 and brought the instruction count to exactly equal.
 * ‼ READ THIS BEFORE TRUSTING THE REGION COUNT: `divergence.py` compares
 * "mnemonic + operand SHAPE with imm32 wildcarded" (see its `norm`), so it
 * is BLIND to immediate-operand defects -- all four of these sites scored
 * as matching for eight passes.  The region count went UP here (19 -> 20)
 * while the function got 128 bytes closer, because fixing the constant
 * re-opened the 0x189c `mov edi,1` sink that the seventh pass had closed.
 * Region count is a proxy; on any change that touches a constant, check
 * the reloc-masked byte diff and the instruction count too.  A periodic
 * register-blind instruction-multiset diff (normalise registers and
 * reloc'd addresses, keep small immediates) is what surfaced this and is
 * worth re-running on any long-stalled function.
 *
 * Seventh pass: 19 regions, 2,324 instructions.  CLOSED that
 * pass: BOTH drain loops converted to BYTE-offset induction variables
 * together (`c2 << 4` / `dCar << 4`, `+= 4`, every ring read and write
 * spelled `*(int *)((char *)base + off)`).  This is the form wall 7 below
 * had recorded as rejected -- the earlier measurement applied it to the
 * SECOND loop only, and one loop alone genuinely does flip the allocation
 * (first loop alone: +7 insns, 27 raw; second alone: the effects wall 7
 * lists).  Applied to both at once it closes the `mov edi,1` sink at
 * 0x189c outright and shrinks the two drain pre-header regions (0x1f59 ->
 * 0x1f66, 0x2006 -> 0x2016; ours now emits orig's `shl edx,4` dring init).
 * GENERAL LESSON, worth more than the region: a strength-reduction
 * spelling has to be applied to EVERY loop that shares the induction
 * pattern before it is measured.  VC5 picks one IV strategy per region;
 * a half-converted pair leaves it straddling both and scores worse than
 * either consistent form.  Re-test any "structurally right but flips the
 * allocation" verdict in this file the same way before trusting it.
 *
 * Sixth pass (2026-09-01): 20 regions.  Slot map now
 * matches orig for scale/fMin/fMax/i (0x10/0x14/0x18/0x1c): the clamp
 * factor is written back into `scale` (orig stores it to scale's slot),
 * and the active[]-init loop has its own block-scoped counter (a shared
 * function-scoped iCar sat in one slot for both loops; orig packs the
 * second counter into bTexLoaded's dead slot).  /O2 slot packing ignores
 * names, declaration order and scope -- it is lifetime-driven only.  CLOSED this
 * pass: the 8-byte frame delta (prologue now exact, `sub esp,0xdc`), the
 * whole trail-quad x87 stream, and the `cmp [ebp+0xc],ebx` param_2 test.
 * The trail block is a struct-typed wheel pointer (BrWheelRec): the FIRST
 * float def through a multi-use pointer var is an integer bit-copy home
 * (`mov edx,[..]; mov [slot],edx`, every later use reloads the slot), the
 * second is fld'd and kept on x87; ox/oy are repeated expressions (CSE
 * temps: `fst`-homed and kept on the stack, px/py loaded once), not named
 * locals (named temps are fstp'd and force px/py double loads); the sum is
 * `dx*dx + dy*dy`; pT is bound inside `if (param_2)`.
 * REMAINING WALLS:
 *   1. per-row: D-product's fld wants exactly ONE hoist notch (orig:
 *      fld V4; fxch3; faddp2 -- ours: fxch2; faddp1 first).  Hoisting is
 *      binary by operand kind (aggressive or none); no probed spelling
 *      gives one notch.
 *   2. scale block: 5|7 preload split vs orig 8|4 (one fld leak, greedy
 *      refill after first fstp).  Nested-scope k copies, global-symbol
 *      helpers, volatile dest barrier, counted loops: no improvement.
 *   3. wheel-pointer address form: orig computes `iw*0x40 + param_4 +
 *      negCar0` into eax (param_4 added FIRST) and keeps pCar as the index
 *      register ([eax+ecx+disp] loads, `lea edi,[eax+ecx]` after the dx
 *      reload).  A single-use `wb` is forward-substituted and the 4-term
 *      sum merges into one register (esi); any second source use of `wb`
 *      restores the 3+1 split.  The BrGroundProbeZ arg spelled through
 *      `wb + pCar + 0x70` is that placeholder second use (orig: `lea
 *      eax,[edi+0x70]` from pW).  Probed and dead: add-order permutations,
 *      negCar0 renames/types/scopes/LICM, param_4 renames, two-step wb or
 *      pW defs, pointer-difference car base (VC5 cancels it), index-based
 *      `param_4 + iCar*0x2b68` (VC5 mints a second IV; cls block breaks),
 *      py/px/z/vy via the two-part sum (uses after the call spill).
 *   4. `ring*4` CSE: orig materializes `lea edx,[ecx*4]` and addresses
 *      both ring arrays as [edx+base] (edx live through the tail, so pDst
 *      is memory-homed and ebx is scratch); ours folds [ecx*4+base].
 *      Byte-offset spellings still fold.  2026-09-03 refinement: orig keeps
 *      BOTH values live -- the int index in ecx (it is still needed for the
 *      `ring * 500` record term, `lea ecx,[ecx+ecx*4]` at 0x1dd7) and the
 *      byte offset in edx.  That rules out the single-variable form: making
 *      `ring` itself the byte offset (`(iWheel + iCar*4) * 4`) is
 *      SEMANTICALLY WRONG here, because `ring * 500` then scales four times
 *      over -- it emits a closer `shl ecx,2` only by breaking the record
 *      index, so ignore that shape.  The correct two-variable form (a
 *      separate `rb2 = ring * 4` used only for the two flat arrays, 17 uses)
 *      is byte-identical to the plain index: VC5 forward-substitutes it
 *      because `ring` is itself an affine expression it can re-fold into a
 *      scale-4 addressing mode.  Both `* 4` and `<< 2` measured.  To reach
 *      orig the scaled index has to be un-foldable at its uses; no spelling
 *      found yet.
 *   5. loop entry (0xad4): orig keeps cHead/base in ecx/eax across the
 *      param_2 join (else-arm reloads them after the call) and compares
 *      i from memory; ours reloads them in the pre-header.
 *   6. slot permutation (residual): bSolo/iCar vs pDst swapped between
 *      0x20 and 0x24; dx overlays 0x28 (orig 0x18); the drain counter
 *      c2/dCar overlays 0x30 (orig 0x28).  Not lowest-free-slot, not
 *      LIFO/FIFO of freed slots; packer order still unknown.
 *   6b. tail visible-count fixup (0x23ec): orig computes `(cHead + base)
 *      - g` left-to-right with cHead RELOADED from its slot; ours
 *      reassociates to `(base - g) + cHead` and rotates eax/ecx.  Same
 *      instruction count either way.  Probed and dead: an explicit temp
 *      (`t = g; if (t == 0x1000) t = cHead; g = cHead + base - t;`) --
 *      byte-identical codegen to the global-store form, so VC5 canonicalizes
 *      the two and the association is not source-selectable here.
 *   6c. trail-arena guard (0x20f4): orig loads the cursor global first and
 *      adds 0x80 to it first; ours loads the arena base first.  Reversing
 *      the comparison (`(uint32_t *)(arena + 0x3e800) <= cursor + 0x20`)
 *      gives byte-identical codegen -- VC5 canonicalizes relational operand
 *      order before scheduling the loads.  Dead.
 *   6d. the varargs logger's integer arg (0x11eb): `mov edx,[esp+0x104]`
 *      (idx) is scheduled two 8-byte double-pushes LATER in orig than in
 *      ours; same slot, same variable (displacements differ by exactly the
 *      0x10 of the two pushes).  Pure scheduler notch inside a 50-argument
 *      push stream -- no source construct addresses it.  T3a.
 *   7. drain loop (T3a now): the orig's block layout (both `goto dead`
 *      sites as forward `je` to ONE block after the fl==0 arm) comes from
 *      the FIRST test nested (`if (dh != tail) {...} else { pA[dw] = 0; }`)
 *      -- the goto form makes VC5 place the dead block after the second
 *      test and invert it; the orig's `shl edx,4` init of the inner ring
 *      IV comes from a BYTE-offset IV (`dCar << 4`, `+= 4`, `*(int *)
 *      ((char *)cursor + off)`; `dCar * 4` mints an outer dCar*16 IV,
 *      `<< 2` gives lea;lea).  SUPERSEDED 2026-09-03 for the IV half: the
 *      byte-offset form IS in the tree now and gains a region, because it
 *      is applied to BOTH drain loops at once (see STATE above).  The
 *      "flips the global allocation" finding stands only for converting
 *      one loop of the pair.  The goto-vs-nested-if half was RE-MEASURED
 *      2026-09-03 after the IV half landed (the two had only ever been
 *      measured jointly, so the verdict was stale) and it HOLDS: nesting
 *      the first test costs nine regions, 19 -> 28, in both spellings --
 *      the dossier's literal `if (dh != tail) {...} else { dead: pA[dw]=0; }`
 *      and a fully structured form with one shared exit.  Do not re-run.
 *      What actually places the block is SIZE, not the source construct:
 *      the dead block is two instructions and the body is ~500, and VC5
 *      lays the short successor down first (ours: `jne body` over an inline
 *      dead block; orig: `je` forward from BOTH tests to one dead block
 *      after the fl==0 arm).  Regions 16 and 18 are the two halves of that
 *      one placement decision.
 *      Region 15 (0x2016) is now a pure schedule permutation -- identical
 *      seven-instruction multiset (2 loads, xor, shl, 3 stores), orig
 *      orders it load/load/xor/store/shl/store/store and ours
 *      load/load/shl/xor/store/store/store.  T3a.
 * @implements stays live for the sweep; rule 2 forbids claiming a match
 * until the diff is clean.
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <math.h>

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;

/* ------------------------------------------------------------------ */
/* Callees                                                            */
/* ------------------------------------------------------------------ */
void  FUN_1000e320(int a, int b, int c);              /* 0x1000E320 */
float BrFloat12MaxAbs(const float *pv);               /* 0x1002A957 */
int   BrPodNop();                                     /* 0x10008D60, varargs no-op logger */
void  BrRdpSetCombineLERP(uint32_t *pOut,             /* 0x1001CF90 */
                          int a0,  int b0,  int c0,  int d0,
                          int Aa0, int Ab0, int Ac0, int Ad0,
                          int a1,  int b1,  int c1,  int d1,
                          int Aa1, int Ab1, int Ac1, int Ad1);
void  BrDlRectCmdFlush(void);                         /* 0x1002C4A3 */
void  FUN_1002af17(void);                             /* 0x1002AF17 */
void  FUN_10008f90(void);                             /* 0x10008F90 */
float *BrMtxPoolAlloc(void);                          /* 0x10062500 (BrSub_10069490) */
void  BrGuMtxStore(const float *pSrc, float *pDst);   /* 0x10029E50 */
void  FUN_1000cba0(int a, int idx, int cls, int b, int c); /* 0x1000CBA0 */
void  BrCopy8Words(void *pDst, const void *pSrc);     /* 0x100189A0 */
float BrGroundProbeZ(const float *pPoint);            /* 0x100682C0 */

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */
extern int       DAT_106ed6a8;      /* view/split flags */
extern int       DAT_106ed6ac;
extern int       DAT_106ed6b0;
extern int       DAT_106ed6b4;
extern int       DAT_100b2f04;      /* car count */
extern int       DAT_1035fb8c;      /* object count (total) */
extern int       DAT_1035fb9c;
extern int       DAT_1035fb74;
extern uint32_t  DAT_1035fb84;      /* othermode_L low word  */
extern uint32_t  DAT_1035fb88;      /* othermode_L high word */
extern float     DAT_1035f7d0;
extern uint32_t  DAT_1035f7e0;      /* object cull mask */
extern int       DAT_100a9360;
extern int       DAT_100b3014;
extern int       DAT_102e0c9c;
extern int       DAT_102e16bc;
extern int       DAT_102e0ca0;
extern uint32_t *DAT_106e7710;      /* display-list write cursor */
extern uint32_t  DAT_106ea360;
extern uint16_t  DAT_106e770c;
extern uint32_t  DAT_100a9ec0[];    /* identity/default matrix */
extern int       DAT_1035fba0;      /* light-table pointer (value, not array) */
extern uint8_t   DAT_100a5ca8[];
extern uint8_t   DAT_100a5cb0[];
extern int       DAT_1035fb70;
extern uint32_t  DAT_106ecb40;      /* fog colour */
extern uint32_t  DAT_106e9a78;
extern uint32_t  DAT_106e7718;
extern uint32_t  DAT_106e79b0;
extern int       DAT_100aa00c;
extern uint32_t  DAT_106e72e8;
extern int       DAT_106ea3f4;
extern int       DAT_106e8204;
extern int       DAT_100aa010;
extern int       DAT_100aa044;
extern int       DAT_1035f7d4;
extern int       DAT_102e170c;
extern int       DAT_102e16a8;
extern uint16_t  DAT_1035e710[];    /* sorted object index list */
extern int       DAT_10396eb0;      /* trail-quad enable */
extern int       DAT_106eed38;      /* object table base, 0x54 stride */
extern int       DAT_10396eb4;   /* mask read 32-bit, tested 16-bit */
extern int       DAT_10396eac;
extern int       DAT_10396ea8;
extern uint32_t  DAT_106e79e0[];    /* fog colour table [4] */
extern int       DAT_106e772c;
extern int       DAT_106e7734;
extern int       DAT_106e86a0;
extern int       DAT_106eed34;      /* object name table */
extern char      DAT_100a5ea0[];    /* fallback name */
extern char      DAT_100a5db4[];    /* "Bad Final Matrix..." */
extern float     DAT_100771f8;      /* clip range constants */
extern double    DAT_10077238;
extern double    DAT_10077240;
extern float     DAT_10077248;
extern float     DAT_1007724c;
extern uint32_t *DAT_1035f7d8;      /* trail-batch patch cursor */
extern uint32_t  DAT_1184c470;      /* trail texture id */
extern uint32_t  DAT_118ec98c;      /* light texture id */
extern float    *DAT_1035faec;      /* trail vertex build cursor */
extern float     DAT_10077218;
extern int       DAT_10b71b00;
extern int       DAT_10226e80;
extern int       DAT_100a5d98[];    /* wheel index remap [4] */
extern float     DAT_10077250;
extern float     DAT_10077254;
extern float     DAT_10077258;
extern int       DAT_1035faf0[];    /* per-wheel ring heads [cars*4] */
extern int       DAT_1035f750[];    /* per-wheel ring tails [cars*4] */
extern uint8_t   DAT_10386ca8[];    /* surface class table */
extern int       DAT_102e16ac;      /* trail vertex arena base */
extern uint32_t *DAT_1035f7dc;      /* trail vertex write cursor */

/* The output and view matrices are DEFINED here, initialized, in address
 * order: VC5's x87 operand selection ranks memory operands by their known
 * section offsets, which zero-valued extern relocs cannot reproduce.
 * (0x106e78f0 precedes 0x106e9a38 in the image; the definitions keep that
 * order.) */
extern float DAT_106e9a38[16];   /* the view matrix (1-D, scale block) */
extern float DAT_106e9a48[4];    /* view matrix row 1 */
extern float DAT_106e9a58[4];    /* view matrix row 2 */
extern float DAT_106e9a68[4];    /* view matrix row 3 */
extern float DAT_106e78f0[16];   /* the output matrix */
typedef struct BrObjXlat { float tx, ty, tz, tw; } BrObjXlat;
#define OUTM(k)  (DAT_106e78f0[k])
#define VIEW(k)  (pView[k])

#define VIEWS(k) (DAT_106e9a38[k])   /* symbol spelling, scale block */

static __inline void BrRowScale8(float *d, float *sr, float k)
{
    d[0] = k * sr[0];
    d[1] = k * sr[1];
    d[2] = k * sr[2];
    d[3] = k * sr[3];
    d[4] = k * sr[4];
    d[5] = k * sr[5];
    d[6] = k * sr[6];
    d[7] = k * sr[7];
}
static __inline void BrRowScale4(float *d, float *sr, float k)
{
    d[0] = k * sr[0];
    d[1] = k * sr[1];
    d[2] = k * sr[2];
    d[3] = k * sr[3];
}

/* The per-wheel trail record, viewed from the car base + iw*0x40: velocity
 * at +0x50 and the contact point at +0x70.  A struct-typed pointer is what
 * makes VC5 bit-copy the first float def and fld the second. */
typedef struct BrWheelRec {
    char  pad[0x50];
    float vx, vy;
    char  pad2[0x18];
    float x, y, z;
} BrWheelRec;
/* The trail ring: 500 segments per wheel, 4 wheels per car. */
typedef struct BrTrailSeg {
    float    x1;                    /* 0x10273690 */
    float    y1;                    /* 0x10273694 */
    float    z1;                    /* 0x10273698 */
    float    x2;                    /* 0x1027369C */
    float    y2;                    /* 0x102736A0 */
    float    z2;                    /* 0x102736A4 */
    uint32_t flags;                 /* 0x102736A8 */
} BrTrailSeg;
extern BrTrailSeg DAT_10273690[];

/* The eight-byte display-list append, inlined at every site. */
#define EMIT(W0, W1) \
    { uint32_t *p_ = DAT_106e7710; DAT_106e7710 += 2; \
      p_[0] = (uint32_t)(W0); p_[1] = (uint32_t)(W1); }

/* An advanced-slot handed to the combiner builder. */
#define EMIT_SLOT(S) \
    { (S) = DAT_106e7710; DAT_106e7710 += 2; }

/* The same append through the trail-batch's own cursor. */
#define TEMIT(W0, W1) \
    { uint32_t *q_ = pT; pT += 2; \
      q_[0] = (uint32_t)(W0); q_[1] = (uint32_t)(W1); }

/* WHAT IT DOES: build the frame's scene display list -- global state
 * preamble, every scene object's matrix + DL, then the trail quads. */
/* @implements 0x1000EAF0 glide BrSceneDlBuild */
void BrSceneDlBuild(int param_1, int param_2, int param_3, int param_4)
{
    int      i;
    int      bSolo;
    int      bTexLoaded;
    uint32_t *pS;
    int      cHead;
    int      base;
    int      nTotal;
    int      firstVis;
    uint16_t idx;
    float    *pObj;
    uint16_t *pDst;
    int      iCar;
    int      iWheel;
    int      ring;
    int      head;
    int      slot;
    uint32_t *pT;
    int      *pCar;
    uint8_t  active[32];
    int      cursor[32];

    bTexLoaded = 0;
    if (DAT_106ed6ac == 0) {
        bSolo = 1;
        if (DAT_106ed6b4 != 0) {
            bSolo = 0;
        }
    } else {
        bSolo = 0;
    }
    if (param_2 == 0) {
        i = 0;
        if (0 < DAT_100b2f04) {
            uint32_t *p = (uint32_t *)(param_4 + 0x2a00);
            do {
                p[-1] = 0x44800000;
                p[0] = 0x44800000;
                p[7] = 0;
                p[8] = 0x44800000;
                p[0xf] = 0x44800000;
                p[0x10] = 0;
                p[0x17] = 0;
                p[0x18] = 0;
                p[-6] = 0;
                p[-7] = 0;
                p[-8] = 0;
                p[-9] = 0;
                i = i + 1;
                p = p + 0xada;
            } while (i < DAT_100b2f04);
        }
        FUN_1000e320(param_1, param_3, param_4);
        DAT_1035fb9c = DAT_1035fb8c;
        base = DAT_1035fb8c;
        DAT_1035fb74 = -1;
        cHead = -1;
        DAT_1035f7d0 = BrFloat12MaxAbs(DAT_106e9a38);
        DAT_1035f7e0 = 0;
        if (bSolo) {
            DAT_1035f7e0 = 0x800;
        }
        if (DAT_100a9360 != 1 && DAT_100a9360 != 6 &&
            (DAT_100a9360 != 5 || *(char *)(*(int *)(param_4 + 0xe8c) + 4) != '\0')) {
            DAT_1035f7e0 = DAT_1035f7e0 | 0x4000;
        }
        if (DAT_106ed6a8 != 0 &&
            (DAT_106ed6b0 != 0 || DAT_106ed6b4 != 0 || DAT_106ed6ac != 0) &&
            DAT_100b3014 != 2 && DAT_100b3014 != 8) {
            DAT_1035f7e0 = DAT_1035f7e0 | 0x20;
        }
        DAT_102e0c9c = 0x1000;
        DAT_102e16bc = 0x1000;
        DAT_102e0ca0 = 0x1000;
    }
    BrPodNop(0, 0xff, 0x80, 0x80, 0xff);
    DAT_1035fb84 = 0xc8000000;
    if (DAT_106ed6a8 == 0) {
        DAT_1035fb84 = 0xc080000;
    }
    DAT_1035fb88 = 0x112038;
    EMIT(0x1030040, DAT_106ea360);
    EMIT(0x1060040, DAT_100a9ec0);
    EMIT(0xbc00000e, DAT_106e770c);
    EMIT(0x3840010, DAT_1035fba0);
    EMIT(0x3820010, DAT_1035fba0 + 0x10);
    EMIT(0xbc000002, 0x80000040);
    EMIT(0x3860010, DAT_100a5cb0 + DAT_1035fb70 * 0x18);
    EMIT(0x3880010, DAT_100a5ca8 + DAT_1035fb70 * 0x18);
    EMIT(0xbc00000a, DAT_106ecb40);
    EMIT(0xbc00040a, DAT_106ecb40);
    EMIT(0xbc00200a, DAT_106e9a78);
    EMIT(0xbc00240a, DAT_106e9a78);
    EMIT(0xe7000000, 0);
    EMIT(0xba001301, 0x80000);
    EMIT(0xba000903, 0xc00);
    EMIT(0xba000801, 0);
    EMIT(0xb9000002, 1);
    EMIT(0xba000602, DAT_106e7718);
    EMIT(0xba000402, DAT_106e79b0);
    EMIT(0xba001402, 0);
    EMIT(0xf9000000, 0);
    EMIT(0xba001402, 0x100000);
    EMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
    EMIT_SLOT(pS);
    BrRdpSetCombineLERP(pS, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3e8, 0, 0x3ec, 0,
                            0, 0, 0, 0x3e8);
    EMIT(0xba001102, 0);
    EMIT(0xba001001, DAT_100aa00c != 0 ? 0x10000 : 0);
    EMIT(0xba000e02, 0);
    EMIT(0xba000c02, DAT_106e72e8);
    EMIT(0xbc000006, 0);
    EMIT(0xb6000000, 0x853200);
    EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000) |
                     (DAT_100aa010 != 0 ? 0x200 : 0) |
                     (DAT_106ed6a8 != 0 ? 0x10000 : 0) | 0xa0005);
    BrDlRectCmdFlush();
    EMIT(0x1030040, DAT_106ea360);
    EMIT(0xbc00000e, DAT_106e770c);
    EMIT(0x1060040, DAT_100a9ec0);
    EMIT(0xbc000002, 0x80000040);
    EMIT(0x3860010, DAT_100a5cb0 + DAT_1035fb70 * 0x18);
    EMIT(0x3880010, DAT_100a5ca8 + DAT_1035fb70 * 0x18);
    EMIT(0xbc00000a, DAT_106ecb40);
    EMIT(0xbc00040a, DAT_106ecb40);
    EMIT(0xbc00200a, DAT_106e9a78);
    EMIT(0xbc00240a, DAT_106e9a78);
    FUN_1002af17();
    FUN_1002af17();
    EMIT(0xf9000000, 0);
    EMIT(0xb6000000, 0x53200);
    EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000) |
                     (DAT_100aa010 != 0 ? 0x200 : 0) |
                     (DAT_106ed6a8 != 0 ? 0x10000 : 0) | 0xa0005);
    EMIT(0xe7000000, 0);
    EMIT(0xba001402, 0x100000);
    EMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
    EMIT_SLOT(pS);
    BrRdpSetCombineLERP(pS, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3ea, 0x3e9, 0x3f5, 0x3e9,
                            0x3e8, 0, 0x3ec, 0,
                            0, 0, 0, 0x3e8);
    EMIT(0xfa001700, 0xff0000ff);
    EMIT(0xba001102, 0);
    EMIT(0xba001001, DAT_100aa00c != 0 ? 0x10000 : 0);
    EMIT(0xba000e02, 0);
    EMIT(0xba000c02, DAT_106e72e8);
    if (DAT_100aa044 == 1) {
        EMIT(0xbc000404, 1);
        EMIT(0xbc000c04, 1);
        EMIT(0xbc001404, 0xffff);
        EMIT(0xbc001c04, 0xffff);
    } else {
        EMIT(0xbc000404, 6);
        EMIT(0xbc000c04, 6);
        EMIT(0xbc001404, 0xfffa);
        EMIT(0xbc001c04, 0xfffa);
    }
    EMIT(0xbb001001, 0xffffffff);
    EMIT(0xb6000000, 0xc0000);
    EMIT(0xe8000000, 0);
    EMIT(0xf5100000, 0x7000000);
    EMIT(0xf50001f0, 0x6000000);
    EMIT(0xf5000100, 0x5000000);
    if (param_2 != 0) {
        cHead = DAT_1035fb74;
        base = DAT_1035fb9c;
        DAT_1035fb8c = DAT_1035fb74 + 1 + DAT_1035fb9c;
        i = DAT_1035fb9c;
    } else {
        FUN_10008f90();
        i = 0;
    }
    if (i < DAT_1035fb8c) {
        nTotal = cHead + base;
        firstVis = 1 - base;
        pDst = DAT_1035e710 + cHead;
        do {
            if (i == DAT_1035f7d4 || i == DAT_102e170c) {
                if (DAT_100aa044 == 1) {
                    EMIT(0xbc000404, 1);
                    EMIT(0xbc000c04, 1);
                    EMIT(0xbc001404, 0xffff);
                    EMIT(0xbc001c04, 0xffff);
                } else {
                    EMIT(0xbc000404, 6);
                    EMIT(0xbc000c04, 6);
                    EMIT(0xbc001404, 0xfffa);
                    EMIT(0xbc001c04, 0xfffa);
                }
                if (i == DAT_102e170c) {
                    EMIT(0xb7000000, 0x800000);
                }
            }
            if (i < base) {
                idx = DAT_1035e710[i];
                pObj = (float *)(DAT_106eed38 + idx * 0x54);
                if ((uint16_t)(*(uint16_t *)(pObj + 0x12) & DAT_10396eb4) == 0 &&
                    (DAT_10396eac == 0 || idx != (uint32_t)DAT_10396ea8) &&
                    (DAT_1035f7e0 & *(uint16_t *)(pObj + 0x13)) == 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 8) == 0) goto draw;
                    if (DAT_102e0ca0 == 0x1000 && i > DAT_102e16a8) {
                        DAT_102e0ca0 = firstVis + nTotal;
                    }
                    cHead = cHead + 1;
                    pDst = pDst + 1;
                    nTotal = nTotal + 1;
                    *pDst = DAT_1035e710[i];
                }
            } else {
                idx = DAT_1035e710[nTotal - i];
                pObj = (float *)(DAT_106eed38 + idx * 0x54);
draw:
                if ((*((uint8_t *)pObj + 0x4d) & 0x20) != 0) {
                    float fMax, fMin, scale;
                    float *pView = DAT_106e9a38;
                    float *pTw = pObj + 0xf;
                    float *pTy = pObj + 0xd;
                    float *pPos = pObj + 0xc;
                    float *pTz = pObj + 0xe;
                    if (!bTexLoaded) {
                        bTexLoaded = 1;
                        EMIT(0x1020040, DAT_100a9ec0);
                    }
                    OUTM(12) = ((DAT_106e9a38[0] * pPos[0] + DAT_106e9a68[0] * pTw[0]) + DAT_106e9a58[0] * pTz[0]) + DAT_106e9a48[0] * pTy[0];
                    OUTM(13) = ((DAT_106e9a38[1] * pPos[0] + DAT_106e9a68[1] * pTw[0]) + DAT_106e9a58[1] * pTz[0]) + DAT_106e9a48[1] * pTy[0];
                    OUTM(14) = ((DAT_106e9a38[2] * pPos[0] + DAT_106e9a68[2] * pTw[0]) + DAT_106e9a58[2] * pTz[0]) + DAT_106e9a48[2] * pTy[0];
                    OUTM(15) = ((DAT_106e9a38[3] * pPos[0] + DAT_106e9a68[3] * pTw[0]) + DAT_106e9a58[3] * pTz[0]) + DAT_106e9a48[3] * pTy[0];
                    scale = *pObj;
                    BrRowScale8(DAT_106e78f0, DAT_106e9a38, scale);
                    BrRowScale4(DAT_106e78f0 + 8, DAT_106e9a58, scale);
                    {
                    float *pM = BrMtxPoolAlloc();
                    fMax = 0.0f;
                    fMin = 0.0f;
                    if (OUTM(12) >= DAT_100771f8) {
                        fMax = OUTM(12);
                    }
                    if (OUTM(12) <= DAT_100771f8) {
                        fMin = OUTM(12);
                    }
                    if (OUTM(13) >= fMax) {
                        fMax = OUTM(13);
                    }
                    if (OUTM(13) <= fMin) {
                        fMin = OUTM(13);
                    }
                    if (OUTM(14) >= fMax) {
                        fMax = OUTM(14);
                    }
                    if (OUTM(14) <= fMin) {
                        fMin = OUTM(14);
                    }
                    if (OUTM(15) >= fMax) {
                        fMax = OUTM(15);
                    }
                    if (OUTM(15) <= fMin) {
                        fMin = OUTM(15);
                    }
                    if (fMax > DAT_10077238 || fMin < DAT_10077240) {
                        if (i == 1) {
                            char *pName;
                            if (DAT_106eed34 != 0) {
                                pName = *(char **)(DAT_106eed34 + idx * 4);
                            } else {
                                pName = DAT_100a5ea0;
                            }
                            BrPodNop(DAT_100a5db4, idx, pName, (double)scale,
                                     (double)pObj[0], (double)pObj[1], (double)pObj[2],
                                     (double)pObj[3], (double)pObj[4], (double)pObj[5],
                                     (double)pObj[6], (double)pObj[7], (double)pObj[8],
                                     (double)pObj[9], (double)pObj[10], (double)pObj[0xb],
                                     (double)pObj[0xc], (double)pObj[0xd], (double)pObj[0xe],
                                     (double)pObj[0xf],
                                     (double)VIEW(0), (double)VIEW(1),
                                     (double)VIEW(2), (double)VIEW(3),
                                     (double)VIEW(4), (double)VIEW(5),
                                     (double)VIEW(6), (double)VIEW(7),
                                     (double)VIEW(8), (double)VIEW(9),
                                     (double)VIEW(10), (double)VIEW(11),
                                     (double)DAT_106e9a68[0], (double)DAT_106e9a68[1],
                                     (double)DAT_106e9a68[2], (double)DAT_106e9a68[3],
                                     (double)OUTM(0), (double)OUTM(1),
                                     (double)OUTM(2), (double)OUTM(3),
                                     (double)OUTM(4), (double)OUTM(5),
                                     (double)OUTM(6), (double)OUTM(7),
                                     (double)OUTM(8), (double)OUTM(9),
                                     (double)OUTM(10), (double)OUTM(11),
                                     (double)OUTM(12), (double)OUTM(13),
                                     (double)OUTM(14), (double)OUTM(15));
                        }
                        if (fMax > -fMin) {
                            scale = DAT_10077248 / fMax;
                        } else {
                            scale = DAT_1007724c / fMin;
                        }
                        OUTM(0) = scale * OUTM(0);
                        OUTM(1) = scale * OUTM(1);
                        OUTM(2) = scale * OUTM(2);
                        OUTM(3) = scale * OUTM(3);
                        OUTM(4) = scale * OUTM(4);
                        OUTM(5) = scale * OUTM(5);
                        OUTM(6) = scale * OUTM(6);
                        OUTM(7) = scale * OUTM(7);
                        OUTM(8) = scale * OUTM(8);
                        OUTM(9) = scale * OUTM(9);
                        OUTM(10) = scale * OUTM(10);
                        OUTM(11) = scale * OUTM(11);
                        OUTM(12) = scale * OUTM(12);
                        OUTM(13) = scale * OUTM(13);
                        OUTM(14) = scale * OUTM(14);
                        OUTM(15) = scale * OUTM(15);
                    }
                    BrGuMtxStore((float *)0x106e78f0, pM);
                    EMIT(0x39e0010, pM);
                    EMIT(0x3980010, pM + 4);
                    EMIT(0x39a0010, pM + 8);
                    EMIT(0x39c0010, pM + 0xc);
                    }
                } else {
                    float *pM = BrMtxPoolAlloc();
                    BrGuMtxStore(pObj, pM);
                    EMIT(0x1020040, pM);
                    bTexLoaded = 0;
                }
                if ((*(uint16_t *)(pObj + 0x13) & 0x4a4) != 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 0x400) != 0) {
                        if (DAT_106ed6ac == 0 || (*(uint16_t *)(pObj + 0x13) & 0x100) == 0) {
                            EMIT(0xbc00000a, 0);
                            EMIT(0xbc00040a, 0);
                        } else {
                            EMIT(0xbc00000a, DAT_106ecb40 >> 1 & 0x7f7f7f00);
                            EMIT(0xbc00040a, DAT_106ecb40 >> 1 & 0x7f7f7f00);
                        }
                        EMIT(0xbc00200a, DAT_106e79e0[*((uint8_t *)pObj + 0x4c) & 3]);
                        EMIT(0xbc00240a, DAT_106e79e0[*((uint8_t *)pObj + 0x4c) & 3]);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 4) != 0) {
                        EMIT(0xb6000000, 0x3000);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 0x20) != 0 && DAT_106ed6a8 != 0 &&
                        DAT_106ed6ac == 0 && DAT_106ed6b0 == 0 && DAT_106ed6b4 == 0) {
                        EMIT(0xb6000000, 0x10000);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 0x80) != 0 && DAT_100aa010 != 0) {
                        EMIT(0xb6000000, 0x200);
                    }
                }
                if (i > DAT_102e16a8 && i < DAT_102e0ca0) {
                    EMIT(0xbb001001, 0xffffffff);
                    EMIT(0xe8000000, 0);
                    EMIT(0x6000000, *(uint32_t *)(pObj + 0x11));
                    DAT_106e772c = DAT_106e772c + *(uint16_t *)(pObj + 0x14);
                    DAT_106e7734 = DAT_106e7734 + *(uint16_t *)((char *)pObj + 0x4e);
                    DAT_106e86a0 = DAT_106e86a0 + *(uint16_t *)((char *)pObj + 0x52);
                } else {
                    FUN_1000cba0(param_1, idx, DAT_10386ca8[idx], param_2, param_4);
                }
                if ((*(uint16_t *)(pObj + 0x13) & 0x4a4) != 0) {
                    if ((*(uint16_t *)(pObj + 0x13) & 0x80) != 0 && DAT_100aa010 != 0) {
                        EMIT(0xb7000000, 0x200);
                    }
                    if ((*((uint8_t *)pObj + 0x4d) & 4) != 0) {
                        EMIT(0xbc00000a, DAT_106ecb40);
                        EMIT(0xbc00040a, DAT_106ecb40);
                        EMIT(0xbc00200a, DAT_106e9a78);
                        EMIT(0xbc00240a, DAT_106e9a78);
                    }
                    if ((*(uint16_t *)(pObj + 0x13) & 4) != 0) {
                        EMIT(0xb7000000, ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000));
                    }
                    if (DAT_106ed6a8 != 0 && DAT_106ed6ac == 0 && DAT_106ed6b0 == 0 &&
                        DAT_106ed6b4 == 0 && (*(uint16_t *)(pObj + 0x13) & 0x20) != 0) {
                        EMIT(0xb7000000, 0x10000);
                    }
                }
            }
            i = i + 1;
        } while (i < DAT_1035fb8c);
    }
    if (param_2 != 0) {
        pT = DAT_1035f7d8;
        EMIT(0x6000000, pT);
        TEMIT(0x1060040, DAT_100a9ec0);
        TEMIT((DAT_1184c470 & 0xffffff) | 0xdc000000, 1);
        if (DAT_106ed6ac == 0 && DAT_106ed6b4 == 0) {
            TEMIT(0xb900031d, 0x504b50);
        } else {
            TEMIT(0xb900031d, 0x504f50);
        }
        TEMIT(0xb900031d, 1);
        TEMIT(0xb9000002, 1);
        TEMIT(0xf9000000, 8);
        {
        uint32_t *q_ = pT; pT += 2;
        BrRdpSetCombineLERP(q_, 0x3ed, 0, 0x3f4, 0,
                                0, 0, 0, 0x3e9,
                                0x3ed, 0, 0x3f4, 0,
                                0, 0, 0, 0x3e9);
        }
        iCar = 0;
        if (0 < DAT_100b2f04) {
            int negCar0;
            pCar = (int *)(param_4 + 0x29e0);
            negCar0 = -(param_4 + 0x29e0);
            do {
                if (DAT_10396eb0 != 0 && pCar[-1] != 0 && pCar[0] != 0 &&
                    pCar[1] != 0 && pCar[2] != 0) {
                    TEMIT((DAT_1184c470 & 0xffffff) | 0xdc000000, 1);
                    TEMIT(0x400107f, DAT_1035faec);
                    {
                    int *pW = pCar + 4;
                    pDst = (uint16_t *)4;
                    do {
                        BrCopy8Words(DAT_1035faec, pW);
                        pW = pW + 8;
                        DAT_1035faec[2] = DAT_1035faec[2] - DAT_10077218;
                        DAT_1035faec = DAT_1035faec + 8;
                        pDst = (uint16_t *)((int)pDst - 1);
                    } while (pDst != (uint16_t *)0);
                    }
                    TEMIT(0xb900031d, 1);
                    TEMIT(0xb1000103, 0x302);
                }
                if (DAT_10b71b00 != 0) {
                    iWheel = 0;
                    do {
                        int cls;
                        if (DAT_10226e80 == 2 || DAT_10226e80 == 3) {
                            if ((iWheel != 0 ||
                                 (pCar[-0xa1e] == 0 || *(int *)(pCar[-0xa1e] + 0x1b4) == 0)) &&
                                (iWheel != 1 ||
                                 (pCar[-0xa1c] == 0 || *(int *)(pCar[-0xa1c] + 0x1b4) == 0)) &&
                                (iWheel != 2 ||
                                 (pCar[-0xa1d] == 0 || *(int *)(pCar[-0xa1d] + 0x1b4) == 0)) &&
                                (iWheel != 3 ||
                                 (pCar[-0xa1b] == 0 || *(int *)(pCar[-0xa1b] + 0x1b4) == 0)))
                                goto no_mark;
                            cls = 1;
                        } else {
                            int e;
                            if ((iWheel == 0 && (e = pCar[-0xa1e]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 1 && (e = pCar[-0xa1c]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 2 && (e = pCar[-0xa1d]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0) ||
                                (iWheel == 3 && (e = pCar[-0xa1b]) != 0 &&
                                 *(char *)(e + 0x1a0) == '\x03' && *(int *)(e + 0x1b4) != 0)) {
                                cls = *((uint8_t *)pCar + -0x2673);
                            } else {
no_mark:
                                cls = 0;
                            }
                        }
                        if (cls != 0) {
                            float dx, dy, len, ox, oy, x1, x2, y1, y2, z;
                            int iw = DAT_100a5d98[iWheel];
                            int wb = iw * 0x40 + param_4 + negCar0;
                            BrWheelRec *pW = (BrWheelRec *)(wb + (int)pCar);
                            float *pP = (float *)(wb + (int)pCar + 0x70);
                            dx = pW->vx;
                            dy = pW->vy;
                            len = (float)sqrt(dx * dx + dy * dy);
                            x1 = pP[0] - (dx / len) * DAT_10077250;
                            x2 = (dx / len) * DAT_10077250 + pP[0];
                            y1 = pW->y - (dy / len) * DAT_10077250;
                            y2 = (dy / len) * DAT_10077250 + pW->y;
                            z = (pW->z - BrGroundProbeZ(pP)) -
                                DAT_10077254;
                            ring = iWheel + iCar * 4;
                            if (DAT_1035faf0[ring] != DAT_1035f750[ring]) {
                                int h1 = DAT_1035faf0[ring] - 1;
                                pDst = (uint16_t *)h1;
                                if (h1 < 0) {
                                    pDst = (uint16_t *)0x1f3;
                                }
                                if (pDst != (uint16_t *)DAT_1035f750[ring] &&
                                    (DAT_10273690[(int)pDst + ring * 500].flags & 0x8000000) ==
                                    0x8000000) {
                                    slot = (int)pDst - 1;
                                    if (slot < 0) {
                                        slot = 499;
                                    }
                                    if (slot != DAT_1035f750[ring] &&
                                        (DAT_10273690[slot + ring * 500].flags & 0x8000000) ==
                                        0x8000000) {
                                        float ex1 = DAT_10273690[slot + ring * 500].x1 - x1;
                                        float ey1 = DAT_10273690[slot + ring * 500].y1 - y1;
                                        float ex2 = DAT_10273690[slot + ring * 500].x2 - x2;
                                        float ey2 = DAT_10273690[slot + ring * 500].y2 - y2;
                                        /* Comma-in-if computes both sqlens before
                                         * either fcomp, so VC5 `fst`-homes them
                                         * (same dest-once class as in-place fchs).
                                         * `&&` of the raw products short-circuits
                                         * the second pair to fstp-st + recompute. */
                                        {
                                        float d1, d2;
                                        if (d1 = ex1 * ex1 + ey1 * ey1,
                                            d2 = ex2 * ex2 + ey2 * ey2,
                                            d1 < DAT_10077258 && d2 < DAT_10077258) {
                                            if (h1 < 0) {
                                                h1 = 499;
                                            }
                                            DAT_1035faf0[ring] = h1;
                                        }
                                        }
                                    }
                                }
                            }
                            head = DAT_1035faf0[ring];
                            DAT_10273690[head + ring * 500].x1 = x1;
                            DAT_10273690[head + ring * 500].x2 = x2;
                            DAT_10273690[head + ring * 500].y1 = y1;
                            DAT_10273690[head + ring * 500].y2 = y2;
                            DAT_10273690[head + ring * 500].z1 = z;
                            DAT_10273690[head + ring * 500].z2 = z;
                            DAT_10273690[head + ring * 500].flags =
                                *(uint16_t *)(pCar + -0x24) | 0x8000000;
                            head = head + 1;
                            if (head >= 500) {
                                head = 0;
                            }
                            DAT_1035faf0[ring] = head;
                            if (head == DAT_1035f750[ring]) {
                                head = DAT_1035f750[ring] + 1;
                                if (head >= 500) {
                                    head = 0;
                                }
                                DAT_1035f750[ring] = head;
                            }
                        } else {
                            ring = iWheel + iCar * 4;
                            head = DAT_1035faf0[ring];
                            if (DAT_1035f750[ring] != head) {
                                slot = head - 1;
                                if (slot < 0) {
                                    slot = 499;
                                }
                                if ((DAT_10273690[slot + ring * 500].flags & 0x8000000) ==
                                    0x8000000) {
                                    DAT_10273690[ring * 500 + head].flags =
                                        DAT_10273690[ring * 500 + head].flags & 0xf7ffffff;
                                    head = head + 1;
                                    if (head >= 500) {
                                        head = 0;
                                    }
                                    DAT_1035faf0[ring] = head;
                                    if (head == DAT_1035f750[ring]) {
                                        head = DAT_1035f750[ring] + 1;
                                        if (head >= 500) {
                                            head = 0;
                                        }
                                        DAT_1035f750[ring] = head;
                                    }
                                }
                            }
                        }
                        iWheel = iWheel + 1;
                    } while (iWheel < 4);
                }
                iCar = iCar + 1;
                pCar = pCar + 0xada;
            } while (iCar < DAT_100b2f04);
        }
        if (DAT_10b71b00 != 0) {
            int again;
            TEMIT(0xb6000000, 0x3000);
            TEMIT((DAT_118ec98c & 0xffffff) | 0xdc000000, 1);
            {
            int c2 = 0;
            if (0 < DAT_100b2f04) {
                uint8_t *pA = active;
                int rb;
                do {
                    iWheel = 0;
                    rb = c2 << 4;
                    do {
                        head = *(int *)((char *)DAT_1035faf0 + rb);
                        slot = head - 1;
                        if (slot < 0) {
                            slot = 499;
                        }
                        *(int *)((char *)cursor + rb) = slot;
                        pA[iWheel] = head != *(int *)((char *)DAT_1035f750 + rb);
                        iWheel = iWheel + 1;
                        rb = rb + 4;
                    } while (iWheel < 4);
                    c2 = c2 + 1;
                    pA = pA + 4;
                } while (c2 < DAT_100b2f04);
            }
            }
            do {
                int dCar, dw, dring;
                again = 0;
                dCar = 0;
                if (0 < DAT_100b2f04) {
                    int rowBase = 0;
                    uint8_t *pA = active;
                    do {
                        int row = rowBase;
                        dring = dCar << 4;
                        dw = 0;
                        do {
                            if (pA[dw] != '\0') {
                                int dh, ds;
                                dh = *(int *)((char *)cursor + dring);
                                if (dh == *(int *)((char *)DAT_1035f750 + dring)) goto dead;
                                {
                                    ds = dh - 1;
                                    if (ds < 0) {
                                        ds = 0x1f3;
                                    }
                                    {
                                    uint32_t fl = DAT_10273690[dh + row].flags & 0x8000000;
                                    if (fl == 0x8000000) {
                                        if (ds == *(int *)((char *)DAT_1035f750 + dring)) goto dead;
                                        if ((DAT_10273690[ds + row].flags & 0x8000000) ==
                                            0x8000000 &&
                                            ((DAT_10386ca8[DAT_10273690[dh + row].flags &
                                                           0xf7ffffff] & 0x80) == 0 ||
                                             (DAT_10386ca8[DAT_10273690[ds + row].flags &
                                                           0xf7ffffff] & 0x80) == 0)) {
                                            if (DAT_1035f7dc + 0x20 >=
                                                (uint32_t *)(DAT_102e16ac + 0x3e800))
                                                goto full;
                                            TEMIT(0x400107f, DAT_1035f7dc);
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[ds + row].x2;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[ds + row].y2;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[ds + row].z2;
                                            DAT_1035f7dc[3] = 0;
                                            DAT_1035f7dc[4] = 0;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[dh + row].x2;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[dh + row].y2;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[dh + row].z2;
                                            DAT_1035f7dc[3] = 0x44800000;
                                            DAT_1035f7dc[4] = 0;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[ds + row].x1;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[ds + row].y1;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[ds + row].z1;
                                            DAT_1035f7dc[3] = 0;
                                            DAT_1035f7dc[4] = 0x44800000;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            DAT_1035f7dc[0] =
                                                *(uint32_t *)&DAT_10273690[dh + row].x1;
                                            DAT_1035f7dc[1] =
                                                *(uint32_t *)&DAT_10273690[dh + row].y1;
                                            DAT_1035f7dc[2] =
                                                *(uint32_t *)&DAT_10273690[dh + row].z1;
                                            DAT_1035f7dc[3] = 0x44800000;
                                            DAT_1035f7dc[4] = 0x44800000;
                                            DAT_1035f7dc = DAT_1035f7dc + 8;
                                            TEMIT(0xb1000103, 0x302);
                                        }
                                        again = 1;
                                        *(int *)((char *)cursor + dring) = ds;
                                    } else if (fl == 0) {
                                        again = 1;
                                        *(int *)((char *)cursor + dring) = ds;
                                    }
                                    }
                                }
                                goto next;
dead:
                                pA[dw] = 0;
next:;
                            }
                            dw = dw + 1;
                            row = row + 500;
                            dring = dring + 4;
                        } while (dw < 4);
                        dCar = dCar + 1;
                        rowBase = rowBase + 2000;
                        pA = pA + 4;
                    } while (dCar < DAT_100b2f04);
                }
            } while (again);
full:
            TEMIT(0xb7000000,
                  ((DAT_106ea3f4 ^ DAT_106e8204) ? 0x1000 : 0x2000));
        }
        TEMIT(0xf9000000, 0);
        TEMIT(0xb900031d, 0);
        TEMIT(0xba001402, 0x100000);
        TEMIT(0xb900031d, DAT_1035fb88 | DAT_1035fb84);
        {
        uint32_t *q_ = pT; pT += 2;
        BrRdpSetCombineLERP(q_, 0x3ea, 0x3e9, 0x3f5, 0x3e9,
                                0x3ea, 0x3e9, 0x3f5, 0x3e9,
                                0x3e8, 0, 0x3ec, 0,
                                0, 0, 0, 0x3e8);
        }
        TEMIT(0xba000602, DAT_106e7718);
        TEMIT(0xbd000000, 0);
        pT[0] = 0xb8000000;
        pT[1] = 0;
        pT = pT + 4;
        DAT_1035f7d8 = pT;
    }
    if (param_2 == 0) {
        if (DAT_102e0ca0 == 0x1000) {
            DAT_102e0ca0 = cHead;
        }
        DAT_102e0ca0 = (cHead + base) - DAT_102e0ca0;
    }
    EMIT(0x1020040, DAT_100a9ec0);
    EMIT(0xb6000000, 0x10000);
    EMIT(0xe7000000, 0);
    BrPodNop(0, 0, 0xb4, 0, 0xff);
    DAT_1035fb74 = cHead;
}

#endif /* BR_MATCHING_BUILD */
