# Dpappmsg callmerge zeroweb

*Recorded 2026-09-25.*

> 0x10009010 BrDpAppMsgHandle (C): un-merged same-callee call tail SOLVED via a linker-folded twin callee (VC5 tail merge compares callee symbol; proven by C2 RE + IL surgery). Zero web still open and blocks T3 (A3) and T4. Also: how to capture/edit VC5 IL.

State 2026-09-25: commits 88088a98 + 27301a4b, T2, 908/913 B, 268/270 insns, only
residue = one zero web. Full dead lists live in the function header.

**Residue 1 SOLVED (call tail merge).** C2.EXE (RTM) tail merge = FUN_00434172,
tuple equality FUN_004332a4: same opcode + type(&0xfff) + operand lists (callee
symbol entry among them). Front end gives EVERY same-name spelling one symbol
index (casts, block/typed/implicit decls, wrappers...). Only a different NAME
un-merges -> original case 8 called a twin (identical body) that LINK folded
onto 0x10036A30. LINK 5.0 folds identical COMDATs under plain /OPT:REF (tested in
build/match/t3d/linklab). Source now calls `BrChatLineFinishTwin`; reloc_learn
maps it from the original call site. Same class likely explains 0x10059350 and
0x1000A110 "original doesn't cross-jump" residues -- re-triage them with a twin.

**Residue 2 OPEN (zero web)**, blocks T3 too (A3: imm vs zero-reg rows don't pair).
Trigger: entry pText store + `i = 0` side counter + `&pText` in case 0/1 + ANY call
in the player loop. Dead: loop/counter forms, types, storage, decl order (720
perms), preamble 0..6000, IL constant type bytes, IL line numbers, symbol-attr bit
flips, inline helpers, base-pointer index loops.

**Method (reusable):** IL capture = wrapper C2.EXE in build/match/t3d/tc_cap
(real one renamed C2REAL.EXE); cl passes args via env MSC_CMD_FLAGS (`-il <base>`),
files <base>ex/gl/in/sy. Rerun backend on edited IL by setting MSC_CMD_FLAGS
yourself. Call record in IL: `26 <sym16> 3e <type> <flags>` + args (`..55 41`) +
`4c 4b`; flags 0x10 stdcall, 0x04 fastcall, 0x14 thiscall. Ghidra project of C2
at build/c2re (Java post-scripts; this Ghidra has no Python).
See [cross-jump-wall-final-return-order](cross-jump-wall-final-return-order.md).
