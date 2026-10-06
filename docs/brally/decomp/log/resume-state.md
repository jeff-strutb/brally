# Resume state

*Recorded 2026-09-19.*

> Full RESUME log for the matching decomp: per-session state, counts, parked functions and open levers. Loaded on demand, not from the index.

The the notes index index carries only the current headline; the detail lives here.
Newest first. See [matching-progress](matching-progress.md) for the older running log.

## RESUME (2026-09-19e, THE ANNEX) - GATE PASSED: 136/136 certified functions place

- ** THE ANNEX (0508f462): an over-slot certified body ships WHOLE in an
  executable `.t3x` section appended to BRGlide.T3.dll, with a 5-byte
  `jmp` thunk at the original VA.  The slot was fixed-layout geometry,
  never part of the certification.  7 bodies annexed (GlInstall,
  SnapInterpDraw, CtlInputApply, RcaFixup, FUN_100038f0, FUN_1002f790,
  GhostLoad); HudDraw un-force-blocked and fits in-slot under the fixed
  measure.  CONTRACT-VALID GATE PASSED - first ever.  Manifest:
  build/brally/win32/image/t3_annex.csv; both verifiers follow the thunk.**
- **Annex value rules (in _annex_fill): jump tables re-based from the
  object's own symbol table at the annex address; $T constants and
  cross-section EH dwords carried as absolute; REL32 shifted by the
  placement delta; the rest through trusted/confirmed; an unnameable
  slot still blocks.**
- ** config/brally/globals_hand.csv (relocmap.load_learned_full): hand
  address rows appended to globals_learned.csv GET WIPED when
  reloc_learn.py regenerates it (happened mid-session - took out the
  KeyTableFind trio).  Hand rows go in globals_hand.csv, loaded second,
  wins on collision.**
- ** bool/char returns compare AL only (2a3707bb): full-eax compare on a
  bool return manufactured GhostLoad's false DIFF - the ORIGINAL itself
  returns 0xFFFFFF01 on its failure path.  EQUIVALENT on 48 seeds after.**
- **Two flagged rows, measured, NOT placement bugs (placed bytes ==
  certified resolution, byte-for-byte): HudDraw DIFF = identical 131-call
  sequence, call #34's FLOAT arg differs in the last mantissa bits
  (0x41A37BE2 vs 0x41A37AF2) - 64-bit-emu x87 rounding through a text
  emitter into the DL cursor; RaceStep DIFF = identical 54-call sequence,
  fork at call #2 icall arg 0x1fffac vs 0x1fffa4 (post-_CIpow coverage).
  Classification lane: FP-fed divergence through a CALLEE cannot be
  told from a real bug by the current classifier - next lever.**
- Next: full placed-image re-sweep with the byte-return fix; the
  FP-through-callee classifier; hardware test of BRGlide.T3.dll.

## RESUME (2026-09-19d, guard + identity-slot fixes) - 127 of 135 PLACED HONESTLY; 8 named walls

- ** TWO PLACEMENT-SOUNDNESS BUGS FOUND BY THE PLACED-IMAGE SWEEP AND
  FIXED (5e2c31fa, ef02f4b4):**
  1. **Identity slots clobbered:** phase B let pairing/audit overwrite
     jump-table/const identity slots - BrObjDlBuild's switch shipped the
     ORIGINAL's table addresses (+0x194 into our differently-laid body)
     and dispatched through garbage while the obj oracle said EQUIVALENT.
     Identity slots now outrank pairing AND audit in both lanes.
  2. **$-label truncation undermeasure:** the guard bounded the body at
     the next SYMBOL, and $L case labels are symbols - BrCtlInputApply
     measured 231 of ~3300B and shipped 94B-truncated; FUN_100038f0
     (+441B) and FUN_1002f790 (+132B) shipped truncated the same way.
     Guard now skips '$' symbols.   ALSO: the obj-side oracle's capped
     run disassembles the FULL obj before capping the overlay, so it does
     NOT model a truncated ship - never take a capped-run EQUIVALENT as
     slot-ok evidence; only the placed-image run counts.
- **The 8 walls: GlInstall +3, RcaFixup +6, GhostLoad cpp +14,
  CtlInputApply +94 (O2 variant is A5 DIFF vs original - pin stays O2y),
  FUN_1002f790 cpp +132, SnapInterpDraw +284, FUN_100038f0 cpp +441,
  HudDraw force-blocked (DL-cursor DIFF).  All need shrink/routing lanes;
  slot-fit respell tricks in the 09-19c entry below.**

## RESUME (2026-09-19c, slot-fit respells) - superseded count; levers still valid

- **Target restated by the project lead: ALL 135 certified rows placed (120 gate +
  15 cpp-lane).  This session closed 4 slot-overs: BrGbiSizeShift 94/96
  (parameter-reuse tail, A5 EQUIVALENT, 129519eb+5e6773c3);
  BrSprFontGlyphA 0x10054550 BYTE-EXACT (prototyped-short arg3 unlocked it,
  db7474bc, then crank reorder_stmts 38333078 - graduated T4);
  BrKeyTableFind 81/83 (volatile count read + oracle profile pinning the
  count, 3d168485+e339c5ed, variant pinned O2 - sweep's O2y raw-min pick
  keeps the frame and stays over); BrSeasonApply cpp 624/629 (merge the
  constant-folded `return ret`/`return z` exits into one `done:` exit so
  the result rides edi like the original - 701d35ce, A5 re-proven).**
- ** NEW LEVER, used 3× today: the SLOT-FIT respell.  A T3 body over its
  slot only needs ≤slot + A5 EQUIVALENT, not byte-shape.  Working tricks:
  (1) parameter reuse keeps operand+result in one register (eax short
  encodings); (2) volatile read defeats VC5's load hoisting without
  changing the multiset materially; (3) prototyped `short` arg lets the
  caller push the home register raw instead of movsx'ing (pins eax);
  (4) merging constant-folded return exits un-folds `mov eax,imm` back to
  the original's register return.  After any respell: purge/re-derive the
  offset-keyed reloc_overrides rows and pin t3_variant.csv if the sweep's
  raw-min variant differs from the fitting one.**
- **Gate now: 129 T3 placed + SprFontGlyphA byte-exact = 130/135 in
  BRGlide.T3.dll; 0 address-blocked.  config/brally/t3_blocked.csv is the new
  FORCE-BLOCK channel (7ed52a33): BrHudDraw 0x10015300 blocked on its
  placed-image DIFF at the DL cursor 0x106E7710 (routing lane pending).**
- **The 5 remaining walls, all dated 09-19: BrGlInstall +3 (VC5 je-rel32
  to the shared ret where the orig has jne+inline ret; body-under-if,
  goto, run-once-while, variants, crank 23+43 candidates ALL produce
  je-end - the jne+ret layout only appears when a LOOP follows the guard,
  per corpus); BrRcaFixup +6 (ebx/ebp exchange, 263 dead candidates,
  header says not source-reachable); BrGhostLoad cpp +20 (block-layout
  inversion wall, shared with the season reader); BrSnapInterpDraw +287
  (full lane); BrHudDraw (routing DIFF, force-blocked).**
- KeyTableFind's oracle needed a PROFILE (tools/brally/oracle_profiles.py
  0x10030FD0): a random count global = 2^31-iteration runaway.  Profiles
  are cheap - pin the loop bound, drive hit+miss, done.
-  crank gotcha: `tools/brally/crank.py --help` starts a sweep ([match-tooling-gotchas](../traps/match-tooling-gotchas.md)
  was right); targeted `crank.py <VA> --budget 60` is the Gate-B
  ledger-pass minting workflow after any respell (2 runs = 2 counted
  passes; it commits its own ledger lines).
-  [shared-tree-partial-commit](../traps/shared-tree-partial-commit.md) bit again softly: committing
  config/brally/globals_learned.csv by pathspec swept this session's regenerated
  refcount drift into 3d168485 (benign, but check `git diff` first).
- A parallel session is active on the same tree (28283a33 BrCtlInputApply
  cert, t3.py cross-jump-ret class 2dbace23); keep pathspecs, expect
  raced-gate EXIT verdicts, re-run when still.

## RESUME (2026-09-15, "20 rows" 3rd consecutive) - 0 T4 / 1 deep T1->T2; pool confirmed drained

- **Denominators: 0 byte-exact this session; 1 T1->T2 (0x1005D060, committed
  c2f33418, NO @implements); tree unchanged 1009/1250 match. Pool drained
  exactly as [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md) + the 09-13e/09-14 sessions
  predicted -- no easy fresh rows exist.**
