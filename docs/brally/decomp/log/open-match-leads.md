# Open match leads

*Recorded 2026-08-21.*

> Two unverified match leads salvaged from the killed 2026-08-20 worker round: the 0x10031xxx frame-pointer cluster and BrUiLdPtr inlining.

**UNVERIFIED. Salvaged from the truncated final messages of workers killed by a
usage limit on 2026-08-20; none of their work survived to be checked.** Treat
each as a hypothesis to test, not a finding. See the commit-every-match rule.

**Lead 1 - the 0x10031xxx cluster keeps a frame pointer.** An worker working
slice2_16 / slice2_17 reported that every function it examined in the
`0x10031xxx` range shows `push ebp; mov ebp,esp` in the ORIGINAL, and was about
to test whether that cluster was built with different optimization flags.
VC5 `/O2` implies `/Oy` (frame-pointer omission), so a whole address range that
keeps `ebp` is a strong hint that its translation unit was compiled with
different flags - plausibly `/O2 /Oy-`, or `/O1`, or `/Ox` variants.

Worth testing because it is CHEAP and BROAD: `tools/match_sweep.py` currently
tries only two variants (`VARIANTS = [('O2','/O2'), ('Od','/Od')]`) and takes
the better per function. Adding a third variant would test the whole tree at
once rather than one function at a time. Do NOT assume it - many `/O2`
functions legitimately keep `ebp` when they need a frame. Verify on a handful
in the cluster first, then decide whether to add the variant.

**Lead 2 - BrUiLdPtr should be inlined.** An worker working slice2_23 reported
that a thiscall bridge (`__fastcall`, per [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md)) brought a
function to size-exact 40/40 against the original, and that the ONLY remaining
divergence was `BrUiLdPtr` being emitted as a real `call` where the original has
the body inline. That is the documented "port factored out what the original
inlines" class - write the body out at the call site. This one sounded one edit
away from a match.

Related: [divergence-class-triage](../triage/divergence-class-triage.md), [matching-progress](matching-progress.md),
[cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md).
