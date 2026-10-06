# Collision response decomp state

*Recorded 2026-08-17.*

> Resume point for the OBB collision-response transcription (milestone 6) - what's done, the oracle tool, and exactly where the solver work is blocked.

# Collision-response decomp - COMPLETE (2026-08-17)

**ALL FOUR functions of the OBB collision RESPONSE unit are transcribed, oracle-verified,
golden-pinned, and mutation-tested - committed to main.** BrCrPlaneResolve (0x10067470,
baf1a09/d146195), BrCrImpulseSolve (0x10065C80, 871efec/93f71a3), BrCrContactKick
(0x10065980, c76e809/2b6611b), BrCrRespWalk (0x10067710, 3c85845). The x87emu fixes
(call/ret return-address; qword-as-double; +opcodes) are in 708d72e. The response is now
transcribed end to end; the remaining gap to "cars stop falling in a live race" is the
CALLER substep loop (0x10067D30, see br_collresp.h:30-52) that drives the walker per frame.

The notes below are the historical resume-state; kept for the reverse-engineering detail.

Goal (DONE): transcribe the OBB collision RESPONSE unit (why "cars fall through the world"),
the gate to milestone 6 (driving).

## DONE and verified (committed-quality, but UNCOMMITTED - nothing is git-committed)
- `port/src/driving/br_collrespsolve.c` + `.h` + `port/tests/test_br_collrespsolve.c`:
  **`0x10067470` `BrCrPlaneResolve`** - the contact-plane resolver. Fully transcribed,
  audited, oracle-verified over 6000 random cases, 3 mutations killed, no regressions.
  Two modes: mode!=2 uses `pEdgeN` directly; mode 2 builds a box-face normal from the
  triangle centroid's MIN-|component| axis (a tournament), signed by centroid.x (a
  preserved quirk). Shared tail: `out = (planeD - dot(pA,V))*pA`.
- `tools/brally/x87emu.py`: a validated general x86/x87 emulator (byte memory, full flags,
  SIB, call/ret, `_ftol` intrinsic, loops, IEEE division). Self-checks against
  `BrMat3Solve` (0x1006DE70), `0x10067470` golden vectors, and looping `BrMat4MulVec3`.
  **This is the verification oracle for all remaining dense-x87 work.**
- `slice3_44.c`: added `@implements 0x10074C10/0x1006DE70` tags to `BrMat3Solve`
  (it was already implemented, just untagged - manifest was undercounting).
- `README.md`: corrected the stale "entry point is not ported" section (RallyMain +
  main loop + window + wndproc + DxDetect ARE ported and test-green; 0x1001D8A0 is
  `BrDxDetect`, NOT an arg parser). Fixed the duplicate stale claim too.

## Unit progress (Glide addrs)
- `0x10067470` BrCrPlaneResolve - DONE (prior session), committed baf1a09.
- `0x10065C80` BrCrImpulseSolve - **DONE 2026-08-17**, transcribed + oracle-verified +
  golden-pinned + all mutations killed. In br_collrespsolve.c/.h, test_br_collrespsolve.c.
  tests/brally/deps/test_br_collrespsolve.deps = just `slice3_44` (test self-stubs slice3_44.o's 4
  rigid-body refs - BrStub8B80_1p/BrGbiCall10075330/BrVec4Normalise/BrMat4MulVec3Transposed  - 
  like test_slice3_44.c does; do NOT link slice3_42/collresp/phys, they drag g_pBrCollGridCount etc).
- `0x10065980` BrCrContactKick - **DONE 2026-08-17**, transcribed + verified (6000 cases,
  worst rel 2e-6) + golden-pinned (9 cases) + all mutations killed. The impulse-FREE branch:
  reflects vel off the normal with restitution 1.05, effect record (thr>=10, NOT >10), 0.9
  damp (dampFlag arg3 and/or the effect path), and a spin fold (spinFlag arg4) =
  M·diag(Mt·N)·Mt·angVel where M=[N; (NxNy-Nz², NyNz-Nx², NxNz-Ny²); N×row1]. Colour source
  is g_brCrPlane.normal (the bank), NOT arg2. Helpers 0x1006d980=BrMat4MulVec3,
  0x1006d9d0=BrMat4MulVec3Transposed (inlined in the port).
