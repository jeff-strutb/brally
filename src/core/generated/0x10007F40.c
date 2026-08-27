/* Matching TU for 0x10007F40 — settings loader (BossRally.ini + cmdline).
 * Inferred from orig bytes: /Oi strcpy+strcat, else-if strncmp ladder,
 * CHK_FileExists/FReadOpen/FReadLine/FClose, cmdline strstr+strlen(key). */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif

int FUN_10003680(char *);   /* CHK_FileExists */
int FUN_10003320(char *);   /* CHK_FReadOpen  */
int FUN_10003530(char *, int, int); /* CHK_FReadLine */
int FUN_100035e0(int);      /* CHK_FClose     */

extern char DAT_10b73540[]; /* base dir */
extern char DAT_10226a78[]; /* ini path */
extern char DAT_100b74c0[]; /* TrackDir  */
extern char DAT_100b7900[]; /* CarDir    */
extern char DAT_100b7d40[]; /* SFXDir    */
extern char DAT_10b71648[]; /* player name */
extern char s_BossRally_ini_1007b4dc[];
extern char s_NetworkPlay__1007b4cc[];
extern char s_chosenTrack__1007b4bc[];
extern char s_chosenCar__1007b4b0[];
extern char s_chosenWeather__1007b4a0[];
extern char s_gameMode__1007b494[];
extern char s_ReadJoystick__1007b484[];
extern char s_HandlingType__1007b474[];
extern char s_SuspensionType__1007b464[];
extern char s_TireType__1007b458[];
extern char s_TransmissionType__1007b444[];
extern char s_TrackDir__1007b438[];
extern char s_CarDir__1007b430[];
extern char s_SFXDir__1007b428[];
extern char s_Interpolate__1007b418[];
extern char s_SpeedSensitive__1007b408[];
extern char s_D3DDrawCarShadow__1007b3f4[];
extern char s_RunBenchmark__1007b3e4[];
extern char s_PlayMusic__1007b3d8[];
extern char s_PlaySFX__1007b3cc[];
extern char s_szPlayerName__1007b3bc[];
extern char s_cPlayers__1007b3b0[];
extern char s_bcar__1007b3a8[];
extern char s_btire__1007b3a0[];
extern char s_bsuspension__1007b390[];

extern int DAT_10226a48;    /* NetworkPlay */
extern int DAT_100b3014;    /* chosenTrack */
extern int DAT_10226e7c;    /* chosenCar */
extern int DAT_10226e80;    /* chosenWeather */
extern int DAT_100a9360;    /* gameMode */
extern int DAT_10b71530;    /* ReadJoystick */
extern int *DAT_10b71534;
extern int DAT_10b71338;
extern int DAT_10b713e0;
extern int DAT_10b71488;
extern int DAT_10b71290;    /* default input cfg */
extern int DAT_1007b320;    /* HandlingType */
extern int DAT_1007b328;    /* SuspensionType */
extern int DAT_1007b32c;    /* TireType */
extern int DAT_1007b324;    /* TransmissionType */
extern int DAT_100a5eac;    /* Interpolate */
extern int DAT_100b2e6c;    /* SpeedSensitive */
extern int DAT_10396eb0;    /* D3DDrawCarShadow (inverted) */
extern int DAT_118eeedc;    /* RunBenchmark */
extern int DAT_1007b074;    /* PlayMusic */
extern int DAT_100b55f0;    /* PlaySFX */
extern int DAT_1021cdf8;    /* cPlayers */
extern int DAT_1021ce50;    /* bcar */
extern int DAT_10226a40;    /* btire */
extern int DAT_10226a3c;    /* bsuspension */

