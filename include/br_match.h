/* br_match.h -- calling convention macros for the matching build.
 *
 * MSVC 5.0 uses thiscall for C++ member functions (this in ecx, callee
 * cleans stack args) but the __thiscall keyword was added in VC7, so VC5
 * C code cannot spell it directly.
 *
 * It can be reached indirectly, though. __fastcall passes the first two
 * register-eligible arguments in ecx and edx and has the callee clean the
 * stack. For a function whose only argument is `this`, there is no second
 * argument, so __fastcall and thiscall emit identical code -- that case is
 * BR_THISCALL1 and it is exact, not an approximation.
 *
 * It does NOT generalise. With two or more arguments, __fastcall claims edx
 * for the second one, whereas thiscall leaves it on the stack. Those still
 * need a per-call-site trick (a struct-typed parameter is never
 * register-eligible, so it is forced back onto the stack -- see
 * BrSub10060260 in src/core/slice4_52.c) or .cpp compilation.
 *
 * ‼ CORRECTED 2026-09-03: this used to say "a struct-typed SECOND parameter",
 * and that is only enough when there are exactly two arguments. __fastcall
 * SKIPS a struct when it hands out ecx and edx -- it does not stop there --
 * so with three arguments a wrapper on the second one just lets the THIRD
 * take edx, and the function cleans 4 bytes where thiscall cleans 8. Wrap
 * EVERY argument after `this`. Found on 0x10069BC0 BrFn10069BC0, where the
 * one-wrapper form was byte-exact in shape but 16 bytes short because the
 * per-arm stack reloads of the third argument had turned into a register.
 *
 * BR_THISCALL therefore stays a no-op marker for the general multi-argument
 * case. Do not redefine it to __fastcall; that would silently mis-pass the
 * second argument of every function wearing it.
 */
#ifndef BR_MATCH_H
#define BR_MATCH_H

#define BR_THISCALL

#ifdef _MSC_VER
#define BR_THISCALL1 __fastcall
#else
#define BR_THISCALL1
#endif

#ifdef _MSC_VER
#define BR_STDCALL __stdcall
#else
#define BR_STDCALL
#endif

#endif /* BR_MATCH_H */
