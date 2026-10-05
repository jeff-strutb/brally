# Cpp lane owns top c targets

*Recorded 2026-09-04.*

> triage.py ranked functions the C++ workstream owns and is still working as top C targets; fixed 2026-09-03 with a park-tier "C++ LANE OWNS THIS TU" verdict.

`tools/fnmatch/triage.py` filtered C++ rows whose status was `match`, but not
the ones the C++ lane owns and is **still working**. Those keep a status=diff
row in `report.csv`, so the C ranking scored them as fresh C work - and scored
them WELL, because a half-built twin is usually structurally complete.

Measured 2026-09-03: **10 of 210 ranked rows** were in this state, five offered
as ordinary C targets. The worst two:

- `0x1003AB00` ranked **reggap 3 - the second-best row on the whole board**  - 
  while `src/core/cpp/0x1003AB00.cpp` had it at 130 diff bytes.
- `0x100540D0` read "MISSING CODE (27% complete)" in C triage while the C++
  lane had it at **1 diff byte**.

**Why:** the playbook's own rule ("rank by reggap ascending, take the smallest")
sends a session straight into another lane's work. The manual screen
(`ls src/core/cpp/<VA>.cpp`) was documented but is one more thing to remember,
and the ranking was actively feeding the duplicate-claim hazard `claimcheck.py`
exists to catch.

**How to apply:** fixed in commit c202267 - presence of the `.cpp` TU is the
ownership signal, not its status; such rows now get park-tier verdict
`C++ LANE OWNS THIS TU`, printed with that lane's diff count so a closer can
see where the work stands. Still run the manual screen for anything reached
outside triage (T1 intake, the factored-helper screen), which this does not
cover.

See also [cpp-vcall-family-lode](cpp-vcall-family-lode.md), [cpp-twin-retire-chore](cpp-twin-retire-chore.md),
[divergence-class-triage](../triage/divergence-class-triage.md).
