/* br_hudentrants.c -- the multiplayer entrant list on the HUD (0x10014E00).
 *
 * Fresh transcription from build/ghidra_decomp/0x10014e00.c against the
 * original bytes, 2026-09-13.  Matching arm only.
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <stdio.h>

int         BrSetGlobal_ABB30(int param_1);                 /* 0x100168B0 */
void        BrSub_10019280(void);                           /* 0x10016840 */
int         BrNetSlotGetF02CBiased(int i);                  /* 0x100062B0 */
unsigned    BrSub100714D0(int index);                       /* 0x1006A440 */
int         BrSub10071510(int i);                           /* 0x1006A480 */
int         BrNetSlotGetF974(int param_1);                  /* 0x10006300 */
const char *BrHudFormatGapString(const void *aCars, int i); /* 0x100151F0 */
void        BrTextDraw(const char *psz, int x, int y);      /* 0x100168C0 */

extern int  DAT_100bcc04;           /* list enabled */
extern int  DAT_10226a48;           /* net mode */
extern int  g_brRaceNCar;           /* 0x100B2F04 */
extern int  DAT_118eeee0;           /* show ping column */
extern int  g_brRaceReplay;         /* 0x105CCB88 */
extern char s___55_d__100a6ba0[];   /* "%%55%d." */
extern char s___55_s_100a6b98[];    /* "%%55%s"  */
extern char s___y1_d__100a6c34[];   /* "%%y1%d." */
extern char s___ry_d__100a6c2c[];   /* "%%ry%d." */
extern char s___11_d__100a6c3c[];   /* "%%11%d." */
extern char s___11_s_100a6bd4[];    /* "%%11%s"  */
extern char s___x_02x_02x_02x_s__s_100a6bbc[];
extern char s___x_02x_02x_02x_s_100a6ba8[];
extern char s___11_s__dms__c_c_100a6c18[];
extern char s___x_02x_02x_02x_s__s__dms__c_c_100a6bf8[];
extern char s___x_02x_02x_02x_s__dms__c_c_100a6bdc[];

/* WHAT IT DOES: draws the multiplayer standings list on the HUD -- one line
 * per car with its position number and name, the local car and the AI cars
 * in their own colours, a human opponent in the team colour taken from the
 * car record with the gap to the leader beside it, and, when the ping column
 * is on, the connection's round-trip time plus two quality marks derived
 * from the packet-loss counters. Cars flagged as spectators get the plain
 * grey line. */
/* Byte-exact 2026-09-13 (fresh, 4 fn.py compiles).  Three source facts
 * carried the last 7 bytes: the `+0xF08 != 0` arm and the ping-column arm
 * are the IF bodies (VC5 lays the if body inline and the else out of line,
 * which is where the original puts the plain "%%55" and no-ping arms);
 * `isLocal` is declared before `x` so the two land in the frame slots the
 * original spends (x at +8, the F02C flag at +0xC); and the loss counter
 * `v` is a signed int (`cmp eax,3; jge`).  Each arm carries its own
 * sprintf call -- VC5 cross-jumps the identical `call ebp` tails itself. */
