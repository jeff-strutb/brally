/* cpak.c -- the Controller Pak save data
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrVolumesApply(void);
int BrRumbleInsertPrompt(int anyPad);
int func_8021CB4C();
void func_80223750(float param_1,float param_2);
void func_802237D0(float param_1,float param_2);
int BrSfxFadeDone(void);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
int BrTextSetColours();
void BrTextSetFont(int param_1);
void BrTextPrint();
void BrImageDrawAt(int *param_1,int param_2,int param_3);
int func_80246F90();
void BrPadConsume(unsigned int *param_1,unsigned int param_2);
void osSyncPrintf();
char * memcpy(char *param_1,char *param_2,int param_3);
int func_80261940(int param_1,unsigned char *param_2);
int func_80261CB0();
int func_80261F20(int param_1);
int func_80262370();
int func_80262540();
int func_80262660();
int func_802628C0(int param_1,short param_2,int param_3,int param_4,int param_5,int *param_6);
int func_80262A80();
int func_802635DC(unsigned int *param_1,int param_2,char param_3,unsigned int param_4,unsigned int param_5,int param_6);
int func_802639E0(unsigned int *param_1,int *param_2);
int func_80263B30();
extern int D_80216398;
extern int D_80270784;
extern int D_80271FA8;
extern int D_802723D0;
extern int D_802724F0;
extern unsigned char D_802724F4;
extern unsigned char D_802724F8;
extern unsigned char D_802724FC;
extern int D_80272500;
extern int D_80272558;
extern int D_8027255C;
extern int D_80272560;
extern int D_80272564;
extern int D_80272568;
extern int D_8027256C;
extern int D_80272570;
extern int D_80272574;
extern int D_80272578;
extern int D_8027257C;
extern int D_80272580;
extern int D_80272584;
extern int D_80272588;
extern int D_80272D48;
extern int D_8028D0B0;
extern int D_8028D0C0;
extern int D_8028D0E0;
extern int D_8028D0F0;
extern short D_802A4BE8;
extern float D_802A8FE4;
extern float D_802A8FE8;
extern float D_802A9018;
extern float D_802A901C;
extern unsigned char D_80307F00;
extern int D_80307F01;
extern char D_80316420;
extern int D_8031642C;
extern unsigned char D_80316430;
extern unsigned char D_80316431;
extern char D_8031B1E8;
extern int D_8031C5BC;
extern int D_8031E64C;
extern int D_8036A8E0;
typedef struct { char raw[0x68]; } BrPfs;              /* an OSPfs */
typedef struct BrPfsState {    /* an OSPfsState, 0x20 bytes */
  unsigned int size;
  unsigned int company;         /* 0x04 */
  unsigned short game;          /* 0x08 */
  char pad0a[0x16];
} BrPfsState;
void func_802674D0(void *src, void *dst, int n);
int func_802677E0(BrPfs *pfs, int *maxFiles, int *used);
int func_80267930(BrPfs *pfs, int file, BrPfsState *state);
extern BrPfs D_80369EC0[2];
extern BrPfs D_8031A3F8[4];
extern char D_803163E0[];
extern int D_8026FF08;
/* -- end declarations -- */

/* WHAT IT DOES: Ask the player to swap the Controller Pak for the Rumble
 * Pak: draw the message box and its five lines, then answer 1 (and consume
 * the press) once a confirm button (mask 0x8030) is down on the player's
 * pad -- or on either of the first two pads when anyPad is set -- else 0. */
/* @implements 0x80214A88 tgr BrRumbleInsertPrompt */
int BrRumbleInsertPrompt(int anyPad)
{
  int i;

  BrTextHighlightOff();
  BrTextAlignLeft();
  BrTextSetFont(12);
  BrTextSetColours(0xff, 0xff, 0xff, 0xff, 0xf5, 0);
  func_80246F90(0x73, 0xb4, 0x19a, 0x98, 3, 0, 0, 0x80, 0x80, 0x80);
  BrTextPrint("IF THE RUMBLE PAK IS TO BE USED,", 0x41, 0x6a);
  BrTextPrint("REMOVE THE CONTROLLER PAK AND", 0x41, 0x77);
  BrTextPrint("INSERT THE RUMBLE PAK INTO THE", 0x41, 0x84);
  BrTextPrint("CONTROLLER.  PRESS THE A BUTTON", 0x41, 0x91);
  BrTextPrint("TO CONTINUE.", 0x41, 0x9e);
  for (i = 0; i < 2; i++) {
    if (anyPad == 0 && i != D_80271FA8) continue;
    if (*(unsigned int *)((char *)&D_8036A8E0 + i * 0x15c) & 0x8030) {
      BrPadConsume((unsigned int *)((char *)&D_8036A8E0 + i * 0x15c), 0x8030);
      return 1;
    }
  }
  return 0;
}

