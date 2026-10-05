# Capstone venv auditors

*Recorded 2026-08-17.*

> The capstone-based auditors (claimcheck/crossdiff/dumpasm/globals) need a venv; how to restore and run them.

Four of the validated auditors import `capstone` and die with
`ModuleNotFoundError: No module named 'capstone'` on a fresh checkout, because
the system python3 is PEP-668 externally-managed (`pip install` refused):

- `tools/claimcheck.py` - call-graph parity between original and port (the main
  auditor for an @implements claim)
- `tools/crossdiff.py`
- `tools/dumpasm.py`
- `tools/globals.py`

Restore once per environment with a repo-local venv (the `.gitignore` already
has a `.venv/` rule, so it will not be committed):

```
python3 -m venv .venv
.venv/bin/pip install capstone
```

Then run any capstone tool through it: `.venv/bin/python tools/claimcheck.py`.
The pre-dumped `asm/*.asm` (Glide) and `work/slice*/*.asm` (D3D) do NOT need
capstone - plain grep/sed over them works and is how most transcription reads
the bytes.

**GOTCHA - claimcheck reads `config/ported.csv`, NOT the `@implements` lines.**
`ported.csv` is GENERATED from the source `@implements` lines by
`tools/manifest.py --emit`, but nothing runs that automatically. So after adding
or moving any `@implements` line you MUST run `.venv/bin/python tools/manifest.py
--emit` and commit the regenerated `config/ported.csv`, or claimcheck silently
does not audit the new claim (and manifest.py --list disagrees with ported.csv).
Found 2026-08-17: last session's six collision-response claims
(BrCrRespWalk 0x10067710, BrCrImpulseSolve, BrCrContactKick, BrCrPlaneResolve,
BrMat3Solve x2) were in source but missing from ported.csv - claimcheck had
never checked them. Resynced in `4cc5345`.

Baseline `claimcheck` result (2026-08-17, after resync): **794 clean, 6 FLAGGED**. The 6 are
pre-existing delegation smells (original delegates, port inlined the callees),
NOT regressions: BrStrUpr 0x1007F240, BrUiNavMove 0x100603A0,
br_stricmp_1008C320, BrDlVtxRoutine 0x1001FD70, BrPeerFind 0x10036030,
BrUiSprClip 0x10001320. A new port that trips claimcheck is a real signal.

See [render-frontier-draining](../../port/render-frontier-draining.md).
