# Glide is the reference

*Recorded 2026-08-22.*

> HARD RULE - BRGlide.dll is the reference binary, NOT BRD3D.dll. This error has been made and corrected twice.

**BRGlide.dll is the reference binary for this decomp. BRD3D.dll is NOT.**
Stated in README.md line 25 ("the mature target and the reference") and it is
part of the project definition. Check the reference binary BEFORE doing any
matching, scoring, or extraction work.

**Why:** Glide is the mature renderer. BRD3D statically links ~100 KB of CRT
that has to be identified and fenced off - dead weight the Glide build does not
carry. Pairing one binary's bytes with the other's function map disassembles the
wrong bytes at a right-looking address, which is worse than failing outright.

**How to apply:** Before trusting any matching number, verify what the corpus is
keyed to - do not assume, and do not read it off a tool default:

    python3 - <<'EOF'
    # compare an extracted reference .bin against both DLLs; the one that
    # matches is what the corpus is actually keyed to
    EOF

Tools take BR_REF / BR_MAP overrides; `tools/brally/dumpasm.py` was repointed at
BRGlide in commit d98f480.

**THIS HAS HAPPENED TWICE.** Commit d98f480 (2026-08-15) corrected the D3D
default and said explicitly it was contrary to the project's stated choice. The
matching pipeline introduced in a7eb7cd (2026-08-19) re-made the same mistake,
and the whole `build/brally/win32/match/orig` corpus plus `build/brally/win32/match/report.csv` are keyed
to D3D addresses. Confirmed 2026-08-22: 400 of 400 sampled reference .bin files
match BRD3D, 1 matches BRGlide.

**What that costs, measured, not assumed:** of 290 matched functions, 276 have a
Glide twin in `config/brally/shared.csv`; 53 are already byte-identical in Glide, 201
differ only in baked-in addresses (same code, different link), and 22 are
genuinely different code. The decompiled C transfers; the scoring layer is what
is D3D-keyed. `config/brally/shared.csv` maps d3d_va to glide_va for 2,697 functions,
so re-keying is mechanical.

See [goal-coverage-not-playability](goal-coverage-not-playability.md), [matching-progress](../log/matching-progress.md),
[verify-blockers-through-twins](../triage/verify-blockers-through-twins.md).
