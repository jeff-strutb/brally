# Session 2026 09 13b cpp eh lane

*Recorded 2026-09-13.*

> Second '20 rows to T4/T3' ask on 2026-09-13: 9 delivered (5 byte-exact C++ EH ctors/senders, 4 T3 certifications); the C++ EH constructor class lands first-pass; the certification-only lane; end-of-TU placement inert on every probed wall

**Project lead ask (2026-09-13, second time):** "pick 20 T1/T2 rows, T4 or T3, no
excuses."  Delivered 9 of 20 in ~90 min; the project lead checked in after an hour.

**Byte-exact (5), all in the C++ lane, none past 3 probes:** 0x10004FD0 /
0x100051C0 BrNetSendCarState[Delta] (EH twins, 360/368 B), 0x10040B10
BrMenuObjCtor (478 B, first compile), 0x10054610 BrTextListInit (218 B,
first compile -- was a C T2 at 160/218 for weeks), 0x10041B60 BrOptObjCtor
(309 B, first compile; was a C T2).  Mappings on the tail of
docs/VC5-IDIOMS.md ("C++ EH constructors and `new` sites").
**@t3 (4):** 0x1006D0B0, 0x1003A140 (cpp), 0x10002580 (fresh T1 -> T2 ->
T3 same session), 0x1001CA30.  Found with `t3.py --qualify --all` ("owe
passes") plus `--qualify` on every uncertified >400 B T2.

**Why the C++ class pays:** a C row whose original has an EH prologue
(`6a ff`), a vtable store, or a `call 0x10074800/0x10074572` (ehvec ctor /
operator new) can NEVER close in the C lane -- rc is 40-60 B short by
construction.  Write it as a C++ ctor/method with the class laid out by
offset, member initialisers in declaration order, memset fills, and it lands.
Screen: disassemble every T1/T2 original for those three tells (script in
the session scratch; 30 s for the whole pool).

**Walls confirmed today (do not re-run):** 0x100271F0 (31 probes, the lone
`and eax,0xff`), 0x1006CE80 (register-byte widen, 12), 0x10039D20 (7, order),
0x10060A30 (12, base folds into the trailing add), 0x1001CA30 tail (17),
0x10002580 (25: the original's THREE zero registers -- typing, chains,
locals, inline helpers, arrays, structs all fold to esi; corpus MISS),
0x1003D7D0 (cpp, block layout, 9 shapes + /O1 /Os), 0x100393C0 (3 goto
shapes), 0x10041180 (flag-first arms: worse).  **End-of-TU placement was
inert on all 20 rows it was tried on** -- the file-position lever is not a
general lever.

**Bookkeeping learned:** `sbs.py` prints RECOMP on the LEFT and the
original on the right (I read it backwards for the first hour).  fn.py's
`mov [A], I` vs `mov [A], A` on a reloc'd store with addend 0 is the
deferred masking artefact, not a residue.  cl /Gi runs in parallel collide
on vc50.idb -- run cpp_score variants sequentially.  slice3_44.c had been
left uncompilable by a 2026-09-13 dossier edit (a `*/` closed early);
fixed in 4d200a6.  Retire a C twin by replacing its `@implements` line with
`/* port-only body; Glide match is src/core/cpp/<VA>.cpp */` (no tool).

**Next time:** the remaining fresh C++ EH T1 is 0x1002F790 (2517 B packet
receiver, 60 calls) -- large but the class is deterministic.  Remaining C++
lane diffs with dossiers: 0x10062B80, 0x10059350, 0x10058E20, 0x10041180,
0x10054E20, 0x10039990, 0x10039620, 0x10058680, 0x10069A80, 0x10007750.
Related: [session-2026-09-13-twenty-rows](../log/session-2026-09-13-twenty-rows.md), [cpp-lane-class-cracks-2026-09-12](cpp-lane-class-cracks-2026-09-12.md),
[file-position-regalloc-lever](../levers/file-position-regalloc-lever.md), [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).
