# Vc42 not brglide

*Recorded 2026-09-01.*

> VC++ 4.2 cross-check RUN and FAILED for BRGlide.dll - 5.0 byte-exact functions break under 4.2; keep MSVC 5.0 for the DLL

2026-08-31: Another session staged VC++ 4.2 (tools/toolchains/msvc42, cl 10.20.6166)
and reported it "breaks a wall cold" on one function, proposing a
project-wide compiler re-check. The decisive cross-check (do functions
already byte-exact under 5.0 still match under 4.2?) WAS RUN on
br_drawcar.c: **it fails**. All four byte-exact-under-5.0 functions in
that TU compile smaller and structurally different under 4.2
(BrGuMtxStore 39B vs 47B orig, BrCarDrawWheels 807B vs 932B,
BrCarVisibilityUpdate 311B vs 322B, BrCarDrawBody 1357B vs 1576B), and
BrCarDrawVehicle loses the interleaved-push prologue + ebp zero-reg that
5.0 reproduces exactly.

**Why:** BRGlide.dll is MSVC 5.0 - 851+ byte-exact matches prove it.
Whatever the 4.2 win was, it was on a DIFFERENT binary or TU (unstated in
the report; note [setvideo-exe-complete](../functions/setvideo-exe-complete.md) says SetVideo is proven
VC5 SP3 by 40 matches). 4.2 may matter for one of the other EXEs - check
per-binary via config/brally/binaries.csv, never assume project-wide.

**How to apply:** if a session proposes switching BRGlide work to
tools/toolchains/msvc42, point at this check. To re-verify: compile the TU with
`sh tools/toolchains/wine.sh tools/toolchains/msvc42/bin/CL.EXE` using fn.py's exact flag line
and divergence.py the known-exact functions first.