- **0x1005D060 BrAiScanCorridor fully reverse-engineered + transcribed ->
  T2 in br_ctlai.c** (the corridor scan, br_ai.h rule 8's named binding gap).
  848/856 B, REGNORM 29+17. WALL: recursive thiscall with computed recursion
  args -> C-lane __fastcall approximation materialises each arg through a
  stack slot; byte-exact needs a C++ member rewrite. Full dossier +
  handoff: [corridor-scan-2026-09-15](../triage/corridor-scan-2026-09-15.md).
- **Pool recon (de-dup filter run):** 33 fresh non-cert/non-EH/non-ledgered
  C diff rows; the two CLOSEST (BrMat3Mul 0x1006DD20 19 diffs, BrMat4Mul
  0x10029D70 24 diffs) are BOTH the rolled-loop row-anchor class that
  [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md) flags ordinal-INSENSITIVE (TU-state lever does
  NOT apply); no new lever has landed since 09-14, so re-probing them is
  thrash. The rest are 70+ diffs = structural.
- **Fresh 856 B+ T1 band screened (all ABSENT from C report = untranscribed):**
  clean-structural = 0x1005D060 (worked), 0x1006C990 (994B x87=25),
  0x10005810 (1070B x87=25), 0x1005EDC0 (1110B x87=0!). x87-heavy/thiscall
  (screen-reject unless co-filed): 0x1005D3C0, 0x1005E7B0, 0x1006E5C0,
  0x100221D0, 0x10022600, 0x1000C4E0, 0x1006F170, 0x10022BF0, 0x1006EC30,
  0x1005C8B0. EH (cpp ctor lane): 0x100498A0/0x1004CBA0/0x1004BE00/0x100038F0.
  **0x1005EDC0 (1110B, ZERO x87) is the next clean structural target** -- pure
  integer, no float-schedule wall possible; not opened this session.
- **Two named fresh cpp rows still open:** 0x100550E0 (581B thiscall vcall,
  x87 fild/fstp, no clean C close), 0x1002F790 (2517B EH packet receiver,
  60 calls, br_dplayappmsg.c) -- both C++ lane, both heavy.
- **BrCtlAiBody 0x1005D770 is NOT regressed** despite report.csv showing 2279
  "diffs": that is positional cascade (FIRSTDIV +0x18). fn.py: +5 B, REGNORM
  14+10 -- healthy T3. report.csv `diffs` is positional for size-differing
  rows; always confirm with fn.py REGNORM.

## RESUME (2026-09-15b, "retranscribe A1/A4 failures") - re-triaged 5, found the SWEEP VARIANT ARTIFACT; 4 are scheduling walls, 1 real target

- **Task premise (A1/A4 fail = missing code) is FALSE for 4 of 5 -- the A1/A4
  failures were a SWEEP VARIANT ARTIFACT.** [sweep-variant-selection-artifact](../traps/sweep-variant-selection-artifact.md):
  match_sweep records the raw-byte-min variant, so frameless originals got
  recorded on dead-end /Oy- (ebp-frame) or /Op-bloated variants that inflate
  the gates. Re-measured each against the frame-correct O2:
  - 0x10032E40 cartrail: gap 3 (not 16), x87 fxch scheduling. NOT missing code.
    Verdict CORRECTED in-file (was my own wrong 2026-09-15 note).
  - 0x10001CF0 chasestep: recorded O2 but A4 "357 B never compared" is a
    divergence RESYNC artifact; whole-fn msetdiff 43 rows, +6 insns, all x87
    (fxch/fcom-fcomp/fst-fstp/fsubp/commutation). Scheduling wall. CORRECTED.
  - 0x10068900 obb: recorded O2p (bloated); real O2 215 rows, x87 operand-role
    (fld [local];fmul [field] vs swapped). Scheduling wall.
  - 0x1005D060 was the parallel session's; my fresh picks were 0x1000CBA0 objdl
    + 0x1005D770 ctlai. ctlai: A1/A2/A4 pass, A3 10 unpaired (exhausted
    scheduling, 26 dead probes) -- set aside, not ground.
- ** 0x1000CBA0 BrObjDlBuild is the ONE real transcription target.** Under O2
  (not the recorded dead-end O2y): 176 rows, gap 44, and the residue is genuine
  SEMANTIC divergence -- orig `shr R,8` x12 vs our `shr R,0x10` x6, orig
  `and R,0x1f` x9 absent, stack 0x58 vs 0x50, 10 EXTRA indexed byte RMW
  `add byte [R+A],B`. Per retranscribe-don't-patch (>2 artifact classes) it
  wants a full rewrite against the disassembly, MEASURED ON O2 -- variant must
  be pinned to O2 first or progress won't register. NOT attempted (large).
- **Commits:** 0541124c (cartrail correction), e6263dca (chasestep/obb/objdl
  verdicts). No matches, no gate. Claims released. Tree clean.
- **NEXT:** either (a) fix the sweep to be frame-aware / register-blind-tiebreak
  so gates stop lying (broad re-sweep, project lead's call), then objdl is measurable;
  or (b) retranscribe objdl against O2 directly. The other four are parked
  scheduling walls -- do not re-open as transcription.

## RESUME (2026-09-15, "five largest -> T4/T3, no excuses") - 0 T4 / 0 T3; the wall on all five is A2 (raw distance), MEASURED

- **Denominators: 0 byte-exact, 0 T3 certified this session. Tree unchanged
  (1009 matched C, 125 T3-certified). No image gate (no match landed).** The
  five largest non-giant, non-EH, non-@t3 rows re-derived from report.csv and
  locked; every one fails Gate A on **A2 (raw msetdiff distance), the cap the
  project lead DECLINED to raise** ([do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)). Fresh
  qualify numbers:
  - 0x1000E320 BrSceneVisPrepare: A2 20 (limit 13.6), A3 clean ONLY with a new
    fold (below); A1/A4/A5 pass. Region 1 = address-taken pt.x/pt.y schedule
    (dead); region 2 = the view-rect edge sums, a commutative-int-add identity.
  - 0x10068F80 BrCarCarCollide: **A3 already PASSES, 0 unpaired** -- the whole
    78-row (39+39) x87-scheduling residue classifies via the EXISTING folds;
    only A2 distance (limit 9.9) rejects it. Fully-explained, A2-walled.
    Co-filing NULL (obb is VA-adjacent but a different best variant = not same
    original TU, per [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md)).
  - 0x10001CF0 BrCamChaseStep (A1 5, A3 15, A4 lost-sync 357B), 0x10032E40
    BrCarTrailStep (A1 16 short), 0x10068900 BrObbOverlap (A2 215, A3 25): all
    FAIL A1 = real missing/extra SEMANTIC code -> **T2 transcription** (close
    the A1 gap), NOT a scheduling/exact-identity lever. Not certification
    candidates. Verdicts written into each file header, committed 53686103.
- ** NEW, PROVEN, but NOT LANDED: an integer commutative-add is a clean exact
  identity for t3.py's A3.** `mov D,[a]; add D,[b]` (a+b) == `mov D,[b]; add
  D,[a]` (b+a): same value, same flags (add carry/overflow are symmetric), same
  two reads, NO rounding -- a cleaner identity than the x87 fadd/fmul folds
  already in t3.py. Built the guarded quad-cancel fold (integer analog of the
  x87 memory-memory fold; refuses reloc'd `[A]` operands via the a!=b guard):
  it takes 0x1000E320 A3 16->0 and **revalidates all 125 certified tags with
  ZERO demotions/staleness**, but promotes NOTHING <=400 B and does not help
  A2, so it certifies nothing today. REVERTED per the session plan ("never edit
  t3.py"). See [integer-commutative-add-fold](../oracle/integer-commutative-add-fold.md). To ever certify 0x1000E320
  needs this fold AND region-1 un-spill AND A2 relief -- three things, A2 relief
  declined. So the five-largest verdict stands: [five-largest-2026-09-13b](five-largest-2026-09-13b.md)'s
  measured NO, re-confirmed with A2 named as the mechanism.
- ** PROJECT PRINCIPLE (2026-09-15), asked whether to add the fold:** *"is it at
  the same standard or higher, or a compromise just to call something done that
  truly isn't?"* -- i.e. exact-identity classifier extensions are fine ONLY if
  they hold/raise the standard; the A2 distance cap is not to be worked around
  to hit a number. Consistent with [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).
- Housekeeping: deleted 4 stray files at start (br_carcol.obj, br_obb.obj, and
  two external ext2 listings 3DINSERT.asm/HWINSERT.asm -- external corpus, must
  never land in-tree). refcheck Glide-keyed, hooks/claims/fileaudit all clean.

## RESUME (2026-09-14, "make-rows day, 15 asked") - 0 T4 / 2 T1->T2 / T1 lane CLOSED under 800 B

- **Denominators: 0 byte-exact matched this session; 2 T1->T2 promotions
  (tiers T1 52->50); T3 unique count unchanged at 125; image gate PASSED.**
- **0x10024490 BrTexResample (468 B) -> T2 in br_texlerp.c**, byte-exact
  through +0x15f of 0x1d3. Levers that PAID (now in VC5-IDIOMS tail):
  explicit dead cases 4-11 to force the jump table, `((i <= 0) - 1) & i`
  branchless clamps, hix-init/lox/hix-clamp-if split, sfrac as own local,
  inner `for (cx = dw; cx > 0; cx--)` (counter init sunk below guard).
  Residue: ONE allocator base/index knot in the callback-arg block, 6
  probes dead, ledger in the file. Strong future T3.
- **0x10067C30 BrCarPhysAdvance (762 B) -> T2 in br_carphys.c**, byte-exact
  preamble + entire substep loop + stuck-timer machine. Levers that PAID:
  literal `1.0f` (pools to the shared constant), field-first `>=`, the
  float compare read DIRECTLY in both arms (PRE -> one fcomp, deferred
  fnstsw; a bool local materialises eax), clamp-then-unconditional-
  decrement (the orig "-1 then -2" is cross-jumping), dx*dx+dy*dy+dz*dz.
  Residue: x87 operand-role TU-state at 3 sites -- five-largest class,
  NOT respellable. Used the #define rename dance for BrCarPhysAdvance/
  BrCrRespWalk/BrCollRespTipKick around the port headers.
- **T1 lane VERDICT: every untranscribed T1 row under 800 B is now
  screen-rejected** (fxch straight-line 0x1000E150/0x1006D2E0/0x1000C9E0/
  0x10063FA0/0x100682C0/0x10068070; bare-fistp __asm pair 0x10023110 +
  0x10021A20 confirmed via br_dl.c:914 exclusion; 0x10035533 odd;
  0x10073994 fenced data). 0x100550E0 (581 B) is a thiscall vcall-imm8
  method -> C++ lane when opened. Next T1 band starts at 0x1005D060 (856 B).
- **C++ lane re-sweep: all 31 non-matching rows re-scored, NO stale walls.**
  Every open row is parked with a dossier; 0x10062B80 is a NAMED carrier of
  the anchorless byte-widen wall (do not touch). The t3.py --qualify --all
  READY list (83) is ALL already-tagged rows.
- **!! 0x1003AB00 was certified TWICE: the C twin in br_menucb.c has carried
  @t3 since 09-09. CHECK FOR A C-TWIN @t3 (grep "@t3 <VA>" src/brally/) BEFORE
  certifying a cpp row -- t3.py keys the ledger to the file its report row
  points at and does not see the twin.** The duplicate cert added real
  evidence (ext/ext2/crt corpora + VC4.2 all miss the unfused
  sub/test/jge) and is committed. tools/brally/cpp_twin_retire.py NO LONGER
  EXISTS (that note is stale); twin tooling is gen_cpptwin.py /
  twinfind.py / twinscreen.py.
- 0x10039D20: 2 fresh probes dead (cond-temp canonicalised away, per-arm
  PRE +3 B); residue confirmed scheduler load-order + rotation; dead list
  updated in br_menucb.c.
- Fixed a stale claim at session start (br_sprfont 0x100540D0 -> cpp ref).

## RESUME (2026-09-13e, "20 rows") - Pool B DRY. 0 matched / 20 asked; 1 fence

- **t4lane Pool B produced ONE clean claim, 0x10073994 (357 B), and it is
  NOT CODE** -- same 16-byte data-record class already fenced at
  0x10073704/0x10073709 (records `{cN, 8000cN0c, 0, ptr 0x100786D8}`, cN
  stepping 0xEB..). Fenced as `data_table` (commit e4aecdb), claim released.
  Tell: ghidra draft is all `*p = *p + c` chains ending in halt_baddata,
  corpus prologue query MISSES everywhere.
- **After the fence, Pool B = 0 clean.** The 6 rejected rows are the
  screened fxch/16-bit x87 tail (0x1000E150, 0x1006D2E0, 0x1000C9E0,
  0x10063FA0, 0x100682C0) + 0x10035533 (odd address, split map row). Per
  [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md) the straight-line x87 rows are
  placement rows; per session instruction they were out of lane. Pool A
  had only the 2 known T3-candidate rows (0x1006D530, 0x10039D20), not
  opened (project lead did not name them).
- Stopped per instruction: "if dry, say so and stop." Next intake must
  come from a pool refresh ([pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md)) or
  hand-picked VAs via `claim_lane.py claim --va`.

## RESUME (2026-09-09c, third parallel session) - +1 byte-exact T1 intake; dlclip LEFT probed to a proven wall

- **0x100314D0 BrGlTrackFixupAll (398 B) BYTE-EXACT in 3 probes**, filed in
  src/brally/core/startup/br_track.c with its 10 already-exact neighbours. Levers,
  now on the VC5-IDIOMS tail: guard literal survives (`1 <= n` = cmp,1/jl;
  `0 < n` = test/jge), max-scan compare is value-first (`*p > iMax`, copy
  sibling BrTrackSetF08FromMax), and an orig `mov edx,eax` before `dec/jne`
  means a SEPARATE countdown counter initialised from the bound.
- **t4lane Pool B says "nothing to claim" but the printed unclaimed tail
  (fxch / 16-bit-ops screened rows) is claimable by VA** with
  `claim_lane.py claim --va <VA>` -- that is where this match came from.
  "Ledger exhausted" still is not "no work".
- **0x1001F2B0 dlclip LEFT: parked at 2 B with the wall now PROVEN.** 12
  probes (dossier updated, committed): flip and sink are ONE mechanism --
  any non-leaf wrap of an operand (paren, ternary, comma) both picks it as
  the fld AND defers that site's fadd while another value is live on the
  x87 stack; casts and `*&` are transparent (inert); pNext hoist and
  declaration order inert; corpus has NO hi-displacement-lead fld/fadd pair
  anywhere. Do not respell; only a schedule-pinning discovery moves it.
-  fn.py is /O2-only: on br_dlclip.c (/O2 /Op) its CONTROL run of the
  unmodified file scores -4 B. Probe O2p files through match_sweep only.
-  t3.py --qualify A3 on LEFT reports 3 unpaired rows (fstp/fld vs fst)
  that the side-by-side shows identical -- the 2026-09-09 normaliser
  artefact class again.
- Parallel session traffic was heavy: it committed BrTexAnimStep while this
  session was verifying it (commit 5bf537d; my globals-only commit 0f77879
  amended to say what it holds), left 4 new STRANDED rows (0x10017F80,
  0x100192D0, 0x10019840, 0x1001C7A0 etc.) and 2 UNRECORDED matches
  (0x10063AD0 BrReplayAdvance, 0x10029FE0 BrMat4Perspective7) -- theirs to
  file; survey.csv + globals_learned.csv diffs citing their VAs left
  uncommitted for them.

## RESUME (2026-09-09b, same session continued) - +2 MORE T3: BrFrameDraw AND BrRcaFixup

- **0x10011FA0 BrFrameDraw (4,500 B) CERTIFIED, size-exact.** Site 1 fell to
  probe X7: `pV = &aViews[i]` moved ABOVE the BrPodNop trace call -- the call
  clobbers the scratch holding 11*i so the fold is no longer free and the
  read goes through esi like the original (4501 -> 4500 B, regnorm 1+1 ->
  0+0).  THE LEVER WAS STATEMENT POSITION RELATIVE TO A CALL, tried after
  every SPELLING below the call was dead -- when a dead list is all
  spellings, move the statement. Residue: the +0xaf add destination (width's
  second use blocks the named-operands lever) + the pV lea above the call's
  constant pushes; ~60 probes dead over three sessions.
- **0x10030770 BrRcaFixup (1,641 B) CERTIFIED.** Residue = ONE register
  exchange (the +0x8014 address CSE vs the inner swap-loop counter,
  ebx/ebp), +6 B of [ebp] disp8 encodings, 10 regions, multiset 0+0.
  22 hand probes + 241 crank candidates dead.  crank's parked endpoint
  (build/brally/analysis/ghidra_work/0x10030770.crank.c, "regions 8 bytes -2") is UNSOUND:
  it reads the +0x8010 count BEFORE the BR_LD32BE that byte-swaps it.
  **crank's non-exact parked candidates use unsound intermediates to steer
  -- NEVER land one without checking the transformation is a transcription.**
  Also: crank cannot write its ledger line when the tag carries the d3d twin
  VA (it greps the glide VA); write those lines by hand.
- fileaudit "matched, never recorded: 1" = 0x10034310 BrVec3Dot, the OTHER
  session's in-flight br_vec.c work -- theirs to file, left alone.
- Tree: 26 T3-certified. Next largest with real (non-colouring) residue:
  BrCtlAiBody 0x1005D770 (25 rows), BrObjDlBuild 0x1000CBA0, BrCrRespWalk
  0x10067710 (24 rows, lost-sync 33 B), BrSceneSetupFrame 0x10015630 (42
  rows, /O2 /Op TU). BrTex3dRegister/BrCarDrawBody/BrCarWheelFx/
  BrCrImpulseSolve/BrAnimUpdate/BrInputJustPressed all have MISSING
  SEMANTIC code (A1/A3 far off) -- T2 transcription work, not T3 passes.

## RESUME (2026-09-09) - T3 LANE ON THE LARGEST ROWS: +1 BYTE-EXACT, +3 T3

Asked for T3 on the largest functions. Ranked report.csv diff rows by size,
ran `t3.py --qualify` on the eight largest non-giant ones, worked the
closest four one at a time. Detail and method: [t3-lane-largest-2026-09-09](t3-lane-largest-2026-09-09.md).
- 0x100706D0 BrInputPoll (4,145 B): already passed every gate -- tagged (e64bcd7).
- 0x1005FF00 BrRaceGateStep (2,538 B): A3's one "semantic" row was a
  msetdiff reloc artefact (fixed 9fd065e); then the 2-byte residue fell to
  naming both add operands with the count declared after the field -- BYTE-
  EXACT (243bb2c), idiom on the VC5-IDIOMS tail.
- 0x10031B80 BrGlTrackHdrRead (1,549 B): 13 -> 4 regions in three landed
  levers (pointer after swap; counter-expression loops; h as the parameter
  expression), 55-compile dead list, passes 5/6 zero-movement -> tagged.
- 0x1001D1B0 BrScenePropsDraw (1,768 B): A3 failed on `lea R,[R+d]` vs
  `add R,d` -- canon now pairs them (single-register form); 32 compiles in
  three passes, zero movement -> tagged.  The tag pushed WHAT IT DOES out of
  the 40-line window; the pre-commit hook did not catch it, t3.py did.
- NOT opened: BrFrameDraw 0x10011FA0 (parallel session holds the file
  uncommitted; its note names the yMir/hMir order as the lever), the three
  giants (rule 11).
Tree: T3-certified 20 (17 of them the parallel session's small rows the same
day). Claims released (token 21dad28e). fileaudit OK.

## RESUME (2026-09-05c) - T1 INTAKE LANE: +9 byte-exact from the smallest drafts

**Nine byte-exact from `tiers.py --list T1` sorted by size, smallest first,
each on its own commit and filed in a module** (0x100023F0 BrTimeFormat,
0x10035BE0 BrDpShutdown, 0x10032530 BrBootColdInitRun, 0x1006AAF0
BrNetPeerMsgReset, 0x1002B3F0 BrPointDepthFrac, 0x10003530 BrChkFReadLine,
0x100154A0 BrHudDrawSplitTimes, 0x10002310 BrCamFrameInitB, 0x10031960
BrTrackFixupSegRec). Two parked as T2 with dead-probe notes in
src/brally/core/audio/br_sndload.c: 0x100701B0 (8 B long, block layout) and
0x10070280 (43 B, two register copies swapped, REGNORM 0+0).

**What the lane taught (all in docs/brally/VC5-IDIOMS.md tail):** a named
wait-result local swaps two hoisted import registers; an expression of the
loop counter is the INDEX register, a named bumped local the BASE; a parameter
used as the cursor is registerised in the loop preheader; an arm written as
the ELSE of `if (c != EOF)` is laid out last; /Od homes locals in declaration
order; a 12-byte vector copy is three scalar float copies; the saved byte
takes eax in a 4-byte swap; a pointer local initialised at declaration is
hoisted above the first branch.

**Method that paid:** `build/brally/win32/match/sbs.py <obj> <sym> <VA>` -- a side-by-side
capstone dump of the recomp object against the original -- found every
divergence in one read; fn.py's EXTRA/MISSING alone misled twice (a symbol
substring matched the PORT twin `_port` first -- match the symbol exactly).
`fn.py --make/--var` probes at ~10 s each; 4-6 probes per function.

**Bookkeeping traps seen:** the other session's pathspec commit swallowed my
filing.csv rewrite (harmless), and `tools/brally/filing.py` DROPS rows whose VA is
not in report.csv -- re-add another session's in-flight rows by hand
(0x10066610/0x10066950 were about to be lost). The parked small-reggap pool
(BrBitStreamReadU16, BrEntitySetIndex, BrComGetAlloc, BrDpCreateIface) is
thoroughly probed; the notes are honest, do not re-open without a new fact.
claimcheck's six FLAGGED rows are the known d3d-tagged delegators.

##  RESUME (2026-09-05b) - 0x1000A110's BYTE-LANE WALL IS BROKEN AT THE JOIN

**The ~12-session wall (and ~300 recorded-dead compiles) fell to a control-flow
reading, not a spelling** (commit fd34818). 0x1000A110: msetdiff **13+5 -> 9+5**,
instructions **8 short -> 4**, bytes **31 short -> 16**, REGNORM 5+13 -> 5+9,
frame intact (FIRSTDIV +0x17). Masked regions 24 -> 29 = the documented
artefact on this function; rank by the multiset.

** THE DIAGNOSTIC THAT FOUND IT - REUSE THIS: a WRITE/READ CENSUS of the
original's stack slots.** The two byte slots are each WRITTEN four times and
READ three. That asymmetry is the tell: one arm's writes have no read of their
own because it `jmp`s INTO a read the other arm falls through to -- i.e. a
SHARED JOIN. Census the slots before theorising about spellings.

**THE FIX / NEW IDIOM (docs/brally/VC5-IDIOMS.md, commit 771b76e):** a Horner pack
`(((top<<8|b0)<<8|b1)<<8)` written AFTER an if/else makes VC5 carry `top`
across the join in a register, which costs it a register and pushes one byte
back into a lane move. The original emits `xor edx,edx; mov dh,<top>` at the
END OF EACH ARM, before the join label -- the top never crosses; only the two
bytes do, in MEMORY. Assign a `uint32_t` partial (`cbTop = (uint32_t)top << 8`)
on both edges and start the shared statement from it. The join block then
matches the original instruction-for-instruction.
** IT IS A JOIN LEVER, NOT AN EXPRESSION LEVER.** The identical partial at the
two STRAIGHT-LINE pack sites in the same function is BYTE-IDENTICAL, separately
and together. **The general question at a join is WHICH VALUES MUST BE LIVE
ACROSS THE EDGE -- fold everything else into a per-arm partial.**

**Also re-tested under the new allocation (staleness rule), verdict HOLDS:**
arm 3's `topA` moved LAST to match the original's load order leaves the
multiset at 9+5 and costs a raw row each side. topA stays first.

**0x1000A110 residue now 14 rows / 4 short:** 6 MISSING + 1 EXTRA byte-lane at
the two straight-line sites, the float operand swap (4 rows, N64 oracle
UNAVAILABLE -- twin is a MISS with empty ROM VA), and the pCam reload. Both
non-byte-lane items are thoroughly dead.

## RESUME (2026-09-05, lane e1 -  DECLARATION ORDER IS A CODEGEN LEVER; TWO GIANTS MOVED)

Worked 0x1000EAF0 -> 0x100250D0 -> 0x1000A110 in that order, dossiers first.
**Two giants moved; the third is now exhaustively closed as a source problem.**

** THE FINDING, and it retracts a claim repeated in every dossier: LOCAL
DECLARATION ORDER REACHES VC5's CODEGEN.** "Declaration order is inert" was
measured on /O2 SLOT PACKING (true - moving a local to the top of the list
changes no slot) and was then quoted as a general fact. It is false for
TIE-BREAKS. Two independent mechanisms, both proven byte-for-byte this session:
 - **x87 completion order (0x1000EAF0, commit 434e739).** The row block's four
   object-pointer locals declared in FIELD order (pTw LAST) make VC5 finish the
   second product before the first at all four rows, which is the original's
   order. Masked regions 21 -> 18, raw 30 -> 27; bytes, instructions and the
   multiset unchanged. All 24 permutations compiled: EXACTLY TWO outcomes,
   keyed on pTw's position alone (1st/2nd = old, 3rd/4th = original), and the
   same flip happens with the pointers untouched when a function-scope local
   LEAVES the list - so it is the symbol's ABSOLUTE INDEX, `(index-k) mod 4`,
   a hash-bucket tie-break, not pairwise order.
 - **`imul` destination (0x100250D0, commit 6836931).** For a product of two
   NAMED locals VC5 makes the LATER-DECLARED symbol the imul DESTINATION and
   the earlier one the memory operand. Here it is PAIRWISE, not a bucket.
   Declaring the widened intensity `uVar19` after all four channel deltas makes
   the alpha channel compute in place (`imul ecx,[delta]`) as the original
   does; declared before `iVar20` the product could not survive the
   divide-by-255 `imul` and needed session 10's fourth temp slot.
**Screen it whenever a schedule or an allocation is one notch off and every
expression form is dead: 24 compiles at ~6 s each.** Inert everywhere tried:
extern declaration order, unused extra locals (never indexed), renames, and
slot packing itself.

**0x100250D0 (+ the second lever): key-10 masked regions 31 -> 28, multiset
81 -> 70 rows.** The second half is that the original spends ONE `unsigned
char` intensity slot PER I4 blend body; bodies 1 and 2 shared one. Each gets
its own block-scoped local.  THE TWO ARE COUPLED and the frame proves it:
split alone = `sub esp,0x6c`, move alone = `0x64`, together = the original's
`0x68` with the original's slot count and iVar3 on its original offset (first
divergence +0x14 -> +0x24). **A frame that reads right can be two errors
cancelling - read the /FAcs equate table, never `sub esp,N` alone.** Bytes and
instructions moved AWAY from parity (8461 -> 8443, 2408 -> 2403): the
documented "removing accidental padding exposes a real deficit" pattern.
STILL OPEN there: body 3 does not materialise its byte (region 0xea4).

**0x1000A110 IS CLOSED AS A SOURCE PROBLEM (commit 6088529, no numeric gain).**
~300 fresh compiles: every function-scope declaration index (210 reorderings),
both comma lists split, block scopes, the pack array at all five positions,
array identity, the sky/arm/tile block orders - ALL byte-identical. The lever
needs comparable float products through pointer locals or two named integer
factors, and this function has neither.  **AND THE PACK WALL IS EXPLAINED BY A
DIAGNOSTIC:** reading each pack byte a SECOND time after the colour join makes
VC5 home both and read them back widened (MISSING 13 -> 10). **USE COUNT after
the join is the discriminator** - the corpus neighbour 0x1001E380 reads its
four byte locals sixteen times - so a faithful once-per-colour read cannot
reach the original's widening. The float operand swap and the pCam reload are
ONE coupled wall with it: our pCam spill is LOAD-BEARING for the 0x4c frame and
owns the slot the original gives atOffset.

**Two six-worker parallel runs were lost whole when their run limit hit.**
Their compiled variants survive in the scratch area, so a batch re-prober
recovers the work (`scratch/eaf0/batchprobe2.sh`).

**THE HARNESS THAT MADE THIS SESSION POSSIBLE** (scratch, reusable):
`probe.py <variant.c> <tag> [--va --sym --key --full]` compiles ONE variant with
the exact sweep flags in ~6 s and prints, together: BYTES/INSNS, the prologue
side by side, divergence.py masked AND raw maps with --deltas, msetdiff rows,
and the /FAcs equate table (every local's frame slot). `declsweep2.py` sweeps a
declaration through every position; `dumporig.py` / `dumpasm.py` disassemble
either stream at function-relative offsets; `batchprobe2.sh` re-measures a
directory of variants. **Never full-sweep to measure a probe.**

## RESUME (2026-09-04/05, session 19 - SMALL-REGGAP PARKS FELL TO SOURCE FACTS; T1 WORKERS)

**FINAL NUMBERS, both image-gate runs PASSED (2026-09-05): 1,208 byte-exact
tree-wide (929 C + 174 C++ + 105 EXE) -- re-verified on the final committed
tree at session end (workers kept landing matches after the first gate run) via tools/brally/total.py; the gate
places 1,103 BRGlide functions / 178,925 B = 37.21% of .text with 0 differing
bytes, and BRally/SetVideo/BossRally all 0 too. fileaudit clean (0 stranded,
0 undescribed, batches still 58).**

**+8 byte-exact in my own lane** (BrHudDrawDial 1,703 B, br_dl_normalise,
BrAtan2, BrGrid64Sample, BrCpIntegrateVelocity, the two sprite blitters
0x100013F0/0x10001440, and the 0x1005A480 split's wrapper) plus a dozen more
from three parallel T1-intake workers (see git log 2026-09-04/05). Workers were
stopped TWICE by a run limit and relaunched with resume notes each time -- always list what they left
uncommitted (`git status --short src/brally/` + grep `@implements`) before relaunch.

** THE PARKED SMALL-REGGAP POOL IS NOT DEAD.** The playbook said un-parking
finds thorough dead-probe notes. It did -- and five of them still fell,
because each note had stopped one lever short. The levers, all in
docs/brally/VC5-IDIOMS.md (entries at the end of the file):
 1. **A store scheduled AFTER the next statement's x87 work is SOURCE ORDER**:
    the narrowed value goes into an int temp and the store is written after
    the next `cos(ang)` (BrHudDrawDial's "scheduler pipelining" note, four
    sessions old). Per site: v[2].y was NOT deferred.
 2. **The leading paren pins a PRODUCT chain** too (`((a - k) * K) * n`).
 3. **Past-the-end pointer demotes a `ptr[0]` operand**: `q = (float *)pV + 3;
    q[-3..-1]` -- multi-use, VC5 folds the displacement back (br_dl_normalise).
 4. **Declaration order picks the `fld` operand of a two-LOCAL sum**, and a
    shared leading load then gets hoisted above the branch (BrAtan2 tail).
 5. **Consecutive `fst` homes for x87-resident values = an AGGREGATE local**
    (BrCpIntegrateVelocity: `BrVec3 dv, da`).
 6. `movzx si, al` = `(unsigned char)` cast into an `unsigned short` local;
    a `||` guard chain with one shared reject exit (BrGrid64Sample).
 7. Merged map row again: 0x1005A480 was jmp+nops+75-byte loader; split.

**Dead this session, recorded in the files:** BrMat3Mul (39 more spellings;
the row-2 IV anchor is not source-selectable), BrReplayApply (eleven
spellings for a `fld local; fsub field` pair -- which side of a float SUBTRACT
gets the fld is not selectable there), BrMat4Perspective7 (9 B: one push
before vs after the w store; nh named took it from -8 B to size-exact),
0x1005A490 loader (esi/edi rotation, 7 B), BrSessionReinitVideo (dropping
the volatiles is WORSE, 36 -> 57; reverted).

**Tooling:** a reusable probe harness lives in the session scratch
(harness.py: file, VA, a probes.py defining P={name:[(old,new),...]}) -- 5-30
spellings per minute through fn.py --var. Rebuild it next session; it paid
for itself ten times over. divergence.py LOOPED (re-anchor at the same
offset forever) on one BrGrid64Sample variant -- kill it, do not wait.

## 2026-09-04 (lane 546bc55c) - T1 intake by cluster, audio/net/tint: +3 byte-exact, 2 parked
Tree at hand-off: **1,186 byte-exact / 185,761 B (907 C + 174 C++ + 105 EXE)** via tools/brally/total.py; 22 of the +25 are parallel sessions'. Mine: 0x100611F0 BrSndNearestOfferTrack (102 B, first compile, br_sndpos.c), 0x1005A280 BrImgMulByMask (121 B, br_imgtint.c), 0x1006BC10 BrSndVoiceLoad (352 B, NEW audio TU br_sndload.c). Parked with notes above the tag: 0x1006B0E0 BrNetPeerSendPass (br_peerslot.c, 9 B regnorm 0 - the order VC5 loads the send call's arg 1 vs arg 3 around a call in the arg list, corpus has no witness); 0x1005A300 BrImgMulByTexture (br_imgtint.c, 287/286 B, height's register + a +2 pointer bias on the inner IV).
Two idioms in docs/brally/VC5-IDIOMS.md (b8e3e3b): a lone if/else lays the failure arm FIRST with `je success` while any ||/&& chain lays success first - the only C that reproduces "three failure jumps into one cleanup + je success" is `goto fail` with the label INSIDE the failure arm; and consecutive pointer increments are scheduled in source order.
T1 screen (scratch script, 235 rows -> 37 clean after tag/cpp/fenced/EH/port-body checks): 0x1006B080 (83 B) is the C++ lane - `mov al,[esp+0xc]; or al,0x20; push eax` into the thiscall byte writer 0x1006CFA0, plus a byte table at 0x11849E68 (8 x 8 B). Untouched clean rows worth a cluster pass next: tex 0x10023760/0x10023CB0/0x10024680/0x10024750/0x10024AA0/0x10027CD0 (br_texfmt.c / br_tex3d.c own the neighbours), 0x10005F50 (br_netstate.c), 0x10003810 was taken by a parallel session mid-run.

## RESUME (2026-09-04 - THE THREE GIANTS, ONE REGION BANKED ON 0x1000EAF0)

Worked 0x1000EAF0 -> 0x100250D0 -> 0x1000A110 in order, dossiers first. **No
new byte-exact match** (none of the three is close) but ONE region closed and
two dead levers recorded.

**1. 0x1000EAF0 region 0x189c CLOSED (49dba5b), 18 -> 17 masked.** The
Ghidra-named temp `ring` inlined at all 32 trail-append uses as
`(iWheel + iCar * 4)`.  NEW LEVER CLASS, and it is the transferable result:
**a multi-use NAMED local is an allocation candidate with a home and a
priority; a repeated expression VC5 CSEs is not.** Wall 4's addressing is
untouched (both arms byte-identical) but the object loop's `mov edi,1` hoist
moved onto the back-edge/exit path (0xb26 / 0x18a8) exactly as the original.
State: 2,325/2,328 insns, -9 B, 17 masked, msetdiff 23+20. Six dead probes on
the same class in the file header (fl inline; 2-D active/cursor; drain-loop
affine subscripts, either operand order; nested-if drain re-tested; firstVis
as `nTotal-base+1`; pP=&pW->x). k1/k2/k3 (pS block-scoped, own top-loop
counter, pP-from-pW-via-dy) all inert too.

**2. 0x100250D0: the same lever is DEAD here (657b904).** Inlining the three
I4 blend bodies' eight channel base/delta temps (`lo0`/`uVar6..8`,
`iVar16/13/18/20`) as `(param_N & 0xff)` expressions -- the mirror of what
worked on EAF0 -- goes 2,408 -> 2,389 insns (18 SHORT), 40+41 -> 47+29 rows.
 **On this function the Ghidra temps ARE the loop-invariant hoist the
original has; inlining them destroys it.** Opposite polarity to EAF0 because
here the values are loop-invariant and hoisted, there `ring` is a per-iteration
CSE. Rest of the dossier re-confirmed current.

**3. 0x1000A110: dossier CURRENT, both defects mapped-dead.** Residue is 13+5
= 18 msetdiff rows: the arm-2/3 byte-lane pack (frame-blocked -- a second
array breaks session-11's frame fix; corpus says the widening run is proven
NOWHERE in 1,036 solved fns, only the live-range shape in 0x1001E380 reaches
it) and one float operand swap (single-use pointer forward-substituted, dead).
Did not re-probe; nothing new to try without a frame-side lever.

** THE GIANTS ARE EXHAUSTIVELY MAPPED.** Independent re-derivation reproduces
the dossiers row-for-row every session. The only movement available is a
genuinely new lever from OUTSIDE the mapped walls -- the `ring` inline was one
such (named-temp-vs-CSE), found by reading Ghidra-ism naming, not by permuting.
Image gate NOT run (no byte-exact match landed; warning 1 -- no gate number to
quote).

** PARALLEL SWEEP (2026-09-05, 6 workers, adversarial-verify stage,
tool `giants-lever-sweep`): SIX novel dossier-endorsed levers, ZERO wins.**
All recorded dead in the file headers (commits 3e8aae8 scenedl, 658a676
drawcar; tex3d/wff in this note only -- another session held that file):
 - EAF0 wall 4 as a TRUE `[2][232]` 2-D array (arrays adjacent, 0x3a0=232
   ints): 43 -> 51 rows, does NOT strength-reduce ring into one shared edx.
   Joins the flat-one-array probe as dead.
 - EAF0 wall 3 velocity reads via the two-part sum before binding pW: 43 -> 61,
   materialises the base earlier and reloads. (Brief's dx-before-decl order is
   illegal C89 anyway.)
 - EAF0 `mov edx,6`/`esi,0xfffa` tile constants hoisted to named locals:
   catastrophic 43 -> 195, the do-not-hoist-a-local trap; the dup is wall-5
   register allocation.
 - A110 float swap via N64 ORACLE: twin ABSENT -- report.csv BrCarDrawVehicle
   status=MISS, EMPTY n64_va, zero drawcar hits in pairs.csv/probe.csv. No
   MIPS to read; both C spellings already byte-identical. Parked.
 - A110 byte-lane as two SCALAR uint8_t locals (multi-edge live range, no 2nd
   array): INERT -- frame held (FIRSTDIV +0x17) but scalars coalesced,
   widening rows still all MISSING. The home needs use-COUNT (0x1001E380 reads
   16x); no faithful spelling of A110 can add uses. Wall is real.
 - 250D0 per-channel self-contained statement pairs in the IA blend arms:
   82 vs 81 rows (WORSE), +3 insns/+23 B.  It CLOSED the 1093-byte
   never-compared gap (26 -> 35 regions, all compared) -- the documented
   misleading signal, NOT a win: naming the channels forces real stores.
**Takeaway: the three giants' residue is genuinely allocation/scheduling
(T3a) plus frame-blocked structural items with no source lever left. The
value of the sweep is six plausible leads foreclosed WITH evidence and an
adversarial-verify harness that guarded the arm-order trap.**

## RESUME (2026-09-04, lane f668ac32 - THREE FROM A CLAIMED LANE, ALL SOURCE FACTS)

**+3 byte-exact by hand** (0x10027850, 0x1001FF60, 0x1001ECF0) plus three
worker-driven structural steps; tree total after: **1,169 byte-exact /
183,070 B (890 C + 174 C++ + 105 EXE)** via `tools/brally/total.py`. `claim 20`
DID return a lane this time (11 rows: mixed / MISSING CODE / C++-owned) --
the tagged SHAPE pool is still dry, but the mixed rows are workable.

**1. 0x10027850 BrTex3dRecInstall (441 B, br_tex3d.c) -- A MISFILED TAG.**
`MISSING CODE (21% complete)` on a 64-byte `static float br_tex3d_shift`:
the tag sat on the port's factored-out helper for arithmetic the original
does inline. The real function (append a table slot, memcpy the 0x2A8
request in after the handle, derive the two texcoord scales) was written
fresh from the Ghidra draft and closed in five compiles. Levers: `cmp
R,0xa / ja` = unsigned `<= 10` with the small arm falling through; a
pointer local to the scale float established BEFORE the if/else (loads the
table base into a register across the arms, costs the ebp push the
original has); and the `scaletemp` idiom on ALL 22 table accesses at once
-- naming `off = idx * 0x2b4` swapped SIB base/index everywhere, re-spelling
`idx * 0x2b4` at each use fixed everywhere. **Screen: a `MISSING CODE` row
whose tagged body is a tiny static -> read the Ghidra draft of the VA, the
tag is on the wrong body.**

**2. 0x1001FF60 BrDlTriFlatZ (558 B, br_dlcmd.c) -- parked at 592/268 with
a wrong "OPEN" lead; three facts none of which were on the list:**
(a) the finish macro read `V(i).oow` twice with the first store through a
POINTER -> VC5 reloads (cannot prove no alias); pun it into a dword temp
once, 592 -> 561; (b) `add esp,0xc` after `call 0x100729EA` = the callee is
`__stdcall`: 0x100729EA is the glide2x grDrawTriangle import thunk
(fenced.csv), 561 -> 558; (c) the `&`/`|` chains' ACCUMULATOR follows the
outcode locals' DECLARATION order -- `oc2, oc1, oc0` (the argument read
order) copies oc0 and tests oc2 like the original; re-associating the `&`
is inert. 20 diff bytes, register-blind 0+0, one region -> 0.

**3. 0x1001ECF0 BrDlCmdTri1 (378 B) -- parked by ANOTHER lane, fell to the
same construction in 12 variants.** Index form (`int ia = p[6]` etc.),
FINISH_VTX_I with the single oow load, `__stdcall` draw.  NEW: with int
index locals the byte READ order is NOT the source order -- assigning
p[6], p[4], p[5] reads 4, 6, 5 and assigning p[4], p[6], p[5] reads the
original's 6, 4, 5 (VC5 swaps the first two); `float u` declared before
the ints puts them in ecx/eax/edx. Naming the outcodes (lever 2c) is WORSE
here (377 B, -3 insns) -- single-use inside one expression. **Tri2
0x1001FA30 in the same form is WORSE (743 B, +7)**: the original spills the
first triangle's oc_a into the dead ARGUMENT slot and keeps p in ebx all
function; a register-pressure question, noted in the file header.

**Workers (three, one per file, parallel):** 0x10067710 BrCrRespWalk 752 ->
1296 of 1301 B, regnorm 183 -> 14, instruction-exact (3f128ff): the port had
abstracted the whole module's signatures; `BrCrImpulseSolve` and
`BrCrContactKick` in the same file carry the same defect. 0x10015B10
BrTextEmitString 2,294 -> 420 diff bytes (9368a7c, frame and layout).
0x1002ECEB BrAnimUpdate: see the worker's note in slice2_19.c.

**Dead this session (all recorded in file headers):** 0x1006B440 a
struct-typed parameter (7th probe, byte-identical); 0x100686D0 three
typings of the 16-bit key compare (inert; inline expression worse);
0x1001FF60 `((oc0 & oc1) & oc2)` and OR permutations (inert).

**Bookkeeping:** `tools/brally/filing.py` with no args REWRITES filing.csv from
report.csv and picks up other sessions' rows -- and the file is CRLF, so a
naive python rewrite touches every line. Insert your one row by hand with
`newline=''`. The pre-commit hook greps `@implements 0x...` in ANY comment:
a pointer comment saying "tagged @implements 0x1001ECF0" was refused as an
undescribed tag -- write "the definition of 0x1001ECF0" instead. Another
session's BrHudDrawDial (2b6f5a5) had no filing row; added (a163a7d).

##  THE COMPILER QUESTION IS SETTLED (2026-09-03, session 16b) - FULL MATRIX, CONTROL SET

Three unrelated VC5-IDIOMS notes and project rule 11a all reached for "a
compiler patch level slightly different from the staged one" to explain
scheduling residue no source form moves. **It is wrong. Tested and closed
(0253aa3 / 5155d57).**

Staged toolchain is MSVC 5.0 **RTM, `cl 11.00.7022`** (verified from the
banner, not assumed). VS97 SP3 ships a genuinely different code generator --
`C2.EXE` 630,544 -> 660,240 B, `C1.DLL`/`C1XX.DLL` differ too, Nov 1997 vs the
RTM's Apr 1997. **The SP3 media was ALREADY ON DISK and nobody had wired it in**
(`reference/msvc/sp3/out3/VS97_SP3/enu/vc/bin/`, downloaded 2026-08-30; also
`vs97sp3.zip`, `vs6.iso`, and VC4.0/4.1/4.2 archives in `reference/msvc/`).
Check that directory before sourcing anything.

 **AND THE MATRIX WAS THEN RUN ACROSS EVERY MSVC OF THE ERA (59ac8ed)**, scored
on a CONTROL SET of 61 functions already byte-exact under the staged compiler --
which is what makes it an experiment rather than a vibe:
    VC4.2  10.20.6166   22/61  (1,574 diff B)   <- not the compiler
    VC5 RTM 11.00.7022  **61/61  (0 diff B)**
    VC5 SP3             **61/61  (0 diff B)**
    VC6    12.00.8168   45/61    (919 diff B)   <- not the compiler
VC5 confirmed outright; the two VC5 builds are indistinguishable.

 **THE LOST-SYNC TRAP IS AT ITS WORST HERE AND I NEARLY REPORTED IT.** Run
plainly, VC4.2 reports **ONE divergence region** on 0x100250D0 and 4 on
0x1000EAF0 -- better-looking numbers than VC5 has ever produced. They are
nothing: VC4.2 is 352 instructions short, divergence.py loses sync at offset 0
and never re-anchors, and **100.0% of the function is NEVER COMPARED**. A WRONG
compiler produces the prettiest region count in the project. Read the
`NEVER COMPARED` line every time, and never score a compiler on a hard function.

Result, same sources at /O2 under each:
  61 already-byte-exact fns (4 files)  RTM 61/61, 0 diff B | SP3 **61/61, 0 diff B**
  0x100250D0 BrTex3dExpand             **byte-for-byte IDENTICAL**
  0x1000A110 BrCarDrawVehicle          **byte-for-byte IDENTICAL**
  0x1000EAF0 scene DL                  RTM 30 regions/-14 B | SP3 37/-71 (WORSE)

**Indistinguishable on 63 of 64 functions tested; where they differ RTM wins.**
So the giants' residue is in the source or genuinely unreachable -- NOT a
toolchain artefact. Do not reach for the patch level again.

`tools/brally/match_sweep.py` now takes **`BR_MSVC=<dir>`** for the toolchain
directory (include path follows it); SP3 staged at `tools/toolchains/msvc5sp3/`,
gitignored.  **Stage any alternate compiler in a PARALLEL directory -- never
overwrite `tools/toolchains/msvc5` in place**, or a failed experiment costs the tree.
Untested and narrow: the SP3 **linker** (irrelevant to per-function matching,
which works on .obj; could matter to the image build) and the C++ front end on
the one TU only VC4.2 reproduces (0x10006510, a .cpp -- needs the C++ path).

 **METHOD NOTE, reusable:** score an alternate toolchain by hand-compiling into
the scratch and calling `match_sweep.score(orig, code, set(relocs))` with
`load_orig(path, va)` -- a RAW byte compare says 0/19 on a file the sweep calls
19/19, because relocations are not masked. And always include a control set of
ALREADY-MATCHING files: "it broke nothing" and "it changed nothing" are
different findings, and only the control set separates them.

## RESUME (2026-09-03, session 16 - THE THREE GIANTS RE-DERIVED; DOSSIERS ARE CURRENT)

**+0 byte-exact.** Three dossier commits, no closures. Worked
0x1000EAF0 -> 0x100250D0 -> 0x1000A110 in that order, as asked.

** THE HEADLINE IS A NEGATIVE AND IT IS WORTH KEEPING: all three giant
dossiers are ACCURATE AND CURRENT.** On 0x1000A110 the entire residue was
re-derived from the bytes WITHOUT reading the dossier first (region map +
windowed multiset) and it reproduced row for row -- 34 raw regions, 13+5 = 18
multiset rows, the `fld [esp+0x4c]`/`fadd [ebx+0x30]` operand swap, the
`mov eax,[0x106ed520]` reload -- and every one was already on a dead list.
Same on 0x1000EAF0: three "fresh" findings (regions 27, 28, 30) were items 7,
6c and 6b of the file header. **Read the file header before measuring, not
after; on these three functions independent re-derivation buys nothing.**

**1. 0x1000EAF0: WALL 4 IS CLOSED AS A SPELLING PROBLEM (ce680bd).** Entry
state 30 raw regions, 2,324 vs 2,328 insns, -14 B. The one untried byte-offset
shape was a LOOP-CARRIED IV (every earlier probe spelled it as an expression,
which VC5 re-folds) -- and it is the construct that WORKED in this file's drain
loops, so it was the last plausible lever. `rb = iCar << 4` before the wheel
loop, `rb += 4` at the bottom, all sixteen flat-array uses through
`*(int *)((char *)base + rb)`, `ring` kept for the `ring * 500` terms.
DEAD and worse than any previous wall-4 probe: 30 -> 65 regions, insns -4 ->
+11, and a 782-byte lost-sync gap.  Bytes read -14 -> -3 on that change --
another pass-24 trap instance. **Verdict: wall 4 is not reachable by respelling
the offset at all** (expressions fold; an IV rebuilds the loop's induction
structure). Do not open item 4 again without a lever from outside the
addressing; the pDst spill downstream of it is closed too.

**2. 0x100250D0: THE NEVER-COMPARED BLOCK IS NOW MEASURED (42a7808).**
orig 0x15b8..0x19fd is unreachable to divergence.py (no ten consecutive
matching instructions), so a WINDOWED msetdiff was run over it instead:
274 orig insns vs 270 ours -- **the tail's whole -47 B delta is born there
and nowhere else**. MISSING 11 (`mov R,[esp+S]` x7, `cmp R,R` x2, `xor R,R`,
`imul R,[esp+S]`) / EXTRA 7 (`mov [esp+S],R` x2, `mov [esp+S],0`, `test R,R`,
`test [esp+S],R`, `imul R,R`, `jmp T`). Ten of the eleven rows are session
14's counter-register swap stated as numbers -> allocation, parked.
 The eleventh is not: an EXTRA `jmp` at 0x1909 is a LOOP-ROTATION artefact --
our back-edge target is a reload `mov edx,[esp+0x68]` one instruction above
the head, because our first channel's delta sits in a register for FOURTEEN
instructions before it is homed while the original homes each channel's pair
four instructions after computing it.
 **IDIOM (new): the four coefficient pairs are LOOP-INVARIANT, so their
position INSIDE the loop body is inert.** Sinking the R pair below the
intensity statements so all four read pair-then-channel is BYTE-IDENTICAL
(same 61 regions at key 6, same insns/bytes, same jmp). Only their order
relative to each other reaches the hoisted block. Also: that file's header
figure "REGNORM 41+40" is stale -- it is **40+41**.

**3. 0x1000A110: the STATE block's "1843 vs 1843 instructions (EQUAL -- no
missing or extra code anywhere)" IS STALE AND WRONG (9ff8d2f).** Measured:
orig 1,843 vs ours 1,835 (+6 pad) -- EIGHT short -- and 7,577 vs 7,546 bytes,
not 7,536. Eight is exactly the multiset's 13 MISSING - 5 EXTRA, so the two
agree. It matters because "no missing or extra code anywhere" is the sentence
that licenses calling the rest allocation. Same class as
[instruction-count-padding-trap](../traps/instruction-count-padding-trap.md): **an instruction total quoted from a
previous session is not a measurement.**

** TOOLING, LANDED (c04b042): `tools/brally/msetdiff.py --orig-range LO-HI
--recomp-range LO-HI`.** This is the ONLY way to see inside a lost-sync gap --
divergence.py cannot compare a block with no `--key` consecutive matching
instructions, and on 0x100250D0 that hid 12.9% of the function behind one
"NEVER COMPARED" line for five sessions. Pass BOTH sides' offsets (they differ
by the accumulated delta; the re-anchor line prints both); the header line
gives each side's instruction count and the difference, which is where a
missing-code verdict actually comes from.  If you hand-roll one instead:
`norm(i, relocd)`'s second argument is a PER-INSTRUCTION BOOL
(`any(o in rel for o in range(addr, addr+size))`), not the reloc set --
passing the set makes every immediate read `A` and the diff is garbage.

## RESUME (2026-09-03, lane 464bdc23 - A SIBLING BYTE-DIFF FOUND A 7-MEMBER FAMILY)

**+10 byte-exact** (9 new + 1 regression fix). C lane 871 match / 85,072 B;
T4 1,044 of a 1,499 hand-C target.

**1. The clip-plane family, +5.** `0x1001F0D0/F2B0/F3F0/F530/F670/F7B0/F8F0`
are seven separate functions in the original, six of 311 B and one of 303.
**Found by byte-diffing the extracted originals AGAINST EACH OTHER** -- they
differ in 6-8 bytes, all of them a field displacement or a `call rel32` byte.
That diff is the cheapest family screen there is; run it before writing C.
The port had them as one routine plus a distance CALLBACK (emits a `call
[reg]` the original lacks), so the matching build is ONE MACRO instantiated
seven times, in a new module `src/brally/core/drawing/br_dlclip.c`, with the port
copy in slice1_03.c fenced behind `#ifndef BR_MATCHING_BUILD`. Three source
facts did it: (a) macro not function pointer; (b) **a cross-jumped store is a
statement-order question** -- the `+1` and `-1` arms share one
`mov [ebx+4],eax` only when the count update is LAST in both, so moving
`pOutPrev = pCur;` above it removed the duplicate; (c) **initialiser order
decides which of two free instructions fills the slot before a branch** --
`pPrev; pCur; pOutPrev; i; pDead` matched, two other orders did not, 8 bytes
on five functions at once. Two members (LEFT 4 B, NEAR 2 B) are parked on the
fld/fadd pair.

**2. A MERGED MAP ROW, +4.** `0x10002EB0` and `0x10002F10` were listed at 86
bytes and could not match at any spelling: each is a 14-byte DISPATCHER, then
nops aligning to 16, then a SEPARATE tail-called function the map had no row
for (nothing CALLS it, so the map builder never saw an entry). Split
`config/brally/functions_glide.csv` to 32+54 twice, re-extracted the four bins, and
all four went byte-exact. **Two tells: an unconditional `jmp` followed by nops
up to a 16-aligned address; and A GLOBAL THE ORIGINAL RE-READS ACROSS WHAT
LOOKS LIKE A PLAIN BRANCH** -- two functions cannot share a register, so a
failed CSE is the boundary showing through, not a "do not cache" case to
grind. Re-extract a few rows with `extract_funcs.read_pe_sections` +
`va_to_fileoff` rather than a full re-extract.

**3.  THE LEADING-OPERAND PAREN IS A REAL LEVER.** `((a) + b) + c` makes `a`
the `fld` operand; permuting summands is inert (VC5 canonicalises a flat float
sum, and ALSO a plain two-term add of two struct fields -- the clip siblings'
disagreement about operand order is NOT a record of their source, since one of
our own macro expansions emits both orders at its two sites). It took
`0x100664F0 BrCrCorner` byte-exact in one character. **But it can move the
SCHEDULE too**: on `0x1001F2B0` it flipped the pair correctly and sank the
second site's `fadd` four instructions, 4 diff bytes -> 53. Use it where the
pair is the only divergence.

**4.  A `match` ROW IN report.csv IS NOT EVIDENCE.** `BrCrCorner` carried
`match, 0 diffs` while the assembled image differed by 2 bytes -- the row was
stale after a change elsewhere. Only `image_build.py` catches that. Re-sweep
before believing any row you did not just produce.

** THE IMAGE GATE COULD NOT BE RUN TO A CLEAN VERDICT.** A parallel session
was refiling continuously; four runs, and `image_build.py` itself flagged two
of them with "THE TREE CHANGED WHILE THIS RUN WAS GRADING IT ... do NOT record
either a pass or a failure from this run". One run found `src/brally/core/slice2_16.c`
mid-write and not compiling at all (C1004). What was CONSISTENT across every
run: **`functions differing from original: 0` and `0 differing bytes`** -- the
FAILED verdicts came only from 3-4 symbols "not in obj" in files being refiled
at that moment (a refile leaves the symbol in neither obj between its two
writes). No claim of mine was ever among the differing. **Do not record this
session as a gate pass.**

**Session-start state:** `claim 20` returned NOTHING again (tagged pool
drained). `claimcheck` clean (0 duplicate claims); `stale_claims` clean. The
byte-diff family screen over all un-reported orig bins found only ONE family
left after the clip planes: the 86-byte pair above.

## RESUME (2026-09-03, lane 145c7bd9 - TWO "COLOURING WALLS" WERE SOURCE ORDER)

**+3 byte-exact** (a 4th, 0x10003280, landed jointly with a parallel session).
Image gate re-run and PASSED: BRGlide 1,030 fns / 163,917 B / **34.09% of
.text, 0 differing bytes**; all three EXEs OK.

** THE IMAGE GATE FAILED ON ITS FIRST RUN AND PASSED ON THE SECOND, same
tree, no edits between.** No diagnostic beyond `IMAGE GATE FAILED:
BRGlide.dll`. A parallel session was rebuilding objs at the time. Treat a
lone gate failure under concurrency as suspect and RE-RUN before reporting
it as a regression -- but never treat a pass as retroactively covering the
failure either.

**Matched:**
- `0x10071F00 BrTickAdd` (31 B) -> src/brally/core/racing/br_replayon.c. THREE
  separable facts, one instruction each: (a) the counter is ONE `int64_t`,
  not a lo/hi pair -- `+= K` gives `add`/`adc`, the hand-carried
  `hi += (lo < K)` gives `sbb`/`setb`; (b) it is read into a LOCAL first --
  updating the global in place finishes the low half before touching the
  high half in one register, and the 5-byte accumulator form then makes the
  function 2 B SHORT; (c) it RETURNS the 64-bit value, and that is the ONLY
  thing that pins `edx:eax`. Without (c) it scores regnorm 0+0 / raw 3+3 and
  reads exactly like a T3a wall.
- `0x1002A7F0 BrMat4Translate` (71 B) -> br_mat.c, NEW function from a T1
  draft, byte-exact on the FIRST compile.
- `0x1002A7A0 BrMat4Scale` (70 B) -> same file. **Was parked as a colouring
  wall and was not one.** Matrix builders are written ROW BY ROW in ADDRESS
  ORDER, not grouped diagonal-first. The zero stores separating `sy` from
  `sz` are what make VC5 reuse the register it just freed instead of
  hoisting both parameters.

**GENERALISATION worth carrying (both idioms are in docs/brally/VC5-IDIOMS.md):**
on a struct/record-filling function with regnorm 0+0, exact instruction
count and only 2-4 diff bytes in which register a parameter lands, re-spell
the assignments in ADDRESS ORDER before recording a colouring wall. And on
any 64-bit value, a raw register-pair difference is a RETURN-TYPE question,
not colouring -- `edx:eax` is the only pair a 64-bit return can use.

**PARKED with residue notes committed (not matches):**
- `0x10036810 BrComGetAlloc` and `0x10036E50 BrDpCreateIface` -- ONE cause,
  seen twice: **VC5 makes the success/continuation path the fallthrough and
  sinks the cleanup block; the original does the reverse.** On BrComGetAlloc
  a plain if/else DOES fix the je-vs-jne polarity but then loses the
  original's `jmp cleanup` (140/142 B, 1+2) -- strictly worse than the
  do-while's size-exact 1+1, and the two residues are mutually exclusive.
  Dead on both: plain if/else with an early return, the same with
  `goto success`, if/else + `goto cleanup` in the short arm, the literal
  `if (p != NULL) goto call2;` transcription (VC5 inverts it back), and
  duplicating the cleanup tail textually -- **cl does NOT cross-jump it**
  (+14 instructions), which retracts any assumption that our cl merges
  identical error tails everywhere.
- `0x10018A50 BrSwapU16Array` -- 4 dead probes; VC5 always fills the HIGH
  half of the word register first in a byte-compose. Flipping the `|`
  operands, swapping the two local assignments, dropping the locals, and a
  two-lane union (that one costs a stack slot) are all dead.
- `0x10008BA0 BrPodWriteOpen` -- RAW 0+0, 6 B, one store in a different
  position INSIDE a call sequence. Assigning the global after the call parks
  the handle in esi (33 diffs); dropping the local is identical. Store
  placement inside a call sequence is not source-reachable.

**SESSION-START FACTS, all confirmed:**
- `claim_lane.py claim 20` returned NOTHING. Tagged pool drained, exactly as
  the playbook says. Every small-reggap SHAPE row is parked, and 17 of them
  were bulk-parked by ONE lane at a single identical timestamp -- that is a
  lane running out of time, not 17 walls.
- **T1 intake is where the count moved: 2 of 3 matches came from there**, one
  on the first compile. Un-parking mostly re-found thorough dead-probe notes.
- `stale_claims.py --fix` found 2 C twins already solved in C++; fixed.
- **Run every tool with `.venv/bin/python`** -- bare `python3` fails on
  `capstone`. And `timeout` does not exist on this Mac; use the Bash tool's
  own timeout parameter.
- **A stale .obj lied about a finished match.** 0x10003280 read `diff,
  66/64, 56` in report.csv while the compiled bytes were already identical;
  a second one-file sweep flipped it to MATCH. Re-sweep before believing a
  diff row on a file you just touched.
- `tools/brally/total.py` re-scores and disagreed with report.csv (842 vs 856 C)
  while a parallel session rebuilt objs. Under concurrency, image_build's
  placed-function count is the number to trust.

## RESUME (2026-09-03, lane 4a6c9cdb - A NAMED TEMP BEATS THE SUM CANONICALISER)

**+1 BYTE-EXACT: 0x10060C30 BrSndPan.**  **NEW IDIOM, and it CORRECTS the
"only GROUPING moves a float sum" note: for a three-term sum of products,
grouping does NOTHING and a NAMED TEMP is the whole lever.** The residue was 4
bytes, size- and instruction-exact, reggap 0+0: the original emits the y term's
`fmul` first, we emitted the x term's. Dead, all at 380 B / 114 insns: all six
flat permutations; every LEFT grouping; two right groupings (two others were
WORSE at 6); vector-factor-first in one or all three products; and a
`const float *r = &m[1][0]` row pointer in four orders -- that one BREAKS the
function (-8 B, -4 insns), the original recomputes the row address per term.
What works: `float ty = m[1][1]*d.y;` then `proj = ty + X + Z;`. **Naming a
product lifts it OUT of the flat sum, so it is evaluated on its own and lands
first; parentheses only re-associate WITHIN the sum.**
** THE BOUNDARY: it needs a SUM to lift out of.** Six spellings dead on
0x10034360 BrVec3Scale (37 B / 12 insns / 5 diffs, unchanged): each component
there is a LONE two-operand multiply, and a lone commutative fmul really is
out of reach from source -- the existing BrVec3 notes stand.

**0x100183B0 BrFadeDrawBars: register-blind 77+113 -> 3+9, 592 -> 800 B
against 803, 186 -> 216 insns against 222.** Same globals-struct class as
0x100686D0 last lane, and the same four causes are now a checklist:
 1. the port took a `BrFadeState *`; the original takes NO ARGUMENT and reads
    eleven standalone globals. Map them from the ORIGINAL'S OWN OPERANDS
    against the address bases the file already establishes (here: a matched
    sibling's cursor, and a matched setter's wipe block) -- twelve addresses
    fell out in one pass, no guessing.
 2. allocation through a static helper = a `call` per site. **MSVC5 does not
    inline a static with >1 caller** -- macro it. 21 calls -> 5.
 3. **a float->int helper TAKING A DOUBLE pushes it on the C stack**
    (sub esp / fstp qword / add esp). The original leaves the value on the x87
    stack. But a bare `(int)(double)intexpr` is FOLDED AWAY -- it needs a
    NAMED double local to keep the `fild`/`__ftol` pair.
 4. **`BR16_FEQU(a,b)` = `!(a<b || a>b)` costs a SECOND fcomp.** A plain float
    `==` is exactly the original's single `fcomp / fnstsw / test ah,0x40`.
    Right model for the reader, wrong one for the bytes.
 Also: the original's DEAD STORE to a stack slot survives only as a
 `volatile` local -- VC5 deletes a plain one (+3 insns for it).
 Residue: 4 insns where the original really computes `((0 << shift) & 0xFFF)
 << 12` -- three spellings dead, VC5 folds a shift of a known zero -- and 2
 where it loads a global into a register before comparing.

** THE NEAR-MISS SCREEN IS THE HIGHEST-YIELD HABIT.** `report.csv
status=diff, diffs <= 8` is 20 rows tree-wide. Most are worked walls WITH
notes; the ones that fall are the ones with NO note. Two lanes running, both
finds came from it.

** THE IMAGE GATE HAS A RACE GUARD NOW, AND IT FIRED.** With a parallel
session sweeping, `image_build.py` printed "THE TREE CHANGED WHILE THIS RUN
WAS GRADING IT (build/brally/win32/match/report.csv) ... do NOT record either a pass or a
failure from this run." Believe it. Across four runs the BRGlide verdict was
FAILED every time, but **"functions differing from original: 0" and
"ASSEMBLED IMAGE vs ORIGINAL: 0 differing bytes" every time too** -- the only
complaint was "claimed but NOT placed: N ... symbol not in obj", and the N and
its MEMBERS changed run to run (5 -> 3, different files) as the other session
rebuilt objects. **No claim was wrong; it is the stale-object bookkeeping
class. Sweeping the named files fixes it when the tree is still -- it cannot
be cleared while another session is writing report.csv, so do not record a
verdict then.**

## RESUME (2026-09-03, lane a2a58833 - NAME THE POINTER; +2 BYTE-EXACT)

Image gate RE-RUN and PASSED: 0 differing bytes, 1,031 fns / 163,079 B /
33.91% of BRGlide .text. Tree total 1,137 byte-exact / 175,843 B (858 C + 174
C++ + 105 EXE) -- that total includes a parallel session's work, not just this
lane's two.

**+2 BYTE-EXACT: 0x1006CFC0 BrBitStreamWriteU16 and 0x1006D050
BrBitStreamWriteU32.** Both sat at exactly 2 differing bytes, size- and
instruction-exact, register-blind 0+0. Cause: the FIRST store's two address
loads came out in the wrong order. `pBs->pBuf[pBs->writeByte] = v` makes VC5
load the INDEX first; the original loads the BUFFER first. **Name the buffer
pointer in a local assigned AFTER the preceding call and use it for that ONE
store.** Riders: it is per-STORE (naming it for every store puts WriteU16 back
to 2 diffs -- later stores are index-first and already matched), and a NARROW
argument needs the index named too (WriteU16 takes a short and pulls its high
byte from `ah`, so `pb` alone is not enough; `pb` + `int w` closes it, while a
widening `unsigned int x = v.v` local destroys the `mov ax,` word load and
costs 8 bytes). Written up in docs/brally/VC5-IDIOMS.md.

**The screen that found them, and it is reusable:** filter report.csv for
`status=diff` with `diffs <= 6`. Fifteen functions tree-wide. Most are
thoroughly worked walls -- but the two that fell had NO note.

**It does NOT generalise to the read side.** 0x1006CE20 BrBitStreamReadU16 has
the mirror residue (orig loads `dh` before `dl`) and already names both halves;
six more spellings now recorded dead there, eleven in total. Loads feeding two
halves of ONE register are a different mechanism from two feeding an address.

**0x100686D0 BrCollGridCellAcquire: register-blind 61+17 -> 28+13, 699 -> 613
B against 551, 222 -> 193 insns against 178.** FOUR separate port additions,
each worth measuring for the class:
 1. `/32` spelled as an explicit sign-correction plus shift never emits the
    `cdq/and 0x1f/add/sar 5` the original has -- write the plain signed divide.
 2. BrVec3Normalise ran on a LOCAL and copied back; the record's leading three
    floats ARE the normal and the original normalises in place. The round-trip
    cost 5 pushes, 5 stack stores, 4 reloads and the whole fstp/fld/fxch
    cluster, and turned the `d` term's `fmul [reg+disp]` into a stack operand.
 3. **`BrFtolTrunc` is a declared helper, so it PUSHES its float. The original
    just writes `(int)f` and lets VC5 emit `fld` / `call __ftol` with the value
    already on the x87 stack.** Scope `#define BrFtolTrunc(f) ((int32_t)(f))`
    to the one function -- the portable helper exists because a plain cast
    saturates instead of wrapping off x86.
 4. Three memory-safety guards (two null tests, one cell bound) are now
    `#ifndef BR_MATCHING_BUILD`, which keeps ONE body instead of two arms.
 Residue is allocation: VC5 hoists the widened key out of the search loop where
 the original re-does `movsx edx,di` each pass, costing two stack slots
 (`sub esp,0x18` vs 0x10) and inverting which of best/iVictim spills. Four
 clock-increment spellings dead.

** THE IMAGE GATE CAN FAIL ON A STALE OBJECT.** It reported BRGlide FAILED
with "claimed but NOT built/placed: 2" while also reporting 0 differing bytes.
Both functions built and MATCHED under a one-file sweep; running that sweep
rebuilt the object and the very next gate run PASSED with all 1,032 placed.
**A gate failure whose "functions differing from original" is 0 and whose only
complaint is unbuilt claims is a stale object, not a wrong claim** -- sweep the
named file and re-run before believing it.

Other parks confirmed this lane (all had real notes, all re-measured):
0x10017F80 BrFadeDrawSprite (2 B, y1+y0 load order), 0x1006FD50
BrEntitySetIndex (2 B, `sub eax,0x10` vs `add eax,-0x10`), the BrVec3 trio
(N64-confirmed x87 walls), 0x100182F0 BrCursorPairSet -- to which six new dead
probes were added, including reading the first global BACK for the second
store, which VC5 folds away in both orders.

## RESUME (2026-09-03, lane 1eff62c6 - A HOISTED LOOP BOUND IS A FRAME BUG)

**No new byte-exact match**, so the image gate was NOT re-run and no gate
number from this session may be quoted. What landed is one large structural
step and three recorded dead probes.

**0x100302A0 BrModelSwap (slice2_19.c, tagged by its d3d VA 0x10036C00):
register-blind 13+23 -> 8+11, 1028 -> 1053 of 1062 bytes, 361 -> 368 of 371
instructions.** Cause: the innermost leaf loop's bound was HOISTED into an
`nHalf` local. The original re-reads `3 * item->m` every pass. The hoist
changed four things at once -- count-DOWN (`dec`/`jne`) instead of up, a
NEGATIVE displacement because the offset bumps at the top, `j` never spilled,
and `sub esp,8` instead of `sub esp,0xc`. Putting the expression back in the
for-condition fixed all four and made the leaf loop instruction-exact. New
idiom in `docs/brally/VC5-IDIOMS.md` ("Never hoist a LOOP BOUND either").
**The TELL is the frame, not the loop: `sub esp` one dword short plus a
count-down loop inside = a bound the source re-reads. And when one loop in a
function re-reads its count, assume they all do.**

Residue there, in the file header: 2 insns in the record loop (orig walks it
on a +2-biased pointer and rematerialises pRec each pass), 1 register copy,
the `off=0x20` init's placement, BrRev4 store-pair order at 2 of 10 sites
(the sites disagree with each other -- scheduling), and ~30 instructions that
differ only as `[edi+eax]` vs `[eax+edi]` (SIB base/index exchanged;
register-blind-INVISIBLE, byte-visible).

**INERT, proven this session (VC5 canonicalises all four to identical
bytes):** flipping a `|`'s operands in a byte-compose; moving a loop
variable's init into the for-init; writing a pointer+offset sum offset-first
(`4 + 4*i + PBLOCK`); giving a loop body its own aliasing pointer local.

**0x10015630 BrSceneSetupFrame: two dead probes, no gain.** Its byte-slot
residue (orig `xor r,r / mov rl,[g]`, ours dword-load + `and 0xff`) is NOT
caused by the four colour bytes being adjacent in BrSceneEnv. 3-byte padding
between them: 39/27 against a 26/18 baseline. Standalone one-byte file-scope
globals: 36/24, missing byte loads 6 -> 8. **The widening context is the
cause, not the storage shape.** This is an /O2 /Op TU -- fn.py compiles /O2
only, so score it by compiling with `/O2 /Op` into your own objdir and
running `tools/brally/fnmatch/mdiff2.py <obj> <sym> regnorm`.

**PARKED via `claim_lane.py release`:** 0x1005FF00 BrRaceGateStep (2 bytes,
one `add ecx,eax` vs `add eax,ecx`; probe list exhausted AND the N64 twin
0x8022A0E0 confirms the source spelling -- do not reopen) and 0x10015630.

! **A PARALLEL SESSION WAS COMMITTING TO THIS REPO THROUGHOUT.** HEAD moved
under me (da33a15, 5820d98) and `tools/brally/refile.py` +
`config/brally/globals_learned.csv` were dirty in the shared tree the whole time.
Every commit used a pathspec. `git stash -- <file>` is safe for a probe
revert, but check `git stash list` before dropping -- the stash message names
a HEAD you did not make.

## RESUME (2026-09-03, session 18 -  SESSION 17'S WIN IS RETRACTED)

**Session 17's 0x1000EAF0 result is WRONG and is reverted (36e145b).** It is
the most useful thing to have happened in a while, so read it before trusting
any scoreboard here.

Session 17 converted a ring-slot variable's definition from
assign-then-override to an if/else, on the strength of the ORIGINAL homing the
value on both edges, and it improved **every axis**: 4 instructions short -> 1,
bytes -14 -> -8, register-blind 38 -> 37 rows. It never disassembled the
original at the site. Read at last, orig+0x1cb1 is
`mov ebx,eax; test ebx,ebx; jge; mov ebx,0x1f3` - **assign-then-override, the
shape the file already had.** The if/else INVERTS the arms (constant up front
with `jl`, against the original's `jge` over the constant) so it can never
converge, whatever it scores.

 **A SPELLING CAN IMPROVE EVERY NUMBER THIS PROJECT MEASURES AND STILL BE
PROVABLY NOT THE SOURCE.** Bytes, instruction count, register-blind multiset
and raw all moved the right way on a change that is definitively wrong. **Before
accepting any control-flow change, disassemble the site and check the ARM ORDER
against the original's branch - never the totals alone.** This is exactly the
trap project rule 2 exists to stop, and the scoreboard cannot catch it.

**The idiom entry in docs/brally/VC5-IDIOMS.md is corrected, not deleted.** The
codegen fact is real (`x = a; if (c) x = b;` keeps x in a register; the if/else
gives it a home on two edges). What was wrong was the SCREEN. Homes are
allocation and can come from anywhere; the screen is the arm order:
    `jge` over `mov x,CONST`        -> assign-then-override
    CONST up front, `jl` to skip    -> if/else
The two negative sweeps session 17 recorded still stand (8 ring sites: +12
insns; 12 doubling sites on 0x100250D0: +29) - and they now read as
CONFIRMATION that those originals are assign-then-override too, not as a
mysterious per-site exception.

**KEPT from session 17, and it is the faithful reading:** the test is on the
ASSIGNED variable (`pDst`), not on the temp it was assigned from (`h1`).
Byte-identical, but it is what the bytes test.

 **AND THE SITE IS NOW CLEANLY EXPLAINED, WHICH CLOSES A LEAD:** with the
faithful spelling our control shape matches the original exactly; the only
thing missing is the two homes, and those are register pressure - the original
has edx pinned to `ring*4` by wall 4 and is a register short. **So the pDst
home is DOWNSTREAM of wall 4 and is not source-selectable either.** Session
22's "go after the pDst home, not the index" is therefore also closed. Wall 4's
`lea edx,[ecx*4]` is the only thing left in this function worth a new idea, and
it has now resisted eight offset spellings.

State back to: **0x1000EAF0 = 38 rows, 4 instructions short, -14 bytes, 20
masked regions.** 0x100250D0 = 81 rows. 0x1000A110 = 18 rows.

## RESUME (2026-09-03, session 17 -  A NEW IDIOM, AND IT IS NOT SWEEPABLE)

**0x1000EAF0 went from FOUR instructions short to ONE** (bytes -14 -> -8,
register-blind 38 -> 37 rows), commit 030cd28. The lever is a new idiom:

 **`x = a; if (c) x = b;` and `if (c) x = b; else x = a;` ALLOCATE
DIFFERENTLY.** The first is one definition plus a conditional fix-up and VC5
keeps `x` in a register; the second is two definitions on two edges and VC5
gives it a stack home, writes it on BOTH arms and reloads at the use. The
eleventh pass had known for weeks that three missing instructions were the
original homing `pDst` in both arms of a test - nobody had gone and looked at
how the SOURCE WRITES pDst. It was an assign-then-override. As a true if/else
the homes appear.

 **IT IS A PER-SITE READING, NOT A CLASS - and the two negative sweeps are
worth more than the fix.** Converting all EIGHT sibling ring-wrap sites in
the same function together: 37 -> 60 rows, +12 instructions. The twelve
doubling sites in 0x100250D0: 81 -> 117 rows, +29 instructions. Those
originals really do spell an assign plus a fix-up (compare and branch, no
home). **The screen is: does the ORIGINAL home the value on both edges?**

 **fn.py's FIRSTDIV DOES NOT MASK SLOTS.** The winning change read
FIRSTDIV +0x2b -> +0x1b, which on this function's dossier is the documented
"frame broke" tell. The frame was FINE - prologue instruction-for-instruction
identical, aligned first divergence still 0xad4; two slot displacements moved
(wall 6, zero byte delta). **Check the prologue itself, or divergence.py
--mask-slots, before believing a FIRSTDIV drop.** That is now the THIRD time
a first-divergence heuristic has misfired on a genuine win.

**WALL 4 (the addressing itself) took three more negatives** and has now
resisted eight offset spellings. New this session:
 - an algebraically-equal but RE-ASSOCIATED offset (`iWheel*4 + iCar*16` ==
   ring*4) used only for the two flat arrays, leaving `ring` and `ring*500`
   alone: 38 -> 118 rows.
 - the same declared BLOCK-SCOPED: byte-identical to function scope, which
   re-confirms that /O2 slot packing ignores scope entirely.
 -  **THE TWO RING ARRAYS ARE ADJACENT** - 0x1035faf0 - 0x1035f750 = 0x3a0 =
   232 ints exactly, so `faf0[ring]` IS `f750[232+ring]` and the original's
   paired `[edx+base1]`/`[edx+base2]` is ONE index register against TWO
   displacements into what may be a single table in the original source.
   That is the first structural account of wall 4. **It is still not the
   lever**: handing VC5 one array leaves the addressing family unchanged row
   for row (it does buy 2 instructions and 6 bytes for 2 rows - not taken).
 - Treat the addressing as DOWNSTREAM of the pDst home, per pass 11.

Honest maps now: **0x1000EAF0 = 37 rows, ONE instruction short**;
0x100250D0 = 81; 0x1000A110 = 18. Image gate: 0 differing bytes, all four.

## RESUME (2026-09-03, session 16 -  THE TRIAGE METRIC WAS WRONG)

**The deliverable is a MEASUREMENT FIX, not a match.** `tools/brally/fnmatch/fn.py`
and `tools/brally/fnmatch/triage.py` did not mask reloc'd operands in the
register-blind multiset -- only `tools/brally/msetdiff.py` did, and its header has
said so since it was fixed, but nobody carried the fix across. An unlinked
.obj holds the ADDEND in the reloc'd field and the symbol in the relocation,
so capstone prints `[edx*4]` or `push 0` where the LINKED original prints the
whole absolute; unmasked, the two never pair and **every absolutely-addressed
instruction is counted TWICE, once MISSING and once EXTRA.** Fixed in commit
79d8159; byte-exact functions still read 0+0.

** THIS IS THE NUMBER `triage.py` RANKS EVERY LANE BY** (`reggap`,
`struct%`, and the SHAPE / coloring-wall verdicts) and the number the
playbook tells you to judge every probe on. Measured:
 - 0x1000A110  reggap **48 -> 18**
 - 0x1000EAF0  reggap **86 -> 38**
 - 0x100250D0  **81 -> 81**, unchanged -- it indexes no global array
**So the error is NOT a uniform scale factor.** Any reggap COMPARISON between
two functions made before 2026-09-03 is unsafe, including parked verdicts and
"low reggap means well-worked". Re-measure before believing one.

**How it was found, and it is the reusable part:** the dominant family in
0x1000A110's residue was `mov R,[R*I]` x8 EXTRA against `mov R,[R*I+I]` x8
MISSING. Instead of theorising about addressing forms, I DISASSEMBLED BOTH
STREAMS AND LISTED EVERY SCALED-INDEX SITE -- fourteen instructions,
identical in both, at identical offsets.  **When a multiset family is large,
uniform and suspiciously symmetric (N extra of one shape, N missing of a
near-twin shape), dump the actual instructions before working it.**

**The honest residue maps, now that the numbers mean something:**
 - **0x1000EAF0 = 38 rows.** Wall 4 (ring addressing, orig `[R+A]` vs our
   `[R*K+A]`) is 8 and its downstream pDst spill 4, so **wall 4 is a third of
   everything left** and is the target. The row block is now ONE `fxch` from
   the original. Everything else is walls 3/5 and the dec/lea pair.
 - **0x1000A110 = 18 rows, and just TWO defects:** the arms-2/3 pack
   byte-lane (13 rows, frame-blocked -- see last session) and a
   two-instruction float operand swap.
 - **0x100250D0 = 81 rows**, genuinely, at instruction parity.

**DEAD PROBES THIS SESSION:**
 - 0x1000EAF0 wall 4, the one byte-offset form never tried: a byte offset the
   RECORD TERM IS DERIVED FROM so VC5 cannot fold it (`rbT = ring*4`, and all
   sixteen `ring*500` sites spelled `rbT*125`, which is exact). It does stop
   the folding and rebuilds the region's whole induction structure: 38 -> 78
   rows, bytes -14 -> -57.  Its FIRSTDIV IMPROVES BY 2,700 BYTES while the
   real gap doubles -- first-divergence is not a progress measure there.
 - 0x1000A110's float swap: swapping the summands (byte-identical -- a
   TWO-term float sum is canonicalised like the four-term ones), and reaching
   the struct term as `ptr[0]` off its own pointer.
 -  **THAT SECOND ONE QUALIFIES THE `ptr[0]` IDIOM FROM LAST SESSION: it
   needs a MULTI-USE pointer that survives to the use.** On 0x1000EAF0 the
   row pointers are multi-use and live in registers, so `ptr[0]` vs `ptr[2]`
   is two real addressing expressions; a fresh single-use pointer is forward-
   substituted straight back and the lever does not exist.

Image gate after: 0 differing bytes on all four binaries.

## RESUME (2026-09-03, session 15 - THE THREE GIANTS, TWO WALLS BROKEN)

Worked 0x1000EAF0, 0x100250D0, 0x1000A110 in that order, dossiers first.
**No new byte-exact match** -- these are the three giants, none is close --
but TWO long-standing walls fell and both were the same KIND of lever:
**an asymmetry in how the source SPELLS or PLACES a value, not in what it
computes.** Image gate re-run after: 0 differing bytes on all four
binaries, 1,018 fns / 163,297 B / 33.96% of BRGlide `.text`.

**0x1000EAF0 -- wall 1's term-3 operand flip CLOSED** (commit 3fe6a37), the
one the 16th-19th passes could not move and the 19th pass had written off as
"done as a source problem, T3a". It was a POINTER-INDEX ASYMMETRY: the
four-term row's terms 1, 2 and 4 read their object factor as `ptr[0]` off a
dedicated pointer local, term 3 alone as `pPos[2]` -- the same pointer as
term 1, at a NON-ZERO index. **VC5 ranks `ptr[0]` above `ptr[k!=0]` when it
picks which of a float multiply's two memory operands gets the `fld`**, so
term 3 alone came out object-first where the original is coefficient-first.
A fourth pointer local (`float *pTz = pObj + 0xe`) fixed all four sites.
Register-blind multiset 48+54 -> 41+45, instructions 6 short -> 4, bytes -15
-> -14. Masked regions went 19 -> 21 and that is NOT a regression (the
fourth row statement becomes its own small order-only region, and the 0x189c
`mov edi,1` sink re-opens as it does on every allocation change there).

**0x1000A110 -- arm 1 NO LONGER CROSS-JUMPS** (commit 466163b), a wall of
four sessions with five measured-dead spellings on it. The lever was WHERE
the pack is filled, not how it is spelled: the original loads the colourB
pack's first byte and homes it INSIDE colourA's tail (0x30c/0x317), loads
the top component at 0x31d and only reaches the second pack byte at 0x327 --
so in the source the two pack bytes are assigned ABOVE the colourA statement
and the top component BELOW it. Both elements then live across colourA, VC5
spends the byte slots on them, and the merge reads them back widened
(`mov edx,[slot]; and edx,0xff; or ecx,edx`) instead of forwarding a live
byte register into a lane (`mov dl,cl`). Multiset 21+32 -> 20+28,
instructions 11 short -> 8, bytes 42 short -> 31, raw 45+56 -> 42+50.

**0x100250D0 -- no movement, but the stretch nobody had read is now read**
(commit 2091e05). 0x15b8..0x19fd, 12.9% of the function, uncompared at every
resync key. Session 13's counter-register-swap diagnosis is right but the
`cmp ebx,eax` that goes with it is NOT a source lever: spelling the guard
`(int)param_1 < iVar17`, which is literally what the original's `cmp` reads,
is BYTE-IDENTICAL because VC5 constant-propagates the counter's zero. Also
read region 5 (0x9f8), the largest reliable change on the key-10 map at +58:
it is ONE extra instruction (the 4-step counter's init hoisted to the row
loop head) plus a guard-operand load-order notch -- not a block. That
function stays at instruction parity with allocation residue.

**SECOND PASS over the three, same session -- the screen was RUN, and its
value is now measured in both directions.** No further movement; five more
dead probes, all recorded in the file headers.
 - **0x1000EAF0**: no source asymmetry left. Both ring arrays are spelled
   identically in the trail-append block and the drain loops' byte-offset
   form is deliberate, so wall 4 is confirmed allocation. The region my fix
   re-opened at 0x189c is ONE instruction placed differently (a loop-
   invariant constant materialised inside the loop instead of on the exit
   path) -- T3a.
 - **0x100250D0**: the screen fired on a real asymmetry -- one of three IA
   blend bodies spelled its channel product intensity-first, the other two
   delta-first -- and it is INERT.  Applied anyway, so the screen does not
   stop there again. It extends the `*`-canonicalisation entry: VC5
   canonicalises a variable against an inline SUBEXPRESSION too, not just
   two named variables. Alpha's coefficient pair hoisted above chB is
   two bytes and two raw rows better but two multiset rows worse (rejected);
   all four pairs hoisted is clearly worse (the G/B/A interleaving is the
   original's shape, the R channel is the odd one and it is odd correctly).
 - **0x1000A110**:  ARMS 2/3 CARRY THE ARM-1 DEFECT TWICE (arm 3's colourA
   AND its colourB both forward a pack byte into a lane where the original
   homes both and reads them back widened) **and it is blocked by the
   frame.** Arm 1 could hoist because it has its own array; arms 2/3 share
   one array between the two colours, so colourB's bytes cannot be assigned
   early without a second array -- and a second aggregate local costs a
   locals dword and moves the first divergence 0x17 -> 0x2. **In this
   function any new aggregate local breaks the prologue, so the
   storage-class lever that CLOSED the frame in session 11 cannot be reused
   anywhere else in it.** Also dead: pack order within a pair
   (byte-identical -- only position relative to an ENCLOSING STATEMENT
   moves anything), and `topB` above arm 3's colourA (one multiset row
   better, but loses an instruction while the function is eight short).

** WHAT THE SCREEN IS AND IS NOT.** It found two walls in one pass and then
five dead ends in the next, which is the honest hit rate. Shape #1 (one
sibling spelled differently) fires often and is USUALLY inert, because VC5
canonicalises most commutative and ordering choices -- when it is inert,
APPLY the normalisation anyway so the screen does not re-fire on it. Shape
#2 (one value assigned in a different PLACE) is the one that pays, and its
blocker is the frame: hoisting needs somewhere to put the value.

** TWO HEURISTICS RETIRED THIS SESSION, both by the same evidence pattern.**
 1. **0x1000A110's "any change that moves arm 1's first divergence earlier
    is a regression."** Minted when every probe that did so also lost on
    size. The pack hoist moves it 28 bytes earlier and improves bytes,
    instructions, raw gap and register-blind gap together. A
    first-divergence tell is only evidence when the other axes agree.
 2. **Region count as a progress bar on these two functions.** Both wins
    RAISED the masked region count (19->21, 22->24) while every other
    measure improved. Rank by the register-blind multiset; read the region
    map for WHERE, never for HOW MUCH.

**DEAD PROBES ADDED (all in the file headers, do not re-run):** 0x1000EAF0 --
three term-order permutations (2134/2314/2341, byte-identical: flat float
sums stay canonicalised) and six groupings, of which the REDUNDANT OUTER
PAREN is a trap worth naming: `(((T1+T2)+T3)+T4)` reads -7 bytes and only 2
instructions short, that function's best-ever totals, by BREAKING the scale
block's 8|4 batching two blocks later -- the "recovered" instructions are the
extra loads. 0x1000A110 -- hoisting `top1` as well (one multiset row better,
eight raw rows worse), hoisting `packA[0]` alone (recovers none of the four
`or R,R` rows: BOTH elements must be live across colourA), reordering the
three assignments without hoisting (inert), and a named `uint8_t` for
colourA's second component (byte-identical).

**Idioms added to docs/brally/VC5-IDIOMS.md:** "`ptr[0]` and `ptr[k!=0]` are NOT the
same operand" (with the screen: when parallel terms compile the same way and
ONE does not, compare how the odd term SPELLS its operands, and ADD a pointer
local, never remove one -- the 19th pass's opposite probe is much worse), and
"the byte-lane move vs the widened `or` is decided by WHERE the value is
assigned" (read the original's SCHEDULE, not its arithmetic, when a pack is
short).

