# N64 carplayerctl wip

*Recorded 2026-10-06.*

> BrCarPlayerCtl 0x80226488 (entinit.c) T4 2026-10-07, c66598d6 (session 569e0e), gate 710/0. Last residue (zero in $f12) solved by cfe temp-slot sharing: array chain temp + sqrt-across-call temp share one slot = one uopt web

**DONE 2026-10-07, commit c66598d6, image gate 710/0.** 235003 took it 462 -> 4 words; 569e0e closed the last 4 (the line-follow zero `mtc1 $zero,$f12` + 3 stores; ours was $f0). Drafts: build/tgrally/n64/search/80226488/s569/ (n12/n13 final; pd.py/pt.sh probe disasm+CDX trace, log.py full-fn trace, uc.py cfe/uopt ucode dump, cx.py IDO-corpus disasm).

**The lever (new, general):**
- An ARRAY chain `v[0] = v[1] = v[2] = 0.f` makes cfe reload v[2] into a cfe temp (vreg) and store the temp to v[1], v[0]; ROM store order y, x, z. A struct-field chain makes no temp.
- cfe reuses a freed temp slot of the same type for later statements' temps (int and float temps use separate slots). `s = sqrtf(dy) / (BrVec3Length(...) + 1.0f)` keeps the sqrt result in a temp across the call (the ROM's 0x34 spill/reload), and it took the zero chain's slot.
- uopt keys webs on storage (L26), so the two temps became ONE web {zero block, tail}; its forbids came from the tail ($f0 Length result, $f2 division) -> $f12, and the zero is materialised in the web's register.
- The division needed its own local `s` (PC twin's `v`): through the scan's `dx` (loop web, $f2, live in the block) the division CSE could not take $f2 and took $f12 first.
- `sqrtf(BrVec3Dist(...))` adds a Dist temp in a lower slot; routing Dist through `dy` keeps the shared slot the frame's lowest word (0x34); one unused int above it fills the 39-word frame.

**Diagnosis facts (measured):** p1 colour = lowest non-forbidden caller colour of min cupcost; a single-block web gets forbids only from coloured webs sharing its blocks; split pieces recompute forbids; escaped locals are excluded from any block with an indirect load/store (calls do not exclude; Y8-style escaped webs skip call blocks). The IDO corpus (build/tgrally/ext/n64corpus) showed zero-into-$f12 always comes from a web reaching a block where $f0/$f2 are busy, which pointed at web merging. When a lone constant has an inexplicable colour, look for cfe temps sharing a slot with a temp elsewhere.

Related: [n64-frameend-wip-2026-10-06](n64-frameend-wip-2026-10-06.md), [n64-ido-float-literal-spelling](../levers/n64-ido-float-literal-spelling.md), [n64-trackdrawsetup-2026-10-06](n64-trackdrawsetup-2026-10-06.md).
