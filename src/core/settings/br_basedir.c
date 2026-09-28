/* br_basedir.c -- see br_basedir.h. 0x10063860. */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_basedir.h"

#include <stddef.h>
#include <string.h>

/* 0x10B73540. */
static char             s_szBase[BR_BASEDIR_MAX];
static BrBaseDirReadFn  s_pfnRead;
static void            *s_pUser;

void BrBaseDirSetHost(BrBaseDirReadFn pfnRead, void *pUser)
{
    s_pfnRead = pfnRead;
    s_pUser   = pUser;
}

const char *BrBaseDir(void) { return s_szBase; }

/* @n64 0x802005FC located */
void BrBaseDirResetForTest(void)
{
    memset(s_szBase, 0, sizeof s_szBase);
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
#ifdef BR_MATCHING_BUILD
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
                             (LPBYTE)s_szBase, &cbData);
        RegCloseKey(hKey);          /* closed before the query result is tested */
        if (r == ERROR_SUCCESS) {
            if (s_szBase[strlen(s_szBase) - 1] != BR_BASEDIR_SEP)
                strcat(s_szBase, "\\");
            return;
        }
    }
    {
        const char *fb = BR_BASEDIR_FALLBACK;
        strcpy(s_szBase, fb);
    }
}
#else
void BrBaseDirInit(void)
{
    size_t len;

    /* No host read installed is the same outcome as the key being absent:
     * 0x10063883 and 0x100638BC both jump to 0x10063909, and the original
     * makes no distinction between "no key" and "no value". Inventing one
     * here would be inventing behaviour. */
    if (s_pfnRead == NULL ||
        s_pfnRead(s_pUser, BR_BASEDIR_REGKEY, BR_BASEDIR_REGVAL,
                  s_szBase, sizeof s_szBase) != 0) {
        /* 0x10063909: strcpy(base, "c:\") and return. NOT the empty string --
         * a machine with no registry entry gets an ABSOLUTE path at the root
         * of C:, which is why a mis-installed copy looks for c:\TRACKS\...
         * rather than ./TRACKS. */
        s_szBase[0] = 0;
        strncat(s_szBase, BR_BASEDIR_FALLBACK, sizeof s_szBase - 1);
        return;
    }

    s_szBase[sizeof s_szBase - 1] = 0;   /* the host must not run us off the end */
    len = strlen(s_szBase);

    /* 0x100638CD: cmp byte ptr [ecx + 0x10B7353F], 0x5C, with ecx = strlen.
     * 0x10B7353F is one byte BEFORE the buffer, so the address is
     * base[len - 1] -- the last character. Read the displacement without the
     * register and it looks like a fixed global.
     *
     * DEVIATION, and the only one here: with len == 0 the original reads
     * base[-1], one byte before the buffer, because it has no guard. That is a
     * real out-of-bounds read on an empty "Directory" value, and this port
     * does not reproduce it -- an empty value takes the append path, which is
     * what the original does for every value that does not already end in a
     * separator. The difference is observable only when the byte before the
     * buffer happens to be 0x5C, which is not a state any caller can arrange
     * and not a behaviour worth preserving. Stated rather than silently
     * "fixed". */
    if (len > 0 && s_szBase[len - 1] == BR_BASEDIR_SEP) {
        return;                          /* 0x100638D4 je -> the epilogue */
    }

    /* 0x100638D6: strcat(base, "\") -- 0x100ACACC is a one-character string. */
    if (len + 1 < sizeof s_szBase) {
        s_szBase[len]     = BR_BASEDIR_SEP;
        s_szBase[len + 1] = 0;
    }
}
#endif

#ifdef BR_MATCHING_BUILD
/* Matching TU for 0x10007F40 - settings loader (BossRally.ini + cmdline).
 * Inferred from orig bytes: /Oi strcpy+strcat, else-if strncmp ladder,
 * CHK_FileExists/FReadOpen/FReadLine/FClose, cmdline strstr+strlen(key). */

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

/* WHAT IT DOES: read the game's settings file from the install directory and
 * apply what it finds -- a plain line-by-line parse of KEY: VALUE entries
 * covering network play, the chosen track and the rest of the persisted
 * options. Silently does nothing if the file is absent, leaving the defaults
 * in place. */
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
