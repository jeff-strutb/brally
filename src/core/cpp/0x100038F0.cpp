/* WHAT IT DOES: consumes one received network packet and applies every
 * command in it to the local peer table: a peer's join/state header (name,
 * flags, id, with the whole slot reset when a new peer takes it, and the
 * local player's own bookkeeping when the header is its own), the sixteen
 * nibble pairs of the lobby table, a full or delta car-state sample slotted
 * into the peer's eight-entry ring (with the interpolation base picked from
 * the two newest full samples), the nine race-control messages of command
 * 0x60 (lobby return, kick, phase changes, leave, finish -- each formatting
 * a chat line from the peer's name), a timing pair, and the host's session
 * header; an unknown command is reported on stderr and ends the packet. */
/* @implements 0x100038F0 glide FUN_100038f0
 * @cpp_kind free
 * @cpp_symbol ?FUN_100038f0@@YAXPAX0HH@Z
 *
 * 3799 B cdecl EH-frame packet dispatcher. The reader is a 0x214-byte
 * packet object (bit-stream header + 0x200 payload) constructed on the
 * frame with a THISCALL ctor (two stack args, `ret 8`) and torn down by
 * the unwind action (the out-of-line nop 0x10008D60); every read is a
 * thiscall member, which is what routes this function to the C++ lane
 * (same evidence as the car-state codec pair).
 *
 * T2 2026-09-13 (C++ lane, first transcription): 3799 B orig, 4240 B ours
 * of which ~312 B are the switch tables that follow the ret in .text (the
 * cpp score counts them; divergence.py decodes them as code). Real residue
 * ~130 B in ~20 regions. What is already proven:
 *   - every slot field is its OWN global symbol plus `slot * 0x978`
 *     (`SLOTF`/`SLOTA`): a struct array or a (char *)base + off CSEs the
 *     full address into a register, the original CSEs only the product;
 *   - the two decode arms are `>= 2` first (big arm falls through, the
 *     scratch decode sits at the end);
 *   - the timeout formula reads DAT_10226a2c ONCE into a local (`mul esi`),
 *     `x % 100 / 0x21 + x / 100 * 3`;
 *   - the reset loop is a pointer walk with a counted `do { } while (--n)`,
 *     not `for (i < 8)` (VC5 turns that into three rep stosd);
 *   - `nName = i` is stored BEFORE the NUL at szName[0x18].
 * Open walls, all allocation:
 *   - the original never hoists anything out of the packet loop (`t`,
 *     nMode, the import addresses are re-read from memory in every case);
 *     ours hoists `t` into ebp and the WaitForSingleObject import into
 *     esi at the loop head. Dead: t as a union, as a byte array behind an
 *     int cast, volatile t, volatile nMode (all measured). Compiling WITHOUT
 *     /Oi (strcpy/strcat as calls) makes 0x2f..0x337 byte-exact INCLUDING
 *     the frame slots, but the original's strcpy/strcat ARE inline, and
 *     `#pragma intrinsic` restores the /O2 shape -- the accident says the
 *     wall is register pressure in case 0, not the loop shape.
 *   - frame 0x790 vs 0x794: one 4-byte temp short next to the fild qword
 *     temp of case 0x80 (the original zeroes [esp+0x44] as well as the
 *     high dword at [esp+0x4c]).
 *   - slot order of the thirteen scalar locals is NOT name-keyed at /O2
 *     (a full rename gives identical bytes) and not declaration-keyed
 *     (declaring them in the original's frame order changes nothing).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include <stdio.h>
#endif

struct BrNetHdr {
    int f00;
    int f04;
    int f08;
};

class BrNetPacket {
public:
    BrNetPacket(void *pBuf, int nBytes);    /* 0x1006CDA0 */
    ~BrNetPacket();                         /* 0x10008D60 (nop) */
    void          Reset();                  /* 0x1006CDD0 */
    unsigned char ReadU8();                 /* 0x1006CE00 */
    unsigned short ReadU16();               /* 0x1006CE20 */
    int           ReadU24();                /* 0x1006CE50 */
    int           ReadS32();                /* 0x1006CE80 */
    int           AtEnd();                  /* 0x1006CF80 */
    BrNetHdr     *GetHdr();                 /* 0x1006D190 */

    int            readBit;                 /* +0x00 */
    int            readByte;                /* +0x04 */
    int            writeBit;                /* +0x08 */
    int            writeByte;               /* +0x0C */
    unsigned char *pBuf;                    /* +0x10 */
    unsigned char  payload[0x200];          /* +0x14 */
};

