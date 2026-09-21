/* WHAT IT DOES: walk every file matching a wildcard pattern (at most 100,
 * the same cap as BrFileCountMatching 0x10055ED0), and for each one read
 * the LAST 128 bytes of the file into a zeroed 260-byte buffer and hand the
 * file name and that trailer to the object's +0x18 virtual.  A 128-byte
 * trailer is the save-file's footer record, which is how the saved-game
 * browser labels its slots without parsing whole files.  Returns 0 when
 * nothing matched, else 1. */
/* @implements 0x10055D40 glide BrFileScanTrailers_10055D40
 * @cpp_kind method
 * @cpp_symbol ?ScanTrailers@Scan55D40@@QAEHPAD@Z
 *
 * Thiscall, one stack arg (`ret 4`), 391 B.  The first hit is handled
 * before the loop with the file closed BEFORE the virtual; the loop body
 * (i = 1..99) re-zeroes the buffer, _findnext, and closes AFTER the
 * virtual -- the two orders are the original's, not a slip.  The +0x18
 * vtable slot is loaded ONCE and kept in a frame slot for the loop's
 * `call [esp+0x1c]`: VC5 treats the vptr as invariant across the CRT
 * calls, which is the C++-only shape that keeps this out of the C lane.
 *
 * Byte-exact on the second compile.  The one source fact: the ftell result
 * is a NAMED local (`n = ftell(fp); fseek(fp, n - 128, SEEK_SET)`).  Written
 * inline as an argument, VC5 hoists the SEEK_SET `push 0` above the ftell
 * call and spells the offset `sub eax,0x80` (5 B); with the local the push
 * sinks below the call and the constant folds to `add eax,-0x80` (3 B).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>
#include <io.h>

extern "C" char DAT_100ac9c8[];        /* "r" */

class Scan55D40 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void OnTrailer(char *pszName, char *pTrailer);   /* +0x18 */

    int ScanTrailers(char *pszPattern);
};

int Scan55D40::ScanTrailers(char *pszPattern)
{
    char trailer[260];
    struct _finddata_t fd;
    long h;
    long n;
    FILE *fp;
    int i;

    memset(trailer, 0, sizeof(trailer));
    h = _findfirst(pszPattern, &fd);
    if (h == -1)
        return 0;

    fp = fopen(fd.name, DAT_100ac9c8);
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, n - 128, SEEK_SET);
    fread(trailer, 1, 128, fp);
    fclose(fp);
    OnTrailer(fd.name, trailer);

    for (i = 1; i < 100; i++) {
        memset(trailer, 0, sizeof(trailer));
        if (_findnext(h, &fd) != 0)
            break;
        fp = fopen(fd.name, DAT_100ac9c8);
        fseek(fp, 0, SEEK_END);
        n = ftell(fp);
        fseek(fp, n - 128, SEEK_SET);
        fread(trailer, 1, 128, fp);
        OnTrailer(fd.name, trailer);
        fclose(fp);
    }
    _findclose(h);
    return 1;
}