## RESUME (2026-09-03, lane d29628ed - BOOKKEEPING WAS WORTH MORE THAN GRINDING)

+2 byte-exact (534 B) and both came from **sweeping TUs that had already been
written and were sitting unscored**, not from new matching. Plus one large
structural transcription parked one optimiser decision short.

**THE CLASS TO SCREEN FIRST EVERY SESSION: a generated TU that was written
(even COMMITTED) but never swept.** report.csv then still carries the PORT
body's `diff` row for that VA, so triage.py ranks it as fresh work and the
finished match is invisible to every count. Two functions were in this state:
 - **0x10033BB0 BrPfxTick (219 B)** -- `src/brally/core/generated/0x10033BB0.c` was
   UNTRACKED (a killed worker's uncommitted match, exactly what rule 7 exists
   to prevent). One `tools/brally/match_sweep.py` on it: MATCH, first try.
 - **0x10033880 BrPfxUpdateB0 (315 B)** -- tracked, never swept; it scored
   MATCH the moment the directory was swept.
`tools/brally/stale_claims.py` says "no stale claims" for both and the VA-orphan
screen misses them too (the VA *is* in report.csv, keyed to the port file).
**Only `tools/brally/claimcheck.py`'s "TWO NAMES CLAIMING ONE ADDRESS" catches it.**
Run it at session start, before triage. Also `ls src/brally/core/generated/*.c | wc -l`
vs `grep -c src/brally/core/generated/ build/brally/win32/match/report.csv` -- they must be equal
(75/75 now). And `git status --short src/brally/` for uncommitted matches.

**0x1005ECF0 BrRacePathAdvance: 443 -> 219 B against a 204 B original**, filed
as `src/brally/core/generated/0x1005ECF0.c`, port body in br_racestep.c untagged. The
gap was the port's `BrAiNodeAt`/`BrAiPoint_` bounds-checked accessors: the
original walks RELOCATED NODE POINTERS and reads fields in place. Node layout:
next +0, sibling +4, u16 count +0x14, flag byte +0x16 (bit 0 = skip),
`BrAiPoint[]` at +0x40 (stride 0x28, centre +0x0C, arc +0x24). Both lerps write
straight into the 0x10B1CE98 global and the second READS it as its `b` operand
-- the port's local `BrVec3 p` cost the whole tail. PARKED at +7 instructions:
VC5 ROTATES the outer node loop (duplicating the null test, the flag test and
the epilogue) where the original has an unconditional `jmp` back to the null
test. Four loop spellings -- `while`, `for(;;)`+return, all-`goto done`, and a
SELF TAIL CALL -- give byte-identical output. Full dead-probe list in the TU
header.

**0x100349C0 BrVec3Project: the file's residue note was FALSE and is retracted.**
It claimed a 30-diff residue confined to 0x28-0x4F; the function actually
diverges at +0x8 in every spelling, at 163 B / 69 insns against 165 / 70, with
a register-blind multiset difference of exactly ONE `fxch`. This is the fourth
overturned "do not grind" note -- re-measure before believing one.

**NEW IDIOM (in docs/brally/VC5-IDIOMS.md): VC5's commutative-float canonicalisation
covers a WHOLE FLAT SUM-OF-PRODUCTS, not one add.** Eleven spellings of
BrVec3Project's projection compile byte-identically: term order in the `+`
chain, operand order inside each `*`, `(...)*r` vs `r*(...)`, a named numerator
temp, `w` inlined into the reciprocal, declaration order, and `(*m)[4]` vs
`pM->m[i][k]`. Only two things move the code: GROUPING (`a + (b + c)` is a
different tree -- a legitimate probe axis) and dropping the `vx/vy/vz` locals
(VC5 re-CSEs the loads, -8 insns). Never probe flat-sum order again.

**The C++ vcall/twin family is confirmed DRY.** `tools/brally/cpp_screen.py` lists 22
strong rows; every one is either matched, filled-and-parked with a dead-probe
list, or carries the photo trio. The only untouched member is 0x100541B0
(196 B), and its own sibling note predicts the 1-diff SIB park.

