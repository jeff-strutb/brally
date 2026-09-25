/* br_dplayappmsg.c -- the DirectPlay application-message dispatcher
 * (0x10009010).
 *
 * Fresh transcription from build/ghidra_decomp/0x10009010.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdio.h>

int  FUN_10036a30(int, int, LPCSTR, LPCVOID *, int);        /* 0x10036A30 */
/* The "finished" line's builder: a separately named twin of FUN_10036a30
 * whose identical body the linker folded onto 0x10036A30.  The call site
 * must name a DIFFERENT symbol -- see RESIDUE 1 in the header below. */
int  BrChatLineFinishTwin(int, int, LPCSTR, LPCVOID *, int); /* 0x10036A30 */
int  FUN_1006ba60(int a, int b);                              /* 0x1006BA60 */
void FUN_1002f790(int *, int *, int, int, int);               /* 0x1002F790 */
void FUN_100038f0(int *, int *, int, int, int);               /* 0x100038F0 */

extern int   DAT_10ac5be4;          /* host has started */
extern int   DAT_10ac5890;          /* ready table: {id, ready, x} x 8 */
extern int   DAT_10ac5894;
extern int   DAT_10ac5bec;          /* we were booted */
extern int   DAT_10ac5bac;          /* chosen weather/track kind */
extern HWND  DAT_105bc72c;          /* chat window */
extern char  s_Host_set_ready_message_received__100a5bc8[];
extern char  s_APPMSG_PLAYERREADY__Set___d__ID__100a5ba0[];
extern char  s_APPMSG_HOSTSTARTED__received_100a5b80[];
extern char  s_APPMSG_BOOTPLAYER__received_100a5b60[];
extern char  s_left_the_race__1007b2d8[];
extern char  s_returned_to_race_lobby__1007b2bc[];
extern char  s_finished__s_100a5b50[];
extern char *PTR_s_First__100aa3e8[];

/* T2 2026-09-25 (hand, several thousand fn.py/micro-TU/IL-surgery compiles):
 * 908/913 B of code (the 36-byte jump table follows), 268/270 insns; the
 * only residue is the zero web (RESIDUE 2).  The compiler is plain C
 * (/O2, frameless; /TP and /TP /GX change nothing).  What the bytes taught:
 * the case bodies sit in SOURCE order 0,1,2,5,3,4,6,7,8; the four weather
 * arms stay separate only with the literal 4..7 in the call; the host-ready
 * scan is a pointer walk with a side counter; the net-record tests are on
 * POINTER fields; the text handle is an LPARAM.  2026-09-25: `pText = 0`
 * sits ABOVE the started test (the original stores it before the `jne`),
 * and the tail posts first (`if (hwnd) { PostMessage; return; } free`) --
 * the entry and the whole tail now match.
 * CASE 8 CALLS A TWIN (RESIDUE 1 SOLVED, 2026-09-25).  The original gives
 * each of the five chat calls its own `call` and shares only the
 * `add esp,0x14` (cases 0,1,6,7 `jmp` to it, case 8 falls into it).  VC5's
 * tail merge (C2.EXE 0x00434172, tuple equality 0x004332A4) merges a call
 * whenever the call tuples compare equal -- same opcode, type and operand
 * lists, the callee's SYMBOL entry among them.  The front end's IL (captured
 * with a wrapper C2) gives every same-name spelling the same symbol index:
 * casts, (*f), block-scope/typed/implicit declarations, inline wrappers,
 * result use, dead code, #line, labels, case-8 shapes, flags, all four VC5
 * C2 builds.  Editing the IL so case 8 alone has a different callee
 * reproduces the original exactly, and so does naming a different function
 * in C.  So case 8 names a twin of FUN_10036a30 whose identical body the
 * linker folded onto 0x10036A30 (LINK 5.0 folds identical COMDATs under
 * plain /OPT:REF, verified; the unfolded identical bodies elsewhere in the
 * DLL fit TUs built without /Gy).  The twin's real name is not recoverable;
 * reloc_learn maps it from the original call site at 0x100092D6.
 * RESIDUE 2, one zero web: our `xor ecx,ecx` at the entry serves the
 * pText store, the started test, the case-0 push, both net-field tests,
 * `v >= 0` and the scan counter; the original zeroes the counter alone,
 * inside case 2.  Minimal trigger (bisected): the entry store + `i = 0` +
 * `&pText` passed in case 0 or 1 + a call inside the player-ready loop;
 * drop any one and the web goes.  Any counter-CONTROLLED loop (`i < 8`,
 * `i != 8`, `++i < 8`) also kills it but tests the counter, not the
 * pointer; index forms strength-reduce to an OFFSET walk (`xor eax,eax;
 * cmp eax,0x60`), not the original's absolute one.  Dead: counter width
 * (char works only by going 8-bit), register/static/block scope, sharing
 * `v`, BrSlot struct walk, comma-init orders, pointer-difference index,
 * pText as a void or char pointer, array, struct, volatile, initialiser
 * or memset, the
 * store in both arms, header and neighbour-function context. */