typedef char chk_pkt[sizeof(BrNetPacket) == 0x214 ? 1 : -1];

struct BrCarState {
    char raw[0xA0];
};

struct BrNetSlot {
    void      *hMutex;        /* +0x000 */
    int        f004;          /* +0x004 */
    int        f008;          /* +0x008 */
    unsigned   f00C[8];       /* +0x00C */
    int        f02C;          /* +0x02C */
    int        f030;          /* +0x030 */
    char       f034[4];       /* +0x034 */
    int        f038[8];       /* +0x038 */
    BrCarState cars[8];       /* +0x058 */
    int        f558;          /* +0x558 */
    int        f55C;          /* +0x55C */
    int        f560;          /* +0x560 */
    int        f564;          /* +0x564 */
    int        f568;          /* +0x568 */
    int        f56C;          /* +0x56C */
    char       szName[0x400]; /* +0x570 */
    int        f970;          /* +0x970 */
    int        f974;          /* +0x974 */
};

typedef char chk_slot[sizeof(BrNetSlot) == 0x978 ? 1 : -1];

struct BrPeerRec {
    int  id;                  /* +0x0000 */
    char pad[0x2B68 - 4];
};

extern "C" {
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern void *     DAT_1021ce58;
extern int        DAT_1021ce5c;
extern int        DAT_1021ce60;
extern unsigned   DAT_1021ce64[];
extern int        DAT_1021ce84;
extern int        DAT_1021ce88;
extern char       DAT_1021ce8c[];
extern int        DAT_1021ce90[];
extern BrCarState DAT_1021ceb0[];
extern int        DAT_1021d3b0;
extern int        DAT_1021d3b4;
extern int        DAT_1021d3b8;
extern int        DAT_1021d3bc;
extern int        DAT_1021d3c0;
extern int        DAT_1021d3c4;
extern char       DAT_1021d3c8[];
extern int        DAT_1021d7c8;
extern int        DAT_1021d7cc;
extern BrPeerRec  DAT_10af134c[];        /* the peer records, stride 0x2B68 */
extern int        DAT_100b2f04;          /* peer count */
extern int        DAT_1007b264;          /* local slot */
extern void      *DAT_1021ce4c;
extern int        DAT_1021c900;
extern void      *DAT_10226a64;
extern int        DAT_1021ce40;
extern unsigned   DAT_10226a2c;
extern void      *DAT_10226a5c;
extern int        DAT_1021c904;
extern unsigned   DAT_1021c8c0[];
extern void      *DAT_1021ce54;
extern int        DAT_102265d8;
extern void      *DAT_10226a34;
extern int        DAT_10226624;
extern int        DAT_10226a50;
extern int        DAT_105ccb5c;
extern int        DAT_105ccb80;
extern void      *DAT_1021c90c;
extern int        DAT_1021ce44;
extern void      *DAT_1021c81c;
extern unsigned   DAT_10226a30;
extern void      *DAT_10226a58;
extern int        DAT_1021ce00[];
extern void      *DAT_10226a54;
extern int        DAT_10226a28;
extern int        DAT_10ac5bec;
extern void      *DAT_10226a60;
extern int        DAT_1021ce48;
extern int        DAT_1021cdb8[];
extern int        DAT_1021cdf8;
extern int        DAT_100b3014;
extern int        DAT_10226e80;
extern int        DAT_1021ce50;
extern int        DAT_1021cdb0;
extern int        DAT_10226a40;
extern int        DAT_10226a3c;
extern char       DAT_10af3bb4;
extern char       DAT_10af3bb5;
extern char       DAT_10af3bb6;
extern char       DAT_10b71648;
extern char      *PTR_s_First__100aa3e8[];
extern char       s_Error__unknown_command__02X_rece_1007b26c[];
extern char       s__s_finished__s_1007b2ac[];
extern char       s_returned_to_race_lobby__1007b2bc[];
extern char       s_left_the_race__1007b2d8[];
extern char       s_booted_you_from_the_game__1007b2e8[];
extern char       DAT_1007b304[];

int   FUN_100037d0(void);
void  FUN_10004ad0(void *, unsigned, unsigned, char, char, char, int, char *, unsigned, int);
void  BrCarStateDecode(BrCarState *, BrNetPacket *);            /* 0x10007230 */
void  BrCarStateDecodeDelta(BrCarState *, BrCarState *, BrNetPacket *); /* 0x10007750 */
void  BrCarStateLerp(BrCarState *, float, BrCarState *, BrCarState *);  /* 0x10007D50 */
int   FUN_10006060(int);
char *FUN_100061e0(int);
void  FUN_100038a0(char *);
void  FUN_100099d0(void);
unsigned FUN_10004d80(int);
void  FUN_10004dc0(int, int);
int   FUN_1006e280(void);
void  FUN_10004c80(void *, int);
void  FUN_10003810(int, int, int, unsigned);
void  FUN_10004d30(void);
void  FUN_10004900(void *, unsigned, char, char, char, char *, int);
}

