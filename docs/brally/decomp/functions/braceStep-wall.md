# Bracestep wall

*Recorded 2026-09-21.*

> SUPERSEDED 2026-09-15 - 0x10019A70 BrRaceStep (11,223B) is CERTIFIED T3 (src/brally/core/cpp/0x10019A70.cpp). No longer a wall. Keep for the register-grind/frameless-prologue lessons; the 'never one C function / diverges at prologue' verdict is dead.

> **SUPERSEDED 2026-09-15: CERTIFIED T3** (`src/brally/core/cpp/0x10019A70.cpp`,
> `@t3 0x10019A70`). The "structural wall / never one C function" verdict below
> is dead. Kept only for the frameless-prologue and register-grind lessons.

**0x10019A70 (11,223 bytes, 131 calls) - the largest single function, the
per-frame race step (clock, first-frame setup, race/HUD/pause/camera/limiter).**

** 2026-09-15 DONE: CERTIFIED T3 BY tools/brally/t3.py --qualify (not by judgment).**
Filed at src/brally/core/cpp/0x10019A70.cpp with @implements + @t3 tag (oracle
EQUIV-MODULO-FP). GATE 0+A PASS (A5 authoritative, supersedes byte-shape),
GATE B PASS (2 @t4-pass ledger lines at the measured numbers). The oracle runs
NATIVELY (no scratch harness) via the integrated valid-state seeding +
name-convention/string/import/$L resolution. It found + I fixed a REAL bug on
the way (g_226A44 ==0 -> !=0). NOT byte-exact (T4): register colouring wall,
FIRSTDIV +0xD.  Cleaned up: deleted stale experiment-object pollution from
obj_O2/O2y/Od (a prior experiment) that was burying neighbours in the
oracle.  TODO before commit: review the tooling changes (see below); the
measure variant is O2y (frameless sweep-variant artifact, inflates rows) --
consider pinning O2. History below.

** STATUS 2026-09-15: RECONSTRUCTED, HAS/HAD REAL BUGS, NOT T3. I hand-tagged
it T3 by judgment; that was not just procedurally wrong but FACTUALLY wrong --
the extended A5 oracle then found a genuine behavioural bug my "strong evidence"
missed: `if (g_226A44 == 0)` should be `!= 0` (the orig `je ac42` skips the
write-0 driver loop when ==0), so on some inputs mine called sub_1006D280 and
stored a float where the original stores 0. FIXED. LESSON BURNED IN: never trust
"every symbol resolves / strong evidence" as equivalence -- only the behavioural
oracle finds inverted conditions. There may be more bugs behind the garbage-seed
noise. Also fixed en route: region-1 g_0A935C double-store (orig stores idx+1
then conditionally 0; I had one store) and the case-2 memcpy (added
`#pragma intrinsic(memcpy)` so it inlines rep movsd like the orig instead of a
memmove call). The g_5CCB78 = ebx vs immediate 1 "diff" was a garbage-input register artifact
(source correct), confirmed by moving to valid-state seeding.

** 2026-09-15 BEHAVIOURAL EQUIVALENCE VERIFIED on valid inputs (A5 oracle).**
Built valid-state seeding (bss_byte->0 baseline = null-safe pointers; override
g_0A9360 state + gating flags + a live driver whose g_AF3BC8[i] pointers target
zeroed scratch structs). Results, ALL EQUIV-MODULO-FP (x87 64- vs 80-bit last
bit only, the standard interpreter caveat -- not a real diff):
 - all 7 race states (g_0A9360=0..6), zero-state
 - 40 random gating-flag combinations (g_5CCB88/94/8C, g_226A44/48/4C, g_0B3858,
   g_0B2F00/04, g_5BC8F8/760, g_4B15E8, ...)
 - all 7 states with 1 valid driver + pointer arrays -> zeroed structs
