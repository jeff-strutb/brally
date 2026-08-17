/* br_strres.h -- RESPONSIBILITY: locate, read and decode the game's own
 * files.  This one is BRString.dll, and what it holds is every piece of text
 * the game ever puts on screen.
 *
 * WHAT THIS CLOSES
 *
 * `BrStrGet` (slice4_52.c, D3D 0x10074030 == Glide 0x1006D280) is 27 bytes of
 * bounds check over an array of 303 pointers at Glide 0x1186C488 / D3D
 * 0x11829370, and four separate headers in this tree record that the array is
 * "filled at RUNTIME -- whatever fills it is unidentified".  This module is
 * what fills it: Glide 0x1006D1A0, called by RallyMain at 0x1001CCAA in the
 * init run that follows the start gate.  The D3D counterpart is 0x10073F60.
 *
 * The storage is NOT redefined here.  `g_apBrStrTable` already exists in
 * slice4_52.c and this module writes THAT array -- CONVENTIONS.md, "Aliased
 * storage: a link-clean bug".
 *
 * HOW THE ORIGINAL DOES IT, which is odder than it sounds
 *
 * BRString.dll is a resource-only DLL: no code, a .rsrc of RT_STRING blocks
 * and a .reloc.  0x1006D1A0
 *
 *   1. opens the file with CHK_FReadOpen, asks CHK_FileSize how big it is,
 *      and closes it again -- WITHOUT READING A BYTE.  The file size is used
 *      only as the size of the malloc that follows.  It is a generous bound
 *      rather than a measurement: the resources are UTF-16 and the copies are
 *      ANSI, so the ANSI text cannot exceed half the file, let alone all of
 *      it.  Nothing in the routine checks that it fits.
 *   2. LoadLibraryA's the same path, and if that fails returns having done
 *      nothing but clear the table.
 *   3. mallocs the blob and walks ids 1..0x12E, LoadStringA'ing each one into
 *      the unused tail of the blob and storing the pointer in slot `id`.  A
 *      missing id (LoadStringA returns 0) leaves the slot NULL and does not
 *      advance the cursor.
 *   4. FreeLibrary's the module.  The strings survive because they were
 *      copied out; the pointers in the table are into the malloc'd blob, not
 *      into the module image.
 *
 * THE RE-ENTRY DEFECT, PRESERVED
 *
 * The table is cleared FIRST and the "already loaded" test comes second:
 *
 *      rep stosd            ; 0x12E DWORDs at &g_apBrStrTable[1]
 *      mov eax, [blob]
 *      test eax, eax
 *      jne  done            ; <- returns with the table already emptied
 *
 * So a second call empties the table, keeps the blob, and refills nothing.
 * Every id resolves to NULL afterwards.  Nothing in the shipped binary calls
 * it twice, so the defect never fires -- it is preserved because reversing
 * the two would be a silent behaviour change, and because BrStrResFree
 * (0x1006D2A0) is what makes a legitimate reload possible.
 *
 * WHY THE MODULE LOADER IS BEHIND AN OPS STRUCT AND THE FILE CALLS ARE NOT
 *
 * LoadLibraryA / LoadStringA / FreeLibrary are Win32 and have no portable
 * spelling, so they are supplied by the caller (CONVENTIONS.md: "No Win32
 * types or calling conventions in portable code").  CHK_FReadOpen /
 * CHK_FileSize / CHK_FClose are plain CRT and are ALREADY PORTED in
 * slice6_78.c, so they are called directly rather than re-transcribed.
 *
 * The ops struct is REQUIRED and is not defaulted.  A caller that supplies
 * nothing gets no strings, because there is no honest value to invent for a
 * resource that was never opened.
 */
#ifndef BR_STRRES_H
#define BR_STRRES_H

#include <stdint.h>

/* 0x100B74B0 -- the path, pushed twice: once to CHK_FReadOpen at 0x1006D1BE
 * and once to LoadLibraryA at 0x1006D1E4.  It is a bare filename, so it
 * resolves against the process's current directory and the DLL search path,
 * not against the install directory br_basedir.c recovers. */
#define BR_STRRES_PATH  "BRString.dll"

/* The platform entry points 0x1006D1A0 makes.  All three are required. */
typedef struct BrStrResOps {
    /* LoadLibraryA.  NULL return aborts the load. */
    void *(*pfnLoadModule)(const char *pszPath);

    /* LoadStringA.  Returns the number of characters written, NOT counting
     * the terminator; 0 means the id is absent.  `cchBufMax` is what the
     * original computes as `size - used` and is NOT clamped: once the cursor
     * reaches the blob size this goes to zero and then negative, and the
     * original hands that straight to Win32. */
    int   (*pfnLoadString)(void *hModule, uint32_t uId,
                           char *pszBuf, int cchBufMax);

    /* FreeLibrary.  Called on every path that got a module, including the
     * malloc-failure path. */
    void  (*pfnFreeModule)(void *hModule);
} BrStrResOps;

/* 0x1186C944 (D3D 0x11829370 + 0x4BC) -- the one malloc'd block every entry
 * of g_apBrStrTable points into.  Non-NULL is what "already loaded" means. */
extern char   *g_pBrStrResBlob;

/* 0x1186C948 -- bytes of the blob consumed so far.  NEVER RESET BY THE LOAD;
 * only BrStrResFree zeroes it.  The load reads it once before the walk and
 * re-reads it after every LoadStringA. */
extern int32_t g_brStrResUsed;

/* 0x1186C94C -- the size of BRString.dll on disc, which is the size of the
 * blob.  Written before the module is loaded, so it survives a LoadLibraryA
 * failure. */
extern int32_t g_brStrResSize;

/* 0x1006D1A0 (D3D 0x10073F60).  Fills slice4_52.c's g_apBrStrTable.
 *
 * WARNING, and it is the original's: on every path this routine first clears
 * ids 1..0x12E.  See the re-entry note in the banner. */
void BrStrResLoad(const BrStrResOps *pOps);

/* 0x1006D2A0 (D3D 0x10074050).  free()s the blob and zeroes the three
 * globals -- and DOES NOT clear g_apBrStrTable, so every entry is left
 * dangling at the freed block.  That is the original; the only caller-safe
 * order is BrStrResFree() immediately followed by BrStrResLoad(). */
void BrStrResFree(void);

/* Not in the original: the original gets fresh .data from the loader.  Zeroes
 * the three globals WITHOUT freeing, for a test that installed a fake blob. */
void BrStrResResetForTest(void);

#endif /* BR_STRRES_H */
