/* br_texlevels.c -- drawing: load the texture-detail threshold file.
 *
 * 0x10031030 reads a text file of "%u" then "%u %x %d %d" rows into the
 * 16-byte-stride table at 0x106EEF08, then asks BrTexChooseLevel to pick
 * a detail level from the threshold it just stored.
 *
 * RESIDUE 8B /O2, REGNORM 0+0, FIRSTDIV +0x47. Instruction shape and
 * size match; the 8 bytes are esi/edi swapped (fp vs sscanf IAT). Orig
 * allocates fp to edi while path occupies esi, then reuses esi for
 * sscanf. Recomp reuses esi for fp and puts sscanf in edi. T3a
 * colouring -- park. */

#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

/* FUN_10003680: prototype in br_funcs.h */
/* FUN_10003320: prototype in br_funcs.h */
/* FUN_10003530: prototype in br_funcs.h */
/* FUN_100035e0: prototype in br_funcs.h */
/* BrTexChooseLevel: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* g_brTexLowThreshold */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* "%u" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: load the texture-detail table from a text file. Zeros the
 * row count and sets the low-memory threshold to 2MB, then if the file
 * exists reads a first "%u" line into that threshold and every later
 * "%u %x %d %d" line into one 16-byte row, stopping at 256 rows or EOF.
 * Always closes the file (when opened) and re-runs the detail-level
 * chooser; missing file just runs the chooser on the 2MB default. */
/* @implements 0x10031030 glide FUN_10031030 */
void FUN_10031030(char *pszPath)
{char buf[0x400];  int s30;
(*(int *)&g_brKeyCount) = 0;(*(int *)&g_brTexLowThreshold) = 0x200000;if (BrChkFileExists(pszPath) == 0) {
    BrTexChooseLevel();
    return;
  }{
    register FILE **fp;
    char *ok;
    int off;

    fp = BrChkFReadOpen(pszPath);
    ok = BrChkFReadLine(buf, 0x400, fp);
    if (ok != 0) {
      int (__cdecl *scan)(const char *, const char *, ...) = sscanf;
      scan(buf, DAT_100aa334, &(*(int *)&g_brTexLowThreshold));
      ok = BrChkFReadLine(buf, 0x400, fp);
      while (ok != 0) {
        off = (*(int *)&g_brKeyCount) * 0x10;
        scan(buf, s__u__x__d__d_100aa328,
               &DAT_106eef08 + off,
               &DAT_106eef08 + off + 4,
               &DAT_106eef08 + off + 8,
               &DAT_106eef08 + off + 12);
        s30 = (*(int *)&g_brKeyCount); (*(int *)&g_brKeyCount) = s30 + 1;
        if ((*(int *)&g_brKeyCount) >= 0x100) {
          break;
        }
        ok = BrChkFReadLine(buf, 0x400, fp);
      }
    }
    BrChkFClose(fp);
  }BrTexChooseLevel();}

