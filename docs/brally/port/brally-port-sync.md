# Brally port sync

*Recorded 2026-10-05.*

> 64-bit PC port (ports/brally) vs the 32-bit lane: sync.py reports, lockstep.sh state compare, globfold blocks; lessons from the 2026-10-05 session that wasted the project lead's time

ports/brally is a retyped fork of src/+include/ (stamp ports/brally/src/FORKED-FROM). Commits 2a216bf9 (2026-10-05) and the sync commit before it.

**Verify with `ports/brally/tools/lockstep.sh SCRIPT [EVERY]`**: one run of each build (core RENDER=null in build/portable_null, the 32-bit lane build/wasm/brally) with BR_DUMP_EVERY, then `dumpdiff.py --state` per dump. About 2-3 minutes per script; all 29 single-player scripts at 7 parallel take about 15 minutes. As of 862274da (FORKED-FROM 06002f42, 2026-10-05) ALL 29 single-player scripts are identical. **Build the reference lane from a clean HEAD worktree, not the shared tree:** another session's uncommitted config/globals_learned.csv (texture-scan globals) changed the main tree's lane and produced 13 false lockstep failures. Worktree needs symlinks for orig/, .venv, tools/msvc5/{bin,bin-sp3,include,lib} and a copy of build/match; run lockstep with LANE=<worktree>/build/wasm/brally. The globfold OWN key-table change was committed without a lockstep run.

**Why:** the project lead lost an hour while I bisected with a runner that kept playing each script to its END after the needed dump (16,000 frames for the championship). They also caught me launching runs on a build I had broken (a refold) and had not checked.

**How to apply:**
- Before any long verification, time ONE run and confirm the harness stops when it has its answer. Never replay from the start once per bisect step; dump every N frames in one pass.
- After any regeneration (globfold, datalift), run one quick lockstep on a known-good script BEFORE fanning out.
- Kill stale jobs before rebuilding a binary they use. Report progress without being asked.
- Bug classes found in the port: raw original offsets into records that hold pointers (search `(char *)&rec + 0x..`, `ptr)[0x..]`); local views with original offsets over 64-bit objects; arrays indexed at the wrong element size; the original's uninitialised stack reads (now zero via -ftrivial-auto-var-init=zero).
- **A T3 -> T4 re-transcription CAN change behaviour; sync.py's empty "signatures changed" does not mean nothing to carry.** Old T3 C++ bodies were placed in the image with hundreds of `lockstep` rows in config/reloc_overrides.csv that rebind call/global operands to the original's, so A7 passed while the SOURCE (and so the 32-bit lane and the port) called the wrong targets. 2026-10-05 sync b60299cb -> 8ac856b6: BrRaceStep's new T4 body exposed four real bugs the port carried (pause option 5 sets mode 2 not 1; select keys' A/B handlers swapped; leader search clears every entrant's every record; sound-offer loop indexes view[v] not view[0]). Method: diff old vs new decomp body for behaviour, confirm each difference in `tools/dumpasm.py VA`, carry by hand, rebuild BOTH builds (`sh ports/macos/wasm/build_wasm.sh`, the lane compiles src/), then lockstep.
- The 32-bit lane natively replaces drawing, CD and the timer, so its render, CD and texture state are not a reference. Several "differences" were bugs in that lane (the clock sentinel, message stamps, BrSndBufSetPan's eax return).

Related: [tgrally-port](../../tgrally/port/tgrally-port.md), [port-never-touches-decomp](../decomp/rules/port-never-touches-decomp.md).