/* WHAT IT DOES: Probe every connected controller for a Rumble Pak: each one
 * that answers is marked present and its motor stopped.  Pak access is
 * flagged busy meanwhile.
 * RESIDUE (51): ours hoists the rumble-flag table's address into a saved
 * register; the ROM rebuilds it inside the loop (one fewer saved
 * register). */
/* @implements 0x80214BEC tgr BrRumbleProbe */
void BrRumbleProbe(void)
{
  int i;

  D_802A4BE8 = 0;
  for (i = 0; i < D_8026FF08; i++) {
    if (func_80262370(&D_80272D48, (int)&D_8031A3F8[i], i) == 0) {
      func_80261F20((int)&D_8031A3F8[i]);
      (&D_8031B1E8)[i] = 1;
    }
  }
  D_802A4BE8 = 1;
}

/* WHAT IT DOES: Check the Controller Pak for the save screens: initialise
 * it, and report when a different pak has been inserted, asking the player
 * to confirm before carrying on. */
/* @t4-pass 0x80214E0C 1 2026-09-26 compiles 17 best 1632 moved 7  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 2 2026-09-26 compiles 17 best 1639 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 3 2026-09-26 compiles 17 best 1637 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80214E0C 4 2026-09-26 compiles 41 best 1637 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80214E0C tgr BrCpakCheck */
int BrCpakCheck(int param_1,char param_2)
{
  int bVar1;
  unsigned int uVar2;
  unsigned int *puVar3;
  int iVar4;
  char cVar5;
  int local_7c;
  int local_78;
  int local_70;
  int local_74;
  int local_6c;
  int local_68;
  int local_64;
  int local_60;
  int local_5c;
  int local_58;
  int local_54;
  int local_50;
  int local_4c;
  int local_48;
  int local_44;
  int local_40;
  int local_3c;
  int local_38;
  int local_34;
  unsigned char local_2e;
  int local_2c;
  unsigned char local_2d;
  int *local_28;
  int local_20;
  int local_8;
  
  local_58 = D_80272558;
  local_54 = D_8027255C;
  local_50 = D_80272560;
  local_4c = D_80272564;
  local_68 = D_80272568;
  local_64 = D_8027256C;
  local_60 = D_80272570;
  local_5c = D_80272574;
  local_78 = D_80272578;
  local_74 = D_8027257C;
  local_70 = D_80272580;
  local_6c = D_80272584;
  D_802A4BE8 = 0;
  local_7c = D_80272588;
  if (D_802724F8 == '\0') {
    D_802724F8 = '\x01';
    D_802724F0 = 0;
    D_80316430 = '\0';
    D_80316431 = '\0';
  }
  if (param_1 == 0) {
    local_34 = 0x200;
    local_28 = &local_58;
  }
  else if (param_1 == 1) {
    local_34 = 0x3a00;
    local_28 = &local_68;
  }
  else if ((param_1 == 2) || (param_1 == 3)) {
    local_34 = 0x100;
    local_28 = &local_78;
  }
  switch(D_80316430) {
  case '\0':
    if (param_2 == '\0') {
      func_80223750(0,D_802A8FE4);
      func_802237D0(0,D_802A8FE8);
      D_80316430 = '\x01';
    }
    else {
      D_80316430 = '\x02';
    }
    break;
  case '\x01':
    iVar4 = BrSfxFadeDone();
    if ((iVar4 != 0) &&
       (cVar5 = D_80316431 + '\x01', bVar1 = D_80316431 == '\x03', D_80316431 = cVar5, bVar1))
    {
      D_80316430 = '\x02';
    }
    break;
  case '\x02':
    func_80261940(&D_80272D48,&local_2d);
    if (((unsigned int)local_2d & 1 << (D_80271FA8 & 0x1f)) == 0) {
      D_802724F0 = 1;
      D_80316430 = '\t';
    }
    else {
      osSyncPrintf("\nInitializing controller pak...\n");
      D_802724F0 = func_80261CB0(&D_80272D48,D_80271FA8 * 0x68 + -0x7fc96140,D_80271FA8);
      uVar2 = D_80271FA8;
      if (D_802724F0 == 0) {
        (&D_8031B1E8)[D_80271FA8] = 0;
        if ((&D_80316420)[uVar2] == '\0') {
          (&D_80316420)[uVar2] = '\x01';
          memcpy(uVar2 * 0x20 + -0x7fce9c20,uVar2 * 0x68 + -0x7fc96134,0x20);
          D_80316430 = '\x05';
        }
        else {
          iVar4 = func_80262540(uVar2 * 0x20 + -0x7fce9c20,uVar2 * 0x68 + -0x7fc96134,0x20);
          if (iVar4 == 0) {
            D_80316430 = '\x05';
          }
          else {
            memcpy(D_80271FA8 * 0x20 + -0x7fce9c20,D_80271FA8 * 0x68 + -0x7fc96134,0x20);
            D_80316430 = '\x04';
          }
        }
      }
      else if (D_802724F0 == 10) {
        iVar4 = func_80262370(&D_80272D48,D_80271FA8 * 0x68 + -0x7fce5c08,D_80271FA8);
        if (iVar4 == 0) {
          func_80261F20(D_80271FA8 * 0x68 + -0x7fce5c08);
          D_802724F0 = 9999;
          D_802724FC = '\x01';
          D_80316430 = '\t';
        }
        else {
          D_802724F0 = 0;
          D_80316430 = '\x03';
        }
      }
      else {
        (&D_8031B1E8)[D_80271FA8] = 0;
        D_80316430 = '\t';
      }
    }
    break;
  case '\x03':
    D_802724F0 = func_80262660(D_80271FA8 * 0x68 + -0x7fc96140);
    if (D_802724F0 == 0) {
      D_80316430 = '\x02';
    }
    else {
      D_80316430 = '\t';
    }
    break;
  case '\x04':
    if (param_2 == '\0') {
      BrTextHighlightOff();
      BrTextAlignLeft();
      BrTextSetFont(0xc);
      BrTextSetColours(0xff,0xff,0xff,0xff,0xf5,0);
      func_80246F90(0xaf,0xc3,0x122,0x89,3,0,0,0x80,0x80,0x80);
      BrTextPrint("A NEW CONTROLLER PAK",0x5f,0x71);
      BrTextPrint("WAS INSERTED.",0x5f,0x7e);
      BrTextPrint("THIS CONTROLLER PAK",0x5f,0x91);
      BrTextPrint("WILL BE USED.",0x5f,0x9e);
      if ((*(unsigned int *)(&D_8036A8E0 + D_80271FA8 * 0x15c) & 0x8030) != 0) {
        BrPadConsume(&D_8036A8E0 + D_80271FA8 * 0x15c,0x8030);
        D_80316430 = '\x05';
      }
    }
    else {
      D_80316430 = '\x05';
    }
    break;
  case '\x05':
    osSyncPrintf("Finding file...\n");
    D_802724F0 = func_802628C0(D_80271FA8 * 0x68 + -0x7fc96140,0x3544,0x4e475245,local_28,
                                &local_7c,&D_8031642C);
    if (D_802724F0 == 3) {
      func_80262A80(D_80271FA8 * 0x68 + -0x7fc96140);
      D_802724F0 = func_802628C0(D_80271FA8 * 0x68 + -0x7fc96140,0x3544,0x4e475245,local_28,
                                  &local_7c,&D_8031642C);
    }
    if (D_802724F0 == 0) {
      if (param_1 == 3) {
        D_80316430 = '\b';
      }
      else {
        D_80316430 = '\x06';
      }
    }
    else if (param_1 == 3) {
      D_80316430 = '\a';
    }
    else if (param_2 == '\0') {
      D_80316430 = '\t';
    }
    else {
      D_80316430 = '\f';
    }
    break;
  case '\x06':
    if (param_1 == 0) {
      osSyncPrintf("Loading season data...\n");
    }
    else if (1 == param_1) {
      osSyncPrintf("Loading ghost data...\n");
    }
    else if (param_1 == 2) {
      osSyncPrintf("Loading configuration...\n");
    }
    if (param_1 == 2) {
      D_802724F0 = func_802635DC(D_80271FA8 * 0x68 + -0x7fc96140,D_8031642C,0,0,0x80,
                                  D_80272500);
    }
    else {
      D_802724F0 = func_802635DC(D_80271FA8 * 0x68 + -0x7fc96140,D_8031642C,0,0,local_34,
                                  D_80272500);
    }
    if (D_802724F0 == 0) {
      if (param_1 == 0) {
        iVar4 = *(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090);
        local_48 = *(int *)(iVar4 + 0xd4);
        local_44 = *(int *)(iVar4 + 0xd8);
        local_40 = *(int *)(iVar4 + 0xdc);
        local_3c = *(int *)(iVar4 + 0xe0);
        local_38 = *(int *)(iVar4 + 0xe4);
        memcpy(D_8031C5BC,D_80272500,0x128);
        memcpy(D_8031E64C,D_80272500,0x128);
        *(int *)(*(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090) + 0xd4) = local_48;
        *(int *)(*(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090) + 0xd8) = local_44;
        *(int *)(*(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090) + 0xdc) = local_40;
        *(int *)(*(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090) + 0xe0) = local_3c;
        *(int *)(*(int *)(&D_8031C5BC + (D_80271FA8 ^ 1) * 0x2090) + 0xe4) = local_38;
        osSyncPrintf("Done!\n");
      }
      else if (param_1 == 1) {
        osSyncPrintf("Extracting ghost data...\n");
        D_80270784 = func_8021CB4C(&D_80307F00,0xde5c,D_80272500,2);
        *(short *)(D_8031C5BC + 0xce) =
             *(short *)(D_8031C5BC + 0xce) | (short)(1 << (D_80307F00 & 0x1f));
        *(short *)(D_8031C5BC + 0xcc) =
             *(short *)(D_8031C5BC + 0xcc) | (short)(1 << (D_80307F01 & 0x1f));
        osSyncPrintf("Done!\n");
      }
      else if (param_1 == 2) {
        memcpy(&D_802723D0,D_80272500,0xc);
        osSyncPrintf("Loading car equipment settings...\n");
        D_802724F0 = func_802635DC(D_80271FA8 * 0x68 + -0x7fc96140,D_8031642C,0,0x80,0x80,
                                    D_80272500);
        if (D_802724F0 == 0) {
          memcpy(&local_48,D_80272500,0x14);
          *(int *)(*(int *)(&D_8031C5BC + D_80271FA8 * 0x2090) + 0xd4) = local_48;
          *(int *)(*(int *)(&D_8031C5BC + D_80271FA8 * 0x2090) + 0xd8) = local_44;
          *(int *)(*(int *)(&D_8031C5BC + D_80271FA8 * 0x2090) + 0xdc) = local_40;
          *(int *)(*(int *)(&D_8031C5BC + D_80271FA8 * 0x2090) + 0xe0) = local_3c;
          *(int *)(*(int *)(&D_8031C5BC + D_80271FA8 * 0x2090) + 0xe4) = local_38;
          osSyncPrintf("Done!\n");
        }
        else {
          D_80316430 = '\t';
        }
      }
      if (D_80316430 != '\t') {
        if (param_1 == 2) {
          D_80316430 = '\v';
        }
        else if (D_802724FC == '\0') {
          D_80316430 = '\f';
        }
        else {
          D_80316430 = '\v';
        }
      }
    }
    else {
      D_80316430 = '\t';
    }
    break;
  case '\a':
    D_802724F0 = func_802639E0(D_80271FA8 * 0x68 + -0x7fc96140,&local_2c);
    if (D_802724F0 == 0) {
      if (local_2c < 0x100) {
        D_802724F0 = 7;
        D_80316430 = '\t';
      }
      else {
        osSyncPrintf("Allocating %d bytes for configuration...\n",0x100);
        D_802724F0 = func_80263B30(D_80271FA8 * 0x68 + -0x7fc96140,0x3544,0x4e475245,local_28,
                                    &local_7c,0x100,&D_8031642C);
        if (D_802724F0 == 3) {
          func_80262A80(D_80271FA8 * 0x68 + -0x7fc96140);
          D_802724F0 = func_80263B30(D_80271FA8 * 0x68 + -0x7fc96140,0x3544,0x4e475245,local_28,
                                      &local_7c,0x100,&D_8031642C);
        }
        if (D_802724F0 == 0) {
          D_80316430 = '\b';
        }
        else {
          D_80316430 = '\t';
        }
      }
    }
    else {
      D_80316430 = '\t';
    }
    break;
  case '\b':
    osSyncPrintf("Saving configuration...\n");
    memcpy(D_80272500,&D_802723D0,0xc);
    D_802724F0 = func_802635DC(D_80271FA8 * 0x68 + -0x7fc96140,D_8031642C,1,0,0x80,
                                D_80272500);
    if (D_802724F0 == 0) {
      iVar4 = *(int *)(&D_8031C5BC + D_80271FA8 * 0x2090);
      local_48 = *(int *)(iVar4 + 0xd4);
      local_44 = *(int *)(iVar4 + 0xd8);
      local_40 = *(int *)(iVar4 + 0xdc);
      local_3c = *(int *)(iVar4 + 0xe0);
      local_38 = *(int *)(iVar4 + 0xe4);
      memcpy(D_80272500,&local_48,0x14);
      D_802724F0 = func_802635DC(D_80271FA8 * 0x68 + -0x7fc96140,D_8031642C,1,0x80,0x80,
                                  D_80272500);
      if (D_802724F0 == 0) {
        osSyncPrintf("Done!\n");
        D_80316430 = '\v';
      }
      else {
        D_80316430 = '\t';
      }
    }
    else {
      D_80316430 = '\t';
    }
    break;
  case '\t':
    if (param_2 != '\0') {
      D_802A4BE8 = 1;
      D_80316430 = 0xc;
      return 0;
    }
    BrTextHighlightOff();
    BrTextAlignLeft();
    BrTextSetFont(0xc);
    BrTextSetColours(0xff,0xff,0xff,0xff,0xf5,0);
    if (D_802724F0 < 0xc) {
      switch(D_802724F0) {
      case 1:
      case 0xb:
        func_80246F90(0x94,0xd8,0x158,0x4c,3,0,0,0x80,0x80,0x80);
        BrTextPrint("CONTROLLER PAK NOT FOUND.",0x52,0x7c);
        if (param_1 == 3) {
          BrTextPrint("DATA CANNOT BE SAVED.",0x52,0x8a);
        }
        else {
          BrTextPrint("DATA CANNOT BE LOADED.",0x52,0x8a);
        }
        break;
      case 2:
        func_80246F90(0xaf,0xb9,0x122,0x89,3,0,0,0x80,0x80,0x80);
        BrTextPrint("A NEW CONTROLLER PAK",0x5f,0x6c);
        BrTextPrint("WAS INSERTED.",0x5f,0x79);
        BrTextPrint("THIS CONTROLLER PAK",0x5f,0x8c);
        BrTextPrint("WILL BE USED.",0x5f,0x99);
        break;
      default:
        goto LAB_802162c8;
      case 4:
        func_80246F90(0xbf,0xd9,0x102,0x4a,3,0,0,0x80,0x80,0x80);
        BrTextPrint("CONTROLLER ERROR",0x67,0x7c);
        BrTextPrint("HAS BEEN DETECTED!",0x67,0x8a);
        break;
      case 5:
        func_80246F90(0xaf,0xd9,0x122,0x4a,3,0,0,0x80,0x80,0x80);
        if (param_1 == 3) {
          BrTextPrint("CONTROLLER PAK ERROR",0x5f,0x7c);
          BrTextPrint("HAS BEEN DETECTED!",0x5f,0x8a);
        }
        else {
          BrTextPrint("SAVED DATA NOT FOUND",0x5f,0x7c);
          BrTextPrint("IN CONTROLLER PAK.",0x5f,0x8a);
        }
        break;
      case 7:
      case 8:
        func_80246F90(0xa5,0xa5,0x136,0xbd,3,0,0,0x80,0x80,0x80);
        BrTextPrint("INSUFFICIENT FREE PAGES",0x5a,0x62);
        BrTextPrint("OR FREE NOTES IN THE",0x5a,0x6f);
        BrTextPrint("CONTROLLER PAK.",0x5a,0x7c);
        BrTextPrint("ONE PAGE AND ONE NOTE",0x5a,0x8f);
        BrTextPrint("ARE NEEDED TO SAVE THE",0x5a,0x9c);
        BrTextPrint("OPTIONS.",0x5a,0xa9);
        break;
      case 10:
        func_80246F90(0xbd,0xd9,0x106,0x4a,3,0,0,0x80,0x80,0x80);
        BrTextPrint("THE CONTROLLER PAK",0x66,0x7c);
        BrTextPrint("IS NONFUNCTIONAL!",0x66,0x8a);
      }
    }
    else if (D_802724F0 == 9999) {
      local_20 = 0x148 - D_8028D0C0;
      func_80246F90(0x79,0xb0,0x18d,0xa0,3,0,0,0x80,0x80,0x80);
      BrTextPrint("PLEASE REMOVE THE RUMBLE PAK",0x44,0x68);
      BrTextPrint("AND INSERT THE CONTROLLER PAK",0x44,0x75);
      BrTextPrint("INTO THE CONTROLLER.  PRESS",0x44,0x82);
      BrTextPrint("THE A BUTTON WHEN READY.",0x44,0x8f);
      BrTextAlignLeft();
      BrTextSetFont(10);
      iVar4 = local_20 + 0x12 >> 1;
      BrTextPrint("%wwOK",D_8028D0C0 + 0xe3U >> 1,iVar4);
      BrTextPrint("%wwCANCEL",D_8028D0F0 + 0x144U >> 1,iVar4);
      BrImageDrawAt(&D_8028D0B0,0xdd,local_20);
      BrImageDrawAt(&D_8028D0E0,0x13e,local_20);
    }
    else {
LAB_802162c8:
      func_80246F90(0xaf,0xd9,0x122,0x4a,3,0,0,0x80,0x80,0x80);
      BrTextPrint("CONTROLLER PAK ERROR",0x5f,0x7c);
      BrTextPrint("HAS BEEN DETECTED.",0x5f,0x8a);
    }
    if (D_802724F0 == 9999) {
      puVar3 = (unsigned int *)(&D_8036A8E0 + D_80271FA8 * 0x15c);
      if ((*puVar3 & 0x10) == 0) {
        if ((*puVar3 & 0x20) != 0) {
          BrPadConsume(puVar3,0x20);
          D_80316430 = '\f';
        }
      }
      else {
        BrPadConsume(puVar3,0x10);
        D_802724F0 = 0;
        D_80316430 = '\x02';
      }
    }
    else if ((*(unsigned int *)(&D_80271FA8 + D_8036A8E0 * 0x15c) & 0x8030) != 0) {
      BrPadConsume(&D_8036A8E0 + D_80271FA8 * 0x15c,0x8030);
      D_802724F0 = 0;
      D_80316430 = '\f';
    }
    break;
  case '\n':
    iVar4 = BrRumbleInsertPrompt(0);
    if (iVar4 != 0) {
      D_802724FC = '\0';
      iVar4 = func_80262370(&D_80272D48,D_80271FA8 * 0x68 + -0x7fce5c08,D_80271FA8);
      if (iVar4 == 0) {
        (&D_8031B1E8)[D_80271FA8] = 1;
      }
      D_80316430 = '\f';
    }
    break;
  case '\v':
    if (param_2 == '\0') {
      BrTextAlignCentre();
      BrTextHighlightOff();
      BrTextSetFont(0xc);
      if (param_1 == 0) {
        if (D_802724F4 == '\0') {
          local_8 = local_2e + 0xcb;
        }
        else {
          local_8 = 0xce;
        }
        func_80246F90(0xe2,local_8,0xbc,0x4a,3,0,0,0x80,0x80,0x80);
        iVar4 = local_8 + 0x20 >> 1;
        BrTextPrint("%ywSEASON DATA",0x9f,iVar4,iVar4);
        BrTextPrint("%ywLOADED OK!",0x9f,iVar4 + 0xe);
      }
      else if (param_1 == 1) {
        if (D_802724F4 == '\0') {
          local_8 = local_2e + 0xcb;
        }
        else {
          local_8 = 0xce;
        }
        func_80246F90(0xe2,local_8,0xbc,0x4a,3,0,0,0x80,0x80,0x80);
        iVar4 = local_8 + 0x20 >> 1;
        BrTextPrint("%ywGHOST DATA",0x9f,iVar4,iVar4);
        BrTextPrint("%ywLOADED OK!",0x9f,iVar4 + 0xe);
      }
      else if ((param_1 == 2) || (param_1 == 3)) {
        func_80246F90(0xd8,0xd9,0xd0,0x4a,3,0,0,0x80,0x80,0x80);
        BrTextPrint("%ywCONFIGURATION",0x9f,0x7c);
        if (param_1 == 2) {
          BrTextPrint("%ywLOADED OK!",0x9f,0x8a);
        }
        else {
          BrTextPrint("%ywSAVED OK!",0x9f,0x8a);
        }
      }
      if ((*(unsigned int *)(&D_8036A8E0 + D_80271FA8 * 0x15c) & 0x8030) != 0) {
        BrPadConsume(&D_8036A8E0 + D_80271FA8 * 0x15c,0x8030);
        if (param_1 == 2) {
          BrVolumesApply();
        }
        if (D_802724FC == '\0') {
          D_80316430 = '\f';
        }
        else {
          D_80316430 = '\n';
        }
      }
    }
    else {
      D_80316430 = '\f';
    }
    break;
  case '\f':
    D_802724F0 = 0;
    D_802724F8 = 0;
    if (param_2 == '\0') {
      func_80223750(0x3f800000,D_802A9018);
      func_802237D0(0x3f800000,D_802A901C);
    }
    D_802A4BE8 = 1;
    return 1;
  }
  D_802A4BE8 = 1;
  return 0;
}

