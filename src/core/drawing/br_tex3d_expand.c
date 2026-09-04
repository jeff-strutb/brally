/* 0x100250D0 BrTex3dExpand — matching transcription from Ghidra decomp.
 *
 * ‼ 2026-09-03 (session 16): THE 0x15b8..0x19fd STRETCH IS NOW MEASURED, not
 * just read.  Session 14 read it by eye and called it T3a; that verdict was
 * right but it never said how much is there, so every later ranking had to
 * treat 12.9% of the function as an unknown.  A WINDOWED register-blind
 * multiset (msetdiff's norm over orig 0x15b8..0x19fd against recomp
 * 0x15b4..0x19ce -- divergence.py cannot reach it because the block holds no
 * ten consecutive matching instructions) gives the honest account:
 *     274 orig insns vs 270 ours -- FOUR short, i.e. the whole -47 byte
 *     delta the tail carries is born here and nowhere else.
 *     MISSING 11: `mov R,[esp+S]` x7, `cmp R,R` x2, `xor R,R`,
 *                 `imul R,[esp+S]`
 *     EXTRA    7: `mov [esp+S],R` x2, `mov [esp+S],0`, `test R,R`,
 *                 `test [esp+S],R`, `imul R,R`, `jmp T`
 *   TEN of those eleven rows are exactly session 14's counter-register swap
 *   stated as numbers -- orig reloads counters and bounds from slots and
 *   multiplies from memory, we keep them in registers and home more.  That
 *   half is allocation and stays parked.
 *   ‼ THE ELEVENTH IS NOT: we emit an EXTRA `jmp` at 0x1909, and it is a
 *   LOOP-ROTATION ARTEFACT.  Our inner loop's back-edge target is a reload
 *   (`0x190b mov edx,[esp+0x68]`) one instruction above the real head at
 *   0x190f, so VC5 jumps past it on entry; the original's head at 0x192f is
 *   fallen into.  The reload exists because our first channel's delta stays
 *   in edx for FOURTEEN instructions before it is homed (computed 0x18c2,
 *   stored 0x18fb) while the original homes each channel's coefficient and
 *   delta together, four instructions after computing them (0x18e1/0x18ec).
 *   The cause is the SETUP BLOCK's interleaving: ours issues three channel
 *   loads up front ([0xc0],[0xb0],[0xc4]) and finishes the channels out of
 *   order; the original does one channel at a time.  Session 15's (b) and
 *   (c) both moved coefficient pairs and were rejected on the multiset --
 *   NEITHER tried making each channel's pair a self-contained statement pair
 *   so the loads cannot interleave.  It is the only non-allocation row in
 *   12.9% of the function.
 *   ‼ AND THE OBVIOUS SPELLING OF IT IS DEAD, do not re-run: sinking the R
 *   pair (`lo0` / `iVar16`) below the intensity statements so all four
 *   channels read pair-then-channel is BYTE-IDENTICAL -- same 61 regions at
 *   key 6, same 2,408 insns / 8,461 bytes, same 1,093-byte gap, REGNORM
 *   unchanged, and the `jmp 0x1909` still there with the same reload behind
 *   it.  ‼ IDIOM: the four pairs are LOOP-INVARIANT and VC5 hoists them, so
 *   their position INSIDE the loop body is inert -- only their order
 *   relative to each other reaches the hoisted block.  Any further attempt
 *   here has to change what is invariant, not where it is written.
 *   ‼ AND THE HEADER'S "REGNORM 41+40" IS STALE: re-measured this session it
 *   is 40+41.  Re-measure before comparing to any figure below.
 *
 * 2026-09-03 (session 15): the SIBLING-ASYMMETRY SCREEN run over this
 * function -- twelve near-identical channel arms is the ideal substrate for
 * it -- and all three leads it produced are DEAD.  Recorded so the screen
 * does not re-fire on them:
 *   (a) ONE of the three IA blend bodies spelled its channel product
 *       intensity-first (`uVar14 * ((param_14 & 0xff) - (param_18 & 0xff))`)
 *       where the other two spell it delta-first.  Making all three match is
 *       BYTE-IDENTICAL, so it is APPLIED -- not because it gains anything,
 *       but so the next screen does not stop here.  ‼ This EXTENDS the
 *       session-10 `*`-canonicalisation entry to a case it did not cover:
 *       that one commuted two named VARIABLES, this one commutes a variable
 *       against an inline SUBEXPRESSION, and VC5 canonicalises that too.
 *       Unlike the x87 operand-KIND ranking proved on 0x1000EAF0 the same
 *       day, integer multiply operand order carries no information at all.
 *   (b) alpha's COEFFICIENT PAIR hoisted above the chB statement at the
 *       first blend body -- the untried counterpart of the session-10 probe,
 *       which hoisted alpha's PRODUCT and was sunk straight back.  This one
 *       is not inert but it is not a win: bytes 19 short -> 17 and raw
 *       276+275 -> 275+274, against a register-blind multiset 41+40 -> 42+41.
 *       Rejected on the primary metric.
 *   (c) ALL FOUR coefficient pairs hoisted to the top of the body, i.e. full
 *       symmetry with the R channel (whose pair already sits there).  Clearly
 *       worse: 45+41, +10 bytes, +4 instructions.  The interleaving of the
 *       G/B/A pairs with their channels is the original's shape and the R
 *       channel is the one that is genuinely different.  Do not "tidy" it.
 *
 * 2026-09-03 (session 14): the 0x15b8..0x19fd stretch is now READ, both
 * streams side by side, and the session-13 diagnosis needs one correction.
 * The counter-register swap is real, but the compare that goes with it is
 * NOT a source lever:
 *     orig  15d5 `xor eax,eax` (inner = 0), 15e7 homes it, then
 *           15f1 `cmp ebx,eax` -- the bound against the just-zeroed counter
 *     ours  15da `xor ecx,ecx`, 15e7 homes it, then
 *           15f1 `test ebx,ebx` -- the zero constant-folded away
 *   PROBED AND INERT, do not re-run: spelling that guard as
 *   `if ((int)param_1 < iVar17)` instead of `if (0 < iVar17)` at line 1035,
 *   which is literally what the original's `cmp` reads.  BYTE-IDENTICAL --
 *   VC5 constant-propagates the counter's zero into the guard and emits
 *   `test` either way, so orig's `cmp` is a peephole consequence of having
 *   the zero already in a register for the home, not evidence about the
 *   source.  The only real difference across the whole preheader is WHICH
 *   counter got the register: orig has the outer row counter in ecx and
 *   reloads the inner one and its bound from slots at the 4-step loop head
 *   (0x1612/0x1616); we keep the inner one in ecx and the bound in ebx and
 *   need no reloads.  With session 13's probes (a) and (b) both dead, that
 *   stretch is T3a like the rest of this function.
 * Same session, REGION 5 (0x9f8, +58 and the largest RELIABLE change on the
 * key-10 map) read for the first time -- it had only ever been a delta.  It
 * is ONE extra instruction and one schedule notch, not a block:
 *     ours hoists `mov dword ptr [esp+0x20],0` -- the 4-step counter's
 *     init -- to the ROW LOOP HEAD at 0x9f8, where the original does not
 *     have it at all; and the loop guard loads its two operands the other
 *     way round (orig loads the BOUND then the counter and emits
 *     `cmp counter,bound`; we load the counter then the bound).
 *   So the +58 is drift the block hands back, not 58 bytes of missing code,
 *   which is consistent with this function's instruction parity.  Nobody
 *   should open r5 expecting a block.
 *
 * ‼‼ 2026-09-03 (session 13) -- EVERY REGION MAP BELOW IS TRUNCATED.  Read
 * this before any other note in this file.
 *   `divergence.py` used to STOP at the first divergence it could not
 * resync within 400 instructions and print a region total anyway.  On this
 * function it lost sync at orig+0x15b8 EVERY TIME, at every key tried (6,
 * 8, 10, 12, 14) -- so "32 masked regions", "20 regions at key 10", "r15
 * +36 and r18 -33 are the largest", "no block carries more than ~36 bytes"
 * and the whole session-8..12 ranking were all measured on orig
 * 0x0..0x15b8 and NEVER SAW the last 2,920 bytes, 34% of the function.
 *   The tool now re-anchors globally after a lost sync and prints the
 * uncompared byte count (commit b72676b).  The honest map:
 *     key 10:  31 regions (was 20), 1,093 bytes (12.9%) still uncompared
 *     key  6:  52 regions (was 32),   243 bytes  (2.9%) still uncompared
 *   The tail's own deltas, restarted at 0x1800, are LARGER than anything
 * the prefix has: r2 (0x1a4c) -50, r4 (0x1c2c) -16, r6 (0x1ebc) +25, and
 * seven zero-delta regions from 0x1ebc to 0x20a2 that are pure register
 * choice around the FUN_100271f0 call (`mov dx`/`push edx` against our
 * `mov cx`/`push ecx`).  So the -50 at 0x1a4c, not r15 or r3, is this
 * function's largest single reliable block.
 *   ‼ THE ONE STRETCH NOBODY HAS EVER READ is orig 0x15b8..0x19fd.  It is
 * uncompared precisely because it holds no ten consecutive matching
 * instructions -- that is a diagnosis, not a tooling gap.  /FAcs puts it at
 * source lines 962-1056: the `param_6 == 3 && param_13 == one` IA blend
 * arm.  Read by hand, the defect there is a COUNTER-REGISTER SWAP:
 *     orig  keeps the OUTER row counter in ecx for the whole row loop
 *           (`xor ecx,ecx` at 0x15b8, `test ecx,edx` for the mask test,
 *           `inc ecx; cmp ecx,eax; mov [esp+0x34],ecx; jl 0x15ce` at
 *           0x1a4c) and homes the INNER column counter in slot 0x7c,
 *           reloading it (`mov eax,[esp+0x7c]` at 0x1612)
 *     ours  does the exact opposite -- inner counter in ecx, outer counter
 *           memory-only (`mov [esp+0x1c],0`, `test [esp+0x1c],edx`)
 *   SESSION 13 PROBES ON THAT BLOCK, BOTH DEAD, do not re-run:
 *     (a) the inner counter respelled as a fresh `int iCol` local instead
 *         of the reused `param_1` pointer parameter with its casts (the
 *         obvious Ghidra-ism to retranscribe): REGNORM 41+40 unchanged,
 *         bytes -19 unchanged, insns +1 unchanged, DIFFS 6368 -> 6370
 *         (slot renumbering only).  ‼ IDIOM, worth more than the probe: a
 *         Ghidra "reused parameter as loop counter" is CODEGEN-NEUTRAL
 *         against a fresh local of the same width.  Do not retranscribe
 *         one hoping to move an allocation, here or anywhere.
 *     (b) swapping the two counters' ROLES so the outer one is `param_1`
 *         and the inner a fresh local -- which is how the OTHER arms in
 *         this function are already written (lines 294-299, 1085-1089), so
 *         it was the principled guess: REGNORM 41+40 -> 40+39 (one row
 *         better each side) but bytes -19 -> -25 and RAW 276+275 ->
 *         280+279, with the region count and the lost-sync gap both
 *         unchanged at 31 / 0x15b8.  It shuffles the allocation without
 *         fixing the block; net worse on size.  Rejected.
 *     (c) `iVar5` split in two.  `tools/slotcensus.py` shows the original
 *         writes [esp+0x30] ONCE (at 0x4b, `lea edx,[eax+ebx]` = the tile
 *         record `iVar10 * 0x40 + param_11`) and reads it EIGHT times as
 *         `[eax+8]`, while our build writes its equivalent slot four
 *         times -- because Ghidra recycled `iVar5` as the scratch counter
 *         of all five copy-back loops (lines 567, 754, 987, 1109, 1303) on
 *         top of its job as that record pointer.  Giving the scratch its
 *         own `iSpan` at ALL FIVE sites at once is BYTE-IDENTICAL: VC5
 *         already splits the reused local into independent webs.  The
 *         recycling is still worth undoing for readability if this file is
 *         ever cleaned up, but it is not a matching lever.
 *
 * The insn-3 "coloring wall" is BROKEN (for-loop with a raw-parameter bound);
 * the store-idiom wall is BROKEN too (Ghidra folded orig's two separate
 * `count += 2` updates into one `+= 4` with a `count + 2 >= cbMax` guard --
 * the real source bumps the counter BEFORE each store, advances pOut by ONE
 * element per store, and puts each budget check on its own control edge).
 * State, do-not-re-run probe lists and the open levers: docs/idioms-A.md.
 * 2026-09-01: the IDX4 arm's width is the reused `param_9` (orig homes it in
 * the dead param_9 arg slot [esp+0x9c] and reloads every bound from there);
 * that closed the five 0xae-0x1ee regions.  The same rename on the CI8 arm
 * breaks the frame -- measured, do not apply there.  The copy-back
 * preamble's doubling ternary `(param_7 != 0) ? w * 2 : w` (idioms-A.md)
 * had regressed to if-form at all 18 sites; restoring it at the IDX4 pair
 * alone closes the whole IDX4 tail (0x206-0x343).  Restoring ANY second
 * pair flips the global allocation (+28 insns, tile pointer ebx->ebp):
 * measured per pair and in five combinations, do not re-run.
 * 2026-09-03 (session 5, no movement -- 32 masked / 45 raw): five more
 * negatives, recorded in docs/idioms-A.md "Session 5".  Headline: the old
 * "next concrete lever" (a widened `uVar19 = bI4inten` temp in ONE blend
 * body) now BREAKS the frame -- first divergence collapses +0x2b -> +0x0;
 * do not restore it.  Commutative operand order (`&` in the mask tests,
 * `|` in the nibble merges) is canonicalised by VC5 and byte-neutral at
 * every site.  Map a region address to its source line with one `/FAcs`
 * compile before probing anything here.
 * 2026-09-03 (session 6): the three channel-pack sites now advance the
 * output pointer BEFORE the store and write through `puVar21[-1]`, which
 * is how orig spells them (`mov word ptr [esi-2],dx` with the `add esi,2`
 * already retired).  Pre-biased word stores 9 -> 10 of orig's 12, insns
 * 2415 -> 2413 (orig 2407), raw regions 45 -> 44; masked regions still 32,
 * so this is a shape gain, not a closure.  One site alone does nothing --
 * all three together, or not at all.
 * 2026-09-03 (session 7): the OTHER half of each channel-pack pair is now
 * pre-biased too, so all 12 of orig's word stores are `mov word ptr
 * [esi-2],W` and the store family is GONE from the register-blind multiset.
 * Insns 2413 -> 2411 (orig 2407), regions still 32 masked / 44 raw -- again
 * a shape gain, not a closure.  Session 6 had converted only the first site
 * of each pair; the second sites are the three `*puVar21 = (chA<<5|chR)<<5
 * ...` at the ends of the IA4/IA8 blend bodies, and they behave the same
 * way (all three together).  Found with tools/msetdiff.py, which had to be
 * fixed first (see its header): the branch-target and reloc-addend noise
 * was hiding the two-store residue.
 * ‼ Every instruction count above (2415 / 2413 / 2411 against 2407) counted
 * this TU's 16-byte alignment padding as code; `divergence.py` is fixed
 * (commit a00add5) and the honest figure is 2,408 vs the original's 2,407,
 * i.e. ONE instruction over, not four.  This function is at instruction
 * parity and its residue is genuinely shape, exactly as the file title
 * says -- unlike its two sibling giants, whose "counts are equal" claims
 * were padding artefacts hiding 6 and 15 missing instructions.
 * Same session, the nibble-merge question is now SETTLED.  The clean
 * multiset leaves exactly one 32-bit nibble merge (`and edx,0xf0; and
 * ecx,0xff; shr ecx,4` at 0xda6) against the original's all-8-bit
 * `and dl,0xf0; shr cl,4; or cl,dl`; the other two blend bodies already
 * emit `and al,240` and are right.  Map it with `/FAcs`: the odd one out is
 * the THIRD blend body, the only one that routes the intensity through the
 * widened `uVar19 = (unsigned int)bI4inten` temp -- and that temp is
 * REQUIRED, which is the other face of the session-5 note.  Two probes,
 * both dead, do not re-run:
 *   (a) spelling the two merges in that body through plain uint8_t
 *       intermediates (`b8 = bVar11; b8 &= 0xf0; bI4inten = bVar11;
 *       bI4inten >>= 4; bI4inten |= b8;`, and the `<<4 | &0xf` twin the
 *       same way) is BYTE-IDENTICAL at the site -- VC5 canonicalises it
 *       straight back to the dword form.  32 masked / 44 raw, insns
 *       2411 -> 2410, and the multiset is unchanged row for row.
 *   (b) dropping `uVar19` from that body so it reads like bodies 1 and 2
 *       (inline `(unsigned int)bI4inten` in all four channels) costs
 *       +15 insns and 32 -> 39 masked regions.  Its raw count falls 44 ->
 *       12, which is again the alignment artefact, not progress.
 * So the byte-vs-dword merge is downstream of the temp, not a separate
 * lever: it cannot be fixed at this site without giving the temp up.
 * 2026-09-03 (session 8): ‼ FOUR OF THE 32 REGIONS ARE NOT REAL.  Run
 * `divergence.py --mask-slots --deltas` and read the SUSPECT markers before
 * anything else here.  The aligner resyncs on six consecutive matching
 * instructions, and in this function -- twelve near-identical arms, every
 * one of them ending in the same `shr edx,0x1f; add eax,edx` divide-by-255
 * fixup -- that key is not unique.  Four resyncs lock onto the wrong copy
 * (skews 74/53, 12/34, 26/135, 115/20), and the drift they report is
 * fiction: the headline "+393 bytes in one block" at region 9, which would
 * otherwise look like this function's dominant defect, is an artefact that
 * regions 10 and 11 hand straight back (-87, -327).  Regions 3, 5, 9 and 11
 * and every delta between them are unreliable.  The honest picture from the
 * clean stretch is that NO block carries more than ~36 bytes: r15 +36 and
 * r18 -33 are the largest, everything else is under 15.  This is consistent
 * with the instruction parity above -- the residue really is shape, spread
 * thin, with no missing block to find.
 * Session 8 probe, DEAD, do not re-run: r15 is the original homing a
 * channel product in the reused param_9 arg slot (`mov [esp+0x9c],edx;
 * imul edx; add edx,[esp+0x9c]`) where we keep it in a register.  Spelling
 * that channel through `param_9` (the established reuse idiom in this TU)
 * changes nothing measurable -- 32 masked / 44 raw / 2,408 insns and an
 * identical delta map.
 * 2026-09-03 (session 9): the TU now carries its `@implements` tag, so it
 * has a `report.csv` row and `tools/fnmatch/fn.py 0x100250D0` scores it in
 * ~6 s.  Until now this function had NO row and NO tag: the sweep compiled
 * nothing for it and every measurement had to be a hand-rolled `cl.exe`
 * line, which is why the state notes above drift in what they quote.  The
 * tag is a work marker, not a claim -- the row reads `diff` and MATCH is
 * unchanged at 820/1148.  Re-measured on it: 8,461 vs 8,480 bytes, 2,408 vs
 * 2,407 instructions, 32 masked / 44 raw regions -- session 8's state
 * exactly.
 * Session 9 finding, so nobody grinds the wrong block: REGION 18's -33 IS
 * NOT A DEFECT.  It is the +38 that regions 14-17 (0xe96-0xf43) accumulated
 * being handed back; region 18 itself is a one-instruction scheduling notch
 * -- `inc edi` emitted right after `mov al,[ecx]` where the original sinks
 * it four instructions later, everything else in the block identical.  With
 * regions 3/5/9/11 unreliable (session 8) and r15 probed dead, the whole
 * reliable defect mass is r14-r17.
 * Session 9 re-framing of the register-blind residue (40 missing / 41 extra,
 * `tools/msetdiff.py`): it is ONE cause, not the six families the old
 * "Remaining shape gap" table in docs/idioms-A.md lists.  The original keeps
 * these values in registers and folds their memory reads into the
 * arithmetic -- `imul R,[esp+S]` x3, `add R,[esp+S]`, `lea R,[R+R]` x6,
 * reg-reg `cmp` x4 / `mov` x5 / `xor` x3 / `inc` x3 / `sub` x2 -- while we
 * spill and reload around them: `mov R,[esp+S]` x8, `mov [esp+S],R` x7,
 * `mov [esp+S],0` x3, `imul R,R` x3.  The byte-vs-dword nibble rows and the
 * `movzx W,byte [esp+S]` / `mov byte [esp+S],B` rows are the same story in
 * the byte slots.  So do not open these as separate levers; they move
 * together or not at all, and the open mechanism is the one the last line
 * of docs/idioms-A.md names (orig homes the widened inten BEFORE its first
 * product, we multiply from the register and home after).
 * 2026-09-03 (session 11): ‼ READ THIS FUNCTION AT `--key 10`.  THE REGION
 * MAP EVERY NOTE BELOW QUOTES IS WRONG.  `divergence.py` resyncs on N
 * consecutive matching instructions and N was 6, which is SHORTER than the
 * sequences this function repeats -- twelve near-identical channel arms, all
 * ending in the same divide-by-255 fixup.  The tool now takes `--key N` (and
 * prefers balanced splits within one displacement, which was a second source
 * of wrong locks).  Measured both ways on the same object:
 *     --key 6   32 regions, FOUR flagged SUSPECT (74/53, 12/34, 26/135,
 *               115/20) -- session 8 already knew these were fiction
 *     --key 10  20 regions, ONE SUSPECT (region 2, at 0x6ee)
 *     --key 14   4 regions -- too coarse, the resync starts swallowing arms
 * So "32 masked regions", repeated in every note above, is twelve counting
 * artefacts on top of twenty real ones.  The honest map, at key 10:
 *     r3  (0x846)  -73  <- THE dominant block, and NOT suspect any more
 *     r5  (0x9f8)  +58    r4 (0x966) +30    r10 (0xf4f) -23
 *     r11 (0x111f) +23    r9 (0xe96) +15    r6 (0xcda) -21   r19 -11
 *     everything else under 10
 * ‼ That RETIRES the session-8/9 ranking outright: "r15 +36 and r18 -33 are
 * the largest" was an artefact of the short key, and so was the framing that
 * "the reliable defect mass is r14-r17".
 * ‼ CORRECTION, SAME DAY (session 12), and it retracts the line this note
 * originally ended with ("grind r3 first"): r3's -73 IS ALSO FICTION.  r2 at
 * 0x6ee resyncs with a 74/53 skew at EVERY key tried (10 through 14), and a
 * bad resync corrupts the DELTA of the next region -- so r3's change, and
 * r4's after it, are differences taken from a wrong anchor.  `divergence.py`
 * now labels both ("change unreliable: a preceding resync was SUSPECT");
 * before it did not, which is how a -73 got believed and written down here.
 * PROOF that no block is missing, and the recipe worth reusing: count an
 * instruction that occurs once per unit of work across the WHOLE function in
 * both streams.  The divide-by-255 magic constant appears 33 times in the
 * original and 33 times here; the one-operand `imul` 24 and 24.  Every
 * channel divide is present, so the 21-instruction "gap" the 0x6ee-0x846
 * window reports cannot exist.  With the instruction total at 2,408 vs 2,407
 * that settles it: THE RESIDUE IS ALLOCATION, and the 0x6ee-0x846 stretch is
 * unreadable from the region map at any key.
 * The largest RELIABLE changes at key 10 are r5 (0x9f8) +58 and r6 (0xcda)
 * -21; everything else is under 25.
 * What the 0x6ee-0x846 window does show, read by hand rather than by delta:
 * the original homes the widened intensity in a slot and multiplies from it
 * at every channel (`imul R,[esp+0x38]`), while we keep it in a register and
 * home it one instruction later -- and the original is INCONSISTENT about it
 * within a single block (`imul ebx,[esp+0x38]` at 0x803 takes the register
 * form with no home, `imul edx,[esp+0x38]` at 0x850 homes and adds back from
 * memory).  By the session-11 triage rule that settles it as per-site
 * allocation, not a spelling.
 * 2026-09-03 (session 10): r14's +15 IS THE ALPHA CHANNEL'S PRODUCT TEMP,
 * and the mechanism is now fully read off the bytes.  `/FAcs` puts the block
 * at source lines 568-575, the THIRD blend body's first half.  All four
 * channels are `(intensity * delta) / 255 + base`, and MSVC's divide-by-255
 * is `imul <magic>` followed by `add edx,<the product again>` -- so the
 * product has to SURVIVE the `imul`, which clobbers edx:eax.  The original
 * and this build agree exactly on channels R, G and B: each copies the
 * intensity (`mov edx,ecx; imul edx,[delta]`), homes the product in a reused
 * param arg slot (0x9c, 0x9c, 0x7c -- ours picks the same three) and adds it
 * back from there.  They differ ONLY on alpha, and by one register choice:
 *   orig  0xe2c  `imul ecx,[esp+0x44]`  -- computes alpha's product ONE
 *                CHANNEL EARLY, in place, killing the intensity at its last
 *                use, so the product sits in ecx, `imul ecx` at 0xe57 leaves
 *                it there and `add edx,ecx` needs no slot at all
 *   ours  0xe51  `mov edx,[iVar20]; imul edx,ecx; mov [esp+0x68],edx; imul
 *                edx; mov eax,[esp+0x68]; add edx,eax`
 * That is three extra instructions and a FOURTH temp slot (0x68) the
 * original never spends.  Everything else in r14 is identical.
 * SESSION 10 PROBES, BOTH DEAD, do not re-run -- VC5 canonicalises harder
 * than this file's earlier notes assumed:
 *   (a) commuting the products in that body (`iVar16 * uVar19` etc., all
 *       eight sites in both halves) is BYTE-IDENTICAL.  Multiplication
 *       operand order does NOT pick the `imul` destination -- add `*` to the
 *       session-5 list of commutative operators VC5 canonicalises, which had
 *       only `&` and `|` on it.
 *   (b) hoisting alpha's product into a named temp above the chB statement
 *       (`uPrA = uVar19 * iVar20;`, with `uVar8`/`iVar20` moved up with it,
 *       i.e. literally the original's schedule written into the source) is
 *       BYTE-IDENTICAL too -- VC5 sinks the temp straight back to its use.
 * So the alpha slot is not reachable by scheduling the source; what decides
 * it is that the original lets the intensity DIE into alpha's product and we
 * keep it live.  Treat r14 as T3a unless a spelling is found that ends the
 * intensity's live range at that multiply.
 * MEASURED NEGATIVE, do not re-run: converting ALL TEN remaining
 * `x = w*2; if (param_7 == 0) x = w;` doubling sites to the ternary form
 * at once.  Session 4 measured this per pair and in eight combinations;
 * the all-at-once case is now measured too and is WORSE, not better --
 * it breaks the byte-exact 0x2b-0x6ee prefix (new divergences at 0x91,
 * 0x132, 0x188, 0x1e6, ...) and costs +25 insns.  Its raw-region count
 * falls 45 -> 25, which is an ALIGNMENT ARTEFACT of the broken prefix,
 * not progress -- read masked regions and the first-divergence address
 * together before believing a raw-count drop.  (The "apply a spelling to
 * every sibling site before judging it" lesson that gained a region in
 * br_scenedl.c does NOT generalise here: there the siblings shared one
 * induction variable strategy, here each pair is independently allocated.)
 * Same session, second negative: converting ALL SEVEN remaining row loops
 * from `ctr = 0; if (0 < w) { do {...} while (ctr < w); }` to the for-init
 * form `for (ctr = 0; ctr < w;) {...}` at once is BYTE-IDENTICAL -- 32
 * masked / 44 raw / 2413 insns, unchanged.  (Two arms already use the
 * for-init form; the other seven now measured, together, as a set.)  So
 * the session-4 note that this form "closed the CI4 arm" describes a
 * different lever than the loop shape itself; VC5 canonicalises the
 * guarded-do-while and the for-init spellings of a counted row loop.
 */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

