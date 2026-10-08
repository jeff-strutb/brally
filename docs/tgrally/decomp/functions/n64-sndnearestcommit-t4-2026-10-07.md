# N64 sndnearestcommit t4

*Recorded 2026-10-07.*

> BrSndNearestCommit 0x8022B534 T4 (01d43773, eebbc5): alias extern = table element (address live from the top kills a later address web); compound assignment promotes an address-taken local; v0 "temps" are coloured

BrSndNearestCommit (sfxsrc.c) 120 -> EXACT by hand, gate 723/0, commit 01d43773.

Levers, each measured:
- **ROM v0 where ours has a ring temp is ALWAYS a uopt colour.** ugen never hands out v0/v1 as ring temps (checked: 0 of 1580 ALLOC results in paintshop.c). Find the variable.
- **Address-taken local promoted to a register** (`int vol` passed as `&vol`): uopt makes it a candidate with load/store pieces around calls and indirect accesses; it is coloured only when save > 0. `vol = vol * k >> 8` = save 0 (stays memory). `vol *= k; vol >>= 8;` = two uses in one block, save 1 -> v0 (`lw v0; mul v0,v0,t; sra t,v0,8; sw t`). A later `vol = g; g = ((unsigned)vol >> 1) & m;` (def without store) puts that load in v0 too. An unknown-pointer ilod or a call between def and use zeroes the save (tested in scratch t-files).
- **An extern that is really a table element** (`D_8031C630` = &D_8031B760[0] + 0xED0): reading it through the table (`D_8031B760[0].link->flags`) adds an lda occurrence at the top; the table's address web then spans calls and is not coloured, so the two later `(idx + D_8031B760)->camMode` tests use a ugen-cached temp (t2) as in the ROM. Diagnostic `p1:wN=s` on the address web had shown EXACT first. Screen: ROM lui/addiu of a table base reused in a t-reg across a fall-through block = no uopt web; look for a direct global whose address lies inside that table.
- `p = (float *)(*(int *)&S.pObj + 0x30)` (load as int) makes a load of its own, so the join reloads pObj (ROM `lw v0,0x18(s0)` then later `lw v1,0x18(s0)`).
- PC twin (src/brally/core/audio/br_sndpos.c) confirmed the structure.
- **Same day, BrCarSfxLoad 0x8022BAA0 T4 (0f51b9f2, gate 724/0):** three calls reading `D_8028BC04[0].data/size/loop` kept the table address in s0 (free: s0 already saved). Reading through a pointer local set BEFORE an intervening call (`s = D_8028BC04;` after the loop, before the printf) makes uopt const-propagate the address into every read: direct `lui/lw` per argument, no web. Set in the same block as the first use, s itself is coloured there (v0).
Related: [n64-rumbleupdate-wip-2026-10-07](n64-rumbleupdate-wip-2026-10-07.md) (different-base symbol lever), [n64-cpakcheck-prototype-v0-2026-10-05](../levers/n64-cpakcheck-prototype-v0-2026-10-05.md).