/* WHAT IT DOES: handles one application-level DirectPlay message before the
 * race has started -- chat lines, a player marking ready, the host starting
 * or booting someone, a weather pick, a player leaving or returning, a
 * finishing position -- and posts any chat text it produced to the lobby
 * window (freeing it when there is no window). Once the host has started,
 * messages go to the in-race handler instead, or to the race-message parser
 * when the connection is not yet marked live. */
/* @t4-pass 0x10009010 1 2026-09-13 probes 16 bytes 896 insns 269 regions 1 rows 13 census no  (hand, fn.py variants: case order, literal arms, scan loop forms, pText typing/placement, compare spellings, counter placement) */
/* @implements 0x10009010 glide BrDpAppMsgHandle */
void BrDpAppMsgHandle(int *pNet, int *pMsg, int a3, int idFrom, int a5)
{
    LPARAM  pText;
    char    szDbg[260];
    char    szLine[1024];
    int     i;
    int    *p;
    int     v;

    pText = 0;
    if (DAT_10ac5be4 == 0) {
        switch (*pMsg) {
        case 0x60000000:
            FUN_10036a30(*pNet, idFrom, (LPCSTR)(pMsg + 1), (LPCVOID *)&pText, 0);
            break;
        case 0x60000001:
            FUN_10036a30(*pNet, idFrom, (LPCSTR)(pMsg + 1), (LPCVOID *)&pText, 1);
            break;
        case 0x60000002:
            if (idFrom == 1) {
                i = 0;
                p = &DAT_10ac5890;
                while ((int)p < (int)&DAT_10ac5890 + 0x60) {
                    if (*p == -1) {
                        (&DAT_10ac5890)[i * 3] = 1;
                        (&DAT_10ac5894)[i * 3] = 1;
                        sprintf(szDbg, s_Host_set_ready_message_received__100a5bc8, 1);
                        OutputDebugStringA(szDbg);
                        break;
                    }
                    p += 3;
                    i++;
                }
            } else {
                p = &DAT_10ac5894;
                do {
                    if (p[-1] == idFrom) {
                        *p = pMsg[1];
                        sprintf(szDbg, s_APPMSG_PLAYERREADY__Set___d__ID__100a5ba0, pMsg[1], idFrom);
                        OutputDebugStringA(szDbg);
                    }
                    p += 3;
                } while ((int)p < (int)&DAT_10ac5894 + 0x60);
            }
            break;
        case 0x60000005:
            v = pMsg[1];
            if (v == 4) {
                FUN_1006ba60(4, 0x200020);
                DAT_10ac5bac = v;
            } else if (v == 5) {
                FUN_1006ba60(5, 0x200020);
                DAT_10ac5bac = v;
            } else if (v == 6) {
                FUN_1006ba60(6, 0x200020);
                DAT_10ac5bac = v;
            } else if (v == 7) {
                FUN_1006ba60(7, 0x200020);
                DAT_10ac5bac = v;
            }
            break;
        case 0x60000003:
            DAT_10ac5be4 = 1;
            sprintf(szDbg, s_APPMSG_HOSTSTARTED__received_100a5b80);
            OutputDebugStringA(szDbg);
            return;
        case 0x60000004:
            if (pNet[2] == pMsg[1]) {
                DAT_10ac5bec = 1;
            }
            sprintf(szDbg, s_APPMSG_BOOTPLAYER__received_100a5b60);
            OutputDebugStringA(szDbg);
            break;
        case 0x60000006:
            if (idFrom == pMsg[1]) {
                FUN_10036a30(*pNet, idFrom, s_left_the_race__1007b2d8, (LPCVOID *)&pText, 1);
            }
            break;
        case 0x60000007:
            if (idFrom == pMsg[1]) {
                FUN_10036a30(*pNet, idFrom, s_returned_to_race_lobby__1007b2bc, (LPCVOID *)&pText, 1);
            }
            break;
        case 0x60000008:
            if (idFrom == 1) {
                v = pMsg[2];
                if (v >= 0 && v < 8) {
                    sprintf(szLine, s_finished__s_100a5b50, PTR_s_First__100aa3e8[v]);
                    BrChatLineFinishTwin(*pNet, pMsg[1], szLine, (LPCVOID *)&pText, 1);
                }
            }
            break;
        }
        if (pText != 0) {
            if (DAT_105bc72c != (HWND)0) {
                PostMessageA(DAT_105bc72c, 0x501, 0, pText);
                return;
            }
            GlobalUnlock(GlobalHandle((LPCVOID)pText));
            GlobalFree(GlobalHandle((LPCVOID)pText));
            return;
        }
    } else if (((void **)pNet)[4] == 0) {
        if (((void **)pNet)[3] != 0) {
            FUN_1002f790(pNet, pMsg, a3, idFrom, a5);
            return;
        }
        FUN_100038f0(pNet, pMsg, a3, idFrom, a5);
    }
}

#endif /* BR_MATCHING_BUILD */
