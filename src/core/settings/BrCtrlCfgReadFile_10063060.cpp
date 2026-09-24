/* WHAT IT DOES: loads the player's control settings from disk on top of the
 * settings already in memory.  It snapshots the live settings into a stack
 * copy first, checks the file's "RCfg" magic and version 2, then reads every
 * field straight into the live object; on the first short read it restores
 * the snapshot, closes the file, re-applies the (restored) settings and
 * returns 0, so a partly-read file never leaves them half-updated.  Returns 1
 * on success, 0 if the file cannot be opened. */
/* @implements 0x10063060 glide BrCtrlCfgReadFile
 * @cpp_kind method
 * @cpp_symbol ?m_10063060@BrCtrlCfg_10062B00_10008D60@@QAEHPBD@Z
 *
 * Thiscall (`this` in ecx, the path on the stack, `ret 4`) with an EH frame:
 * the 0x874-byte snapshot is a real object whose constructor (0x10062B00)
 * and destructor (the out-of-line nop 0x10008D60) bracket the reads, which
 * is what routes it to the C++ lane.  The C twin in br_cfgfile.c stays as
 * the port's reader. */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>
#endif

extern "C" char BrGlCfgMagic[];     /* 0x100B4C20  "RCfg" */

/* VA-encoded class name (ctor_dtor) + m_<VA> methods, so the oracle's reloc
 * resolver maps every thiscall to its real address. */
class BrCtrlCfg_10062B00_10008D60 {
public:
    BrCtrlCfg_10062B00_10008D60();                  /* 0x10062B00 */
    ~BrCtrlCfg_10062B00_10008D60();                 /* 0x10008D60 (nop) */
    void m_10062E50(BrCtrlCfg_10062B00_10008D60 *pSrc);   /* copy from pSrc */
    void m_10062D00();                              /* apply */
    int  m_10063060(const char *pszPath);

    unsigned char b[0x874];
};

int BrCtrlCfg_10062B00_10008D60::m_10063060(const char *pszPath)
{
    int   version;
    int   magic;
    FILE *pFile;

    pFile = fopen(pszPath, "rb");
    if (pFile == NULL)
        return 0;

    BrCtrlCfg_10062B00_10008D60 save;
    save.m_10062E50(this);

    if (fread(&magic, 4, 1, pFile) != 1) goto fail;
    if (strncmp((char *)&magic, BrGlCfgMagic, strlen(BrGlCfgMagic)) != 0) goto fail;
    if (fread(&version, 4, 1, pFile) != 1) goto fail;
    if (version != 2) goto fail;
    if (fread(b + 0x2a8, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x2ac, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x2b0, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x2b4, 0x104, 1, pFile) != 1) goto fail;
    if (fread(b + 0x3b8, 0x400, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7b8, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7bc, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7c0, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7c4, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7c8, 0x10, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7d8, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7dc, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7e0, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7e4, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7e8, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7ec, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7f0, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7f4, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7f8, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x7fc, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x800, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x804, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x808, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x80c, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x810, 0x20, 1, pFile) != 1) goto fail;
    if (fread(b + 0x830, 0x40, 1, pFile) != 1) goto fail;
    if (fread(b + 0x870, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x2a0, 4, 1, pFile) != 1) goto fail;
    if (fread(b + 0x000, 0xa8, 1, pFile) != 1) goto fail;
    if (fread(b + 0x0a8, 0xa8, 1, pFile) != 1) goto fail;
    if (fread(b + 0x150, 0xa8, 1, pFile) != 1) goto fail;
    if (fread(b + 0x1f8, 0xa8, 1, pFile) != 1) goto fail;

    fclose(pFile);
    return 1;
fail:
    m_10062E50(&save);
    fclose(pFile);
    m_10062D00();
    return 0;
}
