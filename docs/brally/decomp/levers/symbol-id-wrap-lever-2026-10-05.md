# Symbol id wrap lever

*Recorded 2026-10-05.*

> VC5 order-only choices (copy coalescing of commutative ops, local register picks) follow front-end symbol ids mod 65536; the original BrTex3dExpand TU had its id counter wrap 65536 INSIDE the function. Pad typedefs ahead of the function to place the wrap. 0x100250D0 went 399 -> 61 with it.

Found 2026-10-05 (session, BrTex3dExpand 0x100250D0 hand transcription).

**The lever:** the front end numbers every declaration in the file (headers, typedefs, locals) with one counter; C2 reads ids mod 65536 for order-only decisions. Locals whose low-16 id is in [0, ~512) behave differently from ones with large ids: a commutative `I * d` whose operand d dies is NOT coalesced into d (product copies I instead). The original's I4-blend body 2 has small-id deltas while body 1 has large ids, so the original TU's counter wrapped 65536 between them. Reproduce by inserting N `typedef int pK;` lines ahead of the function (`padit.py` in the session scratch); temp ids (file total) proved irrelevant (pad after the function inert). Typedef spelling/operand order of `|`, `&`, `*` in source is inert: C1 canonicalises, ids decide.

**Measured:** wrap position scan has sharp plateaus (each declaration crossing changes things); best pad 43332-43333 for the current candidate. A mini copy of one loop reproduces the effect (pad scan on a mini found the [0,~512) window).

**Other levers found the same day (all measured):**
- Delta-first statement order (`d = (hi&0xff)-(lo&0xff); lo = ...`) decides which of lo/delta gets the register (the +0x40 tie key follows reference order).
- Frame slot ORDER is a reference-count sort: the original's low slots told us the per-channel lows are CSE temps (inline `(param_X & 0xff)`) shared by all three blend bodies and the IA8 arm; only alpha's delta is a named loop local. Use `slotuse.py`-style per-slot arm usage, orig vs ours, to infer which values are one symbol.
- Statement order inside a pixel (`uVar14 = ...` before `loIA8 = ...`) and per-loop ownership of temps (IA8 loops 2/3 own `ia_g`) fixed IA8 slots/registers.
- Per-loop block-local byte vars (post-wrap ids) fixed nibble-merge register picks in I4-direct and IA4.

**Open (61 lines left):** CI8's exact source is the single-counter form (CI8 code becomes exact) but it raises param_2's register priority by 72 past the body-1 deltas (17752 vs 17728), wrecking the frame; need the original's compensating -24 somewhere. Body 1/2 pixel-2 alpha-low reload register (split pieces) and store/cmp order; I4-direct loop 0 site 1; IA8 loops 1/3 shr/and order.

Candidates saved under build/brally/win32/match/m2_candidates/0x100250D0_BrTex3dExpand_handtx_<lines>_pad<N>.c (best: _61_pad43333). Related: [brtex3dexpand-t4-handtranscription-2026-10-05](../functions/brtex3dexpand-t4-handtranscription-2026-10-05.md), [declaration-order-tiebreak](declaration-order-tiebreak.md).

**Update (same day, later): 61 -> 8 lines.** Best: build/brally/win32/match/m2_candidates/0x100250D0_BrTex3dExpand_handtx_8_pad43330.c (pad 43330 typedefs ahead of the function). Fixes, each one reasoned edit:
- CI8/RGBA row counters as their own function-level vars (pre-wrap) -> param_22 loads first.
- CI8 tail `n4 = n4 + 1; iVar16 = n4;` plus `*param_1 = FUN_100271f0(...)` direct stores in every arm (uVar4 temp removed) -> CI8 exact; the uVar4 candidate was what tipped param_2's priority.
- Byte nibble merges: `x >> 4 | x & 0xf0` puts the shift in the OR dest iff bit 7 of x's id is 1; `x << 4 | x & 0xf` iff bit 3. Same for an unsigned int x in a register. I4dir loop 0 byte = FIRST function-level declaration (id bits 7 and 3 set).
- Store-before-increment (`*param_1 = v; param_1 = param_1 + 1;`) in blend body 1 pixels 1/2 (store wins the scheduler tie over cmp).
- Body 1 pixel 2 alpha computed inline in the pack expression.
- Temporaries' registers: rotating pointer eax/ecx/edx, reset once per function (tools/brally/c2read --rotation; 's note).
**Left (8 lines):** body 2 pixel 2 alpha-low reload (orig `mov ecx,[0x28]` late + `add edx,[0x7c]`; ours ebp early via the forwarded uVar8 copy candidate #283-ish) and IA8 loop 3 (orig `and ecx,0xf0` in the OR dest; 32-bit form is always shift-first, so loop 3 is a different shape). Then: compact the typedef pad (macro-generated), commit, image gate.

**Update 2: 8 -> 5 lines.** Best: build/brally/win32/match/m2_candidates/0x100250D0_BrTex3dExpand_handtx_5_pad43330.c. IA8 loop 3 fixed by `uVar14 = b731; uVar14 = uVar14 >> 4 | uVar14 & 0xf0;` (operand is the int var uVar14, id bit 7 = 0 -> `&` half in the OR dest; the plain 32-bit-from-byte form is always shift-first).
**Only residue:** blend body 2 pixel 2 alpha tail. Original: `add edx,[0x7c]` (blue sum from memory) and alpha low reloaded late `mov ecx,[0x28]`. Ours with store-first: alpha low loaded early into ebp (eax/ecx/edx all busy at its codegen point) and blue sum split into `mov eax,[0x7c]; add edx,eax`. With increment-first the reload goes late into eax and the store/cmp order flips. Inert: own a2 variable, alpha inline in the pack (perturbs pixel 1), pack operand order, `*param_1++ =`, counter after store, compiler-temp vs named alpha low. Next: read regasg.c reload/hint handling (Byte Tactics docs/c2-regalloc.md "Temporaries") for why the original's reload lands in ecx.

**Update 3.** Two candidates, neither T4: _5_pad43330 (blend body 2 pixel 2 alpha tail off) and _11_blendexact_pad43330 (blend arm exact via `(unsigned int)(a2 = (unsigned char)(...)) << 5` in body 2 pixel 2; but the CI8 and RGBA param_8 mirror loops swap their reload/jmp layouts). Original: RGBA mirror has the preheader reload + jmp, CI8 does not. Colouring and piece->region mapping of param_7 are identical in both builds; only the reused piece ids differ (s30 split ids 416,309,336,130,187,339,198,57 vs ae 417,335,425,331,33,340,133,259), and the adjacent-id order flips exactly at the two mirror-loop pieces. The id change traces to pixel-2 g2/b2 8-bit pieces dropping 7296 -> 6912 when alpha is evaluated inside the pack. Next: find the source form that evaluates alpha inside the pack without lengthening g2/b2's ranges (or the original's other id-affecting difference). Inert/worse so far: comma form, body 1/pixel 1 assignment form, row counters at end of decls, alpha before blue.
