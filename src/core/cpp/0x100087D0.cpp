/* @implements 0x100087D0 glide BrCleanupName_100087D0
 * @cpp_kind method
 * @cpp_symbol ?CleanupName@Name87D0@@QAEXPAD0@Z
 *
 * Thiscall, two stack args (`ret 8`), 121 B. Take the basename of `src`
 * into `dst` through the +4 subobject's helper (0x10008D70, the one
 * slice4_52.c matches as `__stdcall BrPathBasename` -- byte-identical
 * because that member never touches its `this`), shout via
 * BrLogFatalPrintf if the result is longer than the 64-byte field, then
 * upper-case it in place and zero-fill the tail of the field.
 *
 * The `this` adjustment for the member call is `add ecx,4`, not a lea,
 * because `this` is still live in ecx at that point.
 *
 * Both string primitives are the /Oi inline forms: strlen as
 * `or ecx,-1 / xor eax,eax / repne scasb / not ecx / dec ecx`, and the
 * variable-length memset as the shr-2 / and-3 stosd+stosb pair.
 *
 * The loop counter is UNSIGNED -- as `int` the two `i < 64` tests come
 * out `jl`/`jge` where the original has `jb`/`jae` (2 of the 9 first-draft
 * diffs).
 *
 * PARKED at 7 diffs, all inside the variable-length memset expansion and
 * all the SAME permutation as 0x1006FCE0's constant-size one:
 *     orig    mov ecx,0x40 / lea edi,[esi+ebx] / sub ecx,esi / xor eax,eax
 *     recomp  mov ecx,0x40 / xor eax,eax / sub ecx,esi / lea edi,[esi+ebx]
 * The original materialises the DESTINATION before the fill value; our cl
 * does the reverse. Same direction in both expansion shapes, so it is the
 * expansion, not the call site: see docs/VC5-IDIOMS.md.
 * DO NOT RE-PROBE -- `dst + i`, `&dst[i]`, and a hoisted `char *p` all
 * leave it unchanged.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Sub8D70 {
public:
    void Basename(char *src, char *dst);    /* 0x10008D70 */
};

class Name87D0 {
public:
    char    pad[4];
    Sub8D70 m4;             /* +0x04 */

    void CleanupName(char *src, char *dst);
};

extern "C" {
_CRTIMP int __cdecl toupper(int c);
void BrLogFatalPrintf(const char *fmt);     /* 0x10008EC0 */
}

void Name87D0::CleanupName(char *src, char *dst)
{
    unsigned int i;

    m4.Basename(src, dst);

    if (strlen(dst) > 64)
        BrLogFatalPrintf(
            "CleanupName: Name is greater than 64 bytes. Memory Corrupted...");

    for (i = 0; i < 64; i++) {
        char c = dst[i];
        if (c == 0)
            break;
        dst[i] = (char)toupper(c);
    }

    if (i < 64)
        memset(&dst[i], 0, 64 - i);
}
