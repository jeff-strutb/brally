# Crt corpus verdicts

*Recorded 2026-09-13.*

> CRT corpus (--corpus crt, 690/734 byte-exact) verdicts on the four unproven byte/string shapes - all four parked rows CONFIRMED walls, do not re-probe

The CRT proven-idiom corpus is live: `tools/brally/crtcorpus.py` scores vendored
DEVSTUDIO/VC/CRT/SRC against LIBC.LIB; `corpus.py find/show --corpus crt`
queries it. 690/734 C-source CRT functions byte-exact; retail flags
(`-Zelp8 -W3 -WX -GFy -GB -Gi- -O2`) and plain `/O2 /W3` give the IDENTICAL
match set - the extra retail flags are codegen-inert on this corpus. The 378
remaining LIBC functions are hand-asm (INTEL/*.asm), no C source to score.

Verdicts (2026-09-13, all in docs/brally/VC5-IDIOMS.md tail, commit 39bd6eb):
- **Dirty byte widen** (`mov dl,[m]; and edx,0xff`): proven 9× but EVERY
  site has `test r8,r8; je` between load and widen (MBCS `while(*s){ if
  (_ISLEADBYTE(*s++))`). No anchor-free form in 690 functions. The `& 0xff`
  mask-at-use spelling on signed char canonicalises back to xor+mov (fn.py
  crtmask, dead). 0x1006CE50 / 0x1006CE80 / 0x10062B80 stay walls.
- **`and eax,0xff` before `mov dh,al`** (0x100271F0): 0/690 miss.
- **Fresh `xor r,r` beside live zero** (0x10002580): proven only as
  loop-carried reassigned zero locals (openfile, _OPEN.C:65-67); that row's
  region is straight-line - wall confirmed, [[no re-probe]] per idiom file.
- **`rep movsd` + `stosw` tail**: zero stosw tokens in the corpus; the
  proven tail is `movsw` from `strcpy(dst, "literal")` inlining (ASSERT.C:152).

Register-blind index tokens ("R") give FALSE adjacency hits - x_ismbbtype's
byte load and its `and 0xff` are DIFFERENT registers. Confirm every corpus
adjacency with a register-aware disasm of the shipped obj
(match_diff.parse_coff_obj) before quoting it as proof.

Also: found the working tree carrying an uncommitted REVERT of committed
work (crtcorpus.py/crtlib.py deleted, corpus.py stripped) - restored from
HEAD via pathspec checkout; [parallel-session-clobber](../traps/parallel-session-clobber.md) applies to
tools/brally/, not just docs.