/* @implements 0x10007F40 glide FUN_10007f40 */
void FUN_10007f40(char *param_1)
{
  char line[256];
  int fp;
  int n;
  char *p;
  char *q;

  strcpy(DAT_10226a78, DAT_10b73540);
  strcat(DAT_10226a78, s_BossRally_ini_1007b4dc);
  if (FUN_10003680(DAT_10226a78) != 0) {
    fp = FUN_10003320(DAT_10226a78);
    while (FUN_10003530(line, 0x100, fp) != 0) {
      if (strncmp(line, s_NetworkPlay__1007b4cc, 12) == 0) {
        DAT_10226a48 = atoi(line + 12);
      }
      else if (strncmp(line, s_chosenTrack__1007b4bc, 12) == 0) {
        DAT_100b3014 = atoi(line + 12);
      }
      else if (strncmp(line, s_chosenCar__1007b4b0, 10) == 0) {
        DAT_10226e7c = atoi(line + 10);
      }
      else if (strncmp(line, s_chosenWeather__1007b4a0, 14) == 0) {
        DAT_10226e80 = atoi(line + 14);
      }
      else if (strncmp(line, s_gameMode__1007b494, 9) == 0) {
        DAT_100a9360 = atoi(line + 9);
      }
      else if (strncmp(line, s_ReadJoystick__1007b484, 13) == 0) {
        n = atoi(line + 13);
        DAT_10b71530 = n;
        switch (n) {
        case 1:
          DAT_10b71534 = &DAT_10b71338;
          break;
        case 2:
          DAT_10b71534 = &DAT_10b713e0;
          break;
        case 3:
          DAT_10b71534 = &DAT_10b71488;
          break;
        default:
          DAT_10b71534 = &DAT_10b71290;
          break;
        }
      }
      else if (strncmp(line, s_HandlingType__1007b474, 13) == 0) {
        DAT_1007b320 = atoi(line + 13);
      }
      else if (strncmp(line, s_SuspensionType__1007b464, 15) == 0) {
        DAT_1007b328 = atoi(line + 15);
      }
      else if (strncmp(line, s_TireType__1007b458, 9) == 0) {
        DAT_1007b32c = atoi(line + 9);
      }
      else if (strncmp(line, s_TransmissionType__1007b444, 17) == 0) {
        DAT_1007b324 = atoi(line + 17);
      }
      else if (strncmp(line, s_TrackDir__1007b438, 9) == 0) {
        strcpy(DAT_100b74c0, line + 9);
        DAT_100b74c0[strlen(DAT_100b74c0) - 1] = 0;
      }
      else if (strncmp(line, s_CarDir__1007b430, 7) == 0) {
        strcpy(DAT_100b7900, line + 7);
        DAT_100b7900[strlen(DAT_100b7900) - 1] = 0;
      }
      else if (strncmp(line, s_SFXDir__1007b428, 7) == 0) {
        strcpy(DAT_100b7d40, line + 7);
        DAT_100b7d40[strlen(DAT_100b7d40) - 1] = 0;
      }
      else if (strncmp(line, s_Interpolate__1007b418, 12) == 0) {
        DAT_100a5eac = atoi(line + 12);
      }
      else if (strncmp(line, s_SpeedSensitive__1007b408, 15) == 0) {
        DAT_100b2e6c = atoi(line + 15);
      }
      else if (strncmp(line, s_D3DDrawCarShadow__1007b3f4, 17) == 0) {
        DAT_10396eb0 = (atoi(line + 17) == 0);
      }
      else if (strncmp(line, s_RunBenchmark__1007b3e4, 13) == 0) {
        DAT_118eeedc = atoi(line + 13);
      }
      else if (strncmp(line, s_PlayMusic__1007b3d8, 10) == 0) {
        DAT_1007b074 = atoi(line + 10);
      }
      else if (strncmp(line, s_PlaySFX__1007b3cc, 8) == 0) {
        DAT_100b55f0 = atoi(line + 8);
      }
    }
    FUN_100035e0(fp);
  }

  if (param_1 != 0) {
    if (strlen(param_1) != 0) {
      p = strstr(param_1, s_NetworkPlay__1007b4cc);
      if (p != 0) {
        DAT_10226a48 = atoi(p + strlen(s_NetworkPlay__1007b4cc));
      }
      p = strstr(param_1, s_szPlayerName__1007b3bc);
      if (p != 0) {
        strcpy(DAT_10b71648, p + strlen(s_szPlayerName__1007b3bc));
        q = strchr(DAT_10b71648, ' ');
        if (q != 0) {
          *q = 0;
        }
        q = strchr(DAT_10b71648, '\n');
        if (q != 0) {
          *q = 0;
        }
      }
      p = strstr(param_1, s_chosenTrack__1007b4bc);
      if (p != 0) {
        DAT_100b3014 = atoi(p + strlen(s_chosenTrack__1007b4bc));
      }
      p = strstr(param_1, s_chosenCar__1007b4b0);
      if (p != 0) {
        DAT_10226e7c = atoi(p + strlen(s_chosenCar__1007b4b0));
      }
      p = strstr(param_1, s_chosenWeather__1007b4a0);
      if (p != 0) {
        DAT_10226e80 = atoi(p + strlen(s_chosenWeather__1007b4a0));
      }
      p = strstr(param_1, s_gameMode__1007b494);
      if (p != 0) {
        DAT_100a9360 = atoi(p + strlen(s_gameMode__1007b494));
      }
      p = strstr(param_1, s_ReadJoystick__1007b484);
      if (p != 0) {
        n = atoi(p + strlen(s_ReadJoystick__1007b484));
        DAT_10b71530 = n;
        switch (n) {
        case 1:
          DAT_10b71534 = &DAT_10b71338;
          break;
        case 2:
          DAT_10b71534 = &DAT_10b713e0;
          break;
        case 3:
          DAT_10b71534 = &DAT_10b71488;
          break;
        default:
          DAT_10b71534 = &DAT_10b71290;
          break;
        }
      }
      p = strstr(param_1, s_HandlingType__1007b474);
      if (p != 0) {
        DAT_1007b320 = atoi(p + strlen(s_HandlingType__1007b474));
      }
      p = strstr(param_1, s_SuspensionType__1007b464);
      if (p != 0) {
        DAT_1007b328 = atoi(p + strlen(s_SuspensionType__1007b464));
      }
      p = strstr(param_1, s_TireType__1007b458);
      if (p != 0) {
        DAT_1007b32c = atoi(p + strlen(s_TireType__1007b458));
      }
      p = strstr(param_1, s_TransmissionType__1007b444);
      if (p != 0) {
        DAT_1007b324 = atoi(p + strlen(s_TransmissionType__1007b444));
      }
      p = strstr(param_1, s_cPlayers__1007b3b0);
      if (p != 0) {
        DAT_1021cdf8 = atoi(p + strlen(s_cPlayers__1007b3b0));
      }
      p = strstr(param_1, s_bcar__1007b3a8);
      if (p != 0) {
        DAT_1021ce50 = atoi(p + strlen(s_bcar__1007b3a8));
      }
      p = strstr(param_1, s_btire__1007b3a0);
      if (p != 0) {
        DAT_10226a40 = atoi(p + strlen(s_btire__1007b3a0));
      }
      p = strstr(param_1, s_bsuspension__1007b390);
      if (p != 0) {
        DAT_10226a3c = atoi(p + strlen(s_bsuspension__1007b390));
      }
    }
  }
}

#endif /* BR_MATCHING_BUILD */