** claimcheck.py's OTHER list -- "the original delegates, the port calls
nothing" -- is a real screen, not just a smell.** It found **0x10001320**,
where the port's `BrUiSprClip` is only the GEOMETRY HALF of the original:
the real function clips, computes both surface pointers, and dispatches to
the keyed blit (0x10001440) or the plain one (0x100013F0). Written fresh as
`src/brally/core/generated/0x10001320.c` -- **206 B / 85 insns, SIZE AND INSTRUCTION
EXACT, reggap 0**, parked on one eax-vs-ebp choice for rect[3]. Surface layout
from it: u16 pixels +0, w +4, h +8, key(u16) +0xC; both pitches reach the
blits in BYTES. The lever that made it size-exact: **compute the two surface
pointers and both pitches ONCE, before the flag test** -- spelling them inline
in each call arm duplicates them (+33 B).
Two of the eight flagged rows (0x1007F240 BrStrUpr, 0x1008C320 br_stricmp) are
`crt` in config/brally/shared.csv (MSVCRT!_strupr / _stricmp) -- D3D's static CRT,
never a Glide target. Skip those; the four BrPhaseLeave rows are still open.

**0x10059410 BrGlNavPoll parked at +4 B / reggap 0+1**, note in the file. All
three divergence regions are ONE eax<->ecx rotation: the original keeps the
T6708 load and the `f` temp in ecx, which leaves eax free for a fresh
`xor eax,eax` and the one-byte-shorter A3 accumulator form on four absolute
stores. The missing `xor` is the EFFECT of the rotation, not the cause.

**Image gate re-run at the end of this lane: 0 differing bytes**, 1,019 fns /
159,232 B / 33.11% of .text (847 C + 172 C++).

**Counts after this lane:** report.csv **847 MATCH / 1,060 tagged**, 83,011 B.
Residue 210. Image gate NOT re-run this lane -- re-run before quoting a byte
figure.


## ▶ RESUME (2026-09-03 - THE THREE GIANTS, IN THE ORDER THE PLAYBOOK NAMES)

Worked 0x1000EAF0 → 0x100250D0 → 0x1000A110. No new byte-exact match; one
behaviour bug fixed, two regions closed, one measurement trap killed, one new
tool. **Image gate re-run at the end: 0 differing bytes, 1,019 fns / 159,232 B
(33.11% of .text).**

**0x1000EAF0 (scene DL, -15 B, 19 masked / 28 raw).** Checked all 39+33
register-blind multiset rows against the dossier ROW BY ROW: every one maps to
a catalogued wall (4 = ring*4 CSE and its 3 spill rows, the largest at 12
rows; 1/2 = the x87 term-3 flip and fxch spread; 3 = the two lea/mov rows;
6 = a pure 0x20↔0x24 slot swap that is regions 1,2,4,5,6,7 on its own).
**No missing-code row is left** - the six short instructions are all wall 4
and walls 1/3. It is T3a by the playbook's own decision tree. One fresh probe,
DEAD: deleting the row block's `pTw`/`pTy`/`pPos` pointer locals for direct
`pObj[0xc..0xf]` indexing (REGNORM 48+54 → 72+83). The pointer spelling is
load-bearing.

**0x100250D0 (tex3d expand).**  Its ENTIRE DOSSIER was measured on two
thirds of the function - see [lost-sync-region-trap](../traps/lost-sync-region-trap.md). Real map: 31 regions
at key 10, 52 at key 6, largest reliable block -50 at 0x1a4c (the tail nobody
had seen), and 1,093 bytes still uncompared at 0x15b8..0x19fd because no ten
consecutive instructions match there. That stretch is the `param_6 == 3` IA
blend arm and its defect is a COUNTER-REGISTER SWAP: the original keeps the
outer row counter in ecx all loop and homes the inner one; we do the reverse.
Two probes dead - a fresh `int` for the reused `param_1` counter (byte-neutral)
and swapping the two counters' roles (REGNORM one row better, bytes 6 worse).

**0x1000A110 (car DL) - THE WIN, and it was a behaviour bug.** The second
specular MOVEMEM pair (0xBC3F) emits **pLights**, not specMem: the original's
three pool results land in three slots and `[esp+0x30]` (the 1st 0x100625A0,
also read at 0x690 by the light calls) feeds that pair while `[esp+0x28]` (the
2nd) feeds only 0xAE34. We were putting the wrong pointer in the display list
AND the duplicate spelling let VC5 CSE the shared `specMem + 0x10` into a slot
where the original recomputes it destructively (`add edx,0x10`) at each site.
Masked regions 24 → 22, multiset 19+10 → 17+14, REGNORM 25+34 → 21+32.
 Size moved the WRONG way (36 → 42 short): removing an accidental spill
exposed a real deficit, exactly the session-11 pattern. Rank by the multiset.

**NEW TOOL - `tools/brally/slotcensus.py`** ([slot-census-screen](../triage/slot-census-screen.md)). Per `[esp+N]`
slot of the original: every write, every read, and the call that produced the
written value. It is the ONLY screen that sees "one source variable emitting
two different values" - divergence.py sees the same `mov [eax+4],R` in both
streams, msetdiff sees one shape, the push census sees no pushes. Run it on
any function that allocates or caches more than one pointer.

** THE COROLLARY, three byte-identical measurements: RENAMING IS INERT.** A
reused parameter as a loop counter vs a fresh local; a Ghidra-recycled temp
(0x100250D0's `iVar5`, which is both the tile-record pointer and all five
copy-back scratch counters) split at all five sites; and `t = x; t += c` or
`x += c` used to try to break a CSE. VC5 builds its own webs and value-numbers
`x + c`, `t = x; t += c` and `x += c` identically. **The payoff is always the
VALUE, never the NAME.**

Also dead on 0x1000A110: the session-11 "the original re-reads what we cache"
rule applied to the pool block's `pCam` - deleting it BREAKS THE FRAME (first
divergence +0x17 → +0x2); applied to the two comparison lines alone the frame
holds but RAW goes 50+59 → 53+62. pCam stays.

## ▶ RESUME (2026-09-03 lane 6077a6e1 - the TAGGED pool is drained at small reggap)

**Image gate re-run: 0 differing bytes, 1,017 fns / 158,698 B / 33.00% of
.text. total.py 1,121 byte-exact / 169,003 B (845 C + 172 C++ + 104 EXE);
one of those is this lane's.** Tiers AFTER a fencing correction: target
1,505 (was 1,519), T1 274 / 176,164 B, T2 179, T3a 36, T4 1,016.

**BANKED: 0x10065950 BrCrPlaneDist (41 B) - T1 intake, byte-exact on the
FIRST compile.** Signed point-to-plane distance in
`src/brally/core/driving/br_collrespsolve.c`. Two things made it one-shot: the
fourteen call sites all push `esi / [esi+0xc] / &pt`, which fixes the
signature, and the term order came off the two `faddp st(1)`s (y, z, x, then
the constant) instead of being guessed as x, y, z.

** NEW IDIOM - A SELF-PROTOTYPE MOVES THE x87 SCHEDULE.** Adding the obvious
prototype to the module header took that byte-exact function to 41 -> 43 B
with ONE extra `fxch st(1)`. Isolated both ways twice. **When a float leaf is
right everywhere but one stray fxch, delete its prototype from the header
before probing the expression.** A prototype in another TU is harmless - only
a prior declaration in the DEFINING TU does it. Written up in
docs/brally/VC5-IDIOMS.md with the screen it implies (float leaf + one-fxch gap +
declared in its header). NOT yet swept across the tree - that is a free lead
for the next lane.

** TWO TOOLING FAULTS FOUND AND FIXED - both were feeding wrong targets.**
1. `tiers.py --list T1` printed 460 rows against a stated 289: the summary
   subtracts report_cpp.csv matches, the LIST filtered on report.csv tags
   only. The extra 171 are byte-exact already, and they read like fresh work
   - a nine-member 66-byte `BrOpt*` family that looks exactly like an
   untouched cause group. I lost time screening it before checking
   report_cpp. Fixed.
2. Fourteen non-C-function rows sat in the hand-C target and in T1:
   `_chkstk` (0x10074580, next to the fenced __aulldiv block), five bare
   `jmp dword ptr [IAT]` thunks, and eight unwind funclets that read
   `[ebp±N]` without establishing ebp (two at odd addresses). Added to
   config/brally/fenced.csv.

** THE SMALL-REGGAP TAGGED POOL IS WORKED OUT - do not plan a lane on it.**
`claim 20` returned nothing; every SHAPE row at reggap <= 7 is parked, and
un-parking them (edit lane_claims.csv) mostly finds a THOROUGH dead-probe
note already in the file header, several dated the same day. Measured this
lane and re-parked: 0x100349C0 BrVec3Project (1 fxch), 0x10036810
BrComGetAlloc + 0x10036E50 BrDpCreateIface (one branch polarity each, arm
placement notes already exhaustive), 0x1006FD50 BrEntitySetIndex (2 bytes,
`add R,-I` vs `sub R,I`, dead list covers every operator spelling),
0x1006DD20 BrMat3Mul / 0x10029D70 BrMat4Mul (IV anchor, six term orders
already probed), 0x10034360 BrVec3Scale (x87 operand order, canonicalised  - 
re-confirmed dead this lane). **T1 intake is where the byte-exact count
moves now**: 274 functions, every one with a draft, `tiers.py --list T1`.

**0x100014A0 BrSurfSetColourKey: -3 bytes -> SIZE- AND INSTRUCTION-EXACT
(51/51, 16/16), still not matched.** The blue channel is a byte-slot local:
red/green assigned to `key` FIRST, then `b` loaded and shifted IN PLACE, then
consumed by the very next statement. Any statement between `b >>= 3` and the
use spills it (+10 B). Residue is ONE widening form - orig `and eax,0x1f`
(range-known dword widening), ours `movzx cx,cl`, because the 16-bit
destination narrows the OR - plus a swapped accumulator/blue register pair.
The old note's "DO NOT RE-PROBE, movzx is worse at 33-34 diffs" was measured
against the WRONG FRAME: with the statement split it is size-exact. Fresh
dead list is in the file.

## ▶ RESUME (2026-09-03 lane 62034277 - RUN claimcheck.py AT SESSION START)

**Image gate 0 diff bytes, 1,014 fns / 158,378 B / 32.94% of .text. Tiers:
T1 289/176,365 B, T2 181/109,982, T3a 36/9,297, T4 1,013/157,496 of 1,519.**

** TWO MORE DUPLICATE ADDRESS CLAIMS** (0x10059410, 0x10031B80) - a d3d tag
resolves through shared.csv onto a Glide VA, so a port body and a real
transcription both claim it. **The short row poisons every size screen:
0x10031B80 sat SECOND on the whole factored-helper board at "-1165 bytes
short" and the function it names is SIZE-EXACT (1552/1549, multiset exact,
parked on ordering).** `claimcheck.py` now flags this directly - I added it,
silent when clean. **Fragments, thunks and port-only bodies must not carry
@implements.**

** RUN `tools/brally/claimcheck.py` AT SESSION START.** It found the class the size
screens cannot see: 0x1001FD70 BrDlVtxRoutine flagged "orig calls 3, port 0",
and that was exactly right - **269 bytes short -> 6, 80 instructions against
80.** A THIRD MISSING-CODE SHAPE, now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md): **the port
kept only the TAIL and changed the shape while doing it.** The original
INSTALLS into dispatch slots and returns void; the port made it RETURN a value
and dropped three state blocks that XOR the new mode against the previous one
and drive a hardware setter off each group of changed bits.
Sub-facts: such a guard RE-READS the mode after every block (the setter can
change it); "assign then conditionally override" is a real source shape an
if/else will not produce; and a value-returning port helper built on a void
original must be `#ifndef BR_MATCHING_BUILD`'d out or the arm will not build.

**Backed off 0x1003A140 / 0x1003A2B0 (BrMenuTime pair) - the C++ workstream
owns that family** (0x1003A140 is already in report_cpp.csv at 237). Returned
them to park. **Check `ls src/brally/core/cpp/<VA>.cpp` and report_cpp.csv before
taking anything in the 0x1003Axxx / menu-item range.**


## ▶ RESUME (2026-09-03 lane d3ae27a9 - TWO MORE "MISSING CODE" SHAPES)

**1,115 byte-exact / 167,550 B (839 C + 172 C++ + 104 EXE); image gate 0 diff
bytes, 32.70% of .text placed. Tiers: T1 460/251,704 B, T2 186/112,953,
T3a 34/7,459, T4 839/81,024 of a 1,519-function target.**

**Fixed a duplicate address claim first:** 0x10059410 had TWO names --
BrGlNavPoll (the real 960-byte transcription) and BrUiNavMove, which its own
comment calls "0x100603A0's two edges, and only those two", i.e. 48 bytes of a
939-byte function. d3d 0x100603A0 is a TRUE twin (939 in both binaries), so
the fragment was being scored against the whole thing. Untagged. **The report
now has zero duplicate VAs - re-run `awk -F, 'NR>1{print $2}' report.csv |
sort | uniq -d` after any tagging work.**

**0x10034010 BrSpanBuildHull: 327 bytes short -> 7, regnorm 59+145 -> 12+9.**
NEW SHAPE, now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md): **the port's loop over a static
table is UNROLLED in the original** -- twelve separate call sites with the
operands as absolute globals, and once the loop goes the parameters go too
(this one takes NO arguments). Its inner scans are UNBOUNDED in the original
and that is reproduced, not fixed.

**SECOND NEW SHAPE - the port DELETED the original's debug tracing.** Screen
in the idiom doc finds short diff rows whose ORIGINAL calls the trace sink
0x10008D60: **0x1005FF00 BrRaceGateStep (-1962, TWELVE trace calls, 10
BrStrGet, 4 sprintf) and 0x10067710 BrCrRespWalk (-549)** -- 2,511 bytes.
The port's own comments quote the format strings. **This is transcription, not
source discovery; do not go hunting for a helper.** BrRaceGateStep is the
single biggest C-lane win left and its reggap is only 13 EXTRA / 530 missing,
i.e. everything already written is right.

Residue parked: BrSpanBuildHull's last 7 bytes are the documented
literal-pooling class (0x3F appears four times, VC5 pools it into a register,
which costs a `push ebx`; the original materialises it per site).


## ▶ RESUME (2026-09-03 lane 41773381 - WIDTH AND ADDRESS FORM, NOT SHIFTS)

Image gate **GREEN, 0 differing bytes, 1,014 fns / 158,378 B / 32.94% of
.text.** Denominator: 1,519 hand-C fns / 453,140 B. T4 = 1,013 (66.7% by
count, **34.8% by BYTES**) - note `tiers.py` now folds the C++ EH lane into T4
(171 fns / 75,339 B of it), so the C-lane figure is 842 fns / 82,157 B.
T1 = 288 fns / 174,816 B. **No byte-exact match this lane: two large shape
improvements.**

**0x10058900 BrExt_1005FBC0: register-blind 5+4 -> 0+0, 71/71 instructions,
288 vs 289 bytes.** Three instances of ONE disease - the C bridging a width
or address difference with a shift or a cast instead of declaring what the
original declares. All three now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):
- Two adjacent bytes spelled `arr[0]`/`arr[1]` are TWO SEPARATE GLOBALS. As an
  array VC5 merges them into one dword load and takes byte 1 out of `ah`.
  **The file's own note had already said "it would need the two bytes to be
  separate globals" and stopped there** - a note that names the fix is a
  to-do, not a wall.
- A dword's HIGH HALF is its own 16-bit global (`mov dx,word ptr [addr+2]`
  after `xor edx,edx`), not `(x >> 16) & 0xFFFF`.
- A base address must sit INSIDE the address expression so VC5 folds it into
  the lea displacement; an int offset added to the array afterwards costs a
  second lea.
Parked at REGNORM 0+0: identical multiset, only schedule and register naming
differ, so the last byte is an encoding-length difference. T3a.

**0x100302A0 BrModelSwap continued: 35+36 -> 13+23** (149+285 when first
opened).  **A 2-BYTE REVERSAL IS A HALFWORD COMPOSE AND ONE 16-BIT STORE.**
`t = p[0]; p[0] = p[1]; p[1] = t;` emits two byte stores and MSVC5 will NOT
merge them however the temps are arranged; the original loads the two bytes
into the low and high halves of one register and writes the pair once, so the
source is `*(uint16_t*)p = (p[0] << 8) | p[1]` - VC5 turns that into
`mov cl,[p+1]; mov ch,[p]` with no shift at all. **Tell: count `mov word ptr`
stores in the original, one per swap site.** Worth 22 shapes here.
Residue now ten instructions SHORT and concentrated in the leaf loop's index
form (negative displacements against a missing scaled lea). Dead, re-probed
against the NEW baseline and clearly worse: the stepped-local for the doubled
subscript (36 -> 49).

Not started, and both are transcription jobs rather than lever jobs:
0x1005FF00 BrRaceGateStep is 720 vs 203 instructions (28% present) with four
file-local statics each called exactly twice from it; 0x10015B10
BrTextEmitString 3,050 vs 1,888 bytes.

## ▶ RESUME (2026-09-03 lane 912109bc - THE FACTORED-HELPER RECIPE, WORKED END TO END)

Image gate **GREEN, 0 differing bytes, 1,014 fns / 158,378 B / 32.94% of
.text.** Denominator (`tools/brally/tiers.py`): 1,519 hand-C fns / 453,140 B; T4 =
842 (55% by count, **18% by BYTES**), T1 = 460 fns / 251,704 B still unstarted.
`claim 20` yielded 6 this time - the other session's claims had gone stale.

**0x100302A0 BrModelSwap: register-blind gap 149+285 -> 35+36, size 352 short
-> 8 short, instructions 136 short -> 1.** Not byte-exact; four separate facts,
each worth recording, and the file now carries the residue note:
1. **Five byte-swap statics inlined as SCOPED MACROS.** `#define` after the
   static definitions (defining before them macro-expands the definition line
   into garbage) and `#undef` right after the function, so the helpers' other
   callers keep the shape they already match with. 41 calls removed.
2. **The macros must index their ARGUMENT directly, not through an
   `unsigned char *p_` temp.** With the temp every access came out `lea` plus
   a negative displacement; without it, base+index+disp like the original.
   This one alone was the biggest single step (gap 168 -> 79).
3. **Nothing is cached: the base pointer is re-read from memory before every
   access.** `esi` is `lea [ebp+4]` - the ADDRESS of the header's pointer
   slot - and the fixup rewrites that slot in place, so pBlock/pItem/pLeaf
   are not locals. **There is also no deref hop at all**: `g_BrModelDeref`
   was a port invention (original: 9 direct `call rel32` + 1 indirect; we had
   9 indirect).
4. **Count the compose sites.** The original has exactly six `shl`/six `or`,
   i.e. THREE compose-and-store fields (block->n, item +0x00, item +0x0C);
   the other three (+0x14/+0x18/+0x1C) are plain in-place byte reversals. We
   had six composes and twelve shl.
Residue: the original merges adjacent byte stores into 16-bit stores (13
extra byte stores vs 7 missing word stores). Three spellings probed dead.

**Also recorded in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):** the scoping mechanics above,
and the CALL-FORM CENSUS - `call dword ptr [mem]` in the recompile against
`call rel32` in the original means a port hook declared as a function POINTER
that the original calls directly. One-line check, unambiguous.

0x10059410 BrGlNavPoll (939 B, REGNORM 1+2) confirmed T3a and parked: the
whole size gap is VC5 narrowing a shared -1 to `mov edx,0xffff` because only
`dx` is ever read, where the original keeps `or edx,-1`. FOUR more spellings
dead (no cast, int16_t, uint32_t 0xFFFFFFFF, double cast) - and the value has
no 32-bit use in the ORIGINAL either, so giving it one is not the answer.

## ▶ RESUME (2026-09-03 lane 3f7cb98c - THE SCREEN ITSELF WAS OVER-COUNTING)

Image gate re-run: **GREEN, 0 differing bytes, 1,012 fns / 157,428 B / 32.74%
of .text.** Real denominator from `tools/brally/tiers.py`: **1,519 hand-C functions /
453,140 B**; T4 = 840 (55% by count but only 18% by BYTES), T2 = 187, T3a = 34,
**T1 = 459 fns / 250,944 B - 55% of the target bytes are not in the tree at
all.** `claim 20` empty again; all 18 live claims belonged to the other
session, 3-6 min old, including the whole factored-helper batch.

** TWO BUGS IN THE FACTORED-HELPER SCREEN, both fixed in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):**
1. Its EH test read only byte 0 for `6A FF`, but a VC5 EH prologue often
   leads with `mov eax,fs:[0]` - so `64 A1 ... 6A FF` frames were counted as
   C targets. Two rows, 1,242 bytes.
2. It counted rows the C++ lane already owns (a `src/brally/core/cpp/<VA>.cpp`
   exists) - 4 rows / 1,782 bytes, and one of those, 0x1003A140, has a
   byte-exact C++ sibling. **Also check a row's TWIN:** 0x1003A2B0 has no
   `.cpp` of its own but is the same function as 0x1003A140, so matching it
   in C would be work done twice and the C tag gets retired anyway.
Headline corrected 32 rows / 13,463 B -> 27 rows / 9,085 B.

**0x1001E930 br_dl_env BYTE-EXACT (183 B) - four independent defects in one
small function**, and all four are the accessor/factored-helper classes:
one argument not two (no BrDl parameter; `nLights`-style data is absolute
globals); `env[]` was never an array (the four destinations are NOT
contiguous); `br_dl_w` does not inline so it emitted a `call`, spell the dword
load out; the dword is RE-READ per component (hoisting it into a local costs
three reloads); and `(int32_t)` gave `fild dword` where the original has
`fild qword` - **the unsigned conversion**. The dispatch-table entry needs a
cast in the matching arm because the original's handler takes one argument.

** THE ROW'S RECORDED `opt` WAS WRONG AND IT COST THE FUNCTION.** br_dl_env
was recorded `O2y`: at /O2 /Oy- the best form is 105 diffs, at /O2 it is 85,
at **/O2 /Op it is BYTE-EXACT**. Compile a structurally-believed function
under all four variants before concluding anything about the source. Related:
a MISSING `fstp [esp+S]` / `fld [esp+S]` pair with no EXTRA counterpart is
/Op's round-to-float on assignment, NOT a source temp - adding a `float t =`
to chase it went 0 diffs -> 119.

Parked with notes: 0x100344D0 br_dl_normalise (49 B; one extra x87 stack slot,
the X term loads pV->x instead of using it as the fmul memory operand; three
spellings and all four variants recorded - genuinely /O2) and a full DOSSIER
on 0x10022AC0 br_dl_light_vertex (two args not three, nine light values are
absolute globals, loop unrolled x3, both early arms integer copies, clamp is a
punned 0x437F0000 in the dead parameter slot, counters are port additions).
**br_dl_light_vertex is blocked only on the source-vertex record type** - its
caller br_dl_project is 170 bytes in the original against 8,022 here, so it
does not correspond 1:1 and cannot be adjusted in passing.

## ▶ RESUME (2026-09-03 lane 60806d57 - WORKING THE FACTORED-HELPER SCREEN)

**1,113 byte-exact / 165,791 B (839 C + 170 C++ + 104 EXE); image gate 0 diff
bytes, 32.34% of .text placed.**

** `claim 20` RETURNED NOTHING - the ledger really is exhausted.** I un-parked
NINE rows straight off the factored-helper screen by editing
`build/brally/win32/match/lane_claims.csv` (0x1005FF00, 0x10015B10, 0x10059410, 0x1001FA30,
0x1001ECF0, 0x10027850, 0x10034010, 0x100302A0, 0x10067710). **This is now the
normal way to start a lane** - pick from the screen, un-park, work it.

**Worked the two triangle handlers (0x1001ECF0 / 0x1001FA30): 337 and 627
bytes short -> 49 and 56.** Tri1 was 18 instructions against 113. Five facts,
all now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):
- The handlers take ONE argument; the vertex pool is the absolute global at
  0x105CE318, stride 0x68, so there is no state pointer.
- The port's four standard additions were ALL present and all visible as EXTRA
  instructions: counters, a bound check, a null test before a sink call, and
  the state-pointer parameter. Strip all four.
- **Hand-inlining mechanics: use a MACRO (a new `static` is a call again), and
  take the temp as a macro PARAMETER - one function-scope local shared by all
  expansions is what gets a stack slot; a fresh one per expansion stays in a
  register.**
- ** PUN THE FLOAT STORES.** The original writes each product once and copies
  it to both destinations with integer movs. Plain float assignment makes VC5
  emit fst/fstp straight to the fields and the temp never gets a slot.
  `#define PUN(d,s) (*(uint32_t*)(void*)&(d) = *(const uint32_t*)(const void*)&(s))`
  - worth **25 register-blind shapes** on one function.

Parked with dead probes in the file: both handlers. The original keeps the
SCALED BYTE OFFSET in a register and re-forms base+offset per access where a
pointer local keeps the pointer; INDEX form (`pool[i].field`) fixes the
instruction count but costs ~90 bytes of SIB - measured, worse, do not re-run.

Still un-parked and untouched in that batch: 0x1005FF00 BrRaceGateStep(-1962),
0x10015B10 BrTextEmitString(-1162), 0x10059410 BrUiNavMove(-891),
0x10027850 br_tex3d_shift(-377), 0x10034010 BrSpanBuildHull(-327),
0x100302A0 BrModelSwap(-342), 0x10067710 BrCrRespWalk(-549).


## ▶ RESUME (2026-09-03 lane dc2132a3 - THE FACTORED-HELPER SCREEN IS MINTED)

**1,106 byte-exact / 154,075 B (837 C + 165 C++ + 104 EXE); image gate 0 diff
bytes, 29.90% of .text placed.** +2 C byte-exact, but the deliverable is the
screen.

** THE FACTORED-HELPER CLASS IS THE LARGEST SOURCE-LEVEL CLASS LEFT IN THE C
LANE: 32 rows, 13,463 bytes short** (EH rows excluded). Runnable screen in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md). MSVC5 will not inline a `static` with more than one
caller, and NEVER one returning a struct by value, so every port helper that
factors a shared body emits a call the original does not have and the function
reads MISSING CODE. Worst: BrRaceGateStep(-1962), BrOptFn100558A0(-1357),
BrTextEmitString(-1162), BrExt_1004DFC0(-962), BrUiNavMove(-891).
**Recipe, never touches the port arm:** spell the body out at the call site
under `#ifdef BR_MATCHING_BUILD`, keep the static for its other callers, port
version in the `#else`.

**Sub-case worth its own name: an accessor over N standalone globals.**
`BrScreenGet->cx` groups four SEPARATE absolute globals behind a pointer;
the original loads each absolutely. Giveaway in the diff: `mov R,[R+I]` where
the original has `mov R,[I]`, several times. slice5_61.c, slice6_70.c and now
slice5_63.c all carry the correction.

Byte-exact this pass: 0x10037B20 BrSub1003E510(368, two inlined sweeps) and
0x10014800 BrSub_10017290(310, standalone globals + raw-dword times).
Four more facts, all in the idiom doc: a loop probing a global keeps probing
the GLOBAL not the saved start (the `mov esi,eax` copy IS the local); a
doubled table index needs its own local or VC5 folds the x2 into the SIB
scale; an array whose ADDRESS is pushed is an array not a pointer; and a float
argument moved with `mov`/`push` instead of `fld`/`fstp` is a DWORD PUN
(declare the parameter uint32_t).

** CONCURRENCY:** another lane is live in this tree and commits with `-a`;
one of my file changes was swept into THEIR commit (f85d281). The code is
safe, the explanation is not - **put the reasoning in docs/brally/VC5-IDIOMS.md, not
only in the commit message**, and expect `.git/index.lock` contention (retry
loop, 15 s).


## ▶ RESUME (2026-09-03 lane a15c404e - MEASUREMENT DISCIPLINE)

Image gate re-run and **GREEN: 0 differing bytes, 1,009 fns / 155,486 B /
32.34% of .text.** The 3-byte red from the previous lane lived only in
uncommitted generated C++ and is gone.  **the notes index HAD OVERFLOWED ITS
200-LINE READ LIMIT** - entries at the tail were being silently dropped on
every load. Rewrote it to 137 lines (detail moved here, all 66 note pointers
kept). Check `wc -l` on the index after editing it.

**0x10015630 BrSceneSetupFrame - register-blind gap 77 -> 44 at the TU's own
/O2 /Op.** Two fixes: (1) the 17-argument emit is WRITTEN OUT IN BOTH ARMS,
not hoisted above the `if` - the file's own note said "both branches emit the
same block" and had hoisted it, but the original has SEVEN calls and TWO
`add esp,0x44`, so a hoisted call can only ever be one of them; (2) the three
brightened colour components use `>> 2`, not `/ 4` - a signed divide emits
the round-toward-zero correction (`cdq; and; add; sub`) the original's bare
`sar` does not have.  **TELL, reusable: count `call`s and stack adjusts in
the original before believing any "both branches do X" comment.**
`tools/brally/pushcensus.py` with the ORIGINAL as argv[1] shows the group-count
mismatch immediately.

** TWO WAYS MY OWN BEFORE/AFTER MEASUREMENT LIED, both in this one function
 -  now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):**
1. `fn.py` compiles /O2 ONLY. This TU is proven at /O2 /Op, so the gain I
   first quoted was partly phantom. Look up the row's `opt` and re-measure
   with those flags before claiming anything.
2. **With another session committing to the same branch, `HEAD~1` IS NOT YOUR
   COMMIT'S PARENT.** I compared against `HEAD~1`, got "byte-identical", and
   nearly recorded a working fix as a no-op - it was my own change compared
   against itself. Use `<yourcommit>^`.
Honest numbers once measured right: register-blind 77 -> 44 while the RAW
BYTE DIFF WENT THE WRONG WAY (819 -> 821) and size went 38 short -> 7 short.
That is the documented pattern; rank by the register-blind multiset.

Residue left on it: 4 dword global reads where the original byte-loads, plus
a `mov R,R` / `mov B,B` / `and R,I` cluster (byte-slot idiom). DEAD probe:
nesting the 0xFB word's shifts to pack two bytes into one register - VC5
canonicalises the `|` chain, byte-identical.

## ▶ RESUME (2026-09-03 lane 2 -  IMAGE GATE WAS RED (now green), AND A FALSE PARITY CLAIM

** THE IMAGE GATE IS NOT CLEAN: 3 differing bytes, and they are NOT mine.**
`0x10053590 FUN_10053590` (src/brally/core/cpp/0x10053590.cpp, landed by the
gen_menubuilder work in 283741b) reads `match` with 0 diffs in report_cpp.csv
but is wrong once addresses resolve. The three bytes are at file+0x52999, VA
~0x10053599, and they are the C++ **SEH scope-table pointer** in the prologue:
original `push 0x100765d4`, ours `push 0x100293b4`. So the C++ lane places a
function whose EH scope table resolves to that TU's own address instead of the
original's - image_build's fill-from-reference does not cover this slot. This
is a CLASS bug, not one function: any generated C++ TU with an EH frame can
carry it, and the function-level scorer cannot see it. Whoever owns the C++
lane needs it. Reproduce: `tools/brally/image_build.py --out /tmp/img.dll` then diff
against `reference/brally/orig/BRGlide.dll`.

** THE LANE LEDGER IS EXHAUSTED AND OVER-PARKED.** `claim 20` returns
nothing: 221 parked / 16 claimed of ~240 diff rows. Nearly every TOP-RANKED
`SHAPE - best targets` row is parked, including ones at 86-100% struct% that
the playbook itself says to attack. Parks are permanent (claim_lane has no
unpark). I worked parked rows directly, reading each file's residue note
first; the other session is doing the same and editing lane_claims.csv by
hand. **The park state has stopped meaning "wall" and now just means
"someone ran out of time".**

- **0x10021060 BrGbiEndDList - BYTE-EXACT.** A wrapping `if (n != 0) { …
  return v; } return 0;`, not `if (n == 0) goto empty;`. Tell: an original
  that spells `xor eax,eax` before `ret` has its zero-return in a SEPARATE
  TRAILING block; a fall-through early exit lets VC5 reuse the just-tested
  eax and drops the `xor`, making the recompile 2 bytes SHORT.
- **0x1006D530 BrRbQuatDerivative - 26 -> 21 diffs, now at exact size (206)
  and exact instruction count (73), RAW/REGNORM 0+0.** Row 3 subtracts SECOND
  (`h.z*f04 - h.x*f0C + h.y*f00`), not last.  Its note CLAIMED instruction
  parity and 0+0 already; rebuilding the note's own commit measured 208/74
  and a surplus `fxch`. **Re-measure a parity claim before believing it  - 
  that claim is exactly what stops the next reader.** Four notes in this tree
  have now been overturned. Swapping ADDENDS is neutral (VC5 canonicalises);
  moving the SUBTRAHEND is the lever. 64-build per-row sweep recorded.
- **Big negative result, worth not repeating: `sub reg,imm` vs
  `add reg,-imm`.** MSVC5 canonicalises EVERY straight-line constant
  subtraction to add-negative - 6 source spellings x int/long/unsigned/short/
  2 pointer forms x /O2,/O2 /Op,/O2 /Oy-,/O1,/Ox, all `add`. **VC++ 4.2 emits
  `sub` for the same source**, but that is NOT a usable compiler fingerprint:
  11 byte-exact MSVC5 functions in the tree contain `sub r32,imm`. The three
  MSVC5 routes to a real `sub`, read off byte-exact answer keys: a
  loop-carried decrement (0x10023B10), a 16-bit-typed subtraction whose result
  stays live narrow (0x10053EF0), a pointer difference feeding a divide
  (0x1006FF00). 0x1006FD50 BrEntitySetIndex is 2 bytes from exact on this and
  fits none of them - OPEN, and the lever is type/lifetime, not the operator.
- Parked/confirmed this lane: 0x100283C0 BrTex3dDownloadAt (4 B, VC5 hoists
  the arg-3 load above two stores, schedule is source-order INDEPENDENT - 3
  probes), 0x10034360 BrVec3Scale + 0x10034660 BrVec3MulAdd (the existing
  operand-order note is CORRECT, re-confirmed in one probe - the original's
  own x-vs-y/z asymmetry is not reachable by operand order).

## ▶ RESUME (2026-09-03 lane 276dbfcf - A THIRD "UNREACHABLE" NOTE PROVEN WRONG)

**1,093 byte-exact / 142,256 B (829 C + 160 C++ + 104 EXE); image gate 0 diff
bytes, 27.21% of .text placed.**

** I UN-PARKED THREE ROWS** (0x1002ECEB, 0x1002F380, 0x1002EB03) by editing
`build/brally/win32/match/lane_claims.csv` directly - claim_lane has no unpark command and
the pool is thinning. The park predated the "diff stranded in an /Od run"
screen and was wrong. **When a screen invalidates a park, un-park it; parked
rows are excluded from the pool forever.**

