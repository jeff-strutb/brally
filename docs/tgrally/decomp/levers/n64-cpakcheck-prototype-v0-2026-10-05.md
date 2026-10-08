# N64 cpakcheck prototype v0

*Recorded 2026-10-05.*

> BrCpakCheck 0x80214E0C T4 2026-10-05 (2856e895): an int-declared void callee holds v0 in its block and hard-forbids v0 for webs there; diagnose via CDX intf (no neighbour holds v0) + cfe ucode capture

BrCpakCheck (src/tgrally/gamedata/cpak.c) went 30 -> 0 on 2026-10-05, commit 2856e895, image gate 667/0. The 30 words were three v0/v1 swaps and one a2/a3 swap.

**Mechanism:** `int BrTextSetColours();` (really void). uopt treats the call's v0 result as live in the block, so any web live in that block gets v0 FORBIDDEN without any neighbour holding v0. The error switch's cfe selector temp was defined in that block. cfe reuses one switch/post-increment temp slot (vreg -128), so the fade counter's `wait++ == 3` temp shared the web. That web took v1, which pushed the promoted static `wait` into v0 and cascaded to &nameSeason. Declaring it void fixed all three swaps.

**How it was found (reading tools only, no search):**
- `n64alloc trace`, then `CDX_DETAIL_WEB=<web>` prints `[CDX] intf` neighbours with their assigned colours. If forbidden0 has v0 and no neighbour is assigned v0, it's a hard block conflict: look for calls with an int return in the web's blocks.
- `decomp-workbench capture make tools/toolchains/ido53 DIR --phase uopt`, build with TGR_CC, and decode `before-8-*` (cfe's ucode). `vreg ... (38,4,off)` shows cfe temps and which statements reuse the same slot (= one uopt web).

**Also:** a 4-argument call to a 3-argument function (an old a3 workaround) made a3 an argument register. The real 3-arg call frees a3 for the CSE temp the ROM has there.

Same class: [n64-carselect-handtranscription-2026-10-05](../functions/n64-carselect-handtranscription-2026-10-05.md) (void BrRomUnpack that returns int). Check every callee prototype against its definition first when residue is v0/v1 or a-register swaps.
