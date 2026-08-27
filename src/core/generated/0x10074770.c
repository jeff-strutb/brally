/* 0x10074770 — CRT __ArrayUnwind. SEH __try/__except, _except_handler3.
 * MATCH /O2: 0 diffs vs orig 64 B (map-split at the filter). Complete
 * function through ret 0x10 is 106 B and also 0 diffs reloc-masked
 * against the DLL; COFF nops take the .obj to 112.
 *
 * Map-split siblings (compiler-outlined, not independent C):
 *   0x100747B0 13 B  __except filter
 *   0x100747BD 29 B  handler + epilogue
 *
 * thiscall element dtor = __fastcall(void *). */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>

typedef void (__fastcall *PDtor)(void *);
int __cdecl FUN_100747e0(struct _EXCEPTION_POINTERS *);

/* @implements 0x10074770 glide FUN_10074770 */
void __stdcall
FUN_10074770(void *ptr, unsigned size, int count, PDtor dtor)
{
    __try {
        for (;;) {
            --count;
            if (count < 0)
                break;
            dtor(ptr = (char *)ptr - size);
        }
    } __except (FUN_100747e0(GetExceptionInformation())) {
    }
}

#endif /* BR_MATCHING_BUILD */
