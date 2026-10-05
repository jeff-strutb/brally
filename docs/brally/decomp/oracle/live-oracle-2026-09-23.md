# Live oracle

*Recorded 2026-09-23.*

> The live A/B oracle (brbox + t3live) replaced the synthetic-seed A5; how it runs, its levers, and the defect classes it found

2026-09-23: A5 is now the LIVE oracle. The original BRGlide.dll runs headless in `tools/brbox.py` (Unicorn), driven by scripts in `tools/brbox_scripts/`. `tools/t3live.py` runs the original body and the placed T3 body (from `image_build_t3.py --out-dir build/brbox/image`) from identical state at real calls, and compares memory, imports, esp, callee-saved registers, x87 state, and live eax/edx. The ledger is `config/t3_live.csv`; `t3.py` Gate A passes only on EQUIVALENT. t3b_verify, oracle_profiles and t3image_verify were deleted.

Coverage levers built:
- `files NAME` fixtures (saves, BossRally.ini switches: PlayMusic, Interpolate, RunBenchmark).
- `peer SCRIPT`: two games on a virtual network. Conservative virtual-time sync with 20 ms latency; sends are published only outside sub-runs.
- `joystick ffb`: a force-feedback wheel.
- An MCI CD-audio drive.
- Cheats are typed over the main-menu CREDITS item.

Machine-model fixes the multiplayer race forced:
- Sleep blocks only the calling thread.
- 10 ms time-slice preemption.
- A DirectPlay call costs a clock tick.

Undefined bytes (the original reading uninitialised stack) are detected by perturbing callee-saved registers at each body call, and masked.

Defect classes found and fixed (all real source bugs):
- swapped arguments or operands;
- float rounding points (model x87 registers with `double`; force stores through the value's `int` image);
- `&local` used as an array;
- int→float where the original copies dwords;
- the wrong compiler variant changing frame depth (cpp_sweep now ranks frame shape first);
- lockstep reloc rows: wrong object (twin), non-address counterparts, D3D-era address-bearing names.

Status at end of 2026-09-23: the ledger has 169/175 T3 EQUIVALENT, 0 DIVERGENT, 6 UNCOVERED.
- BrCdAudioTick needs a network deadline to expire.
- BrPodWriteOpen/Close and BrUiTweenStep are vtable slots nothing in play calls.
- BrDlsTileRectE4 is a display-list command the Glide build never emits.
- BrTexLerpU8 is used only for 8-bit alpha/intensity textures.

BrVertLerp8 became T4 (the residue was a missing return value). There are 22 scripts, including ini fixtures (PlayMusic, D3DDrawCarShadow=0, RunBenchmark), `tmu N` (low texture memory), video modes, and cheats → outro.

**Why:** the project lead's plan (Phase 0 - 5) demanded real-behaviour certification. The synthetic seeds had passed functions with real bugs (e.g. the BrRaceStep arg swap).

**How to apply:** certify T3 only from the live ledger. Debug a divergence with `explain` plus `T3LIVE_SKIP`/`WATCH`/`PROBE`, per docs/MATCHING.md. Related: [t3-certified-standard](../rules/t3-certified-standard.md), [quickrace-crash-argswap-2026-09-22](quickrace-crash-argswap-2026-09-22.md).
