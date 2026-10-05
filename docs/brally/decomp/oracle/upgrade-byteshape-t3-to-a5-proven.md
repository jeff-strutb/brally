# Upgrade byteshape t3 to a5 proven

*Recorded 2026-09-16.*

> An already-certified byte-shape T3 (oracle UNCLASSIFIED) can be UPGRADED to behaviourally-proven (oracle EQUIVALENT) by making the A5 oracle run it; a whole frontier of the 126 old T3s.

**A T3 tagged before the A5-orchestrator capability carries `oracle UNCLASSIFIED`
(byte-shape gates A1-A4 certified the colouring residue).  It can be UPGRADED to
`oracle EQUIVALENT` -- behaviourally proven -- by getting the oracle to RUN it,
then re-`--qualify` and flip the tag's oracle field.**  Done 2026-09-16 for
BrSceneDlBuild 0x1000EAF0 (9,354 B, the largest fn; committed 28ebe65d):
UNCLASSIFIED -> EQUIVALENT on 32 seeds (return + ~148 global-write bytes +
dispatch agree, ~93% of insns run).  Numbers unchanged, so ONLY the tag's
`oracle` field changes; update the date + a prose line noting the upgrade.
This is a frontier: many of the ~126 pre-oracle byte-shape T3s are upgradable.

**How BrSceneDlBuild ran with almost no profile:** it is a scene-graph
orchestrator; seeding ONLY the live-object count g_0B2F04 (0x100B2F04) small
(1..3) and leaving every pointer null-safe (0) was enough -- the per-object
loops run on zeroed objects (same as BrRaceStep's zeroed scratch), bounded and
deterministic, EQUIVALENT.  A giant orchestrator can need a one-line profile.

** NEW oracle capability (t3b_env `_declared_va`, committed 28ebe65d):** a
hand-named callee that no map records now resolves from its SOURCE declaration
comment `Type Name(args);  /* 0x<VA> ... */` (scanned across src/, cached; the
VA must land in mapped .text or it is refused).  Unblocked BrGroundProbeZ +
BrMtxPoolAlloc here, and lifted FOUR other certified functions from UNCLASSIFIED
to EQUIVALENT in the same --certified sweep -- so this is a cheap, broad
upgrade lever, not a one-off.  See [t3-is-not-a-shortcut-for-fresh-transcriptions](t3-is-not-a-shortcut-for-fresh-transcriptions.md),
[oracle-runs-orchestrators](oracle-runs-orchestrators.md).

**Workflow to upgrade one:** `t3b_verify <VA> --name <Name>` -> read the
UNCLASSIFIED reason.  Unresolved callee -> `_declared_va` likely covers it now
(or add its VA declaration).  Runaway -> find the loop's count/gating global
(disasm the loop head), seed it small in a Profile, leave pointers null-safe.
DIFF only on recomp with matching global writes -> the `call reg` stdcall-leak
gotcha ([t3-is-not-a-shortcut-for-fresh-transcriptions](t3-is-not-a-shortcut-for-fresh-transcriptions.md)).  Then re-`--qualify`,
confirm `--certified` sweep DIFF 0, flip the tag, commit with pathspecs.
