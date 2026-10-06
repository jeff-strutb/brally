# Cpp vcall family lode

*Recorded 2026-09-02.*

> The residue's frame/dense/scattered classes are heavily one C++ vcall family; 25 landed in one session, gen_cpptwin stamps reloc-twins, more members remain

2026-09-01: the `--residue` classes are NOT cause-homogeneous - screening the
frame class by prologue bytes found the real families. The big payer is the
**UI/phase C++ vcall family** (GameObj +0x2AE8 pSub / +0x2B5C item / +0x3838
sel), spread across frame, dense, scattered, long AND short stamps. 25 new
byte-exact TUs in `src/brally/core/cpp/` in one session, nearly all FIRST COMPILE
from the 0x1003D4A0-style skeleton (residue 254 → 219; C++ workstream 80 →
104 match).

**Why:** the divergence classifier groups by byte symptom, not source cause;
a C draft of a C++ thiscall/vcall function lands in whatever class its call
shape produces. Screen candidates by ORIGINAL BYTES (thiscall receiver
`8b f1`/`8b ce`, vcall `8b 11 ff 52`/`8b 01 ff 50`, pSub `8b 88 e8 2a 00 00`,
EH `6a ff`) before trusting the class label.

**2026-09-01 later - the screen is now a tool:** `tools/brally/cpp_screen.py`
mechanizes the byte screen (eax-vcall, this+ret-imm, CSEd-vtbl = strong;
bare this-ecx = weak/fastcall-representable). Run of the 211-row residue:
**52 strong**, and they include ALL the multi-KB short/error shells
(0x100425E0 2659B, 0x100476E0 2679B, 0x100439B0 3746B, 0x1004AEE0 3862B,
0x100498A0 3993B…) - ~30 KB of "short" residue is C++ the C refine can
never match. Route those to the C++ TU lane; never assign them as C work.

**2026-09-01 latest - the multi-KB shells are FALLING: the /GX menu-builder
generator pattern (VC5-IDIOMS "menu-builder trio" + the two follow-ups)
landed 0x100425E0 (2659B), 0x100476E0 (2679B, first compile), 0x100439B0
(3746B, first compile) byte-exact - write pages from the Ghidra draft with:
char-bool-after-store null checks, inline `(short)(w14+1)` sublink,
w2AB6-store-before-w2AB4-inc, per-block photo statement order taken from
the draft. 0x1004AEE0 (3862B) PARKED at 34 diffs: photo1's fsub/fstp/store
scheduling resists 14 spellings + /Op (park note in the TU - do not
re-probe). 0x100498A0 (3993B) NOT STARTED - same family, degraded draft
(args in uStack noise; transcribe arg lists from asm), contains the same
photo trio so expect the same ~34-diff park unless the knob is found.
Also landed: 0x100415D0 (738B page Frame), 0x100414B0/0x10041940/
0x100414F0 (small members), 0x1003CD60 BrOpt3810 (490B, sequential-pDesc
jump-threading lever), 0x10041F50 (1668B Close+delete teardown ladder).**

**How to apply next session:**
- `python3 tools/brally/gen_cpptwin.py` after ANY new C++ TU lands - reloc-masked
  twin stamper, +7 free so far ([vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md) entry).
- Remaining family members (screened, unmatched): 0x1003CD60 (490B BrOpt3810),
  0x10041980, 0x10041F50, 0x10041DD0, 0x10036220, 0x1001CE20 BrAppStateSetMode,
  0x1006B440, 0x100325B0 BrExt_10038F30, 0x100415D0, plus the 0x10008xxx and
  0x1006xxxx thiscall-receiver clusters (different object families).
- The residue `error` class (BrExt_10052030, BrPhaseEnterPlaceholder_1004A580,
  BrExt_10054B50, BrOptFn1004CAC0…) is `6a ff` C++ EH - cpp workstream, not C.
- Three "frame" rows are EH epilogue FRAGMENTS, not functions: 0x10074750,
  0x1007488C, 0x100747BD - fence them, never hand them out.
