# N64 paintclick locals lineno

*Recorded 2026-10-05.*

> BrPaintClick 0x8024C184 T4 2026-10-05 (d52546cd, 1005 -> 0): as1 schedules ties by SOURCE LINE (fold statements onto one line); args loaded straight into a0/a1 = an in-place local with parameter preference (available=a0 only); reusing an existing local (i, r) joins its web and changes colour order; nested call in args = cfe frame temp

BrPaintClick (src/tgrally/menus/paintclick.c) went 1005 -> 0 on 2026-10-05, commit d52546cd, image gate 670/0. Hand transcription only.

**Levers, each diagnosed before the edit:**
- **as1 scheduler ties break on the source line number, lowest first** (workbench field-guide lever 33). Read the decisions with `cc -Wa,-R` (trace on stdout, the object is unchanged). If two adjacent instructions issue in the wrong order with allocation and count exact, put both statements on one physical line. Used three times here: the anchor if/else, the plot's x/y shifts, and a hoisted load that took the line of the r statement (fixed by splitting `r = sqrtf(..); r >>= 2;`).
- **An arg loaded straight into a0/a1** (`lw a0; subu a0,a0,t7; sra t8,a0,2; move a0,t8`) means a local updated in place: `x = D110.x; x -= DB94.x; x >>= 2; f(x, ..)`. uopt gives a web copied into an argument register `available0` = that register only (parameter preference). ugen never targets a leaf inside an expression at the destination; only whole webs do.
- **Colour order needs a lower-priority web:** a single-block web with 5 occurrences (5.0) always beats 3.0 webs. Reusing an existing local whose webs merge (`i = D_8028DAC0` joined the i loop web and took t0; `r = .w * 2` took t1 after the x/y webs) gave the ROM order. Try the function's existing locals for the role (w h r t x y), one hypothesis each.
- **Ring order with no fitting FIFO = a different statement structure.** Model the ugen ring (pop head, append on free, only ugen-owned temps) and brute-force the ring and statement orders. If nothing fits, the tree shape is wrong. Here `w = (A - B*4) >> 1; x = base + w` fixed operand order and ring together.
- **Frame one word too big with every local's home matching:** a cfe temp from a call nested in another call's arguments (`f(.., (int)sqrtf(..))`). Hoist it into a local.
- `n64alloc force` to the suspected ROM colouring proves the target before searching for the source (it compiles the tree, so install the draft first).

Related: [n64-ctlaibody-const-spelling-2026-10-05](n64-ctlaibody-const-spelling-2026-10-05.md), [n64-m2-643a5c-levers-2026-10-05](n64-m2-levers-2026-10-05.md).
