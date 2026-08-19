/* br_match.h -- calling convention macros for the matching build.
 *
 * MSVC 5.0 uses thiscall for C++ member functions (this in ecx, callee
 * cleans stack args) but the __thiscall keyword was added in VC7.
 * VC5 C code can't spell it. These 22 functions will need to be
 * compiled as .cpp or use inline asm wrappers for byte-matching.
 * BR_THISCALL marks them for that future work.
 */
#ifndef BR_MATCH_H
#define BR_MATCH_H

#define BR_THISCALL

#ifdef _MSC_VER
#define BR_STDCALL __stdcall
#else
#define BR_STDCALL
#endif

#endif /* BR_MATCH_H */