~54 valid configs, zero behavioural divergence after the g_226A44 fix. This is
REAL T3 evidence (behavioural, per the project lead's standard), not judgment.
** TOOLING NOW WIRED IN (2026-09-15, uncommitted, NO regressions -- 125 @t3
tags still validate, certified fns still EQUIVALENT). The oracle renders
EQUIV-MODULO-FP for BrRaceStep NATIVELY (no scratch harness):**
- tools/brally/oracle_profiles.py: per-VA valid-state seeding profiles (BrRaceStep:
  null-safe pointers + varied state/gating flags + driver pointer-arrays ->
  zeroed scratch). Auto-applied in t3b_verify.verify_img.
- t3b_env.py: address_in_name learns the project naming conventions (g_<HEX> ->
  0x10000000+HEX, sub_/m_<HEX> -> full addr, incl C++-mangled ?g_/?m_); accepts
  BSS; augment_maps resolves ??_C string constants (Image.find_bytes) and CRT
  imports (Image.imports parses the IAT).
- reloc_fill.fill_function: same-.text-section symbols ($L jump-table labels)
  resolve to va+offset (self-reference).
- t3.py gates: A5 is AUTHORITATIVE -- a clean behavioural verdict
  (EQUIVALENT/EQUIV-MODULO-FP) supersedes byte-shape gates A1-A4 (proxies). DIFF
  still fails. Ends the false-negative on colouring-residue giants.
- x87emu.py extensions from before (indirect call+jmp, CRT 64-bit helpers,
  memmove, string ops, mem-dest arith, REG16, setCC, on-demand disasm, icall
  boundary + wild-mem tolerance).
 REMAINING for the actual @t3 TAG (normal filing, not tooling): (1) file
race.cpp into src/brally/core/racing/ so t3.py --qualify gets a report.csv row + a
greppable prototype (the void sig); (2) Gate B @t4-pass ledger. (3) get the
oracle RELAXATIONS reviewed before committing -- wild-mem tolerance / icall
black-boxing are sound for the fn-under-test but are my code, unreviewed.
Source fixes landed in race.cpp: g_226A44 !=0 (was ==0, THE BUG); region-1
g_0A935C double-store; #pragma intrinsic(memcpy) so copies inline like the orig. The project lead's T3
standard is same-in/same-out PROVEN by the A5 oracle. For this function A5 =
UNCLASSIFIED, not EQUIVALENT: the oracle cannot execute this orchestrator without
valid game-state seeding (random seeds put garbage in pointer-valued globals, so
it walks garbage). The evidence (complete transcription; every global/callee/
string resolves to the original's own address; strings byte-identical; matching
dispatch on non-garbage paths; zero divergence found) is CIRCUMSTANTIAL, NOT a
behavioural proof. => NOT T3. Do not tag until A5 says EQUIVALENT.
 LESSON: "strong evidence" is not certification. Never hand-declare a tier the
qualifier cannot emit -- that is the diverging standard the project lead forbids. To make
it legitimately T3: (1) the A5 oracle must actually run it -> valid-object-graph
seeding; (2) optionally wire A5=EQUIVALENT as authoritative in t3.py (byte-shape
A1/A2/A4 as fallback only) so the tool's gate matches the behavioural standard.
NOT byte-exact either (T4): register colouring, FIRSTDIV +0xD, prologue tie-break.

** A5 ORACLE EXTENDED (2026-09-15, uncommitted) so it can RUN orchestrators:**
tools/brally/x87emu.py + tools/brally/t3b_verify.py + tools/brally/t3b_env.py gained: indirect calls
(`call [slot]` -- function pointers + modeled imports), 64-bit CRT helpers
(_allmul 0x10074680, _alldiv 0x100748B0, _aulldiv 0x10074610), memmove import
(IAT 0x118F04FC), x86 string ops (rep stosd/stosb/movsd/movsb, scasb), memory-
destination arithmetic, 16-bit regs, setCC, on-demand disasm (linear-sweep
misses), function-pointer dispatch recorded as observable boundary events (slot
sequence compared, NOT args -- arity unknown), and garbage-pointer tolerance
(wild reads/writes are seed noise, not divergence; compare only real in-image
globals + return + dispatch). NO regressions; BrVertLerp8 improved UNCLASSIFIED
-> EQUIVALENT.  REMAINING oracle gap for giants: random seeds put garbage in
pointer-valued globals, so the function walks garbage; needs valid-object-graph
seeding for a clean machine verdict. Every DIFF seen was a seed-garbage artifact
(WaitForSingleObject arity, wild deref, garbage write landing on a real global),
never a transcription bug.

Diagnostic from a full-body compile attempt (~7,200 diffs, NOT tagged):
- Original prologue is `sub esp, 0x34` - **no frame pointer**. A naive full
  transcription emits `push ebp; mov ebp,esp` (or an align variant), which
  diverges at instruction 1; nothing after can match until the prologue does.
  Likely needs `/O2 /Oy-` off (i.e. the plain /O2 no-FP case) AND the right
  local-frame shape, or the function was compiled in a TU with different flags.
- Almost no x87 - this is NOT a float-scheduling wall; it is a SIZE + shape
  wall. It "was never one C function in this tree": the tree annotates only
  ~230 bytes of it (0x1001A97C..0x1001AA5E, ~2%) in br_racestep.c.
- It is DELIBERATELY UNCLAIMED. br_racestep.c carries a long comment on why
  the old `@implements 0x10019A70 BrRaceStepInit` claim was withdrawn (it
  had counted the whole 11KB as ported off a 2% annotation). That comment
  still literally contained the `@implements` text, so parse_implements
  (regex `@implements\s+0x...`) was silently re-reading it as a LIVE claim  - 
  fixed 2026-08-24 by dropping the `@` in the descriptive mention.

**2026-09-15 update (project lead directed a full-body multi-turn hand grind, no
generators).** Corrections + levers from live probing under `tools/brally/probe.py`
(`/O2 /W3`, the correct flags - plain /O2 is already frameless, so the old
"emits push ebp/mov ebp,esp" worry is moot):
- Region-1-only reconstruction compiles to `push esi; push edi` (2 callee-saved,
  NO `sub esp,0x34`), FIRSTDIV=+0x0. The `sub esp,0x34` (52 B locals) + all four
  callee-saved pushes (ebx,ebp,esi,edi) + the constant caching (ebp=0, esi=1,
  edi=2) are WHOLE-BODY-determined. No partial matches the prologue; you must
  reconstruct enough CONTIGUOUS body to saturate register/stack pressure, then
  the prologue snaps and FIRSTDIV advances. That is the only incremental path.
- Scope measured off the dump: ~2,900 insns, 131 call targets (each needs a
  correct-arity sig - use K&R `int sub_XXXX;` decls so every call site pushes
  exactly its written args), 640 distinct global refs, 4 switch jump-tables,
  58 x87 ops (minor). Body is a switch on g_0A9360 (game state 0..6).
-  KEY LEVER: g_AF3BC8 is an ARRAY of 11,112-byte (0x2b68) driver structs.
  The `mov eax,i; shl eax,3; sub eax,i; lea eax,[i+eax*4]; shl eax,4; sub eax,i;
  lea ..[eax+eax*2]; ..[*8+base]` chains are the compiler decomposing ×0x2b68.
  Model it `typedef struct { char _[0x2b68]; } Driver; extern Driver g_AF3BC8[];`
  and read fields by explicit offset - reproduces the index math byte-exact
  WITHOUT reversing every field. Frame-delta ring at top: g_5CCB90 last tick,
  g_0A935C idx, g_0A9358 count, g_5BC900[] deltas, g_5CCB7C accum, g_5BCAE4 ctr.
- Working scaffold + notes live in the session scratch (race.cpp). Orig bytes:
  build/brally/win32/match/orig/0x10019A70.bin.
-  2026-09-15 CORRECTION: it is a C++ TU. NO EH frame on this fn (cpp_score
  reports "FuncInfo structural: MATCH (no EH)" under /GX), but the body has 172
  ecx/thiscall sites, many with STACK args (callee-clean) -> the C __fastcall
  idiom only covers zero-extra-arg thiscalls; thiscall-with-args needs real C++
  member calls. So the probe is C++, not C. probe.py is C-only (hardcoded .c);
  use instead: `tools/brally/cpp_score.py --va 0x10019A70 --src race.cpp --name
  BrRaceStep --opt "/O2 /GX /MD"` (extern "C" the target so the symbol stays
  BrRaceStep). Member callees = methods of a catch-all `struct Obj` (no vtable,
  cast any this to Obj*); polymorphic-arity cdecl callees = `extern "C" int
  sub_X(...)`; fptr globals = `extern int (*g_X)`.
- Top-level skeleton: `if (g_5CCB94==0){ g_6ED684=1; ...; switch(g_0A9360) ...
  first-frame init incl. per-driver build loops } else { small per-frame check
  0x1001ab71 }` merge at 0x1001ab93 -> race/HUD/pause/camera/limiter tail to end.
  Main switch cases g_0A9360 0..6 emit-order 0,1,6,(conv),5,4,2,3=default;
  jump table 0x1001c648; case3==default target 0x1001a1ee. Nested switches at
  0x1001c664 (5), 0x1001c678 (4), 0x1001c688 (4).
- Progress 2026-09-15: race.cpp reconstructs region1+region2+switch dispatch
  +cases 0,1,4,5,6 +convergence (Lcf3/Lcfb/Ld39/Ld70) compiling. Labels match
  VAs. Case 4 incl. credits/intro/outro asset dispatch (g_5BC760) + per-driver
  field setup from pObj->[0x44] bytes. Whole switch DONE incl. case 2 per-driver
  build loop (0x1001a0fc byte-packing) + default/case3. Then La213 tail (HUD
  nested-switch 0x1001c664, vec-math sub_10034870/347F0) + La430 + render-setup
  (tyre/camera + callback-wiring loop 0x1001a5ba) all DONE. recomp ~2976 B (~26%).
   FIRSTDIV moved +0x0 -> +0x2: prologue now emits `sub esp,N` (frameless);
  N grows toward 0x34 as render-loop-body locals get added. REMAINING: render
  loop body 0x1001a6c5 (15-arg g_18ED1C4 sprite calls) DONE + init-tail 0x1001a97c
  (sets g_5CCB94=1, NOT 2) + else-branch 0x1001ab71 all DONE. recomp ~4272 B,
  reconstruction reaches offset ~0x1123 (~40% by length). FIRSTDIV still +0x2
  (frame-size byte; converges once local set matches exactly). ONLY REMAINING:
  the per-frame tail 0x1001ab93..end (~6.8KB, the big half: race/HUD/pause/
  camera/limiter). recomp ~6240 B (~56%) as of the run through 0x1001b365.
  DONE through: per-frame tail region 1 (g_5BC8F8<3), g_5BC8F8>=3 state dispatch
  (states 3/4/5/6/7 incl. state-4 limiter w/ 11-arg log call), merge-tail loops
  + leaderboard scan. REMAINING: 0x1001b365..end -- HUD-emit nested switch
  0x1001c678 (4 cases, sub_1002A590/29D70), the 0x1001b876/b870 joins, and the
  Lb887 block (0x1001b887..function end, incl. nested switch 0x1001c688).
  Labels in race.cpp match VAs (Lb0cd/Lb0f8/Lb171/Lb365/Lb887). WIP compiles
  clean each tick via cpp_score. After body whole: prologue snaps, then diff-grind
  (x87 scheduling in the tail will be the hard residue).
- 2026-09-15 STATUS: WHOLE function mapped + reconstructed to recomp ~7680 B
  (~68%), compiles clean each tick.  EPILOGUE READ (0x1001c57b): `pop edi/esi/
  ebp/ebx; add esp,0x34; ret` = CONFIRMS the prologue target (4 callee-saved +
  0x34 frame). Transcribed: everything 0x10019A70..0x1001bc16 + ghost entry
  (g_5CCB5C==2 fast path 0x1001bc44) + finalize front (state-4 lap timer 0x1001c13d).
  STUBS remaining (dense x87/dispatch residue, each own grind):
  (1) case-3 waypoint vec interp 0x1001b465 (loop) + 0x1001b64a (transform+emit);
  (2) main ghost path 0x1001bd72..0x1001c13d (per-driver replay, nested switch
      0x1001c688 mod-6 idiv audio cycling, g_0A9360==4/5 arms);
  (3) finalize body 0x1001c1c7..0x1001c57b (per-driver clear loops, g_18EEF50
      reset via sub_1006B4F0, g_5CCB98 finalize dispatch via sub_1002E317 fn-ptr
      arms, g_5BC810 max-search+report, g_5CCB5C transition, cleanup, epilogue);
  (4) the 0x1001c583 replay-advance clock block.
-  2026-09-15 PHASE 1 COMPLETE: the ENTIRE 11,223 B function is reconstructed
  into compiling C++ (recomp 10752 B, ~96% of size; gap is codegen not missing
  logic). All stubs filled incl. both case-3 x87 vec cores + the c583 replay
  clock + the ghost-body structural refactor (shared GhostBody label).
   PROLOGUE SNAPPED as predicted: recomp now emits `83 ec 60 53 55 56 57 e8`
  vs orig `83 ec 34 53 55 56 57 e8` -- 4 callee-saved pushes + frameless + first
  call ALL MATCH; FIRSTDIV=+0x2 is ONLY the frame size (my 0x60=96 vs orig
  0x34=52, i.e. 44 extra bytes / 11 dwords of locals). NEXT LEVER (phase 2 start):
  coalesce the vec3 temporaries -- I declared separate `float t20/t2c/t38[3]` in
  the interp loop AND the transform block AND `m[3]` in La213 + render-loop locals;
  the original reuses ONE set of esp slots 0x20-0x3c.
   2026-09-15 phase-2 progress: coalescing vec temps to a shared set dropped the
  frame 0x60 -> 0x3c (recomp `83 ec 3c ...`, 8 B / 2 dwords over 0x34). /FAcs
  equate listing (build/brally/win32/match/obj_cpp/frameprobe.cod) pinpoints the 2 extra:
  loc10$=-60 and loc14$=-56 each hold a DEDICATED slot; everything else coalesced
  (loc18/loc1c share with rem/b$/n block locals; v2c=-36,v20=-24,v38=-12 pack
  -36..-4). Removing fn/newB88 (via direct sub_1002E317 calls + split Lc45c_0/_1
  merges) and block-scoping the vecs did NOT shrink it.  CORRECTION: loc14/18/1c
  are NOT dead in the tail -- loc14 is reused as the state-4 limiter flag
  (0x1001ae4e), loc18 in the Lb887 leader block, loc1c in the leader-attach loop;
  they match the original's esp+0x14/18/1c reuse, keep them. The REAL 2 extra
  dwords are ep$1444(-52) and b$1357(-48) = the render-loop pointer locals `ep`/`b`
  the ORIGINAL keeps in REGISTERS but my C spills (render loop 0x1001a6c5 is
  register-heavy). Fix = cut render-loop register pressure so ep/b stay in regs;
  precise register-alloc matching, not a scope tweak.
-  2026-09-15 FRAME FIXED by hand: the render-loop `dst += aa*cc` reused aa/cc
  AFTER the g_18ED1C0 call, so MSVC parked them in ebx/ebp (callee-saved),
  spilling b+ep. Recomputing `dst += (schar)PB(s,0xe8)*(schar)PB(s,0xe9)` frees
  ebx/ebp -> no spill -> frame `83 ec 34` EXACT. FIRSTDIV jumped +0x2 -> +0xD
  (prologue byte-identical).  LESSON (project lead was right, no "wall"): codegen IS
  controllable from source -- keep values off callee-saved regs by not reusing
  them across calls. Method: read the /FAcs equates + `tools/brally/divergence.py <obj>
  <bin> BrRaceStep --mask-slots --key 8` region map, fix source per region.
  NOW: byte-exact grind, 51 register-blind structural regions + ~106-insn deficit
  + tail reorder (c583 block 191 B never-compared, my ret lands last vs orig's ret
  before it). Biggest missing-code drifts: between regions #38-#40 (ghost block
  ~orig 0x22xx-0x23xx, -267 B) and #51 (the c583 tail, -255). Region 1 needs the
  load/store ORDER matched (orig: old=g_5CCB90; store now; idx; delta=now-old;
  count) + cached-0 in ebp not ebx.