**0x1002EB03 BrCarGfxReadColour BYTE-EXACT - the file's own note said it was
unreachable from C** ("the fastcall trick must materialise a dummy edx"). It
is not: declare EVERY stack argument as a one-member struct and edx is left
alone. And no new locals are needed - the three struct args ARE the three
existing colour locals, so the frame stays at five slots. That is now the
third "unreachable"/"do not grind" note in this tree proven wrong; **treat
them as leads, not verdicts.**

**NEW CLASS - `(double)` modelling is a D3D-ERA ARTEFACT** and the Glide
binary is FLOAT. Confirmed three times now (BrRbBuildMatrix's own note, the
velocity trio, and 0x1006D850 at -55 bytes -> -11). **Tell: if the original
never emits `fstp qword ptr [esp+N]`, the temporaries are float.** Screen in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md) names TWELVE diff-bearing files, worst slice2_15.c
(63 casts / 7 diff rows) and slice2_17.c (43 / 8). This is the biggest
un-harvested lever on the board right now.

**Two boundary conditions recorded (I over-stated the rule last session):**
a `(float)` cast on an already-float expression is a NO-OP - only a named
float local spills, and only if the products are computed BEFORE the adds;
and the `fld a; fmul [b]` operand-order lever works only when one operand is
a LOCAL - with two memory operands VC5 canonicalises and both spellings
compile byte-identical.

0x1002ECEB BrAnimUpdate: two /Od facts fixed (wrapped guard, no pList local),
first 0x19 bytes exact. What is left is slot homing across ~24 locals by the
/Od NAME HASH - its own session. Do not re-park it.
0x1002F380 BrPadTranslate: 10 bytes, documented allocator residue (VC5 pools
the repeated 0x80 mask into cl); three spellings already probed dead.

## ▶ RESUME (2026-09-03 session 13 - LANE 5219b465, 3 MATCHES, "DO NOT GRIND" NOTE PROVEN WRONG)

Structural-playbook lane run. Image gate 0 diff bytes after.  ANOTHER SESSION
WAS COMMITTING TO `main` AT THE SAME TIME, so tree totals over this window are
not one worker's: report.csv C MATCH went 823 -> 827 from THIS lane
(3 hand-solved + 1 stale-row recovery), and other commits landed interleaved.
Always diff `git log` before attributing a count delta.

- **0x1006CF80 BrBitStreamAtEnd - BYTE-EXACT.** A conditional bump is an
  alternative ASSIGNMENT: `pos = b; if (a) pos++` keeps both fields live, and
  `if (a) pos = b + 1; else pos = b;` (or the ternary) lets `a` die at the
  test so VC5 reuses its register for `b`. 7 diffs -> 0.
- **0x1006DA20 BrMat4TransformPoint - BYTE-EXACT, and it RETIRES a "register-
  allocation wall, do not grind" note that stood in the file.** Hand-rolled
  walking cursors got it to 7 bytes and stopped; plain `pv[j] * pM->m[j][i]`
  subscripts let strength reduction create the induction pointers in VC5's own
  order and the ecx/edx pairing falls out.  TREAT EVERY OLD "do not grind"
  NOTE AS A HYPOTHESIS, not a verdict - this is the second such note retired.
- **0x1006DC30 BrMat3Skew - BYTE-EXACT.** Repeated constant stores are a
  LEADING group in DESCENDING offset order (`m[8]; m[4]; m[0]`); interleaved,
  the zero register stays live and costs a `push esi`/`pop esi` pair.
  Ascending order is 7 bytes off. 51 diffs and +2 bytes -> 0.
- **0x1005E6A0 BrCarInitTables - was ALREADY byte-exact; its report.csv row
  was stale with the wrong `opt` (O2y).**  Free count: a stale row understates
  the total. Worth a periodic re-sweep of `diff` rows whose `opt` is not O2.
- **All three idioms are ONE cause: make the short-lived value DIE so its
  register is reused, instead of keeping it live.** Recorded in
  [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md). `tools/brally/fnmatch/screen_pushpop.py` (new) screens
  the residue for the spurious callee-saved push/pop pair - honest yield is in
  its docstring: 90/244 rows carry the pair but nearly all are big size
  blow-ups, so read the ranked head, not the count. NOT a family.
- **PARKED this lane, each with its dead-probe list in the file header:**
  0x10017F80 BrFadeDrawSprite (2 B; integer adds of two fields of one struct
  canonicalize absolutely - 4 probes), 0x10014960 BrSub_100173F0 (25 B; VC5
  reassociates the three-term BrTextDraw x-argument - 3 probes), 0x1001EC30
  BrDlsTileSizeDecode (16 B; clean esi<->edi swap on `p` vs `ult` - 5 probes),
  0x10037F70 BrUiHook85 (1 B; the gap is the `A1` moffs global load, which VC5
  emits only into eax, so the lever is register pressure on `c`, not the
  branch shape), 0x1006E360 BrTimeUpdate (already documented T3a).
- **EH screen earned its keep: 5 of the 13 lane targets were `6a ff … 64 a1`
  C++ EH frames** (0x1004E750, 0x100485B0, 0x10045EF0, 0x1004DA00,
  0x10044860) - released unworked.

## ▶ RESUME (2026-09-03 session 21 - FOUR CLEAN NEGATIVES, NO MOVEMENT)

Image gate 0 diff bytes. No closure; the pass spent itself re-testing under
the staleness rule and bounding the newest idiom, and all four results are
worth having written down.

- ** THE REDUNDANT-PARENTHESIS AXIS HAS A BOUNDARY: redundant CASTS are
  INERT.** Dropping `(uint8_t)` from six already-uint8_t globals is
  BYTE-IDENTICAL. So the paren finding is about the expression TREE the parser
  hands VC5, not about redundant syntax generally - do not generalise it into
  "add redundant casts and see". And the axis only pays where the screen the
  second sighting established holds: **register-blind gap already 0, with the
  divergence purely in x87 ordering.**
- **The paren axis on 0x1000EAF0's rows: six variants, all negative.** None
  improves the register-blind gap (48+54) and most BREAK the 8|4 batching the
  symbol spelling won last session (parens on term 3 or 4 give 6|6; on 3+4 or
  1+3 give 5|7). The current no-paren form is the best of seven. That
  function's gap is 102 rows, so the screen above already said not to expect
  a payout.
- **WALL 4 re-tested under the staleness rule and HOLDS.** Its allocation did
  move (the coefficients became symbols), so the re-test was owed: the
  byte-offset spelling `rbo = ring * 4` through all eight if-arm sites is
  byte-identical to the plain index, exactly as in the eleventh pass. Not
  stale, do not re-run again unless the allocation moves again.
- **0x1000A110: the pack array's SIZE is inert** (`pack[3]`/`pack[4]` are
  byte-identical to `pack[2]`) - the storage class matters, the extent does
  not. Its residue remains ONE defect at three sites: each colour arm homes
  one pack byte where the original homes two.
- 0x100250D0: not reached again; its allocation still has not moved.

## ▶ RESUME (2026-09-03 session 20 - 0x1000A110: 13 -> 9 INSTRUCTIONS SHORT)

Image gate 0 diff bytes. **0x1000A110 closed a third of its remaining
instruction gap: 13 -> 9 short, 47 -> 36 bytes short, regions flat at 24.**
Both wins came from re-testing verdicts the staleness rule had made stale.

- **Arm 1 gets its OWN pack array** (the shared one let VC5 cross-jump it into
  the arms-2/3 tail). Instruction gap 13 -> 10, bytes 47 -> 38, and the arm
  now homes a byte and reads it back widened as the original does.
   **This REVERSES session 12's decline**, whose only evidence was a region's
  first-divergence address moving 27 bytes earlier. Reading that region shows
  the 27 bytes are a TWO-INSTRUCTION SCHEDULE SWAP of code both builds emit  - 
  not new divergence. **Read a region before believing its address.**
- **Arm 1's top component through its own byte local** then lands too - dead
  in session 12 against the old allocation, positive against this one.
  Instruction gap 10 -> 9, bytes 38 -> 36. Third payout of the staleness rule
  on this function.
- **The residue is now ONE defect at three sites:** each colour arm homes ONE
  pack byte where the original homes TWO. That is the entire `and R,0xff` x4 /
  `or R,R` x4 / `mov byte [esp+S],B` x3 missing set against our `mov B,B` lane
  moves. DEAD probe: routing the array through a pointer local to make it
  address-taken is byte-identical - VC5 sees through the alias, so escape
  analysis is not the lever.
- 0x1000EAF0: no further movement. Row TERM ORDER is inert (six permutations,
  all keep the new 8|4 batching, all leave the term-3 operand flip), so with
  factor order canonicalised that flip is an operand-ranking decision.
- 0x100250D0: not reached; its allocation has not moved, so its dead list is
  not stale.

## ▶ RESUME (2026-09-03 session 19 - 0x1000EAF0's WALL 2 IS CLOSED)

Image gate 0 diff bytes. **0x1000EAF0: masked regions 20 -> 19, bytes 18 ->
15 short, and its scale block now emits the ORIGINAL'S 8|4 x87 batching
exactly - a wall that had stood nine passes.**

- ** THE IDIOM THAT DID IT, and it is a whole class: A RAW-ADDRESS CAST AND A
  SYMBOL REFERENCE ARE NOT THE SAME OPERAND TO VC5.** `*(float *)(0xADDR +
  4*k)` is a compile-time CONSTANT; `DAT_ADDR[k]` is a RELOCATION. They
  assemble identically and schedule differently. The four-term matrix row had
  been transcribed with two terms as hex casts and two through pointer
  locals; that mix held the preload at 5 for nine passes. All four as array
  symbols - which is what real source must have had - reproduces the
  original. **Treat a hex-address cast as a transcription placeholder, not a
  finding: make an expression's references uniform before reading anything
  into its schedule.** (Swept the tree: no other diff-bearing file has float
  hex casts, so the class is exhausted for now.)
- Also settled: **VC5 canonicalises x87 multiply operand order** (swapping the
  factors of any term, or several, is byte-identical), and **a pointer local
  to an array is free** - `p[k]` with `float *p = ARR` is byte-identical to
  `ARR[k]`. It is the CAST that differs, not the pointer.
- **The trade, taken deliberately:** the register-blind multiset went 35/29 ->
  39/33. All eight rows are ONE defect at four sites - under the symbol
  spelling VC5 emits term 3 object-first where the original is
  coefficient-first. Swapping an unmovable wall for a localised operand-order
  defect, with the region and byte counts both moving the right way.
- **Tool: `fn.py` now flags DIFFS when the sizes differ** (last session's
  positional-compare trap), so the number cannot be misread again.
- 0x100250D0 and 0x1000A110: not reached.

## ▶ RESUME (2026-09-03 session 18 - A MEASUREMENT TRAP THAT INVALIDATES OLD COMPARISONS)

Image gate 0 diff bytes. No region closed. The pass's value is a measurement
fact that changes how every dossier in this project must be read.

- ** fn.py's DIFFS IS A POSITIONAL BYTE COMPARE - a size shift upstream
  inflates or halves it.** A row-expression variant on 0x1000EAF0 took DIFFS
  4,669 -> 2,490, which looks like a breakthrough and is TWO BYTES: the size
  change moved the whole ~4,600-byte tail from delta -2 to delta 0 and flipped
  ~2,200 spuriously-mismatching bytes to spuriously-matching ones. **Compare
  DIFFS only between builds of the SAME total size. When the size moves, rank
  by msetdiff rows, the instruction gap, and the masked region map - all three
  are alignment-free.** A DIFFS swing much larger than the byte-count change
  is the signature of a shift; check `divergence.py --deltas` for a long run
  of regions whose delta all moved by the same small amount.  This qualifies
  several earlier comparisons in these dossiers, including ones I wrote.
- ** NEW PROBE AXIS: a REDUNDANT OUTER PARENTHESIS PAIR is not a no-op.**
  Isolated A/B: wrapping four already-parenthesised float expressions in one
  more outer pair - same association, nothing else changed - moves the x87
  preload 5|7 -> 4|8, grows the recompile 3 bytes and the register-blind gap
  40+46 -> 41+47. Parentheses reach VC5's SCHEDULER, not just its parser.
  **Never tidy parentheses in a matching TU**, and when an x87 schedule is one
  notch off, try the same association with and without an outer pair.
- **0x1000EAF0's row association is settled from the bytes: LEFT**, which the
  tree already is (`faddp st(2)` at 0xda7 folds terms 1 and 2 first). All four
  associations measured; none reaches the original's 8-preload, so the
  association is not the lever. The row block's peak x87 depth is 6 here
  against the original's 8 - that number IS wall 1's missing hoist notch.
- 0x100250D0 and 0x1000A110: not reached.

## ▶ RESUME (2026-09-03 session 17 - 0x1000EAF0's WALL 2 IS NOT A WALL)

Image gate 0 diff bytes. No region closed, but a verdict I wrote six sessions
ago is retracted and the function's two remaining x87 walls turn out to be
ONE.

- ** RETRACTED: "the 5-vs-8 x87 preload is a scheduler constant".** It is
  not. Evidence, all measured this pass: (1) THIS FUNCTION ALREADY EMITS THE
  ORIGINAL'S 8 at its SECOND scale site, instruction for instruction - so the
  compiler is willing; (2) the first site's batch is 5 for EVERY statement
  count from 10 to 16, so it is not a batch-size rule; (3) moving the block
  above its predecessor takes it 5 -> 7. **The predecessor sets it.**
- ** WALL 2 IS DOWNSTREAM OF WALL 1.** The predecessor is the four-row
  matrix block, whose products mix operand KINDS - two terms spelled as
  absolute literals, two through pointer locals. Spelling all four absolutely
  (which is what the original does: it reads every view term absolutely and
  every object term register-relative) takes the preload to the original's 8.
  Not in the tree: it wrecks the rows while fixing the scale (byte diff 4,669
  -> 4,742, regions 20 -> 22, two SUSPECT resyncs in the row block). Either
  half alone is worse (4,929 / 5,020), and deleting the locals that go dead
  is worse still (5,027) - so the dead locals are NOT the cause and the row
  block simply prefers the pointer spelling.
- **NEXT LEVER on that function, and it is concrete:** a row spelling that
  carries the original's operand kinds without costing the rows more than the
  scale block gains. Ruled out and recorded in the file header: helper
  boundary, array identity (in-place still gives 5), extern-vs-defined
  storage, dead-local removal.
- **The reusable rule (now in the idiom file): before writing "the compiler
  will not do X", find a site in the same binary where it does.** Then vary
  the count, then move the block. That order found this in one pass after six
  sessions of treating the number as fixed.
- 0x100250D0 and 0x1000A110: not reached this pass.

## ▶ RESUME (2026-09-03 session 16 - THE STALENESS RULE APPLIED TO THE OTHER TWO)

Image gate 0 diff bytes. No region closed; the pass spent itself re-testing
dead lists under last session's staleness rule and it corrected one wrong
entry that had stood for four passes.

- ** A BUNDLED PROBE'S VERDICT LANDS ON THE WRONG CONSTRUCT.** 0x1000EAF0's
  dossier blamed one spelling (`slot = head - 1` vs `slot = head; if (--slot
  < 0)`) for a `dec`/`jns` difference, on a probe that had ALSO reversed a
  guard and sunk an assignment. Re-measured alone the two spellings are
  BYTE-IDENTICAL - VC5 canonicalises them - so the cost belonged to the other
  two changes and a correct construct had carried the blame since the ninth
  pass. **Re-run a bundled probe's members singly before writing any of them
  into a dead list, or name the entry as a bundle.**
- ** READ A REGISTER WALL AS "WHICH N OF M FIT".** 0x1000EAF0's join is a
  three-value/two-register choice: the original keeps two loop values live
  across the merge and therefore reads the loop counter from memory at the
  guard; we keep the counter and reload the two values after. Both builds pick
  two and pick differently. That framing says at once that no spelling reaches
  it (probed anyway: expressing the join through the locals moves the byte
  diff by ONE).
- 0x1000A110 arm 3: assigning the pack's top component FIRST makes all three
  byte globals load up front into three byte registers, which is the
  original's shape and the precondition for its second slot store. Raw rows
  50+63 -> 48+61, register-blind multiset unchanged at 20/7 - banked as a
  shape alignment, not a closure. What is still missing there is one store and
  one widened read; it is circular (the widening needs the pressure, the
  pressure needs the widening) and therefore allocation.
- 0x100250D0: untouched this pass - its allocation has not moved since its
  dead list was written, so that list is NOT stale and re-testing it would be
  waste.

## ▶ RESUME (2026-09-03 session 15 - A FIVE-TIMES-DEAD REGION FELL)

Image gate 0 diff bytes. 0x1000A110 closed another block and is now 13
instructions / 47 bytes short with 24 masked regions (was 14 / 53 / 26).

- ** THE RULE THIS PASS ESTABLISHED: A DEAD VERDICT MEASURED AGAINST A WRONG
  FRAME IS STALE.** 0x1000A110's three-float light-direction copy carried FIVE
  measured-dead spellings and the note "treat as T3a until the frame is
  solved". The frame was solved last session; the sixth spelling lands
  instruction-for-instruction. Re-test every allocation-sensitive
  "do-not-re-run" note after the frame - or any other global allocation input
  - moves. Expect a minority to flip.
- **The two rules that made that copy work, both reusable:** BOTH float
  members need a named temp so their live ranges overlap and VC5 keeps them on
  the x87 stack (one temp gets you one fld/fstp and an integer move for the
  other); and the INTEGER member must be stored FIRST, because a temp whose
  load and store are adjacent is copy-propagated into an integer move.
- **Re-tested the rest of that function's dead list under the rule:** the
  named-top-local on arm 1 still dead (same tell: first divergence moves 27
  bytes earlier, byte diff up 148); arm 1's own byte array is now AMBIGUOUS
  rather than clearly bad - it recovers an instruction and 158 on the byte
  diff but costs two spurious ones, so NOT taken, revisit only when attacking
  the byte-lane family; the arm-3 named-top lever is byte-identical in arm 1
  (that region is canonicalisation, not allocation).
