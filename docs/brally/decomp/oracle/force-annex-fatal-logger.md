# Force annex fatal logger

*Recorded 2026-09-22.*

> Permanent diagnostic - force-annex mechanism spills a byte-locked (T4) function into the .t3x annex so it can grow; used to make BrLogFatalPrintf log its fatal message before exit(1)

**2026-09-21: permanent clean-exit crash localizer.** The game's fatal path
(`BrLogFatalPrintf` 0x10008EC0) formats a message and `exit(1)`s WITHOUT printing
it - so every clean crash-to-desktop (no Win98 fault dialog) gave zero info. Made
it append the message to `brally.log` (game's cwd/install dir), flushed via
fclose, before exit. Now any clean-exit crash self-names its failing check
(POD/track/asset validation, asserts); page-fault crashes still give the Windows
address dialog. Between the two, crashes self-localize with no re-instrumentation.

**The wall this solved:** byte-matched (T4) functions sit in fixed slots - grow
one and the in-slot lane REVERTS it to the original bytes (verified: my logging
vanished from the placed image). Only transcribed (T3) bodies can grow (they
annex). MSVC5 has no `/Gh`/`_penter` auto-hook (MSVC6-only), so no free
whole-program trace either.

**The mechanism - `_force_annex` in image_build_t3.py + config/brally/force_annex.csv**
(`va,file,opt,symbol,name`): spills a listed MATCHED function into the `.t3x`
annex like an over-slot T3 body - grown body placed whole in the appended
section, 5-byte `jmp annex` thunk left at the original VA. Resolution: rel32
calls to known funcs via load_maps fnmap (+ a small known map: BrOperatorNew =
0x1007DFE0, a static-CRT helper not in the maps); `__imp__*` DIR32 slots via
`t3b_env.image.imports` (IAT slot VAs). Its thunk VA is added to `t3_vas` so
the deliberate jmp isn't counted as a T4 regression. Verify: placed 0x10008EC0 =
`jmp .t3x`, annex body's calls resolve (fopen/fwrite/fclose/vsprintf/exit/BrOperatorNew).

**Logging must add NO new .rdata** (fixed image has no room): build strings on
the STACK char-by-char (not literals - /O2 would hoist a literal into .rdata),
and use only ALREADY-IMPORTED CRT calls (fopen/fwrite/fclose/vsprintf/exit are
in BRGlide's import table; fputs/WriteFile/CreateFileA are NOT). Compile-check
the object: no new .rdata section, externals only imports + known funcs.

**BR_TRACE=1 (comprehensive execution tracer, 2026-09-21).** collect_t3 routes
EVERY transcribed function (both lanes, size>=19) through the annex with a stub
at its VA, and `BrDiagTrace` (src/brally/core/diag/br_trace.c, __stdcall, stack strings,
imported fopen/fprintf/fclose) appends the VA to brally.log. Any crash's last log
line = the function it died in; page faults still give the Win98 address. The
stub MUST be register/flag-transparent: `pushad; pushfd; push VA; call sink;
popfd; popad; jmp body`. A bare `push VA; call sink; jmp body` CLOBBERS the
caller-saved regs (eax/ecx/edx) that a thiscall/fastcall body reads at entry --
BrGlNavPoll (`mov esi,ecx`) faulted in the credits exactly this way, moving the
crash EARLIER and masking the real one. The tracer localized its own bug. Cost:
fclose-per-line (crash-survivable) makes a traced run slow; a full-trace build
annexes ~177 fns (~170KB .t3x) and takes ~20 min. Tracing is OFF without the env
flag, so the shipping image is unaffected. Decode brally.log VAs with
report.csv+report_cpp.csv (va->name).

Reusable for any future byte-locked function that needs diagnostic logging: add
a row to config/brally/force_annex.csv and the stack-string/imported-call edit. Related
build-bug crash classes: [jump-table-override-crash-class](jump-table-override-crash-class.md). brally.exe is a
thin launcher (28 fns: INI + LoadRallyMain + WinMain) - the game (RallyMain) and
all crashes live in BRGlide.dll, so EXE logging can't see them.

**2026-09-22: CRASH-EIP FILTER - the exact fault address, not just the last
traced fn.** Per-function BR_TRACE only names the last *traced* fn entered; a
crash in a byte-exact (T4) or leaf body leaves the log at the previous T3 line
(verified the whole texture-install cluster - BrTexInstallRecords 0x100299A0,
FUN_10027710, BrTex3dRecInstall 0x10027850, br_tex3d_append 0x10027A10,
FUN_10028200, FUN_10023d70 - is byte-exact/A5-equivalent AND placed globals
match, so the Quick Race crash is NOT in them). Fix: br_trace.c now installs a
top-level `SetUnhandledExceptionFilter` on every trace line (idempotent, no
guard var needed). SetUnhandledExceptionFilter is NOT imported, but
GetModuleHandleA + GetProcAddress ARE - resolve it at runtime
(`GetProcAddress(GetModuleHandleA("KERNEL32.dll"),"SetUnhandledExceptionFilter")`,
both strings stack-built). `BrGlCrashFilter(EXCEPTION_POINTERS*)` reads
ExceptionRecord (+0x00 code, +0x0C addr, +0x18 AV target) and CONTEXT (x86: Edi
0x9C Esi 0xA0 Ebx 0xA4 Edx 0xA8 Ecx 0xAC Eax 0xB0 Ebp 0xB4 **Eip 0xB8** Esp
0xC4), logs `C <code> <eip> <faultaddr> <8 GP regs>`, returns 1
(EXCEPTION_EXECUTE_HANDLER → clean terminate, no dialog). So a page fault
self-reports its EXACT EIP even when 86box eats the Windows dialog.

Build-side lever: the crash filter is a SECOND annexed function that the sinks
reference by DIR32 &BrGlCrashFilter - an intra-object ref the annex sink could
not resolve (imports only). Extended `_annex_sink` in image_build_t3 with a
`_known_syms` map: annex BrGlCrashFilter FIRST (imports-only, no deps), record
its VA, then annex the sinks resolving _BrGlCrashFilter@4 from the map. Import
names carry no @N so the existing `.split('@',1)[0]` fallback already resolves
GetModuleHandleA@4 / GetProcAddress@8. MSVC5 /Gy puts each fn in its own .text
section; each annex blob is extracted [sym, next-sym-in-section). Verify in the
placed image: sink ends `push <filterVA>; call eax` after the two IAT calls.

**BR_TRACE_ONLY=<comma VAs> (2026-09-22): install the filter WITHOUT per-fn
spam.** Full BR_TRACE fopen's every T3 entry - a per-texel loop (BrTex3dExpand →
BrTex3dTexel 0x100271f0) then looks like a hang on emulated disk. BR_TRACE_ONLY
stubs only the listed VAs; the filter installs from any traced line, so listing
one early NON-hot T3 fn (e.g. the surface blits BrSurfBlt24 0x100011c0 /
BrSurfSetColourKey 0x100014a0, which run on the loading screen) arms the filter
at boot and leaves hot loops native-speed. NOTE: only T3 (diff) fns can be
stubbed - a T4/match fn (e.g. BrGlRaceStart 0x100628b0) is in the T4 backbone,
never routed through collect_t3, so it silently won't stub.

**PROVEN 2026-09-22 - first real catch.** The filter fired on the Quick Race
crash: `C c0000005 1006fe54 ffffffff 1006fe54 0 10af1208 7 0 98 10af2030 0
63f4a8` = AV at EIP 0x1006fe54 (BrEntReset, byte-exact) doing `rep movsd` from
esi=eax+0x98 with eax=0, i.e. entity(0x10af1208)->0x29c4 == NULL. Format is
`C code excAddr faultAddr eip eax ebx ecx edx esi edi ebp esp` (excAddr==eip;
a 2nd cascade `C` line inside KERNEL32 bff9dfff is the OS teardown, ignore it).
Root, traced back: g_BrCarCount(0x100b2f04)==0 ← g_226A4C branch in BrRaceStep
case 6 had its two if/else bodies SWAPPED (set count 0 vs 1 backwards) - a real
transcription bug in a certified-T3 giant on the oracle-unseeded Quick Race
path. FIXED 2026-09-22 (commit ed1768a8): `if(g_226A4C==0){count=1}else{0}`.
The crash-EIP filter is the general tool that ended the guessing. See
bracestep-wall.

** STALE CPP-SWEEP OBJ - the fix silently didn't ship (cost a cycle).** The
C++ lane in image_build_t3.collect_t3 READS cached `build/brally/win32/match/obj_cpp/
<base>_sweep_<VA>_<ti>.obj` and NEVER rebuilds them from source (recompile flag
not consulted there; the T4 "0 reused, 354 rebuilt" line is the C backbone
only). After editing a .cpp lane source you MUST regenerate its sweep objs
(cpp_score.compile_cpp(src, 'sweep_%08X_%d'%(va,i), DEFAULT_OPTS[i]) for i in
0..3) or the image places the OLD bytes. Always verify the PLACED image after a
cpp-lane edit (disasm the VA / annex body) before shipping. Recurring class,
same family as [image-gate-builds-what-it-grades](../traps/image-gate-builds-what-it-grades.md).

**Branch-inversion audit (2026-09-22): the class is CLEAN on the Quick-Race
path.** Built a detector (scratch/branch_audit.py) over the 82 T3(diff) fns
reachable from BrGlRaceStart/BrRaceStep/BrCarStartInit: align register-blind
(CC mnemonics folded to one token so je/jne pair), flag a conditional branch
whose polarity is opposite the original on a compare-to-ZERO or same-IMMEDIATE
(uncompensable - a legit T3 flip swaps two-reg operands), and DISCRIMINATE real
from compensated by comparing the value-CLASS (zero vs nonzero reg) written on
the predicate-TRUE path (reorder/register invariant). Validated: flags the
buggy BrRaceStep @0x10019c43 (orig-true N,N vs ours Z,Z), silent on the fixed
one. Result across all 82: only BrRaceStep. A parallel CONSTANT-mismatch pass
is NOT safely actionable - dominated by reordered-but-identical literals,
stack strides, and A5-equivalent float tuning; do not batch-fix it.
