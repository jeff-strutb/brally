# Boss Rally: Standing rules

- [byte-exact-non-negotiable](byte-exact-non-negotiable.md): HARD RULE - byte-exact is the requirement; never propose the port pivot as an escape from a hard function.
- [do-not-lower-t3-standard](do-not-lower-t3-standard.md): RULE - never relax a T3 gate to hit a target number; asked whether to loosen A4, the answer was \"don't lower our standards\
- [exports-are-1-to-1](exports-are-1-to-1.md): RULE - every asset extractor is a faithful 1:1 rip that assumes nothing; playback decisions belong at playback, never baked into a file.
- [file-as-you-match](file-as-you-match.md): Move a function from its slice into the right named module when you match it - never a bulk reorg.
- [glide-is-the-reference](glide-is-the-reference.md): HARD RULE - BRGlide.dll is the reference binary, NOT BRD3D.dll. This error has been made and corrected twice.
- [goal-coverage-not-playability](goal-coverage-not-playability.md): THE governing objective - a complete MAME-standard bit-exact decomp via the most EFFICIENT path. Playability is only a side effect, NEVER a driver.
- [hand-transcription-only](hand-transcription-only.md): HARD RULE 2026-10-05: hand transcription ONLY on BOTH the N64 and PC decomps, residue included; no search batches, sweeps, permuters, spelling matrices or force-sweeps unless the project lead says otherwise
- [implements-requires-execution](implements-requires-execution.md): An @implements tag asserts 100%; never apply one to a body that has never been executed by a test. Cost a full revert of 0x1000A110.
- [lock-functions-before-targeting](lock-functions-before-targeting.md): RULE - claim/lock functions via claim_lane.py before targeting, so parallel sessions don't collide
- [matching-decomp-pivot](matching-decomp-pivot.md): Project pivoted from port-only to matching decomp + port (SM64 model). Matching build produces bit-identical Win32 DLL, port build targets macOS/Metal. Same source, two build targets.
- [port-never-touches-decomp](port-never-touches-decomp.md): RULE - port work (ports/macos, wasm lane) must NEVER edit decomp source (src/, include/, tests of it), even byte-neutral edits; all fixes go port-side (flags, port include dir, pre-includes, generated overlays in build/).
- [post-m1-nothing-closed](post-m1-nothing-closed.md): RULE 2026-10-03 - after M1 nothing is closed; giants, \"do not reopen\", named-only and park-don't-grind limits are all void for M2 (T3->T4)
- [session-attribution-is-the-claims-file](session-attribution-is-the-claims-file.md): Every parallel session commits as the same git user, so blame cannot say which session owns a function - lane_claims.csv can.
- [t3-certified-standard](t3-certified-standard.md): RULE: T3 = FUNCTIONALLY EXACT to the original game -- same inputs produce same outputs (behavioral equivalence). NOT a byte-diff verdict. tools/t3.py --qualify formalizes it (Gate 0 completeness, Gate A residue is compiler-choice, Gate B @t4-pass ledger), but the STANDARD ITSELF is same-in/same-out.
- [walls-are-dated-verdicts](walls-are-dated-verdicts.md): RULE 2026-09-13 - a \"wall\" is a verdict about the LEVERS TRIED AT THAT DATE, not a permanent do-not-touch; re-triage parked walls whenever a new lever class lands