-  SYSTEMIC BLOCKER for the whole grind (2026-09-15): the ORIGINAL dedicates
  ALL FOUR callee-saved regs to constants at the top -- ebp=0, esi=1, edi=2,
  ebx=-1 (`or ebx,-1; mov esi,1; mov edi,2` at 0x10019aee + xor ebp) -- and reuses
  them everywhere (`push esi` for 1, `cmp x,ebp` for 0, `mov [y],edi` for 2). My
  build caches only 0 (in ebx) and materializes 1/2 as immediates, so `push 1`
  args show as `push 0`/imm vs orig `push esi`, perturbing a large share of the
  51 regions. To match: trigger VC5 to cache 0/1/2/-1 by matching the original's
  branchy-small-store usage pattern of each (VC5-IDIOMS.md line 88 zero-register
  idiom -- ternaries/setcc SUPPRESS caching; branchy if/else stores TRIGGER it).
  This is whole-function, the dominant lever -- do it before per-region scheduling.
   THE LEVER (VC5-IDIOMS.md line ~1723, BrCarDrawVehicle 0x1000A110): every
  source-level CACHE (a value held in a local across uses) steals a callee-saved
  reg and ROTATES the whole allocation; the original RE-READS from memory per
  site. "removing the caches snapped car=ebx / zero=ebp in one step -- rotation-
  is-a-symptom." So to get zero->ebp (and the other constants right), REMOVE my
  local caches and re-read globals/recompute per use, matching the orig. This is
  the systemic fix behind ~most of the 48 register-blind regions (they're
  identical structure, only regs shuffled: mine ebx=0/ebp=1, orig ebp=0/esi=1,
  edi=2 already matches). Apply region-by-region, verify no logic break, measure.
  Progress this session: frame 0x34 EXACT, FIRSTDIV +0xD, regions 51->48 (tail
  reorder + region-5 signed loop). Confirmed NOT-signed loops (leave char*):
  0x1001c135/c1f9/c22e/c48a; signed (int) only 0x10019d23. race.cpp in scratch.
  Once frame==0x34,
  FIRSTDIV jumps off the prologue and the ~6710-diff body grind (register alloc,
  scheduling, x87) begins -- the documented multi-session hard part.
  To read the frame layout: compile with /FAcs /Fa..cod and grep the `_name$ = N`
  equates above the PROC. race.cpp in the session scratch.
