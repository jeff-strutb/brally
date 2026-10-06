# Cpp lane class cracks

*Recorded 2026-09-12.*

> 2026-09-12 (4th session): the project lead asked for 20 T3 in a day again; the pre-qualified pool was EMPTY (0 READY of 132 uncertified non-EH rows), every close C row parked with a dead-list. Delivered 7 BYTE-EXACT instead by cracking two classes in the C++ lane: the byte-typed thiscall parameter (4 net-writers) and vcall-imm8 hooks (3), plus two new register-colouring levers.

**2026-09-12 fourth session. "Can you do 20 contract-valid (T3) today?" --
measured NO from the pool, YES-ish from class cracks: 7 byte-exact (T4)
commits, 0 T3 tags.** Survey method (13 min for 132 rows, reusable):
`t3.py --qualify` over every `status==diff` row minus `t3.py --vas` minus
EH (`6aff`), `xargs -P 6`, then parse the `A1..A5` lines and rank by A3
unpaired then A2 distance.  Result: **0 rows pass Gate A**; the 7 rows with
A3 = 0 unpaired all fail A4 on real ORDER differences (0x10060F40 zero web,
0x10013F20 eax/ebx + tail schedule, 0x10039D20 load order, 0x10058540,
0x10058900, 0x10013FD0, 0x10028620); the 1-unpaired rows (0x10032320
`and 0xff`, 0x1003C950 global re-load, 0x10038A80 `or -1`, 0x10015550
`neg`, 0x10016AA0 cosine rounding store, 0x10040A90) ALL carry hand
dead-lists.  The 09-09 harvest of 17 READY rows does not recur.

**What paid -- supply #1 of [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md), the C++ lane:**
1. **Byte-typed thiscall parameter = the whole net-writer wall.** `class
   BrBitStream { void WriteU8(unsigned char); void WriteU16(unsigned
   short); ... }` declared-not-defined, body verbatim from the C twin:
   0x1006B080, 0x1006AFA0, 0x1006AFF0 byte-exact on the FIRST cpp_sweep;
   0x1006AEB0 needed two more levers (below).  0x1006AFA0 was @t3-certified
   -- a T3 can be promoted by a lane change; retire the @t3 block by hand
   (the validator flags it once `stale_claims --fix` converts the tag).
2. **vcall with an imm8 code push** (`push 0x74; call eax` through a cached
   slot): a class with virtual `s5(int,int,int)` -- 0x10037FA0 /
   0x10038000 size-exact at once, byte-exact with lever 3.
3. **Register = web CREATION ORDER; an expression spelled at the use site
   creates its web later than a named local.** `x + i * 0xc` in the loop
   (strength-reduced running register created after the counter's) colours
   i=esi / x=edi; `x += 0xc` colours them the other way.  0x1006AEB0: no
   named `type` local -- `(int)(flags & 0x3f)` at all four uses gives the
   CSE temp the original has (spilled at once into the SPENT pBs arg slot,
   reloaded after the loop, ebp freed for the loop flag).
4. **`add R,-K` in place = a CSE'd `v - K` spelled at every use** (never a
   `v -= K` / `v = .. - K` statement): 0x10037DC0 parked 16 diffs -> byte-
   exact in ONE probe, also fixing the y/n register swap and the two spill
   slots.  All four are on the tail of docs/brally/VC5-IDIOMS.md.
5. **Positive guard puts the failure exit at the tail** (`if (n <= 0x100)
   {...; return 1;} return 0;`) -- reconfirmed on 0x1006AEB0.

**Tooling for the cpp lane (fn.py does not read it):** compile a variant
with `sh tools/toolchains/wine.sh tools/toolchains/msvc5/bin/cl.exe /nologo /O2 /GX /MD /W3 /I
include /I tools/toolchains/msvc5-compat /I tools/toolchains/msvc5/include /DBR_MATCHING_BUILD
/c build/brally/win32/match/t3d/cp_<tag>.cpp /Fo<obj>` (the obj path must be INSIDE the
repo, backslashed; a scratch path silently produces no obj), then
`divergence.py <obj> reference/brally/orig/<VA>.bin <symbol>` -- the symbol is the MANGLED
name for methods/free C++ functions (read it off the obj with
parse_coff_obj), `_Name` only under extern "C".   divergence.py's 4th
positional is CONTEXT, not the key; and its "1 region" can be a lost sync
-- read the whole output, not the last line (r7 trap: registers were still
swapped).

**Bookkeeping that bit:** `stale_claims.py --fix` also converts PEERS'
in-flight rows (it converted slice6_73.c for a peer's 0x10046E70; I
reverted that file) -- read its list before --fix.  Two peer sessions were
committing cpp matches in parallel; counts moved under me (re-derive).
`claim_lane.py release <TOKEN>` needs the token line printed by the claim
call -- capture it, the second `claim` call's tail hid it.