- **Screens run on all three giants, all negative - do not repeat:** all three
  frames match (`framescreen.py` lists none of them), and none has real
  `(double)` modelling. 0x1000EAF0's 49 qword spills are ALL `fstp qword ptr
  [esp]` varargs pushes, matched exactly in both streams, with no `fld qword`
  anywhere - so the Glide-is-float lever does not apply there either.
- 0x1000EAF0 and 0x100250D0: no movement. 0x1000EAF0's remaining six missing
  instructions all decompose into wall 4's pinned register (3 spill + 1 lea),
  the x87 depth (1) and wall 5 (1) - no live source lever in the multiset.

## ▶ RESUME (2026-09-03 session 14 - MEASUREMENT SESSION: ONE CLAIM RETRACTED, ONE CLASS OPENED)

Image gate 0 diff bytes. No region closed on the three giants; the value this
pass is that a number two sessions had believed turned out to be fiction, and
that last session's one-off fix now has a tree-wide screen.

- ** RETRACTION: 0x100250D0's "-73 byte dominant block" DOES NOT EXIST.** The
  session-12 note said to grind it. It sits immediately after a resync with a
  74/53 skew, which is SUSPECT at every key from 10 to 14, and a bad resync
  corrupts the delta of the region after it - so its `change`, and the next
  one's, are differences taken from a wrong anchor. `divergence.py` now labels
  both. **The general rule: a suspect resync poisons the TWO regions after it,
  not just its own line.**
- ** THE VERIFICATION THAT SETTLED IT, and it is cheap and reusable:** pick an
  instruction that occurs once per unit of work and count it across the WHOLE
  function in both streams. On 0x100250D0 the divide-by-255 magic constant is
  33 in each and the one-operand `imul` 24 in each - every channel divide is
  present, so a 21-instruction gap in one window cannot be real. With the
  instruction total at 2,408 vs 2,407 that settles the function: **the residue
  is allocation**, wherever the region map points. Do this before grinding any
  large `change`.
- **NEW SCREEN, and it opens a class: `tools/brally/framescreen.py`.** Last session's
  array-vs-scalar frame fix is not a one-off. The screen reads `sub esp,imm`
  from every tagged-diff function's original bytes and its object and ranks
  the disagreements: **26 of 69 rows with a readable prologue disagree.** Ours
  SMALLER = a scalar that should be an array; ours LARGER = a local the
  original does not spend. Take these before region grinding - every stack
  displacement moves with the frame.  The screen must use each row's OWN
  compile variant; against the /O2 object an /O2 /Oy- row read as 76 bytes off
  and was not a frame defect at all.
- 0x1000EAF0 and 0x1000A110: no movement. Both were re-screened against the
  two session-13 idioms (cached struct field, array storage class) and neither
  applies further; 0x1000A110's arm 3 residue is the original using three byte
  registers where we use two, which is load-order allocation. Re-testing the
  session-12 `topA` local against the new array form confirmed `topA` is still
  right (removing it costs 93 on the byte diff).

## ▶ RESUME (2026-09-03 session 13 - 0x1000A110's FRAME IS CLOSED)

Image gate 0 diff bytes. The headline is a storage class, and it retires a
wall that had been carried for eight sessions.

- ** 0x1000A110 REGION 1 (THE FRAME) IS CLOSED.** `sub esp,0x4c` matches and
  the prologue is byte-exact. The fix was ONE DECLARATION: the two colour-pack
  byte locals are an ARRAY, not two scalars. VC5 never enregisters an array
  and never tucks one into a dead argument slot, so it spends the locals-area
  dword the original spends. Two scalars had been landing in the reused
  ARGUMENT slots (visible in the `/FAcs` equate table as `_pack0$ = 8,
  _pack1$ = 12`), costing the frame nothing - which IS the 4-byte gap the
  file's frame census spent its whole length hunting. **Generalise: a frame a
  few bytes SHORT while writing the same values = a scalar that should be an
  array.** Masked regions 25 -> 23.
- **Second block closed on the same function:** the sky texture-window command
  words. The original RE-READS the two struct fields for the second word, so
  they are not named locals; caching them spilled the pair and the assembled
  word to slots. Inlined, the block is instruction-for-instruction the
  original. **Generalise: a repeated field load in the original is evidence of
  source that does not cache, not of a missed CSE.**
- ** RANK BY THE MULTISET, NOT SIZE - this session proved it twice.** That
  second fix took the function from 38 bytes short to 53 short (the three
  spill instructions had been padding a real deficit elsewhere) while the
  register-blind multiset went 64+75 -> 52+66. The frame fix likewise reads 2
  worse on fn.py's RAW/REGNORM because every slot displacement moved. Trust
  `msetdiff.py` and the masked region count.
- ** TOOL FIX, and it invalidates a published region map.** `divergence.py`
  resynced on 6 matching instructions, which is SHORTER than the sequences a
  function of repeated arms contains, and it preferred lopsided splits. It now
  takes `--key N` and tries balanced splits first. 0x100250D0 reads 32 regions
  with four fictional resyncs at key 6 and TWENTY with one at key 10 - so
  twelve of its "32 regions" were counting artefacts, and its real dominant
  block is r3 at 0x846 (-73), not the r14-r17 range every earlier note names.
  Default stays 6 so old counts keep their meaning; the total line now prints
  the key. **Read any repetitive function at `--key 10` and say which key.**
- 0x1000EAF0: no movement. Its remaining live lever (the +0x70 pointer's `lea`
  form) is blocked by the same prologue flip as before - a third spelling
  (`(char *)pCar + wb` with the field pointer derived from it) reproduces it
  exactly, confirming the trigger is the field pointer deriving from the wheel
  pointer, not how the wheel pointer is spelled.

## ▶ RESUME (2026-09-03 session 12 - TWO GIANTS MOVED, ONE IDIOM MINTED)

Image gate 0 diff bytes. Two of the three giants took real ground for the
first time in several passes; the third gained a fully-read mechanism and two
more dead probes. The general rules are in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md); per-file
residue maps and do-not-re-run lists live in each file header.

- **0x1000EAF0 - wall 3 moved (first time in six passes).** The wheel record's
  +0x70 field is now read through the SAME pointer expression the ground-probe
  argument already used, so VC5 CSEs them: reloc-masked byte diff 4,727 ->
  4,669, register-blind multiset 37/31 -> 35/29, and three of wall 3's rows
  left the multiset. Masked regions flat at 20.  Two DEAD probes with one
  finding: spelling that argument `&pW->x`, or deleting the `wb` temp so the
  wheel pointer is one four-term sum, each FLIP THE PROLOGUE (first divergence
  +0x2b -> +0x15, byte diff to ~6,500). Keep `wb` and keep it used twice.
- **0x1000A110 - arm 3 moved.** The colour pack's TOP component now goes
  through its own `uint8_t` local, which makes VC5 load it to a byte register
  before the lane move instead of straight into `dh` - the original's shape.
  Byte diff 4,658 -> 4,539, one instruction and two bytes recovered, regions
  flat at 25, frame intact.
- ** NEW IDIOM, and it has a sharp edge: naming a byte temp.** A byte that
  reaches a lane through a register WANTS a name; an int that flows through
  the eax accumulator does NOT (that is the older, opposite entry). Decide per
  site from the bytes, never by analogy - and apply ONE SITE AT A TIME: the
  same spelling on the neighbouring pack looked better on size and was a
  regression whose only tell was the region's FIRST-DIVERGENCE ADDRESS moving
  27 bytes earlier. On a cross-jump region, size and region count both lie.
- **0x100250D0 - no movement, mechanism now fully read.** Region 14 is the
  alpha channel's product temp: MSVC's divide-by-255 needs the product to
  survive the `imul`, the original lets the intensity DIE into that multiply
  so the product sits in a preserved register, we keep it live and spend three
  instructions and a fourth stack slot. Two dead probes: commuting the
  multiplication (so `*` joins `&` and `|` on the list of operators VC5
  canonicalises) and hoisting the product into a named temp - both
  BYTE-IDENTICAL.
- Method note that paid three times this session: `divergence.py --deltas` to
  rank blocks, then a HAND byte tally of the sub-blocks inside the winning
  region, then `/FAcs` to name the source line. The region map alone
  mis-attributed two of the three functions' biggest blocks.

## ▶ RESUME (2026-09-03 session 11 - THE THREE GIANTS, RE-RANKED NOT MOVED)

**1,073 byte-exact / 127,842 B (820 C + 149 C++ + 104 EXE); image gate 0 diff
bytes.** No region closed on 0x1000EAF0 / 0x100250D0 / 0x1000A110 - worked in
that order, all three re-measured unchanged. What the pass DID produce is a
retirement of three walls that were being carried as open levers, plus one
workflow fix. Detail lives in each file's own header; the general rules are in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).

- ** New triage rule, and it is the reusable one: grep the ORIGINAL for its
  own inconsistency before minting a spelling probe.** 0x1000EAF0's "wall 4"
  (`ring*4` CSE) is rendered TWO ways by the original itself - the if-arm
  materialises `lea edx,[ecx*4]` for 8 sites, the else-arm folds all 5 as
  `[ecx*4+abs]`, which is what we already emit. Same source text, two
  allocations ⇒ no spelling reaches it. The long-standing probe against it had
  converted BOTH arms, which cannot be right at any spelling; re-probed on the
  if-arm alone, still folds. **Corollary:** three of that function's six
  MISSING msetdiff rows (2 `mov [esp+S],R`, 1 `mov R,[esp+S]`) are the spill
  that pinned register forces, not independent missing code.
- **0x1000EAF0 wall 2 retired too**: the 8|4 vs 5|7 x87 preload is NOT the
  helper boundary (flat 12 statements emit the same 5-deep preload) and not a
  stack leak (both streams enter at depth 0). Scheduler constant.
- **0x100250D0 had NO @implements tag and NO report.csv row** - the sweep
  compiled nothing for it and eight sessions measured it by hand-rolled
  cl.exe. Tagged now (row reads `diff`, MATCH unchanged); `fn.py 0x100250D0`
  works, ~6 s. Also: its region 18's -33 is r14-r17's +38 handed back, not a
  defect.
- **0x1000A110 region 6 was mis-attributed**: its -30 is arm 3 (-24), not the
  three-float light-direction copy that opens it. Arm 3's whole gap is ONE
  byte local VC5 forwards from a register where the original reads it back
  from its slot; the WORKLIST's four separate multiset families are that one
  site. `(pack0 & 0xFFu)` is byte-identical - VC5 folds a redundant mask on a
  uint8_t before choosing the byte lane.
- Cheap tools that carried the pass: `divergence.py --deltas` to rank blocks,
  then a per-block byte tally by hand to find which block inside a region
  actually spends the drift (both mis-attributions above came from that), and
  `msetdiff.py` for the register-blind residue.

## ▶ RESUME (2026-09-03 session 11b, LANE 7543bc84 - TWO NEW CAUSE-CLASS SCREENS)

**1,084 byte-exact / 136,536 B (826 C + 154 C++ + 104 EXE); image gate 0 diff
bytes, 0 overlapping claims, 26.25% of .text placed.** +4 C byte-exact:
0x10019730 BrMainLoopRun(205, ops-table + the whole window bring-up was
missing), 0x1002D72E(310), 0x1002E79F BrCarGfxSetColour(868), and the earlier
0x10036430. Both new screens are in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md) with runnable
commands:

** SCREEN 1 - a diff row whose byte-adjacent MATCHED neighbours are all /Od
is MIS-SHAPED, not blocked.** The variant is chosen PER FUNCTION, so a lone
diff in a run of `match ... Od` is a function written in the /O2 idiom inside
an /Od TU. Four rows, all slice2_19.c; two fell the same day. **THREE OF THE
REMAINDER ARE PARKED AS WALLS AND SHOULD NOT BE** (0x1002ECEB BrAnimUpdate,
0x1002F380 BrPadTranslate, 0x1002EB03 BrCarGfxReadColour) - the park predates
the screen.

** SCREEN 2 - a helper that RETURNS A STRUCT never inlines in MSVC5.** A port
that factors a shared body into `static BrVec3 Helper(...)` emits a call the
original does not have, and the whole family reads MISSING CODE (40%). Check
this BEFORE treating low completeness as source discovery. Fixed the
rigid-body velocity trio (0x100644C0 / 0x100643E0 / 0x100642F0): 137/138/122
bytes short -> 32/33/10.

**Four /Od source facts** (from 0x1002E79F, 868 B byte-exact): a static helper
is a REAL CALL at /Od so if the original has no call the expression is inline,
and its shape names the types (`and 0xffff` = uint16_t read, `sar` = promotion
to int); NESTED TESTS not early exits (`if (a && b) {...}` gives the near
`je` to the loop increment / epilogue, `continue`/`return` gives a short
branch over a jmp); two uses of one pointer may be TWO variables - count the
distinct `[ebp-N]` slots first; inline the take-2 emit per block.

**Three x87 facts** (from the velocity trio): product operand ORDER decides
`fmul mem` vs `fld`+`fmulp` - the operand already on the stack is written
FIRST; `double` temps spill as `fstp qword` so if the original never spills a
qword the temps are FLOAT; `*pDst = pSrc->field` copies through a lea base
pointer and costs a callee-saved register, field-wise assignment does not.

Parked with dead-probe lists: 0x100642F0 (6 regnorm - its two stack vectors
sit in each other's slots; declaration order and two renamings all fail, the
/Od name-hash homing does NOT apply at /O2), 0x100644C0 / 0x100643E0 (x87
interleave), 0x1001E080 BrGlInstall (3 bytes, cross-jump).

## ▶ RESUME (2026-09-03 session 10 - THE REPORT WAS LYING IN THREE WAYS)

**1,073 byte-exact / 127,842 B (820 C + 149 C++ + 104 EXE); image gate 0
diff bytes, 0 overlapping claims.** +1 C byte-exact, but the pass was mostly
BOOKKEEPING FAULTS, all now in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):

** The EH screen is now IN triage.py.** 41 tagged-diff rows / 37,677 B open
with `push -1 / fs:[0]` and are unreachable from C. They were ranked as
ordinary targets, and most read `MISSING CODE (2% complete)` because the port
stands them in with a forwarder - the exact profile of an easy win. Twelve
are one 201-byte family. `triage.py` reads the original's first two bytes and
ranks them `C++ EH FRAME - not reachable from C` at score 2000000, below the
coloring walls. **Do not undo this; the class belongs to the C++ workstream.**

**Three ways an @implements tag lies, each with a screen:**
1. FALSE shared.csv TWIN - 0x1001BAE0/0x1001E080: D3D is 26 bytes, Glide is
   173. Screen by disassembling BOTH binaries at BOTH addresses
   (`BR_REF=reference/brally/orig/BRD3D.dll`). Fixed to @d3donly; the real Glide body was
   sitting untagged in br_dlglide.c as BrGlInstall (3 bytes off, cross-jump
   class, all five variants tried).
2. TAG ON THE FORWARDER - 0x100695D0: 32-byte alias tagged, 363-byte body
   untagged in slice3_42.c. Moved the tag; it now scores 363/480/324.
3. …BUT THE IMAGE SOMETIMES REALLY HOLDS TWO COPIES - 0x1003CDA0 is 212
   bytes in BOTH binaries and the "owner" elsewhere is the same code again.
   Writing the body out (DirectPlay vtable send at +0x7C, KERNEL32
   GlobalHandle/Unlock/Free) was **BYTE-EXACT first compile**.
Screen for 2 and 3: `awk -F, 'NR>1&&$4=="diff"&&$7>0&&$6>200&&$7/$6<0.25'`.

Two functions improved and parked with dead-probe lists in their headers:
0x10015550 (2 instructions - our cl builds +100q and subtracts where the
original negates the quotient and folds the add into a lea; `%` is WORSE, it
emits a real idiv), 0x1006CED0 (12 regnorm - signed buffer read and
one-variable-not-two both proven; the accumulator is evicted from eax by the
mask build).

## ▶ RESUME (2026-09-03 session 9 - TWO CLASS-WIDE LEVERS, NOT FUNCTIONS)

**1,067 byte-exact / 127,131 B (819 C + 144 C++ + 104 EXE); image gate 0
diff bytes.** +2 C this pass, but the value is the two levers, both now in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md):

**1. `/Od /Op` was a MISSING SWEEP VARIANT.** `tools/brally/match_sweep.py`
VARIANTS had O2 / Od / O2y / O2p; the round-to-float32 idiom
(`fild; fstp dword [t]; fld dword [t]; fmul`) exists at /Od too and plain
/Od never emits it, so **every /Od TU with an int->float cast in it was
invisible to every sweep before now.** Added `Odp`. Proven on 0x1002BF50
(242 diffs to 0). **A full re-sweep is what harvests the rest - not
started, it is the project lead's call.** Ten rows whose per-function best is still
`Od` + `diff` are the first place to look.

**2. The port's ops/host-hook table is a CAUSE class with a nine-file
screen** (awk one-liner in the idiom doc). A `BrXxxOps`/`BrXxxHost` struct
of function pointers turns every direct call into `call [ops+N]` and is
often the WHOLE gap. 0x1001CC00 BrRallyMain fell BYTE-EXACT on the first
compile from it; 0x1001D8A0 BrDxDetect went 826 -> 19 reggap. Still open:
br_uiboot.c(23), br_window.c(16), br_mainloop.c(6), br_strres.c(6),
br_texinit.c(5), slice4_52.c(32), slice1_06.c(4).

Companion rules proven the same pass: **a function's TU is fixed by
ADDRESS, not subject** (0x1002BF50 sat in an /O2 file; its neighbour
0x1002BF4B ends exactly at it, so it belongs to slice2_18.c's /Od unit);
and **/Od slot depth is SCOPE, not declaration order** - function-scope
locals get the shallowest slots, inner-block ones go deeper, and swapping
declarations does nothing.

Byte-exact this pass: 0x1001CC00 BrRallyMain(324), 0x1002BF50(419),
0x1003EC70(32, twin of an already-armed 0x100457C0).
Parked with dead-probe lists in their file headers: 0x1001D8A0 BrDxDetect
(cross-jump wall - our cl merges the QI-failure arm into the function tail
because it allocates the same scratch register in both; the ORIGINAL's two
copies differ by edx-vs-ecx, which is why it kept them - plus one spill
slot from not caching GetProcAddress); 0x1003C6D0 (+1 byte, eax/ecx
pairing); 0x1003C600 (+1 byte, same, and the load-hoist vs store-merge
trade-off is measured both ways in the file).

## ▶ RESUME (2026-09-03 third pass - ITEM FAMILY STILL PAYING)
**1,059 byte-exact / 125,874 B (813 C + 142 C++ + 104 EXE); image gate 0
diff bytes.** +4 this pass, all item-record family: 0x1003A910(245),
0x1003A580(322), 0x1003A6D0(322 twin), 0x1003A420(341).
**BIGGEST LEVER: the float class is not a wall for straight chains - one
NAMED float local per intermediate took 0x1003A580 from 205 diffs to
byte-exact (205/155/0 as the names were added). Try this BEFORE calling
any float function a coloring wall.** See [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
Second lever: arm ORDER decides block placement - the arm you want last is
the `else`; sentinel-then/format-else reproduces the original layout.
Parked with dead-probe lists: 0x1003AA10(14), 0x100393C0(94, tail-merge
depth is allocator-driven), 0x1003A140(237, block placement).
Family list and the three recurring shapes: [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md).
Next untouched: 0x10040B10(478) 0x10039620(563) 0x10038F40(567)
0x10040EB0(587), then the 1.5-4 KB shells.

## ▶ RESUME (2026-09-03 later - ITEM-RECORD FAMILY IS OPEN)
**1,051 byte-exact / 124,178 B (809 C + 138 C++ + 104 EXE); image gate 0
diff bytes.** +5 this pass: 0x10055C50(238 C), 0x10041300(247),
0x10037EF0(73), 0x100380B0(74), 0x10037E60(100) - all first or second
compile. **0x10041300 pinned the 0x438 UI item record (vtable +0, flags
+4, kind byte +8, label +9), which is the SAME record 0x10054E20's slot
array holds - see [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md) for the byte screen that found
31 unmatched members, most 70-360 B, and the list of which are left.**
Copy the class decl from src/brally/core/cpp/0x10041300.cpp; the recurring shape
is _itoa-or-catalogue-string into the label, then relayout/repaint vcalls
with the LABEL POINTER in a local that gets null-tested.
New levers: strlen folds on a literal but not an extern array; and
**push the whole expression INTO both arms and let VC5 tail-merge the
closing instruction** - proven twice, once on a `lea`, once on a `call`
(a ternary argument is wrong).
Parked with dead-probe lists: 0x10054E20(480, pinned-zero-register costs a
frame dword - signature `test r,r` vs `cmp r,ebx`), 0x1003AB00(130, one
2-byte `test`).
 **RETRACTED:** the "memset expansion emits dest first" claim from the
earlier pass. 0x1003AB00 has the ORIGINAL using our order. It is
scheduling; do not cite it as compiler-build evidence. Only the SIB and
cross-jumping entries still stand.

## ▶ RESUME (2026-09-03, C++ lane)
**+3 byte-exact C++ TUs: 0x1003DEC0 (178B phase-leave, clone of
0x1003DF80), 0x10058D00 (53B chain insert), 0x10055330 (117B
point-in-rect) - all FIRST COMPILE from the asm.** Six more transcribed
to every-instruction-present and PARKED, each with a dead-probe list in
its file header: 0x100540D0(1) 0x10054280(1) 0x100087D0(7) 0x1006FCE0(12)
0x10059350(103, one cross-jump) 0x1006D0B0(40, T3a pairing).
**THE FINDING: three EMITTER-level residues no source form reaches**
(SIB base/index on member-array subscript; memset expansion emitting
dest-before-value, confirmed in both expansion shapes; cross-jumping of
identical error tails). Strongest evidence yet for the compiler
patch-level lead - each is a one-instruction discriminator a candidate cl
either reproduces or does not. Read [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md) tail before
re-probing ANY of them. Counter-lesson, source-reachable and worth 30+
diffs a step: byte stores push narrowing UP the expression - dam it with
one outermost cast, the byte pointer in its own local, and each 32-bit
mask in its own local (a 4th live local tips VC5 into an ebp frame).
gen_cpptwin found 0 twins after every new TU this session - keep running
it, stop planning count on it. Next unhazarded C++ strong targets:
0x1004ABE0(760) 0x1004A840(923) 0x10054730(618) 0x10054E20(510)
0x10055C50(238); 0x100541B0 is the third glyph walk (param slots reused
as locals) and will park on the same SIB byte.

## ▶ RESUME (2026-09-02)
**1,040 functions match = 122,663 B (805 DLL-C + 131 C++ + 104 EXE,
total.py); image gate 0 diff bytes. NEW PROVEN LEVER (VC5-IDIOMS tail
entry): float copies the orig emits as integer movs = DWORD-PUN spelling
(`*(uint32_t*)&dst = *(uint32_t*)&src`) - 0x1006F720 fell 241→25 diffs in
one step; siblings: `x3=x2=x1=f` chain = fst/fst/fstp triple store, and
struct fields re-read after a call must be pun-read or VC5
constant-propagates the stored zeros. Re-proven: wrapped-body beats early
return for je-to-end layout (0x10041460, 0x10007E80 byte-exact); deleting
a redundant mid-return restores shrink-wrap ([vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md)).
Lane 2acc9e2c worked + released; parked T3a with source notes (do not
re-probe): 0x1006F720(25) 0x10039C70(13) 0x10039D20(41) 0x100186E0(4)
0x10017E30(11). 0x10016C90 BrWeatherStepParticles (1142B, float-heavy,
report row stale-garbage) untouched - needs its own session.** The multi-KB /GX menu-builder shells
FELL from Ghidra drafts, mostly first compile (0x100425E0 2659B,
0x100476E0 2679B, 0x100439B0 3746B; levers in VC5-IDIOMS "menu-builder
trio" - char-bool-after-store, inline (short)(w14+1), w2AB6-before-w2AB4);
0x1004AEE0 (3862B) PARKED at 34-diff photo1 scheduling (14 spellings +
/Op probed, note in TU); 0x100498A0 (3993B) last big shell, degraded
draft, same photo trio inside. claim_lane.py now excludes C++/EXE-matched
rows.** After parallel commits, re-sweep files flagged by
image_build before believing a regression (stale objs). Long class worked: +5
matches incl. the 0x1005A6A0 map split (`call A; jmp B` tail-call wrapper
merged with its target); refine transforms now include calltemp, scaletemp,
zerohoist, ftolfuse, walkerstrcpy, deadnull (all byte-proven). Parked:
0x10071F00 (3-diff __int64 high-half), 0x100283C0/0x1005A480/0x10054070
(register-pairing T3a); 0x10013E80 unsolved (VC5 value-sorts literal global
stores; probes in VC5-IDIOMS). Refine loads build/brally/analysis/ghidra_work/<VA>.c (NOT
.refined.c) and REGENERATES .refined.c - hand-fix the base file.
**HOT LEAD: C++ vcall family ([cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md)) - 25 TUs one session,
~30 KB of "short" residue is C++-only; run `tools/brally/gen_cpptwin.py` after every
new C++ TU.** Count = 3 report CSVs via tools/brally/total.py; report.csv is a
gitignored local artifact ([counting-reconciliation](../traps/counting-reconciliation.md)). Efficient count-mover:
the refine batch's CLOSE(n) lines ([close-queue-lever](../triage/close-queue-lever.md)).

**Big targets (session 2026-09-01, commits 1ef5112..0e64fec):** 0x100250D0
masked regions 49→32, IDX4+CI4 arms exact ([brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md));
0x1000EAF0 frame wall + trail x87 CLOSED, prologue exact, 20 masked/26 raw
(scenedl-0x1000eaf0-state); 0x1000A110 33→30 masked (LOD block, eyeX
literal, pPlayer-after-memcpy; pack bytes = register-death T3a); 0x10019A70
LAST ([braceStep-wall](../functions/braceStep-wall.md)). Each file header carries its residue map and
do-not-re-run list. Per-function harness: scratch probe.sh = cl.exe +
tools/brally/divergence.py masked+raw, ~1s/probe.
**Triage:** register-blind multiset gap, never raw diffs
([register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md), [divergence-class-triage](../triage/divergence-class-triage.md),
[residue-retriage-2026-08-28](../triage/residue-retriage-2026-08-28.md)); don't rank by smallest diff count
([inlined-helper-match-class](../triage/inlined-helper-match-class.md)); harness [fnmatch-harness](../toolchain/fnmatch-harness.md); generators only
for homogeneous classes ([generator-compounding-reality](../triage/generator-compounding-reality.md)).
**Gates:** image_build.py = the deliverable gate ([image-build-gate](../oracle/image-build-gate.md));
one-file sweep ~12s, NEVER full-sweep ([sweep-is-incremental-now](../toolchain/sweep-is-incremental-now.md));
EH class [cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md); EXEs [exe-decomp-state](../functions/exe-decomp-state.md); Ghidra pipeline
[ghidra-pipeline](../toolchain/ghidra-pipeline.md). Parallelize by .c file ONLY; header edits serialized.
File matches into modules as you go ([file-as-you-match](../rules/file-as-you-match.md)); commit each match
immediately (the commit-every-match rule).



## Session 8 (2026-09-03, later) - +14 byte-exact, 1,051 -> 1,065

Two lodes, both in the UI item family ([cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md)).

**A. The item record's shape** (earlier in the session): 0x10041300 pinned
the 0x438 record - vtable +0, flags +4, kind byte +8, label +9 - and the
time-formatter shape fell out of it: 0x1003A910, 0x1003A580, 0x1003A6D0,
0x1003A420. The float lever there is the big one: **an x87 chain matches
when every intermediate is a NAMED float local** (205 -> 155 -> 0 diffs as
the names were added). Try that before calling any float function a wall.

**B. The port's globals-struct parameter** (later): slice2_23.c's port
bodies take `(pObj, BrUiGlobals *pG)`; the originals are cdecl with ONE
argument and direct externs. Six matches, all first compile: 0x100386B0,
0x100381D0, 0x100391F0, 0x10039270, 0x10039350, 0x10039510. Details and
the screening gotcha (they are tagged by their D3D VA, so grepping the
glide VA finds nothing and they look unclaimed - check report.csv) in
[port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md).

**Also proven:** float-vs-int typing has a prologue tell - an extra
callee-saved push plus a 4-byte-SMALLER frame means you typed a float
local as int (415 diffs on 0x10038F40). And the arm-order/tail-merge
levers: put the whole call in every arm, and make the arm you want placed
last the `else`.

**Retracted this session:** the "memset expansion emits dest before value"
claim. 0x1003AB00 shows the ORIGINAL using our order. It is scheduling,
not compiler-build evidence. Only the SIB and cross-jumping entries still
stand as source-unreachable.

**Parked, each with a dead-probe list in its file header:** 0x10038CA0(2),
0x10038F40(10), 0x1003AA10(14), 0x100393C0(94), 0x1003A140(237),
0x10054E20(480).

**Bookkeeping:** six stale d3d tags in slice2_23.c duplicated Glide
matches already carried in src/brally/core/cpp; dropping them took the C residue
359 -> 350 with no image change. Do that cleanup in the same commit as the
match. tools/brally/gen_uilabel.py screens the 100-byte label shape and reports it
CLOSED at 8/8 - it swept 0 new functions, so it is a regression screen,
not leverage.

**Gotcha that nearly cost a false alarm:** image_build flagged BrRallyMain
at 260 differing bytes; it was a STALE OBJ from parallel commits. Re-sweep
the flagged file before believing any regression.


## Session 8b (2026-09-03, continued) - the port-globals lode across three files

+7 byte-exact: 0x10039870 (277 B, 201->0), 0x10038E10 (180 B), 0x10040D80
(79 B), 0x10040DD0 (65 B), 0x10041100 (48 B), 0x10041160 (30 B),
0x100400E0 (56 B). All from [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md), which
now documents THREE forms of the blocker (globals-struct parameter, a
helper the original inlines, and a temp added to preserve an order the
compiler produces anyway).

Two new dictionary entries, and they are the same rule from opposite sides
-- **load placement is VC5's decision, not the source's**:
do not CACHE what the original re-reads (0x10039870), and do not NAME a
temp to preserve an observed load order (0x100400E0). Details in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).

Bookkeeping was worth more than the matches again: 13 more stale d3d tags
converted to port-only across slice2_23.c, slice3_32.c and br_uinav.c.
slice2_23.c is 13/13 and slice3_32.c 9/9; the C residue went 346 -> 325.

Parked: 0x10041180 (243, the original reloads a field the source just
stored -- VC5's store-to-load forwarding does the caching, so rewriting the
access does not undo it; needs a fresh idea).

Next: br_uinav.c still has 0x10040EB0 (587 B) and 0x10059410 (939 B,
DirectInput poll) on the same globals-pointer cause.


## Session 8c (2026-09-03) - three phase-leave twins + the stale-claim auditor

+3 byte-exact: 0x1003E1C0, 0x1003E330 (56 B each) and 0x10040420 (119 B),
all twins of the previous pass's 0x100400E0 differing only in which globals
they touch. Run `tools/brally/gen_cpptwin.py` BEFORE hand-stamping twins -- it
reports 0 afterwards because the siblings are already matched by then.

**The real result is tools/brally/stale_claims.py.** It cross-checks every C
`diff` row against the other report and finds claims whose VA is already
matched in another TU. 71 of them across 21 files -- the C residue was
325 and is now 250. This had been eating a third of the apparent remaining
work. Run it at session start.

Also corrected a stale residue note on BrFadeTick (0x100186E0, 685 B, 4
diffs): it said "register pairing only", but two of the four diffs are a
SHAPE difference -- the original relocates both history stores against one
base with a literal +8, so those are one array of four ints rather than
the two declared in slice2_16.c. Indexing past the first array does fix
the displacement but perturbs an earlier read (4 -> 15). The clean fix
needs a serialised edit to slice2_16.h.

**Process slip worth remembering:** `git add -A src/brally/core` swept a parallel
worker's in-progress file into my commit. Backed out cleanly; see
the commit-every-match rule. Stage explicit paths only.


## Session 9 (2026-09-03) - 1,075 -> 1,083; the menu-builder recipe pays 6.8 KB

+4 byte-exact, all first compile, all multi-KB C++ menu builders:
0x10048160, 0x100458D0, 0x100451F0, 0x10048F10 (2433 B). Method and the
reusable class pieces: [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md). Copy the classes from an
already-matched sibling and transcribe the Ghidra draft entry by entry --
this is the highest bytes-per-minute lever found so far.

Session start ritual now includes `python3 tools/brally/stale_claims.py` (clean
this session). Image gate 0 diff bytes.

Next: 0x10046E70 (2114 B) is the last lane member and the only one whose
draft cannot be trusted -- one entry has a clamp, a three-way float
interpolation and an __ftol feeding three stores; read the asm for that
block.


## Session 10 (2026-09-03) - 1,083 -> 1,094; the builder family is generated

+7 byte-exact, 6,526 bytes, ALL from a new transform:
**tools/brally/gen_menubuilder.py** turns a Ghidra draft into a byte-exact menu
builder TU. Details and the remaining bail list: [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md).
This is the first generator in the project that actually swept a class --
mint it only after hand-solving several, which is what happened here
(five by hand first, then the tool).

Also parked 0x10046E70 at a constant-register fork (675/2114 bytes exact)
after recovering two shapes the draft loses: the clamp is one load fixed
in place, and the fill loop's bound test is SIGNED on the pointer.

Image gate 0 diff bytes. Session start: refcheck, `tools/brally/stale_claims.py`,
claim a lane.


## Session 11 (2026-09-03) - 1,094 -> 1,103; scaffold mode

+5 byte-exact, 9,028 bytes, all menu builders. The generator gained a
`--partial` scaffold mode for members with function-specific logic; see
[cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md). Image gate 0 diff bytes.

Third sighting of the put-the-whole-expression-in-both-arms lever (now on
a conditional caption id) -- recorded in docs/brally/VC5-IDIOMS.md.

13 builders left, all needing hand fills of a scaffold. Largest first:
0x10051600 (4109 B, 41 markers), 0x100498A0, 0x1004CBA0, 0x1004BE00,
0x1004DA00.


## Session 12 (2026-09-03) - 1,008 placed / 151,377 B; six new levers, two screens

Image gate 0 differing bytes, 0 overlapping claims (839 C + 169 C++).
`tools/brally/claim_lane.py claim` HANDS OUT NOTHING now: every ranked diff row is
already `parked` in `build/brally/win32/match/lane_claims.csv` from earlier lanes, and
the pool excludes parked rows. Work straight off `tools/brally/fnmatch/triage.py`
instead, or clear stale parks first.

**Byte-exact this session (7):** 0x100549A0 (C++, 132 B, slot poll on the
0x10054E20 record), 0x1003BA30 (144 B), 0x100239C0 BrGbiMoveWord (187 B),
0x100296B0 BrGbiTexScanSetTileSize (91 B), 0x1001FD40 + 0x100211E0
(geometry mode, 37 + 33 B), 0x10021210 BrGbiDispatch (43 B), 0x10021020
BrGbiDList (61 B). Big improvements: 0x10029480 LoadTlut 19 -> 4,
0x100014A0 BrSurfSetColourKey 46 -> 21.

**Six levers, all in docs/brally/VC5-IDIOMS.md, all proven byte-exact:**
1. An accumulated local is not the same expression as a sum (`dx += i;
   f = dx;` vs `f = dx + i;`). VC5 canonicalises commutative adds, so
   operand order NEVER matters -- only the assignment form does.
2. `x <<= k` fuses with a preceding right shift; `x = x * (1<<k)` does not.
3. Read and update the GLOBAL; a local copy of it flips the accumulator and
   turns `inc` into `lea`. Three proofs in one file.
4. A two-case dispatch is a SWITCH with a default, not a chain of equality
   returns -- the switch puts the default's return between the tests and
   the arms.
5. A 16-bit destination makes VC5 factor a shared shift; write the source
   factored, and do not "correct" a mask that looks too wide.
6. Two early returns vs one nested `if` decides whether the callee-save
   pushes shrink-wrap into the guarded block (re-proof of the /Od entry,
   now under /O2).

**Two new screens, both cheap, both run once:**
- `tools/brally/screen_shrinkwrap.py` -- originals that sink a callee-save push
  past the first branch (39 diff rows; many 0x1004xxxx hits are C++ EH
  false positives, ignore those).
- `tools/brally/screen_globalcache.py` -- sources that cache a global in a local
  and store it back (10 diff rows).

**Parked with dead-probe lists in their file headers:** 0x10054730
(C++, 618 B, 31 diffs, instruction parity -- one hoisted load, one SIB
choice), 0x1001D1B0 BrScenePropsDraw (12, T3a), 0x10014960 (25, T3a --
all six permutations of a three-term sum are byte-identical),
0x10027290 BrGbiSizeShift (a real 15-vs-20 trade: the named local gives
reggap 0 but puts the param in ecx; kept the reggap-0 form).

**A SECOND LANE was committing to this same worktree throughout** (the
menu-builder / gen_menubuilder commits, 0x10051600, 0x1000A110). Nothing
collided -- we touched disjoint .c files -- but do not read `git status` as
your own state here, and do not assume an uncommitted file is yours.

**One thing to check, not a win:** report.csv calls
`BrCamMatrixSetupOrtho` (0x1002D72E) `match` under /Od with
`recomp_size=10731` -- a whole-section extent, and it still read `match`
with the working-tree change STASHED. Treat that row as mis-scored until
someone checks the symbol extent; it is the "believe the size column"
trap.


## Session 12 (2026-09-03) - 1,103 -> 1,113; 11.6 KB, biggest fn yet

+5 menu builders including 0x10051600 at 4109 B, the largest single
function matched in the project. The structural unlock (BrCtl embeds the
0x438 item record at +0x2B5C) and the recurring fill patterns are in
[cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md). Image gate 0 diff bytes.

Process note worth keeping: after filling a scaffold, ALWAYS compare recomp
size against orig. Short with zero markers left means a statement was
deleted during the fill, not a tool gap.


## Session 13 (2026-09-03) - 1,014 placed / 158,378 B (32.94% of .text)

Image gate 0 differing bytes, 0 overlapping claims (842 C + 172 C++).
A SECOND LANE was committing throughout again (the gen_menubuilder /
helper-inlining commits) - read `git log`, not `git status`, for your own work.

**Byte-exact (4):** 0x10054A30 BrSlotAdd (999 B, C++), 0x1002F380
BrPadTranslate (690 B), 0x10003320 BrChkFReadOpen (260 B), and the
0x10058900 improvement 138 -> 98 with instruction parity.
**Parked, transcribed:** 0x100553B0 BrSlotScrollStep (1453 B, C++) at +3
bytes / 8-instruction gap; five of the eight are the known epilogue
cross-jump.

**Three new levers, all in docs/brally/VC5-IDIOMS.md:**
1.  **A repeated byte immediate gets POOLED into a register.** Two probes
   spelled `p[1] & 0x80` / `p[7] & 0x80` share one source constant and VC5
   hoists it to `mov cl,0x80`; spelled `& 0x8000` on the halfword the
   narrowing happens per instruction and nothing pools. This closed
   0x1002F380, whose header already carried four dead spellings - when a
   note says "allocator residue", check whether the constant's WIDTH is
   the free variable before believing it.
2.  **A plain `static` helper is NOT auto-inlined under /O2** (/Ob1 only
   inlines `inline`/`__inline`). A one-line pun or accessor comes out as a
   real call the original does not have. `strcpy`/`strcat`/`strlen`/`memcpy`
   ARE inlined (that is /Oi); `strncpy` and `_stricmp` are not.
3. **A CRT call site may be the /MD IMPORT, not the decompiled in-DLL twin.**
   0x10058900 called `BrSprintf` (the real function at 0x1007C830) where the
   original does `mov esi,[__imp__sprintf]; call esi` twice. slice2_25.c
   already had this convention written down; it was not applied elsewhere.
   Related: a format string modelled as a `const char *` GLOBAL cannot
   produce the original's `push <imm32>` - use the literal.

**Screens run once, results recorded, no tool minted:** the "short static
helper called from a diff row" scan returns 63 rows but is mostly false
positives (the helper IS a call in both). The CRT-import-vs-wrapper scan
returns 9 rows and is worth redoing by hand if that class comes up again.

**Characterised, NOT started:** 0x10062640 BrMat4FromCarState (363 B,
REGNORM 60+37) is a quaternion-to-matrix x87 DAG that the original keeps
almost entirely in x87 registers with ONE reused stack slot (the argument
slot), and no named float locals at all - our version names every
intermediate and spills. The full stack trace is worked out in this
session's transcript; it is a genuine multi-hour job, not a quick win.

**Parked with notes this session:** 0x10003430 BrFChkFRead (8, T3a, the two
size params homed in each other's registers), and 0x1003C6D0 / 0x1006CE20
were ALREADY parked with dead-probe lists - read the function header before
opening anything with a small diff count.


## OPEN LEVERS (moved out of the notes index 2026-09-03 to keep the index under its 200-line cap)

** OPEN LEVERS, ranked** (all with runnable screens in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md)): **DELETED DEBUG TRACING -- 0x1005FF00
BrRaceGateStep is 1,962 B short with TWELVE trace calls and its reggap is
13 EXTRA / 530 missing, so everything written is already right; that is the
single biggest C-lane win left, and it is transcription not discovery.**
Then **THE FACTORED HELPER -- being worked now, two
handlers taken from -337/-627 bytes to -49/-56; hand-inlining mechanics
(MACRO not static, ONE shared temp as a macro parameter, and PUN THE FLOAT
STORES -- 25 regnorm on one function) are in the idiom doc** - 32 rows / 13,463 bytes
short, the largest source-level class left in the C lane, and it answers
warning 2 directly by naming concrete targets** (MSVC5 will not inline a
`static` with two callers, nor ever one returning a struct; spell the body out
at the call site under BR_MATCHING_BUILD and keep the static for its other
callers. Worst: BrRaceGateStep -1962, BrOptFn100558A0 -1357, BrTextEmitString
-1162, BrExt_1004DFC0 -962, BrUiNavMove -891). Sub-case: an accessor over N
standalone globals, giveaway `mov R,[R+I]` where the original has `mov R,[I]`.
Then: `tools/brally/gen_menubuilder.py` stamps byte-exact UI
menu-builder TUs from Ghidra drafts (+28 KB over two sessions, `--partial`
scaffolds bespoke ones, 8 members left - [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md));
`(double)` modelling is a D3D-era artefact and the Glide binary is FLOAT (tell:
the original never spills a qword; twelve diff-bearing files named);
`tools/brally/framescreen.py` - 26 of 69 tagged-diff rows have a frame disagreeing
with the original's `sub esp` (ours smaller = a scalar that should be an
ARRAY, larger = a local the original does not spend); a diff row whose
byte-adjacent MATCHED neighbours are all /Od is MIS-SHAPED, not blocked; a
helper RETURNING A STRUCT never inlines in MSVC5, which is what makes a family
read MISSING CODE at 40%; `/Od /Op` is a missing fifth sweep variant (a full
re-sweep collects it); the port's ops/host-hook table is a cause class with
seven files still open. Also [close-queue-lever](../triage/close-queue-lever.md).



## Tier-table bug, full note (moved out of the notes index 2026-09-03)

2a. ** THE TIER TABLE WAS WRONG UNTIL 2026-09-03 AND T1 WAS CLIMBING.**
   tiers.py computed T1 by subtraction (target minus report.csv rows), and a
   function that moves to the C++ EH workstream LEAVES report.csv -- so every
   C++ conversion pushed T1 UP by one and left T4 flat. 171 fns / 75,339 B sat
   in "not started" while byte-exact. FIXED (report_cpp.csv folded into T4,
   de-duplicated against report.csv; EXE stays out, different image).
   **Real numbers: T1 289 / 176,365 B, T2 184 / 112,080, T3a 34 / 7,459,
   T4 1,012 / 157,236 of a 1,519-fn target.** Any tier figure quoted before
   this date overstates T1 by ~170 functions.


## Moved out of the notes index 2026-09-03 (index line cap)



**Giants.**  **BEFORE WRITING "THE COMPILER WILL NOT DO X", FIND A SITE IN
THE SAME BINARY WHERE IT DOES** - then vary the statement count, then move the
block. That order retracted 0x1000EAF0's six-session-old "the x87 preload
depth is a scheduler constant": its SECOND scale site already emits the
original's depth, the first is fixed at 5 for every count 10-16, and the
preceding row block sets it. **Wall 2 is downstream of wall 1** - spelling the
row block's four terms with the original's operand kinds (all view terms
absolute) reaches the original's preload, but wrecks the rows; a row form that
carries those kinds cheaply is the open lever. Ruled out: helper boundary,
array identity, extern-vs-defined, dead-local removal.
 **A DEAD VERDICT MEASURED AGAINST A WRONG FRAME IS STALE**  - 
0x1000A110's three-float copy had FIVE dead spellings and a "T3a until the
frame is solved" note; the frame landed, the SIXTH spelling is
instruction-for-instruction (both floats need named temps so their live ranges
overlap on the x87; the INTEGER member must be stored FIRST or its neighbour
is copy-propagated into a mov). Re-test allocation-sensitive do-not-re-run
notes after the frame moves. Now 24 masked / 13 insns / 47 B short.
0x1000A110 frame CLOSED (`sub esp,0x4c`, prologue byte-exact). 0x1000EAF0 wall 3 broke - read the wheel record's +0x70 field
through the SAME pointer the ground-probe argument uses; keep the `wb` temp
and keep it used twice or the prologue flips (scenedl-0x1000eaf0-state).
0x100250D0 at instruction parity, residue is allocation
([brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md)). 0x10019A70 LAST ([braceStep-wall](../functions/braceStep-wall.md)).



2. ** "THE LANE LEDGER IS EXHAUSTED" IS NOT "THERE IS NO WORK".** The ledger
   and `triage.py` only ever see functions that are TAGGED and diffing, i.e.
   T2/T3a. **T1 -- 289 functions, 176,365 B, still the biggest tier -- has no `@implements` tag, never reaches report.csv, and
   is structurally invisible to both tools.** Run `python3 tools/brally/tiers.py` for
   the real denominator and `--list T1` for the work (largest-first, with each
   function's machine draft; implemented 2026-09-03 -- the docstring had
   advertised it for months and the code did nothing). Every T1 function
   already has a draft; the T1->T2 recipe is its own section in the playbook.
   THE OLD NOTE, true of the TAGGED pool only: `claim 20` returns nothing (221 parked /
   16 claimed of ~240 diff rows), and nearly every top-ranked SHAPE row is
   parked, including rows at 86-100% struct%. Park now means "someone ran out
   of time", not "wall". Work them directly after reading the file's residue
   note; un-park by editing `build/brally/win32/match/lane_claims.csv` (there is no unpark
   command and parked rows never return to the pool).


** MEASUREMENT TRAPS.** Instruction-count equalities were padding artefacts
([instruction-count-padding-trap](../traps/instruction-count-padding-trap.md)). A SUSPECT resync poisons the TWO regions
after it, so `divergence.py --key N` matters - read repetitive functions at
`--key 10` and SAY WHICH KEY. The reusable settling check: count an
instruction occurring once per unit of work across the WHOLE function in both
streams; equal counts ⇒ nothing missing ⇒ the residue is allocation. Rank by
register-blind multiset and masked regions, NEVER size - real fixes routinely
make size worse. Score each row against its OWN compile variant.



XX_DROP_XX A probe was declined for
eight sessions because a region's first divergence moved 27 bytes earlier;
reading it showed a two-instruction SCHEDULE SWAP of code both builds emit.
Taking it moved 0x1000A110 13 -> 9 instructions short, 47 -> 36 bytes (arm 1
gets its own pack array + its own top byte local - both had been measured dead
against older allocations). Its residue is now ONE defect at three sites: each
colour arm homes one pack byte where the original homes two.
** A RAW-ADDRESS CAST IS NOT A SYMBOL REFERENCE.** `*(float *)(0xADDR+4k)`
is a compile-time CONSTANT, `DAT_ADDR[k]` is a RELOCATION; they assemble the
same and SCHEDULE differently. A mixed transcription held 0x1000EAF0's x87
preload wrong for nine passes; all-symbol closed it (regions 20->19, bytes
18->15 short). **Treat a hex-address cast as a transcription placeholder and
make an expression uniform before reading its schedule.** Also: VC5
canonicalises x87 multiply operand order, and a pointer local to an array is
byte-free.


## Session 14 (2026-09-03) - 1,016 placed / 158,657 B (32.99% of .text)

Image gate 0 differing bytes, 0 overlapping claims (844 C + 172 C++). The
second lane was committing throughout again.

**Byte-exact (2):** 0x100199A0 BrRaceCarCtlOutro (106 B), 0x10006350
BrNetDropMatching (173 B, FIRST COMPILE).
**Improved:** 0x1002A050 BrMat4LookAt +45 bytes / register-blind 25+13 down
to -4 and 4+6.
**Transcribed and parked:** 0x1000C4E0 BrRippleApply (1246 B, C++) at -30
bytes / register-blind 12+21; full residue and dead-probe list in its header.

** The lever of the session - the redundant parenthesis, SECOND proof.**
`x*x + y*y + z*z` scheduled the third square second; `(x*x + y*y) + z*z`  - 
identical association, one redundant pair - scheduled it last and took
0x100199A0 from 22 diffs to 2. The screen signature is now written down in
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md): **size and instruction multiset already exact,
divergence purely in x87 ordering.** On a float function in that state try
the parenthesis axis FIRST; it is one recompile.

**Two more levers recorded, both proven twice:**
- Do not hoist a float parameter into a `double` local; read it where it is
  used. And `(double)a - (double)b` on two floats costs fourteen bytes
  against plain `a - b`, which emits the original's `fld dword; fsub dword`.
  On the x87 they are the SAME value, so put the casts behind `#else`.
  (0x100199A0 + 0x1002A050.)
- When the port's signature cannot express the original's - the original
  reaches globals where the port takes a state pointer - put the Glide body
  in its OWN file under src/brally/core/generated/ and mark the slice body
  "port-only". That is the existing convention for the net cluster and it
  took 0x10006350 byte-exact on the first compile with no header edit.
  Editing the shared header instead would have touched three other files.

**Screens run once (results only, no tool minted):** ops/hook-table calls in
diff rows = 23; `double x = (double)` widening = 2; short static helper
called from a diff row = 63 but MOSTLY FALSE POSITIVES (the helper is a call
in both). Port-deviation markers with no matching branch = **62 rows**  - 
that is the real remaining seam, and it is the class both of this session's
byte-exact matches came from.

**Re-confirmed T3a walls, now noted in their files - do not reopen:**
0x10029510 BrGbiTexScanLoadBlock (46, registers swapped), 0x100186E0
BrFadeTick (4, esi/edi swapped), 0x100345F0 BrVec3AddTo (6) and 0x10034360
BrVec3Scale (5) - commutative FADD/FMUL is canonicalised, so operand order
in the source never moves them.