#define SLOTF(sym, T) (*(T *)((char *)&sym + slot * 0x978))
#define SLOTA(sym, T) ((T *)((char *)sym + slot * 0x978))

void FUN_100038f0(void *pNet, void *pBuf, int nBytes, int nMode)
{
    BrNetPacket pkt(pBuf, nBytes);
    int        bGo;
    unsigned char tb[4];
    unsigned   b0;
    unsigned   b1;
    int        id;
    int        kind;
    char       c2;
    int        n;
    char       c3;
    float      frac;
    char       c4;
    int        nName;
    unsigned char nib;
    int        dt;
    unsigned   cmd;
    unsigned   slot;
    int        i;
    int       *pRing;
    BrCarState *pCar;
    unsigned   x;
    BrNetHdr  *pHdr;
    BrPeerRec *pPeer;
    char      *psz;
    char       szName[0x400];
    BrCarState scratch2;
    BrCarState scratch;

    WaitForSingleObject(DAT_1021ce4c, 0xffffffff);
    bGo = DAT_1021c900;
    ReleaseMutex(DAT_1021ce4c);
    if (bGo) {
        pkt.Reset();
        (*(int *)tb) = pkt.ReadU24();
        WaitForSingleObject(DAT_10226a64, 0xffffffff);
        DAT_1021ce40 = FUN_100037d0();
        ReleaseMutex(DAT_10226a64);
        while (!pkt.AtEnd()) {
            cmd = pkt.ReadU8();
            slot = cmd & 0xf;
            switch (cmd & 0xe0) {
            case 0x00:
                if (nMode != 1)
                    goto done;
                b0 = pkt.ReadU8();
                b1 = pkt.ReadU8();
                c2 = pkt.ReadU8();
                c3 = pkt.ReadU8();
                c4 = pkt.ReadU8();
                id = pkt.ReadS32();
                nName = 0;
                kind = b0 & 0x3f;
                if (kind <= 2) {
                    for (i = 0; i < 0x18; i++)
                        szName[i] = pkt.ReadU8();
                    nName = i;
                    szName[0x18] = 0;
                }
                if (kind == 4)
                    DAT_10226a2c = pkt.ReadU24();
                WaitForSingleObject(SLOTF(DAT_1021ce58, void *), 0xffffffff);
                if (slot != (unsigned)DAT_1007b264) {
                    if (SLOTF(DAT_1021ce84, int) != (int)b0 && kind == 2) {
                        WaitForSingleObject(DAT_10226a5c, 0xffffffff);
                        DAT_1021c904 += 1;
                        DAT_1021c8c0[DAT_1021c904] = slot;
                        ReleaseMutex(DAT_10226a5c);
                        SLOTF(DAT_1021ce60, int) = 0;
                        memset(SLOTA(DAT_1021ce64, unsigned), 0, 8 * sizeof(unsigned));
                        SLOTF(DAT_1021ce84, int) = 0;
                        pRing = SLOTA(DAT_1021ce90, int);
                        pCar = SLOTA(DAT_1021ceb0, BrCarState);
                        n = 8;
                        do {
                            *pRing = 0;
                            memset(pCar, 0, sizeof(BrCarState));
                            pRing++;
                            pCar++;
                            n--;
                        } while (n != 0);
                        SLOTF(DAT_1021d3b0, int) = 0;
                        SLOTF(DAT_1021d3b4, int) = 0;
                        SLOTF(DAT_1021d3b8, int) = -1;
                        SLOTF(DAT_1021d3c0, int) = 0;
                        SLOTF(DAT_1021d3c4, int) = 0;
                        SLOTF(DAT_1021d3bc, int) = 0;
                        SLOTF(DAT_1021d7cc, int) = 0;
                    }
                }
                if (slot == (unsigned)DAT_1007b264) {
                    if (kind == 3) {
                        WaitForSingleObject(DAT_1021ce54, 0xffffffff);
                        DAT_102265d8 = 0;
                        ReleaseMutex(DAT_1021ce54);
                    }
                    if (slot == (unsigned)DAT_1007b264) {
                        if (b0 & 0x80) {
                            WaitForSingleObject(DAT_10226a34, 0xffffffff);
                            DAT_10226624 = 0;
                            ReleaseMutex(DAT_10226a34);
                            DAT_10226a50 = 1;
                        }
                        if (b0 & 0x40) {
                            if (DAT_10226a50 != 0)
                                DAT_10226a50 = 0;
                            if (DAT_105ccb5c != 0)
                                DAT_105ccb80 = 1;
                            WaitForSingleObject(DAT_1021c90c, 0xffffffff);
                            DAT_1021ce44 = 0;
                            ReleaseMutex(DAT_1021c90c);
                        }
                        if (slot == (unsigned)DAT_1007b264 && kind == 4) {
                            unsigned now;

                            WaitForSingleObject(DAT_1021c81c, 0xffffffff);
                            now = FUN_100037d0();
                            x = DAT_10226a2c;
                            DAT_10226a30 = x % 100 / 0x21 + x / 100 * 3;
                            if (DAT_10226a30 > now + 0x5a)
                                DAT_10226a30 = now + 0x5a;
                            ReleaseMutex(DAT_1021c81c);
                        }
                    }
                }
                SLOTF(DAT_1021ce60, int) = (*(int *)tb);
                SLOTF(DAT_1021ce84, int) = b0;
                SLOTF(DAT_1021ce88, int) = b1;
                SLOTA(DAT_1021ce8c, char)[0] = c2;
                SLOTA(DAT_1021ce8c, char)[1] = c3;
                SLOTA(DAT_1021ce8c, char)[2] = c4;
                SLOTF(DAT_1021ce5c, int) = id;
                if (nName)
                    strcpy(SLOTA(DAT_1021d3c8, char), szName);
                FUN_10004ad0(pNet, slot, b1, c2, c3, c4, id, szName, b0, 0x10);
                ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                break;

            case 0x20: {
                int *p;

                if (nMode != 1)
                    goto done;
                WaitForSingleObject(DAT_10226a58, 0xffffffff);
                for (p = DAT_1021ce00; (int)p < (int)&DAT_1021ce00[16]; p += 2) {
                    nib = pkt.ReadU8();
                    p[1] = nib >> 4;
                    p[0] = nib & 0xf;
                }
                ReleaseMutex(DAT_10226a58);
                break;
            }

            case 0x40: {
                int      best;
                unsigned bestT;
                int      d;

                WaitForSingleObject(SLOTF(DAT_1021ce58, void *), 0xffffffff);
                if ((SLOTF(DAT_1021ce84, int) & 0x3f) >= 2) {
                    best = 0;
                    bestT = 0xffffffff;
                    for (i = 0; i < 8; i++) {
                        if (SLOTA(DAT_1021ce64, unsigned)[i] <= bestT) {
                            best = i;
                            bestT = SLOTA(DAT_1021ce64, unsigned)[i];
                        }
                    }
                    SLOTF(DAT_1021d3b4, int) = best;
                    SLOTF(DAT_1021d3b0, int) += 1;
                    SLOTA(DAT_1021ce64, unsigned)[best] = (*(int *)tb);
                    SLOTA(DAT_1021ce90, int)[SLOTF(DAT_1021d3b4, int)] = 0x40;
                    BrCarStateDecode(&SLOTA(DAT_1021ceb0, BrCarState)[SLOTF(DAT_1021d3b4, int)], &pkt);
                    d = (DAT_1021ce40 - (*(int *)tb)) * 2;
                    SLOTF(DAT_1021d7cc, int) = (d % 3) * 0x21 + (d / 3) * 100;
                    ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                } else {
                    BrCarStateDecode(&scratch, &pkt);
                    ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                }
                break;
            }

            case 0x60:
                switch (0x60000000 | (((*(int *)tb) & 0xff) << 16) | ((*(int *)tb) & 0xff00) | tb[2]) {
                case 0x60000000:
                case 0x60000001:
                    szName[0] = 0;
                    if (DAT_100b2f04 > 0) {
                        pPeer = DAT_10af134c;
                        for (i = 0; i < DAT_100b2f04; i++, pPeer++) {
                            if (nMode == FUN_10006060(pPeer->id)) {
                                psz = FUN_100061e0(pPeer->id);
                                strcpy(szName, psz);
                                strcat(szName, DAT_1007b304);
                                break;
                            }
                        }
                    }
                    pHdr = pkt.GetHdr();
                    strcat(szName, (char *)&pHdr->f04);
                    FUN_100038a0(szName);
                    goto done;

                case 0x60000004:
                    pHdr = pkt.GetHdr();
                    if (pHdr->f04 == FUN_10006060(DAT_1007b264)) {
                        FUN_100099d0();
                        DAT_10ac5bec = 1;
                        if (DAT_100b2f04 > 0) {
                            pPeer = DAT_10af134c;
                            for (i = 0; i < DAT_100b2f04; i++, pPeer++) {
                                if (nMode == FUN_10006060(pPeer->id)) {
                                    psz = FUN_100061e0(pPeer->id);
                                    strcpy(szName, psz);
                                    strcat(szName, s_booted_you_from_the_game__1007b2e8);
                                    FUN_100038a0(szName);
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000005:
                    pHdr = pkt.GetHdr();
                    WaitForSingleObject(DAT_10226a54, 0xffffffff);
                    switch (pHdr->f04) {
                    case 4:
                        DAT_10226a28 = 0x14;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 5:
                        DAT_10226a28 = 0x15;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 6:
                        DAT_10226a28 = 0x16;
                        ReleaseMutex(DAT_10226a54);
                        break;
                    case 7:
                        DAT_10226a28 = 0x17;
                    default:
                        ReleaseMutex(DAT_10226a54);
                        break;
                    }
                    goto done;

                case 0x60000006:
                    pHdr = pkt.GetHdr();
                    if (pHdr->f04 == nMode) {
                        if (DAT_100b2f04 > 0) {
                            pPeer = DAT_10af134c;
                            for (i = 0; i < DAT_100b2f04; i++, pPeer++) {
                                if (nMode == FUN_10006060(pPeer->id)) {
                                    if (FUN_10004d80(pPeer->id) & 0x3f) {
                                        WaitForSingleObject(DAT_10226a60, 0xffffffff);
                                        DAT_1021ce48 += 1;
                                        DAT_1021cdb8[DAT_1021ce48] = pPeer->id;
                                        ReleaseMutex(DAT_10226a60);
                                        FUN_10004dc0(i, 0);
                                        psz = FUN_100061e0(pPeer->id);
                                        strcpy(szName, psz);
                                        strcat(szName, s_left_the_race__1007b2d8);
                                        FUN_100038a0(szName);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000007:
                    pHdr = pkt.GetHdr();
                    if (pHdr->f04 == nMode) {
                        if (DAT_100b2f04 > 0) {
                            pPeer = DAT_10af134c;
                            for (i = 0; i < DAT_100b2f04; i++, pPeer++) {
                                if (nMode == FUN_10006060(pPeer->id)) {
                                    psz = FUN_100061e0(pPeer->id);
                                    strcpy(szName, psz);
                                    strcat(szName, s_returned_to_race_lobby__1007b2bc);
                                    FUN_100038a0(szName);
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                case 0x60000008:
                    pHdr = pkt.GetHdr();
                    if (nMode == 1) {
                        if (DAT_100b2f04 > 0) {
                            pPeer = DAT_10af134c;
                            for (i = 0; i < DAT_100b2f04; i++, pPeer++) {
                                if (pHdr->f04 == FUN_10006060(pPeer->id)) {
                                    int k = pHdr->f08;

                                    if (k >= 0 && k < 8) {
                                        psz = FUN_100061e0(pPeer->id);
                                        sprintf(szName, s__s_finished__s_1007b2ac, psz, PTR_s_First__100aa3e8[k]);
                                        FUN_100038a0(szName);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    goto done;

                default:
                    goto done;
                }

            case 0x80: {
                int      best, iNew, iPrev;
                unsigned bestT, tNew, tPrev;
                int      d;

                WaitForSingleObject(SLOTF(DAT_1021ce58, void *), 0xffffffff);
                if ((SLOTF(DAT_1021ce84, int) & 0x3f) >= 2) {
                    best = 0;
                    bestT = 0xffffffff;
                    for (i = 0; i < 8; i++) {
                        if (SLOTA(DAT_1021ce64, unsigned)[i] <= bestT) {
                            bestT = SLOTA(DAT_1021ce64, unsigned)[i];
                            best = i;
                        }
                    }
                    iNew = 0;
                    tNew = 0;
                    for (i = 0; i < 8; i++) {
                        if (SLOTA(DAT_1021ce90, int)[i] == 0x40 && tNew < SLOTA(DAT_1021ce64, unsigned)[i]) {
                            iNew = i;
                            tNew = SLOTA(DAT_1021ce64, unsigned)[i];
                        }
                    }
                    iPrev = 0;
                    tPrev = 0;
                    for (i = 0; i < 8; i++) {
                        if (SLOTA(DAT_1021ce90, int)[i] == 0x40 && tPrev < SLOTA(DAT_1021ce64, unsigned)[i] && i != iNew) {
                            iPrev = i;
                            tPrev = SLOTA(DAT_1021ce64, unsigned)[i];
                        }
                    }
                    dt = SLOTA(DAT_1021ce64, unsigned)[iNew] - SLOTA(DAT_1021ce64, unsigned)[iPrev];
                    if (dt == 0)
                        frac = 1.0f;
                    else
                        frac = (float)(unsigned)((*(int *)tb) - SLOTA(DAT_1021ce64, unsigned)[iPrev]) / dt;
                    SLOTF(DAT_1021d3b4, int) = best;
                    SLOTF(DAT_1021d3b0, int) += 1;
                    SLOTA(DAT_1021ce64, unsigned)[best] = (*(int *)tb);
                    SLOTA(DAT_1021ce90, int)[SLOTF(DAT_1021d3b4, int)] = 0x80;
                    BrCarStateLerp(&SLOTA(DAT_1021ceb0, BrCarState)[SLOTF(DAT_1021d3b4, int)], frac,
                                   &SLOTA(DAT_1021ceb0, BrCarState)[iPrev], &SLOTA(DAT_1021ceb0, BrCarState)[iNew]);
                    BrCarStateDecodeDelta(&SLOTA(DAT_1021ceb0, BrCarState)[SLOTF(DAT_1021d3b4, int)], &SLOTA(DAT_1021ceb0, BrCarState)[iNew], &pkt);
                    d = (DAT_1021ce40 - (*(int *)tb)) * 2;
                    SLOTF(DAT_1021d7cc, int) = (d % 3) * 0x21 + (d / 3) * 100;
                    ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                } else {
                    BrCarStateDecodeDelta(&scratch2, &scratch2, &pkt);
                    ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                }
                break;
            }

            case 0xc0: {
                int      a, b;
                unsigned short c;

                if (nMode != 1)
                    goto done;
                a = FUN_1006e280();
                b = pkt.ReadU24();
                c = pkt.ReadU16();
                WaitForSingleObject(SLOTF(DAT_1021ce58, void *), 0xffffffff);
                SLOTF(DAT_1021d7c8, int) = b;
                SLOTF(DAT_1021d7cc, int) = c;
                ReleaseMutex(SLOTF(DAT_1021ce58, void *));
                if (slot == (unsigned)DAT_1007b264) {
                    FUN_10004c80(pNet, (*(int *)tb));
                    FUN_10003810((*(int *)tb), a, b, c);
                }
                break;
            }

            case 0xe0:
                if (nMode != 1)
                    goto done;
                DAT_1007b264 = slot;
                FUN_10004d30();
                DAT_1021cdf8 = pkt.ReadU8();
                DAT_100b3014 = pkt.ReadU8();
                DAT_10226e80 = pkt.ReadU8();
                DAT_1021ce50 = pkt.ReadU16();
                DAT_1021cdb0 = pkt.ReadU8();
                DAT_10226a40 = pkt.ReadU8();
                DAT_10226a3c = pkt.ReadU8();
                FUN_10004900(pNet, slot, DAT_10af3bb4, DAT_10af3bb5, DAT_10af3bb6, &DAT_10b71648, 0x10);
                FUN_10004dc0(slot, 2);
                break;

            default:
                fprintf(stderr, s_Error__unknown_command__02X_rece_1007b26c, cmd & 0xe0, slot);
                goto done;
            }
        }
    }
done:
    ;
}
