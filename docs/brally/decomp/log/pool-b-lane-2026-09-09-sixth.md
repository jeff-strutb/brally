# Pool b lane 2026 09 09 sixth

*Recorded 2026-09-09.*

> Sixth 2026-09-09 session - Pool B 253-383 B band; 2 byte-exact + 1 T2 park; two new control-flow levers; 0x10023B70 scoped next

Pool B lane (token fd1bcb5e, released) on the treemap's unfiled swaths, smallest-first from a 19-VA claim (253 - 383 B, all screened clean: no EH, no fxch).

**Landed:**
- **0x100284E0 BrTex3dMakeCurrent BYTE-EXACT** (br_glidestate.c). Lever: `switch (mode) { case 1: ... default: ... }` - a lone case-1 switch lowers to `dec/jne`; `if (x==1)` spells `cmp,1` (+2 B). On the idioms tail.
- **0x100704E0 BrJoyScanAny BYTE-EXACT** (br_dik.c, joystick press-anything scan for the binding UI; ghidra draft MISSED the 0x8200 lY arm and the real 0x8000-byte stack DL... no - 0x8000 frame belongs to 0x10023B70; here the frame is 0x110 DIJOYSTATE2). Lever: interior-exit scan loop must be `for`, not source do-while - do-while gets ROTATED with a peeled first load; `for` stays the unrotated do-while sharing 0x80 mask/bound in ecx. On the idioms tail.
- **0x1005F580 BrRankAssign T2 PARKED** (br_rank.c, qsort ranking via BrRankCmpKey). Size- and insn-exact 259/259, 92/92, regnorm 0+0, Gate 0+A PASS, 1 masked region; needs only Gate B counted passes. Winning spelling: explicit key CURSOR + idx spelled `local[n*2+1]` (mixed). Residue: 4-register cyclic shift (orig allocates the derived cursor FIRST). Six probe classes dead - dossier in the file header. `@t4-pass ... probes 6 ... census no` recorded.

**Traps hit:**
- `char*/int*` written inside a C comment - the `*/` terminates it and the sweep reports MATCH 0/N with COMPILE ERROR. Grep new dossier comments for `*/` before committing.
- Rewriting config/filing.csv with Python `write_text` strips its CRLF endings → a 980-line phantom diff. Insert rows with `read_bytes/write_bytes` keeping `\r\n`.
- A parallel session ran `git pull --rebase --autostash` mid-lane: HEAD moved, br_rank.c/br_dik.c reverted on disk mid-probe (fn.py "symbol not in obj"), then came back after their abort. If a probe suddenly loses the symbol, `git log` FIRST - don't debug the tool.

**Next in the claimed band (released, re-claim first):** 0x10023B70 (273 B) scoped: frame-end DL flush in the 0x1001xxxx/0x1002xxxx swath. Facts read off the asm: `mov eax,0x8000; call chkstk` = an 0x8000-byte local `int dl[0x2000]` (declare it, VC5 emits chkstk itself); the function TAKES ONE ARG read at [esp+0x800c] and passes it to the first BrGbiRun; then points DAT_106e7710 at the stack buffer, BrSetGlobal_ABB30(0x14) (0x100168B0, br_text.c), BrTextDraw(&DAT_118ee590,0,0x3c) (0x100168C0, decl in br_framedrive.c), emits end cmd 0xb8000000 via cursor post-increment, BrGbiRun(dl), calls funcptr DAT_106b7ab8 (decl in br_uiscreen.c), time from 0x1006E280, then the FPS ring: gate 0x100b4c2c, count 0x100b4c28, samples 0x10b73348; the fill loop `for(i=0;i<count;i++) samples[i]=delta` must emit `rep stosd` (VC5 fill-idiom), gate=count after fill, then `gate++; if (count<=gate) gate=0; samples[gate]=delta`. File into br_gbi.c near BrGbiRun. grAlphaCombine(3,8,1,1,0), grAlphaBlendFunction(4,0,4,0) + 4 dword globals (105d17a8=4,105d1758=0,105ccfd4=4,105ccfe4=0), grDepthMask(1), dword store 105ccfe8=0, BrTexQueuePop (0x1006E220).

Remaining claimed-band VAs (see survey.csv): 0x10060A30, 0x10004E00, 0x10036300, 0x10001000, 0x1005A500, 0x10053D20, 0x10036510, 0x10058AF0, 0x1002E186, 0x10058E20, 0x10073994 (dead tier), 0x10027B60 (7 callers, also called from br_tex3d.c:1246), 0x1005C560, 0x1006B240, 0x10035C50. All screened clean 2026-09-09.

Related: [resume-state](resume-state.md), [t1-intake-lane-method](../triage/t1-intake-lane-method.md), [parallel-session-clobber](../traps/parallel-session-clobber.md), [filing-py-drops-rows](../traps/filing-py-drops-rows.md)