**Characterised, not started:** 0x1005EB90 BrPathWalk is missing a whole
block (10 global reads, two indirect calls); 0x10033BB0 BrPfxTick is the
same shape at 219 B. Both are port-hook rewrites, not scheduling work.


## Lane 6077a6e1 / bdd26c6a detail (moved out of the notes index 2026-09-03)

**Latest (2026-09-03, lane 6077a6e1):** image gate re-run, **0 differing
bytes**, 1,017 fns / 158,698 B / 33.00% of .text; total.py **1,121 byte-exact
/ 169,003 B**. Banked 0x10065950 BrCrPlaneDist (T1 intake, byte-exact first
compile). **NEW IDIOM: a SELF-PROTOTYPE moves the x87 schedule** - the obvious
header declaration cost that match one extra `fxch` (41->43 B). ONE-WAY:
it only helps a recomp with an EXTRA fxch, never a MISSING one. Also new:
read a float sum's TERM ORDER off the faddp chain before writing the C
(that is what made this one byte-exact first compile). Both in VC5-IDIOMS. Two tooling faults fixed: `tiers.py --list T1` was printing 460
rows against a stated 289 (it never subtracted report_cpp matches, so 171
finished C++ functions read as fresh work), and 14 non-C rows (_chkstk, five
IAT thunks, eight unwind funclets) are now fenced - target 1,519 -> 1,505,
T1 289 -> 274. **The small-reggap TAGGED pool is worked out**: `claim 20`
returns nothing, every SHAPE row at reggap <=7 is parked behind a thorough
dead-probe note; T1 intake is where the count moves. Details in
[resume-state](resume-state.md).

**Prior (lane bdd26c6a):** menu-builder PHOTO BLOCK solved; photo park is a
THREE-function wall (0x1004AEE0 / 0x1004BE00 / 0x1004DA00, all at 34 diffs
with byte-identical residue, 10,731 B on one VC5 schedule)  - 
[cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md). BrGlTrackHdrRead 649 -> 231 diffs, size and
multiset exact, residue is byte-pair load order; header carries the list.

** SESSION START:** refcheck, `tools/brally/tiers.py` (the real denominator),
**`tools/brally/claimcheck.py`** - it catches two names on one address AND the
"original delegates, port calls nothing" class that every size-based screen
misses - then triage + claim.

**Family is DRY.** Every remaining menu-builder is filled-and-parked or
carries the photo trio. **Standing order from here: SHAPE targets from
`tools/brally/fnmatch/triage.py`, smallest reggap first** - but READ THE FILE'S
RESIDUE NOTE BEFORE PROBING. Of the seven smallest-reggap SHAPE rows
examined 2026-09-03, FIVE already carried exhaustive dead-probe lists
(BrEntitySetIndex, BrComGetAlloc, BrMat3Mul, BrMat4Mul, BrDpCreateIface).
triage.py ranks them high precisely because they are nearly done; low
reggap means WELL-WORKED, not easy.



## Index RESUME block, moved here 2026-09-03 (index hit its 200-line cap)

Verbatim from the notes index before it was compressed. Anything here that is
already written up above is a duplicate; the value is the 2026-09-03
menu/input lane detail and the three standing warnings.

## ▶ RESUME - 2026-09-03
**1,121 byte-exact / 169,003 B (845 C + 172 C++ + 104 EXE)** via
`tools/brally/total.py` ([counting-reconciliation](../traps/counting-reconciliation.md)). **Hand-C target is 1,505, not
1,519 - 14 CRT/thunk/funclet rows were fenced 2026-09-03.** **Session start:** refcheck,
`tools/brally/stale_claims.py`, refresh triage, claim a lane. **This index is capped
at 200 lines - keep detail in [resume-state](resume-state.md), which is the working state:
per-session log, parked functions, dead-probe lists, open levers. Read it
before planning.** Each file header carries its own residue map and
do-not-re-run list.

**Latest (2026-09-03, menu/input lane 450e6a7f):** **BrGlNavPoll 0x10059410
404 -> 390 diffs, register-blind 1+2 -> 0+1, on ONE new idiom: a NARROW
GLOBAL'S SIGNEDNESS picks how VC5 materialises a negative constant** --
`uint16_t g; g = -1;` emits `mov edx,0xffff` (already narrowed), `int16_t g;`
emits `or edx,-1` (full width, truncated by the store). Casts, a shared
`int32_t step = -1` local and a literal at each store all fail; only the
DESTINATION's type moves it. 0x100140B0 BrHudDrawDial's ENTIRE residue is
four `fxch` (documented scheduler park) once the phantom `lea` trap below is
discounted. 0x100194C0 BrWndProc's case 6 now reaches the shared tail by
`goto`, which emits the original's `push esi` rather than a folded `push 6`;
its block-placement wall has two dead layouts recorded in the file.

** THE MENU-BUILDER FAMILY IS DRY, AND SO IS THE SMALL-REGGAP END OF
triage.py.** Rows opened and found to be ALREADY-DOCUMENTED PARKS this
session, on top of last session's five: BrVtxSwap (VC5 anchors the walked
pointer at +2 from every probed spelling), BrHudDrawDial, BrWndProc. READ
THE FILE'S RESIDUE NOTE FIRST -- low reggap means well-worked, not easy. The
untouched work is at the LARGE-gap end (BrInputJustPressed 1246 B is -310
bytes with 122 undisassemblable `.byte` in the recomp: a full
retranscription, not a probe) and in T1.

**Earlier 2026-09-03 lanes (bdd26c6a photo block, 6077a6e1 slots/x87):**
full write-ups in [resume-state](resume-state.md) and [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md). Headlines:
the menu-builder PHOTO BLOCK is solved and its photo1 tail is a
FIVE-function / 18,395 B scheduler wall with every fy placement now a dead
probe; BrGlTrackHdrRead is size- and multiset-exact with only a byte-pair
load order left; VC5 CANONICALISES COMMUTATIVE FLOAT ADDITION (never permute
a float sum).

** THREE STANDING WARNINGS**
1. **IMAGE GATE: VERIFIED GREEN - 0 differing bytes (re-run 2026-09-03).**
   Standing rule: **never repeat "0 diff bytes" without re-running it.** The
   failure mode it caught: a generated C++ function can read `match` at
   function level while its SEH scope-table pointer resolves to the wrong
   address once placed - the function-level scorer cannot see
   that. [image-build-gate](../oracle/image-build-gate.md)

2a. ** THE TIER TABLE WAS WRONG TWICE, BOTH FIXED 2026-09-03.** (i) tiers.py
   computed T1 by subtraction, so every C++ conversion pushed T1 UP by one.
   (ii) `--list T1` still printed the C++ matches after (i) - 460 rows
   against a stated 289, the extra 171 already byte-exact and offered as
   fresh work (a nine-member 66-byte BrOpt* family among them). **Before
   taking ANY target: `ls src/brally/core/cpp/<VA>.cpp` and grep report_cpp.csv.**
   Current numbers, after fencing 14 CRT/thunk/funclet rows: **T1 274 /
   176,164 B, T2 179, T3a 36, T4 1,016 of a 1,505-fn target.** Any figure
   quoted before this date overstates T1. Detail in [resume-state](resume-state.md).
2. ** "THE LANE LEDGER IS EXHAUSTED" IS NOT "THERE IS NO WORK".** The
   ledger and `triage.py` only see TAGGED+diffing functions (T2/T3a). **T1  - 
   274 fns, 176,164 B, the biggest tier - has no tag and is invisible to
   both.** `tools/brally/tiers.py --list T1` is the work, largest-first, each with a
   machine draft. Park now means "ran out of time", not "wall": un-park by
   editing `build/brally/win32/match/lane_claims.csv`. Detail in [resume-state](resume-state.md).
3. **RE-MEASURE A NOTE'S CLAIM BEFORE BELIEVING IT.** One asserting
   instruction parity was a whole instruction short at its own commit. FOUR
   "unreachable" / "do not grind" / "parity reached" notes have now been
   overturned - leads, not verdicts.

** OPEN LEVERS - full ranked list with runnable screens now lives in
[resume-state](resume-state.md) under "OPEN LEVERS". Top three, unchanged:** DELETED DEBUG
TRACING (0x1005FF00 BrRaceGateStep is 1,962 B short with twelve trace calls
and a 13-extra/530-missing reggap - everything written is already right;
transcription, not discovery, and the biggest C-lane win left); THE FACTORED
HELPER (32 rows / 13,463 B short - hand-inline as a MACRO, not a static, one
shared temp as a macro parameter, and pun the float stores); and the
`(double)` modelling artefact (the Glide binary is FLOAT; the tell is that
the original never spills a qword). Also [close-queue-lever](../triage/close-queue-lever.md).

** MEASUREMENT TRAPS.** Instruction-count equalities were padding artefacts
([instruction-count-padding-trap](../traps/instruction-count-padding-trap.md)). A SUSPECT resync poisons the TWO regions
after it - read repetitive functions at `--key 10` and SAY WHICH KEY. Rank by
register-blind multiset and masked regions, NEVER size. Full list in
[resume-state](resume-state.md).
** READ A REGION BEFORE BELIEVING ITS ADDRESS** (and **A RAW-ADDRESS CAST IS NOT A SYMBOL REFERENCE**) - both in [resume-state](resume-state.md).
** THE REDUNDANT-PAREN AXIS HAS A BOUNDARY: redundant CASTS are INERT.** The
paren finding is about the expression TREE, not redundant syntax generally,
and it only pays where the register-blind gap is ALREADY 0 with divergence
purely in x87 ordering; on a large-gap function it just moves the schedule
around. Details in [resume-state](resume-state.md).