- 0x10037DC0 parked at 16 diffs T3a (ebp/ebx rotation + `add r,-0xc` vs
  `sub r,0xc`; every subtraction spelling canonicalizes to sub). Unsigned
  loop count was REQUIRED for the frameless shape (signed → jle + ebp frame).


**2026-09-03 session - the family is NOT dry, but its cheap half is.**
`tools/brally/cpp_screen.py` still lists 37 unmatched CPP-strong VAs. Landed
byte-exact: 0x1003DEC0 (178 B phase-leave, sibling of 0x1003DF80 - clone
that TU and change the swap slot / the doubled 0x10-flag clear),
0x10058D00 (53 B chain insert), 0x10055330 (117 B point-in-rect + flag
word). All three FIRST COMPILE from the asm, no probing.

Five more were transcribed to every-instruction-present and PARKED on
residues that are NOT source-reachable - read the file headers before
touching any of them, each carries its dead-probe list:
0x100540D0 (1), 0x10054280 (1), 0x100087D0 (7), 0x1006FCE0 (12),
0x10059350 (103, all one cross-jump), 0x1006D0B0 (40, T3a pairing).
The three distinct emitter-level causes are in [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md);
0x100540D0 + 0x10054280 both convert the instant the SIB one is cracked.

**Still untouched from the strong list, no known hazard:** 0x1004ABE0
(760 B), 0x1004A840 (923 B), 0x10054730 (618 B), 0x10054E20 (510 B),
0x10055C50 (238 B), 0x100541B0 (196 B - the third glyph walk, pen and
context as PARAMETERS with the param slots reused as the float local and
the fild scratch; expect the same 1-diff SIB park), and the 1-3 KB
0x1004xxxx / 0x1005xxxx shells.

**Screening note:** `gen_cpptwin.py` found 0 twins after every one of the
new TUs this session. It is cheap, keep running it, but do not plan count
on it any more.


**2026-09-03 later - THE ITEM RECORD IS THE FAMILY KEY.** 0x10041300
(247 B, byte-exact first compile) pins down the 0x438 record the whole
UI/phase family manipulates: **vtable at +0x00, flag word +0x04, kind
byte +0x08, label char[0x401] at +0x09, then w40A/w40C/f410/f414/f418/
w41C/f420/a424[4]/f434 - summing to exactly 0x438.** The owner embeds it
at +0x2B5C (an ARRAY of 3: the EH unwind fragments 0x10075030/0x10075060
destroy 3 x 0x438 there). 0x10054E20's slot array is the SAME record.
Copy the class decl from src/brally/core/cpp/0x10041300.cpp.

