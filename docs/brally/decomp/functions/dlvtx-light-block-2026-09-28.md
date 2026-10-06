# Dlvtx light block

*Recorded 2026-09-28.*

> BrDlVtxLitDecal 0x100221D0 T4 DONE 05048e82, EXCLUDED (T4): parens + exact symbol count via real headers + decl order; method natsearch -> count scan -> header subset-sum; NoZLit next

2026-09-28, 0x100221D0 BrDlVtxLitDecal (TU tu_022, /O2 /Op). Project lead: "just T3 or T4 those 2
excluded functions" (T3 impossible: never called). Paused by the project lead; resume here.

**Cause found (commit 03f74669, tools/brally/c2emu.py = VC5 backend under Unicorn, byte-identical):**
- Light-block schedule: the rows' redundant inner parens `((a+b)+c)/K` add a 0x138 precision
  node -> +1 DAG height on the P1/P2 path -> `mov d0` loses to the lightScale[2] store in the
  list scheduler (prio = h<<13, +0x8000 x87, +1 float store). Write rows WITHOUT the inner parens.
- Remaining residue is commutative operand order: sort key = depth<<24|count<<16|XOR hash
  (local leaf = symbol index; load = offset^op^base; product = load^sym^0x143). Controlled by
  symbol indices: file-scope decl = 3, block-scope extern = 0, local decl order/gaps.
- PROVEN byte-exact (only link-time reloc fields differ) with measurement padding: rows 0,2
  unparenthesised, 480 extern pads before the z-column extern + 200 after, 58-61 unused int
  locals between `m` and `fy fz`, 32-35 between fz and fx (build/brally/win32/match/t3d/litdecal/lay.py).

**Natural-layout search** (build/brally/win32/match/t3d/litdecal/natsearch.py; knobs: file vs block-scope
externs, extern order, std includes, local order, per-row parens): best (1,9) = light exact,
loop x-column role flipped in 4 rows; state in build/brally/win32/match/t3d/litdecal/best_2.json/best2.c
(uses <windows.h> OUTSIDE the matching arm -> would break the port; must move inside
#ifdef BR_MATCHING_BUILD before committing). Next: add column-form knobs (x/z/w column
extern vs macro) and rerun; then gates (sweep, fileaudit, image_build, portcheck) and commit;
keep config/brally/excluded.csv row (tiers shows EXCLUDED (T4)).

**NoZLit 0x10023360** not started: same family residue; apply the same paren + key method.

Scratch (persists): build/brally/win32/match/t3d/litdecal/*.py, build/brally/win32/match/t3d/ldil/* (captured IL sets).
Tree note: another session had staged deletions under src/brally/ at pause time -- not ours.
Related: [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md), [m1-final-hand-lane-2026-09-28](m1-final-hand-lane-2026-09-28.md), [gap-sessions-audit-2026-09-28](../oracle/gap-sessions-audit-2026-09-28.md).

**DONE 2026-09-28 (05048e82, filing 676dbc26):** natural source = best natsearch layout +
headers "br_dlcmd.h","br_vecd.h",<math.h>,<ddraw.h>,<dsound.h> (after windows/limits/stdlib/
stddef). Method that closed it: (1) natsearch.py to light-exact; (2) cscan.py: pads at the
matching-section top show ONE exact extra symbol count works (+448, +-1 fails); (3)
hcount.py measures each header's symbol delta via IL capture (gl[7:9] = symbol count);
(4) subset-sum of header deltas = 448, verify by compile (33 sets, many exact). Image gate:
BRGlide 1308 placed, 0 diff, 0 refslot. src/brally/ is now MSVC-only (no BR_MATCHING_BUILD); port
drops go in ports/brally-wasm/patch/<path>.port (portgen.py) -- see other session's message.

**NoZLit 0x10023360 (2026-09-29, 4b0da848): T2, 13 instruction rows (2 are reloc fields), 1019/1019 B.**
Transform rows + setup stores exact. Open: light-row dz/dx pairing (orig (m2*dz+m0*dx)+m1*dy) and each
pair's load order = equal-priority tie in pass 0 broken by tuple order (+0x36); ambient-block int order.
Tools (build/brally/win32/match/t3d/litdecal): natsearch2.py (generic layout search; SPLITLOC/PADKNOB/FREEZEPAR/SETPAR),
structsearch.py (statement structure scored by best forced comparator flips, parallel IL capture via
C2CAPDIR), x87sim.py (maps homes/pairings), dperm/dpos/lrand/cscan2. Parens-on (111) gets the pairing right
but needs the dy fild->home edge +2 (emulator-proven); no source form found yet.
