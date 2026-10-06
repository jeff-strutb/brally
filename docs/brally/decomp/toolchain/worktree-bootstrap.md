# Worktree bootstrap

*Recorded 2026-09-03.*

> A fresh linked worktree (`git worktree add`) has NO toolchain, reference/brally/orig/ or build/brally/win32/match/orig -- symlink them from the main checkout, but never symlink build/ or build/brally/win32/match wholesale (6 tracked files live under build/brally/win32/match).

A linked git worktree starts with **none** of the
gitignored infrastructure, so `refcheck.py` says "no extracted reference bytes"
and every sweep fails. Bootstrap from the main checkout (M):

```sh
ln -s $M/orig $M/reference $M/.venv .          # capstone lives in .venv
ln -s $M/tools/toolchains/msvc5 $M/tools/toolchains/msvc42 $M/tools/toolchains/wine tools/toolchains/
for d in orig obj_O2 obj_Od obj_O2y obj_O2p obj_cpp; do
  ln -sfn $M/build/brally/win32/match/$d build/brally/win32/match/$d; done
for f in report.csv report_cpp.csv report_exe.csv \
         cpp_matches.csv exe_matches.csv learned_globals.csv; do
  ln -sfn $M/build/brally/win32/match/$f build/brally/win32/match/$f; done
```

**Do NOT `ln -s $M/build build`.** Six files under `build/brally/win32/match/` are tracked
in git despite the `build/` ignore rule (`float_worklist.csv`,
`portguard_worklist.csv`, `walls_log.csv`, `idioms_new/*.md`); replacing the
directory with a symlink shows all six as staged deletions. Symlink the
children instead, as above. `build/brally/win32/match` also holds ~26k `obj_*` lane dirs  - 
only `obj_O2`/`obj_Od` are read by `image_build.py`, and match_sweep writes the
four `/O2 /Od /O2y /O2p` variants plus `obj_cpp`.

The symlinks themselves show as untracked (`.gitignore` uses trailing-slash
patterns like `tools/toolchains/msvc5/`, which do not match a symlink). Never `git add -A`
in a worktree - add explicit paths.

Two consequences of sharing `build/brally/win32/match` with the main checkout, both real:
- `report*.csv` become live shared state. Another session writing them mid-task
  changes your join under you (a 95-row join became 98 within two minutes).
  Snapshot to the scratch before deriving anything from them.
- Running any sweep rewrites `config/brally/globals_learned.csv`, which IS tracked.
  That churn belongs to whatever lanes' objs are sitting in the shared dir, not
  to your change - `git checkout --` it before committing.

Related: [toolchain-self-contained](toolchain-self-contained.md), [counting-reconciliation](../traps/counting-reconciliation.md),
[sweep-is-incremental-now](sweep-is-incremental-now.md)
