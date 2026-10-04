/* br_ringmutex.c -- startup: create and destroy the pair-ring mutex.
 *
 * Filed out of the address batch slice6_78.c.  The ring's three globals are
 * still DEFINED there, because the ring's readers and writers are still
 * there; only the bring-up and take-down pair moved.
 */
#include <stddef.h>
#include <stdint.h>

/* 64-bit core: CreateMutexA is declared by the platform headers */
/* 64-bit core: CloseHandle is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: creates the unnamed pair-ring mutex and empties the ring. */
/* @implements 0x10074F20 d3d BrMutexCreateAA0A0 */
void *BrMutexCreateAA0A0(void)
{
    void *h;

    g_br18A9878 = 0;
    g_br18AA098 = 0;
    h = CreateMutexA(0, 0, 0);
    g_br18AA0A0 = h;
    return h;
}

/* WHAT IT DOES: closes that mutex and empties the ring again. */
/* @implements 0x10074F40 d3d BrMutexCloseAA0A0 */
void BrMutexCloseAA0A0(void)
{
    void *h;

    h = g_br18AA0A0;
    g_br18A9878 = 0;
    g_br18AA098 = 0;
    (void)CloseHandle(h);
    g_br18AA0A0 = NULL;
}
