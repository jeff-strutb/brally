/* WHAT IT DOES: loads a save in one of five modes -- a fresh season (4), the
 * in-game option block (0), the Time Attack ghost (1), or the car-equipment
 * file "c:\RallyConfig.dat" (2 installs it on the current player, 3 only reads
 * it); any other mode opens the second argument as a path.  Returns the sub-
 * loader's result for modes 0/1/4, 1/0 for the config read, and -- when the
 * file will not open -- whether the second argument's low byte was non-zero. */
/* @implements 0x1006A080 glide BrSaveLoad
 * @cpp_kind free
 * @cpp_symbol _BrSaveLoad
 *
 * The C++ lane for one expression: the failed-fopen return is
 * `*(char *)&arg ? true : false`, which VC5's C++ front end compiles to the
 * original's `mov al,[arg] / test al,al / setne al`; every C spelling loads
 * the byte into cl.  extern "C", so the C callers link unchanged.  The rest of
 * the shape (from the 2026-10-03 candidate): mode 2 jumps into the mode-3 arm
 * by `goto saveSlot`, the default arm reads its count through a volatile view
 * of arg (the original rematerialises arg rather than CSE it), and the mode-2
 * install sits inside the `mode == 2` arm with its own fclose/return.
 */
/* @t4-pass 0x1006A080 1 2026-09-07 probes 150 bytes 636 insns 220 regions 8 rows 10 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1006A080 2 2026-09-07 probes 149 bytes 636 insns 220 regions 8 rows 10 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1006A080 3 2026-09-13 probes 131 bytes 606 insns 206 regions 14 rows 18 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x1006A080 4 2026-09-20 probes 12 bytes 606 insns 206 regions 14 rows 18 census yes  (default-arm path=/count= assignment-order swap: inert, matches the dead list) */
/* @t4-pass 0x1006A080 5 2026-09-20 probes 10 bytes 606 insns 206 regions 14 rows 18 census no   (baseline reconfirm; the CSE-vs-rematerialise arg cascade has no source handle) */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

extern "C" {
extern char BrSeasonLoad(int mode, int arg);    /* 0x100695C0 */
extern int  BrSub69DC0(int arg);                 /* 0x10069DC0 -> BrGhostLoad */
extern void BrPodNop(const char *msg);           /* 0x10008D60, a bare ret */

struct BrPlayerState {
    int  *pBlock;                                /* +0x000 the option block */
    char  rest[0x2B68 - 4];
};
extern struct BrPlayerState DAT_10af2094[2];     /* 0x10AF2094 / 0x10AF4BFC */
extern int   DAT_105ccbc4;                       /* which player is current  */
extern int   DAT_117a6188[];                     /* the staging buffer       */
extern int   DAT_100ad760, DAT_100ad764, DAT_100ad768;  /* three staged dwords */
extern char  DAT_1007b0e0[];                     /* "rb"                     */
extern char  DAT_100b55d8[];                     /* "c:\\RallyConfig.dat"    */
extern char  DAT_100b55b4[];                     /* "Loading car equipment settings..." */
extern char  DAT_100b55ac[];                     /* "Done."                  */

char BrSaveLoad(int mode, int arg)
{
    FILE  *fp;
    char  *path;
    size_t count;
    int    ok;

    if (mode == 4)
        return BrSeasonLoad(4, arg);
    if (mode == 0)
        return BrSeasonLoad(0, arg);
    if (mode == 1)
        return (char)BrSub69DC0(arg);

    if (mode == 2)
        goto saveSlot;
    if (mode == 3) {
saveSlot:
        path  = DAT_100b55d8;
        count = 0x100;
    } else {
        path  = (char *)arg;
        count = *(volatile size_t *)&arg;
    }
    fp = fopen(path, DAT_1007b0e0);
    if (fp == 0)
        return *(char *)&arg ? true : false;

    if (mode != 2)
        ok = fread(DAT_117a6188, 1, count, fp) == count;
    else
        ok = fread(DAT_117a6188, 1, 0x80, fp) == 0x80;
    if (!ok) {
        fclose(fp);
        return 0;
    }
    if (mode == 2) {
        DAT_100ad760 = DAT_117a6188[0];
        DAT_100ad764 = DAT_117a6188[1];
        DAT_100ad768 = DAT_117a6188[2];
        BrPodNop(DAT_100b55b4);
        ok = fread(DAT_117a6188, 1, 0x80, fp) == 0x80;
        if (ok) {
            int equip[5];

            memcpy(equip, DAT_117a6188, 0x14);
            DAT_10af2094[DAT_105ccbc4].pBlock[0x3e] = equip[0];
            DAT_10af2094[DAT_105ccbc4].pBlock[0x3f] = equip[1];
            DAT_10af2094[DAT_105ccbc4].pBlock[0x40] = equip[2];
            DAT_10af2094[DAT_105ccbc4].pBlock[0x41] = equip[3];
            DAT_10af2094[DAT_105ccbc4].pBlock[0x42] = equip[4];
            BrPodNop(DAT_100b55ac);
            fclose(fp);
            return 1;
        }
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}
}
