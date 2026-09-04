/* br_dplaylobby.c -- net.
 *
 * The DirectPlay lobby side: the EnumAddress callback the session set-up
 * hands to IDirectPlayLobby.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

/* ==========================================================================
 * 6. 0x1003D850 -- IDirectPlayLobby::EnumAddress callback
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* DPAID_Modem at 0x100909E0. */
extern unsigned char g_0909E0[16];

/* dllimport so the IAT pointer is hoisted into esi and called through it,
 * matching `mov esi, [0x118AE45C] / call esi`. */
__declspec(dllimport) int __stdcall lstrlenA(const char *lpString);

/* WHAT IT DOES: DirectPlay EnumAddress callback. When the current address
 * chunk is the modem-name list, walk that packed (double-NUL-terminated)
 * list of names and throw them away. Always tells the enumerator to keep
 * going. arg1 (chunk size) and arg3 (context) are unused; they stay in the
 * signature so the callee pops 16 bytes (`ret 0x10`). */
/* @implements 0x1003D850 d3d BrSub1003D850 */
int __stdcall BrSub1003D850(const void *pGuid, unsigned dwDataSize,
                            char *pList, void *pContext)
{
    char *p = pList;

    if (memcmp(pGuid, g_0909E0, 16) == 0) {
        if (lstrlenA(p) != 0) {
            do {
                p += lstrlenA(p) + 1;
            } while (lstrlenA(p) != 0);
        }
    }
    return 1;
}
#endif
