# Session attribution is the claims file

*Recorded 2026-09-10.*

> Every parallel session commits as the same git user, so blame cannot say which session owns a function - lane_claims.csv can.

Several parallel sessions work this tree at once and they all commit as the
same git identity. `git log --author`, `git blame` and "who touched this
file" therefore carry NO information about which session owns a function.

**How to attribute:** `build/brally/win32/match/lane_claims.csv` - which token held the
VA - plus the owning session's own commits. Nothing else.

**Why it matters:** on 2026-09-10 a peer session inferred from the commits
on a file that a failing `@t3` tag was mine, and asked me to fix it. It was
not mine; I had never opened the file. The inference was reasonable and
still worthless. Before accepting "your certification is broken" from a
peer, check the claims file - and before sending one, check it yourself.

The technical verdict is independent of ownership: a tag failing its gate
at its own certified numbers gets parked whoever wrote it. Correct the
attribution, then judge the residue on its merits.

Related: [parallel-session-clobber](../traps/parallel-session-clobber.md), [lock-functions-before-targeting](lock-functions-before-targeting.md),
the commit-every-match rule.
