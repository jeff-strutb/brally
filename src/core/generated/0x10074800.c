/* 0x10074800 — CRT __ehvec_ctor. SEH __try/__finally, _except_handler3.
 * MATCH /O2: 0 diffs vs orig 111 B (map-split at the finally body).
 * Complete function through the outlined finally + second epilogue is
 * 160 B (nops to 0x100748A0) and also 0 diffs reloc-masked against the DLL.
 *
 * Map-split siblings (compiler-outlined, not independent C):
 *   0x1007486F  9 B  unwind-path ebx/edi/esi reload
 *   0x10074878 20 B  finally body
 *   0x1007488C 19 B  second epilogue
 *
 * `int i;` is declared uninitialized; `for (i = 0; ...)` lives inside
 * __try so trylevel is stored before i (orig: [ebp-0x20], [ebp-4],
 * [ebp-0x1c]). `int i = 0` outside the try permutes those stores. */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>

typedef void (__fastcall *PDtor)(void *);
typedef void (__fastcall *PCtor)(void *);
void __stdcall FUN_10074770(void *ptr, unsigned size, int count, PDtor dtor);

/* @implements 0x10074800 glide FUN_10074800 */
void __stdcall
FUN_10074800(void *ptr, unsigned size, int count, PCtor ctor, PDtor dtor)
{
    int success = 0;
    int i;

    __try {
        for (i = 0; i < count; i++) {
            ctor(ptr);
            ptr = (char *)ptr + size;
        }
        success = 1;
    } __finally {
        if (!success)
            FUN_10074770(ptr, size, i, dtor);
    }
}

#endif /* BR_MATCHING_BUILD */
