# Pool refresh method

*Recorded 2026-09-10.*

> How to build an HONEST fresh candidate pool (the de-dup filter), and the 2026-09-10 finding that both intake pools are drained: every reachable small/close row is already parked with a dead-list, and NEW pool now comes only from the C++ thiscall lane, structural T2 big-gap rows, or cracking a compiler-decision class.

**The question "how do I increase my T3/T4 pool" has a settled answer as of
2026-09-10: you don't find pre-qualified rows - that pool is empty. You
either route a class to the C++ lane, do structural T2, or crack a
compiler-decision class.** ([t3-frontier-map-2026-09-10b](../log/t3-frontier-map-2026-09-10b.md),
[gate-a-distance-survey-2026-09-10](../log/gate-a-distance-survey-2026-09-10.md))

**Why both obvious pools mislead:**
- `t4lane.py --claim` (Pool B, smallest T1) is dry: every fresh small row is
  either CPP-lane (`src/brally/core/cpp/<VA>.cpp` scratch exists), already parked in a
  slice, or a byte-lane/x87 wall (t4lane annotates `N 16-bit ops` / `N fxch`).
- **`tiers.py --list T1` LIES**: it calls a VA "T1 not started" whenever the
  cpp scratch or draft isn't compiled into the project - even when that VA is
  already transcribed and PARKED with a full dead-list (e.g. 0x1006FCE0
  BrCarSlotSetup, parked at 12 diffs on the memset-expansion order). GREP THE
  TREE before believing "not started".
- **Ranking diff rows by diff-count / size-gap re-surfaces PARKED rows.** The
  top three "closest to byte-exact" on 2026-09-10 were all parked walls:
  0x10063CC0 BrReplaySeek (5 diffs, `jns` vs `cmp ecx,edi;jge` fold, 10 probes
  dead), 0x1006DD20 BrMat3Mul (19 diffs, anchor-vs-association, 6 term orders +
  temps dead), 0x10029D70 BrMat4Mul. All carry `@t4-pass` or a dead-list.

**THE DE-DUP FILTER (reusable - build the honest pool with it):**
1. certified set: `t3.py --vas | tr A-Z a-z | sed 's/^0x//' | sort -u`
2. from `build/brally/win32/match/report.csv`, take `status==diff` rows, drop certified,
   drop EH (orig first 2 bytes `6aff` via `xxd -l 2 build/brally/win32/match/orig/0x<VA>.bin`).
3. **drop rows that already carry a ledger**: `grep -rlE "@t4-pass 0x<VA>|@implements 0x<VA>" src/brally/`.
    Match BOTH the glide AND the d3d-twin address - most math/util rows are
   `@implements`-tagged at their d3d twin (BrMat3Mul tagged 0x10074AC0, diffs at
   glide 0x1006DD20), so a glide-only grep falsely calls them fresh.
4. what survives is the genuinely-untouched pool.

**What survived on 2026-09-10 (the real fresh pool) and why it's harder:**
- **BrFfb cluster** - 0x10072210 UpdateSpring, 0x100723D0 EnumDevice,
  0x100724C0 Init, 0x10072680 Setup (in slice3_45.c). Real force-feedback
  (DirectInput), NOT fenced; six siblings already byte-exact in br_ffb.c /
  br_dicmd.c. But the four diff rows have BIG size gaps (recomp >> orig) and
  100-280 diffs = structural T2 with COM/vtable traffic, not register noise.
  0x10072680 is O2y (fn.py is O2-only; trust the one-file sweep).
- The thiscall-shim bitstream family (0x10006BA0, 0x10007230, 0x10007750)  - 
  frontier class #1, **unreachable in C, route to the C++ lane.**

**So the three actual supplies of NEW pool, in value order:**
1. C++ lane for the thiscall/`xor edx,edx` family (biggest untapped block).
2. Crack ONE compiler-decision class from [t3-frontier-map-2026-09-10b](../log/t3-frontier-map-2026-09-10b.md)  - 
   several rows move at once.
3. Structural T2 on big-gap fresh rows (BrFfb cluster) - real missing code.

Do NOT re-probe the parked walls above; their dead-lists are current and
thorough (no-token-thrashing, [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)).

Related: [t1-intake-lane-method](t1-intake-lane-method.md), [unswept-tu-bookkeeping-class](../traps/unswept-tu-bookkeeping-class.md),
[cxx-thiscall-wall](../cpp-lane/cxx-thiscall-wall.md), [cpp-lane-owns-top-c-targets](../cpp-lane/cpp-lane-owns-top-c-targets.md).