unsigned int FUN_100271f0(unsigned short);

/* WHAT IT DOES: expand one of the N64 texture formats into the 16-bit pixels
 * the 3dfx card wants -- walks the source in its own bit layout and writes
 * out a plain texture. The game ships N64-format art, so nothing can be
 * drawn until this has run over it. */
/* @implements 0x100250D0 glide BrTex3dExpand */
void BrTex3dExpand(unsigned short *param_1,int param_2,int param_3,unsigned char *param_4,int param_5,int param_6,
                 int param_7,int param_8,int param_9,int param_10,int param_11,unsigned char param_12,
                 int param_13,unsigned char param_14,unsigned char param_15,unsigned char param_16,unsigned char param_17,unsigned char param_18,
                 unsigned char param_19,unsigned char param_20,unsigned char param_21,int param_22)

{
  unsigned short *puVar2;
  int iVar3;
  unsigned short uVar4;
  int iVar5;
  unsigned int uVar6;
  unsigned int uVar7;
  unsigned int uVar8;
  unsigned short *puVar9;
  int iVar10;
  unsigned char bVar11;
  unsigned char bCI4a, bCI4b, bCI4c;
  unsigned char bIA8a, bIA8b, bIA8c;
  unsigned char bI4inten;
  unsigned char chA, chR, chG, chB;
  unsigned short pal;
  int lo0;
  int loIA8;
  unsigned char *pbVar12;
  int iVar13;
  unsigned int uVar14;
  int iVar15;
  int iVar16;
  int iVar17;
  int iVar18;
  unsigned int uVar19;
  int iVar20;
  unsigned short *puVar21;
  int iVar22;
  unsigned char *local_64;
  int local_54;
  unsigned int local_44;
  unsigned int local_3c;
  unsigned int local_38;
  int local_34;
  int local_24;
  int cbMax;
  int one;
  
  iVar3 = (int)param_4;
  iVar22 = 0;
  iVar10 = param_9;
  if (param_9 >= param_10) {
    return;
  }
  cbMax = param_2;
  puVar21 = param_1;
  for (; iVar10 < param_10; iVar10 = iVar10 + 1) {
    iVar5 = iVar10 * 0x40 + param_11;
    param_4 = (unsigned char *)(iVar3 + *(int *)(iVar10 * 0x40 + 0xc + param_11) * 8);
    if (param_3 == 0) {
      if (param_6 == 2) {
        if (((param_12 & 2) != 0) && (iVar10 == 1)) {
          param_9 = 1 << (*(int *)(param_11 + 0x60) - 1);
          param_1 = (unsigned short *)0x0;
          iVar15 = 1 << *(int *)(param_11 + 0x64);
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if (((unsigned int)param_1 & param_22) != 0) {
                iVar16 = 0;
                if (0 < param_9) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                      if (iVar16 >= param_9) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + -8;
                    for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                      if (iVar16 >= param_9) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 >> 4);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 2;
                      *puVar21 = (unsigned short)(bVar11 & 0xf);
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < param_9);
                }
              }
              else {
                iVar16 = 0;
                if (0 < param_9) {
                  do {
                    bVar11 = *pbVar12;
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(bVar11 >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 2;
                    *puVar21 = (unsigned short)(bVar11 & 0xf);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  } while (iVar16 < param_9);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < param_9)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < param_9);
              }
              param_4 = param_4 + *(int *)(param_11 + 0x48);
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
              puVar9 = puVar9 + iVar16 * -2;
              puVar2 = puVar9;
              iVar16 = (param_7 != 0) ? param_9 * 2 : param_9;
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
                } else {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_44 = 0;
          if (0 < iVar15) {
            do {
              param_9 = (int)param_4;
              if ((local_44 & param_22) != 0) {
                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                    param_9 = param_9 + 4;
                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                      if ((int)param_1 >= iVar17) break;
                      bCI4b = *(unsigned char *)param_9;
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4b >> 4) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4b & 0xf) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      param_9 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                    }
                    param_9 = param_9 + -8;
                    for (local_38 = 0; (int)local_38 < 4; local_38 = local_38 + 1) {
                      if ((int)param_1 >= iVar17) break;
                      bCI4c = *(unsigned char *)param_9;
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)(bCI4c >> 4) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (bCI4c & 0xf) * 2));
                      *puVar21 = uVar4;
                      iVar22 = iVar22 + 2;
                      puVar21 = puVar21 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      param_9 = param_9 + 1;
                      param_1 = (unsigned short *)((int)param_1 + 1);
                    }
                    param_9 = param_9 + 4;
                }
              }
              else {
                for (param_1 = (unsigned short *)0x0; (int)param_1 < iVar17;) {
                  bCI4a = *(unsigned char *)param_9;
                  pal = *(unsigned short *)(param_5 + (unsigned int)(bCI4a >> 4) * 2);
                  uVar4 = FUN_100271f0(pal);
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pal = *(unsigned short *)(param_5 + (bCI4a & 0xf) * 2);
                  uVar4 = FUN_100271f0(pal);
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  param_9 = param_9 + 1;
                  param_1 = (unsigned short *)((int)param_1 + 1);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              puVar9 = puVar9 + iVar5 * -2;
              puVar2 = puVar9;
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
                }
      }
      else if (param_6 == 4) {
        if (param_13 == 1) {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_3c = 0;
          if (0 < iVar15) {
            do {
              local_64 = param_4;
              local_54 = 0;
              if ((local_3c & param_22) != 0) {
                if (0 < iVar17) {
                do {
                  local_64 = local_64 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (local_54 >= iVar17) break;
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  }
                  local_64 = local_64 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (local_54 >= iVar17) break;
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    chR = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)((unsigned int)bI4inten * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  }
                  local_64 = local_64 + 4;
                } while (local_54 < iVar17);
                }
              }
              else {
                if (0 < iVar17) {
                  do {
                    lo0 = param_18 & 0xff;
                    iVar16 = (param_14 & 0xff) - lo0;
                    bVar11 = *local_64;
                    bI4inten = (unsigned char)(bVar11 >> 4 | bVar11 & 0xf0);
                    uVar19 = (unsigned int)bI4inten;
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    uVar6 = param_19 & 0xff;
                    iVar13 = (param_15 & 0xff) - uVar6;
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    uVar7 = param_20 & 0xff;
                    iVar18 = (param_16 & 0xff) - uVar7;
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    uVar8 = param_21 & 0xff;
                    iVar20 = (param_17 & 0xff) - uVar8;
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                        (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    bI4inten = bVar11 << 4 | bVar11 & 0xf;
                    uVar19 = (unsigned int)bI4inten;
                    chR = (unsigned char)((int)((int)(uVar19 * iVar16) / 0xff + lo0) >> 3);
                    chG = (unsigned char)((int)((int)(uVar19 * iVar13) / 0xff + uVar6) >> 3);
                    chB = (unsigned char)((int)((int)(uVar19 * iVar18) / 0xff + uVar7) >> 3);
                    chA = (unsigned char)((int)((int)(uVar19 * iVar20) / 0xff + uVar8) >> 7);
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    puVar21[-1] = (unsigned short)(((((unsigned int)chA << 5 | (unsigned int)chR) << 5 |
                                          (unsigned int)chG) << 5) | (unsigned int)chB);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    local_64 = local_64 + 1;
                    local_54 = local_54 + 1;
                  } while (local_54 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_3c = local_3c + 1;
            } while ((int)local_3c < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_3c = 0, 0 < iVar15)) {
            do {
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              puVar9 = puVar9 + iVar5 * -2;
              puVar2 = puVar9;
              iVar5 = (param_7 != 0) ? iVar17 * 2 : iVar17;
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                puVar2 = puVar2 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_3c = local_3c + 1;
            } while ((int)local_3c < iVar15);
          }
        }
        else {
          iVar17 = 1 << (*(int *)(iVar5 + 0x20) - 1);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_38 = 0;
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if ((local_38 & param_22) != 0) {
                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                    pbVar12 = pbVar12 + 4;
                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                      if ((int)puVar2 >= iVar17) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 & 0xf0;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      puVar2 = (unsigned short *)((int)puVar2 + 1);
                    }
                    pbVar12 = pbVar12 + -8;
                    for (param_1 = (unsigned short *)0x0; (int)param_1 < 4; param_1 = (unsigned short *)((int)param_1 + 1)) {
                      if ((int)puVar2 >= iVar17) break;
                      bVar11 = *pbVar12;
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      iVar22 = iVar22 + 1;
                      *(unsigned char *)puVar21 = bVar11 & 0xf | bVar11 << 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      puVar2 = (unsigned short *)((int)puVar2 + 1);
                    }
                    pbVar12 = pbVar12 + 4;
                }
              }
              else {
                for (puVar2 = (unsigned short *)0x0; (int)puVar2 < iVar17;) {
                    bVar11 = *pbVar12;
                    iVar22 = iVar22 + 1;
                    *(unsigned char *)puVar21 = bVar11 & 0xf0 | bVar11 >> 4;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    iVar22 = iVar22 + 1;
                    *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 & 0xf;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    puVar2 = (unsigned short *)((int)puVar2 + 1);
                  }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 1;
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar22 = iVar22 + 1;
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_38 = local_38 + 1;
            } while ((int)local_38 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_38 = 0, 0 < iVar15)) {
            do {
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              puVar9 = puVar9 + -iVar16;
              puVar2 = puVar9;
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                iVar22 = iVar22 + 1;
                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar2 = (unsigned short *)((int)puVar2 + 1);
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar22 = iVar22 + 1;
                *(unsigned char *)puVar21 = *(unsigned char *)puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar2 = (unsigned short *)((int)puVar2 + 1);
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              local_38 = local_38 + 1;
            } while ((int)local_38 < iVar15);
          }
        }
      }
    }
    else {
      one = 1;
      if (param_3 == one) {
      if (param_6 == 2) {
        iVar15 = one << *(int *)(iVar5 + 0x20);
        local_44 = 0;
        iVar17 = one << *(int *)(iVar5 + 0x24);
        if (0 < iVar17) {
          do {
            pbVar12 = param_4;
            if ((param_22 & local_44) != 0) {
              iVar16 = 0;
              param_1 = (unsigned short *)0x0;
              if (0 < iVar15) {
                do {
                  pbVar12 = pbVar12 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (iVar16 >= iVar15) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    param_1 = (unsigned short *)iVar16;
                  }
                  pbVar12 = pbVar12 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if (iVar16 >= iVar15) break;
                    uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                    *puVar21 = uVar4;
                    iVar22 = iVar22 + 2;
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = (int)param_1 + 1;
                    param_1 = (unsigned short *)iVar16;
                  }
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < iVar15);
              }
            }
            else {
              param_1 = (unsigned short *)0x0;
              iVar16 = iVar15;
              if (0 < iVar15) {
                do {
                  uVar4 = FUN_100271f0(*(unsigned short *)(param_5 + (unsigned int)*pbVar12 * 2));
                  *puVar21 = uVar4;
                  iVar22 = iVar22 + 2;
                  puVar21 = puVar21 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pbVar12 = pbVar12 + 1;
                  iVar16 = (int)param_1 + 1;
                  param_1 = (unsigned short *)iVar16;
                } while (iVar16 < iVar15);
              }
            }
            if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar15)) {
              do {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar9;
                puVar21 = puVar21 + 1;
                puVar9 = puVar9 + -1;
                if (iVar22 >= cbMax) {
                  return;
                }
                iVar16 = iVar16 + 1;
              } while (iVar16 < iVar15);
            }
            param_4 = param_4 + *(int *)(iVar5 + 8);
            local_44 = local_44 + 1;
          } while ((int)local_44 < iVar17);
        }
        if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar17)) {
          do {
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            puVar9 = puVar9 + -iVar5;
            puVar2 = puVar9;
            iVar5 = iVar15 * 2;
            if (param_7 == 0) {
              iVar5 = iVar15;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              iVar22 = iVar22 + 2;
              *puVar21 = *puVar2;
              puVar21 = puVar21 + 1;
              if (iVar22 >= cbMax) {
                return;
              }
              puVar2 = puVar2 + 1;
            }
            local_44 = local_44 + 1;
          } while ((int)local_44 < iVar17);
        }
      }
      else if (param_6 == 3) {
        if (param_13 == one) {
          iVar17 = one << *(int *)(iVar5 + 0x20);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          local_44 = 0;
          if (0 < iVar15) {
            do {
              param_9 = (int)param_4;
              param_1 = (unsigned short *)0x0;
              if ((local_44 & param_22) != 0) {
                if (0 < iVar17) {
                do {
                  param_9 = param_9 + 4;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if ((int)param_1 >= iVar17) break;
                    iVar22 = iVar22 + 2;
                    bIA8b = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8b & 0xf;
                    uVar14 = ((unsigned int)bIA8b >> 4) | ((unsigned int)bIA8b & 0xf0);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                         (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                         (param_18 & 0xff) >> 4) << 4 |
                                        (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                        (param_19 & 0xff) >> 4) << 4) |
                               (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                        (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  }
                  param_9 = param_9 + -8;
                  for (local_34 = 0; local_34 < 4; local_34 = local_34 + 1) {
                    if ((int)param_1 >= iVar17) break;
                    iVar22 = iVar22 + 2;
                    bIA8c = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8c & 0xf;
                    uVar14 = ((unsigned int)bIA8c >> 4) | ((unsigned int)bIA8c & 0xf0);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                        (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  }
                  param_9 = param_9 + 4;
                } while ((int)param_1 < iVar17);
                }
              }
              else {
                if (0 < iVar17) {
                  do {
                    iVar22 = iVar22 + 2;
                    bIA8a = *(unsigned char *)param_9;
                    loIA8 = (unsigned int)bIA8a & 0xf;
                    uVar14 = ((unsigned int)bIA8a & 0xf0) | ((unsigned int)bIA8a >> 4);
                    *puVar21 = (unsigned short)((((loIA8) << 4 |
                                        (((param_14 & 0xff) - (param_18 & 0xff)) * uVar14) / 0xff +
                                        (param_18 & 0xff) >> 4) << 4 |
                                       (((param_15 & 0xff) - (param_19 & 0xff)) * uVar14) / 0xff +
                                       (param_19 & 0xff) >> 4) << 4) |
                              (unsigned short)((((param_16 & 0xff) - (param_20 & 0xff)) * uVar14) / 0xff +
                                       (param_20 & 0xff) >> 4);
                    puVar21 = puVar21 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    param_9 = param_9 + 1;
                    param_1 = (unsigned short *)((int)param_1 + 1);
                  } while ((int)param_1 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = puVar21 + -1, 0 < iVar17)) {
                do {
                  iVar22 = iVar22 + 2;
                  *puVar21 = *puVar9;
                  puVar21 = puVar21 + 1;
                  puVar9 = puVar9 + -1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
            do {
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              puVar9 = puVar9 + -iVar5;
              puVar2 = puVar9;
              iVar5 = iVar17 * 2;
              if (param_7 == 0) {
                iVar5 = iVar17;
              }
              for (; 0 < iVar5; iVar5 = iVar5 + -1) {
                iVar22 = iVar22 + 2;
                *puVar21 = *puVar2;
                puVar21 = puVar21 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = puVar2 + 1;
              }
              local_44 = local_44 + 1;
            } while ((int)local_44 < iVar15);
          }
        }
        else {
          iVar17 = 1 << *(int *)(iVar5 + 0x20);
          iVar15 = 1 << *(int *)(iVar5 + 0x24);
          param_1 = (unsigned short *)0x0;
          if (0 < iVar15) {
            do {
              pbVar12 = param_4;
              if (((unsigned int)param_1 & param_22) != 0) {
                iVar16 = 0;
                if (0 < iVar17) {
                  do {
                    pbVar12 = pbVar12 + 4;
                    for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                      if (iVar16 >= iVar17) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 << 4 | bVar11 >> 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + -8;
                    for (param_9 = 0; param_9 < 4; param_9 = param_9 + 1) {
                      if (iVar16 >= iVar17) break;
                      bVar11 = *pbVar12;
                      *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                      puVar21 = (unsigned short *)((int)puVar21 + 1);
                      iVar22 = iVar22 + 1;
                      if (iVar22 >= cbMax) {
                        return;
                      }
                      pbVar12 = pbVar12 + 1;
                      iVar16 = iVar16 + 1;
                    }
                    pbVar12 = pbVar12 + 4;
                  } while (iVar16 < iVar17);
                }
              }
              else {
                param_9 = 0;
                if (0 < iVar17) {
                  do {
                    bVar11 = *pbVar12;
                    *(unsigned char *)puVar21 = bVar11 >> 4 | bVar11 << 4;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    param_9 = param_9 + 1;
                  } while (param_9 < iVar17);
                }
              }
              if ((param_7 != 0) && (iVar16 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < iVar17)) {
                do {
                  *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  puVar9 = (unsigned short *)((int)puVar9 + -1);
                  iVar22 = iVar22 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  iVar16 = iVar16 + 1;
                } while (iVar16 < iVar17);
              }
              param_4 = param_4 + *(int *)(iVar5 + 8);
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
          if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
            do {
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              puVar9 = (unsigned short *)((int)puVar9 - iVar16);
              puVar2 = puVar9;
              iVar16 = iVar17 * 2;
              if (param_7 == 0) {
                iVar16 = iVar17;
              }
              for (; 0 < iVar16; iVar16 = iVar16 + -1) {
                *(unsigned char *)puVar21 = (unsigned char)*puVar2;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                iVar22 = iVar22 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                puVar2 = (unsigned short *)((int)puVar2 + 1);
              }
              param_1 = (unsigned short *)((int)param_1 + 1);
            } while ((int)param_1 < iVar15);
          }
        }
      }
      else if (param_6 == 4) {
        param_9 = 1 << *(int *)(iVar5 + 0x20);
        iVar15 = 1 << *(int *)(iVar5 + 0x24);
        param_1 = (unsigned short *)0x0;
        if (0 < iVar15) {
          do {
            pbVar12 = param_4;
            if ((param_22 & (unsigned int)param_1) != 0) {
              iVar16 = 0;
              if (0 < param_9) {
                do {
                  pbVar12 = pbVar12 + 4;
                  for (local_24 = 0; local_24 < 4; local_24 = local_24 + 1) {
                    if (iVar16 >= param_9) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  }
                  pbVar12 = pbVar12 + -8;
                  for (iVar13 = 0; iVar13 < 4; iVar13 = iVar13 + 1) {
                    if (iVar16 >= param_9) break;
                    *(unsigned char *)puVar21 = *pbVar12;
                    puVar21 = (unsigned short *)((int)puVar21 + 1);
                    iVar22 = iVar22 + 1;
                    if (iVar22 >= cbMax) {
                      return;
                    }
                    pbVar12 = pbVar12 + 1;
                    iVar16 = iVar16 + 1;
                  }
                  pbVar12 = pbVar12 + 4;
                } while (iVar16 < param_9);
              }
            }
            else {
              iVar16 = 0;
              if (0 < param_9) {
                do {
                  *(unsigned char *)puVar21 = *pbVar12;
                  puVar21 = (unsigned short *)((int)puVar21 + 1);
                  iVar22 = iVar22 + 1;
                  if (iVar22 >= cbMax) {
                    return;
                  }
                  pbVar12 = pbVar12 + 1;
                  iVar16 = iVar16 + 1;
                } while (iVar16 < param_9);
              }
            }
            if ((param_7 != 0) && (local_24 = 0, puVar9 = (unsigned short *)((int)puVar21 + -1), 0 < param_9)) {
              do {
                *(unsigned char *)puVar21 = *(unsigned char *)puVar9;
                puVar21 = (unsigned short *)((int)puVar21 + 1);
                puVar9 = (unsigned short *)((int)puVar9 + -1);
                iVar22 = iVar22 + 1;
                if (iVar22 >= cbMax) {
                  return;
                }
                local_24 = local_24 + 1;
              } while (local_24 < param_9);
            }
            param_4 = param_4 + *(int *)(iVar5 + 8);
            param_1 = (unsigned short *)((int)param_1 + 1);
          } while ((int)param_1 < iVar15);
        }
        if ((param_8 != 0) && (puVar9 = puVar21, param_1 = (unsigned short *)0x0, 0 < iVar15)) {
          do {
            iVar5 = param_9 * 2;
            if (param_7 == 0) {
              iVar5 = param_9;
            }
            puVar9 = (unsigned short *)((int)puVar9 - iVar5);
            puVar2 = puVar9;
            iVar5 = param_9 * 2;
            if (param_7 == 0) {
              iVar5 = param_9;
            }
            for (; 0 < iVar5; iVar5 = iVar5 + -1) {
              *(unsigned char *)puVar21 = (unsigned char)*puVar2;
              puVar21 = (unsigned short *)((int)puVar21 + 1);
              iVar22 = iVar22 + 1;
              if (iVar22 >= cbMax) {
                return;
              }
              puVar2 = (unsigned short *)((int)puVar2 + 1);
            }
            param_1 = (unsigned short *)((int)param_1 + 1);
          } while ((int)param_1 < iVar15);
        }
      }
    } else if ((param_3 == 2) && (param_6 == 0)) {
      uVar14 = 1 << *(int *)(iVar5 + 0x20);
      local_44 = 0;
      iVar15 = 1 << *(int *)(iVar5 + 0x24);
      if (0 < iVar15) {
        do {
          param_9 = 0;
          puVar9 = puVar21;
          iVar17 = iVar22;
          pbVar12 = param_4;
          if ((param_22 & local_44) != 0) {
            uVar6 = local_44;
            if (0 < (int)uVar14) {
              while (1) {
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + 4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
                uVar4 = FUN_100271f0(*(unsigned short *)(pbVar12 + -4));
                *puVar21 = uVar4;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                iVar22 = iVar22 + 2;
                param_9 = param_9 + 1;
                if (param_9 >= (int)uVar14) break;
                if (iVar22 >= cbMax) {
                  return;
                }
              }
              puVar9 = puVar21;
              iVar17 = iVar22;
            }
          }
          else {
            if (0 < (int)uVar14) {
              do {
                uVar4 = FUN_100271f0(*(unsigned short *)pbVar12);
                *puVar21 = uVar4;
                iVar22 = iVar22 + 2;
                puVar21 = puVar21 + 1;
                pbVar12 = pbVar12 + 2;
                if (iVar22 >= cbMax) {
                  return;
                }
                param_9 = param_9 + 1;
                puVar9 = puVar21;
                iVar17 = iVar22;
              } while (param_9 < (int)uVar14);
            }
          }
          iVar22 = iVar17;
          puVar21 = puVar9;
          if ((param_7 != 0) && (iVar17 = 0, puVar9 = puVar21 + -1, 0 < (int)uVar14)) {
            do {
              iVar22 = iVar22 + 2;
              *puVar21 = *puVar9;
              puVar21 = puVar21 + 1;
              puVar9 = puVar9 + -1;
              if (iVar22 >= cbMax) {
                return;
              }
              iVar17 = iVar17 + 1;
            } while (iVar17 < (int)uVar14);
          }
          param_4 = param_4 + *(int *)(iVar5 + 8);
          local_44 = local_44 + 1;
        } while ((int)local_44 < iVar15);
      }
      if ((param_8 != 0) && (puVar9 = puVar21, local_44 = 0, 0 < iVar15)) {
        do {
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          puVar9 = puVar9 + -uVar6;
          puVar2 = puVar9;
          uVar6 = uVar14 * 2;
          if (param_7 == 0) {
            uVar6 = uVar14;
          }
          for (; 0 < (int)uVar6; uVar6 = uVar6 - 1) {
            iVar22 = iVar22 + 2;
            *puVar21 = *puVar2;
            puVar21 = puVar21 + 1;
            if (iVar22 >= cbMax) {
              return;
            }
            puVar2 = puVar2 + 1;
          }
          local_44 = local_44 + 1;
        } while ((int)local_44 < iVar15);
      }
    }
    }
  }
}


#endif /* BR_MATCHING_BUILD */