/* WHAT IT DOES: Check the controller pak in port 1 before the game uses it:
 * initialise it (a rumble pak, or a pak with a damaged id that repairs,
 * is taken as fine), put the saved pak id back, count its files (checking
 * a pak reported inconsistent), then read every file's state and fail if
 * a readable one belongs to another game (not company NGRE, game 5D).
 * Returns 1 when the pak can be used.
 * RESIDUE (3): the ROM's frame is 0x80 with the file-count word at
 * sp+0x64 (24 unreferenced bytes above it, 52 below); ours is 0x40. */
/* @implements 0x80254620 tgr BrPakCheckFiles */
int BrPakCheckFiles(void)
{
  static unsigned short bad = 0;          /* 0x8028DDA0: files whose state would not read */
  static int used;                        /* 0x8036A278 */
  static BrPfsState states[16];           /* 0x8036A280 */
  int maxFiles;
  int i;

  D_802A4BE8 = 0;
  D_802724F0 = func_80261CB0(&D_80272D48, &D_80369EC0[0], 0);
  if (D_802724F0 != 0 && D_802724F0 == 10) {
    if (func_80262370(&D_80272D48, &D_8031A3F8[0], 0) == 0) {
      D_802A4BE8 = 1;
      return 1;
    }
    D_802724F0 = func_80262660(&D_80369EC0[0]);
    if (D_802724F0 == 0) {
      func_80261CB0(&D_80272D48, &D_80369EC0[0], 0);
    }
  }
  if (D_802724F0 != 0) {
    D_802A4BE8 = 1;
    return 1;
  }
  func_802674D0(D_80369EC0[0].raw + 0xc, D_803163E0, 0x20);
  D_802724F0 = func_802677E0(&D_80369EC0[0], &maxFiles, &used);
  if (D_802724F0 != 0) {
    if (D_802724F0 == 3) {
      if (func_80262A80(&D_80369EC0[0]) != 0) {
        D_802A4BE8 = 1;
        return 1;
      }
    } else {
      D_802A4BE8 = 1;
      return 1;
    }
  }
  for (i = 0; i < 16; i++) {
    if (func_80267930(&D_80369EC0[0], i, &states[i]) != 0) {
      bad |= 1 << i;
    }
  }
  for (i = 0; i < 16; i++) {
    if (!(bad & (1 << i))) {
      osSyncPrintf("%d: %08x %04x\n", i, states[i].company, states[i].game);
      if (states[i].company != 0x4e475245 || states[i].game != 0x3544) {
        D_802A4BE8 = 1;
        return 0;
      }
    }
  }
  D_802A4BE8 = 1;
  return 1;
}
