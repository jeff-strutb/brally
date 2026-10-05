# Ext corpus

*Recorded 2026-09-13.*

> External same-compiler corpus (--corpus ext) LIVE, 1588/3956 byte-exact; x87 operand-role wall independently confirmed with a 116 B micro-TU laboratory function; supersedes ext-corpus-blocked-on-exe

**2026-09-13: the external corpus is LIVE.** `--corpus ext` in tools/corpus.py
(committed 2e17883, verdicts in docs/VC5-IDIOMS.md tail, 43f66af). Built from
the C2 project (madebr/ext) - **that name NEVER goes in our tree (PROJECT
RULE)**; in-tree everything is "ext", staged under `build/external/`
(git-ignored): `ext/` clone, `c2/CARMA2_HW_GOG.EXE` (sha256 9b896c2c…,
extracted from the project lead's GOG installer by my scratch Inno 5.5.0
extractor - the retail disc and 1.02 patch are BOTH SafeDisc-wrapped and
useless), and `extcorpus.py` (the builder: compiles each annotated TU with
their matching flags `/O2 /Ob2 /G5 /W3 /GX /ML /Gr /D...` via wine, scores
against the EXE with reloc masking + tail guard).

**Numbers:** 1588/3956 annotated functions byte-exact under OUR cl (RTM
7022; their project pins SP3 - indistinguishable again). 529 compile_fail
(missing DirectX-ish includes, never chased), 1829 diff (their unmatched
rows). Rebuild: `.venv/bin/python build/external/extcorpus.py` then
`tools/corpus.py build --corpus ext`. refcheck stays Glide-keyed (rule 0
checked after).

** THE PAYOFF - the x87 operand-role wall is now independently confirmed
with a laboratory:** their 3x4-matrix family sits on OUR wall with FULL
KNOWN SOURCE: 34Mul 20/404 off (param-homing order + B/C role swap), ApplyP
10/116 off (pair-swapped fld order), TApplyFV 106/107 (only the ret byte).
True source reproduces everything but the operand roles ⇒ scheduling
decision, not a spelling. **Micro-TU sweep target = the ApplyP shape (116 B,
source at build/external/ext/src/brender/core/math/matrix34.c), NOT our
big carriers.** Queries: all four of our parked runs (0x1006DD20,
0x10029D70, 0x1006D530, 0x1002A050) MISS in ext at every window; accumulate
tail `fxch st(2); faddp st(1); fxch st(1)` PROVEN = flat 3-term MAC macro
(already our spelling - no new lever, 0 rows tried, dead-lists cover every
proven construct).

**Gotchas:** corpus.py `find` with a 2-token pattern needs `--exact
--min-len 2` (search_longest's default min-len 3 silently searches
nothing). Ext hits cite as `ext:0xVA`; resolve with
`corpus.py show --corpus ext --va 0xVA --at 0xOFF`.
