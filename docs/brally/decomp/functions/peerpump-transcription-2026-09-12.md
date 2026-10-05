# Peerpump transcription

*Recorded 2026-09-12.*

> 2026-09-12 (3rd session): 0x1006AB80 BrNetPeerPump transcribed FRESH to 802/803 B / 248/248 insns T2 in new br_peerpump.c; the byte-slot 'carrier' list is mostly spent (peer sessions matched two, one was untranscribed); the index-form-vs-raw-pointer lesson and two new mutual-exclusion pairs.

**2026-09-12 third session: the plan was 'sweep the char-width-prototype
lever across the byte-slot carriers' - the honest finding is that list was
STALE within a day:** 0x10036220 matched by a peer (C++ lane), 0x10039620 /
0x10058AF0 claimed by the same peer, 0x10028BB0 parked on the id-spill wall
(not byte-width), 0x1000A110's byte wall is register-death timing (my lever
does not apply - it targets HELPER-ARGUMENT sites only). The real fresh find
was **0x1006AB80 - 803 B, NEVER TRANSCRIBED, invisible because it had no
report row** ([unswept-tu-bookkeeping-class](../traps/unswept-tu-bookkeeping-class.md)).

**Result: BrNetPeerPump, new TU src/core/net/br_peerpump.c (the
br_peerrank.c precedent: own TU so the peer table can be typed as records).
802/803 B, 248/248 insns, REGNORM 8+8, parked T2 with full dossier.**

**THE LESSON THAT PAID: INDEX-FORM SOURCE, NOT HAND-BIASED POINTERS.** The
first draft used br_peer.c's raw `int*` walkers with hex bounds - REGNORM
27+23, VC5 re-biased the walker arbitrarily and derived reg+reg addressing.
Rewriting every access as `g_aBrPeer71[i].field` / `g_aBr178FEF8[i][j].field`
(struct + index everywhere, VC5 strength-reduces to biased walking pointers
itself) went straight to 8+8 and won the prologue. A tail block reaching the
record through `BrPeerRec *p = &g_aBrPeer71[i]` fixed the induction-slot
init order (br_peerrank's 'pointer in the sorted loop' lever, reconfirmed).

**Two new mutual-exclusion pairs (park-grade, do not re-grind):**
1. dword-`and` + byte-compare (`and ecx,0x3f; cmp cl,2`) vs the cached
   ReleaseMutex import (`call edi`): EVERY spelling of an `m = x & 0x3f`
   local (function-, block-, nested-if-scoped) evicts the import cache.
   `(char)(x & 0x3f)` keeps the cache but narrows the and to `and cl`.
2. The walker bias: ours anchors +0x95C (disp8-optimal), the original
   +0x2C (the status word) - the SAME open question br_peerrank.c documents;
   still no source lever known.

**Also this session:** `tools/stale_claims.py --fix` is the cpp-twin-retire
chore's real replacement - it converts C-tree tags for VAs matched in the
cpp lane (did 0x10007230 + a peer's 0x10036220), then re-sweep both files.
fileaudit now shows 'matched, never recorded: 6' - peers' in-flight filing,
left alone per [filing-py-drops-rows](../traps/filing-py-drops-rows.md). Tree T3 count 104 and climbing from
peer sessions; never quote, re-derive.

Related: [carstate-decode-cpp-2026-09-12](../cpp-lane/carstate-decode-cpp-2026-09-12.md), [byte-slot-idiom-cracked](../levers/byte-slot-idiom-cracked.md),
[lock-functions-before-targeting](../rules/lock-functions-before-targeting.md), [parked-is-not-walled](../triage/parked-is-not-walled.md).
