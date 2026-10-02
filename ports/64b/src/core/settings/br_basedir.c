/* br_basedir.c -- see br_basedir.h. 0x10063860. */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include "br_basedir.h"

#include <stddef.h>
#include <string.h>

/* 0x10B73540. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
static BrBaseDirReadFn  s_pfnRead;
static void            *s_pUser;

void BrBaseDirSetHost(BrBaseDirReadFn pfnRead, void *pUser)
{
    s_pfnRead = pfnRead;
    s_pUser   = pUser;
}

const char *BrBaseDir(void) { return g_aBrCfgBaseDir; }

/* @n64 0x802005FC located */
void BrBaseDirResetForTest(void)
{
    memset(g_aBrCfgBaseDir, 0, sizeof g_aBrCfgBaseDir);
    s_pfnRead = NULL;
    s_pUser   = NULL;
}

/* 0x10063860 */
/* WHAT IT DOES: works out where the game's data files live, by reading the
 * install path the installer wrote into the registry. If there is no such
 * entry -- or no way to read one -- it falls back to the root of the C
 * drive, which is why a badly installed copy goes looking in c:\TRACKS
 * rather than beside the executable. */
/* @implements 0x10063860 glide BrBaseDirInit */
#include <windows.h>
#pragma intrinsic(strlen, strcpy, strcat)
void BrBaseDirInit(void)
{
    HKEY  hKey;
    DWORD cbData;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, BR_BASEDIR_REGKEY, 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG r;
        cbData = BR_BASEDIR_MAX;
        r = RegQueryValueExA(hKey, BR_BASEDIR_REGVAL, NULL, NULL,
                             (LPBYTE)g_aBrCfgBaseDir, &cbData);
        RegCloseKey(hKey);          /* closed before the query result is tested */
        if (r == ERROR_SUCCESS) {
            if (g_aBrCfgBaseDir[strlen(g_aBrCfgBaseDir) - 1] != BR_BASEDIR_SEP)
                strcat(g_aBrCfgBaseDir, "\\");
            return;
        }
    }
    {
        const char *fb = BR_BASEDIR_FALLBACK;
        strcpy(g_aBrCfgBaseDir, fb);
    }
}

/* Matching TU for 0x10007F40: settings loader (BossRally.ini + cmdline).
 * Inferred from orig bytes: /Oi strcpy+strcat, else-if strncmp ladder,
 * CHK_FileExists/FReadOpen/FReadLine/FClose, cmdline strstr+strlen(key). */

/* FUN_10003680: prototype in br_funcs.h */
/* FUN_10003320: prototype in br_funcs.h */
/* FUN_10003530: prototype in br_funcs.h */
/* FUN_100035e0: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* base dir */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* ini path */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* TrackDir  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* CarDir    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* SFXDir    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* player name */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* NetworkPlay */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* chosenTrack */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* chosenCar */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* chosenWeather */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* gameMode */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* ReadJoystick */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* default input cfg */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* HandlingType */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* SuspensionType */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* TireType */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* TransmissionType */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* Interpolate */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* SpeedSensitive */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* D3DDrawCarShadow (inverted) */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* RunBenchmark */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* PlayMusic */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* PlaySFX */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* cPlayers */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* bcar */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* btire */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* bsuspension */

/* WHAT IT DOES: read the game's settings file from the install directory and
 * apply what it finds -- a plain line-by-line parse of KEY: VALUE entries
 * covering network play, the chosen track and the rest of the persisted
 * options. Silently does nothing if the file is absent, leaving the defaults
 * in place. */
