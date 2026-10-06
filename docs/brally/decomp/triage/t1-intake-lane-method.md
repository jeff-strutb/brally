# T1 intake lane method

*Recorded 2026-09-05.*

> How the T1 intake lane works (smallest drafts first, sbs.py side-by-side, six-probe budget) and where it stands; paste-ready spec is docs/T1-INTAKE-LANE.md in the repo.

**The lane that moves the count (2026-09-05): T1 drafts, SMALLEST FIRST, one
function at a time, +9 byte-exact in one session.** The paste-ready spec is
committed at `docs/T1-INTAKE-LANE.md` (screening rules, the loop, the levers,
the bookkeeping traps, and the next candidates with reasons).

**Why:** the tagged small-reggap pool is parked with thorough notes; a
100-250 B T1 leaf is size-exact on the first compile most of the time and
closes in 4-6 probes. Nine of eleven attempted closed.

**How to apply:**
- Screen ten at a time (C++ ownership, claims, EH prologue, odd address,
  x87/16-bit/byte-lane bodies rejected on sight).
- After every diff run `tools/brally/sbs.py <obj> <Name> <VA>` (now in `tools/brally/`,
  committed) -- the side-by-side dump finds WHERE; fn.py's EXTRA/MISSING
  summary misled twice. Match the symbol exactly: a `_port` twin can win a
  substring match.
- Budget six `fn.py --var` probes, then park with the dead list as T2 and
  commit it.
- Parked this lane: 0x100701B0 / 0x10070280 in br_sndload.c.
- Next: 0x10002460, 0x1005F580 (see the spec's tail).

Related: [resume-state](../log/resume-state.md), [filing-py-drops-rows](../traps/filing-py-drops-rows.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
