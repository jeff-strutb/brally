# N64 loadsave uopt

*Recorded 2026-10-05.*

> BrLoadSaveScreen 0x80211D70 T4 2026-10-05 (65a3db79, gate 663/0): function-static state, compare operand order = uopt leaf vs node (read ucode via workbench capture), grader .rodata base-by-nearest-pair fix

BrLoadSaveScreen 0x80211D70 (src/tgrally/menus/loadsave.c) went T3 -> T4 on 2026-10-05 (commit 65a3db79; grader fix 338a316b; image gate 663 placed, 0 bytes differ). It is uopt-OPTIMISED (not over -Olimit); the old header said otherwise.

**Levers that closed it:**
- Screen state is FUNCTION-STATIC (uopt promotes a static whose address isn't taken as a dt8 register variable across calls; extern globals are only CSE'd loads). Statics in address order; .data initial values must match (D_80272554 = 1). An uninitialised file-scope global (D_80316420[2]) goes to .bss at offset 0 before the .bss statics.
- **Compare operand order is uopt's, read it from the ucode.** In the ucode uopt hands ugen, `equ` operands are ordered: plain variable leaf (int local) FIRST, constant second; a NODE (cvt of a u8 load/static, or any expression) SECOND, constant first. So a ROM `bne $const, $reg` means the compared value is a node, not a bare int local. `(mode | 0) == 2` (also ^0, *1, >>0) keeps the node for ordering and folds to no code. Unsigned casts make the constant a different constant web (at); uchar locals add andi (cvtl) unless the def is the load itself.
- Read ucode: `build/tgrally/ext/wbvenv/bin/decomp-workbench capture make tools/toolchains/ido53 DIR --phase ugen`, build with TGR_CC=DIR/toolchain/cc, then decode `before-8-*` with `decomp_workbench.ucode.parse_ucode` (loc records carry source lines; mt3 = register, reg 2=v0 3=v1 4=a0 5=a1 6=a2).
- Only 34 webs are coloured in this fn (n64alloc trace); per-block values ugen keeps aren't webs, so force-probing those finds nothing.

**Grader fix (n64build.section_base near=):** a pointer initialiser into .rodata now takes the base from the %hi/%lo pair whose addend is nearest its target. A file's strings and late float literals can sit apart in the ROM when the original file held more (loadsave's floats follow cpak strings), so the first pair placed the label strings wrong.

Related: [n64-giant-ugen-levers-2026-10-05](n64-giant-ugen-levers-2026-10-05.md), [n64-ido-trace-tooling](../toolchain/n64-ido-trace-tooling.md), [n64-a7-literal-mapping-2026-10-04](../oracle/n64-a7-literal-mapping-2026-10-04.md).
