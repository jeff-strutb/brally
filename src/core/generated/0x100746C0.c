/* 0x100746C0 — CRT __ehvec_dtor. SEH __try/__finally, _except_handler3.
 * MATCH /O2: 0 diffs vs orig 115 B (map-split at the finally body).
 * Complete function through the outlined finally + second epilogue is
 * 176 B (nops to 0x10074770) and also 0 diffs reloc-masked against the DLL.
 *
 * Map-split siblings (compiler-outlined, not independent C):
 *   0x10074733  6 B  unwind-path esi/edi reload
 *   0x10074739 23 B  finally body
 *   0x10074750 19 B  second epilogue
 *
 * Pointer is walked to the end before the try; loop destroys backwards.
 * while (--count >= 0) is required: for(;;){--count; if(count<0) break;}
 * inverts to jns and the body is outlined past ret (44 diffs). */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>

typedef void (__fastcall *PDtor)(void *);
void __stdcall FUN_10074770(void *ptr, unsigned size, int count, PDtor dtor);

/* WHAT IT DOES: destroy an array of objects, BACK to front, calling each
 * element's destructor. If one throws, the compiler's cleanup helper
 * finishes destroying the rest. Compiler-generated array teardown, not game
 * code. */
/* @implements 0x100746C0 glide FUN_100746c0 */
void __stdcall
FUN_100746c0(void *ptr, unsigned size, int count, PDtor dtor)
{
    int success = 0;

    ptr = (char *)ptr + size * count;
    __try {
        while (--count >= 0) {
            ptr = (char *)ptr - size;
            dtor(ptr);
        }
        success = 1;
    } __finally {
        if (!success)
            FUN_10074770(ptr, size, count, dtor);
    }
}

#endif /* BR_MATCHING_BUILD */