**Dead today (recorded in the dossiers):** 0x10021570 BrDlsTileRectE4 six
word/field spellings; 0x10016AA0 BrWeatherStepWind eight cast/assignment-
expression spellings of the trig roundings (the cosine's `fst [esp]` is
unreachable from casts); 0x10037FA0 seven declaration/statement orders
before the strength-reduction lever landed.

**"Go again" (same day, second half): +4 more byte-exact = 11 for the day,
this time in the C lane.** (a) **The settings-cycler class** in
slice2_25.c: 0x1003DB50 (117 diffs), 0x1003C6D0 (@t3-certified, retired)
and 0x1003C950 (parked "hoisted load") all fell to the SAME three spellings
-- step the global directly with no `v` temp, the shared call written
inside BOTH arms (never an `fEdited` flag), the table index re-reading the
global with the gate tested inline, sprintf through the import.  Two of
them carried dead-lists that had tried the spellings ONE AT A TIME; the
combination was never measured.  (b) **Index form beats the volatile
pointer walk** on 0x10058540 (4 grid loops, A4-only): `tab[i][k]` lets VC5
build the biased cursor itself AND keeps `inc; cmp; jl` after the stores;
`top` (div) before `left` (mod); the fourth loop bounded `i + 15 < 24`.
Both on the tail of docs/brally/VC5-IDIOMS.md.  Dead today (in the dossiers):
0x10038CA0 five index-temp spellings, 0x10054070 ten declaration/statement
orders of the delta pair, 0x10058900 five head statement orders,
0x10028620 two zero-rectangle orders, 0x10013FD0 two tile-word orders.
Lesson: a dossier's dead-list is a list of SINGLE levers; when two rows in
one file share a wall, re-probe the levers TOGETHER.

**Third stretch (project lead: "4 per session is ridiculous, do 3-4x"): +4 more
byte-exact = 8 THIS SESSION, from the /Od INTAKE VEIN.** The T1 pool's
odd-address rows in 0x1002Axxx-0x1002Fxxx are /Od (frame pointer, every
temp homed) and transcribe from the Ghidra draft near-mechanically:
0x1002E186 (344 B), 0x1002CEE9 (1145 B), 0x1002AF17 (1241 B), 0x1002B480
(1303 B, FIRST compile) -- all four in about 90 minutes, filed in
br_gamestep.c / br_framebegin.c.  /Od facts that paid: `__allmul`/
`__aulldiv` are plain `unsigned __int64` arithmetic; u64->float is C2520,
so widen by parts through a LARGE_INTEGER-style union (immediate-zero high
dword) and `(float)t.q`; `(float)` rounding stores need the sweep's Odp
shape; `x * 5 / 8` is `imul; cdq; and 7; add; sar` (a DIVISION, not >>3);
the converted byte goes on the LEFT of a blend so its fild/fstp temp is
evaluated first; a `for` with `i = i + 1` is the /Od loop shape.
 /Od STACK SLOTS FOLLOW THE LOCALS' NAMES, not declaration order (16, 38
and 28 name sets measured on three functions): renaming a local permutes
the slots; brute force 8-16 sets with `codprobe.sh` + a slot decoder lands
4-5-local functions in one or two batches, and it is NOT a simple hash
(sum/first/last/length/polynomial, 2..127 buckets, both tie-breaks all fail
the 28 observations on 0x1002A957 -- that six-local row stays parked).
Remaining /Od intake: none left odd-addressed in the T1 list; the /Od diff
rows are 0x10016C90 (1142 B, 790 diffs), 0x100695C0, 0x1002ECEB, 0x10022070
-- structural, not name-order.  0x10073994 is DATA, not a function (parked
via `claim_lane.py release <tok> 0x10073994`).  0x1003A140/0x1003A2B0
(split-time twins, cpp lane) stay parked: six layout shapes and VC5 moves
the sentinel block to the end in every one.

**Why:** the honest answer to "20 T3 today" is that the pre-qualified pool
is dry and each parked row is a research problem; the count moves by
cracking a CLASS (one declaration shape moved four rows) and by the
web-creation-order lever, not by tagging.
**How to apply:** at session start run the 13-minute survey, expect 0
READY; then look for a family whose dossiers name the same wall and route
it through the cpp lane or the two colouring levers above.  Candidates
left: the thiscall readers 0x1006CE80/0x1006CED0 (walls are register-death
widening and acc/mask allocation, not the convention), BrInputPollPressed
0x100705F0 (vcall, 11 unpaired), BrDxDetect 0x1001D8A0 (COM).

Related: [t3-frontier-map-2026-09-10b](../log/t3-frontier-map-2026-09-10b.md), [carstate-decode-cpp-2026-09-12](carstate-decode-cpp-2026-09-12.md),
[byte-slot-idiom-cracked](../levers/byte-slot-idiom-cracked.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [t3-certification-day-2026-09-09](../log/t3-certification-day-2026-09-09.md).