- `0x10067710` (1301 B) - **THE LAST ONE**, IN PROGRESS. Harness CRACKED (2026-08-17). It
  orchestrates the broad phase + BrCrPlaneResolve + BrCrImpulseSolve + BrCrContactKick.
  - **Why the prior session couldn't make a contact pass: a SECOND x87emu bug**, not geometry.
    The real box-classify (0x10066260) uses `fcom qword ptr` DOUBLE constants (0.5/-0.5 at
    0x10077B48/B50). The emulator stripped the size qualifier and read every mem operand as
    f32 - so the low dword of double 0.5 = 0.0, making the classify compare against 0.0 and
    reject EVERYTHING. Fixed in x87emu.py: `qword` operands now read/write as f64 (rd_f8/wr_f8).
    Also added missing opcodes reached in the response closure: fsqrt, neg, sbb, adc, cdq,
    idiv/div, imul, shl/sal/shr/sar, not. None of the 3 committed functions use qword or these,
    so their golden vectors are unaffected (verified).
  - **Working harness** `walk.py`: body image @0x20000000 (mass/invInertia/orient/vel@164/
    angVel@180/extents@1dc-1e8/threshold@200), a contact list at g_pBrCollRespList(0x11778198)
    = one node{pPlane,pNext=0} → BrCollPlane{normal@0, d@0xc, pV0/1/2@0x10/14/18}, box matrix
    (arg2). Walker args: arg1=body@[STK+4], arg2=matbox@[STK+8].
  - **A contact that fires the full chain**: triangle v0=(-1,0,0.3) v1=(1,0,0.3) v2=(0,0,-0.8),
    identity box → CrExact → PlaneResolve → ImpulseSolver, vel.y -5→+0.224 (a real bounce).
    NOTE: a triangle with a vertex fully inside [-0.5,0.5]^3 takes BoxClassify's return-1 fast
    path and SKIPS CrExact, so 0x1006d4b0 reads uninitialized contact geometry (=0) and the
    walker bails - you need an INCONCLUSIVE triangle (crosses the cube, no vertex inside) so
    CrExact runs and produces real contact data.
  - Dispatch: after PlaneResolve, `cmp g_117787FC,1; je kick else solver` (modeFC==1→Kick,
    else→ImpulseSolver).
  - **BIG SCOPE FINDING: the walker is almost entirely WIRING of already-ported functions.**
    Every callee is ported (shared.csv Glide→D3D): 0x1006da20=BrMat4TransformPoint,
    0x1006d4b0=BrVec3Normalise(0x10074250), 0x1006d530=BrRbQuatDerivative(0x100742D0),
    0x1006d6b0=BrRbBuildMatrix(0x10074450), 0x10066aa0=BrCrTest, 0x10067470/0x10065c80/0x10065980
    = the 3 response fns. **0x10008D60 is a ONE-BYTE bare `ret` stub** (all the "trace" calls are
    no-ops - drop them). br_collresp.h:36-52 already documents the caller substep loop and the
    post-response `BrRbQuatDerivative(next); BrRbBuildMatrix(&body->m, next)`. The ONLY genuinely
    new dense code is the plane-selection block 0x10067852-0x10067A5A (computes g_brCrPlane.normal
    bank + modeFC + the PlaneResolve args from the contact normal + box axes + extents).
    g_0A9360(=1 retail, config) gates a mode-2 branch (rare). Remaining: decode/transcribe the
    plane-selection block, wire the rest to ported fns, verify C-vs-emulator, golden, mutation.
    LP64 note: walker reads next-state at body+0x158 (=car.next, a sibling BrRbState, NOT inside
    BrRbBodyFull) - pass next explicitly, don't offset from body.

## Solver `0x10065C80` - fully reverse-engineered, key facts:
- **5 args** (from walker call site 0x10067AAD): arg1=body(esi), arg2=&normal
  (=&g_117787F0, the shared normal `BrCrPlaneResolve` writes), arg3=g_117819C,
  arg4=edi(flag), arg5=0. cleanup `add esp,0x14`.
