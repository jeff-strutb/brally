# Lock functions before targeting

*Recorded 2026-09-06.*

> RULE - claim/lock functions via claim_lane.py before targeting, so parallel sessions don't collide

RULE (2026-09-06): when parallel sessions run the T4 lane on the same
working tree, you MUST lock functions before targeting them so another
session doesn't grab the same one. `tools/brally/t4lane.py` only PRINTS targets
(no lock) - two sessions both ran it and collided on br_vtxcache.c
(0x10019040) and br_netpkt.c (0x1006B080); the other session silently
rewrote both files mid-edit.

**Why:** a shared working tree has no isolation; without a lock two sessions
clobber each other's in-flight edits (see [parallel-session-clobber](../traps/parallel-session-clobber.md)).

**How to apply:** use `tools/brally/claim_lane.py claim <N>` - it prints a TOKEN
and N unclaimed diff functions and writes `status=claimed` rows into
`build/brally/win32/match/lane_claims.csv` (fcntl-locked, stale after 90 min). Work ONLY
your claimed set; `release <TOKEN> [wallVA...]` parks walls and frees the
rest at the end. Cross-check `lane_claims.csv` before touching any file, and
back off files another session is actively editing. Coordinate - don't fight
an unknown concurrent writer; ask the project lead if the source is unclear.
