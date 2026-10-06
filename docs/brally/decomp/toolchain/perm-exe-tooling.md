# Perm exe tooling

*Recorded 2026-08-31.*

> tools/brally/perm_exe.py drives the deterministic permuter (permute.py) at in-scope EXE functions (orig_<exe> bins + /ML|/MT|/MD opts). permute.py's anneal is generic; only its seed/orig loaders were DLL-keyed.

## tools/brally/perm_exe.py - permuter for the EXE workstream (added 2026-08-31)

`tools/brally/perm.sh`/`perm_fleet.py` are wired to the DLL `report.csv`. `permute.py`'s
`anneal(va, src, func_name, orig_bytes, opts, iters, ...)` and `score_src` are
fully parameterized, so `perm_exe.py` drives them at EXE functions:

```
.venv/bin/python tools/brally/perm_exe.py --exe setvideo --va 0x00401150 \
    --name CHK_FGets --src build/setvideo_work/0x00401150.c \
    --secs 1800 --seed 5 --sfx _wk5
```

- orig bytes: `build/brally/win32/match/orig_<exe>/<VA>.bin`; opts: `/O2 <CRT>` +
  `/O2 /Oy- <CRT>` where CRT = /MD brally, /ML setvideo, /MT bossrally.
- `--sfx` decouples the compile-tag/worker suffix from the RNG `--seed` so you
  can replay a seed's RNG under a fresh tag WITHOUT colliding with a running
  worker (they share scratch .c + obj dir otherwise -> corrupt scores).
- On improvement it checkpoints `best_src` to
  `build/brally/analysis/ghidra_work/<VA><sfx>.permuted.c` (NOTE: ghidra_work, not
  <exe>_work). Grab the sub-N near-miss there and READ it -- the near-miss
  shows the intended source shape (this is how CHK_FGets fell, see
  [setvideo-exe-complete](../functions/setvideo-exe-complete.md)).

**Runs are NOT reproducible across invocations** even with the same seed: Wine
compile flakiness (a candidate that fails to compile in one run, succeeds in
another) diverges the accept/reject trajectory. Don't rely on deterministic
replay; instead run a fleet and read whatever `.permuted.c` checkpoints land.

**Sweet spot = small functions (<= ~300 B) with a few coloring/scheduling
diffs.** On a 2,144-B function (WinMain) it drifts WORSE than the seed -- too
large a search space. Match those by hand.
