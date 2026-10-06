/* WHAT IT DOES: hosts a brand-new game on the network. Builds the session
 * description -- our application id, room for eight players, the caller's
 * record as the name and the shared settings in the four user slots --
 * and asks DirectPlay to open it as the host. Then fills in the host
 * record and creates our own player in the session; if the player cannot
 * be made the record is put back the way it was and the error returned.
 * Networking switched off, or no DirectPlay object, refuses politely. */
/* @implements 0x10035C50 glide BrNetSessionHost
 * @cpp_kind free
 * @cpp_symbol _BrNetSessionHost
 *
 * cdecl, three arguments, 383 B.  Lives in the C++ lane: the original was
 * built by C1XX against the C++ DirectPlay interface, which is where its
 * single vptr load serving both calls comes from.  Three more source facts
 * the C transcription in br_dplaysession.c never had: the saved host-record
 * fields live in a record-shaped copy (its +0/+0xC/+0x10 frame slots), the
 * player id is left uninitialised for CreatePlayer to fill, and the DPNAME
 * is zeroed and then has its long name cleared again.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
extern "C" {


#include "slice6_73.h"
/* g_br73 is the port's gathering of separate originals.  The matching build
 * names the ones used here as the globals they are (config/brally/globals_glide.csv),
 * so each relocation resolves to its own variable. */
extern int32_t g_brUinAA2880;   /* 0x10AC5BD8 */
#define BR73_NAA2880 g_brUinAA2880
extern void *const *g_brUiapJoinBlob;   /* 0x10AC5D2C */
#define BR73_APJOINBLOB g_brUiapJoinBlob

extern int   DAT_10ac4090;
extern int   DAT_10077500, DAT_10077504, DAT_10077508, DAT_1007750c;
extern int   DAT_100b3014;           /* -> desc.dwUser1                  */
extern int   DAT_10226e80;           /* -> desc.dwUser2                  */
extern int  *DAT_10ac5d70;           /* the host record; -> desc.dwUser3 */
extern int   DAT_100abdf8;           /* -> desc.dwUser4                  */
extern char  DAT_10b71648[];         /* the player name                  */

typedef struct BrDpDesc2 {
    int size, flags;
    int guidI[4];
    int guidA[4];
    int maxPlayers, curPlayers;
    int pszName, pszPassword;
    int reserved1, reserved2;
    int user1, user2, user3, user4;
} BrDpDesc2;


/* IDirectPlay3A as the C++ interface: CreatePlayer is slot 6 (+0x18),
 * SecureOpen slot 39 (+0x9C). */
struct IBrDP3 {
    virtual long __stdcall s00(void) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall s02(void) = 0;
    virtual long __stdcall s03(void) = 0;
    virtual long __stdcall s04(void) = 0;
    virtual long __stdcall s05(void) = 0;
    virtual long __stdcall CreatePlayer(int *pid, void *pName, int hEvent, void *pData, int cb, int flags) = 0;
    virtual long __stdcall s07(void) = 0;
    virtual long __stdcall s08(void) = 0;
    virtual long __stdcall s09(void) = 0;
    virtual long __stdcall s10(void) = 0;
    virtual long __stdcall s11(void) = 0;
    virtual long __stdcall s12(void) = 0;
    virtual long __stdcall s13(void) = 0;
    virtual long __stdcall s14(void) = 0;
    virtual long __stdcall s15(void) = 0;
    virtual long __stdcall s16(void) = 0;
    virtual long __stdcall s17(void) = 0;
    virtual long __stdcall s18(void) = 0;
    virtual long __stdcall s19(void) = 0;
    virtual long __stdcall s20(void) = 0;
    virtual long __stdcall s21(void) = 0;
    virtual long __stdcall s22(void) = 0;
    virtual long __stdcall s23(void) = 0;
    virtual long __stdcall s24(void) = 0;
    virtual long __stdcall s25(void) = 0;
    virtual long __stdcall s26(void) = 0;
    virtual long __stdcall s27(void) = 0;
    virtual long __stdcall s28(void) = 0;
    virtual long __stdcall s29(void) = 0;
    virtual long __stdcall s30(void) = 0;
    virtual long __stdcall s31(void) = 0;
    virtual long __stdcall s32(void) = 0;
    virtual long __stdcall s33(void) = 0;
    virtual long __stdcall s34(void) = 0;
    virtual long __stdcall s35(void) = 0;
    virtual long __stdcall s36(void) = 0;
    virtual long __stdcall s37(void) = 0;
    virtual long __stdcall s38(void) = 0;
    virtual long __stdcall SecureOpen(void *pDesc, int flags, void *pSec, void *pCred) = 0;
};
int BrNetSessionHost(void *pIface, char *pHost, int *pRec)
{
    BrDpDesc2 desc;
    int  saved[5];              /* a record copy; fields 0, 3, 4 used */
    int  name[4];
    int  id;                    /* never initialised: CreatePlayer writes it */
    int *rec;
    int  hr;

    if (DAT_10ac4090 == 0) {
        if (pIface == 0) {
            return (int)0x88770082;
        }
        memset(&desc, 0, 0x50);
        desc.guidA[0] = DAT_10077500;
        desc.guidA[1] = DAT_10077504;
        desc.guidA[2] = DAT_10077508;
        desc.guidA[3] = DAT_1007750c;
        desc.flags = (*(int *)(pHost + 0xc8) != 0 ? 0x100 : 0) + 0x40;
        desc.user1 = DAT_100b3014;
        desc.user2 = DAT_10226e80;
        desc.size = 0x50;
        desc.maxPlayers = 8;
        desc.pszName = (int)pHost;
        desc.user3 = (int)DAT_10ac5d70;
        desc.user4 = DAT_100abdf8;
        hr = ((IBrDP3 *)pIface)->SecureOpen(&desc, 0x82, 0, 0);
        if (hr < 0) {
            return hr;
        }
        rec = pRec;
        memset(name, 0, 0x10);          /* the DPNAME */
        name[3] = 0;                    /* lpszLongName, cleared again */
        saved[0] = rec[0];
        saved[3] = rec[3];
        saved[4] = rec[4];
        rec[0] = (int)pIface;
        rec[3] = 1;
        rec[4] = *(int *)(pHost + 0xc8);
        name[0] = 0x10;
        name[2] = (int)DAT_10b71648;
        hr = ((IBrDP3 *)pIface)->CreatePlayer(&id, name, rec[1], 0, 0, 0x100);
        if (hr < 0) {
            rec[0] = saved[0];
            rec[3] = saved[3];
            rec[4] = saved[4];
            return hr;
        }
        rec[2] = id;
    }
    return 0;
}

}