**A byte screen for this family** (scratch recscan.py pattern): look
for the displacement dwords `5c 2b 00 00` (+0x2B5C item base),
`65 2b 00 00` (+0x2B65 label), `66 2f`/`68 2f` (+0x2F66/+0x2F68) and
`e8 2a 00 00` (+0x2AE8 pSub) in build/brally/win32/match/orig/*.bin. That found **31
unmatched members**, most of them 70-360 B. Five fell immediately:
0x10041300, 0x10037EF0, 0x100380B0, 0x10037E60 (all byte-exact), plus
0x1003AB00 parked on 2 bytes. Remaining and untouched: 0x1003AA10(238),
0x1003A910(245), 0x1003A580(322), 0x1003A6D0(322), 0x100393C0(330),
0x1003A420(341), 0x1003A140(360), 0x1003A2B0(360), 0x10040B10(478),
0x10039620(563), 0x10038F40(567), 0x10040EB0(587), plus the 1.5-4 KB
0x1004F8C0/0x100504A0/0x1004F290/0x1004E750/0x10051600 and the pSub
cluster 0x1003BA30/0x1003BCA0/0x1003B580.

The recurring shapes in the small ones: `_itoa` or a catalogue string into
the label, then relayout (+0x08 or +0x04 vcall) and repaint (+0x2C), with
the LABEL POINTER held in a local and null-tested before the repaint.
The BrUiText* rows triage calls "MISSING CODE (31-62% complete)" are this
same shape - the missing code is the item-label sequence.

**0x10075030 / 0x10075060 are EH unwind-action fragments, not functions**
(no prologue, `[ebp-0x10]`, array-dtor helper). Fence them with the other
three already listed above.


**2026-09-03 (third pass) - the item family keeps paying: +4 more
byte-exact.** 0x1003A910 (245 B), 0x1003A580 (322 B), 0x1003A6D0 (322 B,
twin), 0x1003A420 (341 B). Parked with dead-probe lists in their headers:
0x1003AA10 (14, register rotation), 0x100393C0 (94, arm tail-merge depth),
0x1003A140 (237, block placement).

**Three shapes cover almost everything in this family**, and they compose:
1. _itoa / catalogue string / sprintf into the label, then relayout and
   repaint vcalls, with the LABEL POINTER in a local that is null-tested.
2. A 32-byte scratch buffer memset to 0, a `strlen(buf) == 0` early
   return of 0, then `_strupr(buf)` copied into the label.
3. The TIME FORMATTER (0x1003A580 and friends): sentinel string when the
   value is not above a floor, else a five-step scale-and-truncate chain
   printed with sprintf. **Write that chain as one NAMED float local per
   intermediate** -- see [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md), it is the difference
   between 205 diffs and byte-exact.

Levers that decided several of these: the whole call goes INSIDE every
arm (never a ternary argument or a hoisted index variable); a switch is a
`dec/je` chain, an if-else-if is a `cmp` chain; the arm you want placed
last is the `else`; and an accumulator's zero must be a declaration
INITIALISER to hoist above the register saves.

Still untouched from the screen: 0x1003A2B0 (360, twin of the parked
0x1003A140 -- do it when that one is solved), 0x10040B10 (478),
0x10039620 (563), 0x10038F40 (567), 0x10040EB0 (587), plus the 1.5-4 KB
0x1004F8C0 / 0x100504A0 / 0x1004F290 / 0x1004E750 / 0x10051600 and the
pSub cluster 0x1003BA30 / 0x1003BCA0 / 0x1003B580.


**2026-09-03 (fourth pass) - the family's REAL blocker is the port's
globals-struct parameter, not the record layout.** +6 byte-exact, all
first compile, all by dropping `(pObj, BrUiGlobals *pG)` for the original's
single cdecl argument and direct externs: 0x100386B0, 0x100381D0,
0x100391F0, 0x10039270, 0x10039350, 0x10039510. Details and the screening
gotcha in [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md). 0x10038CA0 parked at 2
diffs (push register), 0x10038F40 at 10 (index-arm register rotation).

**0x10038F40 also produced the float-vs-int typing tell** - see
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md); that one change was worth 415 diffs.

`tools/brally/gen_uilabel.py` screens the 100-byte "catalogue string into the item
label" shape by a byte template derived from the hand-solved members. It
reports the family CLOSED at 8/8 and **swept no new functions** -- keep it
as a regression screen, do not count it as leverage.

Still unclaimed on the item screen: 0x1003BCA0(309) 0x1003B580(331)
0x1003A2B0(360, twin of the parked 0x1003A140) 0x10040B10(478, C++ EH
constructor with an array-new) 0x10039620(563, has a JUMP TABLE)
0x10040EB0(587) and the 1.5-4 KB shells. Remaining slice2_23.c port-body
diffs worth the same treatment: 0x10039870(277) 0x10038E10(180).


## 2026-09-03 (session 9) - the multi-KB MENU BUILDERS are a recipe, not a grind

+4 byte-exact, ALL FIRST COMPILE, 6,838 bytes in one pass: 0x10048160
(1091 B), 0x100458D0 (1565 B), 0x100451F0 (1749 B), 0x10048F10 (2433 B).

**Method, and it is nearly mechanical:** copy the class block out of an
already-matched sibling (src/brally/core/cpp/0x100425E0.cpp is the reference --
GameUi / the 0x348 page / the 0x1E214 BrCtl), then transcribe the Ghidra
draft entry by entry. Each entry is
    p = new BrCtl; cont->a18[cont->w14] = p;
    bad = (p == 0); if (bad) FUN_100378c0(4);
    p->s38(parent, x, y, flags, 2, 5, k, id);
    [ p->pfnNN = hook; p->w1E20C = n; p->s34(BrStrGet(id), 1, 1, &fmt); ]
    cont->w14 += 1; [ cont->w344 += 1; ]
The three levers from 0x100425E0 hold every time: CHAR bool computed AFTER
the slot store; simple float lvalues push raw while computed y offsets
(`f33C - k`) become fld/fsub/fstp; tail is w2AB6-store then w2AB4-inc then
w14. Float constants: decode the draft's hex (0x43430000 = 195.0f etc.).

New pieces this pass, all reusable: the +0x3838 selector sub-object
(vtable +0x14 configure, +0x10 add-item, pfn04 at +4, f14 at +0x14), an
int at +0x1E1F4, pfn14 at +0x14 of BrCtl, and a second page opened by
clearing `parent->a6C[parent->w10]` and running the page prologue again.
Copy 0x10048F10.cpp for those.

**Next in this family: 0x10046E70 (2114 B), the last lane member.** Same
builder, but one entry has a clamp + a three-way float interpolation +
an __ftol whose result feeds three stores, and Ghidra renders that block
unreliably (it invents a bare `ftol` call). Read the ASM for that one
block rather than trusting the draft. Everything else in it is the
standard recipe; entry list is already extracted in the session
transcript.


## 2026-09-03 (session 10) - the builders are now GENERATED

`tools/brally/gen_menubuilder.py` emits a byte-exact TU straight from the Ghidra
draft: it parses the draft's statements, renders the family's three levers
(char bool after the slot store, raw float pushes, w14-then-w344 tails),
and pulls the class block out of src/brally/core/cpp/0x10048F10.cpp. Seven
byte-exact out of it in one pass, 6,526 bytes: 0x10043050, 0x10043370,
0x10043690, 0x10046620, 0x1004A840, 0x10052610, 0x100469B0.

**It BAILS loudly on any draft line it does not recognise** rather than
guessing -- that is deliberate and it is what makes it safe to run over
the whole family. Extend it only for shapes you have verified.

Screen for members with:
    grep -c $'\x68\x14\xe2\x01\x00'   # push 0x1E214 == new BrCtl
over build/brally/win32/match/orig/*.bin, skipping VAs already matched. 26 were
unmatched at the start of this session.

**The remaining ~14 bail for real reasons** and want hand work: a
conditional on a mode global (0x1004FEA0, 0x10050AC0, 0x10051600), a
string-pointer walk (0x1004AEE0, 0x1004BE00, 0x1004DA00, 0x10045EF0),
strlen (0x1004F290, 0x100504A0), a prologue vcall on the root object
(0x10052A60, 0x10053590), a nested-object call (0x1004E750), and
0x10044860 whose draft has a different signature. 0x10046E70 is parked at
a constant-register fork (see its TU header).


## 2026-09-03 (session 11) - the generator grew a SCAFFOLD mode

+5 byte-exact, 9,028 bytes: 0x10053590 (1932), 0x10052A60 (2863),
0x1004FEA0 (1532), 0x10050AC0 (2701) and the extensions that got there.

`tools/brally/gen_menubuilder.py` now also handles: the root-object prologue
vcall (+0xC0 / +0xC4 tables), the selector's +0x04 / +0x14 hook slots, the
dropdown FILL LOOP (collapsed to one marker, shape verified on
0x10048F10), the SUBLINK trio, hook slots at +0x10 / +0x18, global int
stores and copies, and the page's own +0x04/+0x08/+0x0C hooks.

**`--partial` is the important addition.** It scaffolds a member whose
draft has function-specific logic, marking every unrecognised line with a
deliberately UNCOMPILABLE `@@UNHANDLED@@` so a half-generated file can
never pass for a finished one. Fill those in from the asm, then score.
0x1004FEA0 took seven fills and matched first try; 0x10050AC0 took eleven.

Two things the fills taught, both now in docs/brally/VC5-IDIOMS.md:
 - a conditional CAPTION duplicates the whole s34/BrStrGet call in each
   arm. A shared id variable makes VC5 go branchless (`neg/sbb`).
 - a mode conditional can wrap SEVERAL entries; get its extent from the
   draft's indentation, not from the first closing brace. Guessing short
   showed up as a `jne` displacement far too small.

Remaining bails, all needing hand fills: 0x10051600 (4109, strlen/strcpy
block + raw field stores + two conditionals -- 41 markers),
0x100498A0(3993), 0x1004CBA0(3671), 0x1004BE00(3475), 0x1004DA00(3394),
0x1004E750(2877), 0x10044860(2439, draft has a different signature),
0x100485B0(2389, uses FILE*), 0x10045EF0(1834), 0x1004F290(1570),
0x100504A0(1558), 0x1004F8C0(1498), 0x1004AEE0(3862, the known photo
park). 0x10046E70 stays parked on its constant-register fork.


**2026-09-03 (session 12) - the "still untouched, no known hazard" list was
STALE; screen it before working it.** 0x10055C50 (238 B) had been byte-exact
in the C lane the whole time (`src/brally/core/generated/0x10055C50.c`) - it is a
plain `__stdcall` free function, no `this`, and the current
`tools/brally/cpp_screen.py` run does not list it either. Always cross the list
against BOTH report.csv and report_cpp.csv first; the one-liner is in the
session-12 notes of [resume-state](../log/resume-state.md).

Landed: **0x100549A0 (132 B, byte-exact first compile)** - a method on the
same class as 0x10054E20. That class's slot array is based at `this` with
0x2C folded into every displacement, so a record pointer is
`(char *)this + idx*0x438` and the record's own fields read `+0x2C+off`.
The record at +0x2C is polymorphic; this one ticks it (+0x04 vcall), tests
a busy word at record+0x420, asks for a result (+0x14 vcall, returns char),
and hands the owner's +0x14 cdecl callback either the index or -1.

**0x10054730 (618 B) PARKED at 31** with a full dead-probe list in its
header. Worth reading for two reusable findings: member reads reproduce the
original's reload/forward pattern where an expression rewritten from the
same members does NOT (an `int *` parameter may alias `this`, so the rect
gets re-loaded), and the tail's two extents are ACCUMULATED
(`dx += i1a98c; i1a994 = dx;`) - worth 13 diffs on its own and now a
general idiom.

`gen_cpptwin.py` again found 0 twins.


## 2026-09-03 (session 12) - +5 more, 11.6 KB, incl. the project's biggest fn

0x1004F8C0(1498), 0x100504A0(1558), 0x1004F290(1570), 0x1004E750(2877) and
**0x10051600 (4109 B - the largest single function matched in the project
so far)**. Two of those generate END TO END with no fills.

**The unlock was realising BrCtl EMBEDS the 0x438 item record at +0x2B5C**  - 
the same record the item-label family manipulates. Once the reference class
(src/brally/core/cpp/0x10048F10.cpp) carried it as `Item2B5C m2B5C`, the label
strcpy, the rect written twice (control +0x50 AND item a424), and the
measured width `w41C = (a424[2] - a424[0]) - 0x10` all became named fields
and the generator could collapse them.

gen_menubuilder now also handles: the inline label strcpy (Ghidra unrolls
MSVC's scan+movs into two loops), the item rect block, the relayout vcall
in both spellings (`iVarN + 4` and `piVarN[0xad7] + 4`), the item's +0x420
store, an s34 with a non-DAT source, and an s38 whose parent Ghidra lost to
a uVar.

**Fills that keep recurring - check these first:**
 - a conditional caption/strcpy puts the WHOLE call in each arm (fourth and
   fifth sightings this session). A shared variable goes branchless.
 - `strlen(x) <= 1`, NOT `< 2` - the original is `cmp ecx,1 / ja`.
 - a mode conditional can wrap several entries; take its extent from the
   draft's INDENTATION.
 - **after filling, check recomp size against orig. SHORT with no markers
   left means you deleted a statement while filling** - that is how the
   second label copy went missing in 0x1004F290 (34 bytes).

**Session 2026-09-03 - the PHOTO BLOCK is solved, and the photo PARK is
now a three-function wall.** 0x1004ABE0 (760 B) fell FIRST COMPILE and is
the reference for the block; full write-up in docs/brally/VC5-IDIOMS.md ("photo
control block"). Three levers: the two strided array loops are ordinary
INDEXED for loops over contiguous ranges (0..14, 15..23) that VC5
strength-reduces into the pointer walks you see; the ftol rect is stored
**+0x54 FIRST**, then 50/58/5C; and `cmp r,ebx` vs `test r,r` inside one
function is the pinned zero register dying at a vcall's vtable load, not
two source shapes.

Applying it filled 0x1004BE00 and 0x1004DA00 end to end. All three of
0x1004AEE0 / 0x1004BE00 / 0x1004DA00 now sit at **exactly 34 diffs, all in
photo1's ten-instruction tail, byte-identical residue** - 10,731 B of
otherwise-exact code on ONE VC5 schedule. Orig computes both derived ints
(the `lea ebx+0x7f` and the `add edx,0x21`) before its three stores and
sinks the float store past the +0x2968 store; ours interleaves and sinks
the +0x58 store instead. Identical multiset = T3a. **DEAD PROBE closed
2026-09-03: putting the `fy -= K` update between the +0x2968 and +0x2A42
stores - the one slot the old dead list left ambiguous - is WORSE, 34->59.
Every placement is now covered. Do not re-probe.**

Also solved this session:
 - **0x10045EF0 (1834 B)** - the selector fill loop. Its ONLY diff was one
   byte: a signed vs unsigned loop-bound branch. **VC5 emits UNSIGNED for a
   pointer/pointer compare and SIGNED for ints, so a signed branch closing
   a table walk means the source compared the cursor AS AN INT** - cast the
   cursor and the end. Read it straight off the opcode: 0x72 unsigned, 0x7C
   signed. In VC5-IDIOMS as its own entry.
 - **0x100485B0 (2389 B)** - the save-file probe. Ghidra splits the flag
   into two temps because VC5 stores the 1 before the fopen and reuses the
   returned NULL as the 0; it is ONE source variable. The neg/sbb/and/add
   quartet is a BRANCHLESS TERNARY in the call argument, and the two if
   arms that set different +0x1E20C values are tail-merged by VC5, not
   shared in source - write both arms out in full.

**THE FAMILY IS NOW DRY for countable matches.** Every remaining member is
either filled-and-parked or carries the photo trio (so it inherits the
photo1 park). Standing order from here is SHAPE targets from
tools/brally/fnmatch/triage.py.

 - 0x10044860 (2439 B) - the generator used to BAIL ("draft has no
   recognisable entry point") because Ghidra types the parent as
   `float param_1`. That ONE mis-typing produces FIVE symptoms: an `(int)`
   cast on every prologue use plus a `*(float *)` +0x340 store. Fixed at
   parse time in gen_menubuilder.py (regex widened + typing undone), not by
   teaching five patterns. Then hand-read: there is ONE running float `fy`,
   not the two temps Ghidra splits it into - zeroed at the top, set to
   19.0f as the LAST statement of the first `DAT_100abaa4` block (the `je`
   skipping that store is what fixes the block's extent), stepping down past
   four controls. Now STRUCTURALLY EXACT: slot-blind, the only instruction
   differences in 2439 B are four fld/fadd operand orders. **PARKED on a new
   rule - VC5 CANONICALISES COMMUTATIVE FLOAT ADDITION**, so `a+b`, `b+a`
   and a named sum temp are byte-identical; see the VC5-IDIOMS entry. Never
   spend probes permuting a float sum again.
 - 0x1004CBA0 (3671 B, 127 markers) and 0x100498A0 (3993 B, 53) both carry
   the photo trio, so they will land at the same 34-diff park. 0x100498A0
   also has an unsolved itoa/strupr/BrStrGet/sprintf fill loop feeding the
   selector - worth reading when the photo1 window breaks, not before.
 - The photo1 window now gates FIVE functions, 18,395 B. Second dead probe
   closed: naming the two derived ints (`x2 = xi + 0x7f; y2 = yi + 0x21;`)
   before the three stores - literally the original's instruction order  - 
   also stays at 34. It needs a fresh idea, not another permutation.
 - 0x10046E70 still parked on its constant-register fork.


**2026-09-03 (session 13) - the slots class is now four functions deep and
the model is settled.** 0x10054A30 BrSlotAdd (999 B) BYTE-EXACT and
0x100553B0 BrSlotScrollStep (1453 B) transcribed to +3 bytes. Everything
this class needs:

- The record array is based at `this` ITSELF: record n at `this + n*0x438`,
  with its own polymorphic object at +0x2C inside it, so the owner's header
  fields (+0x04..+0x20 function pointers, +0x18 flags) and record 0's first
  0x2C bytes are the SAME storage. A `Rec aRecs[100]` member cannot express
  that and the layout assertions will fail; use raw offsets off a `char *`,
  as 0x10054E20 does. The count is `wCount` at +0x1A92C, and the readout
  block 0x10054730 seeds runs +0x1A92E..+0x1A9D0.
- The original REBUILDS the whole `*135*8` index chain before every single
  store. Spell the count in each statement; hoisting it into a local or a
  record pointer collapses the chains and rewrites the function.
- `strcpy`/`strcat` are /O2 intrinsics (inline `rep movs`); `strncpy` and
  `_stricmp` are import calls. Two copy arms sharing one `and ecx,3 /
  rep movsb` tail is VC5 cross-jumping, not one copy.
- A divisor guard reads `if (d <= 0)` on an UNSIGNED, not `if (d == 0)` --
  the original's test is an unsigned relational (`ja`), and the equality
  spelling was 0x10054A30's entire one-byte residue.
- In 0x100553B0 the three input probes are FLAT guarded blocks, not an
  if/else-if chain: when a probe succeeds but the 0x200000 flag is set the
  original still falls into the NEXT probe. Inside each, the long arm is
  the `if` and the one-line arm follows with its own `return` -- an
  `if/else` with a shared return after it duplicated the epilogue and cost
  130 bytes.

Still untouched on the strong list: 0x1000C4E0 (1246 B, new to the screen),
0x10059410 (939 B), 0x100541B0 (196 B, expected to park on the SIB byte).
`gen_cpptwin.py`: 0 twins again, twice (three times counting the
menu-builder lane).

 **0x1004ABE0 was on this list and is NOT untouched - it is BYTE-EXACT**
(menu-builder lane, same day; it was the smallest carrier of the photo
block and is now the reference for it). See the photo-block section higher
up in this file. Two sessions ran in parallel, so screen against
report_cpp.csv before trusting any "untouched" list here, including this
one.


## 2026-09-03 (session 14) - THE STRONG LIST IS EXHAUSTED. Screened, not guessed.

`tools/brally/cpp_screen.py` (run it under `.venv/bin/python3` - it needs capstone)
reports **22 strong of 174 screened**. Every one of those 22 is now either
byte-exact or carries a dead-probe list in its TU header. Verified row by row
against `src/brally/core/cpp/<VA>.cpp` + `build/brally/win32/match/report_cpp.csv`:

- 0x10054070 BrUiTick (86 B) is at **4 diffs, register-blind 0** - a scratch-pair
  rotation in the delta computation. Its recomp "96 vs 86" is PADDING, not
  extra code; the function body is 86 bytes. T3a, parked, do not reopen.
- 0x1000C4E0 BrRippleApply (1246 B) is the largest and looks untouched in
  triage - it is not. Three mapped causes and a flags sweep in its header.
- **0x100541B0 (196 B) is the only strong member with NO TU at all**, and it is
  the third glyph walk: its two siblings 0x100540D0 and 0x10054280 are both
  parked at exactly 1 diff on the SIB base/index order, so expect a third park,
  not a match. Take it only to close the family's bookkeeping, or after the SIB
  wall breaks - 0x100540D0's header lists ~20 dead spellings.

So: **the family is done as a source of countable matches.** `claim 5` on the
tagged pool returns nothing, and every SHAPE row in `triage.py` is parked.
The productive lane from here is the port's GLOBALS-STRUCT PARAMETER class  - 
see [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md), which now carries the one-line
call-site screen that identifies it without reading any source. Three
particle-step functions fell to it in this session, two byte-exact.


## 2026-09-03 (lane d29628ed) - THE FAMILY IS DRY, confirmed by re-screen

`tools/brally/cpp_screen.py` (needs `.venv/bin/python`, capstone) lists **22 strong**
rows. Every one is now matched, filled-and-parked with its own dead-probe
list, or carries the photo trio and inherits its 34-diff park. The single
untouched member is **0x100541B0 (196 B)**, and 0x100540D0's header already
predicts its residue (the SIB base/index park). 0x10054070 (86 B) is likewise
parked at 4 diffs on a scratch-pair rotation, and 0x1000C4E0 / 0x10059410  - 
listed as "untouched" in the session-13 notes - are NOT: the first is a C++
row at 782 diffs, the second is a C-lane row in br_uinav.c.

Standing order from here is SHAPE targets from `tools/brally/fnmatch/triage.py`, but
run `tools/brally/claimcheck.py` FIRST - see [unswept-tu-bookkeeping-class](../traps/unswept-tu-bookkeeping-class.md).


## 2026-09-05 (session 20) - the SAVE-SLOT TRIO is the live seam, not the vcall list

`cpp_screen.py` strong list confirmed dry again (0 twins from gen_cpptwin, 4
runs). The paying seam was the untagged cdecl siblings AROUND the family:
the record lists' save-slot callbacks at 0x1003B130..0x1003BDE0 plus the
save-file writers/readers at 0x100695C0..0x1006A080. **+6 byte-exact:**
0x1003B580 / 0x1003BCA0 (probes, src/brally/core/menus/br_saveprobe.c),
0x1003B350 / 0x1003BAC0 (name commits, br_savename.c), 0x10069DE0 (ghost
writer, src/brally/core/settings/br_ghostsave.c, first compile), and 0x10008AB0
BrPodOpen in the C++ lane (push imm to a this-in-ecx callee - C twin retired).
Parked, each with a dead-probe list in its header: 0x1003B6D0 (one frame
slot), 0x1003BDE0 (register pair), 0x1003B130 (7 B schedule), 0x100695C0
season reader (open-block layout + `bool` return → C++ front end),
0x10039620 (C++ TU, 572/563, one cross-jump asymmetry).

Levers that decided these (all in docs/brally/VC5-IDIOMS.md): extern arrays not
literals for scanned strings; if/else + ONE return so saves sink past a
guard; header pointer and display name declared as ONE struct so a reload
stays below a strcpy tail; frame layout is size-sorted with spilled scalars
at the bottom regardless of spelling; block-scoped locals SHARE a slot.

**Still untouched in this seam:** 0x10069A80 ghost reader (828 B, draft
read, same shape as the season reader - expect the same C++ residue),
0x1006A080 the five-way loader (641 B). 0x10039620's twin question and the
0x1003B6D0 slot are fresh-idea parks, not permutation parks.
