# Diagnose dont hypothesize

*Recorded 2026-09-05.*

> On an exhaustively-mapped function, probing HYPOTHESISED levers yields ~0; MEASURING the original's own behaviour cracks walls. Same day, same functions: a 6-worker sweep of 6 novel levers returned zero wins, a byte-census broke a 12-session wall in one pass.

**The single cleanest A/B this project has produced on *method*, 2026-09-05,
on the three giant functions.** Both halves ran the same day against the same
three functions, with the same tools and the same dossiers.

- **Six workers, six novel dossier-endorsed levers, adversarial verify: ZERO
  wins.** Every one was a *hypothesis* about how the source might be spelled
  (a 2-D array shape, a two-part-sum read order, hoisting constants into
  locals, per-channel statement pairs, a scalar storage class). Results:
  three WORSE, one DEAD, one INERT, one WORSE. ~424k tokens.
- **One slot write/read census: broke 0x1000A110's byte-lane wall**, which had
  stood ~12 sessions and ~300 recorded-dead compiles. It was a *measurement of
  the original*, not a guess about our source. Minutes, one probe to confirm.
  ([slot-census-screen](slot-census-screen.md), commit fd34818.)

**Why:** these functions already carry 27-plus passes and ~70 measured-dead
probes each. The space of plausible spellings has been swept. What has NOT
been swept is the space of *questions you can ask the original bytes*. A
hypothesis competes against hundreds of already-dead siblings; a measurement
returns a fact nobody has yet written down.

**How to apply - reach for a diagnostic BEFORE a probe:**
1. **Census the original's stack slots** - writes and reads, with counts. Count
   asymmetry locates a shared join; one slot with two source variables is a
   behaviour bug. ([slot-census-screen](slot-census-screen.md))
2. **Census an instruction that occurs once per unit of work** across both
   streams - proves whether a block is genuinely missing or the region map is
   lying. ([lost-sync-region-trap](../traps/lost-sync-region-trap.md))
3. **Census the calls** - argument sequence, not just multiset; cdecl pushes
   right-to-left so the argument list is recoverable exactly.
4. **Ask the solved corpus** what C produces a byte-run; a MISS is a result.
   ([corpus-query-tool](../corpus/corpus-query-tool.md))
5. **Byte-diff the originals against each other** by size class to find cause
   families. ([sibling-byte-diff-screen](sibling-byte-diff-screen.md))

Only after a diagnostic names a specific structural difference should a probe
be written - and then it is usually one probe, not a sweep.

**Corollary about fan-out:** parallel workers are excellent at *executing* a
diagnostic across many sites, and poor at *inventing* levers on ground this
well-trodden. Spend the fan-out on measurement, not on guesses. (Recording six
dead levers with evidence is still worth something - but it is bookkeeping,
not progress.)

Related: no-token-thrashing, [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md),
[register-rotation-is-a-symptom](register-rotation-is-a-symptom.md), [parked-is-not-walled](parked-is-not-walled.md).