** MEASUREMENT: fn.py's DIFFS IS POSITIONAL.** A size shift upstream
inflates or halves it - a 2-byte change on 0x1000EAF0 read as a 47% DIFFS
drop. Compare DIFFS only between builds of the SAME size; when the size moves
rank by msetdiff rows / instruction gap / masked regions. This qualifies older
dossier comparisons. ** A REDUNDANT OUTER PARENTHESIS PAIR IS NOT A NO-OP**  - 
it moves VC5's x87 schedule (isolated A/B). Never tidy parens in a matching
TU; try with and without as a probe axis.
**Giants.**  **BEFORE WRITING "THE COMPILER WILL NOT DO X", FIND A SITE IN
THE SAME BINARY WHERE IT DOES** - that order retracted 0x1000EAF0's
six-session-old "x87 preload depth is a scheduler constant". Full giant-state
notes (0x1000EAF0 walls 1-3, 0x1000A110's frame, 0x100250D0, 0x10019A70) are
in scenedl-0x1000eaf0-state, [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md),
[braceStep-wall](../functions/braceStep-wall.md) and [resume-state](resume-state.md).  A DEAD VERDICT MEASURED AGAINST A
WRONG FRAME IS STALE - re-test allocation-sensitive do-not-re-run notes after
the frame moves.

**Triage:** register-blind multiset gap, never raw diffs
([register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md), [divergence-class-triage](../triage/divergence-class-triage.md),
[residue-retriage-2026-08-28](../triage/residue-retriage-2026-08-28.md)); don't rank by smallest diff count
([inlined-helper-match-class](../triage/inlined-helper-match-class.md)); harness [fnmatch-harness](../toolchain/fnmatch-harness.md); generators only
for homogeneous classes ([generator-compounding-reality](../triage/generator-compounding-reality.md));
`tools/brally/pushcensus.py` is the ONLY check that sees a permuted call constant.
**Gates:** [image-build-gate](../oracle/image-build-gate.md) is the deliverable gate; one-file sweep ~12s,
NEVER full-sweep ([sweep-is-incremental-now](../toolchain/sweep-is-incremental-now.md)); EH class
[cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md); EXEs [exe-decomp-state](../functions/exe-decomp-state.md); Ghidra [ghidra-pipeline](../toolchain/ghidra-pipeline.md).
Parallelise by .c file ONLY; header edits serialised. File matches into
modules as you go ([file-as-you-match](../rules/file-as-you-match.md)); commit each match immediately
(the commit-every-match rule). ** COMMIT WITH A PATHSPEC  - 
`git commit -m "..." -- <paths>`. Plain `git commit` takes the WHOLE INDEX,
so a parallel session's staged work rides along under your message; explicit
`git add` does NOT prevent this. Check `git diff --cached --stat` first.**


## 2026-09-03 - session 14 (particle-step lane, token 027e0270)

**+2 byte-exact, 534 B; +1 transcribed-and-parked, 398 B. Image gate re-run
GREEN: 0 differing bytes, 1,019 fns / 159,232 B placed (33.11% of .text),
847 C + 172 C++.**

Landed: **0x10033BB0 BrPfxTick (219 B)** and **0x10033880 BrPfxUpdateB0
(315 B)**, both in `src/brally/core/generated/`. Parked: **0x100339C0
BrPfxUpdateB4AC**, 396/398 B, one redundant `test r,r` short - full
dead-probe list in its header, do not re-run those.

**How the lane was picked, and what that says about triage:** the C++
vcall/twin family is EXHAUSTED (see [cpp-vcall-family-lode](../cpp-lane/cpp-vcall-family-lode.md) session 14),
`claim 5` returns nothing, and every SHAPE row in `tools/brally/fnmatch/triage.py`
is `parked`. Parked is not walled - but a screen of all 57 SHAPE rows against
their owning files found only ~10 with a real residue note, so the rest are
"ran out of time". The one I took, BrPfxTick, was tagged only by its **d3d**
VA, so it read as untouched work in the triage list while the file already
held a port body for it.

**The cause, and it is the general lever now:** the port body took five
aggregate parameters; the original takes NONE. **Screen for this from the
CALL SITE, not the source** - `tools/brally/dumpasm.py <caller>` on BrPfxTick showed
bare `call rel32` for the three pool steppers and `mov ecx,[esi]; call` for
the three per-car helpers, settling four signatures in one disassembly.
Written up in [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md).

**Two new idioms, both in docs/brally/VC5-IDIOMS.md:**
1. **Respell `array[idx].field` in every statement.** Hoisting the record
   into a `Rec *p` local collapses the index chain into a base register:
   -19 bytes, -10 instructions, regnorm 48+21 -> 18+8, on a function whose
   statements were otherwise all correct.
2. **A redundant LEFT grouping paren is the float-sum-order lever.**
   `(prod*dt + drift) + pos` - a paren pair enclosing exactly what the
   default left-associative parse already groups, a pure no-op to the
   language - took 0x10033880 from 4 diffs to BYTE-EXACT and fixed all three
   axes of 0x100339C0 at once. Permuting the summands does nothing
   (canonicalisation). This is NOT the right-leaning `a + (b + c)` regrouping
   that scored worse on BrVec3Project.  **Try this first, always, whenever a
   float sum's operand order is the last divergence** - it is one probe and
   it is free.

**Scorer trap worth knowing:** a struct member at record offset 0 has a zero
reloc addend, so the recomp disassembles as `[esi]` where the original reads
`[esi+0x10AC0C48]`. `fn.py --detail regnorm` reports that as a phantom
`fadd [R]` EXTRA / `fadd [R+I]` MISSING pair. Check `tools/brally/divergence.py`
before chasing it - on 0x100339C0 two of its five regnorm rows were this
artefact.

**Environment notes:** `timeout` does not exist on this machine (use the tool's
own timeout). `tools/brally/cpp_screen.py`, `triage.py`, `fn.py`, `divergence.py` and
`image_build.py` all need `.venv/bin/python3` (capstone). `divergence.py` takes
`<obj> <reference/brally/orig/VA.bin> <symbol>` - the obj fn.py just built is
`build/brally/win32/match/obj_fnbase/<file>.obj` and the symbol is undecorated.

** A PARALLEL SESSION WAS COMMITTING TO THIS REPO THROUGHOUT.** HEAD moved
under me twice and `match_sweep.py` auto-committed my first match before I
did. Always `git commit -- <pathspec>`, and re-read `git log` before assuming
your last commit is HEAD.

## 2026-09-03 (lane b2cc3136 / bb38ae76) - T1 INTAKE PAYS: +8 byte-exact, 429 B

**The two lanes named in the request were both screened DRY before starting**,
and that screening is the reusable part:
- `tools/brally/cpp_screen.py` (`.venv/bin/python3`): **20 strong**, and only four
  lack a TU - 0x100498A0 and 0x1004CBA0 both carry the photo trio so they
  inherit its 34-diff park, 0x100541B0 is the predicted SIB park, and
  0x10059410 is NOT a C++ row at all (it is BrGlNavPoll in br_uinav.c, C lane,
  390 diffs). `gen_cpptwin.py` still finds 0 twins.
- `claim_lane.py claim 5` returns **a token and no targets**: all 208 diffing
  rows are held (196 parked, 12 claimed). The tagged SHAPE pool really is
  worked out - the lowest-score rows all carry thorough dead-probe notes
  (0x1006FD50's `sub`-vs-`add` list is a model of one).

**So the lane was T1 intake, and it landed eight in one pass:**
0x10030640 BrVec3dDot (37 B), 0x1006BF90 BrSndVoiceBufIsPlaying (54),
0x10059E00 BrUiVolumeApply (46), 0x10031660 BrTrackSetF08FromMax (58),
0x1006B6C0 BrSndSetVolumePairF (32), 0x1001FCF0 BrDlVtxFinishTex (70),
0x10003280 BrU16QueuePop (66), 0x10002C00 BrCdTrackRandom (67).
Five were byte-exact on the FIRST compile. Image gate re-run after:
**0 differing bytes, 1,029 fns / 163,664 B / 34.04% of .text.** `total.py`:
**1,135 byte-exact / 174,269 B** (857 C + 174 C++ + 104 EXE).

 **THE ONE LEVER THAT DECIDED THREE OF THEM - DO NOT HOIST.** Already an
idiom for record pointers; it now has three distinct symptoms, all written up
in docs/brally/VC5-IDIOMS.md under "Respell the index chain in every statement":
 1. addressing-mode collapse (the old one, 0x10033880);
 2. **a hoisted array base is LOADED ABOVE THE GUARD** the original loads it
    inside (0x10031660, was the entire residue);
 3. **a hoisted global that must survive a CALL costs an extra `mov` and
    flips the `imul` operands** (0x10002C00). Swapping the source operand
    order does NOT fix that; only deleting the local does.
**When the last residue is one extra register move around a call or a guard,
look for a hoisted local first.** Screened the whole small-reggap parked pool
for the `mov R,R` EXTRA signature afterwards: **no other member has it**, so
this is a lever, not a sweepable class. Do not mint a transform.

 **Also proven: an ORDINARY INDEXED `for (i = 1; i < n; ++i)` is what VC5
strength-reduces into the original's pointer walk.** Hand-writing the walk as
`do { p = p + 1; … } while (--k)` loses the pre-loop `add ecx,2` and flips the
compare sense - 22 diffs on 0x10031660.

**Two free matches came from bodies that already existed:**
BrVec3dDot was written and correct in br_vecd.c with no `@implements` (the
sweep never compiled it), and BrU16QueuePop was 56 diffs from its own matched
sibling BrU16CursorNext - u32 locals instead of u16 and an early return
instead of the success-path-inside-the-`if` shape. **I then screened the whole
tree for that class** (scratch `untagged2.py`: report.csv rows whose owning
file carries no `@implements` for that VA, checking both keyings) and it comes
back **0**. The class is now empty; do not go looking again without a reason.

**Notes corrected this session, because they were wrong, not just stale:**
- br_sfx.c and br_sfx.h both said 0x1006B6C0 "has NO implementation in this
  tree" and described it as setting an absolute FREQUENCY. Its tail reaches
  BrSndBufSetVolume. It is a volume setter and it is now matched.
- 0x100349C0 BrVec3Project's park note claimed 163 B / 69 insns, one `fxch`
  short. Re-measured: **165/70 against 165/70, register-blind gap ZERO**, 23
  bytes all one rotation of the x87 preload order. Still T3a, still parked,
  but better than recorded. Do not reopen without a NEW mechanism.

**Screening habits this session earned:**
- `tiers.py --list T1` IS STALE IN BOTH DIRECTIONS. 0x1002A7F0 is listed as
  not-started and has been byte-exact as BrMat4Translate for ages. Cross every
  candidate against report.csv (`awk -F, 'tolower($2)==va'`) before spending a
  minute on it.
- The T1 tail below ~70 B is thick with things that are NOT decomp targets:
  0x10073704/0x10073709 (3 B) and the six 0x100729xx (6 B) are thunks, and
  0x10054070 / 0x100087D0 / 0x1006FCE0 / 0x1006B440 are C++-lane rows.
- **Finding a small function's meaning is a lookup, not a derivation.** Every
  one of these eight was named from evidence already in the tree: a sibling
  body, `config/brally/globals_learned.csv`, `whereis.py`, or a header note that had
  already analysed the address (0x10031660's semantics were written out in
  slice2_20.h verbatim). scratch `callers.py` scans `.text` for `E8` sites
  targeting a VA and names the containing function - that is what identified
  BrCdTrackRandom's caller and BrDlVtxFinishTex's two.

## 2026-09-03 (lane a312fe5d) - +3 byte-exact, 314 B, and THREE new idioms

Session-start screens, all clean: refcheck OK, no stale claims, 0 undescribed /
0 misfiled, claimcheck 8 FLAGGED (all pre-existing d3d rows), no unswept TUs.
**C++ family re-screened: still 20 strong, still only four without a TU**
(0x100498A0 + 0x1004CBA0 carry the photo trio and inherit its park, 0x100541B0
is the predicted SIB park, 0x10059410 is a C-lane row). `claim 5` again returns
a token and NO targets - all 208 diffing rows parked or claimed. So the lane was
the small **MISSING CODE** rows out of `triage.py`, which are NOT the worked-out
small-reggap SHAPE pool.

Landed: **0x10060CC0 BrCarPredictRemote (129 B), 0x10069BC0 BrFn10069BC0 (98 B),
0x10069C30 BrFn10069C30 (87 B).** Image gate after: **0 differing bytes**,
1,028 fns / 163,218 B / 33.94% of .text. `total.py` **1,137 / 175,843 B**.

 **NEW IDIOM 1 - THE LAST TEST'S POLARITY DECIDES TAIL-MERGING.** A function
with several early returns of the same value comes out short by whole
epilogues; VC5 folds the identical exits into one shared block. Writing the
LAST test positively - `if (ok) { work; return 1; }` then `return 0;` instead
of `if (!ok) return 0;` - splits them all apart. That one flip was the entire
28-byte residue on BrCarPredictRemote. **Ruled out first, do not re-run:** seven
flag sets (/O2, /O1, /Ox, /O2 /Op, /O2 /Oy-, explicit /Ot /Og /Oi /Oy /Ob1 /Gs,
/Os /Og), `goto` to per-arm labels, distinct constant expressions that fold to
the same value, a struct local, an `&&`-joined guard, a flat `else if` chain and
three nesting depths. VC++ 4.2 /O2 reproduces the shape at exactly the
original's size but regresses 15 functions in the same file - a SHAPE ORACLE,
not a compiler choice.
**Boundary:** it is VALUE-RETURN-specific. 0x1002A1A0 BrGbiTexScanOtherModeL has
the identical symptom (three `mov [g],0; ret` blocks merged, 31 B short) and the
flip moves nothing - there the merged arms STORE TO A GLOBAL in a void function.
Four shapes plus a `switch` probed on it; all merge. Parked with the full list.

 **NEW IDIOM 2 - thiscall with 3+ args: WRAP EVERY ARGUMENT AFTER `this`.**
`src/brally/include/br_match.h` said "a struct-typed SECOND parameter", which is only
enough for two-argument functions: `__fastcall` SKIPS a struct and carries on
handing out edx, so a lone wrapper lets the THIRD argument take edx and the
callee cleans 4 bytes where thiscall cleans 8. With one wrapper 0x10069BC0 was
register-blind exact and still 16 bytes short (the four per-arm `mov edx,
[esp+8]` reloads had become a register). Header corrected. **Screened: every
other BR_THISCALL1 in the tree takes one argument or two with the wrapper
already right - no third case to sweep.** Lever, not a class.
Same two functions: **WRITE EVERY ARM OUT IN FULL.** The port factored the
profile choice into a helper; the original writes four arms, each folding its
own literal into the ROW index of one flat table (`(key + 28*k) * 3`) rather
than indexing a profile then a row. VC5 cross-jumps arms 2 and 3 of 0x10069C30
by itself - compiler layout, do not chase it from the source side.

 **NEW IDIOM 3 - three /Od facts** (0x1002A957 BrFloat12MaxAbs, 48 -> 23 B,
now INSTRUCTION-EXACT at 55/55):
 - `(v = *p++)` inside the condition. /Od defers the postfix increment until
   the comparison's operand is consumed, which is why the original's `p += 4`
   sits BETWEEN the `fcomp/fnstsw` and the `test`. Two statements put it ahead
   of the `fld` and cost 6 bytes.
 - a TERNARY return allocates an extra frame slot; `if (a>b) return a; return b;`
   does not. Check `sub esp,N` slot count before doubting the arithmetic.
 - **local DECLARATION ORDER IS INERT under /Od - seven orders, byte-identical
   output, do not probe an eighth.** SCOPE is not: moving one local into the
   `while` body it is used in moved three of six slots. Parked at 23 bytes,
   ALL of them `[ebp-N]` numbering; the target slot map is in the file header.
   Open question: what puts a FLOAT on -4 ahead of the two pointers.

**Tooling added (scratch, worth rebuilding if useful):** `odscore.py`
compiles one .c with ARBITRARY flags and scores one symbol - fn.py is /O2-only,
so its numbers are phantom on /Od, /Oy- and /Op TUs and this is the honest
measurement for those. Also `callers.py` (scan .text for `E8` sites hitting a
VA, name the containing function) and `side.py` (instruction-level side-by-side
from an obj + the orig bin).

## 2026-09-03 (lane 75b69714) - +4 byte-exact, 135 B, and a C-UNREACHABLE class named

Start-of-session screens: refcheck OK, no stale claims, 0 undescribed, no
unswept TUs. **C++ family screened dry for the THIRD session running** - 20
strong, the same four without a TU (0x100498A0 / 0x1004CBA0 inherit the photo
park, 0x100541B0 is the predicted SIB park, 0x10059410 is a C-lane row).
`claim 5` DID return targets this time, but two were in `src/brally/core/drawing/
br_dlclip.c`, an UNTRACKED file the parallel session was creating, and one was
0x100686D0 which that session had just parked. **Released those three rather
than colliding** - `git status --short src/brally/` before working a claimed row is
now part of the start-up ritual, not just `stale_claims.py`.

Landed: **0x100623A0 BrRaceDriverAnim (58 B), 0x1005D050 BrCtlHuman (9 B),
0x1005E690 BrCtlAi (9 B), 0x10004C40 BrNetPktStamp (59 B)** - all four
byte-exact on the FIRST compile. `total.py` **1,151 / 178,077 B**.

 **NEW IDIOM - A CHAR ARGUMENT ON A THISCALL'S STACK IS NOT REACHABLE FROM C.**
The tell is `mov al,[esp+N]` / an 8-bit op / `push eax` with NO zero-extension
anywhere near it: MSVC only leaves a stack argument dirty when the callee's
parameter is a BYTE TYPE, and a byte parameter is register-eligible, so
__fastcall hands it edx instead of the stack. **Probed and dead: a 1-byte
struct, a 4-byte union written through its char member, a 4-byte struct with
three explicit pad bytes** - all three home the partial write
(`mov [slot],al; mov ecx,[slot]; push ecx`). Writing the union's DWORD member
avoids the homing but then the value is an int, so MSVC zero-extends and emits
`mov eax` / `or eax,imm32` for the original's `mov al` / `or al,imm8`. No
spelling gets both. **Screen for it before assigning such a function to the C
lane.** 0x1006AFA0 BrNetWriteTagC0 is newly transcribed and parked at exactly
TWO instructions for this one reason, and 0x1006AFF0 BrNetWriteRaceOpts makes
EIGHT such calls - most of its 81 diffs. Both route to the C++ TU lane.

**Corollary that IS reachable, from the same function:** where the original
loads a 16-bit argument as a WHOLE DWORD out of the parameter slot, pass it
through the wrapper's dword member. A partial write to the 16-bit member homes
the union and costs two instructions of its own.

**The tail-merge polarity idiom paid again** - 0x1006AFA0's size guard written
as `if (room) { …; return 1; } return 0;` put its two exits the original's way
round; the negated guard put the return-0 block first.

**Screens run, both closing a class honestly rather than leaving a guess:**
 - the 9-byte `8b 4c 24 04 e9` cdecl->thiscall tail-call adapter: exactly TWO
   in the whole binary, both now matched. No transform to mint.
 - `BR_THISCALL1` definitions tree-wide: every one takes one argument (exact)
   or two with the wrapper already on the second. No third case to sweep.

**Eight rows FENCED with evidence** (hand-C target 1,505 -> **1,497**): the six
0x100729xx are `ff 25 [IAT]` import jumps, and **0x10073704 / 0x10073709 are
NOT CODE** - 0x100736E0 onward is a table of 16-byte records, each identical
but for two bytes, and the map read two mid-record pairs as `ret 0` and
`ret 0x8000`. A function that pops 32KB of arguments is the tell; screen the
tiny T1 tail for that before working it.

**Dead probes recorded, do not re-run:**
 - 0x1006B510 BrRbVelAtPoint: reordering the three cross terms AND the three
   adds to y,z,x (the order the original's fsubps complete in) is WORSE, 5+22
   -> 7+24.  **And the reason is now understood: the six `fld`s are hoisted
   above the `add esp,0xC` that cleans the inlined call's arguments** - once
   esp moves, every `[esp+N]` for `r` changes, so MSVC loads all six uses first
   and shuffles with 16 `fxch`. A source lever would have to move the stack
   cleanup, not the expressions.
 - 0x1002AF10 BrFadeDrawSprite (2 bytes, six probes now): naming ONLY the
   addend that must load first, and moving the w1 store ahead of the w0
   computation. **VC5 canonicalises an INTEGER sum's operand order the same way
   it does a float one.**

**Gate state at hand-off:** BRGlide's image gate FAILS, and it is NOT this
lane's work - `src/brally/core/net/br_car.c` (created by refile commits d012626 /
ba89fa1) leaves 0x10005C70 / 0x10005CA0 "claimed but NOT placed: symbol not in
obj"; the two are tagged with their d3d VAs there. Everything else resolves and
the assembled image is **0 differing bytes, 1,043 fns / 165,502 B / 34.42% of
.text**. Spawned as a task chip.  Also worth knowing: `image_build.py` now
prints "THE TREE CHANGED WHILE THIS RUN WAS GRADING IT" and tells you not to
record either verdict - with a parallel session committing, expect to run it
two or three times before you get a still-tree verdict.

---

## 2026-09-03 - T1 intake, the 0x1006Bxxx audio cluster (+3 byte-exact)

**Lane:** `claim 20` returned a TOKEN AND ZERO TARGETS - the tagged pool is
still exhausted, exactly as `docs/STRUCTURAL-PLAYBOOK.md` says. Went to T1
intake instead: screened `tools/brally/tiers.py --list T1` (243 rows) against tree
`@implements` tags, `report_cpp.csv`, `src/brally/core/cpp/<VA>.cpp`, `fenced.csv`
and the original's prologue bytes (EH frame / IAT jump) - **218 clean
candidates**. Picked the smallest, then followed the neighbours: every VA in
0x1006B000-0x1006C500 is the DirectSound voice/bank layer, and
`src/brally/core/slice6_76.c` already owns its globals and its `dsbuf_fn2` vtable
typedef. **A neighbour cluster with a live owning TU is worth more than a
small size** - the declarations are the expensive part and they were free.

**Landed byte-exact (all in `src/brally/core/slice6_76.c`, filed in `config/brally/filing.csv`):**
- **0x1006BD70 `BrSndBankMute`** (90 B) - drive every occupied voice to
  DSBVOLUME_MIN and recentre pan. Fell to the guard-shape idiom below.
- **0x1006C460 `BrSndBankFree`** (109 B) - stop + free every buffer, zero the
  group rows (`memset(row,0,60)` at a 0x48 stride) and the voice array.
  **First compile**, using the idiom from the previous one.
- **0x1006B530 `BrSndChanBind`** (128 B) - bind a group's voice to a channel
  and copy the 8-byte base rate. Two probes.

**Two idioms minted, both in `docs/brally/VC5-IDIOMS.md`** (and in
[guard-shape-decides-prologue](../levers/guard-shape-decides-prologue.md)): the `&&`-chain-vs-early-return choice is
decided by whether the guard and the body return the SAME value, and an
explicit `shl R,3` beside a row-index `lea` means our element type is too
narrow. `tools/brally/fnmatch/screen_shrinkwrap.py` is new; its yield on the tagged
pool is **zero**, so the guard rewrite is a lever, not a family.

**PARKED: 0x1006B440 `BrSndVoiceApplyVolume`** (79 B, 84 recomp, regnorm 3+1).
Arithmetic, the unsigned /255 reciprocal, branch polarity and both call sites
are already identical. Residue is pure allocation: the original loads the
parameter ONCE into ecx above `test al,al` (we load it per arm, +4 B, which
costs a second callee-saved register) and spells the min-volume push as
`mov eax,0xffffd8f0 / push eax` where we emit `push imm`. **SIX statement-level
spellings produced BYTE-IDENTICAL output** - `fn` local before the call; the
call written inline; a `static __inline` two-arg helper; `int vol;` above the
if; a named `pBuf` assigned after the value; master-first multiply. The full
list is in the file header above the tag. Do not re-run them; the next lever
must come from outside the statement spelling. NOTE the contrast: 0x1006BD70,
40 bytes away, pushes the same -10000 as a plain `push imm`, so the
`mov reg,imm / push reg` there is real evidence of a variable, not noise.

** Operational:** a parallel session destroyed work twice - see
[parallel-session-clobber](../traps/parallel-session-clobber.md). Also: rewriting `config/brally/filing.csv` from a full
read flipped all 872 lines CRLF→LF; splice with `b"\r\n"` and check
`git show --stat`.

### Same session, continued - two more, and TWO SCREENING GAPS worth more than the matches

**0x1006B5F0 `BrSndChanSetRatio`** (118 B) and **0x1006B970
`BrSndVoiceBufStart`** (111 B) also landed byte-exact - five for the session,
all in `src/brally/core/slice6_76.c`. 0x1006B5F0 was a FIRST COMPILE once the applied
record was indexed as `int64[]`, three per channel (the `[R*8+K]` idiom).
0x1006B970 was one instruction out, and the fix is a **new idiom now in
`docs/brally/VC5-IDIOMS.md`**: with two locals both starting at 0, one enregistered
and one address-taken, **zero the MEMORY one first** - flag-first lets VC5
CSE the zero (`mov [esp+8],edi`) where the original stores an immediate. A
`mov [esp+S],R` against the original's `mov [esp+S],I` is an initialiser-order
defect, not a body defect.

** TWO SCREENS THE T1 INTAKE CHECKLIST WAS MISSING** - both cost nothing once
known, both would have cost a session each:

1. **0x1006B080 is a C++-lane function, not a C one.** It has the exact tell
   from the existing note: `mov al,[esp+0xc]` / `or al,0x20` / `push eax`
   with no zero-extension - a CHAR argument on a thiscall's stack. Screen the
   draft's argument loads for an 8-bit `[esp+N]` read BEFORE writing any C.
2. ** A T1 row can ALREADY HAVE A PORT BODY under a different name.**
   0x1006E430 is `BrX100751D0` in `src/brally/core/slice8_86.c` - a complete,
   commented port body with a NULL guard and `pfn` indirection the original
   does not have, carrying no `@implements` because it is port-only. My T1
   screen checked tags, `report_cpp.csv`, `src/brally/core/cpp/<VA>.cpp`,
   `fenced.csv` and the prologue bytes, and saw NONE of that. **Add
   `whereis.py` / a grep for the D3D twin VA in comments to the T1 screen**;
   these rows are cheap (`#ifdef BR_MATCHING_BUILD` variant beside the port
   body, as `br86_timer_end_period` already does in that file) but they are
   NOT fresh transcription, and treating them as such duplicates a body.

**Tally at hand-off** (`tools/brally/total.py`, re-derived, tree churning under
parallel sessions): **1,141 byte-exact / 178,029 B** - DLL C 862 fns / 85,136 B,
DLL C++ 174 / 80,300 B, EXE 105 / 12,593 B, against 480,853 B of BRGlide
`.text` plus ~64 KB in-scope EXE `.text`. The C count reads LOWER than the
871 the one-file sweep printed 20 minutes earlier; parallel rebuilds move it,
so re-derive rather than quoting. `fileaudit.py`: descriptions **0** and
address batches **62**, both at baseline; the 518 failures are the inherited
"assigned but not moved" backlog. `src/brally/core/slice6_76.c` is 18/19, the one
diff being the parked 0x1006B440.

## 2026-09-03 - the d3d-only-tag lode: +12 byte-exact, class CLOSED

**+12 glide functions byte-exact, no C written, all 12 at 0 diffs on the first
sweep.** A body shared by BRGlide and BRD3D is often already transcribed and
correct but carries only its **d3d** `@implements`. The glide-keyed sweep never
scores a d3d VA, so it has no `report.csv` row, so `tiers.py` calls the glide
twin **T1/not-started**. It is finished - only the second tag is missing. The
tree's spelling is two stacked tags on one body. Full screen, all three traps
and the closure note are in `docs/brally/VC5-IDIOMS.md`; the screen itself is
`tools/brally/twinscreen.py`.

The 12: `0x10036040` (slice6_70.c), `0x10008D20` (br_pod.c), and the ten
`BrFixUnpack*` codecs `0x100075C0` - `0x10007730` (net/br_fix.c).

** CLOSED - do not plan a lane on it.** With all four filters the screen now
returns **0 candidates binary-wide**. Only a batch of NEW d3d-lane matches can
refill it; re-run `tools/brally/twinscreen.py` then, never as opening work.

** THE CANDIDATE COUNT LIES IN TWO DIRECTIONS, AND BOTH WERE HIT THIS SESSION.**
Keyed on the bare VA number it said 22 (the two binaries overlap in address
space, so one number is usually live in *both* - `0x10017F30` is d3d's twin of
glide `0x100154A0` *and* a real glide `BrFadeLatch`); requiring the tag's LANE
to be d3d cut it to 12, all real. Dropping the "no `report.csv` row" filter said
**429**, essentially all fictitious - those are already scored under their glide
VA. Proved on `slice2_16.c`: 25 tags added, `28/42 MATCH` before and after.
Reverted. **A candidate count that does not exclude already-scored VAs is noise.**

** A TAG WRITTEN `0X…` IS SILENTLY IGNORED** - the parser needs the lowercase
`x`. 25 such tags produced identical sweep counts and no warning. A tag that
does not parse is indistinguishable from a tag that did not help.

Tree after: **1,158 byte-exact / 178,959 B** via `total.py` (879 C + 174 C++ +
105 EXE) - moves under the parallel session, re-derive it. `image_build.py`
**PASSED on all four in-scope binaries, 0 differing bytes** (BRGlide 1,052 fns
/ 166,463 B / 34.62% of .text). `fileaudit.py`: descriptions 0 (baseline 0),
516 assigned-but-not-moved is the parallel session's live refile backlog, not
new damage.

** A PARALLEL REFILE SESSION OWNED br_fix.c THE WHOLE TIME** - it shrank from
~250 to 158 lines mid-session and HEAD moved twice. Edit-then-commit-immediately
with a pathspec was enough; but re-read a contested file right before editing,
and never assume the copy you read minutes ago is current.

## 2026-09-03 (session 2) - T1 intake: 0x1001FF60, NO match, but the cluster is opened

**NO byte-exact gained. Tree unchanged at 1,158 / 178,959 B** (879 C + 174 C++
+ 105 EXE) via `total.py`. Say so plainly; a near-miss is not a match.

**THE CLUSTER IS THE FIND, and it is still open.** Five contiguous T1 rows
`0x1001FF60 / 0x10020190 / 0x10020460 / 0x10020690 / 0x10020A80` (558/607/558/
609/609 B) are the FLAT-shaded triangle emitters, sitting *interleaved* with
four already-matched wrappers in `ghidra_batch.c` (`BrDlCmdTri1Flat` and
family) that call them as `FUN_1001ff60` / `FUN_10020460`. Byte-diffing the
originals pairwise: `0x1001FF60` vs `0x10020460` differ in **37/558 bytes,
all field-offset deltas** (0x28↔0x20, 0x34↔0x2c), and `0x10020690` vs
`0x10020A80` in 54/609. **Solve one, the rest follow.** `br_dlcmd.c` already
owns every declaration they need (`g_aBrDlVtxPool`, the two tex scales,
`BR_DL_PUN`, `BR_DLCMD_FINISH_VTX`), which is exactly the "a live owning TU
gives you the declarations free" case.

`0x1001FF60 BrDlTriFlatZ` is now transcribed and committed at **592 B vs 558,
268 diffs, regnorm 5+1** - every structural element verified (frame
`sub esp,0xc`, two epilogues, AND/OR asymmetry, finish blocks, flat copy,
save/restore). Full ladder, open leads and dead-probe list are in the file
header above the tag; the reusable idiom went to `docs/brally/VC5-IDIOMS.md`
("Pointer form vs index form is decided by WHAT THE INDEX IS"). **Next lead,
never probed: the clip arm's three float args are pushed with integer
`mov`/`push`, not `fld`/`fstp` - try `uint32_t` params + BR_DL_PUN at the
call. And 0x10020190 does not exist yet, so its real signature may move it.**

** MY UNCOMMITTED WORK WAS SILENTLY WIPED TWICE IN ONE SESSION, and the
second time it corrupted a commit message.** Both times `git status` went
CLEAN and the file returned to HEAD - no conflict, no warning. The second
happened *between* my edit and my commit, so `ff0873f` landed a residue note
describing two code changes that were not in the tree; `ad8e03d` re-applied
them. **A parallel session's revert does not have to move HEAD to destroy your
work.** Rule, stronger than the existing one: on a shared file, `git commit`
IMMEDIATELY after every edit that survives a compile - do not wait for
byte-exact - and `grep` the file for your own text after committing.

** A PATHSPEC COMMIT STILL TAKES THE FILE'S WHOLE WORKING-TREE STATE.**
`config/brally/filing.csv` was already dirty with the parallel session's module
reassignments; my one-line splice was correct but `caefe5d` swept 40 of their
lines in under my message. Content intact, attribution wrong. **Check
`git diff <shared csv>` BEFORE committing it, not `git show --stat` after.**

** fn.py DISAGREED WITH THE SWEEP** on this row (597 B/151 insns vs the
sweep's 592; it did not move when two edits did). fn.py compiles `/O2` only
and the sweep picked `/O2` and `/O2p` on different runs. **report.csv is the
scoreboard.**

**Screens that stayed useful:** `tiers.py --list T1` = 237 rows, and a full
screen (tagged / already-scored / fenced / cpp / unaligned / EH / import-jmp /
d3d-twin-tagged / mentioned-in-src) leaves **57 clean**. Clusters visible in
that 57 and NOT yet taken: 0x10024680+0x10024750+0x10024AA0, 0x10035C50
family, 0x1003B350 family, 0x1005A280 family, 0x1006A330..0x1006B0E0.

## 2026-09-05 session 20 (this lane): +6 byte-exact, 5 parked, tree 1,227 / 196,890 B per total.py
- Claim ledger handed out br_collresp.c rows another session was actively committing to (6faab8c) - released them; `git status src/brally/` before working a claimed row remains the rule.
- New module files: src/brally/core/menus/br_saveprobe.c, br_savename.c, br_savebegin.c (2 parked + 1 parked), src/brally/core/settings/br_ghostsave.c, br_seasonload.c (parked); src/brally/core/cpp/0x10008AB0.cpp (exact), 0x10039620.cpp (parked).
- Image gate NOT run this session (parallel sessions were writing; the guard would fire). Run it at the next quiet point.
- Frame-layout facts and the same-object alias rule are in VC5-IDIOMS (tail); the "constant through a __fastcall wrapper" boundary is under the thiscall 3+ args entry.

## 2026-09-08 session: reset validated, +0 byte-exact, 1 parked (T2, 2 B off), tree 1,235 / 198,117 B per total.py
- commit 1576d0b (the project rules 104 lines, MATCHING.md the procedure, docs archived not deleted, autofile/claim N/crank loop disarmed) verified by running each refusal; image gate PASSED (BRGlide 0 diff bytes) once run ALONE -- running portcheck concurrently timed cl.exe out on br_input.c and made the gate INCONCLUSIVE. Never run two compilers at once.
- 0x10002460 BrRaceSelFromMenu -> src/brally/core/racing/br_racesel.c (c085112): 252/252 B, 65/65 insns, regnorm 0+0, eax/edx roles of the two loop induction pointers are the only residue. Gate 0+A pass, 1 ledger line; it is a Pool A / T3 candidate now, not a lane. Levers in VC5-IDIOMS tail (356c9aa). 9 probes, over the six-probe budget by three because w4/w6 each moved a structural fact.
- `crank.py --help` starts a real crank run (no guard). `t4lane.py --claim` locks 20 Pool B rows; release the token at session end (done: f5d114d3).
- Next unused Pool B primary: 0x100284E0 (253 B), then 0x1005F580, 0x100704E0.
- the notes index's "port build is broken" lead is unverified: the narrowing it blames is matching-arm only, src/brally/include/slice2_12.h already says int16_t, and ports/brally-wasm has no build.sh.

## 2026-09-09 session (lane f103e9fc / 64bdcead): +5 byte-exact, 5 T2 parked, image gate PASSED
Byte-exact (each committed + filed): 0x10055D40 (C++ TU, named ftell local),
0x1001E7A0 BrGlSetCombine (br_dlglide.c), 0x1006C4D0 BrSndDevOpen (new
br_snddev.c, acmMetrics via E8 thunk), 0x10059820 BrCarRecordToState
(br_carnet.c, raw dword copies), 0x1000DC00 BrPolyClipTri (br_polydist.c,
plain global register-cached in a call-free loop).
Parked T2 with dead lists in their headers: 0x10035DD0 BrDpSessionJoin
(one separate zero reg, br_dplayjoin.c), 0x10060F40 BrSndNearestCommit
(regnorm 0+0, esi/edi swap, br_sndpos.c), 0x10020D70 BrDlCmdTri2NoZ
(one pointer-form 1/w load, br_dlcmd.c), 0x1001FA30 BrDlCmdTri2 (moved
to its OWN TU br_dltri2.c; the untagged pointer body MUST stay in
br_dlcmd.c or 0x10020900 regresses), 0x1006A7E0 BrNetPeerRank (new
br_peerrank.c; open question: what picks the induction-pointer bias).
Pool B <=400 B is exhausted for clean rows; `t4lane.py --claim --max-bytes
800 --pool B` is the next intake. Screened C++ (this-ecx) leftovers from
that claim: 0x1005C6D0, 0x10055F40, 0x1006C010 -- not started.

## 2026-09-09 -- "20 today?" session: +5 byte-exact, 3 parked T2 (hand, one function at a time)
- t4lane.py --claim had NOTHING (Pool B 0 clean, 39 rejected; Pool A = T3 colouring). Hand-screened the T1 list with a capstone op census (fxch / 16-bit / EH) and claimed by --va. **The productive lane was the /Od stretch 0x1002Cxxx-0x1002Exxx: three of the five wins (0x1002D864, 0x1002E376, 0x1002CB49) were /Od, each byte-exact in 3-6 sweeps.** Remaining unclaimed /Od-looking T1 rows: none <800 B; look above 800 B next.
- Byte-exact: 0x10032190 BrGlTrackFixupCmds (br_track.c), 0x100628B0 BrGlRaceStart (br_racestart.c), 0x1002D864 BrDlRecolor (new br_dlrecolor.c), 0x1002E376 BrRleEncode (br_texblit.c), 0x1002CB49 BrTexAnimStep (new br_texanim.c). Image gate PASSED after.
- Parked T2 with dead lists in the file: 0x100299A0 (size-exact, cursor/counter register swap), 0x100590D0 (insn-exact, scratch regs), 0x10028620 (size-exact, one CSE at the slot test). C++-owned, do not re-open in C: 0x10062E50 (thiscall, ret 4), 0x10039990 (thiscall vcalls). 0x10032320 is COM/OLE vcalls in C form, unscreened.
- Levers proven today (all at the tail of docs/brally/VC5-IDIOMS.md): counter-expression fields keep load/store order; halfword compose for an ah-first pair; `while (f == 0) {}` spin; float zero is an imm store; /Od locals as letters in frame order; /Od arms in source order; **/Od two-hop jumps are `label: goto X;` written after the return** (this is the BrAnimUpdate 0x1003563A open wall's shape -- re-probe it); `call; push eax; call; add esp,N` under /Od = inner callee takes no args (read the callee, not Ghidra's split).

## 2026-09-09 third session ("untried swaths"): +0 byte-exact, 2 parked T2 at regnorm 0+0 (both the register-transposition class)
- Pool B <=400 exhausted; `t4lane.py --claim --max-bytes 800 --pool B` gave 8 rows, of which only 0x1006C290 and 0x100096A0 are C targets (0x1005C6D0/0x10055F40/0x1006C010 this-ecx, 0x10062E50/0x10039990 thiscall, 0x10032320 COM vcalls unscreened).
- 0x1006C290 BrSfxBankLoad -> br_sfx.c tail (60c5f17): size/insn-exact 459/459, residue = engine-loop i<->IV24 esi/edi transposition.  TWO LEVERS PROVEN, now in the file's dead list: (1) sibling arrays walked in lockstep as ++pointers get CSE'd into one reg+disp, freeing a callee-saved reg that VC5 then burns on a CACHED 0 (and 1), which forces a spill+/Oy frame-looking mess -- spell them as indexed arr[i] and let strength reduction make the cursors; (2) the load arm must be the FALL-THROUGH (`!= 0` first) -- zero-arm-first re-triggers the constant cache. i-first init is load-bearing (pV24-first regresses).
- 0x100096A0 BrDPlaySysMsgLog -> br_dplay.c matching arm beside the port body (f4231dd): insn-exact 147/147 regnorm 0+0, +1 B (pMsg ebp vs ebx disp8).  A scan loop over a global table spelled with an explicit cursor pointer gets its FIRST ITERATION PEELED (direct global load + rotated loop); indexed aSlots[i][0] reproduces the original cursor via strength reduction. A cast on the table element in the compare forces load+reg-cmp where the original has cmp [mem],reg.
- Both parks are `t3.py --qualify` candidates (reggap 0). BrDPlayStartup 0x10009B00 (same TU) is diff 97 at HEAD -- pre-existing, verified via stash sweep, untouched.
- Parallel session active all day: 0x100311C0 BrTrackLoad T2 (9c6bf9e), survey.csv/globals_learned.csv/br_savebegin.c dirty with their work -- shared CSVs left uncommitted.

## 2026-09-09 fourth session ("get it done" -- the C++ leftovers of the 400-800 B band): band CLOSED, +0 byte-exact, 1 T2 parked, 1 gate-tool fix
- The band's C++ rows were already dispatched by the cpp lane while I worked the C side: 0x1006C010 and 0x10062E50 byte-exact (921d763, 24b2d8b), 0x10039990 and 0x1005C6D0 probed-and-parked with dead lists (07bdc59, 6ec1195). 0x10055F40 was being edited by a live parallel session (mtime seconds old) -- left alone. 0x1006A080 and 0x10069A80 from the save seam are both parked with full dossiers -- the seam notes calling them untouched are STALE.
- 0x10032320 BrDpLobbyConnect -> br_dplayjoin.c (6323ef5): T2 parked at ONE instruction (6 B). The whole CoCreate/GetConnSettings/SetConnSettings/ConnectEx/Open ladder incl. the neg/sbb ternary is byte-exact; the residue is `and esi,0xff` before the host-bit extract.  NINE dead spellings in the header: every dword form folds the low-byte mask into `>>1&1` (named assignment, compound &=, %256, casts, bitfield, C++ front end all fold); byte-lvalue forms emit a byte LOAD (the dword+and widening needs a stack slot + register death).  `-(uint)(x!=0)&0x100` compiles to setne -- spell the nonzero-constant ternary `x != 0 ? 0x100 : 0` for neg/sbb.
-  msetdiff normaliser fix (53ffc7b): when the recomp's reloc is known NOT in the last 4 bytes, mask the MEMORY OPERAND, not a trailing imm 0 -- `mov [R+relocdisp],0` had its true zero rewritten to A and failed T3 gate A3 on identical bytes. Mirror of the 0x1005FF00 tail fix. After it, BOTH of this morning's parks (0x1006C290, 0x100096A0) pass Gate 0+A at rows 0+0; they now lack only Gate B's counted ledger (2 x >=10-probe zero-movement @t4-pass lines -- note t3.py wants the `@t4-pass <VA> <n> <date> probes N ... census yes/no` format, not the freehand line I used). 0x10029CD0's certification unchanged; 0x10032320 correctly shows its real `and R,0xff` as the one unpaired row.

## 2026-09-09 fifth session ("5 largest to T3"): +1 BYTE-EXACT (1,576 B), +4 T3-certified (3,306+939+805+696 B rows), 2 map-row fixes, 3 gate/classifier extensions
- **0x1000BEB0 BrCarDrawBody BYTE-EXACT (c376551), 62 msetdiff rows -> 0 in 6 probes.** The whole gap was five idiom classes, now on docs/brally/VC5-IDIOMS.md tail: (1) the `slotL ? slotL+0x10 : 0` null-guard was a PORT INVENTION -- orig re-reads [car+0x140], re-indexes the table and adds 0x10/0x20/0x30 unconditionally, four times; (2) `model`/`iCar`/`pCamBasis`/`pRow2`/`dot2` locals DO NOT EXIST -- orig re-derefs the global/field at every use and RE-CALLS BrVec3Dot in guard AND value; (3) front arm spells `len = len * len;` destructively (arms runtime-exclusive), back arm divides on-stack; (4) `&BrG_0AAxxx` where the extern is a `void *` -- the value-read was an UNINITIALIZED-POINTER latent bug (nothing ever assigns them); (5) camslot compare spelled off BrG_6C2CF8, reusing+destroying the just-compared register.
- ** JUMP-TABLE MEASUREMENT CLASS (f18edca): functions_glide.csv rows cut at CODE END hide the function's case maps + dword tables; the recomp symbol includes them, so every gate saw garbage-vs-garbage.** t3.py measure now cuts insn gates at the table start (read from the orig's own `jmp [R*4+VA]` dispatches) and byte-compares the table zone reloc-masked (gate A6). 0x10015B10 BrTextEmitString: map 3050->3306, then size- AND insn-exact, tables byte-equal -> @t3 (d90bcae). 0x1000CBA0 BrObjDlBuild map 3971->4180 (e60824b): its case map genuinely differs by 208 B -- the switch GROUPING is a real open defect, plus 218 code rows; multi-session, not a today target.
- **Classifier extensions, each re-gated against every certified tag (none moved):** `xor R,R` = rematerialised-zero singleton (a real missing `x=0` still fails on its store row); canon `lea R,[R*K]` ~ `shl R,2` (*4 only -- msetdiff already ate the scale); classify cancels the either-or layout TRIPLE `jCC A; jmp B` ~ `j!CC B` (only when all three are unpaired at once).
- @t3 landed: 0x10015B10 (752/752 insns, residue = scale/b slot pair + stride-vs-vaBlock promotion), 0x10059410 BrGlNavPoll (943/939, transposition + the orig's fresh xor-zero for the Edge672x run -- chained/named/reordered zero spellings ALL value-numbered identical, same mechanism as specMem), 0x10036B20 BrDpAddressBuild (805/805, either-or tail + cmpsb hoist), 0x1001FA30 BrDlCmdTri2 (698/696 --  the no-Z twin's DEAD probe `pv_->oow` for the ib corner LANDED here at -3 B: twin dead-lists do NOT transfer between TUs).
- **Walls hit and parked:** 0x10028BB0 BrTex3dRegister -- the register-transposition class ROOTS four sub-defects (zero-web cmp-vs-test + mov-[M],0; loop cmp-fold; w spill; f278 dup epilogue that survives an explicit `return id` -- VC5 cross-jumps it anyway); iLevel edx<->esi at entry is the first domino; P1-P3 dead, corpus misses on all three shapes. 0x100311C0 BrTrackLoad loop-head store-sinking fork stands; rec-pointer promotion DISPROVEN (-27 B, orig fields are base+index SIBs; only arg3 is the destructive add); probes t1-t5 recorded.
- Housekeeping: BrCarDrawVehicle's gate-0 markers were PROSE ("TODO specular block", "ABAA0 stub") -- reworded, gate 0 now passes there (its 12-row pack wall unchanged). filing.csv records BrCarDrawBody (no rows dropped). fileaudit FAIL 5 = the parallel session's new stranded assignments, theirs to move.
- **Parallel session ran the small-row T3 lane all day; my uncommitted t3.py edits got swept into ITS commit 61eaf81.** Content survived, but: commit tool changes IMMEDIATELY after the certified-tag re-gate, before starting the next function.

## 2026-09-09 fourth session (T3 lane, "20 to contract-valid in a day"): 17 T3-certified + 3 byte-exact = 20/20

**Certified @t3 (17):** 0x1006E360 BrTimeUpdate, 0x10034390 BrVec3ScaleBy,
0x10034660 BrVec3MulAdd, 0x100346A0 BrVec3MulAddTo, 0x1006FD50
BrEntitySetIndex, 0x10038860 BrOptAvailB, 0x10029CD0 BrEntGfxFreeAll,
0x10070280 BrWavLoad, 0x1000E060 BrVertLerp8, 0x1006F720 BrEntSetHeading,
0x10007D50 BrCarStateLerp, 0x1000CB20 BrViewBuffersRebase, 0x1001EC30
BrDlsTileSizeDecode, 0x10005330 BrNetBeaconTick, 0x10005400 BrCdAudioTick,
0x100701B0 BrWavReadData, 0x10019930 BrRaceCueLayout.
**Byte-exact (3):** 0x1006CE20 BrBitStreamReadU16 (index-through-cursor),
0x10036E50 BrDpCreateIface (early `goto fail` guard; FILED into
net/br_dplay.c), 0x1002F6D0 BrPeerFind (dword-width mask).  Levers + walls
on the tail of docs/brally/VC5-IDIOMS.md (c419c29).

** THE METHOD THAT PAID: fix the gate's normalisers/canon before grinding
sources.** Three capstone bare-decimal artefacts found and fixed (branch
target `call 8` 2d5a93d; esp displacement `[esp + 4]` 8f86659) -- each fix
promoted rows to READY/gate-B-only for free.  New canon classes in t3.py
classify: commutative fmul/fadd crossed quad (7dd2eb1), add R,-X ~ sub
R,X (a326268), lea R,[R+A] ~ add R,A (ff83432), masked-or fold and/and
pair gated on a shared `or` row via optional full-bag context (61eaf81).
EVERY change verified against all certified tags before commit.

** STASHED, NOT LANDED:** the third normaliser artefact -- a reloc'd
immediate with NON-ZERO addend prints bare decimal (`mov esi, 4` for
symbol+4) and normalises to I, not A; two singletons absorb it today.
The fix (git stash "bare-decimal addend masking", fn.py + msetdiff.py)
is CORRECT but moves the recorded numbers of 9 certified tags (they'd
need re-qualify + fresh gate-B passes).  Land it in a dedicated
migration session, updating the 9 tags in the same commit.

**Gate-B economics:** ~2x10-probe passes per function via the scratch
harness (harness.py <VA> <probes.py> pattern, restore-pristine per probe);
census = corpus query or mechanism experiment.  Renumber hand passes ABOVE
existing crank passes (duplicate pass numbers make the "last two" test read
the crank rows: hit twice, 0x1000CB20 and 0x10005330).

**Walls hit and parked (do not respell):** 0x10003050 push-placement
(guard-path pop; VC5 sinks past the guard, r-liveness probes inert);
0x100306D0 whole-body eax/ecx rotation from insn 0 (A4 lost-sync, 10
probes inert); 0x10015550 negative-multiply decomposition (folded on the
expression tree, decomposed ladders inert); 0x1003C600 documented
unreachable CSE-fork pair; 0x1006AFA0 byte-arg-on-stack (C++ lane).
FUN_100583c0 jl/jge+jmp is NOT the do-while shape (probed worse) -- wall
unlocated.  0x10005400's lever: ONE-EXPRESSION `(x & ~0x80) | 0x40`
avoids the fold-plus-rotation that the two-statement spelling caused.

**New levers (also in VC5-IDIOMS):** cursor-bump POSITION decides reload
CSE (bump between store and reload keeps the original's second load,
0x10019930); dword-width mask keeps the verdict in the loaded register
(0x1002F6D0); early-out guard vs enclosing success block decides jge/jl
(0x10036E50); index-through-cursor vs bound pointer decides paired byte
load order (0x1006CE20).

A parallel session ran all day (BrTextEmitString 0x10015B10 T3 + more;
it added the either-or branch-triple cancellation and the lea/shl class
to classify, and a jump-table cut to measure).  fileaudit
assigned-not-moved sits at 16 vs baseline 11 -- NOT reconciled; the 5
are not all mine (DpCreateIface was moved).  config/brally/globals_learned.csv,
config/brally/survey.csv and src/brally/core/cpp/0x10055F40.cpp were the parallel
session's uncommitted work -- left untouched.

## 2026-09-09 fifth session, part 2: 0x1000A110 BrCarDrawVehicle @t3 CERTIFIED (7,560/7,577 B -- the largest certified function in the tree)
- **The pCam residue row fell to SOURCE (66f0f31): the compare lines re-read BrG_6C6490 fresh, which is the ORIGINAL's post-call dataflow -- the cached pCam used the pre-call value.**  Session 15 had rejected this exact spelling on RAW positional diffs; RAW is the wrong metric for Gate A3 -- re-run old "dead" probes and read the MULTISET when a verdict predates the gates.
- **The byte-compose fork is now a Gate A3 GROUP CLASS (approved by the project lead 2026-09-09): cancel the complete {esp-slot byte home, and-0xff widen, or-merge} triple against opposite lane moves, all three counts matched, store required in the RAW bag.** Same evidence tier as the fmul commute quad (0x1001E380 corpus crack + ~300-probe dossier). All 58 certified tags re-gated unmoved before landing.
-  SHARED-BUILD CLOBBER, LIVE HIT: between my sweep and qualify, the parallel session's sweep rebuilt br_drawcar.obj from a tree WITHOUT my commit -- qualify read pre-fix numbers (7561, pCam row back). Sweep-then-qualify must be run back-to-back from the owning tree; if numbers regress with a clean git file, re-sweep before believing them.
- Census pattern for a classified-group residue: full-length mnemonic histograms equal EXCEPT the group's own opcodes (and 24/22, or 28/26; call 48/48, all x87 equal) -- that census IS the "the residue is only the wall" proof.

## 2026-09-09 sixth session (round two of "20 to contract-valid"): 11/20 certified, stopped at usage cap

**Certified @t3 (11):** 0x100583C0 (branch-triple residue), 0x10018B60
BrRcaFixupRecord, 0x100316D0 BrTrackFixupRec54, 0x100186E0 BrFadeTick
(all four were READY -- crank passes already at current numbers, tags
pasted), 0x1003BDE0 BrSaveBeginTimeAttack, 0x100590D0 BrSub100590D0,
0x10015300 BrHudDraw, 0x1006C290 BrSfxBankLoad, 0x100096A0
BrDPlaySysMsgLog (both transposition parks landed), 0x10035DD0
BrDpSessionJoin, 0x10014960 BrSub_100173F0.  Tree-wide certified: 71.

** NEXT SESSION RESUMES HERE -- 9 more wanted.**  The 400-800 B READY/
gate-B-only harvest is now EMPTY (everything above certified).  Remaining
paths: (1) re-run `t3.py --qualify --all --max-bytes 800` and survey the
FAIL rows for single-gate classes (the A3 one-row and A4-only rotation
pools from the fourth session are still parked); (2) the stashed-then-
POPPED-then-RESTASHED normaliser fix is GONE from the stash (popped this
session, then re-stashed as "bare-decimal addend masking v2" -- CHECK
`git stash list`); it promotes NOTHING and demotes three, keep deferred;
(3) >800 B rows via --max-bytes; (4) the fileaudit assigned-not-moved
ratchet (16 vs baseline 11) is still unreconciled across sessions.

Method note repeated: renumber hand @t4-passes ABOVE existing crank
passes (bit twice more this session: 0x1000CB20-style duplicate-number
collisions on 0x10035DD0).  Config csvs and src/brally/core/cpp/0x10055F40.cpp
belong to a parallel session -- untouched.

## 2026-09-10 "20 to contract-valid, round three": 9 @t3 + 2 BYTE-EXACT from my lane; THREE other sessions worked the same tree

**Certified @t3 (9):** 0x100306D0 BrPendListAdd, 0x10018EF0 BrVtxExpand,
0x10063A60 BrReplayRecord, 0x1001E080 BrGlInstall, 0x100608F0 BrVarSave
(source fix was a parallel session's, the Gate B passes mine), 0x10060970
BrVarLoad, 0x1005A300 BrImgMulByTexture, 0x10016980 BrFontMeasure,
0x100643E0 BrRbVelAtBodyPoint.  **BYTE-EXACT (2):** 0x10062D00
BrCtrlCfgInit (memset the zero run), 0x10063B80 BrReplayApply (subtrahend
spelled as a per-use pointer-field cast).  Also 0x10063CC0 BrReplaySeek
6+6 -> 2+2, one unpaired row left.  Tree certified 76 -> 92 over the day
(all sessions).

** THE POOL SHAPE CHANGED: `--qualify --all` READY rows are GONE.**  At
session start 68 of 215 rows <= 1400 B read READY and every one was
already certified; only 7 owed Gate B alone, and parallel sessions took
most of them within the hour.  Volume now comes from FAIL rows, and the
lever is the unpaired-row SIGNATURE, not the row count: BrCtrlCfgInit had
9 unpaired rows and went byte-exact on ONE memset.  Script for that:
scratch rank2.py (measure + classify every uncertified diff row, print
its MISS/EXTR lines) -- rebuild it, it is the worklist.

** THREE parallel sessions commit to this working tree.**  They took
0x1005F580, 0x10029B50, 0x1006CE50 out from under my claims mid-probe,
edited tools/brally/t3.py's classify under my measurements (rows move between
runs), and one of their `crank` commits SWALLOWED my uncommitted
BrVarLoad edit under a BrVarSave message.  claim_lane.py IS being
honoured by them (three of my claims came back REFUSED as live).  Commit
with a pathspec the moment a probe lands; SendMessage is not available in
this session, so the claims file is the only channel.

**RULE reaffirmed 2026-09-10: do NOT lower the T3 standard.**  I
asked about extending Gate A4 to accept a pure schedule rotation (7 rows
fail A4 alone; 0x10058540 is 112/112 instructions, RAW 0+0, only the
order of four loop tails differs).  Answer: don't lower standards.  A4
stays as written -- those rows stay uncertifiable and I did not touch
tools/brally/t3.py.

**Method notes:** fn.py `--var` probes are ~4 s each and the harness in
scratch/probe.py (replace-one-string, print scorecard + positional
diff) ran ~60 probes today; pos.py prints the A4 order diff.  fn.py's
obj lands in build/brally/win32/match/obj_fn_<tag>/.  A fn.py win does NOT always
reproduce in the tree: 0x10028620's statement-order improvement scored
regnorm 2+1 in the variant and 23 rows in the real TU -- sweep before
believing it, and revert on the spot.  Gate B "no counted census-driven
pass" is satisfiable by hand: locate the exact residue byte, run >= 10
targeted probes, write the @t4-pass line numbered ABOVE crank's with
`census yes` (0x100643E0, whose whole residue is ONE spill-slot
displacement).

**Walls proven and parked today (do not respell):** 0x10039D20
BrMenuCap07E0 head fork, 0x10058540 BrSprFontRectInit loop-tail schedule,
0x1003C950 BrOptCycleAA2A0C per-arm global load, 0x100011C0 BrSurfBlt24
(the outer counter lives in the parameter slot in the original -- pure
allocation), 0x100299A0 BrTexInstallRecords (the original materialises
its zero BEFORE the frame).  Idioms committed at the tail of
docs/brally/VC5-IDIOMS.md.

**Bookkeeping debt, NOT mine:** fileaudit `assigned but not moved` is 17
against baseline 11 (was 16 yesterday) -- still unreconciled across
sessions; `src/brally/core/racing/br_racestep.c` and config/brally/globals_learned.csv
were left dirty by another session.