/* @implements 0x10014E00 glide BrHudDrawEntrants */
void BrHudDrawEntrants(int *pScr, int cars)
{
    int  yBase;
    int  i;
    int  isLocal;
    int  x;
    char buf[256];
    int  car;
    int  y;
    int  c1;
    int  c2;
    int  v;

    if (DAT_100bcc04 != 0) {
        BrSetGlobal_ABB30(0xf);
        BrSub_10019280();
        x     = pScr[0] + 0x10;
        yBase = pScr[1] + 0x1d;
        if (DAT_10226a48 != 0 && g_brRaceNCar != 0) {
            i = 0;
            if (g_brRaceNCar > 0) {
                do {
                    car = cars + i * 0x2b68;
                    if (*(int *)(cars + 0xf08 + i * 0x2b68) != 0) {
                        isLocal = BrNetSlotGetF02CBiased(*(int *)(car + 0x144));
                        if (isLocal != 0) {
                            sprintf(buf, s___11_d__100a6c3c, *(int *)(car + 0xff8) + 1);
                        } else if (i == 0) {
                            sprintf(buf, s___y1_d__100a6c34, *(int *)(cars + 0xff8) + 1);
                        } else {
                            sprintf(buf, s___ry_d__100a6c2c, *(int *)(car + 0xff8) + 1);
                        }
                        BrTextDraw(buf, x, *(int *)(car + 0xff8) * 0x10 + 0x14 + yBase);
                        if (DAT_118eeee0 != 0) {
                            c2 = 0x20;
                            c1 = 0x20;
                            if (DAT_10226a48 > 1) {
                                v = BrSub100714D0(*(int *)(car + 0x144)) & 0x3f;
                                if (v < 3) {
                                    c2 = 0x2a;
                                    c1 = 0x2a;
                                } else if (v == 3) {
                                    v = BrSub10071510(*(int *)(car + 0x144)) & 0x3f;
                                    c1 = (v != 3 ? 10 : 0) + 0x20;
                                }
                            }
                            if (isLocal != 0) {
                                sprintf(buf, s___11_s__dms__c_c_100a6c18, car + 0x148,
                                        BrNetSlotGetF974(*(int *)(car + 0x144)), c1, c2);
                            } else if ((*(unsigned char *)(*(int *)(car + 0xf00) + 0x68) & 1) == 0
                                       && g_brRaceReplay == 0) {
                                sprintf(buf, s___x_02x_02x_02x_s__s__dms__c_c_100a6bf8,
                                        (unsigned)*(unsigned char *)(car + 0x29ac),
                                        (unsigned)*(unsigned char *)(car + 0x29ad),
                                        (unsigned)*(unsigned char *)(car + 0x29ae),
                                        car + 0x148,
                                        BrHudFormatGapString((const void *)cars, i),
                                        BrNetSlotGetF974(*(int *)(car + 0x144)), c1, c2);
                            } else {
                                sprintf(buf, s___x_02x_02x_02x_s__dms__c_c_100a6bdc,
                                        (unsigned)*(unsigned char *)(car + 0x29ac),
                                        (unsigned)*(unsigned char *)(car + 0x29ad),
                                        (unsigned)*(unsigned char *)(car + 0x29ae),
                                        car + 0x148,
                                        BrNetSlotGetF974(*(int *)(car + 0x144)), c1, c2);
                            }
                            BrTextDraw(buf, x + 0x10, *(int *)(car + 0xff8) * 0x10 + 0x14 + yBase);
                        } else {
                            if (isLocal != 0) {
                                sprintf(buf, s___11_s_100a6bd4, car + 0x148);
                            } else if ((*(unsigned char *)(*(int *)(car + 0xf00) + 0x68) & 1) == 0
                                       && g_brRaceReplay == 0) {
                                sprintf(buf, s___x_02x_02x_02x_s__s_100a6bbc,
                                        (unsigned)*(unsigned char *)(car + 0x29ac),
                                        (unsigned)*(unsigned char *)(car + 0x29ad),
                                        (unsigned)*(unsigned char *)(car + 0x29ae),
                                        car + 0x148,
                                        BrHudFormatGapString((const void *)cars, i));
                            } else {
                                sprintf(buf, s___x_02x_02x_02x_s_100a6ba8,
                                        (unsigned)*(unsigned char *)(car + 0x29ac),
                                        (unsigned)*(unsigned char *)(car + 0x29ad),
                                        (unsigned)*(unsigned char *)(car + 0x29ae),
                                        car + 0x148);
                            }
                            BrTextDraw(buf, x + 0x10, *(int *)(car + 0xff8) * 0x10 + 0x14 + yBase);
                        }
                    } else {
                        sprintf(buf, s___55_d__100a6ba0, *(int *)(car + 0xff8) + 1);
                        BrTextDraw(buf, x, *(int *)(car + 0xff8) * 0x10 + 0x14 + yBase);
                        sprintf(buf, s___55_s_100a6b98, car + 0x148);
                        BrTextDraw(buf, x + 0x10, *(int *)(car + 0xff8) * 0x10 + 0x14 + yBase);
                    }
                    i++;
                } while (i < g_brRaceNCar);
            }
        }
    }
}

#endif /* BR_MATCHING_BUILD */
