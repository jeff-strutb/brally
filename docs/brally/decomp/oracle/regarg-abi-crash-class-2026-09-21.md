# Regarg abi crash class

*Recorded 2026-09-21.*

> Hardware crash class: T3 caller misdeclares a thiscall callee, leaves ecx dead at the call. Two found by an image-wide screen (0x100590D0→0x100597C0, BrCarStep→0x1002F640); screen tool in scratch, worth keeping

2026-09-21: first real-hardware run of the T3 image (Win98) crashed at
0x100597C0 (`mov eax,[ecx+0x50]`, ecx=0). Root cause class: **a T3-placed
CALLER misdeclares a register-arg callee, so ecx/edx is dead at the call.**
Two instances, both fixed in commit ce085b88:

1. **0x100590D0 BrSub100590D0** declared 0x100597C0 `__stdcall`; it is a
   2-arg thiscall, `this` = the pointer at 0x10AC61E0. The cert had recorded
   the original's ecx load at the three test sites as *register colouring*  - 
   it was the argument. **A "colouring" difference on ecx/edx immediately
   before a call is ABI-load-bearing until proven otherwise.**
2. **BrCarStep 0x1006F170** called BrBitLatchTake (0x1002F640) as cdecl
   1-arg: no `this` AND `add esp,4` after the callee's `ret 4` (double pop).
   Its A5 verdict was **EQUIVALENT - falsely**: no seed set the `&0x10` mode
   bit, so the broken arm never executed ([lockstep-oracle-arbitration](lockstep-oracle-arbitration.md)'s
   degenerate-seed hazard, now seen in the wild).

**The screen** (found both, zero false negatives known): for every reference
call target, disassemble the entry; the callee is register-arg if ecx/edx is
READ before written, excluding `push ecx/edx` (VC5 frame slot) and
`xor r,r` (a def). Then flag call sites in T3-differing regions where the
placed body does not write that register in the preceding window. 47 raw
hits → 2 real after those two exclusions and byte-identical pass-throughs.
Script: regarg_screen.py (session scratch 2026-09-21; recreate from this
description if gone - ~100 lines, capstone + PE .text walk of
reference/brally/orig/BRGlide.dll vs build/brally/win32/image/BRGlide.T3.dll).

**Second sweep (same day, commit 764c51a1) found a SECOND class: twin
placement.** Five VAs had a report.csv row from a C twin AND the certified
cpp transcription in report_cpp.csv; the cpp lane's `not in rows` filter
ceded the slot to the twin. 0x10062B80 shipped cdecl/`ret 0` where the
reference is thiscall/`ret 0x10` (16 B stack leak per call); 0x1004AEE0
shipped 550 B short. Fix: the C-lane row selection requires the @t3 cert
to live in the row's OWN file (TWIN-DEFERRED print). Also: a bare object
declaration inside a C++ `extern "C" {}` block is a DEFINITION - two cpp
TUs grew private .bss copies of image globals; always write `extern`
(the resolver's `/* 0x<VA> */` reader also needs the keyword). The rigorous
screens (abi_screen2.py, scratch): A = callee ecx/edx live-in via CFG
walk vs caller writes-since-last-clobber; B = `add esp` right after a
`ret K>0` callee; C = placed ret-K vs reference ret-K per T3 body (use
EXACT function bounds - +32 spillover false-positives).

**The screens are now PERMANENT gate stages (commit 0f504e29):**
`tools/brally/t3abi.py`, run by `image_build_t3.py` on the final placed bytes;
any flag fails the CONTRACT-VALID gate, so a new T3 function cannot ship
a convention error. Acceptance is reference-derived (pass-through = the
reference caller of the same target in the same function also leaves the
reg untouched) - no site allowlist. Annexed bodies are screened from their
.t3x bytes, not the thunk span - the scratch version missed a THIRD
instance because of exactly that: br_ghoststep.c called BrRaceGateStep
(0x1005FF00 thiscall) as void(void); its A5 EQUIVALENT was blind because
the helper was BLACK-BOXED. Black-boxed callees are an A5 coverage hole  - 
the ABI screen is the check that covers them.

**THE ACTUAL 0x100597C0 CRASH (found third, commit e0f88276): TRUNCATION,
not a call site.** Both hardware dumps had a KERNEL32 return address at
[esp] - the callee was never CALLED; BrGlNavPoll (0x10059410) was placed
with its last 4 bytes cut off under the `TRUNCATION ADMITTED` /
config/brally/t3_slot_ok.csv escape, its third arm lost `add esp,0x10; ret 4`,
and execution FELL THROUGH the nop padding into 0x100597C0 with ecx zeroed
by the preceding 0x10059060 call. **Read the return address in a Win9x
fault dump FIRST: a return address outside the module means the faulting
function was reached by fall-through or callback, not by the in-module
callers.** The admission escape is deleted - over-slot bodies annex or
block, never truncate (11 functions affected; all now ship whole).
Screen D (t3abi.py) is the permanent guard: CFG walk of every under-slot
span; any reachable path past the span end fails the gate; data tails are
unreachable and never flag; teeth proven against the shipped bytes.
A5-EQUIVALENT-on-partial-seeds is NEVER evidence for shipping missing
bytes - the two coverage holes so far: black-boxed callees, unreached arms.

Answer to "are all T3s suspect?": no. 175 `@t3` certs; the class needs a
register-arg callee misdeclared at a call site, and the whole image now
screens clean. But UNCLASSIFIED-oracle T3s (88 of the cert lines) were never
behaviourally tested, and EQUIVALENT ones only cover arms the seeds reach  - 
hardware runs remain the only full-coverage oracle. Fix arm to reuse:
`__fastcall (void *pThis, void *_edx, unsigned arg)` with the dword arg as a
struct when it must stay on the stack ([thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md),
br_ctlinput.c:21, br_dik.c).
