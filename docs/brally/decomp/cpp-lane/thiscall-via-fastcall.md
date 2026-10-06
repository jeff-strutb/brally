# Thiscall via fastcall

*Recorded 2026-08-20.*

> VC5 C can reach __thiscall after all - __fastcall for one argument, struct-typed second param for two.

The old note in `src/brally/include/br_match.h` claimed ~22 thiscall functions "will need
to be compiled as .cpp or use inline asm wrappers." **That is too pessimistic
and is now corrected in the header.**

MSVC 5.0 has no `__thiscall` keyword, but `__fastcall` passes the first two
register-eligible arguments in ecx and edx with callee stack cleanup:

- **One argument (just `this`)**: there is no second argument, so `__fastcall`
  and thiscall emit *identical* code. Exact, not an approximation. This is
  `BR_THISCALL1` in `src/brally/include/br_match.h`, defined as `__fastcall` under
  `_MSC_VER` and empty otherwise so the macOS port is unaffected.
- **Two or more arguments**: `__fastcall` claims edx for the second argument
  where thiscall leaves it on the stack, so a blanket macro would silently
  mis-pass it. `BR_THISCALL` therefore stays a no-op marker for this case  - 
  **do not redefine it to `__fastcall`.** Two workers proposed exactly that;
  it is wrong.
- **The two-argument workaround**: a struct-typed parameter is never
  register-eligible, so a 4-byte struct in second position is forced back onto
  the stack, reproducing thiscall's split and callee cleanup. Landed this way
  in `BrSub10060260` (`src/brally/core/slice4_52.c`). It is per-call-site, not a macro.

Matched with this: BrTextBoxDtor, BrCtrlCfgInitGlobal, BrS17Release,
BrSub10060260.

**Two traps that go with it.** The scorer could not see fastcall symbols at all
until fixed - see [match-tooling-gotchas](../traps/match-tooling-gotchas.md); without that fix these functions
report `not_in_obj` forever no matter how right the bytes are. And where the
original plants an address as an immediate (`mov [ecx], 0x1008F728`), the C
must be an address-of a declared object, not a load through a pointer variable
 -  a pointer read costs an extra `mov` and misses by an instruction. Declaring
the object `extern` without defining it is fine: the sweep compiles `/c` and
never links, and objdiff zeroes the relocation.

Related: [matching-progress](../log/matching-progress.md), [implements-requires-execution](../rules/implements-requires-execution.md).
