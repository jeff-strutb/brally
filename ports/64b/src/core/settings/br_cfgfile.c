/* br_cfgfile.c -- reads "BossRally.cfg".  See br_cfgfile.h for the format,
 * the ESP trace, the identification of `this`, and the two preserved bugs.
 *
 * RESPONSIBILITY: settings.  One function of the original lives here, Glide
 * 0x10063060 / D3D 0x10069FF0, plus the byte layout it demands.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_cfgfile.h"

#include <stdio.h>
#include <string.h>

/* The header's arithmetic, checked at compile time rather than trusted.
 * C99 has no _Static_assert and this tree stays on one standard, so it is
 * the negative-array-size trick test_layout.c established. */
typedef char br_cfgfile_assert_size
    [(BR_CTRLCFG_FILE_SIZE == 0x878) ? 1 : -1];
typedef char br_cfgfile_assert_profile
    [(sizeof(BrCtrlProfile) == 0xA8) ? 1 : -1];

/* ======================================================================
 * Little-endian codecs.
 *
 * The original does none of this: it freads raw bytes over the live object
 * and the dwords land correctly because the host is 32-bit LE x86.  That is
 * not available here -- BrCtrlCfg carries a host pointer, so its tail is not
 * at the original's offsets -- so every integer crosses the file boundary
 * byte-wise, per CONVENTIONS.md.  On any little-endian host the result is
 * identical to the original's; on a big-endian one it is the same FILE,
 * which is what portability means for a format.
 * ====================================================================== */

/* (port-only BrCfgLoad32 removed) */


/* (port-only BrCfgStore32 removed) */


/* (port-only BrCfgLoad16 removed) */


/* (port-only BrCfgStore16 removed) */


/* ======================================================================
 * One field, one fread.
 *
 * Every read in 0x10063060 is `fread(p, size, 1, fp)` with the result
 * compared against 1, so a field either arrives whole or the load fails on
 * it.  Keeping one call per field keeps that boundary exactly where the
 * original puts it: `fread(p, 1, size, fp)` would report a partial field as
 * a partial success and move the failure to the next check.
 *
 * The scratch buffer is sized for the largest field, +0x3B8's 0x400 bytes.
 * ====================================================================== */

#define BR_CFG_MAX_FIELD  0x400

/* (port-only BrCfgReadRaw removed) */


/* (port-only BrCfgReadU32 removed) */


/* (port-only BrCfgReadI32 removed) */


/* n dwords in ONE fread of n*4 bytes -- the original's `fread(p, 0x104, 1,
 * fp)` and friends, not n separate reads. */
/* (port-only BrCfgReadU32Array removed) */


/* One 0xA8-byte profile: BR_CTRL_ACTIONS * 3 little-endian 16-bit entries,
 * in the order they sit in memory (action-major, slot-minor). */
/* (port-only BrCfgReadProfile removed) */


/* ======================================================================
 * 0x10063060 -- load the settings file over an existing config object.
 * ====================================================================== */

/* WHAT IT DOES: loads the player's control settings from disk on top of the
 * settings already in memory. It checks the file's magic word and version
 * first, and works through a temporary copy so a partly-read file cannot
 * leave the live settings half-updated. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is the C++ method in
 * src/core/settings/BrCtrlCfgReadFile_10063060.cpp */
/* BrCtrlCfgReadFile: prototype in br_funcs.h */

/* ======================================================================
 * The layout, as bytes.  Not a decompilation -- see the header.
 * ====================================================================== */

/* (port-only BrCfgPutU32Array removed) */


/* (port-only BrCfgPutI32 removed) */


/* (port-only BrCfgPutProfile removed) */


/* (port-only BrCtrlCfgFileEncode removed) */


/* ======================================================================
 * The writer of this same file, 0x100634B0, filed out of the address batch
 * slice4_53.c.  Its port-side counterpart, BrCfgSave1006A4A0, stays in that
 * slice as br_cfgfile.h records.
 * ====================================================================== */

/* ------------------------------------------------------------------
 * 0x100634B0 -- the GLIDE build of the config writer.  thiscall
 * (this in ecx, path on the stack, callee-pops), reached through the
 * proven __fastcall(this, _edx_unused, ...) shim.  Unlike the port
 * body in slice4_53.c, the original is UNROLLED: 32 separate checked
 * fwrites, every failure jumping to one shared fclose/return-0.  The
 * magic is written as strlen(global) of a string the file also owns
 * (repne scasb intrinsic), the version dword straight from its global. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x100B4C20  "RCfg" */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x10077A2C  02 00 00 00 */

/* WHAT IT DOES: write the Glide renderer's settings to a file -- a magic
 * string, a version, then each setting in turn, bailing out on the first
 * short write. Returns zero if it could not open or could not finish, so a
 * half-written file is reported rather than trusted. */
/* @implements 0x100634B0 glide BrGlCfgSave */
int __fastcall BrGlCfgSave(void *pThis, const char *pszPath)
{
    unsigned char *pBase = (unsigned char *)pThis;
    FILE          *pFile;
    pFile = fopen(pszPath, "wb");
    if (pFile == NULL)
        return 0;

    if (fwrite(BrGlCfgMagic, strlen(BrGlCfgMagic), 1, pFile) != 1) goto fail;
    if (fwrite(BrGlCfgVersion, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2A8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2AC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2B0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2B4, 0x104, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x3B8, 0x400, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7B8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7BC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7C8, 0x10, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7D8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7DC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7E8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7EC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F4, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7F8, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x7FC, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x800, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x804, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x808, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x80C, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x810, 0x20, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x830, 0x40, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x870, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x2A0, 4, 1, pFile) != 1) goto fail;
    if (fwrite(pBase, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x0A8, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x150, 0xA8, 1, pFile) != 1) goto fail;
    if (fwrite(pBase + 0x1F8, 0xA8, 1, pFile) != 1) goto fail;

    fclose(pFile);
    return 1;
fail:
    fclose(pFile);
    return 0;
}
