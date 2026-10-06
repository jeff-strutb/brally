# Port stub vs body relative arm

*Recorded 2026-09-21.*

> The largest untiered function may be a port-only stub whose real ABI is body-relative; write the matching arm from the certified caller's cast typedef

**2026-09-21: BrCrImpulseSolve 0x10065C80 (1448 B, the largest available untiered
Glide fn) CERTIFIED T3.** It looked like a plain diff, but the committed
definition was a PORT-ONLY STUB with a flat 10-argument signature that could
NEVER match: the original is __body-relative__ - arg0 is the body block and it
reads mass (+0x2C), invInertia (+0x54), orient (+0xBC), vel (+0x164), angVel
(+0x180) and writes the effect record (+0x1EC..+0x200) all off it, colour from
the global normal bank g_brCrPlane (0x117787F0), not a pointer arg.

 REUSABLE PATTERN: when the recomp is much SMALLER than the orig and the calls
already line up, suspect a wrong ABI, not missing code. The __certified caller__
often already documents the true signature via a cast typedef in its own
matching arm - here BrCrRespWalk (0x10067710, already T3) had
`typedef int(*Fn)(char*, const BrVec3*, const BrCollPlane*, int, float)` and
called through it. Write the matching arm with THAT signature under
`#ifdef BR_MATCHING_BUILD`, keep the port body under `#else`.

The work was NOT a byte grind: getting the A5 oracle to RUN and say EQUIVALENT
is the win (500/500 seeds), and A5 EQUIVALENT supersedes byte gates A1-A4
([equivalence-oracle-in-image-2026-09-10](../oracle/equivalence-oracle-in-image-2026-09-10.md), [oracle-runs-orchestrators](../oracle/oracle-runs-orchestrators.md)).

t3b_verify UNCLASSIFIED reasons hit this session, each a quick fix:
- "signature not a plain cdecl of scalar/ptr args" = the parser greps ONE line;
  a multi-line prototype never closes its `(` on the name's line. Collapse the
  prototype to a single line (cosmetic, zero codegen change).
- "relocation names a symbol with no known address: _fabsf / _<static wrapper>"
  = an out-of-line call the original inlines. `fabsf(x)` where x<0 is proven =
  `-x` (orig `fchs`); a static ftol-byte wrapper = inline `(uint8_t)(int)x`
  (orig `call __ftol`, which the oracle knows). Fixing these was also byte-real.
- "_g_<global>: no known address" = add one row to `config/brally/globals_hand.csv`
  (symbol,addr,refs,corroborated,class,sources); base addr = the first field's
  address (the-annex learned CSV regen wipes hand rows - hand file only).

Gate B ledger: `crank.py 0x<VA> --budget 40` on the SINGLE NAMED VA is allowed
(the refusal is only for `--all --loop` / `--all --max-bytes>400`); run twice
for two zero-movement @t4-pass lines (census auto-yes when a divergence exists).
Gate 0 gotcha: the `WHAT IT DOES:` marker must be within 40 lines ABOVE the @t3
tag - the tag block itself can push a long dossier's marker out of range; add a
one-line WHAT IT DOES: just above the tag. See [t3-certified-standard](../rules/t3-certified-standard.md).
