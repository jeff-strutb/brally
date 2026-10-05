# Port build drift

*Recorded 2026-09-21.*

> Port build DRIFTS as functions are rematched. Green 2026-08-27 (136/136); re-broke by 2026-09-21 (br_inputpoll.c __declspec). Recurring pattern: matching changes signatures/decls without re-running ./build.sh, so guard Win32-isms behind BR_MATCHING_BUILD.

## RE-BROKEN 2026-09-21: one compile error, fresh drift

`./build.sh` stops at `src/core/controls/br_inputpoll.c:203`  - 
`__declspec(dllimport) short __stdcall GetAsyncKeyState(int)` is unguarded and
clang rejects `__declspec`. Guard it behind `BR_MATCHING_BUILD`/`_WIN32`. This
is exactly the drift class below: a function was rematched (BrInputPoll
0x100706D0 certified T3) and pulled in a Win32 declaration without re-running
the port build. The old `slice2_12.c BrFixPackS16Q15Neg` blocker is gone.
**Lesson stands: matching changes signatures/decls; re-run `./build.sh` after a
refile and guard Win32-isms.** See [goal-coverage-not-playability](../decomp/rules/goal-coverage-not-playability.md).

## RESOLVED 2026-08-27 (final): port build GREEN, 136/136 tests pass

All reconciled. Two durable lessons: (1) many port tests were written against a
PRE-MATCH decomp and drifted when functions were rematched to their real
(glide, byte-exact) form -- the net funcs forward via IDirectPlay4A::Send
(BrComCallLocked68) gated on g_brAA288C, NOT the old BrDPlaySendTagN model;
BrStrGet inlines its lookup; and the glide funcs that drive flat globals
(BrFadeTick, BrInputIsDown) vs the D3D struct the test used are the recurring
cross-binary split. (2) x87 80-bit results (test_slice3_44) differ 1 ULP on the
non-x87 port target -- accept both; exact bytes are a matching-build property.
Also fixed a real latent bug (g_br5CCB5C undefined) and a doc error (the f34
ramp freezes on g_br5CCB5C, not g_Br6909B4 -- verified vs orig 0x1002F54A).
Quarantined (verified-impossible on 64-bit / unverified NaN codegen):
test_rca_fixup, test_fade_nan's BrFadeTick block. Everything else GREEN.

## UPDATE 2026-08-27 (earlier same day): port build GREEN, 129/136 runtime tests pass

The compile error at test_slice2_16 was masking everything after it. Fixed the
3 compile errors + latent LINK errors (they only surfaced once the compile
error cleared): `g_br5CCB5C` was declared extern and defined nowhere (real bug,
now defined in slice2_19.c); several tests link the real `br_netmsg` funcs or
carry stand-ins instead of dragging big link closures. **`./build.sh` now exits
0 (compile + link + host all green).**

**7 runtime failures remain -- domain reconciliation, NOT mechanical:**
test_slice4_52/53, test_slice6_70 (the real matched net funcs are gated on slot
globals / g_brAA288C the OLD test fixture didn't feed -- fixture must be rewired
to the real function's gating), test_slice3_45 (pad-input reads g_BrPadModeBytes,
needs the real mode table), and pre-existing drift untouched by the link work:
test_br_dl (opcode-skip), test_slice3_44 (a matched float-matrix value changed),
test_slice2_19 (g_pad.f34). Each needs deciding whether the TEST or the MATCH is
the source of truth. QUARANTINED with reasons: test_fade_nan's BrFadeTick NaN
block (D3D unordered-compare semantics vs glide plain-C, unverified) and
test_rca_fixup (glide 32-bit-pointer record model, unrunnable on 64-bit).

## Port build vs test drift (found 2026-08-27, original entry)

`./build.sh` compiles the PORT target (clang over `src/core` via
`find src/core -name '*.c'`). Two facts, both verified:

- **Port SOURCE builds clean.** The integration files are invisible/guarded:
  `src/core/cpp/*.cpp` (`.cpp` isn't matched by `-name '*.c'`), `src/exe/**`
  (outside `src/core`, and `#ifdef BR_MATCHING_BUILD`-guarded),
  `src/core/generated/*.c` (all `BR_MATCHING_BUILD`-guarded). One real source
  bug fixed: `br_dl.c` `br_dl_skip` is byte-exact as 1-param (`return p+8`) but
  the handler table dispatches `(pDl,p)` - added a 2-param `br_dl_skip_h`
  wrapper for the table, guarded the 1-param original for BR_MATCHING_BUILD
  (a stricter clang turned the old incompatible-fn-ptr warning into an error).

- **Port TEST suite is OUT OF SYNC - does NOT build.** As functions were
  refined to their byte-exact prototypes (e.g. `BrRcaFixupRecord(void *pRec)`,
  1-param), the port tests written for the OLD interfaces (e.g.
  `BrRcaFixupRecord(&ctx, rec)`, 2-arg) stopped compiling.
  `tests/test_slice2_16.c` alone has 20 errors ("too many arguments"); the
  build stops there, so more test files are likely broken beyond it. **The
  README's old "137 / 137 green" was historical, not live** - corrected in the
  README 2026-08-27.

**OPEN CLEANUP TASK:** reconcile the port tests to the current matched
signatures (read include/<slice>.h for the live prototype, update the test's
calls + setup - often a logic change, not mechanical, since a dropped param
means the data moved into a struct). Do this per test file; `./build.sh` stops
at the first broken one, so fix-and-rebuild iteratively to surface the rest.
This drift accumulated because matching (the focus) changed signatures without
re-running the port build. See [readme-status-was-stale](../decomp/traps/readme-status-was-stale.md), [goal-coverage-not-playability](../decomp/rules/goal-coverage-not-playability.md).