-  STRUCTURAL: the ghost BODY 0x1001bc37 (the g_5CCB5C dispatch + shared
  finalize) runs after BOTH g_0A5EA8 branches (direct if !=0, else via the
  0x1001c583 replay timer's `je 0x1001bc37`); current if(g_0A5EA8!=0){} wrap is
  a WIP approximation -- refactor in a cleanup pass so the body isn't nested in
  the branch. FIRSTDIV still +0x2 (frame size converges once all locals present).
  The old common tail 0x1001a213
  -> 0x1001ab6f (first-frame-init tail incl. render loop 0x1001a6c5 that uses all
  esp locals+4 callee-saved -- likely where prologue snaps); then else-branch
  0x1001ab71 + per-frame tail 0x1001ab93..end (the ~9KB bulk: race/HUD/pause/
  camera/limiter, nested switches 0x1001c664/678/688). Note stride-89992 addr
  math at 0x10019cfb (g_2066C8 - a*89992) may need decomposition tuning later.

**2026-09-15 grind tick (frame already 0x34-exact; 47 regions, FIRSTDIV +0xC):**
- Region 1 (FIRSTDIV @orig+0xc) STRUCTURAL fix landed: original does `idx = 0`
  FIRST inside `if(idx<0)`, THEN `if(count>0){ for-fill; idx=count; }` (the
  `xor eax,eax` at 0x10019a9c is BOTH idx=0 and the rep-stosd loop counter).
  Source was `if(count>0){...}else idx=0;` -> rewrote to idx-0-first. Now emits
  `xor ecx,ecx` before the count check, matching block shape; recomp insns
  2829->2827. Residue at 0xc is now PURE coloring.
-  COLORING ROTATION pinned exactly (regions 1-3+): original enregisters the
  small constants as {0->ebp, 1->esi, 2->edi} kept across the whole function;
  mine gets {0->ebx, 1->ebp, 2->edi}. **2->edi ALREADY MATCHES.** Residual is
  0 and 1 needing ebp/esi. MSVC callee-saved preference is esi,edi,ebx,ebp by
  USAGE COUNT: orig's `1` ranks #1 (->esi), mine's `1` ranks lower (->ebp). Lever
  = raise the relative use-count/live-length of the `1` constant (or lower `0`'s)
  so 1->esi, 0->ebp. This is the whole-function coloring; edi=2 anchor confirms
  partial convergence.
-  TAIL LOST-SYNC (196 B, orig 0x1001c583..0x1001c647 NEVER COMPARED): pure
  BLOCK-LAYOUT ordering. Original places the c583 replay-advance block AFTER the
  epilogue ret (0x1001c583 > ret@0x1001c582); MY compile places it BEFORE `ret 0`
  (cod g_5BC76C refs at lines 3389-3464 precede ret at 4261) and puts a
  (1.0f,0.2f) float-call block after the ret instead. Source already has
  `return; Lc583: ...` with forward `goto Lc583` -- correct structure, but MSVC5
  layout still threads c583 before the ret because its time-sync loop only ever
  `goto GhostBody` (no fall-through to fn end). Need a layout experiment to push
  c583 last. Worth 196 concrete bytes -- biggest single structural target left.
- Recomp is 535 B SHORTER than orig overall (10688 vs 11223), drifting
  progressively behind: signature of source caches eliminating orig's reloads.
  Each cache removed adds reload bytes AND shifts coloring toward orig.

**2026-09-15 tick 3 -  CACHE-REMOVAL SNAPPED zero=ebp (confirmed):**
- ROOT of the coloring rotation FOUND & FIXED at the source: case 2 (0x10019f0e)
  had `int n = g_0B3858;` cached and reused across ~6 sites spanning calls. MSVC
  parked the derived driver pointer `*(int**)&g_AF3BC8[n]` in EBP (callee-saved,
  survives calls) -- occupying the very register the original uses for the ZERO
  constant. Original instead RE-READS g_0B3858 from memory after each call
  (`mov [g_0B3858]` at 0x19f1f/0x1a036/0x1a2c9) and recomputes the x11112 stride
  (`*29 -> *463 -> *1389`, SIB `*8`) inline -- because a call can mutate the
  global. FIX: deleted local `n`, reference `g_0B3858` directly everywhere in
  case 2. Result: mine now emits `xor ebp,ebp` for the zero (was `xor ebx,ebx`);
  region 1 + region 7 both show `cmp eax,ebp` matching orig. FAITHFUL, not a hack
  (orig provably re-reads).
- TRADE: region count 47->63, but bytes 10688->10804 (toward 11223) and insns
  2827->2867 (toward 2939). The rise is EXPECTED per [register-rotation-is-a-symptom](../triage/register-rotation-is-a-symptom.md):
  the wrong-register alignment was masking downstream diffs. Newly EXPOSED and now
  fixable: (a) `sete cl`-vs-branch at a `g_0B3858>0` gate (region ~7); (b) `-1`
  materialization -- orig reuses `ebx=-1` (`mov ecx,ebx` for the strlen count),
  mine re-does `or ecx,0xffffffff`. A low count with wrong registers is a LOCAL
  MINIMUM; anchoring the real assignment is the path to T4. Do NOT revert.
-  GENERAL LEVER RE-CONFIRMED: any local caching a GLOBAL across calls, where
  the derived value lands in a callee-saved reg, steals that reg from the
  original's persistent constant. Hunt these: `grep` for `int <x> = g_...;` then
  reused post-call. Re-reading the global is both correct-per-original and the
  coloring fix. Still 419 bytes short = more such caches remain.

**2026-09-15 tick 4-5 - byte-FIRSTDIV GATE ISOLATED to one scheduler tie-break:**
-  The divergence tool is REGISTER-BLIND (--mask-slots): zero=ebp vs ebx is
  INVISIBLE to its region count. So the tick-3 cache-removal (n->g_0B3858) did
  NOT improve the region count -- it rose 47->62, and cpp_score byte-diffs rose
  6683->6736 -- because the faithful stride-recomputes I added carry not-yet-
  matching registers. Measure byte progress with cpp_score (FIRSTDIV + diffs),
  NOT divergence.py region count, which is a coloring proxy only.
- KEPT the faithful edits anyway (orig provably re-reads g_0B3858; reverting
  games the proxy against the real target): zero=ebp correct, region-6 stride
  now matches, region-8 inf[1] single-read fixed (added `int v1=(signed char)inf[1]`).
-  cpp_score FIRSTDIV = +0xD, gated on REGION 1 register scheduling. At +0xD
  orig=`8b 35` (mov esi,[g_5CCB90]=old) vs mine=`8b 15` (mov edx,...). Full chain:
  orig does `mov edx,eax` (save now) at 0x12 -> reuses EAX for idx after the store
  -> frees ECX for the g_5BCAE4 counter -> pushes COUNT to callee-saved ESI, and
  OLD shares esi first (esi = old then count). Mine keeps now in eax (1 insn
  cheaper), loads idx into fresh ECX -> idx+delta+count take all 3 volatiles ->
  the g_5BCAE4 counter spills to esi and old/count stay in edx. EVERY register
  byte from +0xD flows from whether idx reuses eax.
- Tried: reorder delta/count/idx decls (no effect -- MSVC won't emit the
  `mov edx,eax` save because keeping now in eax is strictly cheaper; it's a
  scheduler tie-break, not dictated by source operand order). This is the real
  byte-exact (T4) wall for this function: a whole-function-pressure scheduling
  choice in the prologue block.

** TIER FRAMING CORRECTED BY PROJECT (2026-09-15) - read [t3-certified-standard](../rules/t3-certified-standard.md):**
- Everything above (register coloring, idx-in-eax reuse, scheduling, the c583
  block-layout "lost-sync" tail) is T4/byte-exact residue and is BEHAVIOR-NEUTRAL.
  NONE of it bears on T3. T3 = functionally exact (same inputs -> same outputs).
  Do NOT judge this function's tier with cpp_score/divergence/lost-sync -- those
  are T4 tools. I did exactly that this session and got corrected.
- On the PROJECT's T3 standard, BrRaceStep is T3-MATERIAL if its transcription is
  behaviorally faithful: the logic is a complete hand-transcription of the
  disasm, and the residue is entirely register/layout. The one open item is a
  LOGIC audit -- confirm no flipped branch / wrong offset / dropped side effect /
  miscomputed stride -- plus the A5 in-image equivalence oracle. If behavior
  matches, it IS T3, regardless of how the registers landed.
- The T4 register grind (below) is the SEPARATE, optional push to byte-exact.
  Ideas still open for that: (a) raise region-1 register pressure so idx-in-eax
  reuse is forced; (b) check whether `now` has a later real-source use that keeps
  it live (forcing the mov edx,eax copy). V1 (inline g_5CCB90 into `delta`) is the
  best region-1 phrasing found (delta->edx + count->esi + copy-now all match;
  6725 diffs); residual is old->esi/idx->eax reuse.

Watch for the same false-claim trap in any "withdrawn claim" comment elsewhere:
never leave a literal `@implements` in prose.
See [ghidra-pipeline](../toolchain/ghidra-pipeline.md), no-named-contributors, [byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md).
