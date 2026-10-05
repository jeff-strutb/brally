# Reloc pair lever

*Recorded 2026-09-18.*

> tools/reloc_pair.py recovers unresolvable T3 reloc addresses from the original body by instruction-shape pairing; 81/118 T3 place (d203f757); selftest 329/329, exposed stale D3D map rows; remaining 37 blockers classified

**2026-09-18 (d203f757), sequel to [t3-image-reference-fill-unsound](t3-image-reference-fill-unsound.md):** T3
image placement went 56 → 81 of 118 via three levers, all evidence-gated:

1. **Place under the certification's own resolution:** `collect_t3` now uses
   `t3b_env.augment_maps` per object (address-in-name, `/* 0x<VA> */`
   declaration comments, `??_C` string constants, imports) - the exact maps
   the A5 oracle certified each function under. (+12)
2. **`$T` .rdata constants:** `t3b_env.const_slot_values` (factored out of
   `resolve_bytes`, pure refactor - the `$L` SEH dummy stays oracle-only,
   never in an image) feeds `compiled_functions(extra_sites=)` per-site
   values. (+4)
3. **`tools/reloc_pair.py` - THE NEW LEVER:** recovers a hand-named
   static/global's address from the ORIGINAL body's own dwords by
   register-blind instruction-shape pairing. Forced assignments only:
   per-key count match, one symbol + one implied address per group,
   whole-function conservation, section validation, known-address decoy
   refusal, cross-function agreement. (+9)

**Evidence protocol:** `.venv/bin/python tools/reloc_pair.py --selftest` =
leave-one-out over every certified T3 row. 329 recoveries; 324 agree with the
maps; the 5 "disagreements" all reproduce the original image's own dword  - 
they are STALE D3D-SPACE MAP ROWS (g_220C40, g_br0AB3D8, BrSub10071130
suffix, __imp__wsprintfA), the constant-per-region D3D→Glide drift. Pairing
follows the binary; the binary is the truth. Blind (no-anchor) mode is NOT
production and measures worse - don't quote its numbers.

**2026-09-18 (cont., 9c23d7ac/fddf2ade/3760149f): 99/118 place.** Levers
added, each measured by leave-one-out before use (final: 2632 recoveries, 0
non-stale disagreements): audit_function (every map anchor re-derived by
pairing; ~20 STALE hand-coined rows overridden with the original's own
dword), full ??_C decode + data-section-only find_bytes, jump_table_slots
($L switch tables = own placement + label offset, exact),
content_anchor_syms (initialized statics by content identity; RO=any copy,
RW=unique), import_thunk_syms (import table → jmp-[IAT] thunk, exact),
call/jmp→'xfer' key, jcc excluded from DIR32 pool, per-BUCKET conservation
(buckets independent), context refinement (neighbour mnemonics),
bounded-surplus solving (≤2 strays, unique mapping).  ORDER-preserving
assignment MEASURED 162/194 and REJECTED - never adopt it.

**THE 19 STILL BLOCKED = information floor for shape recovery; each is a
named per-function hand lane:** (a) DETECTED layout conflicts - g_menu$S445
(br_menucb.c trio) and g_brFfb imply different bases in different functions
→ compare source struct offsets against original disasm, fix the spelling;
(b) semantic-named BSS clusters shape cannot split - br_fade's 25 globals
(BrFadeTick), texscan 5 (BrGbiTexScanSetImg), g_brKeyBias/Count,
g_BrDPlay$S710; hand-derive from original usage, file /* 0x<VA> */ decls;
(c) heavy-drift giants - BrCarDrawVehicle (22 sites vs 16 fields deficits +
9v152), BrScenePropsDraw, BrExt_10052030/10054B50 (24v76, 32v78 call
counts: inlining differences), BrHudDraw, BrRcaFixup, BrSub_100173F0,
BrTrackFixupRec54, BrCtrlCfgAssign (1v0: original reaches the callee some
other way - check for inline), BrFfbUpdateSpring/EnumDevice/Init,
BrDPlaySysMsgLog, BrMenuCap0730/Time0C00/Text15A0, BrNetPeerSendPass(?),
BrSfxBankLoad(?). `globals_learned.csv` refreshed (1878 rows, 367/367)  - 
these blockers appear ONLY in T3 bodies; reloc_learn can never see them.

**Trap:** the gate's "T3 residue" bucket hides wrong addresses inside T3
spans by design - soundness lives entirely in the resolution path, so any
new resolution source must carry its own proof (selftest or oracle), never
a plausible guess.

**2026-09-18 (81b87ec8, branch archive/epic-leakey-34c81e off d203f757): the
5 stale rows FIXED AT SOURCE** - all were names/comments, not CSV rows
(globals_glide.csv does not exist; globals.csv is D3D-space by design and
unread by resolution). g_220C40→g_21C770 (br_cd.c), g_br0AB3D8→g_br0AAB78
+ its `/* 0x100AB3D8 */` extern comments, BrSub10071130 callers→BrSaveLoad
(0x1006A080; slice5_60 port twin keeps the name, headers untouched),
BrFChkFRead wsprintfA→sprintf (original calls __imp__sprintf 0x118F0570).
All four verified against the original bytes first. On that branch:
selftest 388/388/0, T3 place 82 (was 81), blocked 36 (was 37), sweeps
byte-identical, portcheck clean. **LATENT CLASS still unfixed:** every
D3D-suffixed `g_<HEX>` name whose decode lands inside a Glide section
poisons augment_maps the same way whenever the learned map lacks the key  - 
br_cd.c siblings g_220CD0/g_220C3C/g_220CD8/g_0940A4/g_0940A8/g_575470/
g_575454, br_optcycle.c g_br0AA010/g_br0AB3E0/g_br0B4050/g_br22AF18, and
include/slice2_25.h:333's stale decl comment. Each rename needs its own
byte verification (globals_shared.csv gives the expected Glide twin).