- Body field map (body = a BrRbBodyFull; LP64 rule → pass sub-objects, don't use offsets):
  `+0x2c`=mass, `+0x54`=invInertia(BrMat3), `+0xbc`=orientation m(BrMat4),
  `+0x164`=`next.vel` (next = BrRbState @ body+0x158, per br_carphys.h), `+0x180`=`next.angVel`,
  `+0x1ec/1f0/1f4`=effect color dwords, `+0x1fc/1ff/0x200`=effect bytes (intensity/clamp/threshold).
- **Algorithm - VERIFIED via the fixed emulator, exact helper dataflow (10 calls):**
  1. `BrMat4ToMat3Both(m)` → `Rt` (transpose), `R` (straight 3x3 of orientation).
  2. `nb = BrMat3MulVec3(Rt, N)` - normal rotated into body frame; **nb is the lever arm r**
     (this engine uses the normal direction as the contact arm, not a contact point).
  3. **Gate (corrected - prior note was wrong):** `vc = next.vel + cross(next.angVel, nb)`
     (contact-point velocity; verified numerically). `if (dot(vc, arg3) >= 0.0f) return 0;`
     - spelled `>=` so NaN continues, matching `fcomp g_077A78(=0)`/`test ah,1`/`jne`.
     `vc` is stashed to the fr-0x138 rhs slot (later overwritten).
  4. `skew = BrMat3Skew(nb)`.
  5. `Wworld = Rt·invInertia·R` via three `BrMat3Mul` (W·R, then Rt·(W·R)); then
     `WSS = skew·Wworld·skew` (two more `BrMat3Mul`). **Note: 4 Mat3Mul total** (not the
     count in the old note); helper order is Skew, Mul, Mul, Mul, Mul.
  6. `D = diag(1/mass)` built inline (identity loop @0x10065E1F then diagonal ← g_077A7C(1.0)/mass).
     `K = BrMat3Sub(D, WSS)` - ONE Mat3Sub (0x1006DD80 = packed 3x3 sub, per br_collresp.h).
  7. Effect-intensity block (always runs): a dot with arg3 → clamp to [.,27] (g_077AB8) →
     `_ftol` → byte[+0x1fc]; effect color[+0x1ec/1f0/1f4] = the 3 normal dwords copied verbatim.
  8. `if (byte[+0x200] > 10)` → effect-velocity/damping path (g_077B38=0.9 damping, g_0B5170=1.0,
     intensity byte[+0x1ff] = max(old, ftol(128 + g_077B30(-4.7037)*intensity))). **This block is a
     15-fxch juggle of 7 stack values with CROSS-BLOCK x87 stack deps - decode via emulator dumps,
     never by hand.** arg5(=0) gates it too (`[esp+0x160]<g_077B3C(1e-4)`, always true since arg5=0).
  9. Combine → `J = BrMat3Solve(K, rhs)`; apply. `rhs` for the Solve = the big combined vector
     built @0x10066025-0xaa (NOT the raw normal - fr-0x138 is reused). `flag`(arg4) @0x10066047
     selects whether a term is ×g_077B40(0.2) or zeroed. Final apply @0x100661xx uses g_077B44
     (-1.05 = -(1+e), e=0.05) and `BrMat3MulVec3(Wworld, rhs)` for the angular update.
- **Verification harness** (scratch, regenerable): `run_solver.py` (sweep), `trace2.py`
  (helper-call dataflow, hook at helper ENTRY addr not the `call` - run intercepts call before
  step), `dump_inter.py` / `track_rhs.py` (intermediate slot dumps). Use `Machine` + preload all
  absolute DLL globals. **Recommended next step: build a Python reference model, diff block-by-block
  against the emulator until it matches over ~2000 random cases (both byte<=10 and >10 paths), THEN
  transcribe to C, generate golden vectors, pin + mutation-test - same discipline as BrCrPlaneResolve.**

## THE BLOCKER - DIAGNOSED AND FIXED 2026-08-17 (was a misdiagnosis):
The previous note claimed the solver was "not verifiable in isolation" because r/rhs
needed the walker's per-contact geometry, and that synthetic inputs gave r=0/rhs=0. **That
was WRONG.** The real cause was a bug in `tools/brally/x87emu.py`: `call`/`ret` used a side
`callstack` and never pushed/popped a return address on the memory stack, so `esp` was 4
bytes too high inside every nested cdecl callee - each `[esp+N]` stack-arg read landed one
slot high ([esp+4]→arg2 instead of arg1). The solver's matrix helpers (BrMat4ToMat3Both,
BrMat3Skew, BrMat3Mul, BrMat3MulVec3, BrMat3Sub, BrMat3Solve) therefore received garbage,
producing a degenerate K and zero impulse - which *looked* like "walker geometry missing."
BrCrPlaneResolve (0x10067470) only escaped because it is a **leaf** (zero calls).

FIX (committed-quality, applied): x87emu `call` now does `esp-=4; wr_i(esp, retVA)`, and
`ret` does `esp+=4`. `_ftol` stays a no-esp intrinsic (push+pop net zero). Validated:
0x1006da90 (BrMat3MulVec3) and 0x1006de70 (BrMat3Solve) emulated standalone match
independent Python math over 900 checks each, 0 mismatches; BrCrPlaneResolve golden
vectors unaffected (leaf).

**The solver IS verifiable in isolation.** Harness `run_solver.py` builds a non-zero body
image (mass@+0x2c, invInertia BrMat3@+0x54, orientation BrMat4@+0xbc, next.vel@+0x164,
next.angVel@+0x180, effect byte@+0x200) + normal bank @0x117787F0 (arg2) + a vec3 arg3,
lays args at [esp+4..] (top-level entry gets no synthetic retaddr), preloads all absolute
DLL globals, and runs `Machine.run(0x10065C80)`. Gate passes ~50% of random inputs and the
impulse is large and non-zero (dvel/dang O(10)). Next: transcribe 0x10065C80 to C calling
the ported helpers, generate golden vectors, pin + mutation-test.

## scratch (EPHEMERAL - session-specific, will NOT survive to a new chat)
Was at the session scratch dir. Key regenerable pieces: `run_solver.py` harness,
`asm_<ADDR>.txt` dumps (regenerate with `BR_REF=reference/brally/orig/BRGlide.dll .venv/bin/python
tools/brally/dumpasm.py 0x<ADDR>`), `walker_funcs.txt` (the 29-func closure), `allconsts.txt`.
`tools/brally/x87emu.py` IS in-tree and persists - use `load_many(*asm_files)` + `Machine`.

## Honest ROI note for whoever resumes
I spent ~3h and produced 1 verified function + the oracle + doc fixes. The remaining
solver is blocked on the collision-harness-construction problem above. Reasonable
alternatives if that stays hard: (a) crack the broad-phase contact construction (or use
real testdata contacts), then transcribe+verify the whole unit end-to-end against the
oracle walker; or (b) redirect to independently-verifiable pure functions for cleaner
coverage gains. See [readme-status-was-stale](../decomp/traps/readme-status-was-stale.md).