/* @implements 0x10007F40 glide FUN_10007f40 */
void FUN_10007f40(char *param_1)
{
  char line[256];
  FILE **fp;
  int n;
  char *p;
  char *q;

  strcpy(g_aBrCfgIniPath, g_aBrCfgBaseDir);
  strcat(g_aBrCfgIniPath, s_BossRally_ini_1007b4dc);
  if (BrChkFileExists(g_aBrCfgIniPath) != 0) {
    fp = BrChkFReadOpen(g_aBrCfgIniPath);
    while (BrChkFReadLine(line, 0x100, fp) != 0) {
      if (strncmp(line, s_NetworkPlay__1007b4cc, 12) == 0) {
        (*(int *)&g_brRaceNet) = atoi(line + 12);
      }
      else if (strncmp(line, s_chosenTrack__1007b4bc, 12) == 0) {
        (*(int *)&g_Br0B380C) = atoi(line + 12);
      }
      else if (strncmp(line, s_chosenCar__1007b4b0, 10) == 0) {
        g_226e7c = atoi(line + 10);
      }
      else if (strncmp(line, s_chosenWeather__1007b4a0, 14) == 0) {
        g_226e80 = atoi(line + 14);
      }
      else if (strncmp(line, s_gameMode__1007b494, 9) == 0) {
        (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ = atoi(line + 9);
      }
      else if (strncmp(line, s_ReadJoystick__1007b484, 13) == 0) {
        n = atoi(line + 13);
        (*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ = n;
        switch (n) {
        case 1:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0xA8)) /* BR_LP64_BYTE_VIEW */;
          break;
        case 2:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0x150)) /* BR_LP64_BYTE_VIEW */;
          break;
        case 3:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0x1F8)) /* BR_LP64_BYTE_VIEW */;
          break;
        default:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)&g_BrCtrlCfg);
          break;
        }
      }
      else if (strncmp(line, s_HandlingType__1007b474, 13) == 0) {
        g_7b320 = atoi(line + 13);
      }
      else if (strncmp(line, s_SuspensionType__1007b464, 15) == 0) {
        g_7b328 = atoi(line + 15);
      }
      else if (strncmp(line, s_TireType__1007b458, 9) == 0) {
        g_7b32c = atoi(line + 9);
      }
      else if (strncmp(line, s_TransmissionType__1007b444, 17) == 0) {
        g_7b324 = atoi(line + 17);
      }
      else if (strncmp(line, s_TrackDir__1007b438, 9) == 0) {
        strcpy(s_tracks__100b74c0, line + 9);
        s_tracks__100b74c0[strlen(s_tracks__100b74c0) - 1] = 0;
      }
      else if (strncmp(line, s_CarDir__1007b430, 7) == 0) {
        strcpy(DAT_100b7900, line + 7);
        DAT_100b7900[strlen(DAT_100b7900) - 1] = 0;
      }
      else if (strncmp(line, s_SFXDir__1007b428, 7) == 0) {
        strcpy(g_aBrCfgSfxDir, line + 7);
        g_aBrCfgSfxDir[strlen(g_aBrCfgSfxDir) - 1] = 0;
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
        g_demoFlag = atoi(line + 13);
      }
      else if (strncmp(line, s_PlayMusic__1007b3d8, 10) == 0) {
        DAT_1007b074 = atoi(line + 10);
      }
      else if (strncmp(line, s_PlaySFX__1007b3cc, 8) == 0) {
        (*(int *)&DAT_100b51e4[1036]) = atoi(line + 8);
      }
    }
    BrChkFClose(fp);
  }

  if (param_1 != 0) {
    if (strlen(param_1) != 0) {
      p = strstr(param_1, s_NetworkPlay__1007b4cc);
      if (p != 0) {
        (*(int *)&g_brRaceNet) = atoi(p + strlen(s_NetworkPlay__1007b4cc));
      }
      p = strstr(param_1, s_szPlayerName__1007b3bc);
      if (p != 0) {
        strcpy(g_aBrCfgPlayerName, p + strlen(s_szPlayerName__1007b3bc));
        q = strchr(g_aBrCfgPlayerName, ' ');
        if (q != 0) {
          *q = 0;
        }
        q = strchr(g_aBrCfgPlayerName, '\n');
        if (q != 0) {
          *q = 0;
        }
      }
      p = strstr(param_1, s_chosenTrack__1007b4bc);
      if (p != 0) {
        (*(int *)&g_Br0B380C) = atoi(p + strlen(s_chosenTrack__1007b4bc));
      }
      p = strstr(param_1, s_chosenCar__1007b4b0);
      if (p != 0) {
        g_226e7c = atoi(p + strlen(s_chosenCar__1007b4b0));
      }
      p = strstr(param_1, s_chosenWeather__1007b4a0);
      if (p != 0) {
        g_226e80 = atoi(p + strlen(s_chosenWeather__1007b4a0));
      }
      p = strstr(param_1, s_gameMode__1007b494);
      if (p != 0) {
        (*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ = atoi(p + strlen(s_gameMode__1007b494));
      }
      p = strstr(param_1, s_ReadJoystick__1007b484);
      if (p != 0) {
        n = atoi(p + strlen(s_ReadJoystick__1007b484));
        (*(int *)((char *)&g_BrCtrlCfg + 0x2A0)) /* BR_LP64_BYTE_VIEW */ = n;
        switch (n) {
        case 1:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0xA8)) /* BR_LP64_BYTE_VIEW */;
          break;
        case 2:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0x150)) /* BR_LP64_BYTE_VIEW */;
          break;
        case 3:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)((char *)&g_BrCtrlCfg + 0x1F8)) /* BR_LP64_BYTE_VIEW */;
          break;
        default:
          (*(int * *)&g_BrPadModeBytes) = &(*(int *)&g_BrCtrlCfg);
          break;
        }
      }
      p = strstr(param_1, s_HandlingType__1007b474);
      if (p != 0) {
        g_7b320 = atoi(p + strlen(s_HandlingType__1007b474));
      }
      p = strstr(param_1, s_SuspensionType__1007b464);
      if (p != 0) {
        g_7b328 = atoi(p + strlen(s_SuspensionType__1007b464));
      }
      p = strstr(param_1, s_TireType__1007b458);
      if (p != 0) {
        g_7b32c = atoi(p + strlen(s_TireType__1007b458));
      }
      p = strstr(param_1, s_TransmissionType__1007b444);
      if (p != 0) {
        g_7b324 = atoi(p + strlen(s_TransmissionType__1007b444));
      }
      p = strstr(param_1, s_cPlayers__1007b3b0);
      if (p != 0) {
        (*(int *)&g_brCfgPlayers) = atoi(p + strlen(s_cPlayers__1007b3b0));
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
