/* br_saveload.c -- settings: the five-way save-load dispatcher.
 *
 *   0x1006A080  641 B   the loader the save-slot menus reach (the port calls
 *                       it BrSub10071130, though that name is also 0x10071130
 *                       in slice5_60 -- a port mislabel, not fixed here).
 *
 * Dispatches on the mode argument:
 *   mode 4    a fresh season   -> BrSeasonLoad(4, arg)   (0x100695C0)
 *   mode 0    the option block  -> BrSeasonLoad(0, arg)
 *   mode 1    the Time Attack ghost -> BrSub69DC0(arg)   (0x10069DC0, which
 *                                 forwards the ghost path to BrGhostLoad)
 *   mode 2/3  "c:\RallyConfig.dat": read 0x100 bytes; mode 2 then installs the
 *             car-equipment set on the current player and reports "Done.",
 *             mode 3 just validates the read
 *   default   opens the arg as a path and reads (size_t)arg bytes -- a quirk
 *             (the byte count IS the pointer value); no shipped caller hits it.
 *
 * Shape notes from the bytes:
 *  - modes 4/0/1 return the callee's char straight through, each with its own
 *    epilogue; a failed fopen returns whether arg's low byte was non-zero;
 *  - the read result is materialised into a bool (`sete`) before the test;
 *  - the mode-2 install re-reads the current-player index and re-indexes the
 *    0x2B68-stride player array for EVERY one of the five equipment stores
 *    (the intervening stores could alias the index global, so VC5 reloads it);
 *  - the status prints go through BrPodNop (0x10008D60, a bare `ret`).
 *
 * PARKED 2026-09-06 at 608/641 B, register-blind 2+16 (14 insns short).  The
 * body is logically complete and correct against the disassembly -- every mode
 * arm, the config read, the car-equipment install (re-indexed per store) and
 * every return are present.  The whole residue is ONE allocation cascade with
 * a single root: the original REMATERIALISES the second argument, loading it
 * from its stack slot twice (`mov eax,[arg]` for the path, `mov edi,[arg]` for
 * the count), so its four contended values -- mode(ebp), fp(ebx), fread-import
 * (esi), count(edi) -- fit the four callee-saved registers with none spilled.
 * VC5 here instead CSEs the two arg loads into ebx (`mov ebx,[arg]; mov eax,ebx;
 * mov edi,ebx`), which makes arg a FIFTH callee-saved value; that forces `mode`
 * out of a register (it lands in esi, collides with the fread import and spills
 * to [esp+0x28]), and the resulting per-block register differences let VC5
 * cross-jump the three tail returns the original keeps as separate epilogues
 * (the 14-instruction / 33-byte deficit).  Rematerialise-vs-copy is a low-level
 * codegen choice with no source handle found.
 *
 * DEAD, do not re-run (each measured with tools/fnmatch/fn.py --detail regnorm):
 *   - `arg` typed char* (Ghidra's param type): inert, still CSEs (2+16);
 *   - Ghidra's literal comma-operator form with a char* count (`if (mode==2 ||
 *     (path=arg, cnt=arg, mode==3))`): 1+16, FIRSTDIV worse (+0x4);
 *   - swapping the default's `path=`/`count=` assignment order: inert (2+16).
 */
#ifdef BR_MATCHING_BUILD

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>
#include <string.h>

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

/* WHAT IT DOES: loads a save in one of five modes -- a fresh season (4), the
 * in-game option block (0), the Time Attack ghost (1), or the car-equipment
 * file "c:\RallyConfig.dat" (2 installs it on the current player, 3 only reads
 * it); any other mode opens the second argument as a path.  Returns the sub-
 * loader's result for modes 0/1/4, 1/0 for the config read, and -- when the
 * file will not open -- whether the second argument's low byte was non-zero. */
/* @t4-pass 0x1006A080 1 2026-09-07 probes 150 bytes 636 insns 220 regions 8 rows 10 census yes  (tools/crank.py) */
/* @implements 0x1006A080 glide BrSaveLoad */
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

    if (mode == 2 || mode == 3) {
        path  = DAT_100b55d8;
        count = 0x100;
    } else {
        path  = (char *)arg;
        count = (size_t)arg;
    }
    fp = fopen(path, DAT_1007b0e0);
    if (fp == 0)
        return (char)arg != 0;

    if (mode == 2)
        ok = fread(DAT_117a6188, 1, 0x80, fp) == 0x80;
    else
        ok = fread(DAT_117a6188, 1, count, fp) == count;
    if (!ok) {
        fclose(fp);
        return 0;
    }
    if (mode != 2) {
        fclose(fp);
        return 1;
    }

    DAT_100ad760 = DAT_117a6188[0];
    DAT_100ad764 = DAT_117a6188[1];
    DAT_100ad768 = DAT_117a6188[2];
    BrPodNop(DAT_100b55b4);
    if (fread(DAT_117a6188, 1, 0x80, fp) != 0x80) {
        fclose(fp);
        return 0;
    }
    {
        int equip[5];

        memcpy(equip, DAT_117a6188, 0x14);
        DAT_10af2094[DAT_105ccbc4].pBlock[0x3e] = equip[0];
        DAT_10af2094[DAT_105ccbc4].pBlock[0x3f] = equip[1];
        DAT_10af2094[DAT_105ccbc4].pBlock[0x40] = equip[2];
        DAT_10af2094[DAT_105ccbc4].pBlock[0x41] = equip[3];
        DAT_10af2094[DAT_105ccbc4].pBlock[0x42] = equip[4];
    }
    BrPodNop(DAT_100b55ac);
    fclose(fp);
    return 1;
}

#endif /* BR_MATCHING_BUILD */
